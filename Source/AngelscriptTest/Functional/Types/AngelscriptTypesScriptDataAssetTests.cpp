#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional - Round1 vacuum-fill (DataAsset script subclassing)
#if WITH_DEV_AUTOMATION_TESTS


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

		ASSERT_THAT(IsTrue(
			DataAssetClass->IsChildOf(UDataAsset::StaticClass()),
			TEXT("UFunctionalWeaponData should derive from UDataAsset")));

		FStrProperty* WeaponNameProp = FindFProperty<FStrProperty>(DataAssetClass, TEXT("WeaponName"));
		ASSERT_THAT(IsNotNull(WeaponNameProp, TEXT("WeaponName FStrProperty should be registered")));
		// AngelscriptSettings::bScriptFloatIsFloat64 defaults to true, so AS 'float' lowers to FDoubleProperty.
		FDoubleProperty* BaseDamageProp = FindFProperty<FDoubleProperty>(DataAssetClass, TEXT("BaseDamage"));
		ASSERT_THAT(IsNotNull(BaseDamageProp, TEXT("BaseDamage FDoubleProperty should be registered")));
		FDoubleProperty* FireRateProp = FindFProperty<FDoubleProperty>(DataAssetClass, TEXT("FireRate"));
		ASSERT_THAT(IsNotNull(FireRateProp, TEXT("FireRate FDoubleProperty should be registered")));
		FIntProperty* MaxAmmoProp = FindFProperty<FIntProperty>(DataAssetClass, TEXT("MaxAmmo"));
		ASSERT_THAT(IsNotNull(MaxAmmoProp, TEXT("MaxAmmo FIntProperty should be registered")));
		FArrayProperty* AttachmentsProp = FindFProperty<FArrayProperty>(DataAssetClass, TEXT("AllowedAttachments"));
		ASSERT_THAT(IsNotNull(AttachmentsProp, TEXT("AllowedAttachments FArrayProperty should be registered")));

		UObject* CDO = DataAssetClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(CDO, TEXT("UFunctionalWeaponData should have a valid CDO")));
		ASSERT_THAT(IsNear(10.0, BaseDamageProp->GetPropertyValue_InContainer(CDO), static_cast<double>(UE_KINDA_SMALL_NUMBER), TEXT("BaseDamage CDO default should be 10.0")));
		ASSERT_THAT(IsNear(0.5, FireRateProp->GetPropertyValue_InContainer(CDO), static_cast<double>(UE_KINDA_SMALL_NUMBER), TEXT("FireRate CDO default should be 0.5")));
		ASSERT_THAT(AreEqual(30, MaxAmmoProp->GetPropertyValue_InContainer(CDO), TEXT("MaxAmmo CDO default should be 30")));

		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("AFunctionalWeaponActor"));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("AFunctionalWeaponActor class should be generated")));
		FObjectProperty* WeaponConfigProp = FindFProperty<FObjectProperty>(ActorClass, TEXT("WeaponConfig"));
		ASSERT_THAT(IsNotNull(WeaponConfigProp, TEXT("WeaponConfig FObjectProperty should be registered")));
		ASSERT_THAT(IsTrue(
			WeaponConfigProp->PropertyClass == DataAssetClass,
			TEXT("WeaponConfig FObjectProperty class should reference UFunctionalWeaponData")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
