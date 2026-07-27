#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

// Raw SDK global-property coverage.
// AngelscriptSDKGlobalPropertyTests.cpp
// Tests for as_globalproperty.cpp - global variable registration and access.
// Automation IDs: Angelscript.TestModule.AngelScriptSDK.TypeSystem.GlobalProperty.*

#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FGlobalPropertyTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.GlobalProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static int32 GTestValue = 0;
	inline static int32 GTestA = 0;
	inline static int32 GTestB = 0;
	inline static double GTestDouble = 0.0;
	inline static bool GTestBool = false;

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNoDiscardAsserter LocalAssert(*TestRunner);
		if (!LocalAssert.IsTrue(ScriptEngine->RegisterGlobalProperty("int GTestValue", &GTestValue) >= 0,
			TEXT("RegisterGlobalProperty int GTestValue should succeed")))
		{
			return;
		}
		if (!LocalAssert.IsTrue(ScriptEngine->RegisterGlobalProperty("int GTestA", &GTestA) >= 0,
			TEXT("RegisterGlobalProperty int GTestA should succeed")))
		{
			return;
		}
		if (!LocalAssert.IsTrue(ScriptEngine->RegisterGlobalProperty("int GTestB", &GTestB) >= 0,
			TEXT("RegisterGlobalProperty int GTestB should succeed")))
		{
			return;
		}
		if (!LocalAssert.IsTrue(ScriptEngine->RegisterGlobalProperty("double GScalar", &GTestDouble) >= 0,
			TEXT("RegisterGlobalProperty double GScalar should succeed")))
		{
			return;
		}
		if (!LocalAssert.IsTrue(ScriptEngine->RegisterGlobalProperty("double GTestDouble", &GTestDouble) >= 0,
			TEXT("RegisterGlobalProperty double GTestDouble should succeed")))
		{
			return;
		}
		if (!LocalAssert.IsTrue(ScriptEngine->RegisterGlobalProperty("bool GTestBool", &GTestBool) >= 0,
			TEXT("RegisterGlobalProperty bool GTestBool should succeed")))
		{
			return;
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
		GTestValue = 0;
		GTestA = 0;
		GTestB = 0;
		GTestDouble = 0.0;
		GTestBool = false;
	}

	TEST_METHOD(GlobalPropertiesByTypeAndAccess)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Runtime
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Global-property access product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const TCHAR* Types[] =
		{
			TEXT("int"),
			TEXT("double"),
			TEXT("bool"),
		};
		const TCHAR* Accesses[] =
		{
			TEXT("read"),
			TEXT("write"),
			TEXT("read-modify-write"),
		};

		for (const TCHAR* Type : Types)
		{
			for (const TCHAR* Access : Accesses)
			{
				const FString CaseId = MakeNativeCaseId(
					"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES",
					{ Type, Access });
				const FString ModuleName =
					TEXT("TypeGlobalProperty_")
					+ CaseId.Replace(TEXT("-"), TEXT("_"));
				FString Source;
				int32 ExpectedReturn = 0;

				if (FCString::Strcmp(Type, TEXT("int")) == 0)
				{
					GTestValue = 10;
					if (FCString::Strcmp(Access, TEXT("read")) == 0)
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestValue;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
						ExpectedReturn = 10;
					}
					else if (FCString::Strcmp(Access, TEXT("write")) == 0)
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\tGTestValue = 99;"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestValue;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
						ExpectedReturn = 99;
					}
					else
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\tGTestValue += 5;"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestValue;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
						ExpectedReturn = 15;
					}
				}
				else if (FCString::Strcmp(Type, TEXT("double")) == 0)
				{
					GTestDouble = 2.5;
					if (FCString::Strcmp(Access, TEXT("read")) == 0)
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestDouble == 2.5 ? 1 : 0;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
					}
					else if (FCString::Strcmp(Access, TEXT("write")) == 0)
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\tGTestDouble = 4.5;"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestDouble == 4.5 ? 1 : 0;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
					}
					else
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\tGTestDouble = GTestDouble * 2.0 + 1.0;"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestDouble == 6.0 ? 1 : 0;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
					}
					ExpectedReturn = 1;
				}
				else
				{
					GTestBool = false;
					if (FCString::Strcmp(Access, TEXT("read")) == 0)
					{
						GTestBool = true;
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestBool ? 1 : 0;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
					}
					else if (FCString::Strcmp(Access, TEXT("write")) == 0)
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\tGTestBool = true;"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestBool ? 1 : 0;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
					}
					else
					{
						AppendGeneratedAsLine(Source, TEXT("int Entry()"));
						AppendGeneratedAsLine(Source, TEXT("{"));
						AppendGeneratedAsLine(Source, TEXT("\tGTestBool = !GTestBool;"));
						AppendGeneratedAsLine(Source, TEXT("\treturn GTestBool ? 1 : 0;"));
						AppendGeneratedAsLine(Source, TEXT("}"));
					}
					ExpectedReturn = 1;
				}

				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					ModuleName,
					Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				FScopedNativeModule Module(
					*TestRunner,
					Engine,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get());
				if (!Module.IsValid())
				{
					continue;
				}

				asIScriptFunction* const Entry =
					GetNativeFunctionByExactDecl(Module, "int Entry()");
				ASSERT_THAT(IsNotNull(
					Entry,
					*FString::Printf(
						TEXT("%s should publish exact Entry metadata"),
						*CaseId)));
				if (Entry == nullptr)
				{
					continue;
				}
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asTYPEID_INT32),
					Entry->GetReturnTypeId(),
					*FString::Printf(
						TEXT("%s should preserve the integer observation ABI"),
						*CaseId)));

				{
					FSdkFunctionInvoker Invoker(
						*TestRunner,
						ScriptEngine,
						Module,
						"int Entry()");
					ASSERT_THAT(IsTrue(
						Invoker.IsValid(),
						*FString::Printf(
							TEXT("%s should prepare its exact entry"),
							*CaseId)));
					if (Invoker.IsValid())
					{
						ASSERT_THAT(AreEqual(
							ExpectedReturn,
							Invoker.CallAndReturn<int32>(INDEX_NONE),
							*FString::Printf(
								TEXT("%s should preserve native property storage and script access"),
								*CaseId)));
					}
				}

				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Module.Discard(),
					*FString::Printf(
						TEXT("%s should explicitly discard its generated module"),
						*CaseId)));
				ASSERT_THAT(IsNull(
					ScriptEngine->GetModule(
						ModuleNameUtf8.Get(),
						asGM_ONLY_IF_EXISTS),
					*FString::Printf(
						TEXT("%s module should be absent before the next independent cell"),
						*CaseId)));
			}
		}
	}

	TEST_METHOD(GlobalPropertyScriptReads)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES supersedes this int-read smoke through the complete int/double/bool by read/write/read-modify-write product");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestValue = 42;

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return GTestValue;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "GPRead", ScriptSource);
		if (!M.IsValid()) return;

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(42, Result, TEXT("Script should read C++ global value 42")));
	}

	TEST_METHOD(GlobalPropertyScriptWrites)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES supersedes this int-write smoke with exact type, metadata, runtime storage, cleanup, and isolation evidence");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestValue = 0;

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Entry()
			{
				GTestValue = 99;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "GPWrite", ScriptSource);
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		ASSERT_THAT(AreEqual(99, GTestValue, TEXT("C++ should see script-written value 99")));
	}

	TEST_METHOD(GlobalPropertyMultipleGlobals)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"AggregateSupport",
			"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES owns each registered property independently; this two-int aggregate remains compatibility evidence");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestA = 10;
		GTestB = 20;

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return GTestA + GTestB;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "GPMulti", ScriptSource);
		if (!M.IsValid()) return;

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "int Entry()", Result)) return;
		ASSERT_THAT(AreEqual(30, Result, TEXT("Script reads both globals: 10+20=30")));
	}

	TEST_METHOD(ScalarReadModifyWrite)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES supersedes this scalar-only mutation with all three registered types and all three access shapes");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		// This fork runs with asEP_FLOAT_IS_FLOAT64=1: the script-level scalar
		// float type is registered as `double` (8 bytes). Registering a global
		// property declared `float` is rejected (asINVALID_DECLARATION); the
		// supported scalar floating declaration is `double` backed by a C++ double.
		GTestDouble = 1.5;

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Entry()
			{
				GScalar = GScalar * 2.0 + 1.0;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "GPFloat", ScriptSource);
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GTestDouble, 4.0),
			TEXT("C++ should see script-written scalar 4.0")));
	}

	TEST_METHOD(GlobalPropertyDoubleProperty)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES supersedes this double-read smoke with double read, write, and read-modify-write execution");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestDouble = 2.5;

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			double Entry()
			{
				return GTestDouble * 4.0;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "GPDouble", ScriptSource);
		if (!M.IsValid()) return;

		double Result = 0.0;
		if (!ExecuteScriptFunction(*TestRunner, SE, M, "double Entry()", Result)) return;
		ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Result, 10.0),
			TEXT("Script reads C++ double and computes 10.0")));
	}

	TEST_METHOD(GlobalPropertyBoolProperty)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_NON_PRODUCT(
			"LegacyCompatibility",
			"TYPE-GLOBAL-PROPERTY-ACCESS-SHAPES supersedes this bool-toggle smoke with bool read, write, and read-modify-write execution");

		asIScriptEngine* SE = Engine.Get();
		ASSERT_THAT(IsNotNull(SE, TEXT("Should create engine")));

		GTestBool = false;

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			void Entry()
			{
				GTestBool = !GTestBool;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule M(*TestRunner, Engine, "GPBool", ScriptSource);
		if (!M.IsValid()) return;

		ExecuteScriptVoidFunction(*TestRunner, SE, M, "void Entry()");
		ASSERT_THAT(IsTrue(GTestBool, TEXT("C++ should see script-toggled bool true")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
