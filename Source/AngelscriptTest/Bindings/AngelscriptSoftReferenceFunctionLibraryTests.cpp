// ============================================================================
// AngelscriptSoftReferenceFunctionLibraryTests.cpp
//
// Soft reference async delegate binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.SoftReference.FAngelscriptSoftReferenceFunctionLibraryTest.*
//
// Sections:
//   AsyncDelegates — object/class success/failure async load callbacks
//
// CQTest adaptation notes:
//   Single IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into TEST_CLASS.
//   Async harness pattern preserved with object instantiation and pumped callbacks.
//   Uses `*TestRunner` instead of `this` for assertions.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

namespace
{
	static const FName SoftReferenceAsyncModuleName(TEXT("ASoftReferenceAsyncDelegates"));
	static const FString SoftReferenceAsyncFilename(TEXT("SoftReferenceAsyncDelegates.as"));
	static const FName SoftReferenceAsyncClassName(TEXT("USoftReferenceAsyncScriptHarness"));
	static const FString SuccessTexturePath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	static constexpr double SoftReferenceAsyncTimeoutSeconds = 5.0;

	void PumpSoftReferenceCallbacks()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
		FTSTicker::GetCoreTicker().Tick(0.0f);
		FPlatformProcess::Sleep(0.001f);
	}

	bool WaitUntil(
		FAutomationTestBase& Test,
		TFunctionRef<bool()> Predicate,
		double TimeoutSeconds,
		const TCHAR* FailureContext)
	{
		const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
		while (FPlatformTime::Seconds() < Deadline)
		{
			if (Predicate())
			{
				return true;
			}

			PumpSoftReferenceCallbacks();
		}

		Test.AddError(FString::Printf(TEXT("%s did not complete within %.2f seconds."), FailureContext, TimeoutSeconds));
		return false;
	}

	bool ExecuteGeneratedIntMethod(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		FName FunctionName,
		int32& OutResult)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Soft-reference async method '%s' should exist"), *FunctionName.ToString()),
			Function))
		{
			return false;
		}

		return Test.TestTrue(
			*FString::Printf(TEXT("Soft-reference async method '%s' should execute"), *FunctionName.ToString()),
			ExecuteGeneratedIntEventOnGameThread(&Engine, Object, Function, OutResult));
	}

	// ReadIntPropertyChecked / ReadStringPropertyChecked provided by
	// Shared/AngelscriptFunctionalTestUtils.h

	bool VerifyObjectCallbackSignature(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		FName FunctionName,
		FName ParameterName,
		UClass* ExpectedClass)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Soft-reference callback '%s' should exist"), *FunctionName.ToString()),
			Function))
		{
			return false;
		}

		FObjectProperty* Property = FindFProperty<FObjectProperty>(Function, ParameterName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Soft-reference callback '%s' should expose object parameter '%s'"), *FunctionName.ToString(), *ParameterName.ToString()),
			Property))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("Soft-reference callback '%s' should keep the current UObject delegate surface"), *FunctionName.ToString()),
			Property->PropertyClass.Get(),
			ExpectedClass);
	}

	bool VerifyClassCallbackSignature(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		FName FunctionName,
		FName ParameterName,
		UClass* ExpectedMetaClass)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Soft-reference callback '%s' should exist"), *FunctionName.ToString()),
			Function))
		{
			return false;
		}

		FClassProperty* Property = FindFProperty<FClassProperty>(Function, ParameterName);
		if (!Test.TestNotNull(
			*FString::Printf(TEXT("Soft-reference callback '%s' should expose class parameter '%s'"), *FunctionName.ToString(), *ParameterName.ToString()),
			Property))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("Soft-reference callback '%s' should keep the current UClass delegate surface"), *FunctionName.ToString()),
			Property->MetaClass.Get(),
			ExpectedMetaClass);
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSoftReferenceFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.SoftReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: AsyncDelegates
	// ====================================================================

	TEST_METHOD(AsyncDelegates)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*SoftReferenceAsyncModuleName.ToString());
		};

		const FString SuccessClassPath = AActor::StaticClass()->GetPathName();
		const FString MissingObjectName = FString::Printf(TEXT("MissingTexture_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString MissingClassPackageName = FString::Printf(TEXT("MissingScriptPackage_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString MissingClassName = FString::Printf(TEXT("MissingActor_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString MissingObjectPath = FString::Printf(TEXT("/Engine/EngineResources/%s.%s"), *MissingObjectName, *MissingObjectName);
		const FString MissingClassPath = FString::Printf(TEXT("/Script/%s.%s"), *MissingClassPackageName, *MissingClassName);

		const FString ScriptSource = FString::Printf(
			TEXT(R"AS(
UCLASS()
class USoftReferenceAsyncScriptHarness : UObject
{
	UPROPERTY()
	int ObjectSuccessCallbackCount = 0;
	UPROPERTY()
	int ObjectFailureCallbackCount = 0;
	UPROPERTY()
	int ClassSuccessCallbackCount = 0;
	UPROPERTY()
	int ClassFailureCallbackCount = 0;
	UPROPERTY()
	int bObjectSuccessWasNonNull = 0;
	UPROPERTY()
	int bObjectFailureWasNull = 0;
	UPROPERTY()
	int bClassSuccessWasNonNull = 0;
	UPROPERTY()
	int bClassFailureWasNull = 0;
	UPROPERTY()
	int bObjectPayloadMatchesExpectedType = 0;
	UPROPERTY()
	int bClassPayloadMatchesExpectedType = 0;
	UPROPERTY()
	FString LastObjectName;
	UPROPERTY()
	FString LastClassName;

	UFUNCTION()
	int StartObjectSuccessLoad()
	{
		FOnSoftObjectLoaded Delegate;
		Delegate.BindUFunction(this, n"HandleObjectSuccess");
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath("%s")).LoadAsync(Delegate);
		return 1;
	}

	UFUNCTION()
	int StartObjectFailureLoad()
	{
		FOnSoftObjectLoaded Delegate;
		Delegate.BindUFunction(this, n"HandleObjectFailure");
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath("%s")).LoadAsync(Delegate);
		return 1;
	}

	UFUNCTION()
	int StartClassSuccessLoad()
	{
		FOnSoftClassLoaded Delegate;
		Delegate.BindUFunction(this, n"HandleClassSuccess");
		TSoftClassPtr<AActor>(FSoftObjectPath("%s")).LoadAsync(Delegate);
		return 1;
	}

	UFUNCTION()
	int StartClassFailureLoad()
	{
		FOnSoftClassLoaded Delegate;
		Delegate.BindUFunction(this, n"HandleClassFailure");
		TSoftClassPtr<AActor>(FSoftObjectPath("%s")).LoadAsync(Delegate);
		return 1;
	}

	UFUNCTION()
	void HandleObjectSuccess(UObject LoadedObject)
	{
		UTexture2D TypedTexture = Cast<UTexture2D>(LoadedObject);
		ObjectSuccessCallbackCount += 1;
		bObjectSuccessWasNonNull = LoadedObject != null ? 1 : 0;
		bObjectPayloadMatchesExpectedType = TypedTexture != null ? 1 : 0;
		LastObjectName = TypedTexture == null ? FString() : TypedTexture.GetName().ToString();
	}

	UFUNCTION()
	void HandleObjectFailure(UObject LoadedObject)
	{
		ObjectFailureCallbackCount += 1;
		bObjectFailureWasNull = LoadedObject == null ? 1 : 0;
	}

	UFUNCTION()
	void HandleClassSuccess(UClass LoadedClass)
	{
		ClassSuccessCallbackCount += 1;
		bClassSuccessWasNonNull = LoadedClass != null ? 1 : 0;
		bClassPayloadMatchesExpectedType = LoadedClass != null && LoadedClass.IsChildOf(AActor::StaticClass()) ? 1 : 0;
		LastClassName = LoadedClass == null ? FString() : LoadedClass.GetName().ToString();
	}

	UFUNCTION()
	void HandleClassFailure(UClass LoadedClass)
	{
		ClassFailureCallbackCount += 1;
		bClassFailureWasNull = LoadedClass == null ? 1 : 0;
	}
}
)AS"),
			*SuccessTexturePath,
			*MissingObjectPath,
			*SuccessClassPath,
			*MissingClassPath);

		UClass* ScriptHarnessClass = CompileScriptModule(
			*TestRunner,
			Engine,
			SoftReferenceAsyncModuleName,
			SoftReferenceAsyncFilename,
			ScriptSource,
			SoftReferenceAsyncClassName);
		if (ScriptHarnessClass == nullptr)
		{
			return;
		}

		if (!VerifyObjectCallbackSignature(*TestRunner, ScriptHarnessClass, TEXT("HandleObjectSuccess"), TEXT("LoadedObject"), UObject::StaticClass())
			|| !VerifyObjectCallbackSignature(*TestRunner, ScriptHarnessClass, TEXT("HandleObjectFailure"), TEXT("LoadedObject"), UObject::StaticClass())
			|| !VerifyClassCallbackSignature(*TestRunner, ScriptHarnessClass, TEXT("HandleClassSuccess"), TEXT("LoadedClass"), UObject::StaticClass())
			|| !VerifyClassCallbackSignature(*TestRunner, ScriptHarnessClass, TEXT("HandleClassFailure"), TEXT("LoadedClass"), UObject::StaticClass()))
		{
			return;
		}

		UObject* ScriptHarness = NewObject<UObject>(GetTransientPackage(), ScriptHarnessClass, TEXT("SoftReferenceAsyncHarness"));
		if (!TestRunner->TestNotNull(TEXT("Soft-reference async harness should be created"), ScriptHarness))
		{
			return;
		}

		ScriptHarness->AddToRoot();
		ON_SCOPE_EXIT
		{
			ScriptHarness->RemoveFromRoot();
		};

		auto RunLoadAndWait = [this, &Engine, ScriptHarness, ScriptHarnessClass](
			FName StartFunctionName,
			FName CounterPropertyName,
			const TCHAR* WaitContext) -> bool
		{
			int32 StartResult = 0;
			if (!ExecuteGeneratedIntMethod(*TestRunner, Engine, ScriptHarness, ScriptHarnessClass, StartFunctionName, StartResult))
			{
				return false;
			}

			if (!TestRunner->TestEqual(
				*FString::Printf(TEXT("Soft-reference async starter '%s' should acknowledge launch"), *StartFunctionName.ToString()),
				StartResult,
				1))
			{
				return false;
			}

			return WaitUntil(
				*TestRunner,
				[this, ScriptHarness, CounterPropertyName]()
				{
					int32 CallbackCount = 0;
					return ReadIntPropertyChecked(*TestRunner, ScriptHarness, CounterPropertyName, CallbackCount) && CallbackCount >= 1;
				},
				SoftReferenceAsyncTimeoutSeconds,
				WaitContext);
		};

		if (!RunLoadAndWait(TEXT("StartObjectSuccessLoad"), TEXT("ObjectSuccessCallbackCount"), TEXT("Soft object success callback"))
			|| !RunLoadAndWait(TEXT("StartObjectFailureLoad"), TEXT("ObjectFailureCallbackCount"), TEXT("Soft object failure callback"))
			|| !RunLoadAndWait(TEXT("StartClassSuccessLoad"), TEXT("ClassSuccessCallbackCount"), TEXT("Soft class success callback"))
			|| !RunLoadAndWait(TEXT("StartClassFailureLoad"), TEXT("ClassFailureCallbackCount"), TEXT("Soft class failure callback")))
		{
			return;
		}

		int32 ObjectSuccessCallbackCount = 0;
		int32 ObjectFailureCallbackCount = 0;
		int32 ClassSuccessCallbackCount = 0;
		int32 ClassFailureCallbackCount = 0;
		int32 bObjectSuccessWasNonNull = 0;
		int32 bObjectFailureWasNull = 0;
		int32 bClassSuccessWasNonNull = 0;
		int32 bClassFailureWasNull = 0;
		int32 bObjectPayloadMatchesExpectedType = 0;
		int32 bClassPayloadMatchesExpectedType = 0;
		FString LastObjectName;
		FString LastClassName;
		if (!ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ObjectSuccessCallbackCount"), ObjectSuccessCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ObjectFailureCallbackCount"), ObjectFailureCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ClassSuccessCallbackCount"), ClassSuccessCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ClassFailureCallbackCount"), ClassFailureCallbackCount)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bObjectSuccessWasNonNull"), bObjectSuccessWasNonNull)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bObjectFailureWasNull"), bObjectFailureWasNull)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bClassSuccessWasNonNull"), bClassSuccessWasNonNull)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bClassFailureWasNull"), bClassFailureWasNull)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bObjectPayloadMatchesExpectedType"), bObjectPayloadMatchesExpectedType)
			|| !ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bClassPayloadMatchesExpectedType"), bClassPayloadMatchesExpectedType)
			|| !ReadStringPropertyChecked(*TestRunner, ScriptHarness, TEXT("LastObjectName"), LastObjectName)
			|| !ReadStringPropertyChecked(*TestRunner, ScriptHarness, TEXT("LastClassName"), LastClassName))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Soft object success load should invoke the callback exactly once"), ObjectSuccessCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Soft object failure load should invoke the callback exactly once"), ObjectFailureCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Soft class success load should invoke the callback exactly once"), ClassSuccessCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Soft class failure load should invoke the callback exactly once"), ClassFailureCallbackCount, 1);
		TestRunner->TestEqual(TEXT("Soft object success callback should receive a non-null payload"), bObjectSuccessWasNonNull, 1);
		TestRunner->TestEqual(TEXT("Soft object failure callback should receive a null payload"), bObjectFailureWasNull, 1);
		TestRunner->TestEqual(TEXT("Soft class success callback should receive a non-null payload"), bClassSuccessWasNonNull, 1);
		TestRunner->TestEqual(TEXT("Soft class failure callback should receive a null payload"), bClassFailureWasNull, 1);
		TestRunner->TestEqual(TEXT("Soft object success callback should deliver an object of the expected texture type"), bObjectPayloadMatchesExpectedType, 1);
		TestRunner->TestEqual(TEXT("Soft class success callback should deliver a class of the expected actor type"), bClassPayloadMatchesExpectedType, 1);
		TestRunner->TestEqual(TEXT("Soft object success callback should resolve the expected texture asset"), LastObjectName, FString(TEXT("DefaultTexture")));
		TestRunner->TestEqual(TEXT("Soft class success callback should resolve the expected actor class"), LastClassName, AActor::StaticClass()->GetName());
	}
};

#endif
