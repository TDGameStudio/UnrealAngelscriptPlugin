#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Camera/CameraActor.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadReloadDelegateTests,
	"Angelscript.TestModule.HotReload.ReloadDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName DelegateReloadModuleName = FName(TEXT("HotReloadDelegateMod"));
	inline static const FString DelegateReloadFilename = FString(TEXT("HotReloadDelegateMod.as"));
	inline static const FName DelegateReloadCarrierClassName = FName(TEXT("UHotReloadEventCarrier"));
	inline static const FString DelegateReloadEnumName = FString(TEXT("EHotReloadEventState"));

	inline static const FName SignatureReloadModuleName = FName(TEXT("HotReloadDelegateSignatureMod"));
	inline static const FString SignatureReloadFilename = FString(TEXT("HotReloadDelegateSignatureMod.as"));
	inline static const FString SignatureReloadDelegateName = FString(TEXT("FHotReloadSignal"));
	inline static const FName SignatureReloadCarrierClassName = FName(TEXT("UHotReloadDelegateSignatureCarrier"));

	struct FEnumReloadObservation
	{
		int32 EnumChangedCount = 0;
		int32 FullReloadCount = 0;
		int32 PostReloadCount = 0;
		bool bPostReloadSawFullReload = false;
		bool bEnumVisibleDuringPostReload = false;
		bool bCarrierVisibleDuringPostReload = false;
		UEnum* EnumSeenDuringReload = nullptr;
		TArray<TPair<FName, int64>> OldNamesSeenDuringReload;
	};

	struct FDelegateReloadObservation
	{
		int32 DelegateReloadCount = 0;
		UDelegateFunction* OldDelegateSeenDuringReload = nullptr;
		UDelegateFunction* NewDelegateSeenDuringReload = nullptr;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool ContainsEnumEntryWithSuffix(const TArray<TPair<FName, int64>>& Entries, const TCHAR* Suffix, const int64 ExpectedValue)
	{
		const FString ExpectedSuffix(Suffix);
		for (const TPair<FName, int64>& Entry : Entries)
		{
			if (Entry.Value == ExpectedValue && Entry.Key.ToString().EndsWith(ExpectedSuffix))
			{
				return true;
			}
		}

		return false;
	}

	static bool TryFindEnumValueBySuffix(const UEnum& Enum, const TCHAR* Suffix, int64& OutValue)
	{
		const FString ExpectedSuffix(Suffix);
		const int32 EnumeratorsToCheck = Enum.NumEnums();
		for (int32 Index = 0; Index < EnumeratorsToCheck; ++Index)
		{
			const FString EnumEntryName = Enum.GetNameByIndex(Index).ToString();
			if (EnumEntryName.EndsWith(TEXT("_MAX")))
			{
				continue;
			}

			if (EnumEntryName.EndsWith(ExpectedSuffix))
			{
				OutValue = Enum.GetValueByIndex(Index);
				return true;
			}
		}

		return false;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(BroadcastEnumChangeAndFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FEnumReloadObservation Observation;
			FDelegateHandle EnumChangedHandle;
			FDelegateHandle FullReloadHandle;
			FDelegateHandle PostReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnEnumChanged().Remove(EnumChangedHandle);
				Engine.GetOnFullReload().Remove(FullReloadHandle);
				Engine.GetOnPostReload().Remove(PostReloadHandle);
				Engine.DiscardModule(*DelegateReloadModuleName.ToString());
			};

			const FString EnumReloadV1Source = ASTEST_AS(R"AS(
				UENUM(BlueprintType)
				enum class EHotReloadEventState : uint16
				{
					Alpha,
					Beta = 4
				}

				UCLASS()
				class UHotReloadEventCarrier : UObject
				{
					UPROPERTY()
					EHotReloadEventState State;

					default State = EHotReloadEventState::Alpha;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, DelegateReloadModuleName, DelegateReloadFilename, EnumReloadV1Source),
				TEXT("Initial enum reload-delegate module compile should succeed")));

			const TSharedPtr<FAngelscriptEnumDesc> EnumBeforeReload = Engine.GetEnum(DelegateReloadEnumName);
			ASSERT_THAT(IsTrue(EnumBeforeReload.IsValid(), TEXT("Enum metadata should exist before reload-delegate test")));

			UEnum* EnumObjectBeforeReload = EnumBeforeReload->Enum;
			ASSERT_THAT(IsNotNull(EnumObjectBeforeReload, TEXT("Enum object should exist before full reload")));

			ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, DelegateReloadCarrierClassName), TEXT("Enum carrier class should exist before full reload")));

			EnumChangedHandle = Engine.GetOnEnumChanged().AddLambda(
				[&Observation](UEnum* Enum, EnumNameList OldNames)
				{
					++Observation.EnumChangedCount;
					Observation.EnumSeenDuringReload = Enum;
					Observation.OldNamesSeenDuringReload = OldNames;
				});

			FullReloadHandle = Engine.GetOnFullReload().AddLambda(
				[&Observation]()
				{
					++Observation.FullReloadCount;
				});

			PostReloadHandle = Engine.GetOnPostReload().AddLambda(
				[&Engine, &Observation](const bool bWasFullReload)
				{
					++Observation.PostReloadCount;
					Observation.bPostReloadSawFullReload = bWasFullReload;
					Observation.bEnumVisibleDuringPostReload = Engine.GetEnum(DelegateReloadEnumName).IsValid();
					Observation.bCarrierVisibleDuringPostReload = FindGeneratedClass(&Engine, DelegateReloadCarrierClassName) != nullptr;
				});

			const FString EnumReloadV2Source = ASTEST_AS(R"AS(
				UENUM(BlueprintType)
				enum class EHotReloadEventState : uint16
				{
					Alpha,
					Beta = 4,
					Gamma = 9
				}

				UCLASS()
				class UHotReloadEventCarrier : UObject
				{
					UPROPERTY()
					EHotReloadEventState State;

					default State = EHotReloadEventState::Gamma;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, DelegateReloadModuleName, DelegateReloadFilename, EnumReloadV2Source, ReloadResult),
				TEXT("Enum full reload compile should succeed")));

			ASSERT_THAT(IsTrue(
				IsHandledReloadResult(ReloadResult),
				TEXT("Enum full reload should stay on a handled reload path")));

			const TSharedPtr<FAngelscriptEnumDesc> EnumAfterReload = Engine.GetEnum(DelegateReloadEnumName);
			ASSERT_THAT(IsTrue(EnumAfterReload.IsValid(), TEXT("Enum metadata should still exist after full reload")));

			UEnum* EnumObjectAfterReload = EnumAfterReload->Enum;
			ASSERT_THAT(IsNotNull(EnumObjectAfterReload, TEXT("Reloaded enum object should still be queryable")));

			ASSERT_THAT(IsNotNull(FindGeneratedClass(&Engine, DelegateReloadCarrierClassName), TEXT("Enum carrier class should still be queryable after full reload")));

			ASSERT_THAT(AreEqual(1, Observation.FullReloadCount, TEXT("Full reload should broadcast the full-reload delegate exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.EnumChangedCount, TEXT("Enum full reload should broadcast the enum-changed delegate exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.PostReloadCount, TEXT("Enum full reload should broadcast the post-reload delegate exactly once")));
			ASSERT_THAT(IsTrue(Observation.bPostReloadSawFullReload, TEXT("Post-reload delegate should report that the completed reload was a full reload")));
			ASSERT_THAT(IsTrue(Observation.bEnumVisibleDuringPostReload, TEXT("Enum should already be queryable when post-reload broadcasts")));
			ASSERT_THAT(IsTrue(Observation.bCarrierVisibleDuringPostReload, TEXT("Carrier class should already be queryable when post-reload broadcasts")));
			ASSERT_THAT(AreEqual(EnumObjectAfterReload, Observation.EnumSeenDuringReload, TEXT("Enum-changed broadcast should expose the live enum object")));

			ASSERT_THAT(AreEqual(2, Observation.OldNamesSeenDuringReload.Num(), TEXT("Enum-changed broadcast should preserve the old enum member count")));
			ASSERT_THAT(IsTrue(ContainsEnumEntryWithSuffix(Observation.OldNamesSeenDuringReload, TEXT("Alpha"), 0), TEXT("Old enum member list should keep Alpha before reload")));
			ASSERT_THAT(IsTrue(ContainsEnumEntryWithSuffix(Observation.OldNamesSeenDuringReload, TEXT("Beta"), 4), TEXT("Old enum member list should keep Beta before reload")));
			ASSERT_THAT(IsFalse(ContainsEnumEntryWithSuffix(Observation.OldNamesSeenDuringReload, TEXT("Gamma"), 9), TEXT("Old enum member list should not include the new Gamma value")));

			int64 GammaValue = INDEX_NONE;
			ASSERT_THAT(IsTrue(TryFindEnumValueBySuffix(*EnumObjectAfterReload, TEXT("Gamma"), GammaValue), TEXT("Reloaded enum should expose the new Gamma enumerator")));

			ASSERT_THAT(AreEqual(int64(9), GammaValue, TEXT("Reloaded enum should assign the expected value to Gamma")));
		}
	}

	TEST_METHOD(BroadcastDelegateSignatureSwap)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FDelegateReloadObservation Observation;
			FDelegateHandle DelegateReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
				Engine.DiscardModule(*SignatureReloadModuleName.ToString());
			};

			const FString DelegateSignatureV1Source = ASTEST_AS(R"AS(
				delegate void FHotReloadSignal(int Value);

				UCLASS()
				class UHotReloadDelegateSignatureCarrier : UObject
				{
					UPROPERTY()
					FHotReloadSignal Signal;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, SignatureReloadModuleName, SignatureReloadFilename, DelegateSignatureV1Source),
				TEXT("Initial delegate-signature baseline compile should succeed")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(SignatureReloadDelegateName);
			UClass* CarrierClassBeforeReload = FindGeneratedClass(&Engine, SignatureReloadCarrierClassName);
			ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Delegate metadata should exist before signature reload")));
			ASSERT_THAT(IsNotNull(DelegateBeforeReload->Function, TEXT("Initial delegate function should exist before signature reload")));
			ASSERT_THAT(IsNotNull(CarrierClassBeforeReload, TEXT("Carrier class should exist before delegate signature reload")));

			ASSERT_THAT(IsNull(FindFProperty<FProperty>(DelegateBeforeReload->Function, TEXT("Label")), TEXT("Initial delegate signature should not expose the future Label parameter")));

			DelegateReloadHandle = Engine.GetOnDelegateReload().AddLambda(
				[&Observation](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
				{
					++Observation.DelegateReloadCount;
					Observation.OldDelegateSeenDuringReload = OldDelegate;
					Observation.NewDelegateSeenDuringReload = NewDelegate;
				});

			const FString DelegateSignatureV2Source = ASTEST_AS(R"AS(
				delegate void FHotReloadSignal(int Value, const FString& Label);

				UCLASS()
				class UHotReloadDelegateSignatureCarrier : UObject
				{
					UPROPERTY()
					FHotReloadSignal Signal;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, SignatureReloadModuleName, SignatureReloadFilename, DelegateSignatureV2Source, ReloadResult),
				TEXT("Delegate signature full reload compile should succeed")));

			ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Delegate signature reload should stay on a handled reload path")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(SignatureReloadDelegateName);
			UClass* CarrierClassAfterReload = FindGeneratedClass(&Engine, SignatureReloadCarrierClassName);
			FDelegateProperty* DelegatePropertyAfterReload = CarrierClassAfterReload != nullptr ? FindFProperty<FDelegateProperty>(CarrierClassAfterReload, TEXT("Signal")) : nullptr;
			ASSERT_THAT(IsTrue(DelegateAfterReload.IsValid(), TEXT("Delegate metadata should still exist after signature reload")));
			ASSERT_THAT(IsNotNull(DelegateAfterReload->Function, TEXT("Reloaded delegate function should remain queryable")));
			ASSERT_THAT(IsNotNull(CarrierClassAfterReload, TEXT("Carrier class should remain queryable after delegate signature reload")));
			ASSERT_THAT(IsNotNull(DelegatePropertyAfterReload, TEXT("Carrier class should keep the delegate property after reload")));

			ASSERT_THAT(AreEqual(1, Observation.DelegateReloadCount, TEXT("Delegate signature reload should broadcast exactly once")));
			ASSERT_THAT(AreEqual(DelegateBeforeReload->Function, Observation.OldDelegateSeenDuringReload, TEXT("Delegate-reload callback should expose the old delegate function")));
			ASSERT_THAT(AreEqual(DelegateAfterReload->Function, Observation.NewDelegateSeenDuringReload, TEXT("Delegate-reload callback should expose the new delegate function")));
			ASSERT_THAT(AreNotEqual(Observation.OldDelegateSeenDuringReload, Observation.NewDelegateSeenDuringReload, TEXT("Delegate-reload callback should broadcast distinct old/new delegate functions")));
			ASSERT_THAT(IsTrue(DelegatePropertyAfterReload->SignatureFunction == DelegateAfterReload->Function, TEXT("Delegate property should retarget to the reloaded signature function")));
			ASSERT_THAT(IsNotNull(FindFProperty<FProperty>(DelegateAfterReload->Function, TEXT("Label")), TEXT("Reloaded delegate signature should expose the new Label parameter")));
		}
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadDelegateTests,
	"Angelscript.TestModule.HotReload.Delegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName TypeReloadModuleName = FName(TEXT("HotReloadDelegateTypeMod"));
	inline static const FString TypeReloadFilename = FString(TEXT("HotReloadDelegateTypeMod.as"));
	inline static const FString TypeReloadStructName = FString(TEXT("FHotReloadDelegatePayload"));
	inline static const FName TypeReloadCarrierClassName = FName(TEXT("UHotReloadDelegateCarrier"));

	struct FTypeReloadObservation
	{
		int32 ClassReloadCount = 0;
		int32 StructReloadCount = 0;
		int32 PostReloadCount = 0;
		bool bPostReloadSawFullReload = false;
		bool bStructVisibleDuringPostReload = false;
		bool bClassVisibleDuringPostReload = false;
		UClass* OldClassSeenDuringReload = nullptr;
		UClass* NewClassSeenDuringReload = nullptr;
		UScriptStruct* OldStructSeenDuringReload = nullptr;
		UScriptStruct* NewStructSeenDuringReload = nullptr;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(BroadcastOldAndNewTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FTypeReloadObservation Observation;
			FDelegateHandle ClassReloadHandle;
			FDelegateHandle StructReloadHandle;
			FDelegateHandle PostReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnClassReload().Remove(ClassReloadHandle);
				Engine.GetOnStructReload().Remove(StructReloadHandle);
				Engine.GetOnPostReload().Remove(PostReloadHandle);
				Engine.DiscardModule(*TypeReloadModuleName.ToString());
			};

			const FString TypeReloadV1Source = ASTEST_AS(R"AS(
				USTRUCT()
				struct FHotReloadDelegatePayload
				{
					UPROPERTY()
					int Value = 1;
				}

				UCLASS()
				class UHotReloadDelegateCarrier : UObject
				{
					UPROPERTY()
					int Revision = 1;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, TypeReloadModuleName, TypeReloadFilename, TypeReloadV1Source),
				TEXT("Initial class/struct reload-delegate module compile should succeed")));

			const TSharedPtr<FAngelscriptClassDesc> StructBeforeReload = Engine.GetClass(TypeReloadStructName);
			ASSERT_THAT(IsTrue(StructBeforeReload.IsValid(), TEXT("Struct metadata should exist before reload")));

			UScriptStruct* StructObjectBeforeReload = static_cast<UScriptStruct*>(StructBeforeReload->Struct);
			ASSERT_THAT(IsNotNull(StructObjectBeforeReload, TEXT("Struct object should exist before full reload")));

			UClass* ClassObjectBeforeReload = FindGeneratedClass(&Engine, TypeReloadCarrierClassName);
			ASSERT_THAT(IsNotNull(ClassObjectBeforeReload, TEXT("Carrier class should exist before full reload")));

			ClassReloadHandle = Engine.GetOnClassReload().AddLambda(
				[&Observation](UClass* OldClass, UClass* NewClass)
				{
					++Observation.ClassReloadCount;
					Observation.OldClassSeenDuringReload = OldClass;
					Observation.NewClassSeenDuringReload = NewClass;
				});

			StructReloadHandle = Engine.GetOnStructReload().AddLambda(
				[&Observation](UScriptStruct* OldStruct, UScriptStruct* NewStruct)
				{
					++Observation.StructReloadCount;
					Observation.OldStructSeenDuringReload = OldStruct;
					Observation.NewStructSeenDuringReload = NewStruct;
				});

			PostReloadHandle = Engine.GetOnPostReload().AddLambda(
				[&Engine, &Observation](const bool bWasFullReload)
				{
					++Observation.PostReloadCount;
					Observation.bPostReloadSawFullReload = bWasFullReload;
					Observation.bStructVisibleDuringPostReload = Engine.GetClass(TypeReloadStructName).IsValid();
					Observation.bClassVisibleDuringPostReload = FindGeneratedClass(&Engine, TypeReloadCarrierClassName) != nullptr;
				});

			const FString TypeReloadV2Source = ASTEST_AS(R"AS(
				USTRUCT()
				struct FHotReloadDelegatePayload
				{
					UPROPERTY()
					int Value = 2;

					UPROPERTY()
					int Bonus = 7;
				}

				UCLASS()
				class UHotReloadDelegateCarrier : UObject
				{
					UPROPERTY()
					int Revision = 2;

					UPROPERTY()
					int Epoch = 9;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, TypeReloadModuleName, TypeReloadFilename, TypeReloadV2Source, ReloadResult),
				TEXT("Class/struct full reload compile should succeed")));

			ASSERT_THAT(IsTrue(
				IsHandledReloadResult(ReloadResult),
				TEXT("Class/struct full reload should stay on a handled reload path")));

			const TSharedPtr<FAngelscriptClassDesc> StructAfterReload = Engine.GetClass(TypeReloadStructName);
			ASSERT_THAT(IsTrue(StructAfterReload.IsValid(), TEXT("Struct metadata should still exist after reload")));

			UScriptStruct* StructObjectAfterReload = static_cast<UScriptStruct*>(StructAfterReload->Struct);
			ASSERT_THAT(IsNotNull(StructObjectAfterReload, TEXT("Reloaded struct object should still be queryable")));

			UClass* ClassObjectAfterReload = FindGeneratedClass(&Engine, TypeReloadCarrierClassName);
			ASSERT_THAT(IsNotNull(ClassObjectAfterReload, TEXT("Reloaded carrier class should still be queryable")));

			ASSERT_THAT(AreEqual(1, Observation.ClassReloadCount, TEXT("Full reload should broadcast class-reload delegate exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.StructReloadCount, TEXT("Full reload should broadcast struct-reload delegate exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.PostReloadCount, TEXT("Full reload should broadcast post-reload delegate exactly once")));
			ASSERT_THAT(IsTrue(Observation.bPostReloadSawFullReload, TEXT("Post-reload delegate should report a full reload")));
			ASSERT_THAT(IsTrue(Observation.bStructVisibleDuringPostReload, TEXT("Struct should already be queryable when post-reload broadcasts")));
			ASSERT_THAT(IsTrue(Observation.bClassVisibleDuringPostReload, TEXT("Class should already be queryable when post-reload broadcasts")));
			ASSERT_THAT(AreEqual(StructObjectBeforeReload, Observation.OldStructSeenDuringReload, TEXT("Struct-reload delegate should expose the old struct")));
			ASSERT_THAT(AreEqual(StructObjectAfterReload, Observation.NewStructSeenDuringReload, TEXT("Struct-reload delegate should expose the new struct")));
			ASSERT_THAT(AreEqual(ClassObjectBeforeReload, Observation.OldClassSeenDuringReload, TEXT("Class-reload delegate should expose the old class")));
			ASSERT_THAT(AreEqual(ClassObjectAfterReload, Observation.NewClassSeenDuringReload, TEXT("Class-reload delegate should expose the new class")));
			ASSERT_THAT(AreNotEqual(Observation.OldStructSeenDuringReload, Observation.NewStructSeenDuringReload, TEXT("Struct-reload delegate should broadcast distinct old/new structs")));
			ASSERT_THAT(AreNotEqual(Observation.OldClassSeenDuringReload, Observation.NewClassSeenDuringReload, TEXT("Class-reload delegate should broadcast distinct old/new classes")));
		}
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadDelegateParameterInvocationTests,
	"Angelscript.TestModule.HotReload.ReloadDelegates.Parameters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName PrimitiveModuleName = FName(TEXT("HotReloadDelegateParameterPrimitiveMod"));
	inline static const FString PrimitiveFilename = FString(TEXT("HotReloadDelegateParameterPrimitiveMod.as"));
	inline static const FString PrimitiveDelegateName = FString(TEXT("FHotReloadPrimitiveSignal"));

	inline static const FName NativeStructModuleName = FName(TEXT("HotReloadDelegateParameterNativeStructMod"));
	inline static const FString NativeStructFilename = FString(TEXT("HotReloadDelegateParameterNativeStructMod.as"));
	inline static const FString NativeStructDelegateName = FString(TEXT("FHotReloadNativeStructSignal"));

	inline static const FName ContainerModuleName = FName(TEXT("HotReloadDelegateParameterContainerMod"));
	inline static const FString ContainerFilename = FString(TEXT("HotReloadDelegateParameterContainerMod.as"));
	inline static const FString ContainerDelegateName = FString(TEXT("FHotReloadContainerSignal"));

	inline static const FName ReferenceModuleName = FName(TEXT("HotReloadDelegateParameterReferenceMod"));
	inline static const FString ReferenceFilename = FString(TEXT("HotReloadDelegateParameterReferenceMod.as"));
	inline static const FString ReferenceDelegateName = FString(TEXT("FHotReloadReferenceSignal"));

	inline static const FName ScriptStructModuleName = FName(TEXT("HotReloadDelegateParameterScriptStructMod"));
	inline static const FString ScriptStructFilename = FString(TEXT("HotReloadDelegateParameterScriptStructMod.as"));
	inline static const FString ScriptStructDelegateName = FString(TEXT("FHotReloadScriptStructSignal"));
	inline static const FString ScriptStructName = FString(TEXT("FHotReloadScriptPayload"));

	struct FDelegateParameterReloadObservation
	{
		int32 DelegateReloadCount = 0;
		UDelegateFunction* OldDelegateSeenDuringReload = nullptr;
		UDelegateFunction* NewDelegateSeenDuringReload = nullptr;
	};

	struct FScriptStructReloadObservation
	{
		int32 DelegateReloadCount = 0;
		int32 StructReloadCount = 0;
		UDelegateFunction* OldDelegateSeenDuringReload = nullptr;
		UDelegateFunction* NewDelegateSeenDuringReload = nullptr;
		UScriptStruct* OldStructSeenDuringReload = nullptr;
		UScriptStruct* NewStructSeenDuringReload = nullptr;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool CompileReload(
		FAngelscriptEngine& Engine,
		ECompileType CompileType,
		FName ModuleName,
		const FString& Filename,
		const FString& Source,
		ECompileResult& OutReloadResult)
	{
		if (!CompileModuleWithResult(&Engine, CompileType, ModuleName, Filename, Source, OutReloadResult))
		{
			return false;
		}

		return IsHandledReloadResult(OutReloadResult);
	}

	static asIScriptModule* FindScriptModule(FAngelscriptEngine& Engine, FName ModuleName)
	{
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ModuleName.ToString());
		return ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	}

	static bool ExecuteIntGlobal(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const TCHAR* Declaration,
		UObject* Receiver,
		const int32 ExpectedResult,
		const TCHAR* Context)
	{
		asIScriptModule* Module = FindScriptModule(Engine, ModuleName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Module, TEXT("Delegate parameter reload should expose a script module")))
		{
			return false;
		}

		FAngelscriptTestExecutor Executor(Test, Engine, *Module, Declaration);
		if (!LocalAssert.IsTrue(Executor.IsValid(), TEXT("Delegate parameter reload should resolve the global entry function")))
		{
			return false;
		}

		const int32 Actual = Executor
			.AddArgObject(Receiver)
			.ExecuteAndGet<int32>(INDEX_NONE);
		return LocalAssert.AreEqual(ExpectedResult, Actual, Context);
	}

	static UObject* CreateReceiverObject(FAutomationTestBase& Test, UClass* ReceiverClass, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(ReceiverClass, Context))
		{
			return nullptr;
		}

		UObject* Receiver = NewObject<UObject>(GetTransientPackage(), ReceiverClass);
		if (!LocalAssert.IsNotNull(Receiver, TEXT("Delegate parameter reload should create a receiver instance")))
		{
			return nullptr;
		}

		return Receiver;
	}

	static bool ValidateDelegateProperty(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc,
		const TCHAR* ParamName,
		const FFieldClass* ExpectedPropertyClass,
		const TCHAR* Context)
	{
		FProperty* Property = FindFProperty<FProperty>(DelegateDesc.Function, ParamName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Property, Context))
		{
			return false;
		}

		return LocalAssert.IsTrue(Property->IsA(ExpectedPropertyClass), Context);
	}

	static bool ValidateStructDelegateProperty(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc,
		const TCHAR* ParamName,
		const FName ExpectedStructName,
		const TCHAR* Context)
	{
		FStructProperty* Property = FindFProperty<FStructProperty>(DelegateDesc.Function, ParamName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Property, Context))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(Property->Struct, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedStructName, Property->Struct->GetFName(), Context);
	}

	static bool ValidateObjectDelegateProperty(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc,
		const TCHAR* ParamName,
		UClass* ExpectedClass,
		const TCHAR* Context)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(DelegateDesc.Function, ParamName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Property, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedClass, Property->PropertyClass, Context);
	}

	static bool ValidateClassDelegateProperty(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc,
		const TCHAR* ParamName,
		UClass* ExpectedMetaClass,
		const TCHAR* Context)
	{
		FClassProperty* Property = FindFProperty<FClassProperty>(DelegateDesc.Function, ParamName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Property, Context))
		{
			return false;
		}

		if (ExpectedMetaClass == nullptr)
		{
			return true;
		}

		return LocalAssert.AreEqual(ExpectedMetaClass, Property->MetaClass, Context);
	}

	static bool ValidateSoftObjectDelegateProperty(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc,
		const TCHAR* ParamName,
		UClass* ExpectedClass,
		const TCHAR* Context)
	{
		FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(DelegateDesc.Function, ParamName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Property, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedClass, Property->PropertyClass, Context);
	}

	static bool ValidateSoftClassDelegateProperty(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc,
		const TCHAR* ParamName,
		UClass* ExpectedMetaClass,
		const TCHAR* Context)
	{
		FSoftClassProperty* Property = FindFProperty<FSoftClassProperty>(DelegateDesc.Function, ParamName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Property, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedMetaClass, Property->MetaClass, Context);
	}

	static bool ValidateContainerDelegateProperties(
		FAutomationTestBase& Test,
		const FAngelscriptDelegateDesc& DelegateDesc)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FArrayProperty* VectorArray = FindFProperty<FArrayProperty>(DelegateDesc.Function, TEXT("Vectors"));
		FMapProperty* Scores = FindFProperty<FMapProperty>(DelegateDesc.Function, TEXT("Scores"));
		FSetProperty* Tags = FindFProperty<FSetProperty>(DelegateDesc.Function, TEXT("Tags"));

		bool bPassed = true;
		bPassed &= LocalAssert.IsNotNull(VectorArray, TEXT("Container delegate should expose Vectors array parameter"));
		bPassed &= LocalAssert.IsNotNull(Scores, TEXT("Container delegate should expose Scores map parameter"));
		bPassed &= LocalAssert.IsNotNull(Tags, TEXT("Container delegate should expose Tags set parameter"));
		if (!bPassed)
		{
			return false;
		}

		bPassed &= LocalAssert.IsTrue(VectorArray->Inner->IsA(FStructProperty::StaticClass()), TEXT("Vectors should be an array of structs"));
		bPassed &= LocalAssert.IsTrue(Scores->KeyProp->IsA(FStrProperty::StaticClass()), TEXT("Scores should use FString keys"));
		bPassed &= LocalAssert.IsTrue(Scores->ValueProp->IsA(FIntProperty::StaticClass()), TEXT("Scores should use int values"));
		bPassed &= LocalAssert.IsTrue(Tags->ElementProp->IsA(FNameProperty::StaticClass()), TEXT("Tags should use FName elements"));
		return bPassed;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(InvokePrimitiveSignatureAfterParameterExpansion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FDelegateParameterReloadObservation Observation;
			FDelegateHandle DelegateReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
				Engine.DiscardModule(*PrimitiveModuleName.ToString());
			};

			const FString PrimitiveReloadV1Source = ASTEST_AS(R"AS(
				delegate int FHotReloadPrimitiveSignal(int Value);

				UCLASS()
				class UHotReloadPrimitiveReceiver : UObject
				{
					UFUNCTION()
					int HandlePrimitive(int Value)
					{
						int Result = Value + 10;
						Log(n"HotReloadDelegateTests", "Primitive V1 HandlePrimitive Value=" + Value + " Result=" + Result);
						return Result;
					}
				}

				int RunPrimitive(UHotReloadPrimitiveReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "Primitive V1 RunPrimitive: binding HandlePrimitive");
					FHotReloadPrimitiveSignal Signal;
					Signal.BindUFunction(Receiver, n"HandlePrimitive");
					int Result = Signal.Execute(5);
					Log(n"HotReloadDelegateTests", "Primitive V1 RunPrimitive Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, PrimitiveModuleName, PrimitiveFilename, PrimitiveReloadV1Source),
				TEXT("Primitive delegate parameter V1 compile should succeed")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(PrimitiveDelegateName);
			ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Primitive delegate metadata should exist before reload")));
			ASSERT_THAT(IsNotNull(DelegateBeforeReload->Function, TEXT("Primitive delegate function should exist before reload")));
			ASSERT_THAT(IsNull(FindFProperty<FProperty>(DelegateBeforeReload->Function, TEXT("Label")), TEXT("Primitive V1 delegate should not expose the future Label parameter")));

			UClass* ReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadPrimitiveReceiver"));
			UObject* Receiver = CreateReceiverObject(*TestRunner, ReceiverClass, TEXT("Primitive delegate receiver class should exist before reload"));
			ASSERT_THAT(IsNotNull(Receiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				PrimitiveModuleName,
				TEXT("int RunPrimitive(UHotReloadPrimitiveReceiver)"),
				Receiver,
				15,
				TEXT("Primitive V1 delegate invocation should return the baseline result"))));

			DelegateReloadHandle = Engine.GetOnDelegateReload().AddLambda(
				[&Observation](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
				{
					++Observation.DelegateReloadCount;
					Observation.OldDelegateSeenDuringReload = OldDelegate;
					Observation.NewDelegateSeenDuringReload = NewDelegate;
				});

			const FString PrimitiveReloadV2Source = ASTEST_AS(R"AS(
				delegate int FHotReloadPrimitiveSignal(int Value, bool bEnabled, float Scale, const FString& Label, FName Tag);

				UCLASS()
				class UHotReloadPrimitiveReceiver : UObject
				{
					UFUNCTION()
					int HandlePrimitive(int Value, bool bEnabled, float Scale, const FString& Label, FName Tag)
					{
						Log(n"HotReloadDelegateTests", "Primitive V2 HandlePrimitive Value=" + Value + " bEnabled=" + bEnabled + " Scale=" + Scale + " Label=" + Label + " Tag=" + Tag);
						int Result = Value;
						if (bEnabled)
						{
							Result += int(Scale * 10.0);
						}

						Result += Label.Len();
						if (Tag == FName("Ready"))
						{
							Result += 100;
						}

						Log(n"HotReloadDelegateTests", "Primitive V2 HandlePrimitive Result=" + Result);
						return Result;
					}
				}

				int RunPrimitive(UHotReloadPrimitiveReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "Primitive V2 RunPrimitive: binding HandlePrimitive");
					FHotReloadPrimitiveSignal Signal;
					Signal.BindUFunction(Receiver, n"HandlePrimitive");
					int Result = Signal.Execute(7, true, 2.5f, "Alpha", FName("Ready"));
					Log(n"HotReloadDelegateTests", "Primitive V2 RunPrimitive Result=" + Result);
					return Result;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileReload(Engine, ECompileType::FullReload, PrimitiveModuleName, PrimitiveFilename, PrimitiveReloadV2Source, ReloadResult),
				TEXT("Primitive delegate parameter expansion should compile and reload")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(PrimitiveDelegateName);
			ASSERT_THAT(IsTrue(DelegateAfterReload.IsValid(), TEXT("Primitive delegate metadata should exist after reload")));
			ASSERT_THAT(IsNotNull(DelegateAfterReload->Function, TEXT("Primitive delegate function should exist after reload")));
			ASSERT_THAT(AreEqual(1, Observation.DelegateReloadCount, TEXT("Primitive delegate reload should broadcast exactly once")));
			ASSERT_THAT(AreEqual(DelegateBeforeReload->Function, Observation.OldDelegateSeenDuringReload, TEXT("Primitive delegate reload should expose old delegate")));
			ASSERT_THAT(AreEqual(DelegateAfterReload->Function, Observation.NewDelegateSeenDuringReload, TEXT("Primitive delegate reload should expose new delegate")));
			ASSERT_THAT(AreNotEqual(Observation.OldDelegateSeenDuringReload, Observation.NewDelegateSeenDuringReload, TEXT("Primitive delegate reload should replace the delegate function")));
			ASSERT_THAT(IsTrue(ValidateDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("bEnabled"), FBoolProperty::StaticClass(), TEXT("Primitive delegate should expose bool parameter"))));
			ASSERT_THAT(IsTrue(ValidateDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Scale"), FDoubleProperty::StaticClass(), TEXT("Primitive delegate should expose float parameter as double property"))));
			ASSERT_THAT(IsTrue(ValidateDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Label"), FStrProperty::StaticClass(), TEXT("Primitive delegate should expose FString parameter"))));
			ASSERT_THAT(IsTrue(ValidateDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Tag"), FNameProperty::StaticClass(), TEXT("Primitive delegate should expose FName parameter"))));

			UClass* ReloadedReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadPrimitiveReceiver"));
			UObject* ReloadedReceiver = CreateReceiverObject(*TestRunner, ReloadedReceiverClass, TEXT("Primitive delegate receiver class should exist after reload"));
			ASSERT_THAT(IsNotNull(ReloadedReceiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				PrimitiveModuleName,
				TEXT("int RunPrimitive(UHotReloadPrimitiveReceiver)"),
				ReloadedReceiver,
				137,
				TEXT("Primitive V2 delegate invocation should receive every expanded parameter"))));
		}
	}

	TEST_METHOD(InvokeNativeStructSignatureAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FDelegateParameterReloadObservation Observation;
			FDelegateHandle DelegateReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
				Engine.DiscardModule(*NativeStructModuleName.ToString());
			};

			const FString NativeStructReloadV1Source = ASTEST_AS(R"AS(
				delegate int FHotReloadNativeStructSignal(FVector Location);

				UCLASS()
				class UHotReloadNativeStructReceiver : UObject
				{
					UFUNCTION()
					int HandleStructs(FVector Location)
					{
						int Result = int(Location.X + Location.Y + Location.Z);
						Log(n"HotReloadDelegateTests", "NativeStruct V1 HandleStructs Location=" + Location + " Result=" + Result);
						return Result;
					}
				}

				int RunNativeStruct(UHotReloadNativeStructReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "NativeStruct V1 RunNativeStruct: binding HandleStructs");
					FHotReloadNativeStructSignal Signal;
					Signal.BindUFunction(Receiver, n"HandleStructs");
					int Result = Signal.Execute(FVector(1.0, 2.0, 3.0));
					Log(n"HotReloadDelegateTests", "NativeStruct V1 RunNativeStruct Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, NativeStructModuleName, NativeStructFilename, NativeStructReloadV1Source),
				TEXT("Native struct delegate parameter V1 compile should succeed")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(NativeStructDelegateName);
			ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Native struct delegate metadata should exist before reload")));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateBeforeReload, TEXT("Location"), FName(TEXT("Vector")), TEXT("Native struct V1 delegate should expose FVector parameter"))));

			UClass* ReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadNativeStructReceiver"));
			UObject* Receiver = CreateReceiverObject(*TestRunner, ReceiverClass, TEXT("Native struct delegate receiver class should exist before reload"));
			ASSERT_THAT(IsNotNull(Receiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				NativeStructModuleName,
				TEXT("int RunNativeStruct(UHotReloadNativeStructReceiver)"),
				Receiver,
				6,
				TEXT("Native struct V1 delegate invocation should return the baseline result"))));

			DelegateReloadHandle = Engine.GetOnDelegateReload().AddLambda(
				[&Observation](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
				{
					++Observation.DelegateReloadCount;
					Observation.OldDelegateSeenDuringReload = OldDelegate;
					Observation.NewDelegateSeenDuringReload = NewDelegate;
				});

			const FString NativeStructReloadV2Source = ASTEST_AS(R"AS(
				delegate int FHotReloadNativeStructSignal(FVector Location, FVector2D Offset, FTransform Transform, FLinearColor Tint, FColor PackedColor, FGuid Id);

				UCLASS()
				class UHotReloadNativeStructReceiver : UObject
				{
					UFUNCTION()
					int HandleStructs(FVector Location, FVector2D Offset, FTransform Transform, FLinearColor Tint, FColor PackedColor, FGuid Id)
					{
						FVector Translation = Transform.GetTranslation();
						Log(n"HotReloadDelegateTests", "NativeStruct V2 HandleStructs Location=" + Location + " Offset=" + Offset + " Translation=" + Translation + " Tint=" + Tint + " PackedColor=" + PackedColor + " Id=" + Id.ToString());
						int Result = int(
							Location.X + Location.Y + Location.Z +
							Offset.X + Offset.Y +
							Translation.X + Translation.Y + Translation.Z +
							Tint.R * 10.0f + Tint.G * 10.0f + Tint.B * 10.0f +
							PackedColor.R + PackedColor.G + PackedColor.B +
							Id[0] + Id[1] + Id[2] + Id[3]);
						Log(n"HotReloadDelegateTests", "NativeStruct V2 HandleStructs Result=" + Result);
						return Result;
					}
				}

				int RunNativeStruct(UHotReloadNativeStructReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "NativeStruct V2 RunNativeStruct: binding HandleStructs");
					FHotReloadNativeStructSignal Signal;
					Signal.BindUFunction(Receiver, n"HandleStructs");
					int Result = Signal.Execute(
						FVector(1.0, 2.0, 3.0),
						FVector2D(4.0, 5.0),
						FTransform(FVector(6.0, 7.0, 8.0)),
						FLinearColor(0.1f, 0.2f, 0.3f, 1.0f),
						FColor(9, 10, 11, 255),
						FGuid(12, 13, 14, 15));
					Log(n"HotReloadDelegateTests", "NativeStruct V2 RunNativeStruct Result=" + Result);
					return Result;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileReload(Engine, ECompileType::FullReload, NativeStructModuleName, NativeStructFilename, NativeStructReloadV2Source, ReloadResult),
				TEXT("Native struct delegate parameter expansion should compile and reload")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(NativeStructDelegateName);
			ASSERT_THAT(IsTrue(DelegateAfterReload.IsValid(), TEXT("Native struct delegate metadata should exist after reload")));
			ASSERT_THAT(AreEqual(1, Observation.DelegateReloadCount, TEXT("Native struct delegate reload should broadcast exactly once")));
			ASSERT_THAT(AreEqual(DelegateBeforeReload->Function, Observation.OldDelegateSeenDuringReload, TEXT("Native struct delegate reload should expose old delegate")));
			ASSERT_THAT(AreEqual(DelegateAfterReload->Function, Observation.NewDelegateSeenDuringReload, TEXT("Native struct delegate reload should expose new delegate")));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Location"), FName(TEXT("Vector")), TEXT("Native struct delegate should expose FVector parameter"))));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Offset"), FName(TEXT("Vector2D")), TEXT("Native struct delegate should expose FVector2D parameter"))));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Transform"), FName(TEXT("Transform")), TEXT("Native struct delegate should expose FTransform parameter"))));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Tint"), FName(TEXT("LinearColor")), TEXT("Native struct delegate should expose FLinearColor parameter"))));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("PackedColor"), FName(TEXT("Color")), TEXT("Native struct delegate should expose FColor parameter"))));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Id"), FName(TEXT("Guid")), TEXT("Native struct delegate should expose FGuid parameter"))));

			UClass* ReloadedReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadNativeStructReceiver"));
			UObject* ReloadedReceiver = CreateReceiverObject(*TestRunner, ReloadedReceiverClass, TEXT("Native struct delegate receiver class should exist after reload"));
			ASSERT_THAT(IsNotNull(ReloadedReceiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				NativeStructModuleName,
				TEXT("int RunNativeStruct(UHotReloadNativeStructReceiver)"),
				ReloadedReceiver,
				126,
				TEXT("Native struct V2 delegate invocation should receive every struct parameter"))));
		}
	}

	TEST_METHOD(InvokeContainerSignatureAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FDelegateParameterReloadObservation Observation;
			FDelegateHandle DelegateReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
				Engine.DiscardModule(*ContainerModuleName.ToString());
			};

			const FString ContainerReloadV1Source = ASTEST_AS(R"AS(
				delegate int FHotReloadContainerSignal(TArray<int> Values);

				UCLASS()
				class UHotReloadContainerReceiver : UObject
				{
					UFUNCTION()
					int HandleContainers(TArray<int> Values)
					{
						int Result = Values.Num() + Values[0] + Values[1];
						Log(n"HotReloadDelegateTests", "Container V1 HandleContainers Values.Num=" + Values.Num() + " First=" + Values[0] + " Second=" + Values[1] + " Result=" + Result);
						return Result;
					}
				}

				int RunContainers(UHotReloadContainerReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "Container V1 RunContainers: building Values");
					TArray<int> Values;
					Values.Add(3);
					Values.Add(4);

					FHotReloadContainerSignal Signal;
					Signal.BindUFunction(Receiver, n"HandleContainers");
					int Result = Signal.Execute(Values);
					Log(n"HotReloadDelegateTests", "Container V1 RunContainers Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, ContainerModuleName, ContainerFilename, ContainerReloadV1Source),
				TEXT("Container delegate parameter V1 compile should succeed")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(ContainerDelegateName);
			ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Container delegate metadata should exist before reload")));
			ASSERT_THAT(IsTrue(ValidateDelegateProperty(*TestRunner, *DelegateBeforeReload, TEXT("Values"), FArrayProperty::StaticClass(), TEXT("Container V1 delegate should expose TArray parameter"))));

			UClass* ReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadContainerReceiver"));
			UObject* Receiver = CreateReceiverObject(*TestRunner, ReceiverClass, TEXT("Container delegate receiver class should exist before reload"));
			ASSERT_THAT(IsNotNull(Receiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				ContainerModuleName,
				TEXT("int RunContainers(UHotReloadContainerReceiver)"),
				Receiver,
				9,
				TEXT("Container V1 delegate invocation should return the baseline result"))));

			DelegateReloadHandle = Engine.GetOnDelegateReload().AddLambda(
				[&Observation](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
				{
					++Observation.DelegateReloadCount;
					Observation.OldDelegateSeenDuringReload = OldDelegate;
					Observation.NewDelegateSeenDuringReload = NewDelegate;
				});

			const FString ContainerReloadV2Source = ASTEST_AS(R"AS(
				delegate int FHotReloadContainerSignal(TArray<int> Values, TArray<FVector> Vectors, TMap<FString, int> Scores, TSet<FName> Tags);

				UCLASS()
				class UHotReloadContainerReceiver : UObject
				{
					UFUNCTION()
					int HandleContainers(TArray<int> Values, TArray<FVector> Vectors, TMap<FString, int> Scores, TSet<FName> Tags)
					{
						Log(n"HotReloadDelegateTests", "Container V2 HandleContainers Values.Num=" + Values.Num() + " Vectors.Num=" + Vectors.Num() + " Scores.Num=" + Scores.Num() + " Tags.Num=" + Tags.Num());
						int AlphaScore = 0;
						Scores.Find("Alpha", AlphaScore);

						int BetaScore = 0;
						Scores.Find("Beta", BetaScore);

						int Result = Values.Num() + Values[0] + Values[1];
						Result += int(Vectors[0].X + Vectors[1].Y);
						Result += AlphaScore + BetaScore;
						Result += Tags.Contains(FName("Ready")) ? 100 : 0;
						Result += Tags.Contains(FName("Missing")) ? 1000 : 0;
						Log(n"HotReloadDelegateTests", "Container V2 HandleContainers AlphaScore=" + AlphaScore + " BetaScore=" + BetaScore + " HasReady=" + Tags.Contains(FName("Ready")) + " HasMissing=" + Tags.Contains(FName("Missing")) + " Result=" + Result);
						return Result;
					}
				}

				int RunContainers(UHotReloadContainerReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "Container V2 RunContainers: building containers");
					TArray<int> Values;
					Values.Add(5);
					Values.Add(6);

					TArray<FVector> Vectors;
					Vectors.Add(FVector(7.0, 0.0, 0.0));
					Vectors.Add(FVector(0.0, 8.0, 0.0));

					TMap<FString, int> Scores;
					Scores.Add("Alpha", 9);
					Scores.Add("Beta", 10);

					TSet<FName> Tags;
					Tags.Add(FName("Ready"));
					Tags.Add(FName("Live"));

					FHotReloadContainerSignal Signal;
					Signal.BindUFunction(Receiver, n"HandleContainers");
					int Result = Signal.Execute(Values, Vectors, Scores, Tags);
					Log(n"HotReloadDelegateTests", "Container V2 RunContainers Result=" + Result);
					return Result;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileReload(Engine, ECompileType::FullReload, ContainerModuleName, ContainerFilename, ContainerReloadV2Source, ReloadResult),
				TEXT("Container delegate parameter expansion should compile and reload")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(ContainerDelegateName);
			ASSERT_THAT(IsTrue(DelegateAfterReload.IsValid(), TEXT("Container delegate metadata should exist after reload")));
			ASSERT_THAT(AreEqual(1, Observation.DelegateReloadCount, TEXT("Container delegate reload should broadcast exactly once")));
			ASSERT_THAT(AreEqual(DelegateBeforeReload->Function, Observation.OldDelegateSeenDuringReload, TEXT("Container delegate reload should expose old delegate")));
			ASSERT_THAT(AreEqual(DelegateAfterReload->Function, Observation.NewDelegateSeenDuringReload, TEXT("Container delegate reload should expose new delegate")));
			ASSERT_THAT(IsTrue(ValidateContainerDelegateProperties(*TestRunner, *DelegateAfterReload)));

			UClass* ReloadedReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadContainerReceiver"));
			UObject* ReloadedReceiver = CreateReceiverObject(*TestRunner, ReloadedReceiverClass, TEXT("Container delegate receiver class should exist after reload"));
			ASSERT_THAT(IsNotNull(ReloadedReceiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				ContainerModuleName,
				TEXT("int RunContainers(UHotReloadContainerReceiver)"),
				ReloadedReceiver,
				147,
				TEXT("Container V2 delegate invocation should receive every container parameter"))));
		}
	}

	TEST_METHOD(InvokeReferenceSignatureAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FDelegateParameterReloadObservation Observation;
			FDelegateHandle DelegateReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
				Engine.DiscardModule(*ReferenceModuleName.ToString());
			};

			const FString ReferenceReloadV1Source = ASTEST_AS(R"AS(
				delegate int FHotReloadReferenceSignal(UTexture2D Texture);

				UCLASS()
				class UHotReloadReferenceReceiver : UObject
				{
					UFUNCTION()
					int HandleReferences(UTexture2D Texture)
					{
						int Result = IsValid(Texture) ? 17 : 0;
						Log(n"HotReloadDelegateTests", "Reference V1 HandleReferences TextureValid=" + IsValid(Texture) + " Result=" + Result);
						return Result;
					}
				}

				int RunReferences(UHotReloadReferenceReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "Reference V1 RunReferences: creating texture");
					UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));

					FHotReloadReferenceSignal Signal;
					Signal.BindUFunction(Receiver, n"HandleReferences");
					int Result = Signal.Execute(Texture);
					Log(n"HotReloadDelegateTests", "Reference V1 RunReferences Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, ReferenceModuleName, ReferenceFilename, ReferenceReloadV1Source),
				TEXT("Reference delegate parameter V1 compile should succeed")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(ReferenceDelegateName);
			ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Reference delegate metadata should exist before reload")));
			ASSERT_THAT(IsTrue(ValidateObjectDelegateProperty(*TestRunner, *DelegateBeforeReload, TEXT("Texture"), UTexture2D::StaticClass(), TEXT("Reference V1 delegate should expose UTexture2D parameter"))));

			UClass* ReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadReferenceReceiver"));
			UObject* Receiver = CreateReceiverObject(*TestRunner, ReceiverClass, TEXT("Reference delegate receiver class should exist before reload"));
			ASSERT_THAT(IsNotNull(Receiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				ReferenceModuleName,
				TEXT("int RunReferences(UHotReloadReferenceReceiver)"),
				Receiver,
				17,
				TEXT("Reference V1 delegate invocation should receive the object parameter"))));

			DelegateReloadHandle = Engine.GetOnDelegateReload().AddLambda(
				[&Observation](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
				{
					++Observation.DelegateReloadCount;
					Observation.OldDelegateSeenDuringReload = OldDelegate;
					Observation.NewDelegateSeenDuringReload = NewDelegate;
				});

			const FString ReferenceReloadV2Source = ASTEST_AS(R"AS(
				delegate int FHotReloadReferenceSignal(UTexture2D Texture, UClass TextureClass, TSubclassOf<AActor> ActorClass, TSoftObjectPtr<UTexture2D> SoftTexture, TSoftClassPtr<AActor> SoftActorClass);

				UCLASS()
				class UHotReloadReferenceReceiver : UObject
				{
					UFUNCTION()
					int HandleReferences(UTexture2D Texture, UClass TextureClass, TSubclassOf<AActor> ActorClass, TSoftObjectPtr<UTexture2D> SoftTexture, TSoftClassPtr<AActor> SoftActorClass)
					{
						TSubclassOf<AActor> ResolvedSoftActorClass = SoftActorClass.Get();
						FString TextureClassName = TextureClass != null ? TextureClass.GetName().ToString() : "null";
						FString ActorClassName = ActorClass.Get() != null ? ActorClass.Get().GetName().ToString() : "null";
						FString SoftActorClassName = ResolvedSoftActorClass.Get() != null ? ResolvedSoftActorClass.Get().GetName().ToString() : "null";
						Log(n"HotReloadDelegateTests", "Reference V2 HandleReferences TextureValid=" + IsValid(Texture) + " TextureClass=" + TextureClassName + " ActorClass=" + ActorClassName + " SoftTextureValid=" + IsValid(SoftTexture.Get()) + " SoftActorClass=" + SoftActorClassName);
						int Result = 0;

						if (IsValid(Texture))
						{
							Result += 1;
						}

						if (TextureClass == UTexture2D::StaticClass())
						{
							Result += 2;
						}

						if (ActorClass == ACameraActor::StaticClass())
						{
							Result += 4;
						}

						AActor DefaultActor = ActorClass.GetDefaultObject();
						if (IsValid(DefaultActor) && DefaultActor.IsA(ACameraActor::StaticClass()))
						{
							Result += 8;
						}

						if (SoftTexture == Texture && SoftTexture.Get() == Texture)
						{
							Result += 16;
						}

						if (SoftActorClass.Get() == AActor::StaticClass())
						{
							Result += 32;
						}

						Log(n"HotReloadDelegateTests", "Reference V2 HandleReferences Result=" + Result);
						return Result;
					}
				}

				int RunReferences(UHotReloadReferenceReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "Reference V2 RunReferences: creating reference parameters");
					UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
					TSubclassOf<AActor> ActorClass = ACameraActor::StaticClass();
					TSoftObjectPtr<UTexture2D> SoftTexture(Texture);
					TSoftClassPtr<AActor> SoftActorClass(AActor::StaticClass());

					FHotReloadReferenceSignal Signal;
					Signal.BindUFunction(Receiver, n"HandleReferences");
					int Result = Signal.Execute(Texture, UTexture2D::StaticClass(), ActorClass, SoftTexture, SoftActorClass);
					Log(n"HotReloadDelegateTests", "Reference V2 RunReferences Result=" + Result);
					return Result;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileReload(Engine, ECompileType::FullReload, ReferenceModuleName, ReferenceFilename, ReferenceReloadV2Source, ReloadResult),
				TEXT("Reference delegate parameter expansion should compile and reload")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(ReferenceDelegateName);
			ASSERT_THAT(IsTrue(DelegateAfterReload.IsValid(), TEXT("Reference delegate metadata should exist after reload")));
			ASSERT_THAT(AreEqual(1, Observation.DelegateReloadCount, TEXT("Reference delegate reload should broadcast exactly once")));
			ASSERT_THAT(AreEqual(DelegateBeforeReload->Function, Observation.OldDelegateSeenDuringReload, TEXT("Reference delegate reload should expose old delegate")));
			ASSERT_THAT(AreEqual(DelegateAfterReload->Function, Observation.NewDelegateSeenDuringReload, TEXT("Reference delegate reload should expose new delegate")));
			ASSERT_THAT(IsTrue(ValidateObjectDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Texture"), UTexture2D::StaticClass(), TEXT("Reference delegate should expose UTexture2D parameter"))));
			ASSERT_THAT(IsTrue(ValidateClassDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("TextureClass"), nullptr, TEXT("Reference delegate should expose UClass parameter"))));
			ASSERT_THAT(IsTrue(ValidateClassDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("ActorClass"), AActor::StaticClass(), TEXT("Reference delegate should expose TSubclassOf<AActor> parameter"))));
			ASSERT_THAT(IsTrue(ValidateSoftObjectDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("SoftTexture"), UTexture2D::StaticClass(), TEXT("Reference delegate should expose TSoftObjectPtr<UTexture2D> parameter"))));
			ASSERT_THAT(IsTrue(ValidateSoftClassDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("SoftActorClass"), AActor::StaticClass(), TEXT("Reference delegate should expose TSoftClassPtr<AActor> parameter"))));

			UClass* ReloadedReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadReferenceReceiver"));
			UObject* ReloadedReceiver = CreateReceiverObject(*TestRunner, ReloadedReceiverClass, TEXT("Reference delegate receiver class should exist after reload"));
			ASSERT_THAT(IsNotNull(ReloadedReceiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				ReferenceModuleName,
				TEXT("int RunReferences(UHotReloadReferenceReceiver)"),
				ReloadedReceiver,
				63,
				TEXT("Reference V2 delegate invocation should receive object, class, and soft-reference parameters"))));
		}
	}

	TEST_METHOD(InvokeScriptStructSignatureAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{
			FAngelscriptEngineScope AutoEngineScope(Engine);

			FScriptStructReloadObservation Observation;
			FDelegateHandle DelegateReloadHandle;
			FDelegateHandle StructReloadHandle;

			ON_SCOPE_EXIT
			{
				Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
				Engine.GetOnStructReload().Remove(StructReloadHandle);
				Engine.DiscardModule(*ScriptStructModuleName.ToString());
			};

			const FString ScriptStructReloadV1Source = ASTEST_AS(R"AS(
				USTRUCT()
				struct FHotReloadScriptPayload
				{
					UPROPERTY()
					int Value = 0;
				}

				delegate int FHotReloadScriptStructSignal(const FHotReloadScriptPayload& Payload);

				UCLASS()
				class UHotReloadScriptStructReceiver : UObject
				{
					UFUNCTION()
					int HandlePayload(const FHotReloadScriptPayload& Payload)
					{
						Log(n"HotReloadDelegateTests", "ScriptStruct V1 HandlePayload Value=" + Payload.Value);
						return Payload.Value;
					}
				}

				int RunPayload(UHotReloadScriptStructReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "ScriptStruct V1 RunPayload: building payload");
					FHotReloadScriptPayload Payload;
					Payload.Value = 12;

					FHotReloadScriptStructSignal Signal;
					Signal.BindUFunction(Receiver, n"HandlePayload");
					int Result = Signal.Execute(Payload);
					Log(n"HotReloadDelegateTests", "ScriptStruct V1 RunPayload Result=" + Result);
					return Result;
				}
				)AS");

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, ScriptStructModuleName, ScriptStructFilename, ScriptStructReloadV1Source),
				TEXT("Script struct delegate parameter V1 compile should succeed")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateBeforeReload = Engine.GetDelegate(ScriptStructDelegateName);
			const TSharedPtr<FAngelscriptClassDesc> StructBeforeReload = Engine.GetClass(ScriptStructName);
			ASSERT_THAT(IsTrue(DelegateBeforeReload.IsValid(), TEXT("Script struct delegate metadata should exist before reload")));
			ASSERT_THAT(IsTrue(StructBeforeReload.IsValid(), TEXT("Script struct metadata should exist before reload")));
			UScriptStruct* StructObjectBeforeReload = static_cast<UScriptStruct*>(StructBeforeReload->Struct);
			ASSERT_THAT(IsNotNull(StructObjectBeforeReload, TEXT("Script struct object should exist before reload")));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateBeforeReload, TEXT("Payload"), FName(TEXT("HotReloadScriptPayload")), TEXT("Script struct delegate should expose payload parameter before reload"))));

			UClass* ReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadScriptStructReceiver"));
			UObject* Receiver = CreateReceiverObject(*TestRunner, ReceiverClass, TEXT("Script struct delegate receiver class should exist before reload"));
			ASSERT_THAT(IsNotNull(Receiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				ScriptStructModuleName,
				TEXT("int RunPayload(UHotReloadScriptStructReceiver)"),
				Receiver,
				12,
				TEXT("Script struct V1 delegate invocation should return the baseline result"))));

			DelegateReloadHandle = Engine.GetOnDelegateReload().AddLambda(
				[&Observation](UDelegateFunction* OldDelegate, UDelegateFunction* NewDelegate)
				{
					++Observation.DelegateReloadCount;
					Observation.OldDelegateSeenDuringReload = OldDelegate;
					Observation.NewDelegateSeenDuringReload = NewDelegate;
				});

			StructReloadHandle = Engine.GetOnStructReload().AddLambda(
				[&Observation](UScriptStruct* OldStruct, UScriptStruct* NewStruct)
				{
					++Observation.StructReloadCount;
					Observation.OldStructSeenDuringReload = OldStruct;
					Observation.NewStructSeenDuringReload = NewStruct;
				});

			const FString ScriptStructReloadV2Source = ASTEST_AS(R"AS(
				USTRUCT()
				struct FHotReloadScriptPayload
				{
					UPROPERTY()
					int Value = 0;

					UPROPERTY()
					int Bonus = 0;
				}

				delegate int FHotReloadScriptStructSignal(const FHotReloadScriptPayload& Payload);

				UCLASS()
				class UHotReloadScriptStructReceiver : UObject
				{
					UFUNCTION()
					int HandlePayload(const FHotReloadScriptPayload& Payload)
					{
						int Result = Payload.Value + Payload.Bonus;
						Log(n"HotReloadDelegateTests", "ScriptStruct V2 HandlePayload Value=" + Payload.Value + " Bonus=" + Payload.Bonus + " Result=" + Result);
						return Result;
					}
				}

				int RunPayload(UHotReloadScriptStructReceiver Receiver)
				{
					Log(n"HotReloadDelegateTests", "ScriptStruct V2 RunPayload: building payload");
					FHotReloadScriptPayload Payload;
					Payload.Value = 20;
					Payload.Bonus = 22;

					FHotReloadScriptStructSignal Signal;
					Signal.BindUFunction(Receiver, n"HandlePayload");
					int Result = Signal.Execute(Payload);
					Log(n"HotReloadDelegateTests", "ScriptStruct V2 RunPayload Result=" + Result);
					return Result;
				}
				)AS");

			ECompileResult ReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileReload(Engine, ECompileType::FullReload, ScriptStructModuleName, ScriptStructFilename, ScriptStructReloadV2Source, ReloadResult),
				TEXT("Script struct delegate parameter expansion should compile and reload")));

			const TSharedPtr<FAngelscriptDelegateDesc> DelegateAfterReload = Engine.GetDelegate(ScriptStructDelegateName);
			const TSharedPtr<FAngelscriptClassDesc> StructAfterReload = Engine.GetClass(ScriptStructName);
			ASSERT_THAT(IsTrue(DelegateAfterReload.IsValid(), TEXT("Script struct delegate metadata should exist after reload")));
			ASSERT_THAT(IsTrue(StructAfterReload.IsValid(), TEXT("Script struct metadata should exist after reload")));
			UScriptStruct* StructObjectAfterReload = static_cast<UScriptStruct*>(StructAfterReload->Struct);
			ASSERT_THAT(IsNotNull(StructObjectAfterReload, TEXT("Script struct object should exist after reload")));
			ASSERT_THAT(AreEqual(1, Observation.DelegateReloadCount, TEXT("Script struct delegate reload should broadcast exactly once")));
			ASSERT_THAT(AreEqual(1, Observation.StructReloadCount, TEXT("Script struct layout change should broadcast struct reload exactly once")));
			ASSERT_THAT(AreEqual(DelegateBeforeReload->Function, Observation.OldDelegateSeenDuringReload, TEXT("Script struct delegate reload should expose old delegate")));
			ASSERT_THAT(AreEqual(DelegateAfterReload->Function, Observation.NewDelegateSeenDuringReload, TEXT("Script struct delegate reload should expose new delegate")));
			ASSERT_THAT(AreEqual(StructObjectBeforeReload, Observation.OldStructSeenDuringReload, TEXT("Script struct reload should expose old struct")));
			ASSERT_THAT(AreEqual(StructObjectAfterReload, Observation.NewStructSeenDuringReload, TEXT("Script struct reload should expose new struct")));
			ASSERT_THAT(AreNotEqual(Observation.OldStructSeenDuringReload, Observation.NewStructSeenDuringReload, TEXT("Script struct reload should replace the struct object")));
			ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StructObjectAfterReload, TEXT("Bonus")), TEXT("Script struct reload should expose the new Bonus field")));
			ASSERT_THAT(IsTrue(ValidateStructDelegateProperty(*TestRunner, *DelegateAfterReload, TEXT("Payload"), FName(TEXT("HotReloadScriptPayload")), TEXT("Script struct delegate should expose payload parameter after reload"))));

			UClass* ReloadedReceiverClass = FindGeneratedClass(&Engine, TEXT("UHotReloadScriptStructReceiver"));
			UObject* ReloadedReceiver = CreateReceiverObject(*TestRunner, ReloadedReceiverClass, TEXT("Script struct delegate receiver class should exist after reload"));
			ASSERT_THAT(IsNotNull(ReloadedReceiver));
			ASSERT_THAT(IsTrue(ExecuteIntGlobal(
				*TestRunner,
				Engine,
				ScriptStructModuleName,
				TEXT("int RunPayload(UHotReloadScriptStructReceiver)"),
				ReloadedReceiver,
				42,
				TEXT("Script struct V2 delegate invocation should receive the reloaded payload layout"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
