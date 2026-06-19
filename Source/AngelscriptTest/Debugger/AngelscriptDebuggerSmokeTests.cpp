#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptDebuggerSmokeTests_Private
{
	class FScopedDebugBreakFiltersBinding
	{
	public:
		FScopedDebugBreakFiltersBinding(FAngelscriptEngine& Engine, TFunction<void(FAngelscriptDebugBreakFilters&)> InPopulateFilters)
			: TargetDelegate(Engine.GetDebugBreakFilters())
			, PreviousDelegate(TargetDelegate)
			, PopulateFilters(MoveTemp(InPopulateFilters))
		{
			TargetDelegate.BindLambda([this](FAngelscriptDebugBreakFilters& OutFilters)
			{
				PopulateFilters(OutFilters);
			});
		}

		~FScopedDebugBreakFiltersBinding()
		{
			TargetDelegate = MoveTemp(PreviousDelegate);
		}

	private:
		FAngelscriptGetDebugBreakFilters& TargetDelegate;
		FAngelscriptGetDebugBreakFilters PreviousDelegate;
		TFunction<void(FAngelscriptDebugBreakFilters&)> PopulateFilters;
	};

	bool WaitForDebuggerEnvelopeType(
		FAutomationTestBase& Test,
		FAngelscriptDebuggerTestSession& Session,
		FAngelscriptDebuggerTestClient& Client,
		EDebugMessageType ExpectedType,
		TOptional<FAngelscriptDebugMessageEnvelope>& OutEnvelope,
		const TCHAR* Context)
	{
		FNoDiscardAsserter Assert(Test);
		const bool bReceivedEnvelope = Session.PumpUntil(
			[&Client, &OutEnvelope, ExpectedType]()
			{
				if (OutEnvelope.IsSet())
				{
					return true;
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Client.ReceiveEnvelope();
				if (Envelope.IsSet() && Envelope->MessageType == ExpectedType)
				{
					OutEnvelope = MoveTemp(Envelope);
					return true;
				}

				return false;
			},
			Session.GetDefaultTimeoutSeconds());

		if (!Assert.IsTrue(bReceivedEnvelope, Context))
		{
			if (!Client.GetLastError().IsEmpty())
			{
				Test.AddError(Client.GetLastError());
			}
			return false;
		}

		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDebuggerSmokeTests,
	"Angelscript.TestModule.Debugger.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FDebuggerTestContext Ctx;

	BEFORE_EACH()
	{
		ASSERT_THAT(IsTrue(Ctx.SetUp(*TestRunner)));
	}

	AFTER_EACH()
	{
		Ctx.TearDown();
	}

	TEST_METHOD(Handshake)
	{
		ASSERT_THAT(IsTrue(
			Ctx.GetDebugServer().bIsDebugging,
			TEXT("Debugger.Smoke.Handshake should put the session in debugging mode after StartDebugging")));

		ASSERT_THAT(IsTrue(
			Ctx.Client.SendRequestBreakFilters(),
			TEXT("Debugger.Smoke.Handshake should request debugger break filters")));

		TOptional<FAngelscriptDebugMessageEnvelope> BreakFiltersEnvelope;
		const bool bReceivedBreakFilters = Ctx.Session.PumpUntil(
			[this, &BreakFiltersEnvelope]()
			{
				if (BreakFiltersEnvelope.IsSet())
				{
					return true;
				}

				TOptional<FAngelscriptDebugMessageEnvelope> Envelope = Ctx.Client.ReceiveEnvelope();
				if (Envelope.IsSet() && Envelope->MessageType == EDebugMessageType::BreakFilters)
				{
					BreakFiltersEnvelope = MoveTemp(Envelope);
					return true;
				}

				return false;
			},
			Ctx.GetDefaultTimeoutSeconds());

		ASSERT_THAT(IsTrue(
			bReceivedBreakFilters,
			TEXT("Debugger.Smoke.Handshake should receive a BreakFilters response")));

		const TOptional<FAngelscriptBreakFilters> BreakFilters =
			FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptBreakFilters>(BreakFiltersEnvelope.GetValue());
		ASSERT_THAT(IsTrue(
			BreakFilters.IsSet(),
			TEXT("Debugger.Smoke.Handshake should deserialize the BreakFilters payload")));

		ASSERT_THAT(IsTrue(
			Ctx.Client.SendStopDebugging(),
			TEXT("Debugger.Smoke.Handshake should send StopDebugging")));

		const bool bStoppedDebugging = Ctx.Session.PumpUntil(
			[this]() { return !Ctx.GetDebugServer().bIsDebugging; },
			Ctx.GetDefaultTimeoutSeconds());
		ASSERT_THAT(IsTrue(
			bStoppedDebugging,
			TEXT("Debugger.Smoke.Handshake should leave debugging mode after StopDebugging")));
	}

	TEST_METHOD(BreakFiltersRoundtrip)
	{
		using namespace AngelscriptDebuggerSmokeTests_Private;

		FScopedDebugBreakFiltersBinding ScopedBinding(
			Ctx.GetEngine(),
			[](FAngelscriptDebugBreakFilters& OutFilters)
			{
				OutFilters.Add(FName(TEXT("break:ensure")), TEXT("Ensure"));
				OutFilters.Add(FName(TEXT("break:script")), TEXT("Script"));
			});

		ASSERT_THAT(IsTrue(
			Ctx.GetDebugServer().bIsDebugging,
			TEXT("Debugger smoke protocol should enter debugging mode after StartDebugging")));

		ASSERT_THAT(IsTrue(
			Ctx.Client.SendRequestBreakFilters(),
			TEXT("Debugger smoke protocol should request break filters")));

		TOptional<FAngelscriptDebugMessageEnvelope> BreakFiltersEnvelope;
		ASSERT_THAT(IsTrue(WaitForDebuggerEnvelopeType(
			*TestRunner,
			Ctx.Session,
			Ctx.Client,
			EDebugMessageType::BreakFilters,
			BreakFiltersEnvelope,
			TEXT("Debugger smoke protocol should receive a BreakFilters response"))));

		const TOptional<FAngelscriptBreakFilters> BreakFilters =
			FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptBreakFilters>(BreakFiltersEnvelope.GetValue());
		ASSERT_THAT(IsTrue(
			BreakFilters.IsSet(),
			TEXT("Debugger smoke protocol should deserialize the BreakFilters payload")));

		ASSERT_THAT(IsTrue(
			Ctx.GetDebugServer().bIsDebugging,
			TEXT("Debugger smoke protocol should stay in debugging mode after querying break filters")));
		ASSERT_THAT(AreEqual(2, BreakFilters->Filters.Num(), TEXT("Debugger smoke protocol should report two break filters")));
		ASSERT_THAT(AreEqual(2, BreakFilters->FilterTitles.Num(), TEXT("Debugger smoke protocol should report two filter titles")));

		TMap<FString, FString> ActualPairs;
		for (int32 Index = 0; Index < BreakFilters->Filters.Num() && Index < BreakFilters->FilterTitles.Num(); ++Index)
		{
			ActualPairs.Add(BreakFilters->Filters[Index], BreakFilters->FilterTitles[Index]);
		}

		ASSERT_THAT(AreEqual(2, ActualPairs.Num(), TEXT("Debugger smoke protocol should preserve two unique filter/title pairs")));

		const FString* EnsureTitle = ActualPairs.Find(TEXT("break:ensure"));
		ASSERT_THAT(IsNotNull(EnsureTitle, TEXT("Debugger smoke protocol should include the break:ensure filter")));
		ASSERT_THAT(AreEqual(FString(TEXT("Ensure")), *EnsureTitle, TEXT("Debugger smoke protocol should preserve the break:ensure title")));

		const FString* ScriptTitle = ActualPairs.Find(TEXT("break:script"));
		ASSERT_THAT(IsNotNull(ScriptTitle, TEXT("Debugger smoke protocol should include the break:script filter")));
		ASSERT_THAT(AreEqual(FString(TEXT("Script")), *ScriptTitle, TEXT("Debugger smoke protocol should preserve the break:script title")));
	}
};

#endif
