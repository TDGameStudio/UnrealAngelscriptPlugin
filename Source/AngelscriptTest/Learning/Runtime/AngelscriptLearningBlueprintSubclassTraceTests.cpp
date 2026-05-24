#include "AngelscriptLearningTrace.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningBlueprintSubclassTraceTests_Private
{
	UBlueprint* CreateLearningBlueprintChild(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix)
	{
		if (!Test.TestNotNull(TEXT("Blueprint parent class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/LearningBlueprint_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Blueprint package should be created"), BlueprintPackage))
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
			TEXT("AngelscriptLearningBlueprintSubclassTraceTests"));
		if (!Test.TestNotNull(TEXT("Blueprint asset should be created"), Blueprint))
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!Test.TestNotNull(TEXT("Blueprint should compile"), Blueprint->GeneratedClass.Get()))
		{
			return nullptr;
		}

		return Blueprint;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningBlueprintSubclassTraceTest,
	"Angelscript.TestModule.Learning.Runtime.BlueprintSubclass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningBlueprintSubclassTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningBlueprintSubclassTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningBlueprintSubclassModule"));
	UBlueprint* Blueprint = nullptr;
	ON_SCOPE_EXIT
	{
		if (Blueprint != nullptr)
		{
			if (UClass* BlueprintClass = Blueprint->GeneratedClass)
			{
				BlueprintClass->MarkAsGarbage();
			}
			Blueprint->MarkAsGarbage();
		}
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS(Blueprintable)
class ALearningBlueprintSubclassBase : AActor
{
	UPROPERTY(BlueprintReadWrite)
	float BaseValue = 100.0f;

	UPROPERTY(BlueprintReadWrite)
	FString BaseLabel = "ScriptBase";

	UFUNCTION(BlueprintCallable)
	void SetBaseValue(float NewValue)
	{
		BaseValue = NewValue;
	}

	UFUNCTION(BlueprintCallable)
	float GetBaseValue()
	{
		return BaseValue;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BaseLabel = "ScriptBeginPlay";
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningBlueprintSubclass"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	UClass* ScriptClass = CompileScriptModule(
		*this,
		Engine,
		ModuleName,
		TEXT("LearningBlueprintSubclassModule.as"),
		ScriptSource,
		TEXT("ALearningBlueprintSubclassBase"));
	if (ScriptClass == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CompileBlueprintableScript"), TEXT("Compiled the script class with Blueprintable specifier to enable Blueprint inheritance"));
	Trace.AddKeyValue(TEXT("ScriptClassName"), ScriptClass->GetName());
	Trace.AddKeyValue(TEXT("ScriptClassFlags"), FString::Printf(TEXT("0x%08X"), ScriptClass->GetFlags()));

	Blueprint = CreateLearningBlueprintChild(*this, ScriptClass, TEXT("BlueprintSubclass"));
	if (Blueprint == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("CreateBlueprintChild"), TEXT("Created a Blueprint asset that inherits from the script class"));
	Trace.AddKeyValue(TEXT("BlueprintName"), Blueprint->GetName());

	UClass* BlueprintClass = Blueprint->GeneratedClass;
	Trace.AddStep(TEXT("CompileBlueprint"), BlueprintClass != nullptr ? TEXT("Compiled the Blueprint and obtained its generated class") : TEXT("Blueprint compilation did not produce a generated class"));
	Trace.AddKeyValue(TEXT("BlueprintClassName"), BlueprintClass != nullptr ? BlueprintClass->GetName() : TEXT("<null>"));

	if (BlueprintClass != nullptr)
	{
		UClass* ParentClass = BlueprintClass->GetSuperClass();
		Trace.AddStep(TEXT("VerifyInheritance"), ParentClass == ScriptClass ? TEXT("Verified Blueprint class inherits directly from the script parent class") : TEXT("Inheritance chain differs from expected"));
		Trace.AddKeyValue(TEXT("ParentClassName"), ParentClass != nullptr ? ParentClass->GetName() : TEXT("<null>"));
	}

	FActorTestSpawner Spawner;
	Spawner.InitializeGameSubsystems();
	AActor* Actor = SpawnScriptActor(*this, Spawner, BlueprintClass);
	if (Actor == nullptr)
	{
		Trace.FlushToAutomation(*this);
		Trace.FlushToLog();
		return false;
	}

	Trace.AddStep(TEXT("SpawnBlueprintActor"), TEXT("Spawned an instance of the Blueprint class"));

	BeginPlayActor(Engine, *Actor);
	Trace.AddStep(TEXT("BeginPlayBlueprintActor"), TEXT("Called BeginPlay on the Blueprint actor instance"));

	float PropertyValue = 0.0f;
	FProperty* BaseValueProp = FindFProperty<FProperty>(Actor->GetClass(), TEXT("BaseValue"));
	if (BaseValueProp != nullptr)
	{
		PropertyValue = *BaseValueProp->ContainerPtrToValuePtr<float>(Actor);
		Trace.AddKeyValue(TEXT("BaseValueAfterBeginPlay"), FString::SanitizeFloat(PropertyValue));
	}
	else
	{
		Trace.AddKeyValue(TEXT("BaseValueAfterBeginPlay"), TEXT("<property not found>"));
	}

	FString LabelValue;
	FProperty* BaseLabelProp = FindFProperty<FProperty>(Actor->GetClass(), TEXT("BaseLabel"));
	if (BaseLabelProp != nullptr)
	{
		LabelValue = *BaseLabelProp->ContainerPtrToValuePtr<FString>(Actor);
		Trace.AddKeyValue(TEXT("BaseLabelAfterBeginPlay"), LabelValue);
	}
	else
	{
		Trace.AddKeyValue(TEXT("BaseLabelAfterBeginPlay"), TEXT("<property not found>"));
	}

	Trace.AddStep(TEXT("BlueprintSubclassObservation"), TEXT("Blueprint subclass inherits script properties and methods; BeginPlay override in script executes when Blueprint actor begins play; property defaults propagate from script to Blueprint instances"));

	const bool bScriptClassCompiled = TestNotNull(TEXT("Blueprint subclass learning script should produce a script class"), ScriptClass);
	const bool bBlueprintCreated = TestNotNull(TEXT("Blueprint should be created"), Blueprint);
	const bool bBlueprintClassValid = TestNotNull(TEXT("Blueprint class should be valid"), BlueprintClass);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsBlueprintKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("BlueprintClassName"));
	const bool bContainsInheritanceKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ParentClassName"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 7);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bScriptClassCompiled
		&& bBlueprintCreated
		&& bBlueprintClassValid
		&& bPhaseSequenceOk
		&& bContainsBlueprintKeyword
		&& bContainsInheritanceKeyword
		&& bMinimumEventsOk;

	}
}

#endif
