#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"

#include "Core/AngelscriptSettings.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptPrimitiveTypeTest,
	"Angelscript.TestModule.Functional.Types.PrimitiveAndEnum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptPrimitiveTypeTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypePrimitiveAndEnum",
		TEXT("enum EState { Idle = 2, Running = 4 } int Run() { bool bFlag = true; float Value = 1.5f + 2.5f; return (bFlag ? 1 : 0) + int(Value) + int(EState::Running); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Primitive and enum math should preserve the expected result"), Result, 9);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInt64TypedefTest,
	"Angelscript.TestModule.Functional.Types.Int64AndTypedef",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInt64TypedefTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int64 Result = 0;
	ASTEST_COMPILE_RUN_INT64(Engine, "ASTypeInt64AndTypedef",
		TEXT("int64 Run() { int64 Value = 1; Value <<= 40; Value += 7; return Value; }"),
		TEXT("int64 Run()"), Result);

	TestEqual(TEXT("int64 arithmetic should preserve wide integer precision"), Result, static_cast<int64>(1099511627783LL));

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeBoolTest,
	"Angelscript.TestModule.Functional.Types.Bool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeBoolLogicMatrixTest,
	"Angelscript.TestModule.Functional.Types.Bool.LogicMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeBoolTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeBool",
		TEXT("int Run() { bool A = true; bool B = false; return (A && !B) ? 1 : 0; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Bool expressions should preserve logical truthiness"), Result, 1);

	}
	return true;
}

bool FAngelscriptTypeBoolLogicMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeBoolLogicMatrix",
		TEXT("int Run() { bool A = true; bool B = false; return (A && B ? 1000 : 0) + (A || B ? 100 : 0) + (!A ? 10 : 0) + (!B ? 1 : 0); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Bool logic matrix should preserve &&, ||, and ! semantics across the false path"), Result, 101);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeFloatTest,
	"Angelscript.TestModule.Functional.Types.Float",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeFloatDebuggerFormattingTest,
	"Angelscript.TestModule.Functional.Types.FloatDebuggerFormatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeFloatNegativeAndFractionalMatrixTest,
	"Angelscript.TestModule.Functional.Types.Float.NegativeAndFractionalMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeFloatConfigurationModesTest,
	"Angelscript.TestModule.Functional.Types.Float.ConfigurationModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeFloatTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Float should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("double Run() { double A = 3.14; double B = 2.0; return A * B; }")
		: TEXT("float Run() { float A = 3.14f; float B = 2.0f; return A * B; }");
	const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

	asIScriptModule* Module = BuildModule(*this, Engine, "ASTypeFloat", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, Declaration);
	if (Function == nullptr)
	{
		return false;
	}

	bPassed = ReadExpectedFloatResult(*this, Engine, *Function, 6.28);

	}
	return bPassed;
}

