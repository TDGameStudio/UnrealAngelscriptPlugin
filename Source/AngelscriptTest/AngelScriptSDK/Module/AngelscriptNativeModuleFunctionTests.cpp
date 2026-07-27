#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleFunctionTests, "Angelscript.TestModule.AngelScriptSDK.Module.Functions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asIScriptModule* CreateScriptModule(asIScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return ScriptEngine != nullptr
			? ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE)
			: nullptr;
	}

	static FString FormatPointer(const void* Pointer)
	{
		return FString::Printf(TEXT("%p"), Pointer);
	}

	static FString DescribeObjectTypes(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT TypeCount = Module->GetObjectTypeCount();
		for (asUINT TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = Module->GetObjectTypeByIndex(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(TypeInfo->GetName());
		}

		return Result.IsEmpty() ? TEXT("<no object types>") : Result;
	}

	static FString DescribeGlobals(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* Declaration = Module->GetGlobalVarDeclaration(GlobalIndex, true);
			if (Declaration == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(Declaration);
		}

		return Result.IsEmpty() ? TEXT("<no globals>") : Result;
	}

	static FString DescribeTypeInfoList(asIScriptModule* Module, asUINT Count, asITypeInfo* (asIScriptModule::*Getter)(asUINT) const, const TCHAR* EmptyText)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		for (asUINT TypeIndex = 0; TypeIndex < Count; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = (Module->*Getter)(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			const char* Namespace = TypeInfo->GetNamespace();
			if (Namespace != nullptr && Namespace[0] != '\0')
			{
				Result += UTF8_TO_TCHAR(Namespace);
				Result += TEXT("::");
			}
			Result += UTF8_TO_TCHAR(TypeInfo->GetName());
		}

		return Result.IsEmpty() ? EmptyText : Result;
	}

	static int32 FindGlobalVarIndexByName(asIScriptModule* Module, const char* Name)
	{
		if (Module == nullptr || Name == nullptr)
		{
			return INDEX_NONE;
		}

		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* GlobalName = nullptr;
			if (Module->GetGlobalVar(GlobalIndex, &GlobalName) >= 0 &&
				GlobalName != nullptr &&
				FCStringAnsi::Strcmp(GlobalName, Name) == 0)
			{
				return static_cast<int32>(GlobalIndex);
			}
		}

		return INDEX_NONE;
	}

	static asIScriptFunction* FindFunctionByNameAndNamespace(asIScriptModule* Module, const char* Name, const char* Namespace)
	{
		if (Module == nullptr || Name == nullptr || Namespace == nullptr)
		{
			return nullptr;
		}

		const asUINT FunctionCount = Module->GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(FunctionIndex);
			if (Function != nullptr &&
				FCStringAnsi::Strcmp(Function->GetName(), Name) == 0 &&
				FCStringAnsi::Strcmp(Function->GetNamespace(), Namespace) == 0)
			{
				return Function;
			}
		}

		return nullptr;
	}

	static void LogModuleState(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const TCHAR* Stage)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		Test.AddInfo(FString::Printf(
			TEXT("ScriptModule state [%s]: engineModuleCount=%u module=%s name=%s defaultNamespace=%s functions={%s} globals={%s} objectTypes={%s} enums={%s} typedefs={%s} imports=%u"),
			Stage != nullptr ? Stage : TEXT("<unknown>"),
			ScriptEngine != nullptr ? ScriptEngine->GetModuleCount() : 0,
			*FormatPointer(Module),
			Module != nullptr ? UTF8_TO_TCHAR(Module->GetName()) : TEXT("<null>"),
			Module != nullptr ? UTF8_TO_TCHAR(Module->GetDefaultNamespace()) : TEXT("<null>"),
			*CollectFunctionDeclarations(Module),
			*DescribeGlobals(Module),
			*DescribeObjectTypes(Module),
			*DescribeTypeInfoList(Module, Module != nullptr ? Module->GetEnumCount() : 0, &asIScriptModule::GetEnumByIndex, TEXT("<no enums>")),
			*DescribeTypeInfoList(Module, Module != nullptr ? Module->GetTypedefCount() : 0, &asIScriptModule::GetTypedefByIndex, TEXT("<no typedefs>")),
			Module != nullptr ? Module->GetImportedFunctionCount() : 0));
	}

