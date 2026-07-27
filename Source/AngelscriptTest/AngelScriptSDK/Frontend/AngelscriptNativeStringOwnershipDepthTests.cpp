#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "StartAngelscriptHeaders.h"
#include "source/as_string.h"
#include "source/as_string_util.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FStringOwnershipDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.StringOwnershipDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FStringState
	{
		const TCHAR* Id;
		const char* Value;
		size_t Length;
	};

	struct FScanCase
	{
		const TCHAR* Id;
		const char* Input;
		double ExpectedValue;
		int32 ExpectedConsumed;
	};

	static FString DescribeBytes(const FStringState& State)
	{
		FString Result;
		for (size_t Index = 0; Index < State.Length; ++Index)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(" ");
			}

			Result += FString::Printf(
				TEXT("%02X"),
				static_cast<uint32>(static_cast<uint8>(static_cast<unsigned char>(State.Value[Index]))));
		}

		return Result.IsEmpty() ? TEXT("<empty>") : Result;
	}

	static bool HasSameBytes(const asCString& Value, const FStringState& State)
	{
		return Value.GetLength() == State.Length
			&& std::memcmp(Value.AddressOf(), State.Value, State.Length) == 0
			&& Value.AddressOf()[State.Length] == '\0';
	}

public:
	TEST_METHOD(CopyMoveAndAliasOwnershipBySourceState)
	{
		AS_NATIVE_PRODUCT("FRONTEND-STRING-OWNERSHIP-DEPTH",
			ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const char EmbeddedNullValue[] =
		{
			'A',
			'l',
			'p',
			'h',
			'a',
			'\0',
			'T',
			'a',
			'i',
			'l',
		};
		const char LongValue[] = "AlphaBetaGammaDeltaEpsilon";
		const FStringState States[] =
		{
			{ TEXT("empty"), "", 0 },
			{ TEXT("short"), "Alpha", 5 },
			{ TEXT("long"), LongValue, sizeof(LongValue) - 1 },
			{ TEXT("embedded_null"), EmbeddedNullValue, UE_ARRAY_COUNT(EmbeddedNullValue) },
		};

		int32 ObservedStates = 0;
		for (const FStringState& State : States)
		{
			const FString CaseId = MakeNativeCaseId(
				"FRONTEND-STRING-OWNERSHIP-DEPTH",
				{ State.Id });
			const FString ModuleName = FString::Printf(TEXT("StringOwnershipDepth_%s"), State.Id);
			FString ReviewSource;
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// CaseId: %s"), *CaseId));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// asCString source state: %s"), State.Id));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// source byte length: %d"), static_cast<int32>(State.Length)));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// source bytes: %s"), *DescribeBytes(State)));
			AppendGeneratedAsLine(ReviewSource, TEXT("// exercised: copy construction, rvalue construction, copy assignment, rvalue assignment, self-assignment"));
			AppendGeneratedAsLine(ReviewSource, TEXT("// exercised: object-backed and external-range asCStringPointer aliases, including length observation"));
			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, ReviewSource);

			asCString Source(State.Value, State.Length);
			asCString CopyConstructed(Source);
			asCString CopyAssigned("copy-assignment-seed");
			CopyAssigned = Source;

			ASSERT_THAT(IsTrue(HasSameBytes(Source, State),
				FString::Printf(TEXT("%s should retain the original bytes and terminator"), *CaseId)));
			ASSERT_THAT(IsTrue(HasSameBytes(CopyConstructed, State),
				FString::Printf(TEXT("%s copy construction should preserve explicit length"), *CaseId)));
			ASSERT_THAT(IsTrue(HasSameBytes(CopyAssigned, State),
				FString::Printf(TEXT("%s copy assignment should preserve explicit length"), *CaseId)));
			ASSERT_THAT(IsTrue(CopyConstructed.AddressOf() != Source.AddressOf(),
				FString::Printf(TEXT("%s copy construction should own independent storage"), *CaseId)));
			ASSERT_THAT(IsTrue(CopyAssigned.AddressOf() != Source.AddressOf(),
				FString::Printf(TEXT("%s copy assignment should own independent storage"), *CaseId)));

			// The fork keeps the C++11 move declarations commented out in as_string.h/cpp.
			// std::move therefore selects the public const-copy overload. Keep this
			// explicit fork behavior covered until a selective 2.38 move API is adopted.
			asCString RvalueSource(Source);
			asCString RvalueConstructed(std::move(RvalueSource));
			asCString RvalueAssignmentSource(Source);
			asCString RvalueAssigned("rvalue-assignment-seed");
			RvalueAssigned = std::move(RvalueAssignmentSource);
			ASSERT_THAT(IsTrue(HasSameBytes(RvalueConstructed, State),
				FString::Printf(TEXT("%s rvalue construction should use the fork's copy fallback"), *CaseId)));
			ASSERT_THAT(IsTrue(HasSameBytes(RvalueSource, State),
				FString::Printf(TEXT("%s rvalue construction should leave the source unchanged in this fork"), *CaseId)));
			ASSERT_THAT(IsTrue(HasSameBytes(RvalueAssigned, State),
				FString::Printf(TEXT("%s rvalue assignment should use the fork's copy fallback"), *CaseId)));
			ASSERT_THAT(IsTrue(HasSameBytes(RvalueAssignmentSource, State),
				FString::Printf(TEXT("%s rvalue assignment should leave the source unchanged in this fork"), *CaseId)));

			asCString SelfAssigned(Source);
			SelfAssigned = SelfAssigned;
			ASSERT_THAT(IsTrue(HasSameBytes(SelfAssigned, State),
				FString::Printf(TEXT("%s self-assignment should preserve bytes and length"), *CaseId)));

			asCStringPointer ObjectAlias(&Source);
			asCStringPointer RangeAlias(Source.AddressOf(), Source.GetLength());
			ASSERT_THAT(AreEqual(static_cast<int32>(State.Length), static_cast<int32>(ObjectAlias.GetLength()),
				FString::Printf(TEXT("%s object alias should expose the source length"), *CaseId)));
			ASSERT_THAT(AreEqual(static_cast<int32>(State.Length), static_cast<int32>(RangeAlias.GetLength()),
				FString::Printf(TEXT("%s external range alias should expose its explicit length"), *CaseId)));
			ASSERT_THAT(IsTrue(ObjectAlias == RangeAlias,
				FString::Printf(TEXT("%s object and range aliases should compare by bytes"), *CaseId)));

			TArray<char> ExternalBytes;
			ExternalBytes.SetNum(static_cast<int32>(State.Length + 1));
			if (State.Length > 0)
			{
				FMemory::Memcpy(ExternalBytes.GetData(), State.Value, State.Length);
			}
			ExternalBytes[State.Length] = '\0';
			asCString OwnedFromExternal(ExternalBytes.GetData(), State.Length);
			asCStringPointer ExternalAlias(ExternalBytes.GetData(), State.Length);
			ASSERT_THAT(IsTrue(HasSameBytes(OwnedFromExternal, State),
				FString::Printf(TEXT("%s asCString should copy an external range"), *CaseId)));

			if (State.Length > 0)
			{
				const char Replacement = Source[0] == 'Z' ? 'Y' : 'Z';
				Source[0] = Replacement;
				ExternalBytes[0] = Replacement;
				ASSERT_THAT(IsTrue(ObjectAlias.AddressOf()[0] == Replacement,
					FString::Printf(TEXT("%s object alias should follow source mutation"), *CaseId)));
				ASSERT_THAT(IsTrue(RangeAlias.AddressOf()[0] == Replacement,
					FString::Printf(TEXT("%s range alias should follow in-place source mutation"), *CaseId)));
				ASSERT_THAT(IsTrue(ExternalAlias.AddressOf()[0] == Replacement,
					FString::Printf(TEXT("%s external alias should follow external-range mutation"), *CaseId)));
				ASSERT_THAT(IsTrue(CopyConstructed.AddressOf()[0] == State.Value[0],
					FString::Printf(TEXT("%s copy construction should not alias source mutation"), *CaseId)));
				ASSERT_THAT(IsTrue(CopyAssigned.AddressOf()[0] == State.Value[0],
					FString::Printf(TEXT("%s copy assignment should not alias source mutation"), *CaseId)));
				ASSERT_THAT(IsTrue(OwnedFromExternal.AddressOf()[0] == State.Value[0],
					FString::Printf(TEXT("%s owned external copy should not alias external mutation"), *CaseId)));
			}

			if (State.Length > 0)
			{
				const size_t OriginalLength = Source.GetLength();
				Source.SetLength(OriginalLength - 1);
				ASSERT_THAT(AreEqual(
					static_cast<int32>(OriginalLength - 1),
					static_cast<int32>(ObjectAlias.GetLength()),
					FString::Printf(TEXT("%s object alias should follow the source length"), *CaseId)));
				ASSERT_THAT(AreEqual(
					static_cast<int32>(State.Length),
					static_cast<int32>(RangeAlias.GetLength()),
					FString::Printf(TEXT("%s external-range alias should retain its captured length"), *CaseId)));
			}

			++ObservedStates;
		}

		ASSERT_THAT(AreEqual(4, ObservedStates,
			TEXT("String ownership product should execute empty, short, long, and embedded-null source states")));

		for (const FStringState& State : States)
		{
			const FString CleanupCaseId = MakeNativeCaseId(
				"FRONTEND-STRING-OWNERSHIP-DEPTH",
				{ State.Id, TEXT("cleanup") });
			TArray<char> ExternalBytes;
			ExternalBytes.SetNum(static_cast<int32>(State.Length + 1));
			if (State.Length > 0)
			{
				FMemory::Memcpy(ExternalBytes.GetData(), State.Value, State.Length);
			}
			ExternalBytes[State.Length] = '\0';
			asCString IndependentControl(State.Value, State.Length);

			{
				asCString Owned(State.Value, State.Length);
				asCString Copy(Owned);
				asCStringPointer OwnedAlias(&Owned);
				asCStringPointer ExternalAlias(ExternalBytes.GetData(), State.Length);
				ASSERT_THAT(IsTrue(OwnedAlias == ExternalAlias,
					FString::Printf(TEXT("%s aliases should agree before the owned graph is released"), *CleanupCaseId)));
				if (State.Length > 0)
				{
					Owned[0] = Owned[0] == 'Z' ? 'Y' : 'Z';
					ASSERT_THAT(IsTrue(Copy[0] == State.Value[0],
						FString::Printf(TEXT("%s independent copy should ignore owner mutation"), *CleanupCaseId)));
				}
			}

			ASSERT_THAT(IsTrue(
				std::memcmp(ExternalBytes.GetData(), State.Value, State.Length) == 0
					&& ExternalBytes[State.Length] == '\0',
				FString::Printf(TEXT("%s releasing owned strings should preserve the external buffer"), *CleanupCaseId)));
			ASSERT_THAT(IsTrue(HasSameBytes(IndependentControl, State),
				FString::Printf(TEXT("%s releasing owned strings should preserve an independent control"), *CleanupCaseId)));
			const asCString EmptyBaseline;
			ASSERT_THAT(AreEqual(
				0,
				static_cast<int32>(EmptyBaseline.GetLength()),
				FString::Printf(TEXT("%s fresh string state should remain empty after cleanup"), *CleanupCaseId)));
		}
	}

	TEST_METHOD(NumericScanningBoundariesAndHostPrefixOracle)
	{
		AS_NATIVE_PRODUCT("FRONTEND-STRING-SCAN-BOUNDARIES",
			ENativeEvidence::Runtime
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Isolation);

		const FScanCase Cases[] =
		{
			{ TEXT("empty"), "", 0.0, 0 },
			{ TEXT("negative_sign"), "-42.125", -42.125, 7 },
			{ TEXT("positive_sign"), "+42.125", 42.125, 7 },
			{ TEXT("sign_only"), "+", 0.0, 0 },
			{ TEXT("leading_dot"), ".5", 0.5, 2 },
			{ TEXT("trailing_token"), "1.25xyz", 1.25, 4 },
			{ TEXT("malformed_exponent"), "1e", 1.0, 1 },
			{ TEXT("malformed_exponent_plus"), "1e+", 1.0, 1 },
			{ TEXT("malformed_exponent_minus"), "2.5e-", 2.5, 3 },
			{ TEXT("valid_exponent"), "2.5e-2", 0.025, 6 },
			{ TEXT("missing_mantissa"), "e10", 0.0, 0 },
			{ TEXT("fraction_trailing"), "-.75tail", -0.75, 4 },
		};

		int32 ObservedCases = 0;
		for (const FScanCase& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"FRONTEND-STRING-SCAN-BOUNDARIES",
				{ Case.Id });
			const FString ModuleName = FString::Printf(TEXT("StringScanBoundaries_%s"), Case.Id);
			FString ReviewSource;
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// CaseId: %s"), *CaseId));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// native input: \"%s\""), UTF8_TO_TCHAR(Case.Input)));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// expected numeric value: %.17g"), Case.ExpectedValue));
			AppendGeneratedAsLine(ReviewSource, FString::Printf(TEXT("// expected standard-parser prefix length: %d"), Case.ExpectedConsumed));
			AppendGeneratedAsLine(ReviewSource, TEXT("// fork API: asStringScanDouble(const char*) and asStringScanFloat(const char*)"));
			AppendGeneratedAsLine(ReviewSource, TEXT("// note: the fork API does not expose an end pointer; std::strtod supplies the observable prefix-length oracle"));
			PrintGeneratedAsSource(*TestRunner, CaseId, ModuleName, ReviewSource);

			char* HostEnd = nullptr;
			const double HostValue = std::strtod(Case.Input, &HostEnd);
			const int32 HostConsumed = HostEnd != nullptr
				? static_cast<int32>(HostEnd - Case.Input)
				: -1;
			ASSERT_THAT(AreEqual(Case.ExpectedConsumed, HostConsumed,
				FString::Printf(TEXT("%s standard parser should consume the expected numeric prefix"), *CaseId)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Case.ExpectedValue, HostValue),
				FString::Printf(TEXT("%s standard parser should produce the expected boundary value"), *CaseId)));

			const double ScannedDouble = asStringScanDouble(Case.Input);
			const float ScannedFloat = asStringScanFloat(Case.Input);
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(Case.ExpectedValue, ScannedDouble),
				FString::Printf(TEXT("%s double scanner should preserve result boundaries"), *CaseId)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(static_cast<float>(Case.ExpectedValue), ScannedFloat),
				FString::Printf(TEXT("%s float scanner should preserve result boundaries"), *CaseId)));
			ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(static_cast<float>(HostValue), ScannedFloat),
				FString::Printf(TEXT("%s float scanner should agree with the host prefix oracle"), *CaseId)));
			++ObservedCases;
		}

		ASSERT_THAT(AreEqual(12, ObservedCases,
			TEXT("String scanning product should execute empty, sign, leading-dot, trailing-token, and exponent cases")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
