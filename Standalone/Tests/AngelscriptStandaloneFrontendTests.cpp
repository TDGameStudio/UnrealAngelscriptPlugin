#include "Compiler/Frontend/AngelscriptStandaloneDeclarations.h"
#include "Compiler/Frontend/AngelscriptStandaloneLexing.h"
#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"
#include "Compiler/Frontend/AngelscriptStandaloneRewrite.h"
#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>

namespace
{
	class FTestTypeOracle final
		: public AngelscriptStandalone::Frontend::ITypeOracle
	{
	public:
		bool ResolveType(
			const std::string_view TypeSpelling,
			std::string& OutStableTypeId) const override
		{
			const auto Found = Types.find(std::string(TypeSpelling));
			if (Found == Types.end())
			{
				return false;
			}
			OutStableTypeId = Found->second;
			return true;
		}

		std::map<std::string, std::string> Types = {
			{"FString", "type:fstring"},
			{"UObject", "type:uobject"},
		};
	};

	bool Require(bool Condition, const char* What)
	{
		if (!Condition)
		{
			std::cerr << "FAILED: " << What << '\n';
		}
		return Condition;
	}
}

int main()
{
	using namespace AngelscriptStandalone::Frontend;

	bool Passed = true;

	const FPathResult Normalized = NormalizeLogicalPath("Tests\\Foo.as\\Baz.asset.as");
	Passed &= Require(Normalized.bSuccess, "normalize a relative logical path");
	Passed &= Require(
		Normalized.Value == "Tests/Foo.as/Baz.asset.as",
		"normalize separators without stripping intermediate suffixes");
	Passed &= Require(
		ModuleNameFromLogicalPath(Normalized.Value) == "Tests.Foo.as.Baz.asset",
		"strip only the terminal .as suffix");
	Passed &= Require(
		!NormalizeLogicalPath("../outside.as").bSuccess,
		"reject logical paths that escape the source root");
	Passed &= Require(
		MakeStableModuleId("Tests\\Main.as") == MakeStableModuleId("Tests/Main.as"),
		"module identity uses the normalized logical path");

	const FSourceText UnicodeSource("first\n\xE4\xBD\xA0x\n");
	const FSourceLocation XLocation = UnicodeSource.GetLocation(9);
	Passed &= Require(XLocation.Line == 2, "UTF-8 line lookup");
	Passed &= Require(XLocation.Column == 2, "UTF-8 scalar column lookup");
	Passed &= Require(
		UnicodeSource.GetByteOffset(2, 2) == 9,
		"UTF-8 location round trip");

	const std::string LexicalInput =
		"string value = \"// not a comment\"; // comment\n"
		"int result = call(\")\", /* ignored ) */ (2));\n";
	const std::string WithoutComments = BlankCommentsPreservingLayout(LexicalInput);
	Passed &= Require(
		WithoutComments.size() == LexicalInput.size(),
		"comment blanking preserves byte offsets");
	Passed &= Require(
		WithoutComments.find("\"// not a comment\"") != std::string::npos,
		"comment lexer preserves comment markers inside strings");
	const std::size_t OpenParenthesis = LexicalInput.find('(', LexicalInput.find("call"));
	const FDelimiterMatch Delimiter = FindMatchingDelimiter(
		LexicalInput,
		OpenParenthesis,
		'(',
		')');
	Passed &= Require(Delimiter.bSuccess, "find a delimiter outside strings and comments");
	Passed &= Require(
		LexicalInput.substr(Delimiter.CloseOffset, 1) == ")",
		"delimiter result points at the closing token");

	FRewritePlan RewritePlan;
	Passed &= Require(RewritePlan.Add({0, 3, "one"}).bSuccess, "add first rewrite");
	Passed &= Require(RewritePlan.Add({4, 7, "two"}).bSuccess, "add ordered rewrite");
	Passed &= Require(!RewritePlan.Add({2, 5, "overlap"}).bSuccess, "reject overlapping rewrite");
	const FRewriteResult Rewritten = RewritePlan.Apply("abc def");
	Passed &= Require(Rewritten.bSuccess && Rewritten.Text == "one two", "apply rewrite plan");
	Passed &= Require(
		Rewritten.SourceMap.MapProcessedToOriginal(5) == 5,
		"rewrite source map preserves corresponding offsets");

	FPreprocessConfig Config;
	Config.Flags.emplace("EDITOR", true);
	Config.Flags.emplace("TEST", false);
	const FSourceInput ConditionalSource{
		"Scripts/Main.as",
		"#if EDITOR\n"
		"import Scripts.Shared;\n"
		"int Active = 1;\n"
		"#else\n"
		"import Scripts.Release;\n"
		"int Inactive = 2;\n"
		"#endif\n"
		"string Marker = \"#if TEST\";\n",
	};
	const FPreprocessResult Conditional = PreprocessSource(ConditionalSource, Config);
	Passed &= Require(Conditional.bSuccess, "preprocess supported conditionals");
	Passed &= Require(Conditional.Imports.size() == 1, "record only active imports");
	Passed &= Require(
		Conditional.Imports[0].ModuleName == "Scripts.Shared",
		"record normalized active import");
	Passed &= Require(
		Conditional.ProcessedSource.find("int Active = 1;") != std::string::npos,
		"preserve active branch");
	Passed &= Require(
		Conditional.ProcessedSource.find("int Inactive = 2;") == std::string::npos,
		"blank inactive branch");
	Passed &= Require(
		Conditional.ProcessedSource.find("\"#if TEST\"") != std::string::npos,
		"ignore directive spelling inside strings");
	const std::string RangeForSource =
		"void Test(TArray<int>& Values)\n"
		"{\n"
		"    for (int Value : Values) {}\n"
		"    for (int Index = 0; Index < 1; ++Index) {}\n"
		"    FString Text = \"for (int Fake : Text)\";\n"
		"}\n";
	const FRewriteResult RangeFor =
		RewriteRangeBasedFor(RangeForSource);
	Passed &= Require(
		RangeFor.bSuccess,
		"range-for fixture preprocesses");
	Passed &= Require(
		RangeFor.Text.find(
			"Values.Iterator()") != std::string::npos
			&& RangeFor.Text.find(
				".CanProceed") != std::string::npos
			&& RangeFor.Text.find(
				".Proceed()") != std::string::npos
			&& RangeFor.Text.find(
				"int __auto_constref_type Value")
				!= std::string::npos,
		"range-for is lowered to the UE iterator protocol");
	Passed &= Require(
		RangeFor.Text.find(
			"for (int Index = 0; Index < 1; ++Index)") != std::string::npos,
		"classic for loop is preserved");
	Passed &= Require(
		RangeFor.Text.find(
			"\"for (int Fake : Text)\"") != std::string::npos,
		"range-for spelling in a string is preserved");
	const FRewriteResult NameLiterals = RewriteNameLiterals(
		"FName Value = n\"Example\"; FString Text = \"n\\\"Ignored\\\"\";");
	Passed &= Require(
		NameLiterals.bSuccess
			&& NameLiterals.Text.find(
				"FName Value = FName(\"Example\")")
				!= std::string::npos
			&& NameLiterals.Text.find(
				"\"n\\\"Ignored\\\"\"") != std::string::npos,
		"FName literals are lowered without rewriting ordinary strings");

	const FPreprocessResult UnknownCondition = PreprocessSource(
		{"Scripts/Bad.as", "#if UNKNOWN\nint Value = 1;\n#endif\n"},
		Config);
	Passed &= Require(!UnknownCondition.bSuccess, "unknown condition fails");
	Passed &= Require(
		!UnknownCondition.Diagnostics.empty()
			&& UnknownCondition.Diagnostics[0].Code == "ASL-PREPROCESS-UNKNOWN-FLAG",
		"unknown condition has a stable diagnostic code");

	FFrontendSession Session(Config);
	Passed &= Require(
		Session.AddSource({"Scripts/A.as", "import Scripts.B;\nint A = 1;\n"}).bSuccess,
		"add first session source");
	Passed &= Require(
		Session.AddSource({"Scripts/B.as", "int B = 2;\n"}).bSuccess,
		"add dependency source");
	const FFrontendSessionResult SessionResult = Session.Process();
	Passed &= Require(SessionResult.bSuccess, "process language session");
	Passed &= Require(SessionResult.Modules.size() == 2, "session emits two modules");
	Passed &= Require(
		SessionResult.Modules[0].ModuleName == "Scripts.B"
			&& SessionResult.Modules[1].ModuleName == "Scripts.A",
		"session emits deterministic dependency-first order");

	FFrontendSession CyclicSession(Config);
	CyclicSession.AddSource({"Cycle/A.as", "import Cycle.B;\n"});
	CyclicSession.AddSource({"Cycle/B.as", "import Cycle.A;\n"});
	const FFrontendSessionResult CyclicResult = CyclicSession.Process();
	Passed &= Require(!CyclicResult.bSuccess, "cycle fails language session");
	Passed &= Require(
		!CyclicResult.Diagnostics.empty()
			&& CyclicResult.Diagnostics.back().Code == "ASL-PREPROCESS-IMPORT-CYCLE",
		"cycle has stable diagnostic code");

	FFrontendSession MissingImportSession(Config);
	MissingImportSession.AddSource(
		{"Missing.as", "import Not.Provided;\nvoid Entry() {}\n"});
	const FFrontendSessionResult MissingImportResult =
		MissingImportSession.Process();
	Passed &= Require(!MissingImportResult.bSuccess, "missing import fails language session");
	Passed &= Require(
		!MissingImportResult.Diagnostics.empty()
			&& MissingImportResult.Diagnostics.back().Code
				== "ASL-PREPROCESS-IMPORT-MISSING",
		"missing import has stable diagnostic code");
	Passed &= Require(
		Session.GetStage() == EFrontendStage::Completed,
		"successful language session records the completed stage");

	const FPreprocessResult Declarations = PreprocessSource(
		{
			"Game/Declarations.as",
			"UENUM(BlueprintType)\n"
			"enum class EState : uint8 { Idle, Active = 7 }\n"
			"enum EPlainState { One, Two = 2 }\n"
			"delegate void FOnValue(int Value);\n"
			"event void FOnBroadcast(int Value);\n"
			"UCLASS(Abstract, BlueprintType)\n"
			"class UCarrier : UObject\n"
			"{\n"
			"    UPROPERTY(EditAnywhere, meta=(DisplayName=\"Score\"))\n"
			"    private int Score = 3;\n"
			"    UPROPERTY()\n"
			"    FOnBroadcast Broadcast;\n"
			"    UFUNCTION(BlueprintEvent)\n"
			"    protected void OnScore(const FString&in Label, EPlainState State = EPlainState::One) const {}\n"
			"}\n",
		},
		Config);
	Passed &= Require(
		Declarations.bSuccess,
		"annotated declaration fixture preprocesses");
	Passed &= Require(
		Declarations.Declarations.size() == 8,
		"declaration IR contains annotated/plain enums, delegate/event "
			"types, class, properties, and event method");
	Passed &= Require(
		Declarations.ProcessedSource.find("UCLASS") == std::string::npos
			&& Declarations.ProcessedSource.find("UPROPERTY")
				== std::string::npos
			&& Declarations.ProcessedSource.find("UFUNCTION")
				== std::string::npos
			&& Declarations.ProcessedSource.find("class UCarrier : UObject")
				!= std::string::npos,
		"annotations are blanked while AngelScript declarations remain");
	std::vector<FDeclaration> ResolvedDeclarations =
		Declarations.Declarations;
	const FTypeResolutionResult Resolved = ResolveDeclarationTypes(
		ResolvedDeclarations,
		FTestTypeOracle());
	Passed &= Require(
		Resolved.bSuccess,
		"bundle-style type oracle resolves declaration IR");
	const auto Class = std::find_if(
		ResolvedDeclarations.begin(),
		ResolvedDeclarations.end(),
		[](const FDeclaration& Value)
		{
			return Value.Kind == EDeclarationKind::Class;
		});
	const auto Property = std::find_if(
		ResolvedDeclarations.begin(),
		ResolvedDeclarations.end(),
		[](const FDeclaration& Value)
		{
			return Value.Kind == EDeclarationKind::Property;
		});
	const auto Event = std::find_if(
		ResolvedDeclarations.begin(),
		ResolvedDeclarations.end(),
		[](const FDeclaration& Value)
		{
			return Value.Kind == EDeclarationKind::Event
				&& Value.Owner == "UCarrier";
		});
	const auto EventProperty = std::find_if(
		ResolvedDeclarations.begin(),
		ResolvedDeclarations.end(),
		[](const FDeclaration& Value)
		{
			return Value.Kind == EDeclarationKind::Property
				&& Value.Name == "Broadcast";
		});
	const auto Enum = std::find_if(
		ResolvedDeclarations.begin(),
		ResolvedDeclarations.end(),
		[](const FDeclaration& Value)
		{
			return Value.Kind == EDeclarationKind::Enum;
		});
	Passed &= Require(
		Class != ResolvedDeclarations.end()
			&& Class->BaseTypes.size() == 1
			&& Class->BaseTypes[0].StableTypeId == "type:uobject",
		"class base is resolved through the type oracle");
	Passed &= Require(
		Property != ResolvedDeclarations.end()
			&& Property->DefaultValue == "3"
			&& Property->Access == EAccess::Private
			&& Property->Type.StableTypeId == "builtin:int",
		"property type/default survive declaration parsing");
	Passed &= Require(
		Event != ResolvedDeclarations.end()
			&& Event->Parameters.size() == 2
			&& Event->Parameters[0].Type.StableTypeId == "type:fstring"
			&& Event->Parameters[1].Type.StableTypeId.starts_with(
				"source-type:")
			&& Event->Parameters[1].DefaultValue == "EPlainState::One"
			&& Event->Access == EAccess::Protected
			&& Event->bConst,
		"event parameters/default/qualifiers survive declaration parsing");
	Passed &= Require(
		EventProperty != ResolvedDeclarations.end()
			&& EventProperty->Type.StableTypeId.starts_with(
				"source-type:"),
		"source event declaration is available as a local property type");
	Passed &= Require(
		Enum != ResolvedDeclarations.end()
			&& Enum->EnumValues.size() == 2
			&& Enum->EnumValues[0].Name == "Idle"
			&& Enum->EnumValues[1].Name == "Active"
			&& Enum->EnumValues[1].ValueExpression == "7",
		"enum values survive declaration parsing");

	static_assert(std::is_copy_constructible_v<FDeclaration>);
	FDeclaration Declaration;
	Declaration.Kind = EDeclarationKind::Class;
	Declaration.Name = "Example";
	Declaration.QualifiedName = "Scripts::Example";
	Declaration.Source = {"Scripts/Main.as", 0, 7};
	Passed &= Require(
		Declaration.Kind == EDeclarationKind::Class
			&& Declaration.Source.LogicalPath == "Scripts/Main.as",
		"value-only declaration IR is usable without a host");

	return Passed ? 0 : 1;
}
