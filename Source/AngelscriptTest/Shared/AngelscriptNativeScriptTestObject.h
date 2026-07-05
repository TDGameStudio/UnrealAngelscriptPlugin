#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/Optional.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "AngelscriptNativeScriptTestObject.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EAngelscriptNativeTestEnum : uint8
{
	None,
	Alpha,
	Beta,
	Gamma
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=AngelscriptTest)
class ANGELSCRIPTTEST_API UAngelscriptVerifyClassPlainActorComponent : public UActorComponent
{
	GENERATED_BODY()
};

UCLASS(Abstract, BlueprintType, Blueprintable, ClassGroup=AngelscriptTest)
class ANGELSCRIPTTEST_API UAngelscriptVerifyClassAbstractSceneComponent : public USceneComponent
{
	GENERATED_BODY()
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=AngelscriptTest, meta=(NotAngelscriptSpawnable))
class ANGELSCRIPTTEST_API UAngelscriptVerifyClassNotSpawnableSceneComponent : public USceneComponent
{
	GENERATED_BODY()
};

UCLASS()
class ANGELSCRIPTTEST_API UAngelscriptNativeScriptTestObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bNativeFlag = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ExtensionPoint = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> NameHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UClass* PlainClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UClass*> NativeClassHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UObject> PrimaryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSubclassOf<UObject>> SupportedClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTexture2D> PrimaryTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<UTexture2D>> TextureHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TWeakObjectPtr<UTexture2D> WeakPrimaryTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UTexture2D> SoftPrimaryTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftObjectPtr<UTexture2D>> SoftTextureHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<UObject> SoftPrimaryClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftClassPtr<UObject>> SoftClassHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double PreciseValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 LargeCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector NativeVector = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator NativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform NativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText NativeLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TOptional<int32> OptionalCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TOptional<FName> OptionalName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSet<FName> NameSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, int32> NameCounts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAngelscriptNativeTestEnum NativeEnumValue = EAngelscriptNativeTestEnum::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EAngelscriptNativeTestEnum> NativeEnumHistory;

	UFUNCTION(BlueprintNativeEvent)
	float NativeEventWithParameter(float Value);
	virtual float NativeEventWithParameter_Implementation(float Value);

	UFUNCTION()
	int32 NativeIntEventWithParameter(int32 Value);

	UFUNCTION()
	int32 NativeRefIntEvent(int32& Value) const;

	UFUNCTION()
	int32 NativeNoArgValue() const;

	UFUNCTION()
	int32 NativeIntStringEvent(int32 Value, const FString& Label) const;

	UFUNCTION()
	void SetIntStringFromDelegate(int32 Value, const FString& Label);

	UFUNCTION()
	int32 NativeIntStringNameEvent(int32 Value, const FString& Label, FName Name) const;

	UFUNCTION()
	void SetIntStringNameFromDelegate(int32 Value, const FString& Label, FName Name);

	UFUNCTION()
	FString NativeStringEventWithParameter(const FString& Value) const;

	UFUNCTION()
	FString NativeRefStringEvent(FString& Value) const;

	UFUNCTION()
	FName NativeNameEventWithParameter(FName Value) const;

	UFUNCTION()
	FName NativeRefNameEvent(FName& Value) const;

	UFUNCTION()
	void SetPlainClassFromDelegate(UClass* Value);

	UFUNCTION()
	void SetExtensionPointFromDelegate(FName Value);

	UFUNCTION()
	void MarkNativeFlagFromDelegate();

	UFUNCTION()
	void SetPreciseValueFromDelegate(float Value);

	UFUNCTION()
	void SetLargeCountFromDelegate(int32 Value);

	UFUNCTION()
	float GetPreciseValueFromDelegate() const;
};
