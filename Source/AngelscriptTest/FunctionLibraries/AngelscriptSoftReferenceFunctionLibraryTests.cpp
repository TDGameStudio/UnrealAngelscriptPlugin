// ============================================================================
// AngelscriptSoftReferenceFunctionLibraryTests.cpp
//
// Soft reference FunctionLibrary async delegate coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.SoftReference.FAngelscriptSoftReferenceFunctionLibraryTest.*
//
// Sections:
//   AsyncDelegates — object/class success/failure async load callbacks
//
// CQTest adaptation notes:
//   Single legacy automation test merged into TEST_CLASS.
//   Async harness pattern preserved with object instantiation and pumped callbacks.
//   Uses `*TestRunner` instead of `this` for assertions.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptFunctionalTestUtils.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSoftReferenceFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.SoftReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName SoftReferenceAsyncModuleName = FName(TEXT("ASoftReferenceAsyncDelegates"));
	inline static const FString SoftReferenceAsyncFilename = FString(TEXT("SoftReferenceAsyncDelegates.as"));
	inline static const FName SoftReferenceAsyncClassName = FName(TEXT("USoftReferenceAsyncScriptHarness"));
	inline static const FString SuccessTexturePath = FString(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
	static constexpr double SoftReferenceAsyncTimeoutSeconds = 5.0;

	static void PumpSoftReferenceCallbacks()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread_Local);
		FTSTicker::GetCoreTicker().Tick(0.0f);
		FPlatformProcess::Sleep(0.001f);
	}

	static bool WaitUntilSoftReference(
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

	static bool ExecuteGeneratedIntMethod(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		FName FunctionName,
		int32& OutResult)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Soft-reference async method '%s' should exist"), *FunctionName.ToString())))
		{
			return false;
		}

		return LocalAssert.IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, Object, Function, OutResult),
			*FString::Printf(TEXT("Soft-reference async method '%s' should execute"), *FunctionName.ToString()));
	}

	// ReadIntPropertyChecked / ReadStringPropertyChecked provided by
	// Shared/AngelscriptFunctionalTestUtils.h

	static bool VerifyObjectCallbackSignature(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		FName FunctionName,
		FName ParameterName,
		UClass* ExpectedClass)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Soft-reference callback '%s' should exist"), *FunctionName.ToString())))
		{
			return false;
		}

		FObjectProperty* Property = FindFProperty<FObjectProperty>(Function, ParameterName);
		if (!LocalAssert.IsNotNull(
			Property,
			*FString::Printf(TEXT("Soft-reference callback '%s' should expose object parameter '%s'"), *FunctionName.ToString(), *ParameterName.ToString())))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedClass,
			Property->PropertyClass.Get(),
			*FString::Printf(TEXT("Soft-reference callback '%s' should keep the current UObject delegate surface"), *FunctionName.ToString()));
	}

	static bool VerifyClassCallbackSignature(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		FName FunctionName,
		FName ParameterName,
		UClass* ExpectedMetaClass)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
			Function,
			*FString::Printf(TEXT("Soft-reference callback '%s' should exist"), *FunctionName.ToString())))
		{
			return false;
		}

		FClassProperty* Property = FindFProperty<FClassProperty>(Function, ParameterName);
		if (!LocalAssert.IsNotNull(
			Property,
			*FString::Printf(TEXT("Soft-reference callback '%s' should expose class parameter '%s'"), *FunctionName.ToString(), *ParameterName.ToString())))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedMetaClass,
			Property->MetaClass.Get(),
			*FString::Printf(TEXT("Soft-reference callback '%s' should keep the current UClass delegate surface"), *FunctionName.ToString()));
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

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

		const FString ScriptTemplate = ASTEST_AS(R"AS(
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
					TSoftObjectPtr<UTexture2D>(FSoftObjectPath("$SUCCESS_TEXTURE_PATH$")).LoadAsync(Delegate);
					return 1;
				}

				UFUNCTION()
				int StartObjectFailureLoad()
				{
					FOnSoftObjectLoaded Delegate;
					Delegate.BindUFunction(this, n"HandleObjectFailure");
					TSoftObjectPtr<UTexture2D>(FSoftObjectPath("$MISSING_OBJECT_PATH$")).LoadAsync(Delegate);
					return 1;
				}

				UFUNCTION()
				int StartClassSuccessLoad()
				{
					FOnSoftClassLoaded Delegate;
					Delegate.BindUFunction(this, n"HandleClassSuccess");
					TSoftClassPtr<AActor>(FSoftObjectPath("$SUCCESS_CLASS_PATH$")).LoadAsync(Delegate);
					return 1;
				}

				UFUNCTION()
				int StartClassFailureLoad()
				{
					FOnSoftClassLoaded Delegate;
					Delegate.BindUFunction(this, n"HandleClassFailure");
					TSoftClassPtr<AActor>(FSoftObjectPath("$MISSING_CLASS_PATH$")).LoadAsync(Delegate);
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
			)AS");
		FString ScriptSource = ScriptTemplate;
		ScriptSource.ReplaceInline(TEXT("$SUCCESS_TEXTURE_PATH$"), *SuccessTexturePath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("$MISSING_OBJECT_PATH$"), *MissingObjectPath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("$SUCCESS_CLASS_PATH$"), *SuccessClassPath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("$MISSING_CLASS_PATH$"), *MissingClassPath, ESearchCase::CaseSensitive);

		UClass* ScriptHarnessClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
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
		if (!this->Assert.IsNotNull(ScriptHarness, TEXT("Soft-reference async harness should be created")))
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

			if (!this->Assert.AreEqual(
				1,
				StartResult,
				*FString::Printf(TEXT("Soft-reference async starter '%s' should acknowledge launch"), *StartFunctionName.ToString())))
			{
				return false;
			}

			return WaitUntilSoftReference(
				*TestRunner,
				[this, ScriptHarness, CounterPropertyName]()
				{
					int32 CallbackCount = 0;
					return AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, CounterPropertyName, CallbackCount) && CallbackCount >= 1;
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
		if (!AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ObjectSuccessCallbackCount"), ObjectSuccessCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ObjectFailureCallbackCount"), ObjectFailureCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ClassSuccessCallbackCount"), ClassSuccessCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("ClassFailureCallbackCount"), ClassFailureCallbackCount)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bObjectSuccessWasNonNull"), bObjectSuccessWasNonNull)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bObjectFailureWasNull"), bObjectFailureWasNull)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bClassSuccessWasNonNull"), bClassSuccessWasNonNull)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bClassFailureWasNull"), bClassFailureWasNull)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bObjectPayloadMatchesExpectedType"), bObjectPayloadMatchesExpectedType)
			|| !AngelscriptFunctionalTestUtils::ReadIntPropertyChecked(*TestRunner, ScriptHarness, TEXT("bClassPayloadMatchesExpectedType"), bClassPayloadMatchesExpectedType)
			|| !AngelscriptFunctionalTestUtils::ReadStringPropertyChecked(*TestRunner, ScriptHarness, TEXT("LastObjectName"), LastObjectName)
			|| !AngelscriptFunctionalTestUtils::ReadStringPropertyChecked(*TestRunner, ScriptHarness, TEXT("LastClassName"), LastClassName))
		{
			return;
		}

		ASSERT_THAT(AreEqual(1, ObjectSuccessCallbackCount, TEXT("Soft object success load should invoke the callback exactly once")));
		ASSERT_THAT(AreEqual(1, ObjectFailureCallbackCount, TEXT("Soft object failure load should invoke the callback exactly once")));
		ASSERT_THAT(AreEqual(1, ClassSuccessCallbackCount, TEXT("Soft class success load should invoke the callback exactly once")));
		ASSERT_THAT(AreEqual(1, ClassFailureCallbackCount, TEXT("Soft class failure load should invoke the callback exactly once")));
		ASSERT_THAT(AreEqual(1, bObjectSuccessWasNonNull, TEXT("Soft object success callback should receive a non-null payload")));
		ASSERT_THAT(AreEqual(1, bObjectFailureWasNull, TEXT("Soft object failure callback should receive a null payload")));
		ASSERT_THAT(AreEqual(1, bClassSuccessWasNonNull, TEXT("Soft class success callback should receive a non-null payload")));
		ASSERT_THAT(AreEqual(1, bClassFailureWasNull, TEXT("Soft class failure callback should receive a null payload")));
		ASSERT_THAT(AreEqual(1, bObjectPayloadMatchesExpectedType, TEXT("Soft object success callback should deliver an object of the expected texture type")));
		ASSERT_THAT(AreEqual(1, bClassPayloadMatchesExpectedType, TEXT("Soft class success callback should deliver a class of the expected actor type")));
		ASSERT_THAT(AreEqual(FString(TEXT("DefaultTexture")), LastObjectName, TEXT("Soft object success callback should resolve the expected texture asset")));
		ASSERT_THAT(AreEqual(AActor::StaticClass()->GetName(), LastClassName, TEXT("Soft class success callback should resolve the expected actor class")));
	}
};

#endif
