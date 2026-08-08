#include "Testing/AngelscriptTestSuite.h"

#include "AngelscriptBinds.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSuite_Functions.h"

#include "Math/Quat.h"
#include "Math/Transform.h"

namespace
{
	template <typename T>
	void BindEquals(
		FAngelscriptBinds& SuiteBinds,
		const TCHAR* ScriptType)
	{
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertEquals(const %s Expected, const %s Actual, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertEquals<T>);
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertNotEquals(const %s Expected, const %s Actual, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertNotEquals<T>);
	}

	template <typename T>
	void BindRelational(
		FAngelscriptBinds& SuiteBinds,
		const TCHAR* ScriptType)
	{
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertLessThan(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertLessThan<T>);
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertLessThanOrEqual(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertLessThanOrEqual<T>);
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertGreaterThan(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertGreaterThan<T>);
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertGreaterThanOrEqual(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertGreaterThanOrEqual<T>);
	}

	template <typename T>
	void BindScalarNear(
		FAngelscriptBinds& SuiteBinds,
		const TCHAR* ScriptType)
	{
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertNear(const %s Expected, const %s Actual, const %s Tolerance = 0.0001, const FString& Message = \"\")"),
				ScriptType,
				ScriptType,
				ScriptType),
			&FAngelscriptScriptTestSuiteBinds::AssertScalarNear<T>);
	}
}

UWorld* UAngelscriptTestSuite::GetWorld() const
{
	return FAngelscriptScriptTestRunner::FindWorld(this);
}

void UAngelscriptTestSuite::BeforeAll_Implementation()
{
}

void UAngelscriptTestSuite::BeforeEach_Implementation()
{
}

void UAngelscriptTestSuite::AfterEach_Implementation()
{
}

void UAngelscriptTestSuite::AfterAll_Implementation()
{
}

namespace
{
	void BindAngelscriptScriptTestSuite(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds Suite =
			Binds.ExistingClassForTarget(
				"UAngelscriptTestSuite");

		Suite.Method(
			"void Fail(const FString& Message)",
			&FAngelscriptScriptTestSuiteBinds::Fail);
		Suite.Method(
			"void AssertTrue(bool Expression, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertTrue);
		Suite.Method(
			"void AssertFalse(bool Expression, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertFalse);
		Suite.Method(
			"void AssertNull(const UObject Object, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertNull);
		Suite.Method(
			"void AssertNotNull(const UObject Object, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertNotNull);
		Suite.Method(
			"void AssertSame(const UObject Expected, const UObject Actual, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertSame);
		Suite.Method(
			"void AssertNotSame(const UObject Expected, const UObject Actual, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertNotSame);

		BindEquals<int32>(Suite, TEXT("int"));
		BindEquals<int64>(Suite, TEXT("int64"));
		BindEquals<float>(Suite, TEXT("float32"));
		BindEquals<double>(Suite, TEXT("float64"));
		BindEquals<bool>(Suite, TEXT("bool"));
		BindEquals<FName>(Suite, TEXT("FName"));
		BindEquals<FString>(Suite, TEXT("FString"));
		BindEquals<FVector>(Suite, TEXT("FVector"));
		BindEquals<FRotator>(Suite, TEXT("FRotator"));
		BindEquals<FQuat>(Suite, TEXT("FQuat"));
		Suite.Method(
			"void AssertEquals(const FTransform& Expected, const FTransform& Actual, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertTransformEquals);
		Suite.Method(
			"void AssertNotEquals(const FTransform& Expected, const FTransform& Actual, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertTransformNotEquals);

		BindRelational<int32>(Suite, TEXT("int"));
		BindRelational<int64>(Suite, TEXT("int64"));
		BindRelational<float>(Suite, TEXT("float32"));
		BindRelational<double>(Suite, TEXT("float64"));
		BindScalarNear<float>(Suite, TEXT("float32"));
		BindScalarNear<double>(Suite, TEXT("float64"));

		Suite.Method(
			"void AssertNear(const FVector& Expected, const FVector& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertVectorNear);
		Suite.Method(
			"void AssertNear(const FRotator& Expected, const FRotator& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertRotatorNear);
		Suite.Method(
			"void AssertNear(const FQuat& Expected, const FQuat& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertQuatNear);
		Suite.Method(
			"void AssertNear(const FTransform& Expected, const FTransform& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
			&FAngelscriptScriptTestSuiteBinds::AssertTransformNear);

		Suite.Method(
			"void ExpectError(const FString& Pattern, int Occurrences = 1)",
			&FAngelscriptScriptTestSuiteBinds::ExpectError);
		Suite.Method(
			"void ExpectErrorRegex(const FString& Pattern, int Occurrences = 1)",
			&FAngelscriptScriptTestSuiteBinds::ExpectErrorRegex);

	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_AngelscriptScriptTestSuite(
	TEXT("AngelscriptScriptTestSuite.ManualBindings"),
	EAngelscriptBindPhase::ManualBindings,
	&BindAngelscriptScriptTestSuite);
