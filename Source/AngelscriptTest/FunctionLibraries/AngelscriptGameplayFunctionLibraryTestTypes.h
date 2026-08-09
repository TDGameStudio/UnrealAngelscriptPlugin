#pragma once

// Reflected fixtures for the flat FunctionLibraries.Gameplay test theme.

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UObject/Object.h"

#include "AngelscriptGameplayFunctionLibraryTestTypes.generated.h"

UCLASS()
class ANGELSCRIPTTEST_API UAngelscriptAsyncSaveGameTestObject : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Marker = 0;
};

UCLASS()
class ANGELSCRIPTTEST_API UAngelscriptAsyncSaveLoadCallbackRecorder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SaveCallbackCount = 0;

	UPROPERTY()
	FString SaveSlotName;

	UPROPERTY()
	int32 SaveUserIndex = INDEX_NONE;

	UPROPERTY()
	bool bLastSaveSuccess = false;

	UPROPERTY()
	bool bSaveCallbackOnGameThread = false;

	UPROPERTY()
	int32 LoadCallbackCount = 0;

	UPROPERTY()
	FString LoadSlotName;

	UPROPERTY()
	int32 LoadUserIndex = INDEX_NONE;

	UPROPERTY()
	int32 LoadedMarker = INDEX_NONE;

	UPROPERTY()
	bool bLoadCallbackOnGameThread = false;

	UPROPERTY()
	bool bLoadReceivedNullObject = true;

	UPROPERTY()
	TObjectPtr<USaveGame> LastLoadedSaveGame = nullptr;

	UFUNCTION()
	void OnSaveComplete(const FString& SlotName, int32 UserIndex, bool bSuccess)
	{
		++SaveCallbackCount;
		SaveSlotName = SlotName;
		SaveUserIndex = UserIndex;
		bLastSaveSuccess = bSuccess;
		bSaveCallbackOnGameThread = IsInGameThread();
	}

	UFUNCTION()
	void OnLoadComplete(const FString& SlotName, int32 UserIndex, USaveGame* SaveGameObject)
	{
		++LoadCallbackCount;
		LoadSlotName = SlotName;
		LoadUserIndex = UserIndex;
		bLoadCallbackOnGameThread = IsInGameThread();
		LastLoadedSaveGame = SaveGameObject;
		bLoadReceivedNullObject = SaveGameObject == nullptr;
		LoadedMarker = INDEX_NONE;

		if (const UAngelscriptAsyncSaveGameTestObject* TypedSaveGame = Cast<UAngelscriptAsyncSaveGameTestObject>(SaveGameObject))
		{
			LoadedMarker = TypedSaveGame->Marker;
		}
	}

	void ResetSaveState()
	{
		SaveCallbackCount = 0;
		SaveSlotName.Reset();
		SaveUserIndex = INDEX_NONE;
		bLastSaveSuccess = false;
		bSaveCallbackOnGameThread = false;
	}

	void ResetLoadState()
	{
		LoadCallbackCount = 0;
		LoadSlotName.Reset();
		LoadUserIndex = INDEX_NONE;
		LoadedMarker = INDEX_NONE;
		bLoadCallbackOnGameThread = false;
		bLoadReceivedNullObject = true;
		LastLoadedSaveGame = nullptr;
	}
};
