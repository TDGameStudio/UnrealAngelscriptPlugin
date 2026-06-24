#include "AngelscriptConstructionContextProbe.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace ASClassConstructionContextTest
{
	static const FName ModuleName(TEXT("ASClassConstructionContext"));
	static const FString ScriptFilename(TEXT("ASClassConstructionContext.as"));
	static const FName GeneratedClassName(TEXT("UConstructionContextCarrier"));

	void ResetProbeState()
	{
		UAngelscriptConstructionContextProbe::ResetCaptureState();
	}

	bool VerifyProbeBaseline(FAutomationTestBase& Test)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bCapturedObjectCleared = LocalAssert.IsNull(
			UAngelscriptConstructionContextProbe::GetLastCapturedObject(),
			TEXT("Construction-context probe should start without a captured object"));
		const bool bCaptureCountCleared = LocalAssert.AreEqual(
			0,
			UAngelscriptConstructionContextProbe::GetLastCaptureCount(),
			TEXT("Construction-context probe should start with a zero capture count"));
		return bCapturedObjectCleared && bCaptureCountCleared;
	}

	UASClass* CompileConstructionContextCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
delegate UObject FConstructionContextProbe();

UCLASS()
class UConstructionContextCarrier : UObject
{
	UPROPERTY()
	UObject CapturedDuringDefaults = nullptr;

	default CapturedDuringDefaults = FConstructionContextProbe(
		FindClass("UAngelscriptConstructionContextProbe").GetDefaultObject(),
		n"CaptureConstructingObject").ExecuteIfBound();
}
)AS");

		UClass* GeneratedClass = CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
		if (GeneratedClass == nullptr)
		{
			return nullptr;
		}

		UASClass* GeneratedASClass = Cast<UASClass>(GeneratedClass);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(
			GeneratedASClass,
			TEXT("Construction-context test case should compile to a generated UASClass")))
		{
			return nullptr;
		}
		return GeneratedASClass;
	}

	bool VerifyPostConstructionState(
		FAutomationTestBase& Test,
		UObject* Instance)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Instance, TEXT("Construction-context test case should create the generated script object")))
		{
			return false;
		}

		const bool bCaptureCountMatches = LocalAssert.AreEqual(
			1,
			UAngelscriptConstructionContextProbe::GetLastCaptureCount(),
			TEXT("Construction-context test case should capture the constructing object exactly once during instance defaults"));

		UObject* CapturedObject = UAngelscriptConstructionContextProbe::GetLastCapturedObject();
		const bool bProbeCapturedInstance = LocalAssert.IsTrue(
			CapturedObject == Instance,
			TEXT("Construction-context test case should record the final instance through the native probe"));

		const bool bConstructionStateCleared = LocalAssert.IsNull(
			UASClass::GetConstructingASObject(),
			TEXT("Construction-context test case should clear GetConstructingASObject after NewObject completes"));

		return bCaptureCountMatches
			&& bProbeCapturedInstance
			&& bConstructionStateCleared;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassConstructionContextTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GetConstructingASObjectReportsCurrentScriptInstance)
	{
		using namespace ASClassConstructionContextTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ASClassConstructionContextTest::ResetProbeState();

		ON_SCOPE_EXIT
		{
			ASClassConstructionContextTest::ResetProbeState();
			Engine.DiscardModule(*ASClassConstructionContextTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
			CollectGarbage(RF_NoFlags, true);
		};

		if (!ASClassConstructionContextTest::VerifyProbeBaseline(*TestRunner))
		{
			return;
		}

		ASSERT_THAT(IsNull(
			UASClass::GetConstructingASObject(),
			TEXT("Construction-context test case should not expose a constructing object before compiling or instantiating")));

		UASClass* GeneratedASClass = ASClassConstructionContextTest::CompileConstructionContextCarrier(*TestRunner, Engine);
		if (GeneratedASClass == nullptr)
		{
			return;
		}

		ASClassConstructionContextTest::ResetProbeState();
		if (!ASClassConstructionContextTest::VerifyProbeBaseline(*TestRunner))
		{
			return;
		}

		ASSERT_THAT(IsNull(
			UASClass::GetConstructingASObject(),
			TEXT("Construction-context test case should clear any compile-time CDO capture before the runtime instantiation step")));

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedASClass, TEXT("ConstructionContextCarrier"));
		if (Instance == nullptr)
		{
			return;
		}

		Instance->AddToRoot();
		TWeakObjectPtr<UObject> WeakInstance = Instance;
		ON_SCOPE_EXIT
		{
			if (WeakInstance.IsValid())
			{
				WeakInstance->RemoveFromRoot();
				WeakInstance->MarkAsGarbage();
			}
		};

		ASClassConstructionContextTest::VerifyPostConstructionState(
			*TestRunner,
			Instance);

		}
	}
};

#endif
