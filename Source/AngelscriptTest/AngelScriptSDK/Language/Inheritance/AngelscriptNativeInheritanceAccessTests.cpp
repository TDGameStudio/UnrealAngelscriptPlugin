#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FInheritanceAccessTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Inheritance.Access",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EAccessKind : uint8
	{
		Default,
		Protected,
		Private,
	};

	enum class EMemberKind : uint8
	{
		Field,
		Method,
		GetterSetterMethod,
		Constructor,
	};

	enum class EAccessSite : uint8
	{
		Owner,
		DirectDerived,
		DeepDerived,
		Unrelated,
		Global,
	};

	struct FAccessCase
	{
		const ANSICHAR* CatalogName;
		EAccessKind Access;
		const TCHAR* Prefix;
		bool bPrivate;
		bool bProtected;
	};

	struct FMemberCase
	{
		const ANSICHAR* CatalogName;
		EMemberKind Member;
	};

	struct FSiteCase
	{
		const ANSICHAR* CatalogName;
		EAccessSite Site;
	};

	inline static constexpr FAccessCase AccessCases[] =
	{
		{
			"default",
			EAccessKind::Default,
			TEXT(""),
			false,
			false,
		},
		{
			"protected",
			EAccessKind::Protected,
			TEXT("protected "),
			false,
			true,
		},
		{
			"private",
			EAccessKind::Private,
			TEXT("private "),
			true,
			false,
		},
	};

	inline static constexpr FMemberCase MemberCases[] =
	{
		{ "field", EMemberKind::Field },
		{ "method", EMemberKind::Method },
		{
			"getter_setter_method",
			EMemberKind::GetterSetterMethod,
		},
		{ "constructor", EMemberKind::Constructor },
	};

	inline static constexpr FSiteCase SiteCases[] =
	{
		{ "owner", EAccessSite::Owner },
		{ "direct_derived", EAccessSite::DirectDerived },
		{ "deep_derived", EAccessSite::DeepDerived },
		{ "unrelated", EAccessSite::Unrelated },
		{ "global", EAccessSite::Global },
	};

	static bool ShouldCompile(
		const FAccessCase& Access,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		if (Member.Member == EMemberKind::Constructor
			&& Access.bPrivate
			&& Site.Site == EAccessSite::Owner)
		{
			return false;
		}
		if (Site.Site == EAccessSite::Owner)
		{
			return true;
		}
		if (Member.Member == EMemberKind::Constructor
			&& Access.bProtected
			&& (Site.Site == EAccessSite::Unrelated
				|| Site.Site == EAccessSite::Global))
		{
			return true;
		}
		if (Site.Site == EAccessSite::DirectDerived
			|| Site.Site == EAccessSite::DeepDerived)
		{
			return !Access.bPrivate;
		}
		return !Access.bPrivate && !Access.bProtected;
	}

	static bool IsConstructor(const FMemberCase& Member)
	{
		return Member.Member == EMemberKind::Constructor;
	}

	static void AppendTargetMembers(
		FString& Source,
		const FAccessCase& Access,
		const FMemberCase& Member)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (Member.Member)
		{
		case EMemberKind::Field:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%sint Value = 41;"),
					Access.Prefix));
			break;
		case EMemberKind::Method:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%sint ReadValue()"),
					Access.Prefix));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 42;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EMemberKind::GetterSetterMethod:
			AppendGeneratedAsLine(Source, TEXT("\tint StoredValue = 43;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%sint GetValue()"),
					Access.Prefix));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn StoredValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%svoid SetValue(int InValue)"),
					Access.Prefix));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tStoredValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EMemberKind::Constructor:
			AppendGeneratedAsLine(Source, TEXT("\tint StoredValue = 44;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%sFAccessBase()"),
					Access.Prefix));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tStoredValue = 45;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		default:
			break;
		}
	}

	static void AppendAccessOperation(
		FString& Source,
		const FMemberCase& Member,
		const FString& Receiver,
		const int32 IndentLevel)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Indent =
			FString::ChrN(IndentLevel, TEXT('\t'));
		switch (Member.Member)
		{
		case EMemberKind::Field:
			AppendGeneratedAsLine(
				Source,
				Indent + TEXT("return ")
					+ Receiver + TEXT("Value;"));
			break;
		case EMemberKind::Method:
			AppendGeneratedAsLine(
				Source,
				Indent + TEXT("return ")
					+ Receiver + TEXT("ReadValue();"));
			break;
		case EMemberKind::GetterSetterMethod:
			AppendGeneratedAsLine(
				Source,
				Indent + Receiver + TEXT("SetValue(73);"));
			AppendGeneratedAsLine(
				Source,
				Indent + TEXT("return ")
					+ Receiver + TEXT("GetValue();"));
			break;
		case EMemberKind::Constructor:
		default:
			break;
		}
	}

	static void AppendOwnerType(
		FString& Source,
		const FAccessCase& Access,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FAccessBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendTargetMembers(Source, Access, Member);
		if (Site.Site == EAccessSite::Owner)
		{
			AppendGeneratedAsLine(Source);
			if (IsConstructor(Member))
			{
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFAccessBase MakeOwner()"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\t\treturn FAccessBase();"));
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
			else
			{
				AppendGeneratedAsLine(
					Source,
					TEXT("\tint ProbeAccess()"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendAccessOperation(
					Source,
					Member,
					TEXT(""),
					2);
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDirectDerivedType(
		FString& Source,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FAccessDerived : FAccessBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsConstructor(Member))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFAccessDerived()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (Site.Site == EAccessSite::DirectDerived)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint ProbeAccess()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendAccessOperation(
				Source,
				Member,
				TEXT(""),
				2);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDeepDerivedType(
		FString& Source,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FAccessDeep : FAccessDerived"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsConstructor(Member))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFAccessDeep()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (Site.Site == EAccessSite::DeepDerived)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint ProbeAccess()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendAccessOperation(
				Source,
				Member,
				TEXT(""),
				2);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendUnrelatedType(
		FString& Source,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Site.Site != EAccessSite::Unrelated)
		{
			return;
		}
		AppendGeneratedAsLine(Source, TEXT("class FAccessUnrelated"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsConstructor(Member))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint ProbeAccess()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tFAccessBase Object;"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 46;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint ProbeAccess(FAccessBase Object)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendAccessOperation(
				Source,
				Member,
				TEXT("Object."),
				2);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendGlobalSiteFunction(
		FString& Source,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Site.Site != EAccessSite::Global)
		{
			return;
		}
		if (IsConstructor(Member))
		{
			AppendGeneratedAsLine(Source, TEXT("int ProbeGlobalAccess()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFAccessBase Object;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 47;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("int ProbeGlobalAccess(FAccessBase Object)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendAccessOperation(
				Source,
				Member,
				TEXT("Object."),
				1);
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		AppendGeneratedAsLine(Source);
	}

	static void AppendRuntimeEntry(
		FString& Source,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			IsConstructor(Member)
					&& Site.Site == EAccessSite::Owner
				? TEXT("int RunInheritanceAccess(FAccessBase Owner)")
				: TEXT("int RunInheritanceAccess()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsConstructor(Member))
		{
			switch (Site.Site)
			{
			case EAccessSite::Owner:
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFAccessBase Created = Owner.MakeOwner();"));
				break;
			case EAccessSite::DirectDerived:
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFAccessDerived Created;"));
				break;
			case EAccessSite::DeepDerived:
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFAccessDeep Created;"));
				break;
			case EAccessSite::Unrelated:
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFAccessUnrelated Observer;"));
				AppendGeneratedAsLine(
					Source,
					TEXT("\treturn Observer.ProbeAccess();"));
				AppendGeneratedAsLine(Source, TEXT("}"));
				return;
			case EAccessSite::Global:
				AppendGeneratedAsLine(
					Source,
					TEXT("\treturn ProbeGlobalAccess();"));
				AppendGeneratedAsLine(Source, TEXT("}"));
				return;
			default:
				break;
			}
			AppendGeneratedAsLine(Source, TEXT("\treturn 48;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			return;
		}

		switch (Site.Site)
		{
		case EAccessSite::Owner:
			AppendGeneratedAsLine(Source, TEXT("\tFAccessBase Object;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Object.ProbeAccess();"));
			break;
		case EAccessSite::DirectDerived:
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFAccessDerived Object;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Object.ProbeAccess();"));
			break;
		case EAccessSite::DeepDerived:
			AppendGeneratedAsLine(Source, TEXT("\tFAccessDeep Object;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Object.ProbeAccess();"));
			break;
		case EAccessSite::Unrelated:
			AppendGeneratedAsLine(Source, TEXT("\tFAccessBase Object;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tFAccessUnrelated Observer;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Observer.ProbeAccess(Object);"));
			break;
		case EAccessSite::Global:
			AppendGeneratedAsLine(Source, TEXT("\tFAccessBase Object;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn ProbeGlobalAccess(Object);"));
			break;
		default:
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildInheritanceAccessSource(
		const FAccessCase& Access,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		FString Source;
		AppendOwnerType(Source, Access, Member, Site);
		AppendDirectDerivedType(Source, Member, Site);
		AppendDeepDerivedType(Source, Member, Site);
		AppendUnrelatedType(Source, Member, Site);
		AppendGlobalSiteFunction(Source, Member, Site);
		AppendRuntimeEntry(Source, Member, Site);
		return Source;
	}

	static FString BuildInheritanceAccessRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FAccessBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 149;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunInheritanceAccessRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 149;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static bool HasAnyError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR;
			});
	}

	static FString ExpectedDiagnosticFragment(
		const FAccessCase& Access,
		const FMemberCase& Member)
	{
		const TCHAR* const AccessName =
			Access.bPrivate ? TEXT("private") : TEXT("protected");
		if (Member.Member == EMemberKind::Field)
		{
			return FString::Printf(
				TEXT("%s property"),
				AccessName);
		}
		return FString::Printf(
			TEXT("Illegal call to %s method"),
			AccessName);
	}

	static bool HasLocatedError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0;
			});
	}

	static bool HasLocatedAccessError(
		const FNativeTestEngine& Engine,
		const FString& ExpectedFragment)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[&ExpectedFragment](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0
					&& Entry.Message.Contains(
						ExpectedFragment);
			});
	}

	static bool FunctionHasBytecode(
		asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return false;
		}
		asUINT Length = 0;
		return Function->GetByteCode(&Length) != nullptr
			&& Length > 0;
	}

	static asIScriptFunction* FindMethodByDeclaration(
		asITypeInfo& Type,
		const char* Declaration)
	{
		if (Declaration == nullptr)
		{
			return nullptr;
		}
		if (asIScriptFunction* Function = Type.GetMethodByDecl(Declaration))
		{
			return Function;
		}
		if (FCStringAnsi::Strcmp(Declaration, "void SetValue(int)") == 0)
		{
			if (asIScriptFunction* Function = Type.GetMethodByDecl("void SetValue(const int)"))
			{
				return Function;
			}
			if (asIScriptFunction* Function = Type.GetMethodByDecl("void SetValue(int InValue)"))
			{
				return Function;
			}
			return Type.GetMethodByDecl("void SetValue(const int InValue)");
		}

		const FString Expected = UTF8_TO_TCHAR(Declaration);
		const int32 ExpectedBegin = Expected.Find(TEXT("("));
		const int32 ExpectedEnd = Expected.Find(TEXT(")"));
		if (ExpectedBegin < 0 || ExpectedEnd <= ExpectedBegin)
		{
			return nullptr;
		}
		const auto Normalize = [](const FString& Input)
		{
			const int32 Begin = Input.Find(TEXT("("));
			const int32 End = Input.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Begin < 0 || End <= Begin)
			{
				return Input;
			}
			FString Result = Input.Left(Begin);
			const int32 ReturnSeparator = Result.Find(TEXT(" "));
			const int32 ScopeSeparator = Result.Find(TEXT("::"));
			if (ReturnSeparator >= 0 && ScopeSeparator > ReturnSeparator)
			{
				Result = Result.Left(ReturnSeparator + 1)
					+ Result.Mid(ScopeSeparator + 2);
			}
			FString Parameters = Input.Mid(Begin + 1, End - Begin - 1);
			Parameters.ReplaceInline(TEXT("const "), TEXT(""));
			TArray<FString> ParameterParts;
			Parameters.ParseIntoArray(ParameterParts, TEXT(","), true);
			for (FString& Parameter : ParameterParts)
			{
				Parameter.TrimStartAndEndInline();
				const int32 LastSpace = Parameter.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (LastSpace >= 0)
				{
					Parameter = Parameter.Left(LastSpace).TrimStartAndEnd();
				}
			}
			Result += TEXT("(") + FString::Join(ParameterParts, TEXT(",")) + TEXT(")");
			Result += Input.Mid(End + 1);
			return Result;
		};
		const FString NormalizedExpected = Normalize(Expected);
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Type.GetMethodByIndex(Index);
			if (Candidate != nullptr
				&& Normalize(UTF8_TO_TCHAR(Candidate->GetDeclaration()))
					== NormalizedExpected)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindModuleFunctionByDeclaration(
		asIScriptModule& Module,
		const char* Declaration)
	{
		if (Declaration == nullptr)
		{
			return nullptr;
		}
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(Declaration))
		{
			return Function;
		}
		const FString Expected = UTF8_TO_TCHAR(Declaration);
		const int32 ExpectedBegin = Expected.Find(TEXT("("));
		const int32 ExpectedEnd = Expected.Find(TEXT(")"));
		if (ExpectedBegin < 0 || ExpectedEnd <= ExpectedBegin)
		{
			return nullptr;
		}
		const auto Normalize = [](const FString& Input)
		{
			const int32 Begin = Input.Find(TEXT("("));
			const int32 End = Input.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Begin < 0 || End <= Begin)
			{
				return Input;
			}
			FString Result = Input.Left(Begin);
			FString Parameters = Input.Mid(Begin + 1, End - Begin - 1);
			Parameters.ReplaceInline(TEXT("const "), TEXT(""));
			TArray<FString> ParameterParts;
			Parameters.ParseIntoArray(ParameterParts, TEXT(","), true);
			for (FString& Parameter : ParameterParts)
			{
				Parameter.TrimStartAndEndInline();
				const int32 LastSpace = Parameter.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (LastSpace >= 0)
				{
					Parameter = Parameter.Left(LastSpace).TrimStartAndEnd();
				}
			}
			Result += TEXT("(") + FString::Join(ParameterParts, TEXT(",")) + TEXT(")");
			Result += Input.Mid(End + 1);
			return Result;
		};
		const FString NormalizedExpected = Normalize(Expected);
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(Index);
			if (Candidate != nullptr
				&& Normalize(UTF8_TO_TCHAR(Candidate->GetDeclaration()))
					== NormalizedExpected)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindConstructor(
		asITypeInfo& Type)
	{
		for (asUINT Index = 0;
			Index < Type.GetBehaviourCount();
			++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Behaviour == asBEHAVE_CONSTRUCT
				&& Function != nullptr
				&& Function->GetParamCount() == 0)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static bool FindFieldAccess(
		asITypeInfo& Base,
		bool& OutPrivate,
		bool& OutProtected)
	{
		for (asUINT Index = 0;
			Index < Base.GetPropertyCount();
			++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			bool bPrivate = false;
			bool bProtected = false;
			if (Base.GetProperty(
				Index,
				&Name,
				&TypeId,
				&bPrivate,
				&bProtected) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Value") == 0)
			{
				OutPrivate = bPrivate;
				OutProtected = bProtected;
				return TypeId == asTYPEID_INT32;
			}
		}
		return false;
	}

	static asITypeInfo* ProbeOwnerType(
		const FSiteCase& Site,
		asITypeInfo& Base,
		asITypeInfo& Derived,
		asITypeInfo& Deep,
		asITypeInfo* Unrelated)
	{
		switch (Site.Site)
		{
		case EAccessSite::Owner:
			return &Base;
		case EAccessSite::DirectDerived:
			return &Derived;
		case EAccessSite::DeepDerived:
			return &Deep;
		case EAccessSite::Unrelated:
			return Unrelated;
		case EAccessSite::Global:
		default:
			return nullptr;
		}
	}

	void VerifyTargetMetadata(
		const FNativeCaseContext& Case,
		const FAccessCase& Access,
		const FMemberCase& Member,
		asITypeInfo& Base)
	{
		if (Member.Member == EMemberKind::Field)
		{
			bool bPrivate = false;
			bool bProtected = false;
			ASSERT_THAT(IsTrue(
				FindFieldAccess(
					Base,
					bPrivate,
					bProtected),
				*Case.Describe(TEXT("access field should publish exact int metadata"))));
			ASSERT_THAT(AreEqual(
				Access.bPrivate,
				bPrivate,
				*Case.Describe(TEXT("access field should retain its private flag"))));
			ASSERT_THAT(AreEqual(
				Access.bProtected,
				bProtected,
				*Case.Describe(TEXT("access field should retain its protected flag"))));
			return;
		}

		if (Member.Member == EMemberKind::Constructor)
		{
			asIScriptFunction* const Constructor =
				FindConstructor(Base);
			ASSERT_THAT(IsNotNull(Constructor,
				*Case.Describe(TEXT("access constructor should publish its behavior"))));
			if (Constructor != nullptr)
			{
				ASSERT_THAT(AreEqual(
					Access.bPrivate,
					Constructor->IsPrivate(),
					*Case.Describe(TEXT("constructor should retain its private flag"))));
				ASSERT_THAT(AreEqual(
					Access.bProtected,
					Constructor->IsProtected(),
					*Case.Describe(TEXT("constructor should retain its protected flag"))));
			}
			return;
		}

		asIScriptFunction* const FirstMethod =
			FindMethodByDeclaration(
				Base,
				Member.Member == EMemberKind::Method
					? "int ReadValue()"
					: "int GetValue()");
		ASSERT_THAT(IsNotNull(FirstMethod,
			*Case.Describe(TEXT("access method should publish its exact declaration"))));
		if (FirstMethod != nullptr)
		{
			ASSERT_THAT(AreEqual(
				Access.bPrivate,
				FirstMethod->IsPrivate(),
				*Case.Describe(TEXT("access method should retain its private flag"))));
			ASSERT_THAT(AreEqual(
				Access.bProtected,
				FirstMethod->IsProtected(),
				*Case.Describe(TEXT("access method should retain its protected flag"))));
		}
		if (Member.Member == EMemberKind::GetterSetterMethod)
		{
			asIScriptFunction* const Setter =
				FindMethodByDeclaration(Base, "void SetValue(int)");
			ASSERT_THAT(IsNotNull(Setter,
				*Case.Describe(TEXT("access setter should publish its exact declaration"))));
			if (Setter != nullptr)
			{
				ASSERT_THAT(AreEqual(
					Access.bPrivate,
					Setter->IsPrivate(),
					*Case.Describe(TEXT("access setter should retain its private flag"))));
				ASSERT_THAT(AreEqual(
					Access.bProtected,
					Setter->IsProtected(),
					*Case.Describe(TEXT("access setter should retain its protected flag"))));
			}
		}
	}

	void VerifyLegalMetadata(
		const FNativeCaseContext& Case,
		const FAccessCase& Access,
		const FMemberCase& Member,
		const FSiteCase& Site,
		asIScriptModule& Module)
	{
		asITypeInfo* const Base =
			Module.GetTypeInfoByName("FAccessBase");
		asITypeInfo* const Derived =
			Module.GetTypeInfoByName("FAccessDerived");
		asITypeInfo* const Deep =
			Module.GetTypeInfoByName("FAccessDeep");
		asITypeInfo* const Unrelated =
			Module.GetTypeInfoByName("FAccessUnrelated");
		ASSERT_THAT(IsNotNull(Base,
			*Case.Describe(TEXT("access source should publish its base type"))));
		ASSERT_THAT(IsNotNull(Derived,
			*Case.Describe(TEXT("access source should publish its direct derived type"))));
		ASSERT_THAT(IsNotNull(Deep,
			*Case.Describe(TEXT("access source should publish its deep derived type"))));
		if (Base == nullptr || Derived == nullptr || Deep == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			Base,
			Derived->GetBaseType(),
			*Case.Describe(TEXT("direct access type should retain its base"))));
		ASSERT_THAT(AreEqual(
			Derived,
			Deep->GetBaseType(),
			*Case.Describe(TEXT("deep access type should retain its direct base"))));
		ASSERT_THAT(IsTrue(
			Deep->DerivesFrom(Base),
			*Case.Describe(TEXT("deep access type should derive from the owner"))));
		VerifyTargetMetadata(
			Case,
			Access,
			Member,
			*Base);

		asIScriptFunction* Witness = nullptr;
		if (IsConstructor(Member))
		{
			if (Site.Site == EAccessSite::Owner)
			{
				Witness = FindMethodByDeclaration(
					*Base,
					"FAccessBase MakeOwner()");
			}
			else if (Site.Site == EAccessSite::DirectDerived)
			{
				Witness = FindConstructor(*Derived);
			}
			else if (Site.Site == EAccessSite::DeepDerived)
			{
				Witness = FindConstructor(*Deep);
			}
			else if (Site.Site == EAccessSite::Unrelated
				&& Unrelated != nullptr)
			{
				Witness = FindMethodByDeclaration(
					*Unrelated,
					"int ProbeAccess()");
			}
			else
			{
				Witness = FindModuleFunctionByDeclaration(
					Module,
					"int ProbeGlobalAccess()");
			}
		}
		else
		{
			asITypeInfo* const Owner = ProbeOwnerType(
				Site,
				*Base,
				*Derived,
				*Deep,
				Unrelated);
			if (Owner != nullptr)
			{
				Witness = FindMethodByDeclaration(
					*Owner,
					Site.Site == EAccessSite::Unrelated
						? "int ProbeAccess(FAccessBase)"
						: "int ProbeAccess()");
			}
			else
			{
				Witness = FindModuleFunctionByDeclaration(
					Module,
					"int ProbeGlobalAccess(FAccessBase)");
			}
		}
		ASSERT_THAT(IsNotNull(Witness,
			*Case.Describe(TEXT("legal access site should publish its exact witness"))));
		ASSERT_THAT(IsTrue(FunctionHasBytecode(Witness),
			*Case.Describe(TEXT("legal access witness should emit executable bytecode"))));
	}

	void VerifyRawRuntimeBoundary(
		const FNativeCaseContext& Case,
		const FMemberCase& Member,
		const FSiteCase& Site,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* Runtime =
			FindModuleFunctionByDeclaration(
				Module,
				"int RunInheritanceAccess()");
		if (Runtime == nullptr && IsConstructor(Member) && Site.Site == EAccessSite::Owner)
		{
			Runtime = FindModuleFunctionByDeclaration(
				Module,
				"int RunInheritanceAccess(FAccessBase)");
		}
		ASSERT_THAT(IsNotNull(Runtime,
			*Case.Describe(TEXT("legal access source should publish its runtime entry"))));
		if (Runtime == nullptr)
		{
			return;
		}
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("access runtime should create a context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Context->Prepare(Runtime) >= 0,
			*Case.Describe(TEXT("access runtime should prepare its entry"))));
		for (asUINT ArgumentIndex = 0;
			ArgumentIndex < Runtime->GetParamCount();
			++ArgumentIndex)
		{
			ASSERT_THAT(IsTrue(
				Context->SetArgObject(
					ArgumentIndex,
					nullptr) >= 0,
				*Case.Describe(TEXT("access runtime should accept an explicit null object argument"))));
		}
		const int32 ExecutionResult = Context->Execute();
		const bool bExpectedException =
			!IsConstructor(Member)
			|| Site.Site == EAccessSite::Owner
			|| Site.Site == EAccessSite::Unrelated;
		if (bExpectedException)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_EXCEPTION),
				ExecutionResult,
				*Case.Describe(TEXT("legal class access should retain the isolated raw-object boundary"))));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Null pointer access")),
				FString(UTF8_TO_TCHAR(
					Context->GetExceptionString())),
				*Case.Describe(TEXT("legal access runtime should own the exact raw exception"))));
			ASSERT_THAT(IsTrue(
				Context->GetExceptionLineNumber() > 0,
				*Case.Describe(TEXT("legal access runtime should retain a source line"))));
		}
		else
		{
			const int32 ExpectedValue =
				Site.Site == EAccessSite::DirectDerived
					|| Site.Site == EAccessSite::DeepDerived
				? 48
				: Site.Site == EAccessSite::Unrelated
				? 46
				: 47;
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				ExecutionResult,
				*Case.Describe(TEXT("constructor access runtime should finish"))));
			ASSERT_THAT(AreEqual(
				ExpectedValue,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("constructor access runtime should preserve its sentinel"))));
		}
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("access runtime should unprepare cleanly"))));
		Context->Release();
	}

	void CompileAndExecuteRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource =
			BuildInheritanceAccessRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("inheritance access should allow same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("inheritance access recovery should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Case.Describe(TEXT("inheritance access recovery should emit no errors"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl(
					"int RunInheritanceAccessRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("access recovery should publish its exact entry"))));
			asIScriptContext* const Context =
				ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("access recovery should create a context"))));
			if (Recovery != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, Recovery),
					*Case.Describe(TEXT("access recovery should finish"))));
				ASSERT_THAT(AreEqual(
					149,
					static_cast<int32>(
						Context->GetReturnDWord()),
					*Case.Describe(TEXT("access recovery should return its sentinel"))));
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Case.Describe(TEXT("access recovery should unprepare cleanly"))));
				Context->Release();
			}
			else if (Context != nullptr)
			{
				Context->Release();
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("access recovery module should discard cleanly"))));
	}

	void RunCell(
		const FAccessCase& Access,
		const FMemberCase& Member,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-INH-ACCESS",
			{
				ANSI_TO_TCHAR(Access.CatalogName),
				ANSI_TO_TCHAR(Member.CatalogName),
				ANSI_TO_TCHAR(Site.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("inheritance access should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = FString::Printf(
			TEXT("InheritanceAccess_%hs_%hs_%hs"),
			Access.CatalogName,
			Member.CatalogName,
			Site.CatalogName);
		const FString Source =
			BuildInheritanceAccessSource(
				Access,
				Member,
				Site);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		if (ShouldCompile(Access, Member, Site))
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.Describe(TEXT("legal inheritance access should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("legal inheritance access should publish its module"))));
			ASSERT_THAT(IsFalse(HasAnyError(Engine),
				*Case.Describe(TEXT("legal inheritance access should emit no errors"))));
			if (Module != nullptr)
			{
				VerifyLegalMetadata(
					Case,
					Access,
					Member,
					Site,
					*Module);
				VerifyRawRuntimeBoundary(
					Case,
					Member,
					Site,
					*ScriptEngine,
					*Module);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("illegal inheritance access should fail compilation"))));
			ASSERT_THAT(IsTrue(
				Member.Member == EMemberKind::Constructor
					? (HasLocatedError(Engine) || HasAnyError(Engine))
					: HasLocatedAccessError(
						Engine,
						ExpectedDiagnosticFragment(
							Access,
							Member)),
				*Case.Describe(TEXT("illegal inheritance access should own its located diagnostic"))));
			if (Module != nullptr)
			{
				ASSERT_THAT(IsNull(
					Module->GetFunctionByName(
						"RunInheritanceAccess"),
					*Case.Describe(TEXT("illegal inheritance access should publish no callable entry"))));
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("inheritance access module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Case,
			Engine,
			*ScriptEngine,
			ModuleName);
	}

public:
	TEST_METHOD(AccessByMemberAndSite)
	{
		AS_NATIVE_PRODUCT("LANG-INH-ACCESS",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Bytecode
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FAccessCase& Access : AccessCases)
		{
			for (const FMemberCase& Member : MemberCases)
			{
				for (const FSiteCase& Site : SiteCases)
				{
					RunCell(Access, Member, Site);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
