#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (DataAsset script subclassing)
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptDataAssetTests,
	"Angelscript.TestModule.Functional.Types.ScriptDataAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CompilesAndRegistersProperties)
	{
		using namespace AngelscriptFunctionalTestUtils;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		static const FName ModuleName(TEXT("FunctionalScriptDataAsset"));
		ON_SCOPE_EXIT { Engine.DiscardModule(*ModuleName.ToString()); };

		UClass* DataAssetClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("FunctionalScriptDataAsset.as"),
			TEXT(R"AS(
UCLASS()
class UFunctionalWeaponData : UDataAsset
{
	UPROPERTY(EditAnywhere)
	FString WeaponName;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	float BaseDamage = 10.0;

	UPROPERTY(EditAnywhere)
	float FireRate = 0.5;

	UPROPERTY(EditAnywhere)
	int32 MaxAmmo = 30;

	UPROPERTY(EditAnywhere)
	TArray<FName> AllowedAttachments;
}

UCLASS()
class AFunctionalWeaponActor : AActor
{
	UPROPERTY(EditAnywhere)
	UFunctionalWeaponData WeaponConfig;
}
)AS"),
			TEXT("UFunctionalWeaponData"));
		if (DataAssetClass == nullptr) { return; }

		TestRunner->TestTrue(
			TEXT("UFunctionalWeaponData should derive from UDataAsset"),
			DataAssetClass->IsChildOf(UDataAsset::StaticClass()));

		FStrProperty* WeaponNameProp = FindFProperty<FStrProperty>(DataAssetClass, TEXT("WeaponName"));
		TestRunner->TestNotNull(TEXT("WeaponName FStrProperty should be registered"), WeaponNameProp);
		// AngelscriptSettings::bScriptFloatIsFloat64 defaults to true, so AS 'float' lowers to FDoubleProperty.
		FDoubleProperty* BaseDamageProp = FindFProperty<FDoubleProperty>(DataAssetClass, TEXT("BaseDamage"));
		TestRunner->TestNotNull(TEXT("BaseDamage FDoubleProperty should be registered"), BaseDamageProp);
		FDoubleProperty* FireRateProp = FindFProperty<FDoubleProperty>(DataAssetClass, TEXT("FireRate"));
		TestRunner->TestNotNull(TEXT("FireRate FDoubleProperty should be registered"), FireRateProp);
		FIntProperty* MaxAmmoProp = FindFProperty<FIntProperty>(DataAssetClass, TEXT("MaxAmmo"));
		TestRunner->TestNotNull(TEXT("MaxAmmo FIntProperty should be registered"), MaxAmmoProp);
		FArrayProperty* AttachmentsProp = FindFProperty<FArrayProperty>(DataAssetClass, TEXT("AllowedAttachments"));
		TestRunner->TestNotNull(TEXT("AllowedAttachments FArrayProperty should be registered"), AttachmentsProp);

		UObject* CDO = DataAssetClass->GetDefaultObject();
		if (TestRunner->TestNotNull(TEXT("UFunctionalWeaponData should have a valid CDO"), CDO))
		{
			if (BaseDamageProp != nullptr)
			{
				TestRunner->TestEqual(
					TEXT("BaseDamage CDO default should be 10.0"),
					BaseDamageProp->GetPropertyValue_InContainer(CDO),
					10.0);
			}
			if (FireRateProp != nullptr)
			{
				TestRunner->TestEqual(
					TEXT("FireRate CDO default should be 0.5"),
					FireRateProp->GetPropertyValue_InContainer(CDO),
					0.5);
			}
			if (MaxAmmoProp != nullptr)
			{
				TestRunner->TestEqual(
					TEXT("MaxAmmo CDO default should be 30"),
					MaxAmmoProp->GetPropertyValue_InContainer(CDO),
					30);
			}
		}

		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("AFunctionalWeaponActor"));
		if (TestRunner->TestNotNull(TEXT("AFunctionalWeaponActor class should be generated"), ActorClass))
		{
			FObjectProperty* WeaponConfigProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("WeaponConfig"));
			if (TestRunner->TestNotNull(TEXT("WeaponConfig FObjectProperty should be registered"), WeaponConfigProp))
			{
				TestRunner->TestTrue(
					TEXT("WeaponConfig FObjectProperty class should reference UFunctionalWeaponData"),
					WeaponConfigProp->PropertyClass == DataAssetClass);
			}
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