bool FAngelscriptTypeFloatConfigurationModesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	bool bPassed = true;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

	UAngelscriptSettings* Settings = GetMutableDefault<UAngelscriptSettings>();
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should access mutable angelscript settings"), Settings))
	{
		return false;
	}

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
	TUniquePtr<FAngelscriptEngine> Float32Engine = AngelscriptTestSupport::CreateScriptScanFreeEngineForTesting(Config, Dependencies);
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should create a float32 testing engine"), Float32Engine.Get()))
	{
		return false;
	}

	asIScriptEngine* AmbientScriptEngine = Engine.GetScriptEngine();
	asIScriptEngine* Float32ScriptEngine = Float32Engine->GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should expose the ambient full engine"), AmbientScriptEngine)
		|| !TestNotNull(TEXT("Types.Float.ConfigurationModes should expose a float32 script engine"), Float32ScriptEngine))
	{
		return false;
	}

	bPassed &= TestTrue(
		TEXT("Types.Float.ConfigurationModes should create a dedicated float32 script engine instead of reusing the ambient full engine"),
		Float32ScriptEngine != AmbientScriptEngine);
	bPassed &= TestEqual(
		TEXT("Types.Float.ConfigurationModes should wire float32 mode to asEP_FLOAT_IS_FLOAT64=0"),
		static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)),
		0);

	asIScriptModule* Float32Module = BuildModule(*this, *Float32Engine, "ASTypeFloatConfigurationModes32", BuildFloatConfigurationModesScript(false));
	if (Float32Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Float32Function = FindFunctionByDeclExact(*Float32Module, TEXT("float32 Run()"));
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should resolve float32 Run() in float32 mode"), Float32Function))
	{
		return false;
	}

	bPassed &= TestNull(
		TEXT("Types.Float.ConfigurationModes should not resolve float64 Run() in float32 mode"),
		FindFunctionByDeclExact(*Float32Module, TEXT("float64 Run()")));
	bPassed &= ReadExpectedFloatResult(*this, *Float32Engine, *Float32Function, 1.25);
	bPassed &= TestEqual(
		TEXT("Types.Float.ConfigurationModes should keep the float32 engine property stable after float32 compilation"),
		static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)),
		0);

	ApplyFloatSettings(true);
	TUniquePtr<FAngelscriptEngine> Float64Engine = AngelscriptTestSupport::CreateScriptScanFreeEngineForTesting(Config, Dependencies);
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should create a float64 testing engine"), Float64Engine.Get()))
	{
		return false;
	}

	asIScriptEngine* Float64ScriptEngine = Float64Engine->GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should expose a float64 script engine"), Float64ScriptEngine))
	{
		return false;
	}

	bPassed &= TestTrue(
		TEXT("Types.Float.ConfigurationModes should create a dedicated float64 script engine instead of reusing the ambient full engine"),
		Float64ScriptEngine != AmbientScriptEngine);
	bPassed &= TestTrue(
		TEXT("Types.Float.ConfigurationModes should keep the float32 and float64 testing engines independent"),
		Float32ScriptEngine != Float64ScriptEngine);
	bPassed &= TestEqual(
		TEXT("Types.Float.ConfigurationModes should wire float64 mode to asEP_FLOAT_IS_FLOAT64=1"),
		static_cast<int32>(Float64ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)),
		1);
	bPassed &= TestEqual(
		TEXT("Types.Float.ConfigurationModes should keep the float32 engine property isolated after creating the float64 engine"),
		static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)),
		0);
	bPassed &= ReadExpectedFloatResult(*this, *Float32Engine, *Float32Function, 1.25);

	asIScriptModule* Float64Module = BuildModule(*this, *Float64Engine, "ASTypeFloatConfigurationModes64", BuildFloatConfigurationModesScript(true));
	if (Float64Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Float64Function = FindFunctionByDeclExact(*Float64Module, TEXT("float64 Run()"));
	if (!TestNotNull(TEXT("Types.Float.ConfigurationModes should resolve float64 Run() in float64 mode"), Float64Function))
	{
		return false;
	}

	bPassed &= TestNull(
		TEXT("Types.Float.ConfigurationModes should not resolve float32 Run() in float64 mode"),
		FindFunctionByDeclExact(*Float64Module, TEXT("float32 Run()")));
	bPassed &= ReadExpectedFloatResult(*this, *Float64Engine, *Float64Function, 1.25);
	bPassed &= TestEqual(
		TEXT("Types.Float.ConfigurationModes should keep the float64 engine property stable after float64 compilation"),
		static_cast<int32>(Float64ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)),
		1);
	bPassed &= TestEqual(
		TEXT("Types.Float.ConfigurationModes should keep the float32 engine property isolated after switching settings for the float64 engine"),
		static_cast<int32>(Float32ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64)),
		0);

	}
	return bPassed;
}

bool FAngelscriptTypeFloatDebuggerFormattingTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	bool bUsesScientificNotation = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	FAngelscriptEngineScope EngineScope(Engine);

	FAngelscriptTypeUsage FloatUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("float")));
	if (!TestTrue(TEXT("Float debugger formatting test should resolve the float type"), FloatUsage.IsValid()))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = Engine.GetScriptEngine()->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	FDebuggerValue DebugValue;
	if (bFloatUsesFloat64)
	{
		double SmallValue = 0.000000123456;
		if (!FloatUsage.GetDebuggerValue(&SmallValue, DebugValue))
		{
			AddError(TEXT("Float debugger formatting should read a small float64 value"));
			return false;
		}
	}
	else
	{
		float SmallValue = 0.000000123456f;
		if (!FloatUsage.GetDebuggerValue(&SmallValue, DebugValue))
		{
			AddError(TEXT("Float debugger formatting should read a small float value"));
			return false;
		}
	}

	bUsesScientificNotation = DebugValue.Value.Contains(TEXT("e")) || DebugValue.Value.Contains(TEXT("E"));
	TestTrue(TEXT("Small float debugger values should use scientific notation"), bUsesScientificNotation);

	}
	return bUsesScientificNotation;
}

bool FAngelscriptTypeFloatNegativeAndFractionalMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Float.NegativeAndFractionalMatrix should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = BuildNegativeAndFractionalFloatScript(bFloatUsesFloat64);
	const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

	asIScriptModule* Module = BuildModule(*this, Engine, "ASTypeFloatNegativeAndFractionalMatrix", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, Declaration);
	if (Function == nullptr)
	{
		return false;
	}

	bPassed = ReadExpectedFloatResult(*this, Engine, *Function, -1.5);

	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeInt8Test,
	"Angelscript.TestModule.Functional.Types.Int8",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeInt8SignAndBoundsTest,
	"Angelscript.TestModule.Functional.Types.Int8.SignAndBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeInt8Test::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeInt8",
		TEXT("int Run() { int8 A = 100; int8 B = 50; return int(A + B); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Int8 arithmetic should survive promotion back to int"), Result, 150);

	}
	return true;
}

