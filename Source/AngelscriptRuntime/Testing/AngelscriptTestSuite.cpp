#include "Testing/AngelscriptTestSuite.h"

#include "AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Testing/AngelscriptScriptTestRunner.h"

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
			if (FAngelscriptScriptTestRunner::
				ReportSuiteLifecycleMisuse(
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

	template <typename T>
	FString ValueToString(const T& Value)
	{
		return LexToString(Value);
	}

	template <>
	FString ValueToString<FString>(const FString& Value)
	{
		return Value;
	}

	template <>
	FString ValueToString<FVector>(const FVector& Value)
	{
		return Value.ToString();
	}

	template <>
	FString ValueToString<FRotator>(const FRotator& Value)
	{
		return Value.ToString();
	}

	template <>
	FString ValueToString<FQuat>(const FQuat& Value)
	{
		return Value.ToString();
	}

	template <>
	FString ValueToString<FTransform>(const FTransform& Value)
	{
		return Value.ToString();
	}

	template <typename T>
	void AssertComparison(
		UAngelscriptTestSuite* Suite,
		bool bPassed,
		const TCHAR* Expression,
		const T& Expected,
			const T& Actual,
			const FString& Message)
		{
			TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
				RequireContext(Suite);
			if (!Context.IsValid())
			{
				return;
			}
			if (bPassed)
			{
				return;
			}
			Context->Fail(FString::Printf(
				TEXT("Assertion failed (%s). Expected `%s`, actual `%s`.%s%s"),
				Expression,
				*ValueToString(Expected),
				*ValueToString(Actual),
				Message.IsEmpty() ? TEXT("") : TEXT(" "),
				*Message));
		}

	void AssertPredicate(
		UAngelscriptTestSuite* Suite,
			bool bPassed,
			const TCHAR* Expression,
			const FString& Message)
		{
			TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
				RequireContext(Suite);
			if (!Context.IsValid())
			{
				return;
			}
			if (bPassed)
			{
				return;
			}
			Context->Fail(FString::Printf(
				TEXT("Assertion failed (%s).%s%s"),
				Expression,
				Message.IsEmpty() ? TEXT("") : TEXT(" "),
				*Message));
		}

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
			[](UAngelscriptTestSuite* Suite,
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
			});
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertNotEquals(const %s Expected, const %s Actual, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			[](UAngelscriptTestSuite* Suite,
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
			});
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
			[](UAngelscriptTestSuite* Suite,
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
			});
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertLessThanOrEqual(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			[](UAngelscriptTestSuite* Suite,
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
			});
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertGreaterThan(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			[](UAngelscriptTestSuite* Suite,
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
			});
		SuiteBinds.Method(
			FString::Printf(
				TEXT("void AssertGreaterThanOrEqual(const %s Left, const %s Right, const FString& Message = \"\")"),
				ScriptType,
				ScriptType),
			[](UAngelscriptTestSuite* Suite,
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
			});
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
			[](UAngelscriptTestSuite* Suite,
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
			});
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

