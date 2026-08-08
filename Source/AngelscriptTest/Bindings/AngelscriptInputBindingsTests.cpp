// AngelscriptInputBindingsTests.cpp
// CQTest coverage for FInputActionKeyMapping, FInputBindingHandle, InputEvents.
// Automation IDs: Angelscript.TestModule.Bindings.Input.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Components/SlateWrapperTypes.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptInputBindingsTest,
	"Angelscript.TestModule.Bindings.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}
	AFTER_ALL()
	{
		FAngelscriptEngine& E = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(E);
	}

	TEST_METHOD(FInputActionValue)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASInput_ActionValue"), ASTEST_AS(R"AS(
			int InputActionValue_DefaultZero()
			{
				FInputActionValue V;
				return V.IsNonZero() ? 0 : 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int InputActionValue_DefaultZero()"), TEXT("Default FInputActionValue is zero"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FKeyManualConstructionConversionAndFormatting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int FKeyManualWrappersRoundTrip()
			{
				FKey Constructed(n"SpaceBar");
				FKey Converted = n"SpaceBar";

				return Constructed == EKeys::SpaceBar
					&& Converted == EKeys::SpaceBar
					&& Constructed.GetKeyName() == n"SpaceBar"
					&& Constructed.ToString() == "SpaceBar"
					&& Constructed.IsValid() ? 1 : 0;
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASInput_FKeyManualWrappers"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("FKey manual-wrapper module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExecuteAndExpectInt(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int FKeyManualWrappersRoundTrip()"),
				TEXT("FKey constructor, FName conversion, equality, name, and ToString should dispatch through manual bindings"),
				1),
			TEXT("FKey manual wrappers should preserve the SpaceBar key identity")));
	}

	TEST_METHOD(FInputChordManualConstructors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			FInputChord MakePlainInputChord()
			{
				return FInputChord(EKeys::Enter);
			}

			FInputChord MakeModifiedInputChord()
			{
				return FInputChord(EKeys::LeftMouseButton, true, false, true, true);
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASInput_FInputChordManualConstructors"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("FInputChord manual-constructor module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndValidate<FInputChord>(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("FInputChord MakePlainInputChord()"),
			TEXT("FInputChord single-key constructor should preserve its script argument"),
			[](FAutomationTestBase& Test, const FInputChord& Actual) -> bool
			{
				FNoDiscardAsserter LocalAssert(Test);
				bool bValid = LocalAssert.AreEqual(Actual.Key, EKeys::Enter, TEXT("Plain chord should preserve its key"));
				bValid &= LocalAssert.IsFalse(Actual.bShift, TEXT("Plain chord should not require Shift"));
				bValid &= LocalAssert.IsFalse(Actual.bCtrl, TEXT("Plain chord should not require Ctrl"));
				bValid &= LocalAssert.IsFalse(Actual.bAlt, TEXT("Plain chord should not require Alt"));
				bValid &= LocalAssert.IsFalse(Actual.bCmd, TEXT("Plain chord should not require Cmd"));
				return bValid;
			}), TEXT("FInputChord single-key constructor should initialize native fields")));

		ASSERT_THAT(IsTrue(ExecuteAndValidate<FInputChord>(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("FInputChord MakeModifiedInputChord()"),
			TEXT("FInputChord modifier constructor should preserve all script arguments"),
			[](FAutomationTestBase& Test, const FInputChord& Actual) -> bool
			{
				FNoDiscardAsserter LocalAssert(Test);
				bool bValid = LocalAssert.AreEqual(Actual.Key, EKeys::LeftMouseButton, TEXT("Modified chord should preserve its key"));
				bValid &= LocalAssert.IsTrue(Actual.bShift, TEXT("Modified chord should require Shift"));
				bValid &= LocalAssert.IsFalse(Actual.bCtrl, TEXT("Modified chord should not require Ctrl"));
				bValid &= LocalAssert.IsTrue(Actual.bAlt, TEXT("Modified chord should require Alt"));
				bValid &= LocalAssert.IsTrue(Actual.bCmd, TEXT("Modified chord should require Cmd"));
				return bValid;
			}), TEXT("FInputChord modifier constructor should initialize native fields")));
	}

	TEST_METHOD(FEventReplyFactoryAndFluentMethods)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			FEventReply MakeHandledFluentReply()
			{
				FEventReply Reply = FEventReply::Handled();
				return Reply.ClearUserFocus().ReleaseMouseCapture().ReleaseMouseLock().PreventThrottling();
			}

			FEventReply MakeUnhandledReply()
			{
				return FEventReply::Unhandled();
			}
			)AS");
		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASInput_FEventReplyFactoryAndFluentMethods"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("FEventReply factory and fluent-method module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndValidate<FEventReply>(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("FEventReply MakeHandledFluentReply()"),
			TEXT("FEventReply::Handled and fluent reply methods should keep a handled native reply"),
			[](FAutomationTestBase& Test, const FEventReply& Actual) -> bool
			{
				FNoDiscardAsserter LocalAssert(Test);
				return LocalAssert.IsTrue(Actual.NativeReply.IsEventHandled(), TEXT("Handled reply should remain handled after fluent methods"));
			}), TEXT("Handled FEventReply should cross the script boundary with its native reply state")));

		ASSERT_THAT(IsTrue(ExecuteAndValidate<FEventReply>(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("FEventReply MakeUnhandledReply()"),
			TEXT("FEventReply::Unhandled should create an unhandled native reply"),
			[](FAutomationTestBase& Test, const FEventReply& Actual) -> bool
			{
				FNoDiscardAsserter LocalAssert(Test);
				return LocalAssert.IsFalse(Actual.NativeReply.IsEventHandled(), TEXT("Unhandled reply should remain unhandled"));
			}), TEXT("Unhandled FEventReply should cross the script boundary with its native reply state")));
	}
};

#endif