public:

	TEST_METHOD(EnumerateFunctions)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("MOD-FUNCTION-INVENTORY-RUNTIME",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule enumerate test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Alpha()
			{
				return 1;
			}

			int Beta()
			{
				return 2;
			}

			int Gamma()
			{
				return 3;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleEnumerate", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(3, static_cast<int32>(Module->GetFunctionCount()), TEXT("ScriptModule enumerate test should report three functions")));
		asIScriptFunction* const IndexedAlpha = Module->GetFunctionByIndex(0);
		asIScriptFunction* const IndexedBeta = Module->GetFunctionByIndex(1);
		asIScriptFunction* const IndexedGamma = Module->GetFunctionByIndex(2);
		ASSERT_THAT(IsNotNull(IndexedAlpha, TEXT("ScriptModule enumerate test should expose the first indexed function")));
		ASSERT_THAT(IsNotNull(IndexedBeta, TEXT("ScriptModule enumerate test should expose the second indexed function")));
		ASSERT_THAT(IsNotNull(IndexedGamma, TEXT("ScriptModule enumerate test should expose the third indexed function")));
		if (IndexedAlpha == nullptr || IndexedBeta == nullptr || IndexedGamma == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha")), FString(UTF8_TO_TCHAR(IndexedAlpha->GetName())),
			TEXT("ScriptModule enumerate test should preserve Alpha as the first function")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta")), FString(UTF8_TO_TCHAR(IndexedBeta->GetName())),
			TEXT("ScriptModule enumerate test should preserve Beta as the second function")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gamma")), FString(UTF8_TO_TCHAR(IndexedGamma->GetName())),
			TEXT("ScriptModule enumerate test should preserve Gamma as the third function")));
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("enumerate-after-build"));

		bool bFoundBeta = false;
		for (asUINT Index = 0; Index < Module->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(Index);
			if (Function != nullptr && FString(UTF8_TO_TCHAR(Function->GetName())) == TEXT("Beta"))
			{
				bFoundBeta = true;
			}
		}

		ASSERT_THAT(IsTrue(bFoundBeta, TEXT("ScriptModule enumerate test should find Beta via GetFunctionByIndex")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Gamma()"), TEXT("ScriptModule enumerate test should resolve Gamma by declaration")));

		int32 AlphaResult = 0;
		int32 BetaResult = 0;
		int32 GammaResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Alpha()", AlphaResult) ||
			!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Beta()", BetaResult) ||
			!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Gamma()", GammaResult))
		{
			return;
		}

		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule enumerate execution: Alpha=%d Beta=%d Gamma=%d functions={%s}"),
			AlphaResult,
			BetaResult,
			GammaResult,
			*CollectFunctionDeclarations(Module)));
		ASSERT_THAT(AreEqual(1, AlphaResult, TEXT("ScriptModule enumerate test should execute Alpha")));
		ASSERT_THAT(AreEqual(2, BetaResult, TEXT("ScriptModule enumerate test should execute Beta")));
		ASSERT_THAT(AreEqual(3, GammaResult, TEXT("ScriptModule enumerate test should execute Gamma")));

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("ScriptModule enumerate test should create an explicit execution context")));
		if (Context != nullptr)
		{
			asIScriptFunction* BetaFunction = Module->GetFunctionByDecl("int Beta()");
			ASSERT_THAT(IsNotNull(BetaFunction, TEXT("ScriptModule enumerate test should resolve Beta before preparing the explicit context")));
			if (BetaFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(Context->Prepare(BetaFunction)),
					TEXT("ScriptModule enumerate test should prepare Beta on the explicit context")));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), static_cast<int32>(Context->Execute()),
					TEXT("ScriptModule enumerate test should execute Beta on the explicit context")));
				ASSERT_THAT(AreEqual(2, static_cast<int32>(Context->GetReturnDWord()),
					TEXT("ScriptModule enumerate test should read Beta's exact return value from the explicit context")));
				ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(Context->Unprepare()),
					TEXT("ScriptModule enumerate test should unprepare the explicit context before module cleanup")));
			}
			Context->Release();
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(Module.Discard()),
			TEXT("ScriptModule enumerate test should explicitly discard the owning module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("ScriptModuleEnumerate", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule enumerate test should remove the discarded module from the engine")));

		AngelscriptNativeTestSupport::FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("ScriptModule enumerate test should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != ScriptEngine, TEXT("ScriptModule enumerate test should use a distinct engine for isolation")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule("ScriptModuleEnumerate", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule enumerate test should not publish the module into an independent engine")));
	}
	TEST_METHOD(ModuleFunctionsPreserveDeclaredReturnTypes)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-FUNCTION-INVENTORY-RUNTIME", "scalar_return_types");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule return-value coverage should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			bool ReturnBool()
			{
				return true;
			}

			int8 ReturnInt8()
			{
				return -128;
			}

			uint8 ReturnUInt8()
			{
				return 255;
			}

			int16 ReturnInt16()
			{
				return -32768;
			}

			uint16 ReturnUInt16()
			{
				return 65535;
			}

			int ReturnInt()
			{
				return -123456789;
			}

			uint ReturnUInt()
			{
				return 4000000000;
			}

			int64 ReturnInt64()
			{
				return -9000000000;
			}

			uint64 ReturnUInt64()
			{
				return 9000000000;
			}

			float ReturnFloat()
			{
				return 3.5;
			}

			double ReturnDouble()
			{
				return 6.25;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleReturnTypes", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("return-values-after-build"));

		ASSERT_THAT(AreEqual(11, static_cast<int32>(Module->GetFunctionCount()), TEXT("ScriptModule return-value coverage should expose every return function")));
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool ReturnBool()");
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("ScriptModule return-value coverage should execute bool return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int8 ReturnInt8()");
			ASSERT_THAT(AreEqual(static_cast<int8>(-128), Invoker.CallAndReturn<int8>(0), TEXT("ScriptModule return-value coverage should execute int8 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint8 ReturnUInt8()");
			ASSERT_THAT(AreEqual(static_cast<uint8>(255), Invoker.CallAndReturn<uint8>(0), TEXT("ScriptModule return-value coverage should execute uint8 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int16 ReturnInt16()");
			ASSERT_THAT(AreEqual(static_cast<int16>(-32768), Invoker.CallAndReturn<int16>(0), TEXT("ScriptModule return-value coverage should execute int16 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint16 ReturnUInt16()");
			ASSERT_THAT(AreEqual(static_cast<uint16>(65535), Invoker.CallAndReturn<uint16>(0), TEXT("ScriptModule return-value coverage should execute uint16 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int ReturnInt()");
			ASSERT_THAT(AreEqual(-123456789, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("ScriptModule return-value coverage should execute int return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint ReturnUInt()");
			ASSERT_THAT(AreEqual(static_cast<uint32>(4000000000u), Invoker.CallAndReturn<uint32>(0), TEXT("ScriptModule return-value coverage should execute uint return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int64 ReturnInt64()");
			ASSERT_THAT(AreEqual(static_cast<int64>(-9000000000ll), Invoker.CallAndReturn<int64>(0), TEXT("ScriptModule return-value coverage should execute int64 return")));
		}
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint64 ReturnUInt64()");
			ASSERT_THAT(AreEqual(static_cast<uint64>(9000000000ull), Invoker.CallAndReturn<uint64>(0), TEXT("ScriptModule return-value coverage should execute uint64 return")));
		}

		double FloatResult = 0.0;
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float ReturnFloat()");
			FloatResult = Invoker.CallAndReturn<double>(0.0);
		}
		double DoubleResult = 0.0;
		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float ReturnDouble()");
			DoubleResult = Invoker.CallAndReturn<double>(0.0);
		}
		ASSERT_THAT(IsNear(3.5, FloatResult, 0.0001, TEXT("ScriptModule return-value coverage should execute float source return through the fork double ABI slot")));
		ASSERT_THAT(IsNear(6.25, DoubleResult, 0.0001, TEXT("ScriptModule return-value coverage should execute double return")));
		TestRunner->AddInfo(FString::Printf(
			TEXT("ScriptModule return-value execution: float=%f double=%f functions={%s}"),
			FloatResult,
			DoubleResult,
			*CollectFunctionDeclarations(Module)));
	}
	TEST_METHOD(FunctionArgumentReturnRoundTripExecutesModuleFunctions)
	{
		AS_NATIVE_PRODUCT_PART("MOD-FUNCTION-INVENTORY-RUNTIME", "argument_return_round_trip");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule argument roundtrip test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Sum(int A, int B)
			{
				return A + B;
			}

			uint64 Mix(uint64 High, uint64 Low)
			{
				return (High << 32) | Low;
			}

			double Scale(double Value, double Multiplier)
			{
				return Value * Multiplier;
			}

			bool IsInside(int Value, int Min, int Max)
			{
				return Value >= Min && Value <= Max;
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleArgumentReturnRoundTrip", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("argument-roundtrip-after-build"));

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "int Sum(const int, const int)");
			Invoker.AddArg(static_cast<int32>(-10)).AddArg(static_cast<int32>(52));
			ASSERT_THAT(AreEqual(42, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("ScriptModule argument roundtrip test should return Sum(-10,52)=42")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "uint64 Mix(const uint64, const uint64)");
			Invoker.AddArg(static_cast<uint64>(0x1234ull)).AddArg(static_cast<uint64>(0xABCDull));
			ASSERT_THAT(AreEqual(static_cast<uint64>(0x12340000ABCDull), Invoker.CallAndReturn<uint64>(0), TEXT("ScriptModule argument roundtrip test should preserve uint64 arguments and return")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "float Scale(const float, const float)");
			Invoker.AddArg(7.5).AddArg(4.0);
			const double Result = Invoker.CallAndReturn<double>(0.0);
			ASSERT_THAT(IsNear(30.0, Result, 0.0001, TEXT("ScriptModule argument roundtrip test should return Scale(7.5,4.0)=30.0 through the fork double ABI slot")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsInside(const int, const int, const int)");
			Invoker.AddArg(static_cast<int32>(5)).AddArg(static_cast<int32>(1)).AddArg(static_cast<int32>(10));
			ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false), TEXT("ScriptModule argument roundtrip test should return true for an in-range value")));
		}

		{
			AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(*TestRunner, ScriptEngine, Module, "bool IsInside(const int, const int, const int)");
			Invoker.AddArg(static_cast<int32>(-5)).AddArg(static_cast<int32>(1)).AddArg(static_cast<int32>(10));
			ASSERT_THAT(IsFalse(Invoker.CallAndReturn<bool>(true), TEXT("ScriptModule argument roundtrip test should return false for an out-of-range value")));
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("argument-roundtrip-after-execute"));
	}
};

#endif
