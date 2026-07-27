#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionReturnTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.Returns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FReturnTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		const ANSICHAR* ValueExpression;
		const ANSICHAR* IncompatibleExpression;
		ENativeScalarAccessor Accessor;
		int64 ExpectedInteger;
		double ExpectedFloatingPoint;
		bool bVoid;
		bool bScriptValue;
		bool bScriptReference;
		bool bNullReference;
	};

	struct FReturnPathCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FReturnTypeCase TypeCases[] =
	{
		{ "void", "void", "", "1", ENativeScalarAccessor::None, 0, 0.0, true, false, false, false },
		{ "int8", "int8", "int8(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::Byte, 7, 0.0, false, false, false, false },
		{ "int16", "int16", "int16(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::Word, 7, 0.0, false, false, false, false },
		{ "int", "int", "7", "FScriptCaseValue(7)", ENativeScalarAccessor::DWord, 7, 0.0, false, false, false, false },
		{ "int64", "int64", "int64(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::QWord, 7, 0.0, false, false, false, false },
		{ "uint8", "uint8", "uint8(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::Byte, 7, 0.0, false, false, false, false },
		{ "uint16", "uint16", "uint16(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::Word, 7, 0.0, false, false, false, false },
		{ "uint", "uint", "uint(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::DWord, 7, 0.0, false, false, false, false },
		{ "uint64", "uint64", "uint64(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::QWord, 7, 0.0, false, false, false, false },
		{ "float32", "float32", "7.25f", "FScriptCaseValue(7)", ENativeScalarAccessor::Float, 0, 7.25, false, false, false, false },
		{ "float64", "double", "7.5", "FScriptCaseValue(7)", ENativeScalarAccessor::Double, 0, 7.5, false, false, false, false },
		{ "bool", "bool", "true", "FScriptCaseValue(7)", ENativeScalarAccessor::Byte, 1, 0.0, false, false, false, false },
		{ "enum", "ENativeCaseEnum", "ENativeCaseEnum::One", "FScriptCaseValue(7)", ENativeScalarAccessor::DWord, 1, 0.0, false, false, false, false },
		{ "typedef", "NativeCaseAlias", "NativeCaseAlias(7)", "FScriptCaseValue(7)", ENativeScalarAccessor::DWord, 7, 0.0, false, false, false, false },
		{ "script_value", "FScriptCaseValue", "FScriptCaseValue(7)", "7", ENativeScalarAccessor::Object, 7, 0.0, false, true, false, false },
		{ "script_reference", "FScriptCaseReference", "FScriptCaseReference()", "7", ENativeScalarAccessor::Object, 7, 0.0, false, false, true, false },
		{ "null_reference", "FScriptCaseReference", "nullptr", "7", ENativeScalarAccessor::Object, 0, 0.0, false, false, false, true },
	};

	inline static constexpr FReturnPathCase PathCases[] =
	{
		{ "direct" },
		{ "if_else" },
		{ "switch" },
		{ "early" },
		{ "recursive_base" },
		{ "exception" },
		{ "missing_invalid" },
		{ "incompatible_invalid" },
	};

	static bool IsPath(const FReturnPathCase& PathCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(PathCase.CatalogName, Name) == 0;
	}

	static bool ShouldCompile(const FReturnTypeCase& TypeCase, const FReturnPathCase& PathCase)
	{
		if (IsPath(PathCase, "incompatible_invalid"))
		{
			return false;
		}
		if (IsPath(PathCase, "missing_invalid"))
		{
			return TypeCase.bVoid;
		}
		return true;
	}

	static void AppendReturnStatement(
		FString& Source,
		const FReturnTypeCase& TypeCase,
		const TCHAR* Indentation)
	{
		if (TypeCase.bVoid)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sreturn;"), Indentation));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sreturn %hs;"), Indentation, TypeCase.ValueExpression));
		}
	}

	static void AppendProbeBody(
		FString& Source,
		const FReturnTypeCase& TypeCase,
		const FReturnPathCase& PathCase,
		const FString& ProbeName)
	{
		if (IsPath(PathCase, "direct"))
		{
			AppendReturnStatement(Source, TypeCase, TEXT("\t"));
			return;
		}
		if (IsPath(PathCase, "if_else"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tbool bTakeFirst = true;"));
			AppendGeneratedAsLine(Source, TEXT("\tif (bTakeFirst)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\telse"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}
		if (IsPath(PathCase, "switch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Selector = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\tswitch (Selector)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\tcase 1:"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			return;
		}
		if (IsPath(PathCase, "early"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t"));
			return;
		}
		if (IsPath(PathCase, "exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Zero = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint Crash = 1 / Zero;"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Crash == 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t"));
			return;
		}
		if (IsPath(PathCase, "missing_invalid"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Marker = 1;"));
			AppendGeneratedAsLine(Source, TEXT("\tMarker += 1;"));
			return;
		}
		if (IsPath(PathCase, "incompatible_invalid"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %hs;"), TypeCase.IncompatibleExpression));
			return;
		}

		if (TypeCase.bVoid)
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s_Recursive(3);"), *ProbeName));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s_Recursive(3);"), *ProbeName));
		}
	}

	static FString BuildReturnSource(
		const FReturnTypeCase& TypeCase,
		const FReturnPathCase& PathCase)
	{
		const FString Suffix = FString::Printf(TEXT("%hs_%hs"), PathCase.CatalogName, TypeCase.CatalogName);
		const FString ProbeName = TEXT("Probe_") + Suffix;
		FString Source;
		AppendCoreLanguageTypeDeclarations(Source);
		AppendGeneratedAsLine(Source, TEXT("class FScriptCaseReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (IsPath(PathCase, "recursive_base"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%hs %s_Recursive(int Depth)"), TypeCase.ScriptType, *ProbeName));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Depth <= 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReturnStatement(Source, TypeCase, TEXT("\t\t"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			if (TypeCase.bVoid)
			{
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s_Recursive(Depth - 1);"), *ProbeName));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s_Recursive(Depth - 1);"), *ProbeName));
			}
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%hs %s()"), TypeCase.ScriptType, *ProbeName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendProbeBody(Source, TypeCase, PathCase, ProbeName);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages, const FString& ModuleName)
	{
		return Messages.Entries.ContainsByPredicate([&ModuleName](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == ModuleName
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	void AssertReturnValue(
		asIScriptContext& Context,
		const FReturnTypeCase& TypeCase,
		const FNativeCaseContext& Case)
	{
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::None:
			ASSERT_THAT(IsNotNull(Context.GetAddressOfReturnValue(),
				*Case.Describe(TEXT("void return should expose a stable context return slot in this fork"))));
			break;
		case ENativeScalarAccessor::Byte:
			ASSERT_THAT(AreEqual(static_cast<uint8>(TypeCase.ExpectedInteger), Context.GetReturnByte(),
				*Case.Describe(TEXT("byte return accessor should preserve the value"))));
			break;
		case ENativeScalarAccessor::Word:
			ASSERT_THAT(AreEqual(static_cast<uint16>(TypeCase.ExpectedInteger), Context.GetReturnWord(),
				*Case.Describe(TEXT("word return accessor should preserve the value"))));
			break;
		case ENativeScalarAccessor::DWord:
			ASSERT_THAT(AreEqual(static_cast<uint32>(TypeCase.ExpectedInteger), Context.GetReturnDWord(),
				*Case.Describe(TEXT("dword return accessor should preserve the value"))));
			break;
		case ENativeScalarAccessor::QWord:
			ASSERT_THAT(AreEqual(static_cast<uint64>(TypeCase.ExpectedInteger), Context.GetReturnQWord(),
				*Case.Describe(TEXT("qword return accessor should preserve the value"))));
			break;
		case ENativeScalarAccessor::Float:
			ASSERT_THAT(IsNear(TypeCase.ExpectedFloatingPoint, static_cast<double>(Context.GetReturnFloat()), 0.0001,
				*Case.Describe(TEXT("float return accessor should preserve the value"))));
			break;
		case ENativeScalarAccessor::Double:
			ASSERT_THAT(IsNear(TypeCase.ExpectedFloatingPoint, Context.GetReturnDouble(), 0.0001,
				*Case.Describe(TEXT("double return accessor should preserve the fork's double-backed ABI"))));
			break;
		case ENativeScalarAccessor::Object:
			if (TypeCase.bNullReference)
			{
				ASSERT_THAT(IsNull(Context.GetReturnObject(),
					*Case.Describe(TEXT("null reference return should expose a null object pointer"))));
			}
			else
			{
				const int32* const Value = static_cast<const int32*>(Context.GetReturnObject());
				ASSERT_THAT(IsNotNull(Value,
					*Case.Describe(TEXT("object return accessor should expose the returned object"))));
				if (Value != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(TypeCase.ExpectedInteger), *Value,
						*Case.Describe(TEXT("object return should preserve its first value field"))));
				}
			}
			break;
		default:
			ASSERT_THAT(IsTrue(false, *Case.Describe(TEXT("return product selected an unsupported accessor"))));
			break;
		}
	}

public:
	TEST_METHOD(ReturnTypesByControlPath)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-RETURN",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function return product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Function return product should register its core typedef through the raw SDK API")));

		for (const FReturnPathCase& PathCase : PathCases)
		{
			for (const FReturnTypeCase& TypeCase : TypeCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-FN-RETURN",
					{ ANSI_TO_TCHAR(PathCase.CatalogName), ANSI_TO_TCHAR(TypeCase.CatalogName) }));
				const FString Suffix = FString::Printf(TEXT("%hs_%hs"), PathCase.CatalogName, TypeCase.CatalogName);
				const FString ModuleName = TEXT("FunctionReturn_") + Suffix;
				const FString Source = BuildReturnSource(TypeCase, PathCase);
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

				if (!ShouldCompile(TypeCase, PathCase))
				{
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("invalid return path should fail compilation"))));
					ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
						*Case.Describe(TEXT("invalid return path should report a located section-owned diagnostic"))));
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("invalid return path should leave no retained module"))));
					continue;
				}

				ASSERT_THAT(IsTrue(BuildResult >= 0,
					*Case.Describe(TEXT("legal return path should compile"))));
				ASSERT_THAT(IsNotNull(Module,
					*Case.Describe(TEXT("legal return path should publish a module"))));
				if (BuildResult >= 0 && Module != nullptr)
				{
					const FString ProbeDeclaration = FString::Printf(TEXT("%hs Probe_%s()"), TypeCase.ScriptType, *Suffix);
					asIScriptFunction* const Probe = GetNativeFunctionByExactDecl(Module, TCHAR_TO_ANSI(*ProbeDeclaration));
					ASSERT_THAT(IsNotNull(Probe,
						*Case.Describe(TEXT("return probe should resolve by exact declaration"))));
					if (Probe != nullptr)
					{
						asDWORD ReturnFlags = 0;
						const int ReturnTypeId = Probe->GetReturnTypeId(&ReturnFlags);
						int ExpectedReturnTypeId = ScriptEngine->GetTypeIdByDecl(TypeCase.ScriptType);
						if (FCStringAnsi::Strcmp(TypeCase.CatalogName, "enum") == 0)
						{
							asITypeInfo* const EnumType = Module->GetTypeInfoByName(TypeCase.ScriptType);
							if (EnumType != nullptr)
							{
								ExpectedReturnTypeId = EnumType->GetTypeId();
							}
						}
						if (FCStringAnsi::Strcmp(TypeCase.CatalogName, "script_value") == 0)
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("Return metadata script_value expected=%d actual=%d engine=%d"),
								ExpectedReturnTypeId,
								ReturnTypeId,
								ScriptEngine->GetTypeIdByDecl(TypeCase.ScriptType)));
						}
						const bool bForkEnumReturnAlias = TypeCase.bScriptValue == false
							&& FCStringAnsi::Strcmp(TypeCase.CatalogName, "enum") == 0
							&& ReturnTypeId == asTYPEID_INT32;
						const bool bForkScriptValueReturnCategory = TypeCase.bScriptValue
							&& (ReturnTypeId & asTYPEID_MASK_OBJECT) == asTYPEID_SCRIPTOBJECT;
		const bool bForkScriptReferenceReturnCategory = (TypeCase.bScriptReference || TypeCase.bNullReference)
			&& (ReturnTypeId & asTYPEID_MASK_OBJECT) == asTYPEID_SCRIPTOBJECT
			&& (ReturnTypeId & asTYPEID_OBJHANDLE) != 0;
						ASSERT_THAT(IsTrue(ExpectedReturnTypeId == ReturnTypeId || bForkEnumReturnAlias || bForkScriptValueReturnCategory || bForkScriptReferenceReturnCategory,
							*Case.Describe(TEXT("return metadata should preserve the declared type"))));
						ASSERT_THAT(AreEqual(Probe, ScriptEngine->GetFunctionById(Probe->GetId()),
							*Case.Describe(TEXT("return declaration should round-trip to the same function identity"))));

						asIScriptContext* const Context = ScriptEngine->CreateContext();
						ASSERT_THAT(IsNotNull(Context,
							*Case.Describe(TEXT("return path should create an execution context"))));
						if (Context != nullptr)
						{
							const int ExecuteResult = PrepareAndExecute(Context, Probe);
							if (IsPath(PathCase, "exception"))
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
									*Case.Describe(TEXT("return path should expose the current fork exception behavior"))));
								ASSERT_THAT(IsTrue(Context->GetExceptionString() != nullptr && Context->GetExceptionString()[0] != '\0',
									*Case.Describe(TEXT("exceptional return path should expose exception text"))));
								ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber() > 0,
									*Case.Describe(TEXT("exceptional return path should expose a source line"))));
							}
							else
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
									*Case.Describe(TEXT("legal return path should finish"))));
								if (ExecuteResult == asEXECUTION_FINISHED)
								{
									AssertReturnValue(*Context, TypeCase, Case);
								}
							}
							Context->Release();
						}
					}
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("return cell should discard its module and return storage"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
