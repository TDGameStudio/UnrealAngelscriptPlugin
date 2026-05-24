#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningScriptClassToBlueprintTraceTests_Private
{
UBlueprint* CreateTransientLearningBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("AngelscriptLearningScriptClassToBlueprintTraceTests"))
	{
		if (!Test.TestNotNull(TEXT("Blueprint parent class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/LearningBlueprintChild_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Transient blueprint package should be created"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			CallingContext);
		if (!Test.TestNotNull(TEXT("Transient blueprint asset should be created"), Blueprint))
		{
			return nullptr;
		}

		return Blueprint;
	}

	bool CompileAndValidateLearningBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		return Test.TestNotNull(TEXT("Blueprint should compile to a generated class"), Blueprint.GeneratedClass.Get());
	}

	void CleanupLearningBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	struct FScopedLearningTransientBlueprint
	{
		UBlueprint* BlueprintAsset = nullptr;

		~FScopedLearningTransientBlueprint()
		{
			CleanupLearningBlueprint(BlueprintAsset);
		}

		UClass* GetGeneratedClass() const
		{
			return BlueprintAsset != nullptr ? BlueprintAsset->GeneratedClass.Get() : nullptr;
		}
	};
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningScriptClassToBlueprintTraceTest,
	"Angelscript.TestModule.Learning.Runtime.ScriptClassToBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningScriptClassToBlueprintTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningScriptClassToBlueprintTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningScriptClassToBlueprintModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ALearningScriptClassToBlueprintActor : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	FString ActorLabel = "ScriptParent";

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningScriptClassToBlueprint"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningScriptClassToBlueprintModule.as"),
		ScriptSource,
		TEXT("ALearningScriptClassToBlueprintActor"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileScriptClass"), TEXT("Compiled the script parent class with BlueprintOverride so Unreal reflection can generate a UClass that Blueprint can inherit from"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());
	Trace.AddKeyValue(TEXT("ScriptSuperClass"), ScriptClass->GetSuperClass() != nullptr ? ScriptClass->GetSuperClass()->GetName() : TEXT("<null>"));

	FScopedLearningTransientBlueprint Blueprint;
	Blueprint.BlueprintAsset = CreateTransientLearningBlueprintChild(*this, ScriptClass, TEXT("LearningScriptClassToBlueprintChild"));
	if (Blueprint.BlueprintAsset == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CreateBlueprintChild"), TEXT("Created a transient Blueprint asset that inherits from the generated script class"));
	Trace.AddKeyValue(TEXT("BlueprintName"), Blueprint.BlueprintAsset->GetName());

	if (!CompileAndValidateLearningBlueprint(*this, *Blueprint.BlueprintAsset))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileBlueprintChild"), TEXT("Compiled the Blueprint asset into a generated Blueprint class that preserves the script parent hierarchy"));

	UClass* BlueprintClass = Blueprint.GetGeneratedClass();
	if (!TestNotNull(TEXT("Blueprint-child learning test case should provide a generated blueprint class"), BlueprintClass))
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddKeyValue(TEXT("BlueprintClassName"), BlueprintClass->GetName());
	Trace.AddKeyValue(TEXT("BlueprintSuperClass"), BlueprintClass->GetSuperClass() != nullptr ? BlueprintClass->GetSuperClass()->GetName() : TEXT("<null>"));
	Trace.AddKeyValue(TEXT("InheritsFromScriptClass"), BlueprintClass->IsChildOf(ScriptClass) ? TEXT("true") : TEXT("false"));

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, BlueprintClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnBlueprintActor"), TEXT("Spawned an actor instance from the Blueprint-generated class into a test world"));

	BeginPlayActor(*Actor);

	Trace.AddStep(TEXT("InvokeBeginPlay"), TEXT("Invoked BeginPlay on the spawned actor to trigger the script-defined BlueprintOverride"));

	int32 BeginPlayCount = 0;
	FString ActorLabel;
	const bool bReadBeginPlayCount = ReadPropertyValue<FIntProperty>(*this, Actor, TEXT("BeginPlayCount"), BeginPlayCount);
	const bool bReadActorLabel = ReadPropertyValue<FStrProperty>(*this, Actor, TEXT("ActorLabel"), ActorLabel);

	Trace.AddStep(TEXT("ReadScriptPropertyDefaults"), bReadBeginPlayCount && bReadActorLabel ? TEXT("Read reflected properties from the Blueprint actor instance to show that script defaults propagated through the Blueprint inheritance chain") : TEXT("Failed to read one or more reflected properties from the Blueprint actor"));
	Trace.AddKeyValue(TEXT("BeginPlayCount"), FString::FromInt(BeginPlayCount));
	Trace.AddKeyValue(TEXT("ActorLabel"), ActorLabel);

	const bool bScriptClassCompiled = TestNotNull(TEXT("Script parent class should be generated"), ScriptClass);
	const bool bBlueprintClassCreated = TestNotNull(TEXT("Blueprint child class should be generated"), BlueprintClass);
	const bool bInheritanceCorrect = TestTrue(TEXT("Blueprint class should inherit from the script parent"), BlueprintClass->IsChildOf(ScriptClass));
	const bool bBeginPlayCountCorrect = TestEqual(TEXT("Blueprint actor should preserve the script BeginPlay override"), BeginPlayCount, 1);
	const bool bActorLabelCorrect = TestEqual(TEXT("Blueprint actor should inherit script property defaults"), ActorLabel, FString(TEXT("ScriptParent")));

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsBlueprintKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("BlueprintClassName"));
	const bool bContainsInheritanceKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("InheritsFromScriptClass"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 6);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bBlueprintClassCreated
		&& bInheritanceCorrect
		&& bBeginPlayCountCorrect
		&& bActorLabelCorrect
		&& bPhaseSequenceOk
		&& bContainsBlueprintKeyword
		&& bContainsInheritanceKeyword
		&& bMinimumEventsOk;

	}
}

#endif
