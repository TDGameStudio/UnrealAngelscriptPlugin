#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS



#define AddExpectedError(...) Test.AddExpectedError(__VA_ARGS__)

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadEventTests,
	"Angelscript.TestModule.HotReload.Events",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName PostReloadModeModuleName = FName(TEXT("HotReloadPostReloadModeMod"));
inline static const FString PostReloadModeFilename = FString(TEXT("HotReloadPostReloadModeMod.as"));
inline static const FName PostReloadModeClassName = FName(TEXT("UPostReloadModeTarget"));

static bool IsHandledReloadResult(const ECompileResult ReloadResult)
{
	return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
}

struct FPostReloadObservation
{
	bool bWasFullReload = false;
	UClass* VisibleClass = nullptr;
};

struct FClassReloadObservation
{
	UClass* OldClass = nullptr;
	UClass* NewClass = nullptr;
};

struct FScopedPostReloadListener
{
	explicit FScopedPostReloadListener(FAngelscriptEngine& InEngine, const FName InClassName)
		: Engine(&InEngine)
		, ClassName(InClassName)
	{
		Handle = Engine->GetOnPostReload().AddRaw(this, &FScopedPostReloadListener::HandlePostReload);
	}

	~FScopedPostReloadListener()
	{
		if (Handle.IsValid() && Engine != nullptr)
		{
			Engine->GetOnPostReload().Remove(Handle);
		}
	}

	void HandlePostReload(const bool bWasFullReload)
	{
		FPostReloadObservation& Observation = Observations.AddDefaulted_GetRef();
		Observation.bWasFullReload = bWasFullReload;
		Observation.VisibleClass = FindGeneratedClass(Engine, ClassName);
	}

	FAngelscriptEngine* Engine = nullptr;
	FName ClassName;
	FDelegateHandle Handle;
	TArray<FPostReloadObservation> Observations;
};

struct FScopedReloadEventRecorder
{
	explicit FScopedReloadEventRecorder(FAngelscriptEngine& InEngine)
		: Engine(&InEngine)
	{
		PostReloadHandle = Engine->GetOnPostReload().AddRaw(this, &FScopedReloadEventRecorder::HandlePostReload);
		ClassReloadHandle = Engine->GetOnClassReload().AddRaw(this, &FScopedReloadEventRecorder::HandleClassReload);
		FullReloadHandle = Engine->GetOnFullReload().AddRaw(this, &FScopedReloadEventRecorder::HandleFullReload);
	}

	~FScopedReloadEventRecorder()
	{
		if (Engine == nullptr)
		{
			return;
		}

		if (PostReloadHandle.IsValid())
		{
			Engine->GetOnPostReload().Remove(PostReloadHandle);
		}

		if (ClassReloadHandle.IsValid())
		{
			Engine->GetOnClassReload().Remove(ClassReloadHandle);
		}

		if (FullReloadHandle.IsValid())
		{
			Engine->GetOnFullReload().Remove(FullReloadHandle);
		}
	}

	void HandlePostReload(const bool bWasFullReload)
	{
		PostReloadModes.Add(bWasFullReload);
	}

	void HandleClassReload(UClass* OldClass, UClass* NewClass)
	{
		FClassReloadObservation& Observation = ClassReloads.AddDefaulted_GetRef();
		Observation.OldClass = OldClass;
		Observation.NewClass = NewClass;
	}

	void HandleFullReload()
	{
		++FullReloadCount;
	}

	FDelegateHandle PostReloadHandle;
	FDelegateHandle ClassReloadHandle;
	FDelegateHandle FullReloadHandle;
	TArray<bool> PostReloadModes;
	TArray<FClassReloadObservation> ClassReloads;
	int32 FullReloadCount = 0;
	FAngelscriptEngine* Engine = nullptr;
};

static bool ExecuteGetValue(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	UClass* Class,
	const int32 ExpectedValue,
	const TCHAR* Context)
{
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should expose the generated class"), Context), Class))
	{
		return false;
	}

	UFunction* GetValueFunction = FindGeneratedFunction(Class, TEXT("GetValue"));
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should expose GetValue"), Context), GetValueFunction))
	{
		return false;
	}

	UObject* RuntimeObject = NewObject<UObject>(GetTransientPackage(), Class);
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should instantiate the generated class"), Context), RuntimeObject))
	{
		return false;
	}

	int32 Result = 0;
	if (!Test.TestTrue(
		*FString::Printf(TEXT("%s should execute GetValue on the game thread"), Context),
		ExecuteGeneratedIntEventOnGameThread(&Engine, RuntimeObject, GetValueFunction, Result)))
	{
		return false;
	}

	return Test.TestEqual(
		*FString::Printf(TEXT("%s should surface the expected GetValue result"), Context),
		Result,
		ExpectedValue);
}

