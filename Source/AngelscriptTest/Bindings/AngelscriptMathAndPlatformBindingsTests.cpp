// ============================================================================
// AngelscriptMathAndPlatformBindingsTests.cpp
//
// Math and platform binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.MathAndPlatform.FAngelscriptMathAndPlatformBindingsTest.*
//
// Sections:
//   MathExtended         — extended Math helpers (rand, interp, cubic, etc.)
//   MathDeterministic    — deterministic math comparison vs native baselines
//   PlatformProcess      — FPlatformProcess directory/path queries
//   Logging              — headless-safe logging helpers
//
// CQTest adaptation notes:
//   Four IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   MathExtended: single int Entry() split into per-aspect functions.
//   MathDeterministic: $TOKEN$ replacement pattern retained with single Entry().
//   PlatformProcess: single int Entry() split into per-aspect functions.
//   Logging: retained as single function with AddExpectedError.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Helper utilities (retained from original)
// ----------------------------------------------------------------------------

namespace AngelscriptTest_Bindings_AngelscriptMathAndPlatformBindingsTests_Private
{
	FString FormatScriptFloatLiteral(const double Value)
	{
		FString Literal = FString::Printf(TEXT("%.9g"), Value);
		if (!Literal.Contains(TEXT(".")) && !Literal.Contains(TEXT("e")) && !Literal.Contains(TEXT("E")))
		{
			Literal += TEXT(".0");
		}

		return Literal;
	}

	FString FormatScriptVectorLiteral(const FVector& Value)
	{
		return FString::Printf(
			TEXT("FVector(%s, %s, %s)"),
			*FormatScriptFloatLiteral(Value.X),
			*FormatScriptFloatLiteral(Value.Y),
			*FormatScriptFloatLiteral(Value.Z));
	}

	FString FormatScriptRotatorLiteral(const FRotator& Value)
	{
		return FString::Printf(
			TEXT("FRotator(%s, %s, %s)"),
			*FormatScriptFloatLiteral(Value.Pitch),
			*FormatScriptFloatLiteral(Value.Yaw),
			*FormatScriptFloatLiteral(Value.Roll));
	}
}


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptMathAndPlatformBindingsTest,
	"Angelscript.TestModule.Bindings.MathAndPlatform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: MathExtended
	// ====================================================================

	TEST_METHOD(MathExtended)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptMathAndPlatformBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMathPlatform_MathExtended"), TEXT(R"(
int RandHelperInRange()
{
	int Helper = Math::RandHelper(10);
	return (Helper >= 0 && Helper < 10) ? 1 : 0;
}

int IsPowerOfTwoCorrect()
{
	return (Math::IsPowerOfTwo(8) && !Math::IsPowerOfTwo(7)) ? 1 : 0;
}

int VRandNotZero()
{
	return (!Math::VRand().IsNearlyZero()) ? 1 : 0;
}

int VRandConeNotZero()
{
	return (!Math::VRandCone(FVector::ForwardVector, 0.5f).IsNearlyZero()) ? 1 : 0;
}

int RandPointInCircleBounded()
{
	return (Math::RandPointInCircle(10.0f).Size() <= 10.01f) ? 1 : 0;
}

int ClampAngleInRange()
{
	float Clamped = Math::ClampAngle(370.0f, -180.0f, 180.0f);
	return (Clamped >= -180.0f && Clamped <= 180.0f) ? 1 : 0;
}

int ClampInt64Works()
{
	return (Math::Clamp(int64(20), int64(0), int64(5)) == 5) ? 1 : 0;
}

int ClampUInt64Works()
{
	return (Math::Clamp(uint64(20), uint64(0), uint64(5)) == 5) ? 1 : 0;
}

int FindDeltaAngleDegreesCorrect()
{
	return (Math::FindDeltaAngleDegrees(10.0f, 20.0f) == 10.0f) ? 1 : 0;
}

int UnwindDegreesCorrect()
{
	return (Math::UnwindDegrees(370.0f) == 10.0f) ? 1 : 0;
}

int MakePulsatingValueBounded()
{
	float Pulse = Math::MakePulsatingValue(1.25, 2.0f, 0.0f);
	return (Pulse >= 0.0f && Pulse <= 1.0f) ? 1 : 0;
}

int GetReflectionVectorNotZero()
{
	return (!Math::GetReflectionVector(FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f)).IsNearlyZero()) ? 1 : 0;
}

