#pragma once

#include "CoreMinimal.h"

class UAngelscriptTestSuite;

struct FAngelscriptScriptTestSuiteBinds
{
	static void Fail(
		UAngelscriptTestSuite* Suite,
		const FString& Message);
	static void AssertTrue(
		UAngelscriptTestSuite* Suite,
		bool Expression,
		const FString& Message);
	static void AssertFalse(
		UAngelscriptTestSuite* Suite,
		bool Expression,
		const FString& Message);
	static void AssertNull(
		UAngelscriptTestSuite* Suite,
		const UObject* Object,
		const FString& Message);
	static void AssertNotNull(
		UAngelscriptTestSuite* Suite,
		const UObject* Object,
		const FString& Message);
	static void AssertSame(
		UAngelscriptTestSuite* Suite,
		const UObject* Expected,
		const UObject* Actual,
		const FString& Message);
	static void AssertNotSame(
		UAngelscriptTestSuite* Suite,
		const UObject* Expected,
		const UObject* Actual,
		const FString& Message);

	static void AssertTransformEquals(
		UAngelscriptTestSuite* Suite,
		const FTransform& Expected,
		const FTransform& Actual,
		const FString& Message);
	static void AssertTransformNotEquals(
		UAngelscriptTestSuite* Suite,
		const FTransform& Expected,
		const FTransform& Actual,
		const FString& Message);
	static void AssertVectorNear(
		UAngelscriptTestSuite* Suite,
		const FVector& Expected,
		const FVector& Actual,
		double Tolerance,
		const FString& Message);
	static void AssertRotatorNear(
		UAngelscriptTestSuite* Suite,
		const FRotator& Expected,
		const FRotator& Actual,
		double Tolerance,
		const FString& Message);
	static void AssertQuatNear(
		UAngelscriptTestSuite* Suite,
		const FQuat& Expected,
		const FQuat& Actual,
		double Tolerance,
		const FString& Message);
	static void AssertTransformNear(
		UAngelscriptTestSuite* Suite,
		const FTransform& Expected,
		const FTransform& Actual,
		double Tolerance,
		const FString& Message);
	static void ExpectError(
		UAngelscriptTestSuite* Suite,
		const FString& Pattern,
		int32 Occurrences);
	static void ExpectErrorRegex(
		UAngelscriptTestSuite* Suite,
		const FString& Pattern,
		int32 Occurrences);

private:
	template <typename T>
	static FString ValueToString(const T& Value)
	{
		return LexToString(Value);
	}

	static FString ValueToString(const FString& Value);
	static FString ValueToString(const FVector& Value);
	static FString ValueToString(const FRotator& Value);
	static FString ValueToString(const FQuat& Value);
	static FString ValueToString(const FTransform& Value);

	static void ReportComparison(
		UAngelscriptTestSuite* Suite,
		bool bPassed,
		const TCHAR* Expression,
		const FString& Expected,
		const FString& Actual,
		const FString& Message);
	static void AssertPredicate(
		UAngelscriptTestSuite* Suite,
		bool bPassed,
		const TCHAR* Expression,
		const FString& Message);

	template <typename T>
	static void AssertComparison(
		UAngelscriptTestSuite* Suite,
		bool bPassed,
		const TCHAR* Expression,
		const T& Expected,
		const T& Actual,
		const FString& Message)
	{
		ReportComparison(
			Suite,
			bPassed,
			Expression,
			ValueToString(Expected),
			ValueToString(Actual),
			Message);
	}

public:
	template <typename T>
	static void AssertEquals(
		UAngelscriptTestSuite* Suite,
		const T& Expected,
		const T& Actual,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Expected == Actual,
			TEXT("Expected == Actual"),
			Expected,
			Actual,
			Message);
	}

	template <typename T>
	static void AssertNotEquals(
		UAngelscriptTestSuite* Suite,
		const T& Expected,
		const T& Actual,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Expected != Actual,
			TEXT("Expected != Actual"),
			Expected,
			Actual,
			Message);
	}

	template <typename T>
	static void AssertLessThan(
		UAngelscriptTestSuite* Suite,
		T Left,
		T Right,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Left < Right,
			TEXT("Left < Right"),
			Right,
			Left,
			Message);
	}

	template <typename T>
	static void AssertLessThanOrEqual(
		UAngelscriptTestSuite* Suite,
		T Left,
		T Right,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Left <= Right,
			TEXT("Left <= Right"),
			Right,
			Left,
			Message);
	}

	template <typename T>
	static void AssertGreaterThan(
		UAngelscriptTestSuite* Suite,
		T Left,
		T Right,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Left > Right,
			TEXT("Left > Right"),
			Right,
			Left,
			Message);
	}

	template <typename T>
	static void AssertGreaterThanOrEqual(
		UAngelscriptTestSuite* Suite,
		T Left,
		T Right,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Left >= Right,
			TEXT("Left >= Right"),
			Right,
			Left,
			Message);
	}

	template <typename T>
	static void AssertScalarNear(
		UAngelscriptTestSuite* Suite,
		T Expected,
		T Actual,
		T Tolerance,
		const FString& Message)
	{
		AssertComparison(
			Suite,
			Tolerance >= static_cast<T>(0)
				&& FMath::Abs(Expected - Actual) <= Tolerance,
			TEXT("|Expected - Actual| <= Tolerance"),
			Expected,
			Actual,
			Message);
	}
};