bool FAngelscriptTypeInt8SignAndBoundsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeInt8SignAndBounds",
		TEXT("int Run() { int8 Negative = -1; int8 MinValue = -128; int8 MaxValue = 127; return (Negative < 0 ? 1000 : 0) + (int(Negative) == -1 ? 100 : 0) + (int(MinValue) == -128 ? 10 : 0) + (int(MaxValue) == 127 ? 1 : 0); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Int8 negative literals and boundary promotions should preserve signed semantics"), Result, 1111);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeBitsTest,
	"Angelscript.TestModule.Functional.Types.Bits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeBitsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeBits",
		TEXT("int Run() { int A = 0x0F; int B = 0xF0; return ((A | B) == 0xFF && (A & B) == 0 && (A ^ B) == 0xFF) ? 1 : 0; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Bitwise operations should preserve expected masks"), Result, 1);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeEnumTest,
	"Angelscript.TestModule.Functional.Types.Enum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeEnumExplicitValueMatrixTest,
	"Angelscript.TestModule.Functional.Types.Enum.ExplicitValueMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeEnumTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeEnum",
		TEXT("enum Color { Red, Green, Blue } int Run() { Color Value = Color::Green; return int(Value); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Enums should preserve ordinal values"), Result, 1);

	}
	return true;
}
bool FAngelscriptTypeEnumExplicitValueMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeEnumExplicitValueMatrix",
		TEXT("enum Status { Negative = -4, Sparse = 20, SparsePlusOne = 21, AliasSparse = 20, FlagA = 1, FlagB = 4 } int Run() { return (int(Status::Sparse) + int(Status::SparsePlusOne) - int(Status::Negative)) * 100 + int(Status::AliasSparse) + (int(Status::FlagA) | int(Status::FlagB)); }"),
		TEXT("int Run()"), Result);
	TestEqual(TEXT("Explicit enum values should preserve negative, sparse, alias, and bitwise-composed constants"), Result, 4525);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeAutoTest,
	"Angelscript.TestModule.Functional.Types.Auto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeAutoInferenceMatrixTest,
	"Angelscript.TestModule.Functional.Types.AutoInferenceMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeAutoTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeAuto",
		TEXT("int Run() { auto Value = 42; return Value; }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Auto should infer integer literal types"), Result, 42);

	}
	return true;
}

bool FAngelscriptTypeAutoInferenceMatrixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.AutoInferenceMatrix should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = BuildAutoInferenceMatrixScript(bFloatUsesFloat64);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASTypeAutoInferenceMatrix",
		Script,
		TEXT("int Run()"),
		Result);

	TestEqual(
		TEXT("Auto inference matrix should route int, float or double, and enum auto variables to the expected overloads"),
		Result,
		123);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeConversionTest,
	"Angelscript.TestModule.Functional.Types.Conversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeConversionTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Conversion should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("int Run() { double Value = 3.7; return int(Value); }")
		: TEXT("int Run() { float Value = 3.7f; return int(Value); }");

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASTypeConversion", Script, TEXT("int Run()"), Result);

	TestEqual(TEXT("Explicit numeric conversion should truncate toward zero"), Result, 3);

	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeImplicitCastTest,
	"Angelscript.TestModule.Functional.Types.ImplicitCast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeImplicitCastTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.ImplicitCast should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("double Run() { int Value = 42; double Converted = Value; return Converted; }")
		: TEXT("float Run() { int Value = 42; float Converted = Value; return Converted; }");
	const FString Declaration = bFloatUsesFloat64 ? TEXT("double Run()") : TEXT("float Run()");

	asIScriptModule* Module = BuildModule(*this, Engine, "ASTypeImplicitCast", Script);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, Declaration);
	if (Function == nullptr)
	{
		return false;
	}

	bPassed = ReadExpectedFloatResult(*this, Engine, *Function, 42.0);

	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeImplicitCastNegativeAndParamWideningTest,
	"Angelscript.TestModule.Functional.Types.ImplicitCast.NegativeAndParamWidening",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeImplicitCastNegativeAndParamWideningTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.ImplicitCast.NegativeAndParamWidening should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = BuildImplicitCastNegativeAndParamWideningScript(bFloatUsesFloat64);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASTypeImplicitCastNegativeAndParamWidening",
		Script,
		TEXT("int Run()"),
		Result);

	TestEqual(
		TEXT("Implicit widening should preserve the negative sign across assignment and parameter forwarding"),
		Result,
		111);

	}
	return true;
}

#endif
