#include "Core/AngelscriptDelegateWithPayload.h"
#include "Core/AngelscriptEngine.h"
#include "AngelscriptNativeScriptTestObject.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptDelegateWithPayloadTests,
	"Angelscript.TestModule.Engine.DelegateWithPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static TStrongObjectPtr<UAngelscriptNativeScriptTestObject> CreateDelegateWithPayloadReceiver(FAutomationTestBase& Test)
{
	FNoDiscardAsserter LocalAssert(Test);
	TStrongObjectPtr<UAngelscriptNativeScriptTestObject> Receiver(
		NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("DelegateWithPayloadRuntimeReceiver")));
	if (!LocalAssert.IsNotNull(Receiver.Get(), TEXT("DelegateWithPayload runtime test should create a transient receiver object")))
	{
		return nullptr;
	}

	Receiver->bNativeFlag = false;
	Receiver->PreciseValue = -1.0;
	Receiver->LargeCount = 0;
	return Receiver;
}

static bool ExpectClearedDelegateState(FAutomationTestBase& Test, const FAngelscriptDelegateWithPayload& Delegate)
{
	FNoDiscardAsserter LocalAssert(Test);
	const bool bPayloadCleared = LocalAssert.IsFalse(
		Delegate.Payload.IsValid(),
		TEXT("DelegateWithPayload Reset should clear the instanced payload"));
	const bool bObjectCleared = LocalAssert.IsFalse(
		Delegate.Object.IsValid(),
		TEXT("DelegateWithPayload Reset should clear the target object weak pointer"));
	const bool bFunctionCleared = LocalAssert.IsTrue(
		Delegate.FunctionName.IsNone(),
		TEXT("DelegateWithPayload Reset should clear the bound function name"));
	return bPayloadCleared && bObjectCleared && bFunctionCleared;
}

static bool ExpectBoxedFloatPayload(
	FAutomationTestBase& Test,
	const FAngelscriptDelegateWithPayload& Delegate,
	const float ExpectedValue)
{
	FNoDiscardAsserter LocalAssert(Test);
	if (!LocalAssert.IsTrue(
			Delegate.Payload.IsValid(),
			TEXT("DelegateWithPayload float bind should store an instanced payload")))
	{
		return false;
	}

	if (!LocalAssert.IsTrue(
			Delegate.Payload.GetScriptStruct() == FAngelscriptBoxedFloat::StaticStruct(),
			TEXT("DelegateWithPayload float bind should use the boxed-float helper struct")))
	{
		return false;
	}

	const FAngelscriptBoxedFloat* BoxedFloat = reinterpret_cast<const FAngelscriptBoxedFloat*>(Delegate.Payload.GetMemory());
	if (!LocalAssert.IsNotNull(BoxedFloat, TEXT("DelegateWithPayload float bind should expose boxed payload memory")))
	{
		return false;
	}

	return LocalAssert.IsNear(
		ExpectedValue,
		BoxedFloat->Value,
		KINDA_SMALL_NUMBER,
		TEXT("DelegateWithPayload float bind should preserve the boxed primitive value"));
}

public:
	TEST_METHOD(BindExecuteAndResetPrimitivePayloads)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		TStrongObjectPtr<UAngelscriptNativeScriptTestObject> Receiver = CreateDelegateWithPayloadReceiver(*TestRunner);
		if (!Receiver.IsValid())
		{
			return;
		}

		FAngelscriptDelegateWithPayload Delegate;
		ASSERT_THAT(IsFalse(Delegate.IsBound(), TEXT("DelegateWithPayload should begin unbound")));

		Delegate.ExecuteIfBound();
		ASSERT_THAT(IsFalse(Receiver->bNativeFlag, TEXT("Unbound DelegateWithPayload ExecuteIfBound should not toggle the receiver flag")));

		const FName NoPayloadFunctionName = GET_FUNCTION_NAME_CHECKED(UAngelscriptNativeScriptTestObject, MarkNativeFlagFromDelegate);
		Delegate.BindUFunction(Receiver.Get(), NoPayloadFunctionName);
		ASSERT_THAT(IsTrue(Delegate.IsBound(), TEXT("BindUFunction should mark DelegateWithPayload as bound")));

		ASSERT_THAT(IsTrue(Delegate.Object.Get() == Receiver.Get(), TEXT("BindUFunction should store the receiver object")));

		ASSERT_THAT(AreEqual(NoPayloadFunctionName, Delegate.FunctionName, TEXT("BindUFunction should store the receiver function name")));

		ASSERT_THAT(IsFalse(Delegate.Payload.IsValid(), TEXT("BindUFunction should not retain a payload")));

		Delegate.ExecuteIfBound();
		ASSERT_THAT(IsTrue(Receiver->bNativeFlag, TEXT("BindUFunction ExecuteIfBound should invoke the no-payload receiver function")));

		const int32 FloatTypeId = asTYPEID_FLOAT32;

		ASSERT_THAT(IsTrue(
			FAngelscriptDelegateWithPayload::GetBoxedPrimitiveStructFromTypeId(FloatTypeId) == FAngelscriptBoxedFloat::StaticStruct(),
			TEXT("GetBoxedPrimitiveStructFromTypeId should map float to the boxed-float helper")));

		const float PayloadValue = 3.25f;
		const FName PayloadFunctionName = GET_FUNCTION_NAME_CHECKED(UAngelscriptNativeScriptTestObject, SetPreciseValueFromDelegate);
		Delegate.BindUFunctionWithPayload(Receiver.Get(), PayloadFunctionName, (void*)&PayloadValue, FloatTypeId);
		ASSERT_THAT(IsTrue(Delegate.IsBound(), TEXT("BindUFunctionWithPayload should keep DelegateWithPayload bound")));

		ASSERT_THAT(AreEqual(PayloadFunctionName, Delegate.FunctionName, TEXT("BindUFunctionWithPayload should replace the stored function name")));

		if (!ExpectBoxedFloatPayload(*TestRunner, Delegate, PayloadValue))
		{
			return;
		}

		Delegate.ExecuteIfBound();
		ASSERT_THAT(IsNear(
			PayloadValue,
			static_cast<float>(Receiver->PreciseValue),
			KINDA_SMALL_NUMBER,
			TEXT("BindUFunctionWithPayload ExecuteIfBound should forward the boxed float to the receiver")));

		Receiver->bNativeFlag = false;
		Delegate.BindUFunction(Receiver.Get(), NoPayloadFunctionName);
		ASSERT_THAT(IsFalse(
			Delegate.Payload.IsValid(),
			TEXT("BindUFunction should clear any previously boxed payload")));

		Delegate.ExecuteIfBound();
		ASSERT_THAT(IsTrue(
			Receiver->bNativeFlag,
			TEXT("BindUFunction should still execute correctly after rebinding from a payload delegate")));

		Receiver->bNativeFlag = false;
		Receiver->PreciseValue = 9.5;
		Delegate.Reset();
		ASSERT_THAT(IsFalse(Delegate.IsBound(), TEXT("Reset should leave DelegateWithPayload unbound")));

		if (!ExpectClearedDelegateState(*TestRunner, Delegate))
		{
			return;
		}

		Delegate.ExecuteIfBound();
		ASSERT_THAT(IsFalse(Receiver->bNativeFlag, TEXT("Reset delegate should no longer call the no-payload receiver function")));
		ASSERT_THAT(IsNear(
			9.5f,
			static_cast<float>(Receiver->PreciseValue),
			KINDA_SMALL_NUMBER,
			TEXT("Reset delegate should become a no-op for payload execution")));
		}
	}
};

#endif
