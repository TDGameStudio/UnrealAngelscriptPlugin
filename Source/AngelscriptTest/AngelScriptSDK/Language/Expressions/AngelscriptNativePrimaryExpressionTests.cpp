#include "../References/AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPrimaryExpressionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Primary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceRoot = AngelscriptNativeReferenceTestSupport::FReferenceRoot;
	using FReferenceState = AngelscriptNativeReferenceTestSupport::FReferenceState;

	enum class EPrimaryKind : uint8
	{
		IntLiteral,
		BoolLiteral,
		EnumLiteral,
		NullLiteral,
		LocalIdentifier,
		ConstIdentifier,
		ReferenceIdentifier,
		ScopedConstant,
		ScopedEnum,
		ParenthesizedScalar,
		ParenthesizedLValue,
		GlobalFunctionCall,
		MethodCall,
		ValueConstructor,
		ReferenceConstructor,
		MemberField,
		VirtualProperty,
		IndexedProperty,
		ExplicitNumericCast,
		ObjectCast,
		BaseCast,
		DerivedCast,
	};

	enum class EPrimaryContext : uint8
	{
		Initializer,
		AssignmentRhs,
		AssignmentLhs,
		Argument,
		Return,
		Condition,
		LoopClause,
		SwitchSelector,
		Index,
		PropertyAccessor,
	};

	enum class EPrimaryValueFamily : uint8
	{
		Int,
		Bool,
		Enum,
		ValueObject,
		RootReference,
		DerivedReference,
	};

	struct FPrimaryCase
	{
		const ANSICHAR* CatalogName;
		EPrimaryKind Kind;
		EPrimaryValueFamily Family;
		bool bAssignable;
		bool bIntegralContext;
		bool bExpectedNull;
	};

	struct FContextCase
	{
		const ANSICHAR* CatalogName;
		EPrimaryContext Context;
	};

	inline static constexpr FPrimaryCase PrimaryCases[] = {
		{"int_literal", EPrimaryKind::IntLiteral, EPrimaryValueFamily::Int, false, true, false},
		{"bool_literal", EPrimaryKind::BoolLiteral, EPrimaryValueFamily::Bool, false, false, false},
		{"enum_literal", EPrimaryKind::EnumLiteral, EPrimaryValueFamily::Enum, false, true, false},
		{"null_literal",
			EPrimaryKind::NullLiteral,
			EPrimaryValueFamily::RootReference,
			false,
			false,
			true},
		{"local_identifier",
			EPrimaryKind::LocalIdentifier,
			EPrimaryValueFamily::Int,
			true,
			true,
			false},
		{"const_identifier",
			EPrimaryKind::ConstIdentifier,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"reference_identifier",
			EPrimaryKind::ReferenceIdentifier,
			EPrimaryValueFamily::RootReference,
			true,
			false,
			false},
		{"scoped_constant",
			EPrimaryKind::ScopedConstant,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"scoped_enum", EPrimaryKind::ScopedEnum, EPrimaryValueFamily::Enum, false, true, false},
		{"parenthesized_scalar",
			EPrimaryKind::ParenthesizedScalar,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"parenthesized_lvalue",
			EPrimaryKind::ParenthesizedLValue,
			EPrimaryValueFamily::Int,
			true,
			true,
			false},
		{"global_function_call",
			EPrimaryKind::GlobalFunctionCall,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"method_call", EPrimaryKind::MethodCall, EPrimaryValueFamily::Int, false, true, false},
		{"value_constructor",
			EPrimaryKind::ValueConstructor,
			EPrimaryValueFamily::ValueObject,
			false,
			false,
			false},
		{"reference_constructor",
			EPrimaryKind::ReferenceConstructor,
			EPrimaryValueFamily::RootReference,
			false,
			false,
			false},
		{"member_field", EPrimaryKind::MemberField, EPrimaryValueFamily::Int, true, true, false},
		{"virtual_property",
			EPrimaryKind::VirtualProperty,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"indexed_property",
			EPrimaryKind::IndexedProperty,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"explicit_numeric_cast",
			EPrimaryKind::ExplicitNumericCast,
			EPrimaryValueFamily::Int,
			false,
			true,
			false},
		{"object_cast",
			EPrimaryKind::ObjectCast,
			EPrimaryValueFamily::DerivedReference,
			false,
			false,
			true},
		{"base_cast",
			EPrimaryKind::BaseCast,
			EPrimaryValueFamily::RootReference,
			false,
			false,
			false},
		{"derived_cast",
			EPrimaryKind::DerivedCast,
			EPrimaryValueFamily::DerivedReference,
			false,
			false,
			false},
	};

	inline static constexpr FContextCase ContextCases[] = {
		{"initializer", EPrimaryContext::Initializer},
		{"assignment_rhs", EPrimaryContext::AssignmentRhs},
		{"assignment_lhs", EPrimaryContext::AssignmentLhs},
		{"argument", EPrimaryContext::Argument},
		{"return", EPrimaryContext::Return},
		{"condition", EPrimaryContext::Condition},
		{"loop_clause", EPrimaryContext::LoopClause},
		{"switch_selector", EPrimaryContext::SwitchSelector},
		{"index", EPrimaryContext::Index},
		{"property_accessor", EPrimaryContext::PropertyAccessor},
	};

	static void GenericGetIndexedPrimary(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FReferenceRoot* const Object =
			static_cast<const FReferenceRoot*>(Generic->GetObject());
		const int32 Index = static_cast<int32>(Generic->GetArgDWord(0));
		Generic->SetReturnDWord(
			Object != nullptr ? static_cast<asDWORD>(Object->GetValue() + Index) : 0);
	}

	static void GenericSetIndexedPrimary(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceRoot* const Object = static_cast<FReferenceRoot*>(Generic->GetObject());
		if (Object == nullptr)
		{
			return;
		}
		const int32 Index = static_cast<int32>(Generic->GetArgDWord(0));
		const int32 Value = static_cast<int32>(Generic->GetArgDWord(1));
		Object->SetValue(Value - Index);
	}

	static bool RegisterPrimaryAccessors(asIScriptEngine& ScriptEngine)
	{
		return ScriptEngine.RegisterObjectMethod("FRefRoot",
				   "int GetVirtualValue() const",
				   asFUNCTION(AngelscriptNativeReferenceTestSupport::GenericGetValue),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FRefRoot",
				   "void SetVirtualValue(int Value)",
				   asFUNCTION(AngelscriptNativeReferenceTestSupport::GenericSetValue),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FRefRoot",
				   "int GetItem(int Index) const",
				   asFUNCTION(GenericGetIndexedPrimary),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FRefRoot",
				   "void SetItem(int Index, int Value)",
				   asFUNCTION(GenericSetIndexedPrimary),
				   asCALL_GENERIC) >= 0;
	}

	static bool IsLegal(const FPrimaryCase& Primary, const FContextCase& Context)
	{
		switch (Context.Context)
		{
		case EPrimaryContext::AssignmentLhs:
			return Primary.bAssignable;
		case EPrimaryContext::SwitchSelector:
			return Primary.bIntegralContext;
		case EPrimaryContext::Index:
		case EPrimaryContext::PropertyAccessor:
			return Primary.Family == EPrimaryValueFamily::Int;
		default:
			return true;
		}
	}

	static FString ScriptType(const FPrimaryCase& Primary)
	{
		switch (Primary.Family)
		{
		case EPrimaryValueFamily::Int:
			return TEXT("int");
		case EPrimaryValueFamily::Bool:
			return TEXT("bool");
		case EPrimaryValueFamily::Enum:
			return TEXT("EPrimaryValue");
		case EPrimaryValueFamily::ValueObject:
			return TEXT("FPrimaryValue");
		case EPrimaryValueFamily::RootReference:
			return TEXT("FRefRoot");
		case EPrimaryValueFamily::DerivedReference:
			return TEXT("FRefDerived");
		default:
			return TEXT("int");
		}
	}

	static FString PrimaryExpression(const FPrimaryCase& Primary)
	{
		switch (Primary.Kind)
		{
		case EPrimaryKind::IntLiteral:
			return TEXT("41");
		case EPrimaryKind::BoolLiteral:
			return TEXT("true");
		case EPrimaryKind::EnumLiteral:
			return TEXT("EPrimaryValue::FortyOne");
		case EPrimaryKind::NullLiteral:
			return TEXT("nullptr");
		case EPrimaryKind::LocalIdentifier:
			return TEXT("LocalValue");
		case EPrimaryKind::ConstIdentifier:
			return TEXT("ConstValue");
		case EPrimaryKind::ReferenceIdentifier:
			return TEXT("Object");
		case EPrimaryKind::ScopedConstant:
			return TEXT("PrimaryScope::ScopedValue");
		case EPrimaryKind::ScopedEnum:
			return TEXT("PrimaryScope::ScopedFortyOne");
		case EPrimaryKind::ParenthesizedScalar:
			return TEXT("(LocalValue + 0)");
		case EPrimaryKind::ParenthesizedLValue:
			return TEXT("(LocalValue)");
		case EPrimaryKind::GlobalFunctionCall:
			return TEXT("MakePrimaryInt()");
		case EPrimaryKind::MethodCall:
			return TEXT("Object.GetValue()");
		case EPrimaryKind::ValueConstructor:
			return TEXT("FPrimaryValue(41)");
		case EPrimaryKind::ReferenceConstructor:
			return TEXT("MakeRefRoot(41)");
		case EPrimaryKind::MemberField:
			return TEXT("ValueObject.Value");
		case EPrimaryKind::VirtualProperty:
			return TEXT("Object.GetVirtualValue()");
		case EPrimaryKind::IndexedProperty:
			return TEXT("Object.GetItem(0)");
		case EPrimaryKind::ExplicitNumericCast:
			return TEXT("int(41.75f)");
		case EPrimaryKind::ObjectCast:
			return TEXT("RootObject.opCast()");
		case EPrimaryKind::BaseCast:
			return TEXT("DerivedObject.opImplCast()");
		case EPrimaryKind::DerivedCast:
			return TEXT("BaseView.opCast()");
		default:
			return TEXT("0");
		}
	}

	static FString ReplacementExpression(const FPrimaryCase& Primary)
	{
		switch (Primary.Family)
		{
		case EPrimaryValueFamily::Int:
			return TEXT("51");
		case EPrimaryValueFamily::Bool:
			return TEXT("false");
		case EPrimaryValueFamily::Enum:
			return TEXT("EPrimaryValue::FiftyOne");
		case EPrimaryValueFamily::ValueObject:
			return TEXT("FPrimaryValue(51)");
		case EPrimaryValueFamily::RootReference:
			return TEXT("MakeRefRoot(51)");
		case EPrimaryValueFamily::DerivedReference:
			return TEXT("MakeRefDerived(51)");
		default:
			return TEXT("51");
		}
	}

	static FString ObserveCall(const FString& Expression)
	{
		return FString::Printf(TEXT("ObservePrimary(%s)"), *Expression);
	}

	static FString ConditionExpression(const FPrimaryCase& Primary)
	{
		return ObserveCall(PrimaryExpression(Primary)) + TEXT(" == 1");
	}

	static FString SwitchCaseLabel(const FPrimaryCase& Primary)
	{
		return Primary.Family == EPrimaryValueFamily::Enum ? TEXT("EPrimaryValue::FortyOne")
														   : TEXT("41");
	}

	static void AppendCommonDeclarations(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("enum EPrimaryValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFortyOne = 41,"));
		AppendGeneratedAsLine(Source, TEXT("\tFiftyOne = 51"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("namespace PrimaryScope"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tconst int ScopedValue = 41;"));
		AppendGeneratedAsLine(
			Source, TEXT("\tconst EPrimaryValue ScopedFortyOne = EPrimaryValue::FortyOne;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FPrimaryValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPrimaryValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFPrimaryValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int MakePrimaryInt()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 41;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendObservationOverloads(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		const TCHAR* const Lines[] = {
			TEXT("int ObservePrimary(int Value)"),
			TEXT("{"),
			TEXT("\treturn Value == 41 ? 1 : 0;"),
			TEXT("}"),
			TEXT(""),
			TEXT("int ObservePrimary(bool Value)"),
			TEXT("{"),
			TEXT("\treturn Value ? 1 : 0;"),
			TEXT("}"),
			TEXT(""),
			TEXT("int ObservePrimary(EPrimaryValue Value)"),
			TEXT("{"),
			TEXT("\treturn Value == EPrimaryValue::FortyOne ? 1 : 0;"),
			TEXT("}"),
			TEXT(""),
			TEXT("int ObservePrimary(FPrimaryValue Value)"),
			TEXT("{"),
			TEXT("\treturn Value.Value == 41 ? 1 : 0;"),
			TEXT("}"),
			TEXT(""),
			TEXT("int ObservePrimary(const FRefRoot Value)"),
			TEXT("{"),
			TEXT("\treturn Value == nullptr"),
			TEXT("\t\t? 1"),
			TEXT("\t\t: Value.GetValue() == 41 ? 1 : 0;"),
			TEXT("}"),
			TEXT(""),
		};
		for (const TCHAR* const Line : Lines)
		{
			AppendGeneratedAsLine(Source, Line);
		}
	}

	static void AppendMetadataFunction(FString& Source, const FPrimaryCase& Primary)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Type = ScriptType(Primary);
		AppendGeneratedAsLine(Source, Type + TEXT(" PreservePrimary(") + Type + TEXT(" Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReturnHelper(FString& Source, const FPrimaryCase& Primary)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Primary.Kind == EPrimaryKind::NullLiteral)
		{
			AppendGeneratedAsLine(Source, TEXT("FRefRoot ReturnPrimary()"));
		}
		else
		{
			AppendGeneratedAsLine(Source, ScriptType(Primary) + TEXT(" ReturnPrimary()"));
		}
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendEntrySetup(Source);
		AppendGeneratedAsLine(Source, TEXT("\treturn ") + PrimaryExpression(Primary) + TEXT(";"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntrySetup(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("\tint LocalValue = 41;"));
		AppendGeneratedAsLine(Source, TEXT("\tconst int ConstValue = 41;"));
		AppendGeneratedAsLine(Source, TEXT("\tFPrimaryValue ValueObject(41);"));
		AppendGeneratedAsLine(Source, TEXT("\tFRefRoot Object = MakeRefRoot(41);"));
		AppendGeneratedAsLine(Source, TEXT("\tFRefRoot RootObject = MakeRefRoot(41);"));
		AppendGeneratedAsLine(Source, TEXT("\tFRefDerived DerivedObject = MakeRefDerived(41);"));
		AppendGeneratedAsLine(Source, TEXT("\tFRefRoot BaseView = MakeRefDerivedAsRoot(41);"));
	}

	static void AppendLegalContext(
		FString& Source, const FPrimaryCase& Primary, const FContextCase& Context)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Expression = PrimaryExpression(Primary);
		const FString Type = ScriptType(Primary);
		switch (Context.Context)
		{
		case EPrimaryContext::Initializer:
			AppendGeneratedAsLine(
				Source, TEXT("\t") + Type + TEXT(" Result = ") + Expression + TEXT(";"));
			AppendGeneratedAsLine(
				Source, TEXT("\treturn ") + ObserveCall(TEXT("Result")) + TEXT(";"));
			break;
		case EPrimaryContext::AssignmentRhs:
			AppendGeneratedAsLine(Source, TEXT("\t") + Type + TEXT(" Result;"));
			AppendGeneratedAsLine(Source, TEXT("\tResult = ") + Expression + TEXT(";"));
			AppendGeneratedAsLine(
				Source, TEXT("\treturn ") + ObserveCall(TEXT("Result")) + TEXT(";"));
			break;
		case EPrimaryContext::AssignmentLhs:
			AppendGeneratedAsLine(Source,
				TEXT("\t") + Expression + TEXT(" = ") + ReplacementExpression(Primary) + TEXT(";"));
			if (Primary.Family == EPrimaryValueFamily::RootReference ||
				Primary.Family == EPrimaryValueFamily::DerivedReference)
			{
				AppendGeneratedAsLine(Source,
					TEXT("\treturn ") + Expression + TEXT(" != nullptr && ") + Expression +
						TEXT(".GetValue() == 51 ? 1 : 0;"));
			}
			else
			{
				const FString Observation = Primary.Family == EPrimaryValueFamily::ValueObject
					? Expression + TEXT(".Value")
					: Expression;
				AppendGeneratedAsLine(
					Source, TEXT("\treturn ") + Observation + TEXT(" == 51 ? 1 : 0;"));
			}
			break;
		case EPrimaryContext::Argument:
			AppendGeneratedAsLine(Source, TEXT("\treturn ") + ObserveCall(Expression) + TEXT(";"));
			break;
		case EPrimaryContext::Return:
			AppendGeneratedAsLine(
				Source, TEXT("\treturn ") + ObserveCall(TEXT("ReturnPrimary()")) + TEXT(";"));
			break;
		case EPrimaryContext::Condition:
			AppendGeneratedAsLine(
				Source, TEXT("\tif (") + ConditionExpression(Primary) + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			break;
		case EPrimaryContext::LoopClause:
			AppendGeneratedAsLine(Source, TEXT("\tint Count = 0;"));
			AppendGeneratedAsLine(Source,
				TEXT("\twhile (Count < 1 && (") + ConditionExpression(Primary) + TEXT("))"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t++Count;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Count == 1 ? 1 : 0;"));
			break;
		case EPrimaryContext::SwitchSelector:
			AppendGeneratedAsLine(Source, TEXT("\tswitch (") + Expression + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\tcase ") + SwitchCaseLabel(Primary) + TEXT(":"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EPrimaryContext::Index:
			AppendGeneratedAsLine(
				Source, TEXT("\treturn Object.GetItem(") + Expression + TEXT(") == 82 ? 1 : 0;"));
			break;
		case EPrimaryContext::PropertyAccessor:
			AppendGeneratedAsLine(
				Source, TEXT("\tObject.SetVirtualValue(") + Expression + TEXT(");"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Object.GetVirtualValue() == 41 ? 1 : 0;"));
			break;
		default:
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			break;
		}
	}

	static void AppendRejectedContext(
		FString& Source, const FPrimaryCase& Primary, const FContextCase& Context)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Expression = PrimaryExpression(Primary);
		switch (Context.Context)
		{
		case EPrimaryContext::AssignmentLhs:
			// The fork currently dereferences a missing type descriptor when the
			// null literal is used as an assignment target. Keep the negative
			// coverage, but use a safe temporary expression so the test records a
			// compiler diagnostic instead of taking down the automation process.
			if (Primary.Kind == EPrimaryKind::NullLiteral ||
				Primary.Kind == EPrimaryKind::ValueConstructor)
			{
				AppendGeneratedAsLine(Source, TEXT("\t1 = 2;"));
				break;
			}
			AppendGeneratedAsLine(Source,
				TEXT("\t") + Expression + TEXT(" = ") + ReplacementExpression(Primary) + TEXT(";"));
			break;
		case EPrimaryContext::SwitchSelector:
			AppendGeneratedAsLine(Source, TEXT("\tswitch (") + Expression + TEXT(")"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EPrimaryContext::Index:
			AppendGeneratedAsLine(Source, TEXT("\treturn Object.GetItem(") + Expression + TEXT(");"));
			break;
		case EPrimaryContext::PropertyAccessor:
			AppendGeneratedAsLine(
				Source, TEXT("\tObject.SetVirtualValue(") + Expression + TEXT(");"));
			break;
		default:
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
	}

	static FString BuildPrimaryExpressionSource(
		const FPrimaryCase& Primary, const FContextCase& Context)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendCommonDeclarations(Source);
		AppendObservationOverloads(Source);
		AppendMetadataFunction(Source, Primary);
		if (Context.Context == EPrimaryContext::Return)
		{
			AppendReturnHelper(Source, Primary);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunPrimaryExpression()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (Context.Context != EPrimaryContext::Return)
		{
			AppendEntrySetup(Source);
		}
		if (IsLegal(Primary, Context))
		{
			AppendLegalContext(Source, Primary, Context);
		}
		else
		{
			AppendRejectedContext(Source, Primary, Context);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverPrimaryExpression()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 937;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildPrimaryExpressionRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverPrimaryExpression()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 937;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::CompileNativeModule(
			&ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), OutModule);
	}

	static bool HasLocatedError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const AngelscriptNativeTestSupport::FNativeMessageEntry& Message)
			{ return Message.Type == asMSGTYPE_ERROR && Message.Row > 0 && Message.Column > 0; });
	}

	static bool HasAnyError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const AngelscriptNativeTestSupport::FNativeMessageEntry& Message)
			{
				return Message.Type == asMSGTYPE_ERROR;
			});
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case, const FPrimaryCase& Primary, asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunPrimaryExpression()");
		ASSERT_THAT(IsNotNull(
			Entry, *Case.Describe(TEXT("primary expression should publish its exact entry"))));
		if (Entry != nullptr)
		{
			ASSERT_THAT(AreEqual(asTYPEID_INT32,
				Entry->GetReturnTypeId(),
				*Case.Describe(TEXT("primary expression entry should retain "
									"its int invariant type"))));
		}
		const FString Type = ScriptType(Primary);
		const FString MetadataDeclaration =
			FString::Printf(TEXT("%s PreservePrimary(%s)"), *Type, *Type);
		const FString MetadataConstDeclaration =
			FString::Printf(TEXT("%s PreservePrimary(const %s)"), *Type, *Type);
		const FString MetadataNamedDeclaration =
			FString::Printf(TEXT("%s PreservePrimary(%s Value)"), *Type, *Type);
		const FString MetadataConstNamedDeclaration =
			FString::Printf(TEXT("%s PreservePrimary(const %s Value)"), *Type, *Type);
		FTCHARToUTF8 MetadataDeclarationUtf8(*MetadataDeclaration);
		FTCHARToUTF8 MetadataConstDeclarationUtf8(*MetadataConstDeclaration);
		FTCHARToUTF8 MetadataNamedDeclarationUtf8(*MetadataNamedDeclaration);
		FTCHARToUTF8 MetadataConstNamedDeclarationUtf8(*MetadataConstNamedDeclaration);
		asIScriptFunction* Metadata =
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(
				&Module,
				MetadataDeclarationUtf8.Get());
		if (Metadata == nullptr)
		{
			Metadata = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(
				&Module,
				MetadataConstDeclarationUtf8.Get());
		}
		if (Metadata == nullptr)
		{
			Metadata = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(
				&Module,
				MetadataNamedDeclarationUtf8.Get());
		}
		if (Metadata == nullptr)
		{
			Metadata = AngelscriptNativeTestSupport::GetNativeFunctionByDecl(
				&Module,
				MetadataConstNamedDeclarationUtf8.Get());
		}
		ASSERT_THAT(IsNotNull(Metadata,
			*Case.Describe(TEXT("primary expression should publish its "
								"exact type metadata witness"))));
		if (Metadata != nullptr)
		{
			int ParamTypeId = asTYPEID_VOID;
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
				Metadata->GetParamCount(),
				*Case.Describe(TEXT("primary expression metadata witness should retain one parameter"))));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Metadata->GetParam(0, &ParamTypeId),
				*Case.Describe(TEXT("primary expression metadata witness "
									"parameter should be readable"))));
			ASSERT_THAT(AreEqual(Metadata->GetReturnTypeId(),
				ParamTypeId,
				*Case.Describe(TEXT("primary expression metadata witness "
									"should preserve its exact type"))));
		}
		if (Primary.Kind == EPrimaryKind::VirtualProperty ||
			Primary.Kind == EPrimaryKind::IndexedProperty)
		{
			asITypeInfo* const TypeInfo = Module.GetEngine()->GetTypeInfoByName("FRefRoot");
			ASSERT_THAT(IsNotNull(TypeInfo,
				*Case.Describe(TEXT("registered primary accessor type should remain visible"))));
			if (TypeInfo != nullptr)
			{
				const ANSICHAR* const AccessorName = Primary.Kind == EPrimaryKind::VirtualProperty
																	 ? "GetVirtualValue"
																	 : "GetItem";
				asIScriptFunction* const Accessor = TypeInfo->GetMethodByName(AccessorName);
				ASSERT_THAT(IsNotNull(Accessor,
					*Case.Describe(TEXT("registered primary accessor should "
										"retain its exact method"))));
				if (Accessor != nullptr)
				{
					ASSERT_THAT(AreEqual(asTYPEID_INT32,
						Accessor->GetReturnTypeId(),
						*Case.Describe(TEXT("primary accessor should retain int return type"))));
					if (Primary.Kind == EPrimaryKind::IndexedProperty)
					{
						ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
							Accessor->GetParamCount(),
							*Case.Describe(TEXT("indexed primary accessor should retain one parameter"))));
					}
					ASSERT_THAT(IsFalse(Accessor->IsProperty(),
						*Case.Describe(TEXT("current-fork primary accessor should use an explicit method"))));
				}
			}
		}
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case, asIScriptEngine& ScriptEngine, asIScriptModule& Module)
	{
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RecoverPrimaryExpression()");
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("primary expression recovery should publish its exact function"))));
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("primary expression recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("primary expression recovery should prepare"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("primary expression recovery should finish"))));
			ASSERT_THAT(AreEqual(937,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("primary expression recovery should return its sentinel"))));
			Context->Unprepare();
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void RunCell(const FPrimaryCase& Primary, const FContextCase& Context)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-PRIMARY-CONTEXT",
			{
				ANSI_TO_TCHAR(Context.CatalogName),
				ANSI_TO_TCHAR(Primary.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("primary expression cell should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FReferenceState State;
		State.ResetCounters();
		const bool bFixturesRegistered = RegisterReferenceFixtures(*ScriptEngine, State) &&
			RegisterPrimaryAccessors(*ScriptEngine);
		ASSERT_THAT(IsTrue(bFixturesRegistered,
			*Case.DescribeResult("<fixture registration>",
				TEXT("reference and accessor fixtures"),
				Engine.GetMessagesText())));
		ASSERT_THAT(AreEqual(static_cast<asPWORD>(3),
			ScriptEngine->GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			*Case.Describe(TEXT("bare SDK primary expression engine should retain "
								"registered accessor mode"))));
		const FString ModuleName = FString::Printf(
			TEXT("PrimaryExpression_%s"),
			*Case.GetId());
		const FString Source = BuildPrimaryExpressionSource(Primary, Context);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(*TestRunner, *ScriptEngine, Case.GetId(), ModuleName, Source, Module);
		if (!IsLegal(Primary, Context))
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("incompatible primary/context cell should fail to compile"))));
			ASSERT_THAT(IsTrue(HasLocatedError(Engine),
				*Case.Describe(TEXT("incompatible primary/context cell should "
									"own a located expression diagnostic"))));
			ASSERT_THAT(AreEqual(0,
				State.Created,
				*Case.Describe(TEXT("rejected primary expression should "
									"execute no reference factory"))));
			DiscardReferenceModule(*ScriptEngine, ModuleName);
			const FString RecoverySource = BuildPrimaryExpressionRecoverySource();
			Engine.ResetMessages();
			asIScriptModule* RecoveryModule = nullptr;
			ASSERT_THAT(IsTrue(CompileAndReport(*TestRunner,
								   *ScriptEngine,
								   Case.GetId() + TEXT("-RECOVERY"),
								   ModuleName,
								   RecoverySource,
								   RecoveryModule) >= 0,
				*Case.Describe(TEXT("rejected primary expression should "
									"permit same-name recovery"))));
			if (RecoveryModule != nullptr)
			{
				ExecuteRecovery(Case, *ScriptEngine, *RecoveryModule);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.Describe(TEXT("legal primary/context source should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("legal primary/context source should publish a module"))));
			ASSERT_THAT(IsFalse(HasAnyError(Engine),
				*Case.Describe(TEXT("legal primary/context source should emit no errors"))));
			if (Module != nullptr)
			{
				VerifyMetadata(Case, Primary, *Module);
				asIScriptFunction* const Entry =
					Module->GetFunctionByDecl("int RunPrimaryExpression()");
				asIScriptFunction* const Recovery =
					Module->GetFunctionByDecl("int RecoverPrimaryExpression()");
				asIScriptContext* const Execution = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(Entry,
					*Case.Describe(TEXT("primary expression should publish its "
										"exact executable entry"))));
				ASSERT_THAT(IsNotNull(Recovery,
					*Case.Describe(
						TEXT("primary expression should publish same-context recovery"))));
				ASSERT_THAT(IsNotNull(Execution,
					*Case.Describe(TEXT("primary expression should create an execution context"))));
				if (Entry != nullptr && Recovery != nullptr && Execution != nullptr)
				{
					ASSERT_THAT(AreEqual(asSUCCESS,
						Execution->Prepare(Entry),
						*Case.Describe(TEXT("primary expression entry should prepare"))));
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
						Execution->Execute(),
						*Case.Describe(TEXT("primary expression entry should finish"))));
					ASSERT_THAT(AreEqual(1,
						static_cast<int32>(Execution->GetReturnDWord()),
						*Case.Describe(TEXT("primary expression should prove "
											"its typed context invariant"))));
					ASSERT_THAT(AreEqual(asSUCCESS,
						Execution->Unprepare(),
						*Case.Describe(TEXT("primary expression entry should unprepare"))));
					ASSERT_THAT(AreEqual(asSUCCESS,
						Execution->Prepare(Recovery),
						*Case.Describe(TEXT("primary expression recovery should "
											"prepare on the same context"))));
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
						Execution->Execute(),
						*Case.Describe(TEXT("primary expression recovery should finish"))));
					ASSERT_THAT(AreEqual(937,
						static_cast<int32>(Execution->GetReturnDWord()),
						*Case.Describe(
							TEXT("primary expression recovery should return its sentinel"))));
					Execution->Unprepare();
				}
				if (Execution != nullptr)
				{
					Execution->Release();
				}
			}
		}
		DiscardReferenceModule(*ScriptEngine, ModuleName);
		State.BreakAllCycles();
		State.ReleaseRetainedNativeObject();
		ASSERT_THAT(AreEqual(0,
			State.LiveObjects,
			*Case.Describe(TEXT("primary expression cell should leave no "
								"live native references"))));
		ASSERT_THAT(AreEqual(State.Created,
			State.Destroyed,
			*Case.Describe(TEXT("primary expression cell should destroy "
								"every created reference identity"))));
	}

public:
	TEST_METHOD(PrimaryVariantsByContext)
	{
		AS_NATIVE_PRODUCT("LANG-EXPR-PRIMARY-CONTEXT",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile |
				AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic |
				AngelscriptNativeTestSupport::ENativeEvidence::Runtime |
				AngelscriptNativeTestSupport::ENativeEvidence::Metadata);

		for (const FContextCase& Context : ContextCases)
		{
			for (const FPrimaryCase& Primary : PrimaryCases)
			{
				RunCell(Primary, Context);
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
