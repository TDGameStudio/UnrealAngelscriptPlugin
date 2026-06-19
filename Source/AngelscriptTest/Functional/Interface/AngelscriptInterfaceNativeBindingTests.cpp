#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Functional/Interface/AngelscriptInterfaceTestAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptInterfaceNativeBindingTests, "Angelscript.TestModule.Interface.NativeBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(SignatureRegistrationLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			ASTEST_RESET_ENGINE(Engine);
		};

		const int32 BaselineSignatureCount = FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine);

		FInterfaceMethodSignature* NativeValueSignature =
			Engine.RegisterInterfaceMethodSignature(FName(TEXT("GetNativeValue")));
		FInterfaceMethodSignature* NativeMarkerSignature =
			Engine.RegisterInterfaceMethodSignature(FName(TEXT("SetNativeMarker")));

		ASSERT_THAT(IsNotNull(
			NativeValueSignature,
			TEXT("Interface signature lifecycle test should allocate a signature record for GetNativeValue")));
		ASSERT_THAT(IsNotNull(
			NativeMarkerSignature,
			TEXT("Interface signature lifecycle test should allocate a signature record for SetNativeMarker")));
		ASSERT_THAT(AreEqual(
			FName(TEXT("GetNativeValue")),
			NativeValueSignature->FunctionName,
			TEXT("Interface signature lifecycle test should preserve the registered function name for GetNativeValue")));
		ASSERT_THAT(AreEqual(
			FName(TEXT("SetNativeMarker")),
			NativeMarkerSignature->FunctionName,
			TEXT("Interface signature lifecycle test should preserve the registered function name for SetNativeMarker")));
		ASSERT_THAT(IsTrue(
			NativeValueSignature != NativeMarkerSignature,
			TEXT("Interface signature lifecycle test should return distinct records for separate registrations")));
		ASSERT_THAT(AreEqual(
			BaselineSignatureCount + 2,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface signature lifecycle test should increase the signature count by two after two registrations")));

		Engine.ReleaseInterfaceMethodSignature(NativeValueSignature);
		ASSERT_THAT(AreEqual(
			BaselineSignatureCount + 1,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface signature lifecycle test should decrease the signature count by one after releasing the first signature")));

		Engine.ReleaseInterfaceMethodSignature(nullptr);
		ASSERT_THAT(AreEqual(
			BaselineSignatureCount + 1,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface signature lifecycle test should treat nullptr release as a no-op")));

		Engine.ReleaseInterfaceMethodSignature(NativeMarkerSignature);
		ASSERT_THAT(AreEqual(
			BaselineSignatureCount,
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			TEXT("Interface signature lifecycle test should restore the baseline count after releasing the second signature")));
	}
};

#endif
