#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Containers/Set.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreNet.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptFunctionalTestUtils;

namespace ASClassReplicationTest
{
	static const FName ModuleName(TEXT("ASClassLifetimeScriptReplicationList"));
	static const FString ScriptFilename(TEXT("ASClassLifetimeScriptReplicationList.as"));
	static const FName ParentClassName(TEXT("AReplicationParent"));
	static const FName ChildClassName(TEXT("AReplicationChild"));
	static const FName ParentValueName(TEXT("ParentValue"));
	static const FName ChildValueName(TEXT("ChildValue"));
	static const FName ChildNotifiedValueName(TEXT("ChildNotifiedValue"));
	static const FName ChildRepNotifyFunctionName(TEXT("OnRep_ChildNotifiedValue"));

	FName ResolveReplicatedPropertyName(const UClass* OwnerClass, const FLifetimeProperty& LifetimeProperty)
	{
		for (TFieldIterator<FProperty> It(OwnerClass); It; ++It)
		{
			if (It->RepIndex == LifetimeProperty.RepIndex)
			{
				return It->GetFName();
			}
		}

		return NAME_None;
	}

	TArray<FName> CollectReplicatedPropertyNamesFromLifetimeProps(
		const UClass* OwnerClass,
		const TArray<FLifetimeProperty>& LifetimeProperties)
	{
		TArray<FName> PropertyNames;
		PropertyNames.Reserve(LifetimeProperties.Num());

		for (const FLifetimeProperty& LifetimeProperty : LifetimeProperties)
		{
			const FName PropertyName = ResolveReplicatedPropertyName(OwnerClass, LifetimeProperty);
			if (PropertyName != NAME_None)
			{
				PropertyNames.Add(PropertyName);
			}
		}

		return PropertyNames;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassReplicationTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LifetimeScriptReplicationListIncludesInheritedReplicatedProperties)
	{
		using namespace ASClassReplicationTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASClassReplicationTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class AReplicationParent : AActor
{
	default SetReplicates(true);

	UPROPERTY(Replicated)
	int ParentValue = 7;
}

UCLASS()
class AReplicationChild : AReplicationParent
{
	UPROPERTY(Replicated)
	int ChildValue = 11;

	UPROPERTY(ReplicatedUsing=OnRep_ChildNotifiedValue)
	int ChildNotifiedValue = 29;

	UFUNCTION()
	void OnRep_ChildNotifiedValue()
	{
	}
}
)AS");

		UClass* ParentClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ASClassReplicationTest::ModuleName,
			ASClassReplicationTest::ScriptFilename,
			ScriptSource,
			ASClassReplicationTest::ParentClassName);
		if (ParentClass == nullptr)
		{
			return;
		}

		UClass* ChildClass = FindGeneratedClass(&Engine, ASClassReplicationTest::ChildClassName);
		if (!TestRunner->TestNotNull(TEXT("ASClass replication test case should generate the child script class"), ChildClass))
		{
			return;
		}

		UASClass* ChildASClass = Cast<UASClass>(ChildClass);
		if (!TestRunner->TestNotNull(TEXT("ASClass replication test case should compile the child as a UASClass"), ChildASClass))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("ASClass replication test case should keep the child superclass exact"), ChildClass->GetSuperClass(), ParentClass);

		FProperty* ParentValueProperty = FindFProperty<FProperty>(ChildClass, ASClassReplicationTest::ParentValueName);
		FProperty* ChildValueProperty = FindFProperty<FProperty>(ChildClass, ASClassReplicationTest::ChildValueName);
		FProperty* ChildNotifiedValueProperty = FindFProperty<FProperty>(ChildClass, ASClassReplicationTest::ChildNotifiedValueName);
		if (!TestRunner->TestNotNull(TEXT("ASClass replication test case should expose the inherited ParentValue property"), ParentValueProperty)
			|| !TestRunner->TestNotNull(TEXT("ASClass replication test case should expose the child ChildValue property"), ChildValueProperty)
			|| !TestRunner->TestNotNull(TEXT("ASClass replication test case should expose the child ChildNotifiedValue property"), ChildNotifiedValueProperty))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("ASClass replication test case should mark ParentValue as replicated"), ParentValueProperty->HasAnyPropertyFlags(CPF_Net));
		TestRunner->TestTrue(TEXT("ASClass replication test case should mark ChildValue as replicated"), ChildValueProperty->HasAnyPropertyFlags(CPF_Net));
		TestRunner->TestTrue(TEXT("ASClass replication test case should mark ChildNotifiedValue as replicated"), ChildNotifiedValueProperty->HasAnyPropertyFlags(CPF_Net));
		TestRunner->TestTrue(TEXT("ASClass replication test case should mark ChildNotifiedValue as a RepNotify property"), ChildNotifiedValueProperty->HasAnyPropertyFlags(CPF_RepNotify));
		TestRunner->TestEqual(
			TEXT("ASClass replication test case should preserve the RepNotify function name on the generated child property"),
			ChildNotifiedValueProperty->RepNotifyFunc,
			ASClassReplicationTest::ChildRepNotifyFunctionName);

		TArray<FLifetimeProperty> LifetimeProperties;
		ChildASClass->GetLifetimeScriptReplicationList(LifetimeProperties);

		const TArray<FName> ReplicatedPropertyNames =
			ASClassReplicationTest::CollectReplicatedPropertyNamesFromLifetimeProps(ChildClass, LifetimeProperties);

		TSet<FName> UniquePropertyNames;
		for (const FName PropertyName : ReplicatedPropertyNames)
		{
			UniquePropertyNames.Add(PropertyName);
		}

		TestRunner->TestEqual(
			TEXT("ASClass replication test case should collect exactly the script replicated properties declared across the parent-child chain"),
			LifetimeProperties.Num(),
			3);
		TestRunner->TestEqual(
			TEXT("ASClass replication test case should resolve every lifetime replication entry back to a concrete property name"),
			ReplicatedPropertyNames.Num(),
			LifetimeProperties.Num());
		TestRunner->TestEqual(
			TEXT("ASClass replication test case should not duplicate inherited script replicated properties in the lifetime list"),
			UniquePropertyNames.Num(),
			ReplicatedPropertyNames.Num());
		TestRunner->TestTrue(
			TEXT("ASClass replication test case should include the parent replicated property in the child lifetime list"),
			ReplicatedPropertyNames.Contains(ASClassReplicationTest::ParentValueName));
		TestRunner->TestTrue(
			TEXT("ASClass replication test case should include the direct child replicated property in the child lifetime list"),
			ReplicatedPropertyNames.Contains(ASClassReplicationTest::ChildValueName));
		TestRunner->TestTrue(
			TEXT("ASClass replication test case should include the child RepNotify property in the child lifetime list"),
			ReplicatedPropertyNames.Contains(ASClassReplicationTest::ChildNotifiedValueName));
		}
	}
};

#endif
