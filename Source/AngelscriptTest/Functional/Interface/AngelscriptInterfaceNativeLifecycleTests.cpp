#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Functional/Interface/AngelscriptInterfaceTestAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceNativeLifecycleTests, "Angelscript.TestModule.Interface.NativeLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(SignatureRegistrationRelease)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FInterfaceMethodSignature* FirstSignature = nullptr;
		FInterfaceMethodSignature* SecondSignature = nullptr;
		ON_SCOPE_EXIT
		{
			Engine.ReleaseInterfaceMethodSignature(FirstSignature);
			Engine.ReleaseInterfaceMethodSignature(SecondSignature);
		};

		// Use the current count as baseline instead of assuming zero — prior tests
		// in a batch run may leave interface signatures in the shared engine.
		const int32 BaselineCount = FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine);

		FirstSignature = Engine.RegisterInterfaceMethodSignature(FName(TEXT("GetNativeValue")));
		SecondSignature = Engine.RegisterInterfaceMethodSignature(FName(TEXT("SetNativeMarker")));

		ASSERT_THAT(IsNotNull(
			FirstSignature,
			TEXT("Interface.Native.SignatureRegistrationRelease should allocate the first interface signature")));
		ASSERT_THAT(IsNotNull(
			SecondSignature,
			TEXT("Interface.Native.SignatureRegistrationRelease should allocate the second interface signature")));

		ASSERT_THAT(IsTrue(
			FirstSignature != SecondSignature,
			TEXT("Interface.Native.SignatureRegistrationRelease should return distinct records for distinct registrations")));

		ASSERT_THAT(AreEqual(
			BaselineCount + 2,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface.Native.SignatureRegistrationRelease should grow by 2 entries after two registrations")));

		ASSERT_THAT(AreEqual(
			FName(TEXT("GetNativeValue")),
			FirstSignature->FunctionName,
			TEXT("Interface.Native.SignatureRegistrationRelease should keep the first registered function name at index 0")));

		ASSERT_THAT(AreEqual(
			FName(TEXT("SetNativeMarker")),
			SecondSignature->FunctionName,
			TEXT("Interface.Native.SignatureRegistrationRelease should preserve the second signature function name")));

		Engine.ReleaseInterfaceMethodSignature(FirstSignature);
		FirstSignature = nullptr;

		ASSERT_THAT(AreEqual(
			BaselineCount + 1,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface.Native.SignatureRegistrationRelease should shrink by 1 entry after releasing the first signature")));

		ASSERT_THAT(AreEqual(
			FName(TEXT("SetNativeMarker")),
			SecondSignature->FunctionName,
			TEXT("Interface.Native.SignatureRegistrationRelease should keep the second signature function name intact after releasing the first")));

		Engine.ReleaseInterfaceMethodSignature(nullptr);

		ASSERT_THAT(AreEqual(
			BaselineCount + 1,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface.Native.SignatureRegistrationRelease should keep the count unchanged when releasing nullptr")));

		ASSERT_THAT(AreEqual(
			FName(TEXT("SetNativeMarker")),
			SecondSignature->FunctionName,
			TEXT("Interface.Native.SignatureRegistrationRelease should preserve the remaining signature function name after the nullptr guard path")));

		Engine.ReleaseInterfaceMethodSignature(SecondSignature);
		SecondSignature = nullptr;

		ASSERT_THAT(AreEqual(
			BaselineCount,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface.Native.SignatureRegistrationRelease should shrink back to baseline after releasing the final signature")));
	}
};

#endif