AS_FORCE_LINK const FAngelscriptBinds::FBind
	Bind_AngelscriptScriptTestSuite(
		(int32)FAngelscriptBinds::EOrder::Late + 2,
		[]
		{
			FAngelscriptBinds Suite =
				FAngelscriptBinds::ExistingClass(
					"UAngelscriptTestSuite");

			Suite.Method(
				"void Fail(const FString& Message)",
				[](UAngelscriptTestSuite* Self,
				   const FString& Message)
				{
					if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
						RequireContext(Self))
					{
						Context->Fail(
							Message.IsEmpty()
								? TEXT("Explicit test failure.")
								: Message);
					}
				});
			Suite.Method(
				"void AssertTrue(bool Expression, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   bool Expression,
				   const FString& Message)
				{
					AssertPredicate(
						Self,
						Expression,
						TEXT("Expected true"),
						Message);
				});
			Suite.Method(
				"void AssertFalse(bool Expression, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   bool Expression,
				   const FString& Message)
				{
					AssertPredicate(
						Self,
						!Expression,
						TEXT("Expected false"),
						Message);
				});
			Suite.Method(
				"void AssertNull(const UObject Object, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const UObject* Object,
				   const FString& Message)
				{
					AssertPredicate(
						Self,
						Object == nullptr,
						TEXT("Expected null"),
						Message);
				});
			Suite.Method(
				"void AssertNotNull(const UObject Object, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const UObject* Object,
				   const FString& Message)
				{
					AssertPredicate(
						Self,
						Object != nullptr,
						TEXT("Expected non-null"),
						Message);
				});
			Suite.Method(
				"void AssertSame(const UObject Expected, const UObject Actual, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const UObject* Expected,
				   const UObject* Actual,
				   const FString& Message)
				{
					AssertPredicate(
						Self,
						Expected == Actual,
						TEXT("Expected same object"),
						Message);
				});
			Suite.Method(
				"void AssertNotSame(const UObject Expected, const UObject Actual, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const UObject* Expected,
				   const UObject* Actual,
				   const FString& Message)
				{
					AssertPredicate(
						Self,
						Expected != Actual,
						TEXT("Expected different objects"),
						Message);
				});

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
				[](UAngelscriptTestSuite* Self,
				   const FTransform& Expected,
				   const FTransform& Actual,
				   const FString& Message)
				{
					AssertComparison(
						Self,
						Expected.Equals(Actual, 0.0),
						TEXT("Expected == Actual"),
						Expected,
						Actual,
						Message);
				});
			Suite.Method(
				"void AssertNotEquals(const FTransform& Expected, const FTransform& Actual, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const FTransform& Expected,
				   const FTransform& Actual,
				   const FString& Message)
				{
					AssertComparison(
						Self,
						!Expected.Equals(Actual, 0.0),
						TEXT("Expected != Actual"),
						Expected,
						Actual,
						Message);
				});

			BindRelational<int32>(Suite, TEXT("int"));
			BindRelational<int64>(Suite, TEXT("int64"));
			BindRelational<float>(Suite, TEXT("float32"));
			BindRelational<double>(Suite, TEXT("float64"));
			BindScalarNear<float>(Suite, TEXT("float32"));
			BindScalarNear<double>(Suite, TEXT("float64"));

			Suite.Method(
				"void AssertNear(const FVector& Expected, const FVector& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const FVector& Expected,
				   const FVector& Actual,
				   double Tolerance,
				   const FString& Message)
				{
					AssertComparison(
						Self,
						Tolerance >= 0.0
							&& Expected.Equals(Actual, Tolerance),
						TEXT("FVector::Equals"),
						Expected,
						Actual,
						Message);
				});
			Suite.Method(
				"void AssertNear(const FRotator& Expected, const FRotator& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const FRotator& Expected,
				   const FRotator& Actual,
				   double Tolerance,
				   const FString& Message)
				{
					AssertComparison(
						Self,
						Tolerance >= 0.0
							&& Expected.Equals(Actual, Tolerance),
						TEXT("FRotator::Equals"),
						Expected,
						Actual,
						Message);
				});
			Suite.Method(
				"void AssertNear(const FQuat& Expected, const FQuat& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const FQuat& Expected,
				   const FQuat& Actual,
				   double Tolerance,
				   const FString& Message)
				{
					AssertComparison(
						Self,
						Tolerance >= 0.0
							&& Expected.Equals(Actual, Tolerance),
						TEXT("FQuat::Equals"),
						Expected,
						Actual,
						Message);
				});
			Suite.Method(
				"void AssertNear(const FTransform& Expected, const FTransform& Actual, float64 Tolerance = 0.0001, const FString& Message = \"\")",
				[](UAngelscriptTestSuite* Self,
				   const FTransform& Expected,
				   const FTransform& Actual,
				   double Tolerance,
				   const FString& Message)
				{
					AssertComparison(
						Self,
						Tolerance >= 0.0
							&& Expected.Equals(Actual, Tolerance),
						TEXT("FTransform::Equals"),
						Expected,
						Actual,
						Message);
				});

			Suite.Method(
				"void ExpectError(const FString& Pattern, int Occurrences = 1)",
				[](UAngelscriptTestSuite* Self,
				   const FString& Pattern,
				   int32 Occurrences)
				{
					if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
						RequireContext(Self))
					{
						Context->ExpectError(
							Pattern,
							EAutomationExpectedErrorFlags::MatchType::Contains,
							Occurrences,
							false);
					}
				});
			Suite.Method(
				"void ExpectErrorRegex(const FString& Pattern, int Occurrences = 1)",
				[](UAngelscriptTestSuite* Self,
				   const FString& Pattern,
				   int32 Occurrences)
				{
					if (TSharedPtr<FAngelscriptScriptTestExecutionContext> Context =
						RequireContext(Self))
					{
						Context->ExpectError(
							Pattern,
							EAutomationExpectedErrorFlags::MatchType::Contains,
							Occurrences,
							true);
					}
				});

		});
