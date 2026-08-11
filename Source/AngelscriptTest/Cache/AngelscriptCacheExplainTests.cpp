#include "Cache/AngelscriptCacheDiagnostics.h"

#include "CQTest.h"
#include "HAL/IConsoleManager.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheExplainTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Bytes); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheExplainTests,
	"Angelscript.TestModule.Cache.Explain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TypedExplainFiltersAndOrdersAlreadyCapturedDecisionDtos)
	{
		using namespace AngelscriptCacheExplainTests_Private;
		const FAngelscriptStableModuleKey ModuleA{MakeHash(1)};
		const FAngelscriptStableModuleKey ModuleB{MakeHash(33)};
		const FAngelscriptStableFunctionKey FunctionA{MakeHash(65)};

		FAngelscriptCacheDecisionEvent Publication;
		Publication.EventOrdinal = 1;
		Publication.TransactionOrdinal = 11;
		Publication.Stage =
			EAngelscriptCacheDecisionStage::SuccessfulPublication;
		Publication.Outcome = EAngelscriptCacheDecisionOutcome::Published;
		Publication.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::FreezePublication;
		Publication.ModuleKeys.Add(ModuleA);

		FAngelscriptCacheDecisionEvent Miss;
		Miss.EventOrdinal = 2;
		Miss.TransactionOrdinal = 11;
		Miss.Stage = EAngelscriptCacheDecisionStage::FunctionLookup;
		Miss.Outcome = EAngelscriptCacheDecisionOutcome::Miss;
		Miss.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::FunctionLookup;
		Miss.ReasonCode = 4;
		Miss.ModuleKeys.Add(ModuleA);
		Miss.FunctionKey = FunctionA;
		Miss.ExpectedCoordinate = MakeHash(97);
		Miss.CurrentCoordinate = MakeHash(129);

		FAngelscriptCacheDecisionEvent Flush;
		Flush.EventOrdinal = 3;
		Flush.TransactionOrdinal = 12;
		Flush.Stage = EAngelscriptCacheDecisionStage::LifecycleFlush;
		Flush.Outcome = EAngelscriptCacheDecisionOutcome::Completed;
		Flush.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::LifecycleFlush;
		Flush.ModuleKeys.Add(ModuleB);

		FAngelscriptCacheDecisionTraceSnapshot Trace;
		Trace.bEnabled = true;
		Trace.Capacity = 8;
		Trace.Events = {Flush, Miss, Publication};

		FAngelscriptCacheExplainRequest Request;
		Request.TransactionOrdinal = 11;
		Request.ModuleKey = ModuleA;
		const FAngelscriptCacheExplainResult Result =
			ExplainAngelscriptCacheDecisions(Trace, Request);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(2, Result.Events.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint64>(1),
			Result.Events[0].EventOrdinal));
		ASSERT_THAT(AreEqual(static_cast<uint64>(2),
			Result.Events[1].EventOrdinal));

		FAngelscriptCacheExplainRequest FunctionRequest;
		FunctionRequest.FunctionKey = FunctionA;
		const FAngelscriptCacheExplainResult FunctionResult =
			ExplainAngelscriptCacheDecisions(Trace, FunctionRequest);
		ASSERT_THAT(IsTrue(FunctionResult.IsSuccess()));
		ASSERT_THAT(AreEqual(1, FunctionResult.Events.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDecisionOutcome::Miss,
			FunctionResult.Events[0].Outcome));

		FString Json;
		ASSERT_THAT(IsTrue(
			SerializeAngelscriptCacheExplainResultJson(FunctionResult, Json)));
		ASSERT_THAT(IsTrue(Json.Contains(TEXT("\"matchedEventCount\":1"))));
		ASSERT_THAT(IsTrue(Json.Contains(
			*FunctionA.Hash.ToHexString())));
		ASSERT_THAT(IsFalse(Json.Contains(TEXT("functionId"))));
		ASSERT_THAT(IsFalse(Json.Contains(TEXT("serviceIdentity"))));
	}

	TEST_METHOD(InvalidAndUnmatchedExplainRequestsAreTyped)
	{
		using namespace AngelscriptCacheExplainTests_Private;
		FAngelscriptCacheDecisionTraceSnapshot Trace;
		FAngelscriptCacheDecisionEvent Event;
		Event.EventOrdinal = 1;
		Event.ModuleKeys.Add(FAngelscriptStableModuleKey{MakeHash(1)});
		Trace.Events.Add(Event);

		const FAngelscriptCacheExplainResult Invalid =
			ExplainAngelscriptCacheDecisions(
				Trace, FAngelscriptCacheExplainRequest{});
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::InvalidRequest,
			Invalid.Error));
		ASSERT_THAT(IsFalse(Invalid.IsSuccess()));

		FAngelscriptCacheExplainRequest MissingRequest;
		MissingRequest.EventOrdinal = 999;
		const FAngelscriptCacheExplainResult Missing =
			ExplainAngelscriptCacheDecisions(Trace, MissingRequest);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::NoMatch,
			Missing.Error));
		ASSERT_THAT(IsFalse(Missing.IsSuccess()));
		ASSERT_THAT(IsFalse(Missing.Detail.IsEmpty()));

		IConsoleObject* ExplainCommand =
			IConsoleManager::Get().FindConsoleObject(TEXT("as.Cache.Explain"));
		ASSERT_THAT(IsNotNull(ExplainCommand));
		ASSERT_THAT(IsNotNull(ExplainCommand->AsCommand()));
	}
};

#endif
