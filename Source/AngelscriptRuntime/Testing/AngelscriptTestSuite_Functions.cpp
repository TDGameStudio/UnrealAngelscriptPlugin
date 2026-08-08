#include "Testing/AngelscriptTestSuite_Functions.h"

#include "Core/AngelscriptEngine.h"
#include "Testing/AngelscriptScriptTestRunner.h"
#include "Testing/AngelscriptTestSuite.h"

#include "Math/Quat.h"
#include "Math/Transform.h"

namespace
{
	TSharedPtr<FAngelscriptScriptTestExecutionContext> RequireContext(
		UAngelscriptTestSuite* Suite)
	{
		TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
			FAngelscriptScriptTestRunner::FindContext(Suite);
		if (!Context.IsValid())
		{
			const FString MisuseMessage =
				TEXT("UAngelscriptTestSuite helper called outside an active method leaf.");
			if (FAngelscriptScriptTestRunner::ReportSuiteLifecycleMisuse(
				Suite,
				MisuseMessage))
			{
				return nullptr;
			}
			if (FAutomationTestBase* Current =
				FAutomationTestFramework::Get().GetCurrentTest())
			{
				Current->AddError(MisuseMessage);
			}
			FAngelscriptEngine::Throw(
				"UAngelscriptTestSuite helper called outside an active method leaf.");
		}
		return Context;
	}
}

FString FAngelscriptScriptTestSuiteBinds::ValueToString(
	const FString& Value)
{
	return Value;
}

FString FAngelscriptScriptTestSuiteBinds::ValueToString(
	const FVector& Value)
{
	return Value.ToString();
}

FString FAngelscriptScriptTestSuiteBinds::ValueToString(
	const FRotator& Value)
{
	return Value.ToString();
}

FString FAngelscriptScriptTestSuiteBinds::ValueToString(
	const FQuat& Value)
{
	return Value.ToString();
}

FString FAngelscriptScriptTestSuiteBinds::ValueToString(
	const FTransform& Value)
{
	return Value.ToString();
}

void FAngelscriptScriptTestSuiteBinds::ReportComparison(
	UAngelscriptTestSuite* Suite,
	bool bPassed,
	const TCHAR* Expression,
	const FString& Expected,
	const FString& Actual,
	const FString& Message)
{
	TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		RequireContext(Suite);
	if (!Context.IsValid() || bPassed)
	{
		return;
	}
	Context->Fail(FString::Printf(
		TEXT("Assertion failed (%s). Expected `%s`, actual `%s`.%s%s"),
		Expression,
		*Expected,
		*Actual,
		Message.IsEmpty() ? TEXT("") : TEXT(" "),
		*Message));
}

void FAngelscriptScriptTestSuiteBinds::AssertPredicate(
	UAngelscriptTestSuite* Suite,
	bool bPassed,
	const TCHAR* Expression,
	const FString& Message)
{
	TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		RequireContext(Suite);
	if (!Context.IsValid() || bPassed)
	{
		return;
	}
	Context->Fail(FString::Printf(
		TEXT("Assertion failed (%s).%s%s"),
		Expression,
		Message.IsEmpty() ? TEXT("") : TEXT(" "),
		*Message));
}

void FAngelscriptScriptTestSuiteBinds::Fail(
	UAngelscriptTestSuite* Suite,
	const FString& Message)
{
	if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		RequireContext(Suite))
	{
		Context->Fail(
			Message.IsEmpty()
				? TEXT("Explicit test failure.")
				: Message);
	}
}

