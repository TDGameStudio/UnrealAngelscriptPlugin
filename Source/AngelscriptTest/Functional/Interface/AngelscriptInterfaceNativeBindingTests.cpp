#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Functional/Interface/AngelscriptInterfaceTestAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

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

		TestRunner->TestNotNull(
			TEXT("Interface signature lifecycle test should allocate a signature record for GetNativeValue"),
			NativeValueSignature);
		TestRunner->TestNotNull(
			TEXT("Interface signature lifecycle test should allocate a signature record for SetNativeMarker"),
			NativeMarkerSignature);
		if (NativeValueSignature != nullptr)
		{
			TestRunner->TestEqual(
				TEXT("Interface signature lifecycle test should preserve the registered function name for GetNativeValue"),
				NativeValueSignature->FunctionName,
				FName(TEXT("GetNativeValue")));
		}
		if (NativeMarkerSignature != nullptr)
		{
			TestRunner->TestEqual(
				TEXT("Interface signature lifecycle test should preserve the registered function name for SetNativeMarker"),
				NativeMarkerSignature->FunctionName,
				FName(TEXT("SetNativeMarker")));
		}
		TestRunner->TestTrue(
			TEXT("Interface signature lifecycle test should return distinct records for separate registrations"),
			NativeValueSignature != nullptr && NativeMarkerSignature != nullptr && NativeValueSignature != NativeMarkerSignature);
		TestRunner->TestEqual(
			TEXT("Interface signature lifecycle test should increase the signature count by two after two registrations"),
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			BaselineSignatureCount + 2);

		Engine.ReleaseInterfaceMethodSignature(NativeValueSignature);
		TestRunner->TestEqual(
			TEXT("Interface signature lifecycle test should decrease the signature count by one after releasing the first signature"),
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			BaselineSignatureCount + 1);

		Engine.ReleaseInterfaceMethodSignature(nullptr);
		TestRunner->TestEqual(
			TEXT("Interface signature lifecycle test should treat nullptr release as a no-op"),
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			BaselineSignatureCount + 1);

		Engine.ReleaseInterfaceMethodSignature(NativeMarkerSignature);
		TestRunner->TestEqual(
			TEXT("Interface signature lifecycle test should restore the baseline count after releasing the second signature"),
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			BaselineSignatureCount);
	}
};

#endif