int VInterpToAdvances()
{
	return (!Math::VInterpTo(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 0.1f, 5.0f).IsNearlyZero()) ? 1 : 0;
}

int RInterpToAdvances()
{
	return (!Math::RInterpTo(FRotator::ZeroRotator, FRotator(0.0f, 90.0f, 0.0f), 0.1f, 5.0f).IsNearlyZero()) ? 1 : 0;
}

int FInterpToPositive()
{
	return (Math::FInterpTo(0.0f, 100.0f, 0.1f, 5.0f) > 0.0f) ? 1 : 0;
}

int RandomPointInBoundingBoxBounded()
{
	FVector P = Math::RandomPointInBoundingBox(FVector::ZeroVector, FVector(10.0f, 10.0f, 10.0f));
	return (Math::Abs(P.X) <= 10.01f && Math::Abs(P.Y) <= 10.01f && Math::Abs(P.Z) <= 10.01f) ? 1 : 0;
}

int RandomRotatorNotZero()
{
	return (!Math::RandomRotator(false).IsNearlyZero()) ? 1 : 0;
}

int CubicInterpPositive()
{
	return (Math::CubicInterp(0.0f, 1.0f, 10.0f, 0.0f, 0.5f) > 0.0f) ? 1 : 0;
}

int CubicInterpDerivativePositive()
{
	return (Math::CubicInterpDerivative(0.0f, 1.0f, 10.0f, 0.0f, 0.5f) > 0.0f) ? 1 : 0;
}

int CubicInterp64Positive()
{
	return (Math::CubicInterp(0.0, 1.0, 10.0, 0.0, 0.5) > 0.0) ? 1 : 0;
}

int CubicInterpDerivative64Positive()
{
	return (Math::CubicInterpDerivative(0.0, 1.0, 10.0, 0.0, 0.5) > 0.0) ? 1 : 0;
}

int Vector2fToDirectionAndLengthCorrect()
{
	FVector2f Direction;
	float32 DirectionLength = 0.0f;
	FVector2f(3.0f, 4.0f).ToDirectionAndLength(Direction, DirectionLength);
	return (Direction.Equals(FVector2f(0.6f, 0.8f), 0.001f) && Math::IsNearlyEqual(DirectionLength, 5.0f, 0.001f)) ? 1 : 0;
}

int Int64MathOps()
{
	if (Math::Abs(int64(-7)) != 7) return 0;
	if (Math::Sign(int64(-7)) != -1 || Math::Sign(int64(0)) != 0 || Math::Sign(int64(7)) != 1) return 0;
	if (Math::Min(int64(7), int64(-3)) != -3) return 0;
	if (Math::Max(int64(7), int64(-3)) != 7) return 0;
	if (Math::Square(int64(9)) != 81) return 0;
	return 1;
}

