#include "../Support/AngelscriptNativeCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_string.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptNativeTestSupport;

namespace AngelscriptStringInternalPrivate
{
	struct FStringState
	{
		const TCHAR* Id;
		const char* Value;
		size_t Length;
	};

	inline asCString MakeValue(const FStringState& State)
	{
		return asCString(State.Value, State.Length);
	}
}

TEST_CLASS_WITH_FLAGS(FStringInternalTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.StringInternal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CoreOperationsBySourceState)
	{
		AS_NATIVE_PRODUCT("FRONTEND-STRING-CORE-OPERATIONS",
			ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		enum class EOperation : uint8
		{
			Length,
			Append,
			Substring,
			PrefixSuffix,
			Format,
			Clear,
		};
		struct FOperationCase
		{
			const TCHAR* Id;
			EOperation Operation;
		};
		const FOperationCase Operations[] =
		{
			{ TEXT("length"), EOperation::Length },
			{ TEXT("append"), EOperation::Append },
			{ TEXT("substring"), EOperation::Substring },
			{ TEXT("prefix_suffix"), EOperation::PrefixSuffix },
			{ TEXT("format"), EOperation::Format },
			{ TEXT("clear"), EOperation::Clear },
		};
		const AngelscriptStringInternalPrivate::FStringState States[] =
		{
			{ TEXT("empty"), "", 0 },
			{ TEXT("short_local"), "Alpha", 5 },
			{ TEXT("long_dynamic"), "AlphaBetaGammaDeltaEpsilon", 26 },
			{ TEXT("explicit_length"), "Alpha\0Tail", 5 },
		};

		int32 ObservedCaseCount = 0;
		for (const FOperationCase& Operation : Operations)
		{
			for (const AngelscriptStringInternalPrivate::FStringState& State : States)
			{
				const FString CaseId = FString::Printf(
					TEXT("FRONTEND-STRING-CORE-OPERATIONS-%s-%s"),
					Operation.Id,
					State.Id);
				asCString Value = AngelscriptStringInternalPrivate::MakeValue(State);
				switch (Operation.Operation)
				{
				case EOperation::Length:
					ASSERT_THAT(AreEqual(static_cast<int32>(State.Length), static_cast<int32>(Value.GetLength()),
						FString::Printf(TEXT("%s should retain the explicit source length"), *CaseId)));
					ASSERT_THAT(IsNotNull(Value.AddressOf(),
						FString::Printf(TEXT("%s should expose a null-terminated address"), *CaseId)));
					break;
				case EOperation::Append:
					Value += "_Tail";
					ASSERT_THAT(AreEqual(static_cast<int32>(State.Length + 5), static_cast<int32>(Value.GetLength()),
						FString::Printf(TEXT("%s should append and update its length"), *CaseId)));
					ASSERT_THAT(IsTrue(Value.EndsWith("_Tail"),
						FString::Printf(TEXT("%s should expose the appended suffix"), *CaseId)));
					break;
				case EOperation::Substring:
				{
					const asCString Prefix = Value.SubString(0, Value.GetLength() > 3 ? 3 : Value.GetLength());
					const int32 ExpectedLength = static_cast<int32>(Value.GetLength() > 3 ? 3 : Value.GetLength());
					ASSERT_THAT(AreEqual(ExpectedLength, static_cast<int32>(Prefix.GetLength()),
						FString::Printf(TEXT("%s should retain the requested substring length"), *CaseId)));
					if (ExpectedLength > 0)
					{
						ASSERT_THAT(IsTrue(Prefix.StartsWith("A"),
							FString::Printf(TEXT("%s should retain the source prefix"), *CaseId)));
					}
					break;
				}
				case EOperation::PrefixSuffix:
					ASSERT_THAT(AreEqual(State.Length > 0, Value.StartsWith("A"),
						FString::Printf(TEXT("%s should classify its prefix"), *CaseId)));
					ASSERT_THAT(AreEqual(State.Length > 0 && State.Value[State.Length - 1] == 'a', Value.EndsWith("a"),
						FString::Printf(TEXT("%s should classify its suffix without changing case"), *CaseId)));
					break;
				case EOperation::Format:
					Value.Format("%s-%d", "Fmt", static_cast<int32>(State.Length));
					ASSERT_THAT(IsTrue(Value.StartsWith("Fmt-"),
						FString::Printf(TEXT("%s should format into the owned buffer"), *CaseId)));
					break;
				case EOperation::Clear:
					Value.Clear();
					ASSERT_THAT(AreEqual(0, static_cast<int32>(Value.GetLength()),
						FString::Printf(TEXT("%s should clear its logical length"), *CaseId)));
					ASSERT_THAT(IsTrue(Value.Equals(""),
						FString::Printf(TEXT("%s should clear its visible contents"), *CaseId)));
					break;
				}
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(24, ObservedCaseCount,
			TEXT("The string core product should execute every operation/source-state pair")));
	}

	TEST_METHOD(MutationComparisonDistanceAndPointerViews)
	{
		AS_NATIVE_PRODUCT("FRONTEND-STRING-COMPARISON-POINTER",
			ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const char ExternalText[] = "Alpha";
		asCString ExternalControl("Alpha");
		{
			asCString Value("Alpha");
			asCString Other("alpha");
			asCString Different("Beta");

			Value.Assign("Gamma", 5);
			ASSERT_THAT(IsTrue(Value.Equals("Gamma"), TEXT("Assign should replace the owned text")));
			Value.Concatenate("Tail", 4);
			ASSERT_THAT(IsTrue(Value.Equals("GammaTail"), TEXT("Concatenate should append an explicit byte range")));
			ASSERT_THAT(AreEqual(9, static_cast<int32>(Value.GetLength()), TEXT("Concatenate should update the length")));
			Value.SetLength(5);
			ASSERT_THAT(IsTrue(Value.Equals("Gamma"), TEXT("SetLength should preserve the selected prefix")));
			Value.AddressOf()[0] = 'O';
			ASSERT_THAT(IsTrue(Value[0] == 'O', TEXT("The mutable address should expose the changed byte")));
			Value.AddressOf()[1] = '\0';
			ASSERT_THAT(AreEqual(1, static_cast<int32>(Value.RecalculateLength()), TEXT("RecalculateLength should observe a newly inserted terminator")));

			asCString CompareValue("Alpha");
			int32 MatchCount = 0;
			ASSERT_THAT(AreEqual(4, CompareValue.FindLast("a", &MatchCount), TEXT("FindLast should locate the final case-sensitive match")));
			ASSERT_THAT(AreEqual(1, MatchCount, TEXT("FindLast should count the matching substring")));
			ASSERT_THAT(IsTrue(CompareValue.Equals_CaseInsensitive(Other), TEXT("Case-insensitive equality should ignore letter case")));
			ASSERT_THAT(IsTrue(CompareValue.Compare("Beta") < 0, TEXT("Compare should preserve lexical ordering")));
			ASSERT_THAT(IsTrue(CompareValue.Compare(Different) < 0, TEXT("Object comparison should preserve lexical ordering")));
			ASSERT_THAT(IsTrue(CompareValue.Compare("Alpha", 5) == 0, TEXT("Length-bounded comparison should include the full range")));
			ASSERT_THAT(IsTrue(CompareValue == "Alpha", TEXT("String/operator equality should agree")));
			ASSERT_THAT(IsTrue(CompareValue != Different, TEXT("String/operator inequality should agree")));
			ASSERT_THAT(IsTrue(CompareValue < Different, TEXT("String/operator ordering should agree")));
			ASSERT_THAT(IsTrue(CompareValue[0] == 'A', TEXT("Const indexing should expose the first byte")));
			CompareValue[0] = 'B';
			ASSERT_THAT(IsTrue(CompareValue.StartsWith("B"), TEXT("Mutable indexing should update the owned byte")));

			asCString Formatted;
			ASSERT_THAT(AreEqual(6, static_cast<int32>(Formatted.Format("%s%d", "Value", 1)),
				TEXT("Format should report the resulting byte length")));
			ASSERT_THAT(IsTrue(Formatted.Equals("Value1"), TEXT("Format should write the expected bytes")));
			asCString DistanceValue("Alpha");
			ASSERT_THAT(AreEqual(1, DistanceValue.LevenshteinDistance(asCString("Alph")),
				TEXT("LevenshteinDistance should compute the edit count")));

			asCStringPointer ExternalPointer(ExternalText, 5);
			asCString PointerValue("Alpha");
			asCStringPointer ObjectPointer(&PointerValue);
			ASSERT_THAT(AreEqual(5, static_cast<int32>(ExternalPointer.GetLength()),
				TEXT("External pointer view should retain its explicit length")));
			ASSERT_THAT(IsTrue(ExternalPointer == ObjectPointer,
				TEXT("Pointer views should compare by content rather than storage owner")));
			ASSERT_THAT(IsTrue(!(asCStringPointer("Beta", 4) < ExternalPointer),
				TEXT("Pointer ordering should preserve lexical order")));
			ASSERT_THAT(IsTrue(std::strcmp(ObjectPointer.AddressOf(), "Alpha") == 0,
				TEXT("Object pointer view should expose the owned text")));
		}

		ASSERT_THAT(IsTrue(
			std::strcmp(ExternalText, "Alpha") == 0,
			TEXT("Releasing owned strings and pointer views should not mutate the external buffer")));
		ASSERT_THAT(IsTrue(
			ExternalControl.Equals("Alpha"),
			TEXT("Releasing the owned string graph should not mutate an independent control copy")));
		const asCString EmptyBaseline;
		ASSERT_THAT(AreEqual(
			0,
			static_cast<int32>(EmptyBaseline.GetLength()),
			TEXT("A fresh string should retain the empty baseline after owned graph cleanup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