static bool PostReloadModeFlagMatchesReloadPath(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*PostReloadModeModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UPostReloadModeTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 1;
	}
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class UPostReloadModeTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 2;
	}
}
)AS");

	const FString ScriptV3 = TEXT(R"AS(
UCLASS()
class UPostReloadModeTarget : UObject
{
	UPROPERTY()
	int Epoch = 3;

	UFUNCTION()
	int GetValue()
	{
		return Epoch;
	}
}
)AS");

	if (!Test.TestTrue(
		TEXT("Post-reload mode-flag test should compile the initial module"),
		CompileAnnotatedModuleFromMemory(&Engine, PostReloadModeModuleName, PostReloadModeFilename, ScriptV1)))
	{
		return false;
	}

	UClass* InitialClass = FindGeneratedClass(&Engine, PostReloadModeClassName);
	if (!ExecuteGetValue(Test, Engine, InitialClass, 1, TEXT("Initial post-reload mode-flag baseline")))
	{
		return false;
	}

	FScopedPostReloadListener Listener(Engine, PostReloadModeClassName);

	ECompileResult SoftReloadResult = ECompileResult::Error;
	if (!Test.TestTrue(
		TEXT("Post-reload mode-flag test should compile the body-only update on the soft reload path"),
		CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			PostReloadModeModuleName,
			PostReloadModeFilename,
			ScriptV2,
			SoftReloadResult)))
	{
		return false;
	}

	if (!Test.TestTrue(
		TEXT("Soft reload should stay on a handled reload path"),
		IsHandledReloadResult(SoftReloadResult)))
	{
		return false;
	}

	UClass* ClassAfterSoftReload = FindGeneratedClass(&Engine, PostReloadModeClassName);
	if (!Test.TestNotNull(TEXT("Soft reload should keep the generated class visible"), ClassAfterSoftReload))
	{
		return false;
	}

	Test.TestEqual(TEXT("Soft reload should preserve the live UClass object"), ClassAfterSoftReload, InitialClass);
	Test.TestEqual(TEXT("Soft reload should trigger exactly one post-reload event"), Listener.Observations.Num(), 1);
	if (Listener.Observations.Num() >= 1)
	{
		Test.TestFalse(
			TEXT("Soft reload should be reported as soft reload by the post-reload event"),
			Listener.Observations[0].bWasFullReload);
		Test.TestEqual(
			TEXT("Soft reload should already expose the canonical class when post-reload broadcasts"),
			Listener.Observations[0].VisibleClass,
			ClassAfterSoftReload);
	}

	if (!ExecuteGetValue(Test, Engine, ClassAfterSoftReload, 2, TEXT("Soft reload post-reload mode-flag baseline")))
	{
		return false;
	}

	ECompileResult FullReloadResult = ECompileResult::Error;
	if (!Test.TestTrue(
		TEXT("Post-reload mode-flag test should compile the structural update on the full reload path"),
		CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			PostReloadModeModuleName,
			PostReloadModeFilename,
			ScriptV3,
			FullReloadResult)))
	{
		return false;
	}

	if (!Test.TestTrue(
		TEXT("Full reload should stay on a handled reload path"),
		IsHandledReloadResult(FullReloadResult)))
	{
		return false;
	}

	UClass* ClassAfterFullReload = FindGeneratedClass(&Engine, PostReloadModeClassName);
	if (!Test.TestNotNull(TEXT("Full reload should keep the generated class visible"), ClassAfterFullReload))
	{
		return false;
	}

	Test.TestEqual(TEXT("Full reload should append a second post-reload event"), Listener.Observations.Num(), 2);
	if (Listener.Observations.Num() >= 2)
	{
		Test.TestTrue(TEXT("Full reload should be reported as full reload by the post-reload event"), Listener.Observations[1].bWasFullReload);
		Test.TestEqual(
			TEXT("Full reload should already expose the canonical class when post-reload broadcasts"),
			Listener.Observations[1].VisibleClass,
			ClassAfterFullReload);
	}

	Test.TestNotNull(TEXT("Full reload should expose the newly added Epoch property"), FindFProperty<FIntProperty>(ClassAfterFullReload, TEXT("Epoch")));
	if (!ExecuteGetValue(Test, Engine, ClassAfterFullReload, 3, TEXT("Full reload post-reload mode-flag baseline")))
	{
		return false;
	}

	}
	return true;
}

