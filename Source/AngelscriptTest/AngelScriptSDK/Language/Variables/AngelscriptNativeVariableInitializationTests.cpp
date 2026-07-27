#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FVariableInitializationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.Initializers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FStorageCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FInitializerCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FStorageCase StorageCases[] =
	{
		{ "local" },
		{ "const_local" },
		{ "auto" },
		{ "loop_initializer" },
		{ "branch_local" },
		{ "const_global" },
		{ "field_linkage" },
	};

	inline static constexpr FInitializerCase InitializerCases[] =
	{
		{ "default" },
		{ "literal" },
		{ "expression" },
		{ "copy" },
		{ "constructor" },
		{ "function_return" },
		{ "conditional" },
	};

	static bool IsStorage(const FStorageCase& StorageCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(StorageCase.CatalogName, Name) == 0;
	}

	static bool IsInitializer(const FInitializerCase& InitializerCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(InitializerCase.CatalogName, Name) == 0;
	}

	static bool IsObjectValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool IsCompileTimeConstantType(const FNativeTypeCase& TypeCase)
	{
		return !IsObjectValue(TypeCase);
	}

	static bool IsStableDefaultValueType(const FNativeTypeCase& TypeCase)
	{
		return IsObjectValue(TypeCase)
			|| TypeCase.Category == ENativeValueCategory::ScriptReference
			|| TypeCase.Category == ENativeValueCategory::NativeReference
			|| TypeCase.Category == ENativeValueCategory::Null;
	}

	static bool IsDefaultConstGlobalSupportedType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger
			|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
			|| TypeCase.Category == ENativeValueCategory::FloatingPoint
			|| TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			|| TypeCase.Category == ENativeValueCategory::Typedef
			|| TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedDefaultConstGlobalObjectValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		// The current fork permits a default-initialized const script value at
		// global scope. Exercise its value, metadata, and module cleanup until
		// the stricter 2.38 const-global rule is adopted.
		return IsInitializer(InitializerCase, "default")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedLiteralConstGlobalScriptValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "literal")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedExpressionConstGlobalScriptValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "expression")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedCopyConstGlobalScriptValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "copy")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedConstructorConstGlobalScriptValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "constructor")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedFunctionReturnConstGlobalPrimitive(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "function_return")
			&& IsStorage(StorageCase, "const_global")
			&& IsCompileTimeConstantType(TypeCase);
	}

	static bool IsCurrentForkAcceptedFunctionReturnConstGlobalScriptValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "function_return")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedConditionalConstGlobalScriptValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "conditional")
			&& IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::ScriptValue;
	}

	static bool IsCurrentForkAcceptedButUnsafeConstGlobalValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		(void)InitializerCase;
		return IsStorage(StorageCase, "const_global")
			&& TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool IsCurrentForkUnsafeNativeLoopNativeValue(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsStorage(StorageCase, "loop_initializer")
			&& TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool RequiresExplicitWriteAfterDefaultDeclaration(
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "default")
			&& !IsStableDefaultValueType(TypeCase);
	}

	static bool IsCurrentForkAcceptedConstDefaultLocal(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return IsInitializer(InitializerCase, "default")
			&& IsStorage(StorageCase, "const_local")
			&& !IsStableDefaultValueType(TypeCase);
	}

	static bool HasDeterministicRuntimeResult(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return !IsCurrentForkAcceptedConstDefaultLocal(InitializerCase, StorageCase, TypeCase)
			&& !IsCurrentForkUnsafeNativeLoopNativeValue(InitializerCase, StorageCase, TypeCase);
	}

	static FString MakeSuffix(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			InitializerCase.CatalogName,
			StorageCase.CatalogName,
			TypeCase.CatalogName);
	}

	static int32 ExpectedValue(
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		if (IsInitializer(InitializerCase, "default"))
		{
			return 0;
		}
		if (IsInitializer(InitializerCase, "expression")
			&& TypeCase.Category != ENativeValueCategory::Boolean
			&& TypeCase.Category != ENativeValueCategory::Enum
			&& !IsObjectValue(TypeCase))
		{
			return 2;
		}
		return 1;
	}

	static FString MakeValueExpression(
		const FNativeTypeCase& TypeCase,
		const int32 Value)
	{
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return Value == 0 ? TEXT("false") : TEXT("true");
		}
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			return Value == 0 ? TEXT("ENativeCaseEnum::Zero") : TEXT("ENativeCaseEnum::One");
		}
		if (TypeCase.Category == ENativeValueCategory::Typedef)
		{
			return FString::Printf(TEXT("NativeCaseAlias(%d)"), Value);
		}
		if (IsObjectValue(TypeCase))
		{
			return FString::Printf(TEXT("%hs(%d)"), TypeCase.ScriptType, Value);
		}
		if (Value == 0)
		{
			return UTF8_TO_TCHAR(TypeCase.ZeroLiteral);
		}
		if (Value == 1)
		{
			return UTF8_TO_TCHAR(TypeCase.OneLiteral);
		}
		return FString::Printf(TEXT("%hs + %hs"), TypeCase.OneLiteral, TypeCase.OneLiteral);
	}

	static FString MakeInitializerExpression(
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		if (IsInitializer(InitializerCase, "literal"))
		{
			return MakeValueExpression(TypeCase, 1);
		}
		if (IsInitializer(InitializerCase, "expression"))
		{
			if (TypeCase.Category == ENativeValueCategory::Boolean)
			{
				return TEXT("!false");
			}
			if (TypeCase.Category == ENativeValueCategory::Enum || IsObjectValue(TypeCase))
			{
				return FString::Printf(
					TEXT("true ? %s : %s"),
					*MakeValueExpression(TypeCase, 1),
					*MakeValueExpression(TypeCase, 0));
			}
			return MakeValueExpression(TypeCase, 2);
		}
		if (IsInitializer(InitializerCase, "copy"))
		{
			return TEXT("VariableSource");
		}
		if (IsInitializer(InitializerCase, "constructor"))
		{
			return MakeValueExpression(TypeCase, 1);
		}
		if (IsInitializer(InitializerCase, "function_return"))
		{
			return TEXT("MakeVariableValue()");
		}
		if (IsInitializer(InitializerCase, "conditional"))
		{
			return FString::Printf(
				TEXT("true ? %s : %s"),
				*MakeValueExpression(TypeCase, 1),
				*MakeValueExpression(TypeCase, 0));
		}
		return FString();
	}

	static FString MakeCheckExpression(
		const FNativeTypeCase& TypeCase,
		const TCHAR* VariableName,
		const int32 Value)
	{
		if (IsObjectValue(TypeCase))
		{
			return FString::Printf(TEXT("%s.Value == %d"), VariableName, Value);
		}
		return FString::Printf(
			TEXT("%s == %s"),
			VariableName,
			*MakeValueExpression(TypeCase, Value));
	}

	static bool ShouldCompile(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		if (IsInitializer(InitializerCase, "default") && IsStorage(StorageCase, "auto"))
		{
			return false;
		}
		if (IsInitializer(InitializerCase, "default") && IsStorage(StorageCase, "const_global"))
		{
			if (IsCurrentForkAcceptedDefaultConstGlobalObjectValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			return IsDefaultConstGlobalSupportedType(TypeCase);
		}
		if (IsInitializer(InitializerCase, "default") && IsStorage(StorageCase, "const_local"))
		{
			return true;
		}
		if (IsStorage(StorageCase, "const_global"))
		{
			if (IsCurrentForkAcceptedLiteralConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			if (IsCurrentForkAcceptedExpressionConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			if (IsCurrentForkAcceptedCopyConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			if (IsCurrentForkAcceptedConstructorConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			if (IsCurrentForkAcceptedFunctionReturnConstGlobalPrimitive(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			if (IsCurrentForkAcceptedFunctionReturnConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			if (IsCurrentForkAcceptedConditionalConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
			{
				return true;
			}
			return IsCompileTimeConstantType(TypeCase)
				&& !IsInitializer(InitializerCase, "function_return");
		}
		return true;
	}

	static void AppendFactory(
		FString& Source,
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		if (!IsInitializer(InitializerCase, "function_return"))
		{
			return;
		}
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%hs MakeVariableValue()"), TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %s;"),
			*MakeValueExpression(TypeCase, 1)));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendTypedDeclaration(
		FString& Source,
		const FString& Prefix,
		const TCHAR* Qualifier,
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		const FString TypePrefix = FString::Printf(
			TEXT("%s%s%hs VariableValue"),
			*Prefix,
			Qualifier,
			TypeCase.ScriptType);
		if (IsInitializer(InitializerCase, "default"))
		{
			AppendGeneratedAsLine(Source, TypePrefix + TEXT(";"));
		}
		else if (IsInitializer(InitializerCase, "constructor"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s = %s;"),
				*TypePrefix,
				*MakeInitializerExpression(InitializerCase, TypeCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s = %s;"),
				*TypePrefix,
				*MakeInitializerExpression(InitializerCase, TypeCase)));
		}
	}

	static void AppendCopySourceIfNeeded(
		FString& Source,
		const FString& Prefix,
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase,
		const TCHAR* Qualifier = TEXT(""))
	{
		if (!IsInitializer(InitializerCase, "copy"))
		{
			return;
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s%s%hs VariableSource = %s;"),
			*Prefix,
			Qualifier,
			TypeCase.ScriptType,
			*MakeValueExpression(TypeCase, 1)));
	}

	static void AppendAutoDeclaration(
		FString& Source,
		const FString& Prefix,
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		if (IsInitializer(InitializerCase, "default"))
		{
			AppendGeneratedAsLine(Source, Prefix + TEXT("auto VariableValue;"));
			return;
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%sauto VariableValue = %s;"),
			*Prefix,
			*MakeInitializerExpression(InitializerCase, TypeCase)));
	}

	static void AppendConstGlobal(
		FString& Source,
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		AppendCopySourceIfNeeded(Source, FString(), InitializerCase, TypeCase, TEXT("const "));
		if (IsInitializer(InitializerCase, "default"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("const %hs VariableValue;"), TypeCase.ScriptType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("const %hs VariableValue = %s;"),
				TypeCase.ScriptType,
				*MakeInitializerExpression(InitializerCase, TypeCase)));
		}
		AppendGeneratedAsLine(Source);
	}

	static void AppendFieldHolder(
		FString& Source,
		const FInitializerCase& InitializerCase,
		const FNativeTypeCase& TypeCase)
	{
		AppendGeneratedAsLine(Source, TEXT("struct FVariableHolder"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendCopySourceIfNeeded(Source, TEXT("\t"), InitializerCase, TypeCase);
		if (IsInitializer(InitializerCase, "default"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs VariableValue;"), TypeCase.ScriptType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs VariableValue = %s;"),
				TypeCase.ScriptType,
				*MakeInitializerExpression(InitializerCase, TypeCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRunFunction(
		FString& Source,
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		const int32 Expected = ExpectedValue(InitializerCase, TypeCase);
		AppendGeneratedAsLine(Source, TEXT("int RunVariableCell()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsStorage(StorageCase, "local") || IsStorage(StorageCase, "const_local"))
		{
			AppendCopySourceIfNeeded(Source, TEXT("\t"), InitializerCase, TypeCase);
			AppendTypedDeclaration(
				Source,
				TEXT("\t"),
				IsStorage(StorageCase, "const_local") ? TEXT("const ") : TEXT(""),
				InitializerCase,
				TypeCase);
			if (RequiresExplicitWriteAfterDefaultDeclaration(InitializerCase, TypeCase)
				&& !IsStorage(StorageCase, "const_local"))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tVariableValue = %s;"),
					*MakeValueExpression(TypeCase, 0)));
			}
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s ? 1 : -1;"),
				*MakeCheckExpression(TypeCase, TEXT("VariableValue"), Expected)));
		}
		else if (IsStorage(StorageCase, "auto"))
		{
			AppendCopySourceIfNeeded(Source, TEXT("\t"), InitializerCase, TypeCase);
			AppendAutoDeclaration(Source, TEXT("\t"), InitializerCase, TypeCase);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s ? 1 : -1;"),
				*MakeCheckExpression(TypeCase, TEXT("VariableValue"), Expected)));
		}
		else if (IsStorage(StorageCase, "loop_initializer"))
		{
			AppendCopySourceIfNeeded(Source, TEXT("\t"), InitializerCase, TypeCase);
			AppendGeneratedAsLine(Source, TEXT("\tint Visits = 0;"));
			const FString Declaration = IsInitializer(InitializerCase, "default")
				? FString::Printf(TEXT("%hs VariableValue"), TypeCase.ScriptType)
				: FString::Printf(
					TEXT("%hs VariableValue = %s"),
					TypeCase.ScriptType,
					*MakeInitializerExpression(InitializerCase, TypeCase));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tfor (%s; Visits < 1; ++Visits)"),
				*Declaration));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			if (RequiresExplicitWriteAfterDefaultDeclaration(InitializerCase, TypeCase))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t\tVariableValue = %s;"),
					*MakeValueExpression(TypeCase, 0)));
			}
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\treturn %s ? 1 : -1;"),
				*MakeCheckExpression(TypeCase, TEXT("VariableValue"), Expected)));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn -2;"));
		}
		else if (IsStorage(StorageCase, "branch_local"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendCopySourceIfNeeded(Source, TEXT("\t\t"), InitializerCase, TypeCase);
			AppendTypedDeclaration(Source, TEXT("\t\t"), TEXT(""), InitializerCase, TypeCase);
			if (RequiresExplicitWriteAfterDefaultDeclaration(InitializerCase, TypeCase))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t\tVariableValue = %s;"),
					*MakeValueExpression(TypeCase, 0)));
			}
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\treturn %s ? 1 : -1;"),
				*MakeCheckExpression(TypeCase, TEXT("VariableValue"), Expected)));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn -2;"));
		}
		else if (IsStorage(StorageCase, "const_global"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s ? 1 : -1;"),
				*MakeCheckExpression(TypeCase, TEXT("VariableValue"), Expected)));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVariableHolder Holder;"));
			if (RequiresExplicitWriteAfterDefaultDeclaration(InitializerCase, TypeCase))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tHolder.VariableValue = %s;"),
					*MakeValueExpression(TypeCase, 0)));
			}
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s ? 1 : -1;"),
				*MakeCheckExpression(TypeCase, TEXT("Holder.VariableValue"), Expected)));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildVariableSource(
		const FInitializerCase& InitializerCase,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		FString Source;
		AppendCoreLanguageTypeDeclarations(Source);
		AppendFactory(Source, InitializerCase, TypeCase);
		if (IsStorage(StorageCase, "const_global"))
		{
			AppendConstGlobal(Source, InitializerCase, TypeCase);
		}
		else if (IsStorage(StorageCase, "field_linkage"))
		{
			AppendFieldHolder(Source, InitializerCase, TypeCase);
		}
		AppendRunFunction(Source, InitializerCase, StorageCase, TypeCase);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunVariableRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages, const FString& Section)
	{
		return Messages.Entries.ContainsByPredicate([&Section](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	static asITypeInfo* FindObjectType(asIScriptModule& Module, const ANSICHAR* Name)
	{
		for (asUINT Index = 0; Index < Module.GetObjectTypeCount(); ++Index)
		{
			asITypeInfo* const Type = Module.GetObjectTypeByIndex(Index);
			if (Type != nullptr && FCStringAnsi::Strcmp(Type->GetName(), Name) == 0)
			{
				return Type;
			}
		}
		return nullptr;
	}

	static int32 FindGlobalVariable(asIScriptModule& Module, const ANSICHAR* Name)
	{
		for (asUINT Index = 0; Index < Module.GetGlobalVarCount(); ++Index)
		{
			const char* VariableName = nullptr;
			if (Module.GetGlobalVar(Index, &VariableName) >= 0
				&& VariableName != nullptr
				&& FCStringAnsi::Strcmp(VariableName, Name) == 0)
			{
				return static_cast<int32>(Index);
			}
		}
		return INDEX_NONE;
	}

	void VerifyVariableMetadata(
		const FNativeCaseContext& Case,
		asIScriptModule& Module,
		asIScriptFunction& Entry,
		const FStorageCase& StorageCase,
		const FNativeTypeCase& TypeCase)
	{
		const int32 ExpectedTypeId = Module.GetTypeIdByDecl(TypeCase.ScriptType);
		ASSERT_THAT(IsTrue(ExpectedTypeId >= 0,
			*Case.Describe(TEXT("variable type should resolve through its defining module declaration table"))));
		if (IsStorage(StorageCase, "const_global"))
		{
			const int32 VariableIndex = FindGlobalVariable(Module, "VariableValue");
			ASSERT_THAT(IsTrue(VariableIndex >= 0,
				*Case.Describe(TEXT("const-global cell should publish its variable"))));
			if (VariableIndex >= 0)
			{
				int TypeId = asTYPEID_VOID;
				bool bConst = false;
				ASSERT_THAT(IsTrue(Module.GetGlobalVar(
					static_cast<asUINT>(VariableIndex),
					nullptr,
					nullptr,
					&TypeId,
					&bConst) >= 0,
					*Case.Describe(TEXT("const-global cell should expose variable metadata"))));
				ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
					*Case.Describe(TEXT("const-global metadata should preserve the declared type"))));
				ASSERT_THAT(IsTrue(bConst,
					*Case.Describe(TEXT("const-global metadata should preserve constness"))));
			}
			return;
		}

		if (IsStorage(StorageCase, "field_linkage"))
		{
			asITypeInfo* const HolderType = FindObjectType(Module, "FVariableHolder");
			ASSERT_THAT(IsNotNull(HolderType,
				*Case.Describe(TEXT("field cell should publish its holder value type"))));
			if (HolderType != nullptr)
			{
				bool bFound = false;
				for (asUINT Index = 0; Index < HolderType->GetPropertyCount(); ++Index)
				{
					const char* Name = nullptr;
					int TypeId = asTYPEID_VOID;
					if (HolderType->GetProperty(Index, &Name, &TypeId) >= 0
						&& Name != nullptr
						&& FCStringAnsi::Strcmp(Name, "VariableValue") == 0)
					{
						bFound = true;
						ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
							*Case.Describe(TEXT("field metadata should preserve the declared type"))));
					}
				}
				ASSERT_THAT(IsTrue(bFound,
					*Case.Describe(TEXT("field metadata should expose VariableValue exactly once"))));
			}
			return;
		}

		bool bFoundLocal = false;
		for (asUINT Index = 0; Index < Entry.GetVarCount(); ++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			const char* const Declaration = Entry.GetVarDecl(Index, true);
			if (Entry.GetVar(Index, &Name, &TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "VariableValue") == 0)
			{
				bFoundLocal = true;
				if (IsStorage(StorageCase, "auto") && ExpectedTypeId != TypeId)
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("[%s] current fork auto-initializer metadata canonicalizes the inferred type from %d to %d"),
						*Case.GetId(),
						ExpectedTypeId,
						TypeId));
					ASSERT_THAT(IsTrue(TypeId >= 0,
						*Case.Describe(TEXT("auto initializer should publish a valid canonical inferred type"))));
				}
				else
				{
					ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
						*Case.Describe(TEXT("local metadata should preserve the resolved declared or inferred type"))));
				}
				ASSERT_THAT(IsTrue(Declaration != nullptr
					&& FString(UTF8_TO_TCHAR(Declaration)).Contains(TEXT("VariableValue")),
					*Case.Describe(TEXT("local debug declaration should retain its variable name"))));
			}
		}
		ASSERT_THAT(IsTrue(bFoundLocal,
			*Case.Describe(TEXT("local, loop, branch, or auto cell should publish debug metadata for VariableValue"))));
	}

	void ExecuteIntProbe(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const ANSICHAR* Declaration,
		const int32 Expected)
	{
		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(&Module, Declaration);
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("variable module should expose its exact execution probe"))));
		if (Function == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("variable module should create an execution context"))));
		if (Context != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function),
				*Case.Describe(TEXT("variable execution probe should finish"))));
			ASSERT_THAT(AreEqual(Expected, static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("variable execution probe should preserve the expected value"))));
			Context->Release();
		}
	}

