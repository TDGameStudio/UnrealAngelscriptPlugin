#include "CQTest.h"
#include "AngelscriptDebuggerTestContext.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptDebuggerSmokeTests_Private
{
	class FScopedDebugBreakFiltersBinding
	{
	public:
		FScopedDebugBreakFiltersBinding(FAngelscriptEngine& Engine, TFunction<void(FAngelscriptDebugBreakFilters&)> InPopulateFilters)
			: TargetDelegate(Engine.GetHooks().GetDebugBreakFilters())
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

		if (!Test.TestTrue(Context, bReceivedEnvelope))
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
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger.Smoke.Handshake should put the session in debugging mode after StartDebugging"),
			Ctx.GetDebugServer().bIsDebugging)));

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger.Smoke.Handshake should request debugger break filters"),
			Ctx.Client.SendRequestBreakFilters())));

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

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger.Smoke.Handshake should receive a BreakFilters response"),
			bReceivedBreakFilters)));

		const TOptional<FAngelscriptBreakFilters> BreakFilters =
			FAngelscriptDebuggerTestClient::DeserializeMessage<FAngelscriptBreakFilters>(BreakFiltersEnvelope.GetValue());
		TestRunner->TestTrue(
			TEXT("Debugger.Smoke.Handshake should deserialize the BreakFilters payload"),
			BreakFilters.IsSet());

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger.Smoke.Handshake should send StopDebugging"),
			Ctx.Client.SendStopDebugging())));

		const bool bStoppedDebugging = Ctx.Session.PumpUntil(
			[this]() { return !Ctx.GetDebugServer().bIsDebugging; },
			Ctx.GetDefaultTimeoutSeconds());
		TestRunner->TestTrue(
			TEXT("Debugger.Smoke.Handshake should leave debugging mode after StopDebugging"),
			bStoppedDebugging);
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

		TestRunner->TestTrue(
			TEXT("Debugger smoke protocol should enter debugging mode after StartDebugging"),
			Ctx.GetDebugServer().bIsDebugging);

		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger smoke protocol should request break filters"),
			Ctx.Client.SendRequestBreakFilters())));

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
		ASSERT_THAT(IsTrue(TestRunner->TestTrue(
			TEXT("Debugger smoke protocol should deserialize the BreakFilters payload"),
			BreakFilters.IsSet())));

		TestRunner->TestTrue(
			TEXT("Debugger smoke protocol should stay in debugging mode after querying break filters"),
			Ctx.GetDebugServer().bIsDebugging);
		TestRunner->TestEqual(
			TEXT("Debugger smoke protocol should report two break filters"),
			BreakFilters->Filters.Num(), 2);
		TestRunner->TestEqual(
			TEXT("Debugger smoke protocol should report two filter titles"),
			BreakFilters->FilterTitles.Num(), 2);

		TMap<FString, FString> ActualPairs;
		for (int32 Index = 0; Index < BreakFilters->Filters.Num() && Index < BreakFilters->FilterTitles.Num(); ++Index)
		{
			ActualPairs.Add(BreakFilters->Filters[Index], BreakFilters->FilterTitles[Index]);
		}

		TestRunner->TestEqual(
			TEXT("Debugger smoke protocol should preserve two unique filter/title pairs"),
			ActualPairs.Num(), 2);

		const FString* EnsureTitle = ActualPairs.Find(TEXT("break:ensure"));
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(
			TEXT("Debugger smoke protocol should include the break:ensure filter"),
			EnsureTitle)));
		TestRunner->TestEqual(
			TEXT("Debugger smoke protocol should preserve the break:ensure title"),
			*EnsureTitle, FString(TEXT("Ensure")));

		const FString* ScriptTitle = ActualPairs.Find(TEXT("break:script"));
		ASSERT_THAT(IsTrue(TestRunner->TestNotNull(
			TEXT("Debugger smoke protocol should include the break:script filter"),
			ScriptTitle)));
		TestRunner->TestEqual(
			TEXT("Debugger smoke protocol should preserve the break:script title"),
			*ScriptTitle, FString(TEXT("Script")));
	}
};

#endif