void FAngelscriptScriptTestSuiteBinds::AssertTrue(
	UAngelscriptTestSuite* Suite,
	bool Expression,
	const FString& Message)
{
	AssertPredicate(
		Suite,
		Expression,
		TEXT("Expected true"),
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertFalse(
	UAngelscriptTestSuite* Suite,
	bool Expression,
	const FString& Message)
{
	AssertPredicate(
		Suite,
		!Expression,
		TEXT("Expected false"),
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertNull(
	UAngelscriptTestSuite* Suite,
	const UObject* Object,
	const FString& Message)
{
	AssertPredicate(
		Suite,
		Object == nullptr,
		TEXT("Expected null"),
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertNotNull(
	UAngelscriptTestSuite* Suite,
	const UObject* Object,
	const FString& Message)
{
	AssertPredicate(
		Suite,
		Object != nullptr,
		TEXT("Expected non-null"),
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertSame(
	UAngelscriptTestSuite* Suite,
	const UObject* Expected,
	const UObject* Actual,
	const FString& Message)
{
	AssertPredicate(
		Suite,
		Expected == Actual,
		TEXT("Expected same object"),
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertNotSame(
	UAngelscriptTestSuite* Suite,
	const UObject* Expected,
	const UObject* Actual,
	const FString& Message)
{
	AssertPredicate(
		Suite,
		Expected != Actual,
		TEXT("Expected different objects"),
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertTransformEquals(
	UAngelscriptTestSuite* Suite,
	const FTransform& Expected,
	const FTransform& Actual,
	const FString& Message)
{
	AssertComparison(
		Suite,
		Expected.Equals(Actual, 0.0),
		TEXT("Expected == Actual"),
		Expected,
		Actual,
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertTransformNotEquals(
	UAngelscriptTestSuite* Suite,
	const FTransform& Expected,
	const FTransform& Actual,
	const FString& Message)
{
	AssertComparison(
		Suite,
		!Expected.Equals(Actual, 0.0),
		TEXT("Expected != Actual"),
		Expected,
		Actual,
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertVectorNear(
	UAngelscriptTestSuite* Suite,
	const FVector& Expected,
	const FVector& Actual,
	double Tolerance,
	const FString& Message)
{
	AssertComparison(
		Suite,
		Tolerance >= 0.0 && Expected.Equals(Actual, Tolerance),
		TEXT("FVector::Equals"),
		Expected,
		Actual,
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertRotatorNear(
	UAngelscriptTestSuite* Suite,
	const FRotator& Expected,
	const FRotator& Actual,
	double Tolerance,
	const FString& Message)
{
	AssertComparison(
		Suite,
		Tolerance >= 0.0 && Expected.Equals(Actual, Tolerance),
		TEXT("FRotator::Equals"),
		Expected,
		Actual,
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertQuatNear(
	UAngelscriptTestSuite* Suite,
	const FQuat& Expected,
	const FQuat& Actual,
	double Tolerance,
	const FString& Message)
{
	AssertComparison(
		Suite,
		Tolerance >= 0.0 && Expected.Equals(Actual, Tolerance),
		TEXT("FQuat::Equals"),
		Expected,
		Actual,
		Message);
}

void FAngelscriptScriptTestSuiteBinds::AssertTransformNear(
	UAngelscriptTestSuite* Suite,
	const FTransform& Expected,
	const FTransform& Actual,
	double Tolerance,
	const FString& Message)
{
	AssertComparison(
		Suite,
		Tolerance >= 0.0 && Expected.Equals(Actual, Tolerance),
		TEXT("FTransform::Equals"),
		Expected,
		Actual,
		Message);
}

void FAngelscriptScriptTestSuiteBinds::ExpectError(
	UAngelscriptTestSuite* Suite,
	const FString& Pattern,
	int32 Occurrences)
{
	if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		RequireContext(Suite))
	{
		Context->ExpectError(
			Pattern,
			EAutomationExpectedErrorFlags::MatchType::Contains,
			Occurrences,
			false);
	}
}

void FAngelscriptScriptTestSuiteBinds::ExpectErrorRegex(
	UAngelscriptTestSuite* Suite,
	const FString& Pattern,
	int32 Occurrences)
{
	if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
		RequireContext(Suite))
	{
		Context->ExpectError(
			Pattern,
			EAutomationExpectedErrorFlags::MatchType::Contains,
			Occurrences,
			true);
	}
}
