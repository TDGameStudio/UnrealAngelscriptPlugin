#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Core/AngelscriptSettings.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private
{
	FString BuildAutoInferenceMatrixScript(const bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT(R"AS(
enum EKind
{
	A,
	B
}

int Which(int Value) { return 1; }
int Which(double Value) { return 2; }
int Which(EKind Value) { return 3; }

int Run()
{
	auto IntValue = 42;
	auto FloatValue = 1.5;
	auto EnumValue = EKind::B;
	return Which(IntValue) * 100 + Which(FloatValue) * 10 + Which(EnumValue);
}
)AS")
			: TEXT(R"AS(
enum EKind
{
	A,
	B
}

int Which(int Value) { return 1; }
int Which(float Value) { return 2; }
int Which(EKind Value) { return 3; }

int Run()
{
	auto IntValue = 42;
	auto FloatValue = 1.5f;
	auto EnumValue = EKind::B;
	return Which(IntValue) * 100 + Which(FloatValue) * 10 + Which(EnumValue);
}
)AS");
	}

	FString BuildImplicitCastNegativeAndParamWideningScript(const bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT(R"AS(
double Accept(double Value)
{
	return Value;
}

int Run()
{
	int Negative = -7;
	double Assigned = Negative;
	double Forwarded = Accept(Negative);
	return (Assigned < 0.0 ? 1 : 0) * 100 + (int(Assigned) == -7 ? 1 : 0) * 10 + (int(Forwarded) == -7 ? 1 : 0);
}
)AS")
			: TEXT(R"AS(
float Accept(float Value)
{
	return Value;
}

int Run()
{
	int Negative = -7;
	float Assigned = Negative;
	float Forwarded = Accept(Negative);
	return (Assigned < 0.0f ? 1 : 0) * 100 + (int(Assigned) == -7 ? 1 : 0) * 10 + (int(Forwarded) == -7 ? 1 : 0);
}
)AS");
	}

	FString BuildNegativeAndFractionalFloatScript(const bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT(R"AS(
double Run()
{
	double A = -1.25;
	double B = 0.5;
	double C = 2.0;
	return (A + B) * C;
}
)AS")
			: TEXT(R"AS(
float Run()
{
	float A = -1.25f;
	float B = 0.5f;
	float C = 2.0f;
	return (A + B) * C;
}
)AS");
	}

	FString BuildFloatConfigurationModesScript(const bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT(R"AS(
float Run()
{
	float A = -1.25;
	float B = 2.5;
	return A + B;
}
)AS")
			: TEXT(R"AS(
float Run()
{
	float A = -1.25f;
	float B = 2.5f;
	return A + B;
}
)AS");
	}

	asIScriptFunction* FindFunctionByDeclExact(asIScriptModule& Module, const FString& Declaration)
	{
		FTCHARToUTF8 DeclarationUtf8(*Declaration);
		return Module.GetFunctionByDecl(DeclarationUtf8.Get());
	}

	bool ReadExpectedFloatResult(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, double ExpectedValue)
	{
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (!Test.TestNotNull(TEXT("Float helper should expose a script engine"), ScriptEngine))
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(TEXT("Float helper should create an execution context"), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (!Test.TestEqual(TEXT("Float helper should prepare the function"), PrepareResult, static_cast<int32>(asSUCCESS)) ||
			!Test.TestEqual(TEXT("Float helper should execute the function"), ExecuteResult, static_cast<int32>(asEXECUTION_FINISHED)))
		{
			Context->Release();
			return false;
		}

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		bool bMatches = false;
		if (bFloatUsesFloat64)
		{
			double ReturnValue = 0.0;
			const asQWORD EncodedReturnValue = Context->GetReturnQWord();
			FMemory::Memcpy(&ReturnValue, &EncodedReturnValue, sizeof(ReturnValue));
			bMatches = FMath::IsNearlyEqual(ReturnValue, ExpectedValue, 0.001);
			Test.TestTrue(TEXT("Float helper should preserve float64-compatible return values"), bMatches);
		}
		else
		{
			const float ReturnValue = Context->GetReturnFloat();
			bMatches = FMath::IsNearlyEqual(ReturnValue, static_cast<float>(ExpectedValue), 0.001f);
			Test.TestTrue(TEXT("Float helper should preserve float return values"), bMatches);
		}

		Context->Release();
		return bMatches;
	}
}


TEST_CLASS_WITH_FLAGS(
	FAngelscriptTypeTests,
	"Angelscript.TestModule.Functional.Types",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(PrimitiveAndEnum)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypePrimitiveAndEnum"),
			TEXT("enum EState { Idle = 2, Running = 4 } int Run() { bool bFlag = true; float Value = 1.5f + 2.5f; return (bFlag ? 1 : 0) + int(Value) + int(EState::Running); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Primitive and enum math should preserve the expected result"), 9);
	}

	TEST_METHOD(Int64AndTypedef)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeInt64AndTypedef"),
			TEXT("int64 Run() { int64 Value = 1; Value <<= 40; Value += 7; return Value; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int64 Run()"));
		ASSERT_THAT(IsNotNull(Function));

		int64 Result = 0;
		ASSERT_THAT(IsTrue(ExecuteInt64Function(*TestRunner, Engine, *Function, Result)));
		ASSERT_THAT(AreEqual(static_cast<int64>(1099511627783LL), Result, TEXT("int64 arithmetic should preserve wide integer precision")));
	}

	TEST_METHOD(Bool)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeBool"),
			TEXT("int Run() { bool A = true; bool B = false; return (A && !B) ? 1 : 0; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Bool expressions should preserve logical truthiness"), 1);
	}

	TEST_METHOD(Bool_LogicMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeBoolLogicMatrix"),
			TEXT("int Run() { bool A = true; bool B = false; return (A && B ? 1000 : 0) + (A || B ? 100 : 0) + (!A ? 10 : 0) + (!B ? 1 : 0); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Bool logic matrix should preserve &&, ||, and ! semantics across the false path"), 101);
	}

	TEST_METHOD(Float)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.Float should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = bFloatUsesFloat64
			? TEXT("double Run() { double A = 3.14; double B = 2.0; return A * B; }")
			: TEXT("float Run() { float A = 3.14f; float B = 2.0f; return A * B; }");
		const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeFloat"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, Module.GetModule(), Declaration);
		ASSERT_THAT(IsNotNull(Function));

		ASSERT_THAT(IsTrue(ReadExpectedFloatResult(*TestRunner, Engine, *Function, 6.28)));
	}

	TEST_METHOD(Float_ConfigurationModes)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
		ASSERT_THAT(IsNotNull(Settings, TEXT("Types.Float.ConfigurationModes should access mutable angelscript settings")));

		const bool PreviousFloatIsFloat64 = Settings->bScriptFloatIsFloat64;
		const bool PreviousDeprecateDoubleType = Settings->bDeprecateDoubleType;
		ON_SCOPE_EXIT
		{
			Settings->bScriptFloatIsFloat64 = PreviousFloatIsFloat64;
			Settings->bDeprecateDoubleType = PreviousDeprecateDoubleType;
		};

		auto ApplyFloatSettings = [Settings](const bool bFloatUsesFloat64)
		{
			Settings->bScriptFloatIsFloat64 = bFloatUsesFloat64;
			Settings->bDeprecateDoubleType = false;
		};

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		ApplyFloatSettings(false);
		TUniquePtr<FAngelscriptEngine> Float32Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
		ASSERT_THAT(IsNotNull(Float32Engine.Get(), TEXT("Types.Float.ConfigurationModes should create a float32 testing engine")));

		asIScriptEngine* AmbientScriptEngine = Engine.GetScriptEngine();
		asIScriptEngine* Float32ScriptEngine = Float32Engine->GetScriptEngine();
		ASSERT_THAT(IsNotNull(AmbientScriptEngine, TEXT("Types.Float.ConfigurationModes should expose the ambient full engine")));
		ASSERT_THAT(IsNotNull(Float32ScriptEngine, TEXT("Types.Float.ConfigurationModes should expose a float32 script engine")));

		ASSERT_THAT(IsTrue(Float32ScriptEngine != AmbientScriptEngine, TEXT("Types.Float.ConfigurationModes should create a dedicated float32 script engine instead of reusing the ambient full engine")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)), TEXT("Types.Float.ConfigurationModes should wire float32 mode to asEP_FLOAT_IS_FLOAT64=0")));

		FScopedAngelscriptModule Float32Module(*TestRunner, *Float32Engine, TEXT("ASTypeFloatConfigurationModes32"), BuildFloatConfigurationModesScript(false));
		ASSERT_THAT(IsTrue(Float32Module.IsValid()));

		asIScriptFunction* Float32Function = FindFunctionByDeclExact(Float32Module.GetModule(), TEXT("float32 Run()"));
		ASSERT_THAT(IsNotNull(Float32Function, TEXT("Types.Float.ConfigurationModes should resolve float32 Run() in float32 mode")));

		ASSERT_THAT(IsNull(FindFunctionByDeclExact(Float32Module.GetModule(), TEXT("float64 Run()")), TEXT("Types.Float.ConfigurationModes should not resolve float64 Run() in float32 mode")));
		ASSERT_THAT(IsTrue(ReadExpectedFloatResult(*TestRunner, *Float32Engine, *Float32Function, 1.25)));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)), TEXT("Types.Float.ConfigurationModes should keep the float32 engine property stable after float32 compilation")));

		ApplyFloatSettings(true);
		TUniquePtr<FAngelscriptEngine> Float64Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
		ASSERT_THAT(IsNotNull(Float64Engine.Get(), TEXT("Types.Float.ConfigurationModes should create a float64 testing engine")));

		asIScriptEngine* Float64ScriptEngine = Float64Engine->GetScriptEngine();
		ASSERT_THAT(IsNotNull(Float64ScriptEngine, TEXT("Types.Float.ConfigurationModes should expose a float64 script engine")));

		ASSERT_THAT(IsTrue(Float64ScriptEngine != AmbientScriptEngine, TEXT("Types.Float.ConfigurationModes should create a dedicated float64 script engine instead of reusing the ambient full engine")));
		ASSERT_THAT(IsTrue(Float32ScriptEngine != Float64ScriptEngine, TEXT("Types.Float.ConfigurationModes should keep the float32 and float64 testing engines independent")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Float64ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)), TEXT("Types.Float.ConfigurationModes should wire float64 mode to asEP_FLOAT_IS_FLOAT64=1")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)), TEXT("Types.Float.ConfigurationModes should keep the float32 engine property isolated after creating the float64 engine")));
		ASSERT_THAT(IsTrue(ReadExpectedFloatResult(*TestRunner, *Float32Engine, *Float32Function, 1.25)));

		FScopedAngelscriptModule Float64Module(*TestRunner, *Float64Engine, TEXT("ASTypeFloatConfigurationModes64"), BuildFloatConfigurationModesScript(true));
		ASSERT_THAT(IsTrue(Float64Module.IsValid()));

		asIScriptFunction* Float64Function = FindFunctionByDeclExact(Float64Module.GetModule(), TEXT("float64 Run()"));
		ASSERT_THAT(IsNotNull(Float64Function, TEXT("Types.Float.ConfigurationModes should resolve float64 Run() in float64 mode")));

		ASSERT_THAT(IsNull(FindFunctionByDeclExact(Float64Module.GetModule(), TEXT("float32 Run()")), TEXT("Types.Float.ConfigurationModes should not resolve float32 Run() in float64 mode")));
		ASSERT_THAT(IsTrue(ReadExpectedFloatResult(*TestRunner, *Float64Engine, *Float64Function, 1.25)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Float64ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)), TEXT("Types.Float.ConfigurationModes should keep the float64 engine property stable after float64 compilation")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)), TEXT("Types.Float.ConfigurationModes should keep the float32 engine property isolated after switching settings for the float64 engine")));

	}

	TEST_METHOD(FloatDebuggerFormatting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptTypeUsage FloatUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("float")));
		ASSERT_THAT(IsTrue(FloatUsage.IsValid(), TEXT("Float debugger formatting test should resolve the float type")));

		const bool bFloatUsesFloat64 = Engine.GetScriptEngine()->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		FDebuggerValue DebugValue;
		if (bFloatUsesFloat64)
		{
			double SmallValue = 0.000000123456;
			ASSERT_THAT(IsTrue(FloatUsage.GetDebuggerValue(&SmallValue, DebugValue), TEXT("Float debugger formatting should read a small float64 value")));
		}
		else
		{
			float SmallValue = 0.000000123456f;
			ASSERT_THAT(IsTrue(FloatUsage.GetDebuggerValue(&SmallValue, DebugValue), TEXT("Float debugger formatting should read a small float value")));
		}

		const bool bUsesScientificNotation = DebugValue.Value.Contains(TEXT("e")) || DebugValue.Value.Contains(TEXT("E"));
		ASSERT_THAT(IsTrue(bUsesScientificNotation, TEXT("Small float debugger values should use scientific notation")));
	}

	TEST_METHOD(Float_NegativeAndFractionalMatrix)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.Float.NegativeAndFractionalMatrix should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = BuildNegativeAndFractionalFloatScript(bFloatUsesFloat64);
		const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeFloatNegativeAndFractionalMatrix"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, Module.GetModule(), Declaration);
		ASSERT_THAT(IsNotNull(Function));

		ASSERT_THAT(IsTrue(ReadExpectedFloatResult(*TestRunner, Engine, *Function, -1.5)));
	}

	TEST_METHOD(Int8)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeInt8"),
			TEXT("int Run() { int8 A = 100; int8 B = 50; return int(A + B); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Int8 arithmetic should survive promotion back to int"), 150);
	}

	TEST_METHOD(Int8_SignAndBounds)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeInt8SignAndBounds"),
			TEXT("int Run() { int8 Negative = -1; int8 MinValue = -128; int8 MaxValue = 127; return (Negative < 0 ? 1000 : 0) + (int(Negative) == -1 ? 100 : 0) + (int(MinValue) == -128 ? 10 : 0) + (int(MaxValue) == 127 ? 1 : 0); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Int8 negative literals and boundary promotions should preserve signed semantics"), 1111);
	}

	TEST_METHOD(Bits)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeBits"),
			TEXT("int Run() { int A = 0x0F; int B = 0xF0; return ((A | B) == 0xFF && (A & B) == 0 && (A ^ B) == 0xFF) ? 1 : 0; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Bitwise operations should preserve expected masks"), 1);
	}

	TEST_METHOD(Enum)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeEnum"),
			TEXT("enum Color { Red, Green, Blue } int Run() { Color Value = Color::Green; return int(Value); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Enums should preserve ordinal values"), 1);
	}

	TEST_METHOD(Enum_ExplicitValueMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeEnumExplicitValueMatrix"),
			TEXT("enum Status { Negative = -4, Sparse = 20, SparsePlusOne = 21, AliasSparse = 20, FlagA = 1, FlagB = 4 } int Run() { return (int(Status::Sparse) + int(Status::SparsePlusOne) - int(Status::Negative)) * 100 + int(Status::AliasSparse) + (int(Status::FlagA) | int(Status::FlagB)); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Explicit enum values should preserve negative, sparse, alias, and bitwise-composed constants"), 4525);
	}

	TEST_METHOD(Auto)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeAuto"),
			TEXT("int Run() { auto Value = 42; return Value; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Auto should infer integer literal types"), 42);
	}

	TEST_METHOD(AutoInferenceMatrix)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.AutoInferenceMatrix should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = BuildAutoInferenceMatrixScript(bFloatUsesFloat64);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeAutoInferenceMatrix"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Run()"),
			TEXT("Auto inference matrix should route int, float or double, and enum auto variables to the expected overloads"),
			123);
	}

	TEST_METHOD(Conversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.Conversion should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = bFloatUsesFloat64
			? TEXT("int Run() { double Value = 3.7; return int(Value); }")
			: TEXT("int Run() { float Value = 3.7f; return int(Value); }");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeConversion"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Explicit numeric conversion should truncate toward zero"), 3);
	}

	TEST_METHOD(ImplicitCast)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.ImplicitCast should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = bFloatUsesFloat64
			? TEXT("double Run() { int Value = 42; double Converted = Value; return Converted; }")
			: TEXT("float Run() { int Value = 42; float Converted = Value; return Converted; }");
		const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeImplicitCast"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, Module.GetModule(), Declaration);
		ASSERT_THAT(IsNotNull(Function));

		ASSERT_THAT(IsTrue(ReadExpectedFloatResult(*TestRunner, Engine, *Function, 42.0)));
	}

	TEST_METHOD(ImplicitCast_NegativeAndParamWidening)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.ImplicitCast.NegativeAndParamWidening should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = BuildImplicitCastNegativeAndParamWideningScript(bFloatUsesFloat64);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeImplicitCastNegativeAndParamWidening"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Run()"),
			TEXT("Implicit widening should preserve the negative sign across assignment and parameter forwarding"),
			111);
	}
};

#endif
