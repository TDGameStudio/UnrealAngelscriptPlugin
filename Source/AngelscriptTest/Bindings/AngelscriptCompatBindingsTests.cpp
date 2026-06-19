// ============================================================================
// AngelscriptCompatBindingsTests.cpp
//
// Compat binding coverage -- CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Compat.FAngelscriptCompatBindingsTest.*
//
// Sections:
//   ObjectCastCompat         -- Cast<T> and n"" literal syntax
//   ObjectEditorOnly         -- IsEditorOnly on package
//   ObjectEditorOnlyParity   -- parity between native and script IsEditorOnly
//   TimespanCompat           -- FTimespan legacy compat syntax
//   DateTimeCompat           -- FDateTime legacy compat syntax
//
// CQTest adaptation notes:
//   Five separate legacy automation tests merged into one TEST_CLASS.
//   ObjectCastCompat uses annotated module compilation (UCLASS/UFUNCTION) so
//   the original structure is preserved within its TEST_METHOD.
//   ObjectEditorOnlyParity uses SHARE_CLEAN since it spawns named objects.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Components/InputComponent.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptCompatBindingsTest,
	"Angelscript.TestModule.Bindings.Compat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ObjectCastCompat
	// ====================================================================

	TEST_METHOD(ObjectCastCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Plain module: test Cast<T> and n"" literal
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCompat_CastCompat"), TEXT(R"(
int Compat_CastPackage()
{
	UObject Object = FindObject(GetTransientPackage().GetPathName());
	UPackage Package = Cast<UPackage>(Object);
	return IsValid(Package) ? 1 : 0;
}

int Compat_NameLiteral()
{
	FName LiteralName = n"Compat_Name";
	return (LiteralName == FName("Compat_Name")) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_CastPackage()"), TEXT("Cast<UPackage> on transient package should succeed"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_NameLiteral()"), TEXT("n\"\" literal syntax should produce matching FName"), 1);

		// Annotated module: test Cast<T> on generated script class
		const bool bAnnotatedCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("ASAnnotatedCastCompat"),
			TEXT("ASAnnotatedCastCompat.as"),
			TEXT(R"(
UCLASS()
class ABindingCastActor : AActor
{
}

UCLASS()
class UBindingCastComponent : UActorComponent
{
	UFUNCTION()
	int ReadCastCompat()
	{
		ABindingCastActor OwnerActor = Cast<ABindingCastActor>(GetOwner());
		FName ExpectedName = n"BindingCastOwner";

		if (OwnerActor == null)
			return 0;
		if (!(ExpectedName == FName("BindingCastOwner")))
			return 0;

		return 1;
	}
}

)"));
		ASSERT_THAT(IsTrue(bAnnotatedCompiled, TEXT("Compile annotated module using Cast<T> compat syntax should succeed")));

		UClass* RuntimeActorClass = FindGeneratedClass(&Engine, TEXT("ABindingCastActor"));
		UClass* RuntimeComponentClass = FindGeneratedClass(&Engine, TEXT("UBindingCastComponent"));
		ASSERT_THAT(IsNotNull(RuntimeActorClass, TEXT("Generated actor class for compat cast should exist")));
		ASSERT_THAT(IsNotNull(RuntimeComponentClass, TEXT("Generated component class for compat cast should exist")));

		UFunction* ReadCastCompatFunction = FindGeneratedFunction(RuntimeComponentClass, TEXT("ReadCastCompat"));
		ASSERT_THAT(IsNotNull(ReadCastCompatFunction, TEXT("Compat cast function should exist")));
		AActor* RuntimeActor = NewObject<AActor>(GetTransientPackage(), RuntimeActorClass);
		ASSERT_THAT(IsNotNull(RuntimeActor, TEXT("Generated compat actor instance should be created")));

		UActorComponent* RuntimeComponent = NewObject<UActorComponent>(RuntimeActor, RuntimeComponentClass);
		ASSERT_THAT(IsNotNull(RuntimeComponent, TEXT("Generated compat component instance should be created")));

		int32 AnnotatedResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeComponent, ReadCastCompatFunction, AnnotatedResult),
			TEXT("Compat cast reflected call should execute on the game thread")));
		ASSERT_THAT(AreEqual(1, AnnotatedResult, TEXT("Annotated module Cast<T> should cast native return values to generated script classes")));
	}

	// ====================================================================
	// Section: ObjectEditorOnly
	// ====================================================================

	TEST_METHOD(ObjectEditorOnly)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCompat_EditorOnly"), TEXT(R"(
int Compat_EditorOnly_Package()
{
	UPackage Package = GetTransientPackage();
	if (Package.IsEditorOnly())
		return 10;
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_EditorOnly_Package()"), TEXT("Transient package IsEditorOnly should return false (1)"), 1);
	}

	// ====================================================================
	// Section: ObjectEditorOnlyParity
	// ====================================================================

	TEST_METHOD(ObjectEditorOnlyParity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FName NonEditorOnlyName(*FString::Printf(
			TEXT("ASObjectEditorOnlyFalse_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const FName EditorOnlyName(*FString::Printf(
			TEXT("ASObjectEditorOnlyTrue_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));

		UInputComponent* NonEditorOnlyComponent = NewObject<UInputComponent>(GetTransientPackage(), NonEditorOnlyName, RF_Transient);
		UInputComponent* EditorOnlyComponent = NewObject<UInputComponent>(GetTransientPackage(), EditorOnlyName, RF_Transient);
		ASSERT_THAT(IsNotNull(NonEditorOnlyComponent, TEXT("Non-editor-only input component should be created")));
		ASSERT_THAT(IsNotNull(EditorOnlyComponent, TEXT("Editor-only input component should be created")));

		EditorOnlyComponent->bIsEditorOnly = true;

		ON_SCOPE_EXIT
		{
			if (EditorOnlyComponent != nullptr)
			{
				EditorOnlyComponent->bIsEditorOnly = false;
				EditorOnlyComponent->MarkAsGarbage();
			}

			if (NonEditorOnlyComponent != nullptr)
			{
				NonEditorOnlyComponent->MarkAsGarbage();
			}

			ASTEST_RESET_ENGINE(Engine);
		};

		const bool bNativeNonEditorOnly = NonEditorOnlyComponent->IsEditorOnly();
		const bool bNativeEditorOnly = EditorOnlyComponent->IsEditorOnly();
		ASSERT_THAT(IsFalse(bNativeNonEditorOnly, TEXT("Default transient input component should remain non-editor-only")));
		ASSERT_THAT(IsTrue(bNativeEditorOnly, TEXT("Input component with bIsEditorOnly should report editor-only natively")));

		const FString ScriptSource = FString::Printf(
			TEXT(R"(
int Compat_EditorOnlyParity()
{
	UObject NonEditorOnly = FindObject("%s");
	UObject EditorOnly = FindObject("%s");

	if (NonEditorOnly == null)
		return 100;
	if (EditorOnly == null)
		return 200;

	int Result = 0;
	if (NonEditorOnly.IsEditorOnly())
		Result += 2;
	if (EditorOnly.IsEditorOnly())
		Result += 1;

	return Result;
}
)"),
			*NonEditorOnlyComponent->GetPathName().ReplaceCharWithEscapedChar(),
			*EditorOnlyComponent->GetPathName().ReplaceCharWithEscapedChar());

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCompat_EditorOnlyParity"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const int32 ExpectedResult = (bNativeNonEditorOnly ? 2 : 0) + (bNativeEditorOnly ? 1 : 0);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_EditorOnlyParity()"), TEXT("Script IsEditorOnly should match native results"), ExpectedResult);
	}

	// ====================================================================
	// Section: TimespanCompat
	// ====================================================================

	TEST_METHOD(TimespanCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCompat_TimespanCompat"), TEXT(R"(
int Compat_Timespan_Zero()
{
	FTimespan Zero = FTimespan::Zero();
	return Zero.IsZero() ? 1 : 0;
}

int Compat_Timespan_FromSeconds()
{
	FTimespan NinetySeconds = FTimespan::FromSeconds(90.0);
	if (NinetySeconds.GetMinutes() != 1) return 0;
	if (NinetySeconds.GetSeconds() != 30) return 0;
	if (NinetySeconds.GetTotalSeconds() != 90.0) return 0;
	return 1;
}

int Compat_Timespan_FromHours()
{
	FTimespan TwoHours = FTimespan::FromHours(2.0);
	if (TwoHours.GetHours() != 2) return 0;
	if (TwoHours.GetTotalMinutes() != 120.0) return 0;
	return 1;
}

int Compat_Timespan_Constructed()
{
	FTimespan Constructed(1, 2, 3);
	if (Constructed.GetHours() != 1) return 0;
	if (Constructed.GetMinutes() != 2) return 0;
	if (Constructed.GetSeconds() != 3) return 0;
	return 1;
}

int Compat_Timespan_CopyAndCompare()
{
	FTimespan Constructed(1, 2, 3);
	FTimespan Copy = Constructed;
	if (!(Copy == Constructed)) return 0;
	if (Copy.opCmp(Constructed) != 0) return 0;
	return 1;
}

int Compat_Timespan_Ordering()
{
	FTimespan Constructed(1, 2, 3);
	FTimespan Longer = FTimespan::FromHours(2.0);
	if (!(Longer.opCmp(Constructed) > 0)) return 0;
	if (Longer.GetTotalDays() <= 0.0) return 0;
	if (Longer.ToString().IsEmpty()) return 0;
	return 1;
}

int Compat_Timespan_Arithmetic()
{
	FTimespan Constructed(1, 2, 3);
	FTimespan Sum = Constructed + FTimespan::FromMinutes(30.0);
	if (Sum.GetTotalMinutes() < Constructed.GetTotalMinutes() + 29.99 || Sum.GetTotalMinutes() > Constructed.GetTotalMinutes() + 30.01)
		return 0;

	FTimespan Difference = Sum - Constructed;
	if (Difference.GetTotalMinutes() < 29.99 || Difference.GetTotalMinutes() > 30.01)
		return 0;

	FTimespan Doubled = Difference * 2.0;
	if (Doubled.GetTotalMinutes() < 59.99 || Doubled.GetTotalMinutes() > 60.01)
		return 0;

	FTimespan Halved = Doubled / 2.0;
	if (Halved.GetTotalMinutes() < 29.99 || Halved.GetTotalMinutes() > 30.01)
		return 0;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_Zero()"), TEXT("FTimespan::Zero should be zero"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_FromSeconds()"), TEXT("FTimespan::FromSeconds should decompose correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_FromHours()"), TEXT("FTimespan::FromHours should decompose correctly"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_Constructed()"), TEXT("FTimespan(h,m,s) constructor should populate components"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_CopyAndCompare()"), TEXT("Copy and equality comparison should work"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_Ordering()"), TEXT("opCmp ordering and ToString should work"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_Timespan_Arithmetic()"), TEXT("Arithmetic operators should produce correct results"), 1);
	}

	// ====================================================================
	// Section: DateTimeCompat
	// ====================================================================

	TEST_METHOD(DateTimeCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCompat_DateTimeCompat"), TEXT(R"(
int Compat_DateTime_Epoch()
{
	FDateTime Epoch = FDateTime::FromUnixTimestamp(0);
	if (Epoch.GetYear() != 1970) return 0;
	if (Epoch.GetMonth() != 1) return 0;
	if (Epoch.GetDay() != 1) return 0;
	if (Epoch.ToUnixTimestamp() != 0) return 0;
	return 1;
}

int Compat_DateTime_Constructed()
{
	FDateTime Constructed(2024, 12, 25, 14, 30, 15, 0);
	if (Constructed.GetYear() != 2024) return 0;
	if (Constructed.GetMonth() != 12) return 0;
	if (Constructed.GetDay() != 25) return 0;
	if (Constructed.GetHour() != 14) return 0;
	if (Constructed.GetMinute() != 30) return 0;
	if (Constructed.GetSecond() != 15) return 0;
	if (!Constructed.IsAfternoon()) return 0;
	if (Constructed.IsMorning()) return 0;
	return 1;
}

int Compat_DateTime_CopyAndCompare()
{
	FDateTime Constructed(2024, 12, 25, 14, 30, 15, 0);
	FDateTime Copy = Constructed;
	if (!(Copy == Constructed)) return 0;
	if (Copy.opCmp(Constructed) != 0) return 0;
	return 1;
}

int Compat_DateTime_Arithmetic()
{
	FDateTime Constructed(2024, 12, 25, 14, 30, 15, 0);
	FTimespan OneDay = FTimespan::FromDays(1.0);
	FDateTime NextDay = Constructed + OneDay;
	if (!(NextDay.opCmp(Constructed) > 0)) return 0;
	NextDay -= OneDay;
	if (!(NextDay == Constructed)) return 0;
	return 1;
}

int Compat_DateTime_LeapYear()
{
	if (FDateTime::DaysInMonth(2024, 2) != 29) return 0;
	if (!FDateTime::IsLeapYear(2024)) return 0;
	if (FDateTime::DaysInYear(2024) != 366) return 0;
	return 1;
}

int Compat_DateTime_Ordering()
{
	FDateTime Epoch = FDateTime::FromUnixTimestamp(0);
	FDateTime Constructed(2024, 12, 25, 14, 30, 15, 0);
	if (Epoch.opCmp(Constructed) >= 0) return 0;
	return 1;
}

int Compat_DateTime_Formatting()
{
	FDateTime Constructed(2024, 12, 25, 14, 30, 15, 0);
	if (Constructed.ToIso8601().IsEmpty()) return 0;
	if (Constructed.ToString().IsEmpty()) return 0;
	if (Constructed.ToString("%%Y-%%m-%%d").IsEmpty()) return 0;
	return 1;
}

int Compat_DateTime_NowAndToday()
{
	FDateTime Today = FDateTime::Today();
	if (Today.GetHour() != 0) return 0;

	FDateTime Now = FDateTime::Now();
	if (Now.GetYear() < 2020) return 0;

	FDateTime UtcNow = FDateTime::UtcNow();
	if (UtcNow.GetYear() < 2020) return 0;

	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_Epoch()"), TEXT("Unix epoch should decompose to 1970-01-01"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_Constructed()"), TEXT("Full constructor should populate all components"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_CopyAndCompare()"), TEXT("Copy and equality should work"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_Arithmetic()"), TEXT("Add/subtract timespan should round-trip"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_LeapYear()"), TEXT("Leap year detection and day counts should be correct"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_Ordering()"), TEXT("opCmp ordering should work across dates"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_Formatting()"), TEXT("ToString and ToIso8601 should produce non-empty strings"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Compat_DateTime_NowAndToday()"), TEXT("Today/Now/UtcNow should return reasonable values"), 1);
	}
};

#endif
