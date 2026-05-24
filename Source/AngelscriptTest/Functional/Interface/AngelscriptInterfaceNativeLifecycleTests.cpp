#include "CQTest.h"
#include "Core/AngelscriptEngine.h"
#include "Functional/Interface/AngelscriptInterfaceTestAccess.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;


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

		if (!TestRunner->TestNotNull(
				TEXT("Interface.Native.SignatureRegistrationRelease should allocate the first interface signature"),
				FirstSignature)
			|| !TestRunner->TestNotNull(
				TEXT("Interface.Native.SignatureRegistrationRelease should allocate the second interface signature"),
				SecondSignature))
		{
			return;
		}

		if (!TestRunner->TestTrue(
				TEXT("Interface.Native.SignatureRegistrationRelease should return distinct records for distinct registrations"),
				FirstSignature != SecondSignature))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should grow by 2 entries after two registrations"),
				FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
				BaselineCount + 2))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should keep the first registered function name at index 0"),
				FirstSignature->FunctionName,
				FName(TEXT("GetNativeValue"))))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should preserve the second signature function name"),
				SecondSignature->FunctionName,
				FName(TEXT("SetNativeMarker"))))
		{
			return;
		}

		Engine.ReleaseInterfaceMethodSignature(FirstSignature);
		FirstSignature = nullptr;

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should shrink by 1 entry after releasing the first signature"),
				FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
				BaselineCount + 1))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should keep the second signature function name intact after releasing the first"),
				SecondSignature->FunctionName,
				FName(TEXT("SetNativeMarker"))))
		{
			return;
		}

		Engine.ReleaseInterfaceMethodSignature(nullptr);

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should keep the count unchanged when releasing nullptr"),
				FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
				BaselineCount + 1))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Interface.Native.SignatureRegistrationRelease should preserve the remaining signature function name after the nullptr guard path"),
				SecondSignature->FunctionName,
				FName(TEXT("SetNativeMarker"))))
		{
			return;
		}

		Engine.ReleaseInterfaceMethodSignature(SecondSignature);
		SecondSignature = nullptr;

		TestRunner->TestEqual(
			TEXT("Interface.Native.SignatureRegistrationRelease should shrink back to baseline after releasing the final signature"),
			FAngelscriptInterfaceSignatureTestAccess::GetSignatureCount(Engine),
			BaselineCount);
	}
};

#endif