static bool FailedReloadDoesNotBroadcastReloadDelegates(FAutomationTestBase& Test)
{
static const FName ModuleName(TEXT("HotReloadFailedReloadEventMod"));
	static const FString Filename(TEXT("HotReloadFailedReloadEventMod.as"));
	static const FName ClassName(TEXT("UFailedReloadEventTarget"));

	AddExpectedError(TEXT("HotReloadFailedReloadEventMod.as:"), EAutomationExpectedErrorFlags::Contains, 2);
	AddExpectedError(TEXT("Identifier 'MissingType' is not a data type in global namespace"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Identifier 'MissingType' is not a data type"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Hot reload failed due to script compile errors. Keeping all old script code."), EAutomationExpectedErrorFlags::Contains, 1);

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class UFailedReloadEventTarget : UObject
{
	UFUNCTION()
	int GetValue()
	{
		return 5;
	}
}
)AS");

	const FString BrokenScript = TEXT(R"AS(
UCLASS()
class UFailedReloadEventTarget : UObject
{
	UFUNCTION()
	MissingType GetValue()
	{
		MissingType Value;
		return Value;
	}
}
)AS");

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	bool bPassed = true;
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	if (!Test.TestTrue(
		TEXT("Failed-reload event test should compile the initial module"),
		CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptV1)))
	{
		return false;
	}

	UClass* ClassBeforeFailure = FindGeneratedClass(&Engine, ClassName);
	if (!Test.TestNotNull(
		TEXT("Failed-reload event test should expose the generated class before reload failure"),
		ClassBeforeFailure))
	{
		return false;
	}

	if (!ExecuteGetValue(
		Test,
		Engine,
		ClassBeforeFailure,
		5,
		TEXT("Failed-reload event baseline")))
	{
		return false;
	}

	FScopedReloadEventRecorder ReloadEvents(Engine);

	ECompileResult ReloadResult = ECompileResult::FullyHandled;
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		ModuleName,
		Filename,
		BrokenScript,
		ReloadResult);

	bPassed &= Test.TestFalse(
		TEXT("Failed-reload event test should fail the broken hot reload compile"),
		bCompiled);
	bPassed &= Test.TestTrue(
		TEXT("Failed-reload event test should report an error reload state"),
		ReloadResult == ECompileResult::Error || ReloadResult == ECompileResult::ErrorNeedFullReload);
	bPassed &= Test.TestEqual(
		TEXT("Failed-reload event test should not broadcast post-reload when compilation fails"),
		ReloadEvents.PostReloadModes.Num(),
		0);
	bPassed &= Test.TestEqual(
		TEXT("Failed-reload event test should not broadcast class-reload when compilation fails"),
		ReloadEvents.ClassReloads.Num(),
		0);
	bPassed &= Test.TestEqual(
		TEXT("Failed-reload event test should not broadcast full-reload when compilation fails"),
		ReloadEvents.FullReloadCount,
		0);

	UClass* ClassAfterFailure = FindGeneratedClass(&Engine, ClassName);
	bPassed &= Test.TestEqual(
		TEXT("Failed-reload event test should keep the old generated class visible after the failed reload"),
		ClassAfterFailure,
		ClassBeforeFailure);
	if (ClassAfterFailure != nullptr)
	{
		bPassed &= ExecuteGetValue(
			Test,
			Engine,
			ClassAfterFailure,
			5,
			TEXT("Failed-reload event fallback"));
	}

	}
	return bPassed;
}

public:
	TEST_METHOD(PostReloadModeFlagMatchesReloadPath)
	{
		ASSERT_THAT(IsTrue(PostReloadModeFlagMatchesReloadPath(*TestRunner)));
	}

	TEST_METHOD(FailedReloadDoesNotBroadcastReloadDelegates)
	{
		ASSERT_THAT(IsTrue(FailedReloadDoesNotBroadcastReloadDelegates(*TestRunner)));
	}
};

#undef AddExpectedError

#endif