int LinePlaneIntersectionCorrect()
{
	FVector PlaneIntersection = Math::LinePlaneIntersection(FVector(0.0f, 0.0f, -5.0f), FVector(0.0f, 0.0f, 5.0f), FPlane(FVector::ZeroVector, FVector::UpVector));
	return PlaneIntersection.Equals(FVector::ZeroVector, 0.001f) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandHelperInRange()"), TEXT("RandHelper should be in [0,10)"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int IsPowerOfTwoCorrect()"), TEXT("IsPowerOfTwo(8)=true, (7)=false"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VRandNotZero()"), TEXT("VRand should not be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VRandConeNotZero()"), TEXT("VRandCone should not be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandPointInCircleBounded()"), TEXT("RandPointInCircle should be within radius"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ClampAngleInRange()"), TEXT("ClampAngle should be in [-180,180]"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ClampInt64Works()"), TEXT("Clamp int64 should work"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ClampUInt64Works()"), TEXT("Clamp uint64 should work"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FindDeltaAngleDegreesCorrect()"), TEXT("FindDeltaAngleDegrees should return 10"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UnwindDegreesCorrect()"), TEXT("UnwindDegrees(370) should return 10"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int MakePulsatingValueBounded()"), TEXT("MakePulsatingValue should be in [0,1]"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GetReflectionVectorNotZero()"), TEXT("GetReflectionVector should not be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int VInterpToAdvances()"), TEXT("VInterpTo should advance"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RInterpToAdvances()"), TEXT("RInterpTo should advance"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FInterpToPositive()"), TEXT("FInterpTo should return positive"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandomPointInBoundingBoxBounded()"), TEXT("RandomPointInBoundingBox should be bounded"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RandomRotatorNotZero()"), TEXT("RandomRotator should not be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CubicInterpPositive()"), TEXT("CubicInterp float32 should be positive"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CubicInterpDerivativePositive()"), TEXT("CubicInterpDerivative float32 should be positive"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CubicInterp64Positive()"), TEXT("CubicInterp float64 should be positive"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CubicInterpDerivative64Positive()"), TEXT("CubicInterpDerivative float64 should be positive"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Vector2fToDirectionAndLengthCorrect()"), TEXT("Vector2f ToDirectionAndLength should be correct"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Int64MathOps()"), TEXT("int64 Abs/Sign/Min/Max/Square should work"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int LinePlaneIntersectionCorrect()"), TEXT("LinePlaneIntersection should be at origin"), 1);
	}

	// ====================================================================
	// Section: MathDeterministic
	// ====================================================================

	TEST_METHOD(MathDeterministic)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptMathAndPlatformBindingsTests_Private;
		constexpr float Tolerance = 0.001f;
		const float ExpectedClampAngle = FMath::ClampAngle(370.0f, -180.0f, 180.0f);
		const float ExpectedDeltaAngle = FMath::FindDeltaAngleDegrees(10.0f, 20.0f);
		const float ExpectedUnwind = FMath::UnwindDegrees(370.0f);
		const FVector ExpectedReflection = FMath::GetReflectionVector(FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f));
		const FVector ExpectedVectorInterp = FMath::VInterpTo(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 0.1f, 5.0f);
		const FRotator ExpectedRotatorInterp = FMath::RInterpTo(FRotator::ZeroRotator, FRotator(0.0f, 90.0f, 0.0f), 0.1f, 5.0f);
		const float ExpectedScalarInterp = FMath::FInterpTo(0.0f, 100.0f, 0.1f, 5.0f);

		TestRunner->TestTrue(
			TEXT("Native GetReflectionVector baseline should reflect X across the opposing normal into (-1, 0, 0)"),
			ExpectedReflection.Equals(FVector(-1.0f, 0.0f, 0.0f), 0.0f));
		TestRunner->TestTrue(
			TEXT("Native VInterpTo baseline should advance toward the target on X only"),
			ExpectedVectorInterp.Equals(FVector(ExpectedVectorInterp.X, 0.0f, 0.0f), 0.0f));
		TestRunner->TestTrue(
			TEXT("Native RInterpTo baseline should only accumulate yaw for this setup"),
			ExpectedRotatorInterp.Equals(FRotator(0.0f, ExpectedRotatorInterp.Yaw, 0.0f), 0.0f));

		FString Script = TEXT(R"(
int Entry()
{
	if (!Math::IsNearlyEqual(Math::ClampAngle(370.0f, -180.0f, 180.0f), $EXPECTED_CLAMP_ANGLE$, $TOLERANCE$))
		return 10;

	if (!Math::IsNearlyEqual(Math::FindDeltaAngleDegrees(10.0f, 20.0f), $EXPECTED_DELTA_ANGLE$, $TOLERANCE$))
		return 20;

	if (!Math::IsNearlyEqual(Math::UnwindDegrees(370.0f), $EXPECTED_UNWIND$, $TOLERANCE$))
		return 30;

	const FVector Reflected = Math::GetReflectionVector(FVector(1.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f));
	if (!Reflected.Equals(FVector(-1.0f, 0.0f, 0.0f), 0.0f))
		return 40;

	if (!Math::VInterpTo(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), 0.1f, 5.0f).Equals($EXPECTED_VECTOR_INTERP$, $TOLERANCE$))
		return 50;

	if (!Math::RInterpTo(FRotator::ZeroRotator, FRotator(0.0f, 90.0f, 0.0f), 0.1f, 5.0f).Equals($EXPECTED_ROTATOR_INTERP$, $TOLERANCE$))
		return 60;

	if (!Math::IsNearlyEqual(Math::FInterpTo(0.0f, 100.0f, 0.1f, 5.0f), $EXPECTED_SCALAR_INTERP$, $TOLERANCE$))
		return 70;

	return 1;
}
)");

		Script.ReplaceInline(TEXT("$TOLERANCE$"), *FormatScriptFloatLiteral(Tolerance));
		Script.ReplaceInline(TEXT("$EXPECTED_CLAMP_ANGLE$"), *FormatScriptFloatLiteral(ExpectedClampAngle));
		Script.ReplaceInline(TEXT("$EXPECTED_DELTA_ANGLE$"), *FormatScriptFloatLiteral(ExpectedDeltaAngle));
		Script.ReplaceInline(TEXT("$EXPECTED_UNWIND$"), *FormatScriptFloatLiteral(ExpectedUnwind));
		Script.ReplaceInline(TEXT("$EXPECTED_VECTOR_INTERP$"), *FormatScriptVectorLiteral(ExpectedVectorInterp));
		Script.ReplaceInline(TEXT("$EXPECTED_ROTATOR_INTERP$"), *FormatScriptRotatorLiteral(ExpectedRotatorInterp));
		Script.ReplaceInline(TEXT("$EXPECTED_SCALAR_INTERP$"), *FormatScriptFloatLiteral(ExpectedScalarInterp));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMathPlatform_MathDeterministic"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Entry()"), TEXT("Deterministic Math should match native baselines"), 1);
	}

	// ====================================================================
	// Section: PlatformProcess
	// ====================================================================

	TEST_METHOD(PlatformProcess)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptMathAndPlatformBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMathPlatform_PlatformProcess"), TEXT(R"(
int UserDirNotEmpty() { return (!FPlatformProcess::UserDir().IsEmpty()) ? 1 : 0; }
int UserSettingsDirNotEmpty() { return (!FPlatformProcess::UserSettingsDir().IsEmpty()) ? 1 : 0; }
int UserTempDirNotEmpty() { return (!FPlatformProcess::UserTempDir().IsEmpty()) ? 1 : 0; }
int ApplicationSettingsDirNotEmpty() { return (!FPlatformProcess::ApplicationSettingsDir().IsEmpty()) ? 1 : 0; }
int ExecutablePathNotEmpty() { return (!FPlatformProcess::ExecutablePath().IsEmpty()) ? 1 : 0; }
int ExecutableNameNotEmpty() { return (!FPlatformProcess::ExecutableName().IsEmpty()) ? 1 : 0; }
int CurrentWorkingDirectoryNotEmpty() { return (!FPlatformProcess::CurrentWorkingDirectory().IsEmpty()) ? 1 : 0; }
int ComputerNameNotEmpty() { return (!FPlatformProcess::ComputerName().IsEmpty()) ? 1 : 0; }
int UserNameNotEmpty() { return (!FPlatformProcess::UserName().IsEmpty()) ? 1 : 0; }
int CanLaunchURLWorks() { return FPlatformProcess::CanLaunchURL("https://example.com") ? 1 : 0; }
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UserDirNotEmpty()"), TEXT("UserDir should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UserSettingsDirNotEmpty()"), TEXT("UserSettingsDir should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UserTempDirNotEmpty()"), TEXT("UserTempDir should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ApplicationSettingsDirNotEmpty()"), TEXT("ApplicationSettingsDir should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ExecutablePathNotEmpty()"), TEXT("ExecutablePath should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ExecutableNameNotEmpty()"), TEXT("ExecutableName should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CurrentWorkingDirectoryNotEmpty()"), TEXT("CurrentWorkingDirectory should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ComputerNameNotEmpty()"), TEXT("ComputerName should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int UserNameNotEmpty()"), TEXT("UserName should not be empty"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CanLaunchURLWorks()"), TEXT("CanLaunchURL should return true for https"), 1);
	}

	// ====================================================================
	// Section: Logging
	// ====================================================================

	TEST_METHOD(Logging)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptMathAndPlatformBindingsTests_Private;
		TestRunner->AddExpectedError(TEXT("Test error message"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMathPlatform_Logging"), TEXT(R"(
int LoggingCallsSucceed()
{
	Log("Test log message");
	LogDisplay("Test display message");
	Warning("Test warning message");
	Error("Test error message");
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int LoggingCallsSucceed()"), TEXT("Headless-safe logging helpers should execute"), 1);
	}
};

#endif
