// ============================================================================
// AngelscriptTransformBindingsTests.cpp
//
// FTransform binding coverage �?CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Transform.FAngelscriptTransformBindingsTest.*
//
// Sections:
//   TransformPosition    �?TransformPosition, TransformPositionNoScale,
//                          InverseTransformPosition
//   RelativeTransform    �?GetRelativeTransform, Equals verification
//   SettersAndGetters    �?SetTranslation, SetScale3D, GetTranslation, GetScale3D
//
// CQTest adaptation notes:
//   Native transforms computed at test time and substituted via ReplaceInline
//   for deterministic parity checks.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Math/Quat.h"
#include "Math/Transform.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptTransformBindingsTest,
	"Angelscript.TestModule.Bindings.Transform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString FormatScriptFloat(double Value)
{
	FString Literal = FString::Printf(TEXT("%.9g"), Value);
	if (!Literal.Contains(TEXT(".")) && !Literal.Contains(TEXT("e")) && !Literal.Contains(TEXT("E")))
	{
		Literal += TEXT(".0");
	}
	return Literal;
}

static FString FormatScriptVector(const FVector& V)
{
	return FString::Printf(TEXT("FVector(%s, %s, %s)"),
		*FormatScriptFloat(V.X), *FormatScriptFloat(V.Y), *FormatScriptFloat(V.Z));
}

static FString FormatScriptRotator(const FRotator& R)
{
	return FString::Printf(TEXT("FRotator(%s, %s, %s)"),
		*FormatScriptFloat(R.Pitch), *FormatScriptFloat(R.Yaw), *FormatScriptFloat(R.Roll));
}

static FString FormatScriptQuat(const FQuat& Q)
{
	return FString::Printf(TEXT("FQuat(%s, %s, %s, %s)"),
		*FormatScriptFloat(Q.X), *FormatScriptFloat(Q.Y), *FormatScriptFloat(Q.Z), *FormatScriptFloat(Q.W));
}

