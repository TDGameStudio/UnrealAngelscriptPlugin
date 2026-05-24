#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningClassGenerationTraceTests_Private
{
struct FLearningGeneratedPropertySummary
	{
		FString Name;
		FString Type;
		FString DefaultValue;
	};

	FString GetPropertyDefaultValue(FProperty& Property, UObject& DefaultObject)
	{
		FString Result;
		Property.ExportText_InContainer(0, Result, &DefaultObject, &DefaultObject, &DefaultObject, PPF_None);
		return Result;
	}

	TArray<FLearningGeneratedPropertySummary> GatherGeneratedProperties(UClass& GeneratedClass, UObject& DefaultObject)
	{
		TArray<FLearningGeneratedPropertySummary> Properties;
		for (TFieldIterator<FProperty> PropertyIt(&GeneratedClass); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			if (Property == nullptr || Property->GetOwnerClass() != &GeneratedClass)
			{
				continue;
			}

			FLearningGeneratedPropertySummary& Summary = Properties.AddDefaulted_GetRef();
			Summary.Name = Property->GetName();
			Summary.Type = Property->GetCPPType();
			Summary.DefaultValue = GetPropertyDefaultValue(*Property, DefaultObject);
		}
		return Properties;
	}

	template <typename PropertyType, typename ValueType>
	bool ReadGeneratedDefaultValue(UObject& DefaultObject, FName PropertyName, ValueType& OutValue)
	{
		PropertyType* Property = FindFProperty<PropertyType>(DefaultObject.GetClass(), PropertyName);
		if (Property == nullptr)
		{
			return false;
		}

		OutValue = Property->GetPropertyValue_InContainer(&DefaultObject);
		return true;
	}

	TArray<FString> FormatGeneratedProperties(const TArray<FLearningGeneratedPropertySummary>& Properties)
	{
		TArray<FString> Lines;
		for (const FLearningGeneratedPropertySummary& Property : Properties)
		{
			Lines.Add(FString::Printf(TEXT("%s : %s = %s"), *Property.Name, *Property.Type, *Property.DefaultValue));
		}
		return Lines;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningClassGenerationTraceTest,
	"Angelscript.TestModule.Learning.Runtime.ClassGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningClassGenerationTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningClassGenerationTraceTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	static const FName ModuleName(TEXT("LearningClassGenerationTraceModule"));
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(*ModuleName.ToString());
		
	};

	const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ALearningClassGenerationTraceActor : AActor
{
	UPROPERTY()
	int Health;

	UPROPERTY()
	bool bStartsEnabled;

	default Health = 7;
	default bStartsEnabled = true;

	UFUNCTION()
	int GetHealthValue()
	{
		return Health;
	}
}
)AS");

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningClassGeneration"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	const bool bCompiled = CompileAnnotatedModuleFromMemory(&Engine, ModuleName, TEXT("LearningClassGenerationTraceModule.as"), ScriptSource);
	Trace.AddStep(TEXT("CompileAnnotatedModule"), bCompiled ? TEXT("Compiled the annotated script class through the plugin pipeline so reflected Unreal types could be generated") : TEXT("Annotated script compilation failed before a generated class was produced"));
	Trace.AddKeyValue(TEXT("ModuleName"), ModuleName.ToString());
	Trace.AddKeyValue(TEXT("CompileResult"), bCompiled ? TEXT("true") : TEXT("false"));

	UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ALearningClassGenerationTraceActor"));
	Trace.AddStep(TEXT("FindGeneratedClass"), GeneratedClass != nullptr ? TEXT("Resolved the generated UClass by its Unreal-facing script class name") : TEXT("Generated UClass lookup failed after compilation"));
	Trace.AddKeyValue(TEXT("GeneratedClassName"), GeneratedClass != nullptr ? GeneratedClass->GetName() : TEXT("<null>"));
	Trace.AddKeyValue(TEXT("SuperClass"), GeneratedClass != nullptr && GeneratedClass->GetSuperClass() != nullptr ? GeneratedClass->GetSuperClass()->GetName() : TEXT("<null>"));
	Trace.AddKeyValue(TEXT("IsActorDerived"), GeneratedClass != nullptr && GeneratedClass->IsChildOf(AActor::StaticClass()) ? TEXT("true") : TEXT("false"));

	UFunction* GeneratedFunction = GeneratedClass != nullptr ? FindGeneratedFunction(GeneratedClass, TEXT("GetHealthValue")) : nullptr;
	Trace.AddStep(TEXT("FindGeneratedFunction"), GeneratedFunction != nullptr ? TEXT("Resolved the generated UFunction that mirrors the script-declared reflected method") : TEXT("Generated UFunction lookup failed for the reflected method"));
	Trace.AddKeyValue(TEXT("GeneratedFunctionName"), GeneratedFunction != nullptr ? GeneratedFunction->GetName() : TEXT("<null>"));
	Trace.AddKeyValue(TEXT("GeneratedFunctionFlags"), GeneratedFunction != nullptr ? FString::Printf(TEXT("0x%08X"), GeneratedFunction->FunctionFlags) : TEXT("<null>"));

	UObject* DefaultObject = GeneratedClass != nullptr ? GeneratedClass->GetDefaultObject() : nullptr;
	const TArray<FLearningGeneratedPropertySummary> Properties = GeneratedClass != nullptr && DefaultObject != nullptr
		? GatherGeneratedProperties(*GeneratedClass, *DefaultObject)
		: TArray<FLearningGeneratedPropertySummary>();
	Trace.AddStep(TEXT("EnumerateGeneratedProperties"), Properties.Num() > 0 ? TEXT("Enumerated the generated reflected properties from the class itself and read their default object values") : TEXT("No generated reflected properties were available to enumerate"));
	Trace.AddKeyValue(TEXT("GeneratedPropertyCount"), FString::FromInt(Properties.Num()));
	if (Properties.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(FormatGeneratedProperties(Properties), TEXT("GeneratedProperties")));
	}

	int32 HealthDefault = 0;
	bool bStartsEnabledDefault = false;
	const bool bReadHealthDefault = DefaultObject != nullptr && ReadGeneratedDefaultValue<FIntProperty>(*DefaultObject, TEXT("Health"), HealthDefault);
	const bool bReadStartsEnabledDefault = DefaultObject != nullptr && ReadGeneratedDefaultValue<FBoolProperty>(*DefaultObject, TEXT("bStartsEnabled"), bStartsEnabledDefault);
	Trace.AddStep(TEXT("ReadDefaultValues"), bReadHealthDefault && bReadStartsEnabledDefault ? TEXT("Read the default object to show how script defaults became Unreal property defaults") : TEXT("Failed to read one or more generated default values from the class default object"));
	Trace.AddKeyValue(TEXT("HealthDefault"), FString::FromInt(HealthDefault));
	Trace.AddKeyValue(TEXT("StartsEnabledDefault"), bStartsEnabledDefault ? TEXT("true") : TEXT("false"));

	const bool bCompiledOk = TestTrue(TEXT("Class generation learning script should compile"), bCompiled);
	const bool bGeneratedClassFound = TestNotNull(TEXT("Class generation learning script should produce a generated UClass"), GeneratedClass);
	const bool bIsActorDerived = TestTrue(TEXT("Generated class should derive from AActor"), GeneratedClass != nullptr && GeneratedClass->IsChildOf(AActor::StaticClass()));
	const bool bGeneratedFunctionFound = TestNotNull(TEXT("Generated class should expose the reflected GetHealthValue function"), GeneratedFunction);
	const bool bPropertyCountMatches = TestEqual(TEXT("Generated class should expose two reflected properties declared in the script"), Properties.Num(), 2);
	const bool bHealthDefaultMatches = TestEqual(TEXT("Generated Health default should match the script default"), HealthDefault, 7);
	const bool bStartsEnabledDefaultMatches = TestTrue(TEXT("Generated bool default should match the script default"), bStartsEnabledDefault);

	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsGeneratedClassKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("GeneratedClassName"));
	const bool bContainsGeneratedPropertyKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("GeneratedPropertyCount"));
	const bool bContainsDefaultValueKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("HealthDefault"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 5);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bCompiledOk
		&& bGeneratedClassFound
		&& bIsActorDerived
		&& bGeneratedFunctionFound
		&& bPropertyCountMatches
		&& bHealthDefaultMatches
		&& bStartsEnabledDefaultMatches
		&& bPhaseSequenceOk
		&& bContainsGeneratedClassKeyword
		&& bContainsGeneratedPropertyKeyword
		&& bContainsDefaultValueKeyword
		&& bMinimumEventsOk;

	}
}

#endif