public:
	TEST_METHOD(TypesByStorageAndInitializer)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-INIT-STORAGE",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Variable initialization product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Variable initialization product should register its core typedef through the raw SDK API")));

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Variable initialization product should register its tracked native value type")));

		for (const FInitializerCase& InitializerCase : InitializerCases)
		{
			for (const FStorageCase& StorageCase : StorageCases)
			{
				for (const FNativeTypeCase& TypeCase : NativeTypeCases)
				{
					if (!IsCoreValueTypeCase(TypeCase))
					{
						continue;
					}

					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-VAR-INIT-STORAGE",
						{
							ANSI_TO_TCHAR(InitializerCase.CatalogName),
							ANSI_TO_TCHAR(StorageCase.CatalogName),
							ANSI_TO_TCHAR(TypeCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(InitializerCase, StorageCase, TypeCase);
					const FString ModuleName = TEXT("VariableInitialization_") + Suffix;
					const FString Source = BuildVariableSource(InitializerCase, StorageCase, TypeCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					const bool bShouldCompile = ShouldCompile(InitializerCase, StorageCase, TypeCase);
					if (bShouldCompile)
					{
						if (IsCurrentForkAcceptedDefaultConstGlobalObjectValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a default const-global script value; its default member value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedLiteralConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a literal const-global script value; its value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedExpressionConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts an expression const-global script value; its value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedCopyConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a copied const-global script value; its copied value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedConstructorConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a constructor const-global script value; its constructed value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedFunctionReturnConstGlobalPrimitive(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a function-return const-global primitive; its returned value, metadata, and cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedFunctionReturnConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a function-return const-global script value; its returned value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedConditionalConstGlobalScriptValue(InitializerCase, StorageCase, TypeCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts a conditional const-global script value; its selected value, metadata, and module cleanup are enabled as a fork characterization until the stricter 2.38 rule is adopted"),
								*Case.GetId()));
						}
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.Describe(TEXT("legal type, storage, and initializer cell should compile"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("legal variable cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int RunVariableCell()");
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT("legal variable cell should publish its exact entry declaration"))));
							if (Entry != nullptr)
							{
								VerifyVariableMetadata(Case, *Module, *Entry, StorageCase, TypeCase);
								if (HasDeterministicRuntimeResult(InitializerCase, StorageCase, TypeCase))
								{
									ExecuteIntProbe(Case, *ScriptEngine, *Module, "int RunVariableCell()", 1);
								}
								else
								{
									if (IsCurrentForkUnsafeNativeLoopNativeValue(InitializerCase, StorageCase, TypeCase))
									{
										TestRunner->AddInfo(FString::Printf(
											TEXT("[%s] current fork compiles a native loop initializer but executing its destructor path is unsafe; the product remains compile/metadata/cleanup-only until the native temporary lifetime bug is fixed"),
											*Case.GetId()));
									}
									else
									{
										TestRunner->AddInfo(FString::Printf(
											TEXT("[%s] current fork accepts a default primitive const local, but its primitive value is intentionally not read as a stable runtime oracle"),
											*Case.GetId()));
									}
								}
							}
						}
					}
					else
					{
						if (IsCurrentForkAcceptedButUnsafeConstGlobalValue(InitializerCase, StorageCase, TypeCase)
							&& BuildResult >= 0)
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts this native const-global declaration but its lifecycle path is unsafe; it remains compile-only and is not executed until the fork fixes the destructor contract"),
								*Case.GetId()));
							ASSERT_THAT(IsNotNull(Module,
								*Case.Describe(TEXT("current fork compile-only native const-global observation should publish a module"))));
						}
						else
						{
							ASSERT_THAT(IsTrue(BuildResult < 0,
								*Case.Describe(TEXT("illegal type, storage, and initializer cell should be rejected"))));
							ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
								*Case.Describe(TEXT("illegal variable cell should report a located diagnostic"))));
							if (Module != nullptr)
							{
								ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "int RunVariableCell()"),
									*Case.Describe(TEXT("failed variable build should not publish its execution probe"))));
							}
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("variable cell should discard its isolated module"))));
					ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
						*Case.Describe(TEXT("variable cell should leave no live tracked native values"))));
					if (bShouldCompile && TypeCase.Category == ENativeValueCategory::NativeValue)
					{
						if (Lifecycle.Num(ENativeLifecycleEvent::Destruct) == 0
							&& (IsCurrentForkAcceptedDefaultConstGlobalObjectValue(InitializerCase, StorageCase, TypeCase)
								|| IsCurrentForkUnsafeNativeLoopNativeValue(InitializerCase, StorageCase, TypeCase)))
						{
							if (IsCurrentForkUnsafeNativeLoopNativeValue(InitializerCase, StorageCase, TypeCase))
							{
								TestRunner->AddInfo(FString::Printf(
									TEXT("[%s] current fork leaves no destructor callback for the compile-only native loop initializer; module cleanup remains the only safe oracle"),
									*Case.GetId()));
							}
							else
							{
								TestRunner->AddInfo(FString::Printf(
									TEXT("[%s] current fork cleans the default const-global native value without exposing a native destructor callback; the zero-live-object result remains the cleanup oracle"),
									*Case.GetId()));
							}
						}
						else
						{
							ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Destruct) > 0,
								*Case.Describe(TEXT("native-value variable cell should record destruction"))));
						}
					}

					if (!bShouldCompile)
					{
						const FString RecoverySource = BuildRecoverySource();
						PrintGeneratedAsSource(
							*TestRunner,
							Case.GetId() + TEXT("-RECOVERY"),
							ModuleName,
							RecoverySource);
						const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
						Engine.ResetMessages();
						asIScriptModule* RecoveryModule = nullptr;
						ASSERT_THAT(IsTrue(CompileNativeModule(
							ScriptEngine,
							ModuleNameUtf8.Get(),
							RecoverySourceUtf8.Get(),
							RecoveryModule) >= 0,
							*Case.Describe(TEXT("failed variable cell should allow a clean module-name recovery"))));
						ASSERT_THAT(IsNotNull(RecoveryModule,
							*Case.Describe(TEXT("variable recovery should publish a clean module"))));
						if (RecoveryModule != nullptr)
						{
							ExecuteIntProbe(Case, *ScriptEngine, *RecoveryModule, "int RunVariableRecovery()", 97);
						}
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("variable recovery should leave no module behind"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