static FString FormatScriptTransform(const FTransform& T)
{
	return FString::Printf(TEXT("FTransform(%s, %s, %s)"),
		*FormatScriptQuat(T.GetRotation()),
		*FormatScriptVector(T.GetTranslation()),
		*FormatScriptVector(T.GetScale3D()));
}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: TransformPosition
	// ====================================================================

	TEST_METHOD(TransformPosition)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FRotator BaseRot(0.0, 90.0, 0.0);
		const FVector BaseTrans(10.0, 20.0, 30.0);
		const FVector BaseScale(2.0, 3.0, 4.0);
		const FTransform Baseline(BaseRot, BaseTrans, BaseScale);
		const FVector Input(1.0, 0.0, 0.0);

		const FVector ExpTransformed = Baseline.TransformPosition(Input);
		const FVector ExpTransformedNoScale = Baseline.TransformPositionNoScale(Input);
		const FVector ExpInverse = Baseline.InverseTransformPosition(ExpTransformed);

		constexpr double Tol = 0.001;

		FString Script = TEXT(R"(
FTransform MakeBaseline()
{
	return FTransform($BASELINE_ROT$, $BASELINE_TRANS$, $BASELINE_SCALE$);
}

int Transform_TransformPosition()
{
	FVector Result = MakeBaseline().TransformPosition($INPUT$);
	return Result.Equals($EXP_TRANSFORMED$, $TOL$) ? 1 : 0;
}
int Transform_TransformPositionNoScale()
{
	FVector Result = MakeBaseline().TransformPositionNoScale($INPUT$);
	return Result.Equals($EXP_NO_SCALE$, $TOL$) ? 1 : 0;
}
int Transform_InverseTransformPosition()
{
	FVector Result = MakeBaseline().InverseTransformPosition($EXP_TRANSFORMED$);
	return Result.Equals($EXP_INVERSE$, $TOL$) ? 1 : 0;
}
int Transform_ScaleAffectsResult()
{
	FVector WithScale = MakeBaseline().TransformPosition($INPUT$);
	FVector NoScale = MakeBaseline().TransformPositionNoScale($INPUT$);
	return (!WithScale.Equals(NoScale, 0.0)) ? 1 : 0;
}
)");
		Script.ReplaceInline(TEXT("$BASELINE_ROT$"), *FormatScriptRotator(BaseRot));
		Script.ReplaceInline(TEXT("$BASELINE_TRANS$"), *FormatScriptVector(BaseTrans));
		Script.ReplaceInline(TEXT("$BASELINE_SCALE$"), *FormatScriptVector(BaseScale));
		Script.ReplaceInline(TEXT("$INPUT$"), *FormatScriptVector(Input));
		Script.ReplaceInline(TEXT("$EXP_TRANSFORMED$"), *FormatScriptVector(ExpTransformed));
		Script.ReplaceInline(TEXT("$EXP_NO_SCALE$"), *FormatScriptVector(ExpTransformedNoScale));
		Script.ReplaceInline(TEXT("$EXP_INVERSE$"), *FormatScriptVector(ExpInverse));
		Script.ReplaceInline(TEXT("$TOL$"), *FormatScriptFloat(Tol));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTransform_TransformPosition"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_TransformPosition()"), TEXT("TransformPosition parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_TransformPositionNoScale()"), TEXT("TransformPositionNoScale parity"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_InverseTransformPosition()"), TEXT("InverseTransformPosition recovers input"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_ScaleAffectsResult()"), TEXT("scale affects TransformPosition"), 1);
	}

	// ====================================================================
	// Section: RelativeTransform
	// ====================================================================

	TEST_METHOD(RelativeTransform)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FRotator BaseRot(0.0, 90.0, 0.0);
		const FVector BaseTrans(10.0, 20.0, 30.0);
		const FVector BaseScale(2.0, 3.0, 4.0);
		const FTransform Baseline(BaseRot, BaseTrans, BaseScale);

		const FRotator OtherRot(15.0, -45.0, 5.0);
		const FVector OtherTrans(-5.0, 6.0, 7.0);
		const FVector OtherScale(1.5, 0.5, 2.0);
		const FTransform Other(OtherRot, OtherTrans, OtherScale);

		const FTransform ExpRelative = Baseline.GetRelativeTransform(Other);
		constexpr double Tol = 0.001;

		FString Script = TEXT(R"(
FTransform MakeBaseline()
{
	return FTransform($BASELINE_ROT$, $BASELINE_TRANS$, $BASELINE_SCALE$);
}
FTransform MakeOther()
{
	return FTransform($OTHER_ROT$, $OTHER_TRANS$, $OTHER_SCALE$);
}

int Transform_GetRelativeTransform()
{
	FTransform Rel = MakeBaseline().GetRelativeTransform(MakeOther());
	return Rel.Equals($EXP_RELATIVE$, $TOL$) ? 1 : 0;
}
int Transform_RelativeNotIdentity()
{
	FTransform Rel = MakeBaseline().GetRelativeTransform(MakeOther());
	return (!Rel.Equals(FTransform::Identity, $TOL$)) ? 1 : 0;
}
)");
		Script.ReplaceInline(TEXT("$BASELINE_ROT$"), *FormatScriptRotator(BaseRot));
		Script.ReplaceInline(TEXT("$BASELINE_TRANS$"), *FormatScriptVector(BaseTrans));
		Script.ReplaceInline(TEXT("$BASELINE_SCALE$"), *FormatScriptVector(BaseScale));
		Script.ReplaceInline(TEXT("$OTHER_ROT$"), *FormatScriptRotator(OtherRot));
		Script.ReplaceInline(TEXT("$OTHER_TRANS$"), *FormatScriptVector(OtherTrans));
		Script.ReplaceInline(TEXT("$OTHER_SCALE$"), *FormatScriptVector(OtherScale));
		Script.ReplaceInline(TEXT("$EXP_RELATIVE$"), *FormatScriptTransform(ExpRelative));
		Script.ReplaceInline(TEXT("$TOL$"), *FormatScriptFloat(Tol));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTransform_RelativeTransform"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_GetRelativeTransform()"), TEXT("GetRelativeTransform matches native"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_RelativeNotIdentity()"), TEXT("relative is not identity"), 1);
	}

	// ====================================================================
	// Section: SettersAndGetters
	// ====================================================================

	TEST_METHOD(SettersAndGetters)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FRotator BaseRot(0.0, 90.0, 0.0);
		const FVector BaseTrans(10.0, 20.0, 30.0);
		const FVector BaseScale(2.0, 3.0, 4.0);
		const FTransform Baseline(BaseRot, BaseTrans, BaseScale);

		const FVector UpdatedTrans(-11.0, 12.0, 13.0);
		const FVector UpdatedScale(8.0, 9.0, 10.0);

		FTransform ExpUpdated = Baseline;
		ExpUpdated.SetTranslation(UpdatedTrans);
		ExpUpdated.SetScale3D(UpdatedScale);

		constexpr double Tol = 0.001;

		FString Script = TEXT(R"(
FTransform MakeBaseline()
{
	return FTransform($BASELINE_ROT$, $BASELINE_TRANS$, $BASELINE_SCALE$);
}

int Transform_SetTranslation()
{
	FTransform T = MakeBaseline();
	T.SetTranslation($UPDATED_TRANS$);
	return T.GetTranslation().Equals($UPDATED_TRANS$, 0.0) ? 1 : 0;
}
int Transform_SetScale3D()
{
	FTransform T = MakeBaseline();
	T.SetScale3D($UPDATED_SCALE$);
	return T.GetScale3D().Equals($UPDATED_SCALE$, 0.0) ? 1 : 0;
}
int Transform_UpdatedMatchesNative()
{
	FTransform T = MakeBaseline();
	T.SetTranslation($UPDATED_TRANS$);
	T.SetScale3D($UPDATED_SCALE$);
	return T.Equals($EXP_UPDATED$, $TOL$) ? 1 : 0;
}
int Transform_UpdatedDiffersFromBaseline()
{
	FTransform T = MakeBaseline();
	T.SetTranslation($UPDATED_TRANS$);
	T.SetScale3D($UPDATED_SCALE$);
	return (!T.Equals(MakeBaseline(), $TOL$)) ? 1 : 0;
}
int Transform_GetLocation()
{
	FTransform T = MakeBaseline();
	return T.GetTranslation().Equals($BASELINE_TRANS$, 0.0) ? 1 : 0;
}
int Transform_GetScale()
{
	FTransform T = MakeBaseline();
	return T.GetScale3D().Equals($BASELINE_SCALE$, 0.0) ? 1 : 0;
}
)");
		Script.ReplaceInline(TEXT("$BASELINE_ROT$"), *FormatScriptRotator(BaseRot));
		Script.ReplaceInline(TEXT("$BASELINE_TRANS$"), *FormatScriptVector(BaseTrans));
		Script.ReplaceInline(TEXT("$BASELINE_SCALE$"), *FormatScriptVector(BaseScale));
		Script.ReplaceInline(TEXT("$UPDATED_TRANS$"), *FormatScriptVector(UpdatedTrans));
		Script.ReplaceInline(TEXT("$UPDATED_SCALE$"), *FormatScriptVector(UpdatedScale));
		Script.ReplaceInline(TEXT("$EXP_UPDATED$"), *FormatScriptTransform(ExpUpdated));
		Script.ReplaceInline(TEXT("$TOL$"), *FormatScriptFloat(Tol));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASTransform_SettersAndGetters"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_SetTranslation()"), TEXT("SetTranslation reflected in GetTranslation"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_SetScale3D()"), TEXT("SetScale3D reflected in GetScale3D"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_UpdatedMatchesNative()"), TEXT("updated matches native expected"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_UpdatedDiffersFromBaseline()"), TEXT("updated differs from baseline"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_GetLocation()"), TEXT("GetTranslation returns construction value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Transform_GetScale()"), TEXT("GetScale3D returns construction value"), 1);
	}
};

#endif
