#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheClassGraphReflectionRestoreTests_Private
{
	class FPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPackSource(
			const TConstArrayView<FAngelscriptEncodedPack> InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			for (const FAngelscriptEncodedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			OutBytes = {};
			return false;
		}

	private:
		TConstArrayView<FAngelscriptEncodedPack> Packs;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2ClassGraphReflectionRestore"),
			TEXT("VmExecutionCodec=5"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static TMap<FName, FString> CopyObjectMetadata(const UObject& Object)
	{
		const TMap<FName, FString>* Metadata =
			FMetaData::GetMapForObject(&Object);
		return Metadata != nullptr ? *Metadata : TMap<FName, FString>();
	}

	static TMap<FName, FString> CopyFieldMetadata(const FField& Field)
	{
		const TMap<FName, FString>* Metadata = Field.GetMetaDataMap();
		return Metadata != nullptr ? *Metadata : TMap<FName, FString>();
	}

	static bool CompareMetadata(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const TMap<FName, FString>& Producer,
		const TMap<FName, FString>& Consumer)
	{
		TArray<FName> Keys;
		Producer.GetKeys(Keys);
		for (const TPair<FName, FString>& Pair : Consumer)
		{
			Keys.AddUnique(Pair.Key);
		}
		Keys.Sort(FNameLexicalLess());

		bool bResult = Test.TestEqual(
			*FString::Printf(TEXT("%s metadata count must match"), Label),
			Consumer.Num(), Producer.Num());
		for (const FName Key : Keys)
		{
			const FString* ProducerValue = Producer.Find(Key);
			const FString* ConsumerValue = Consumer.Find(Key);
			bResult &= Test.TestTrue(
				*FString::Printf(TEXT("%s metadata key %s must exist on both sides"),
					Label, *Key.ToString()),
				ProducerValue != nullptr && ConsumerValue != nullptr);
			if (ProducerValue != nullptr && ConsumerValue != nullptr)
			{
				bResult &= Test.TestEqual(
					*FString::Printf(TEXT("%s metadata value %s must match"),
						Label, *Key.ToString()),
					*ConsumerValue, *ProducerValue);
			}
		}
		return bResult;
	}

	struct FPropertyDescriptorSnapshot
	{
		FString Name;
		bool bBlueprintReadable = false;
		bool bBlueprintWritable = false;
		bool bEditableOnDefaults = false;
		bool bEditableOnInstance = false;
		bool bEditConst = false;
		bool bAdvancedDisplay = false;
		bool bTransient = false;
		bool bSaveGame = false;
		TMap<FName, FString> Metadata;
	};

	struct FDescriptorSnapshot
	{
		TMap<FName, FString> ClassMetadata;
		bool bBlueprintCallable = false;
		bool bBlueprintPure = false;
		bool bConstMethod = false;
		TMap<FName, FString> FunctionMetadata;
		int32 ArgumentCount = 0;
		FString ArgumentName;
		FString ArgumentDefault;
		bool bArgumentByValue = false;
		TArray<FPropertyDescriptorSnapshot> Properties;
	};

	static bool CaptureDescriptor(
		FAngelscriptClassDesc& Class,
		FDescriptorSnapshot& Out)
	{
		const TSharedPtr<FAngelscriptFunctionDesc> Function =
			Class.GetMethod(TEXT("ComputeValue"));
		const TSharedPtr<FAngelscriptPropertyDesc> Editable =
			Class.GetProperty(TEXT("EditableValue"));
		const TSharedPtr<FAngelscriptPropertyDesc> Transient =
			Class.GetProperty(TEXT("TransientValue"));
		if (!Function.IsValid() || !Editable.IsValid() || !Transient.IsValid())
		{
			return false;
		}

		Out.ClassMetadata = Class.Meta;
		Out.bBlueprintCallable = Function->bBlueprintCallable;
		Out.bBlueprintPure = Function->bBlueprintPure;
		Out.bConstMethod = Function->bIsConstMethod;
		Out.FunctionMetadata = Function->Meta;
		Out.ArgumentCount = Function->Arguments.Num();
		if (Function->Arguments.Num() == 1)
		{
			Out.ArgumentName = Function->Arguments[0].ArgumentName;
			Out.ArgumentDefault = Function->Arguments[0].DefaultValue;
			Out.bArgumentByValue = Function->Arguments[0].bBlueprintByValue;
		}

		for (const TSharedPtr<FAngelscriptPropertyDesc>& Property
			: {Editable, Transient})
		{
			FPropertyDescriptorSnapshot& Snapshot =
				Out.Properties.AddDefaulted_GetRef();
			Snapshot.Name = Property->PropertyName;
			Snapshot.bBlueprintReadable = Property->bBlueprintReadable;
			Snapshot.bBlueprintWritable = Property->bBlueprintWritable;
			Snapshot.bEditableOnDefaults = Property->bEditableOnDefaults;
			Snapshot.bEditableOnInstance = Property->bEditableOnInstance;
			Snapshot.bEditConst = Property->bEditConst;
			Snapshot.bAdvancedDisplay = Property->bAdvancedDisplay;
			Snapshot.bTransient = Property->bTransient;
			Snapshot.bSaveGame = Property->bSaveGame;
			Snapshot.Metadata = Property->Meta;
		}
		return true;
	}

	static bool CompareDescriptors(
		FAutomationTestBase& Test,
		const FDescriptorSnapshot& Producer,
		const FDescriptorSnapshot& Consumer)
	{
		bool bResult = CompareMetadata(Test, TEXT("class descriptor"),
			Producer.ClassMetadata, Consumer.ClassMetadata);
		bResult &= Test.TestEqual(TEXT("BlueprintCallable descriptor flag must match"),
			Consumer.bBlueprintCallable, Producer.bBlueprintCallable);
		bResult &= Test.TestEqual(TEXT("BlueprintPure descriptor flag must match"),
			Consumer.bBlueprintPure, Producer.bBlueprintPure);
		bResult &= Test.TestEqual(TEXT("const descriptor flag must match"),
			Consumer.bConstMethod, Producer.bConstMethod);
		bResult &= CompareMetadata(Test, TEXT("function descriptor"),
			Producer.FunctionMetadata, Consumer.FunctionMetadata);
		bResult &= Test.TestEqual(TEXT("descriptor argument count must match"),
			Consumer.ArgumentCount, Producer.ArgumentCount);
		bResult &= Test.TestEqual(TEXT("descriptor argument name must match"),
			Consumer.ArgumentName, Producer.ArgumentName);
		bResult &= Test.TestEqual(TEXT("descriptor default argument must match"),
			Consumer.ArgumentDefault, Producer.ArgumentDefault);
		bResult &= Test.TestEqual(TEXT("descriptor by-value trait must match"),
			Consumer.bArgumentByValue, Producer.bArgumentByValue);
		bResult &= Test.TestEqual(TEXT("descriptor property count must match"),
			Consumer.Properties.Num(), Producer.Properties.Num());
		for (int32 Index = 0;
			Index < FMath::Min(Producer.Properties.Num(), Consumer.Properties.Num());
			++Index)
		{
			const FPropertyDescriptorSnapshot& Expected = Producer.Properties[Index];
			const FPropertyDescriptorSnapshot& Actual = Consumer.Properties[Index];
			const FString Label = FString::Printf(TEXT("property descriptor %s"),
				*Expected.Name);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s name must match"), *Label),
				Actual.Name, Expected.Name);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s BlueprintReadable must match"), *Label),
				Actual.bBlueprintReadable, Expected.bBlueprintReadable);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s BlueprintWritable must match"), *Label),
				Actual.bBlueprintWritable, Expected.bBlueprintWritable);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s EditableOnDefaults must match"), *Label),
				Actual.bEditableOnDefaults, Expected.bEditableOnDefaults);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s EditableOnInstance must match"), *Label),
				Actual.bEditableOnInstance, Expected.bEditableOnInstance);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s EditConst must match"), *Label),
				Actual.bEditConst, Expected.bEditConst);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s AdvancedDisplay must match"), *Label),
				Actual.bAdvancedDisplay, Expected.bAdvancedDisplay);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s Transient must match"), *Label),
				Actual.bTransient, Expected.bTransient);
			bResult &= Test.TestEqual(*FString::Printf(TEXT("%s SaveGame must match"), *Label),
				Actual.bSaveGame, Expected.bSaveGame);
			bResult &= CompareMetadata(Test, *Label,
				Expected.Metadata, Actual.Metadata);
		}
		return bResult;
	}

	struct FReflectedSnapshot
	{
		uint32 ClassFlags = 0;
		uint32 FunctionFlags = 0;
		uint64 EditableFlags = 0;
		uint64 TransientFlags = 0;
		uint64 ArgumentFlags = 0;
		uint64 ReturnFlags = 0;
		TMap<FName, FString> ClassMetadata;
		TMap<FName, FString> FunctionMetadata;
		TMap<FName, FString> EditableMetadata;
		TMap<FName, FString> TransientMetadata;
	};

	static bool CaptureReflected(
		UClass& Class,
		FReflectedSnapshot& Out)
	{
		UFunction* Function = Class.FindFunctionByName(TEXT("ComputeValue"));
		FProperty* Editable = FindFProperty<FProperty>(&Class, TEXT("EditableValue"));
		FProperty* Transient = FindFProperty<FProperty>(&Class, TEXT("TransientValue"));
		FProperty* Argument = Function != nullptr
			? FindFProperty<FProperty>(Function, TEXT("Delta")) : nullptr;
		FProperty* Return = Function != nullptr
			? Function->GetReturnProperty() : nullptr;
		if (Function == nullptr || Editable == nullptr || Transient == nullptr
			|| Argument == nullptr || Return == nullptr)
		{
			return false;
		}

		Out.ClassFlags = static_cast<uint32>(Class.GetClassFlags());
		Out.FunctionFlags = static_cast<uint32>(Function->FunctionFlags);
		Out.EditableFlags = static_cast<uint64>(Editable->GetPropertyFlags());
		Out.TransientFlags = static_cast<uint64>(Transient->GetPropertyFlags());
		Out.ArgumentFlags = static_cast<uint64>(Argument->GetPropertyFlags());
		Out.ReturnFlags = static_cast<uint64>(Return->GetPropertyFlags());
		Out.ClassMetadata = CopyObjectMetadata(Class);
		Out.FunctionMetadata = CopyObjectMetadata(*Function);
		Out.EditableMetadata = CopyFieldMetadata(*Editable);
		Out.TransientMetadata = CopyFieldMetadata(*Transient);
		return true;
	}

	static bool CompareReflected(
		FAutomationTestBase& Test,
		const FReflectedSnapshot& Producer,
		const FReflectedSnapshot& Consumer)
	{
		bool bResult = Test.TestEqual(TEXT("UClass flags must match exactly"),
			Consumer.ClassFlags, Producer.ClassFlags);
		bResult &= Test.TestEqual(TEXT("UFunction flags must match exactly"),
			Consumer.FunctionFlags, Producer.FunctionFlags);
		bResult &= Test.TestEqual(TEXT("editable FProperty flags must match exactly"),
			Consumer.EditableFlags, Producer.EditableFlags);
		bResult &= Test.TestEqual(TEXT("transient FProperty flags must match exactly"),
			Consumer.TransientFlags, Producer.TransientFlags);
		bResult &= Test.TestEqual(TEXT("argument FProperty flags must match exactly"),
			Consumer.ArgumentFlags, Producer.ArgumentFlags);
		bResult &= Test.TestEqual(TEXT("return FProperty flags must match exactly"),
			Consumer.ReturnFlags, Producer.ReturnFlags);
		bResult &= CompareMetadata(Test, TEXT("UClass"),
			Producer.ClassMetadata, Consumer.ClassMetadata);
		bResult &= CompareMetadata(Test, TEXT("UFunction"),
			Producer.FunctionMetadata, Consumer.FunctionMetadata);
		bResult &= CompareMetadata(Test, TEXT("editable FProperty"),
			Producer.EditableMetadata, Consumer.EditableMetadata);
		bResult &= CompareMetadata(Test, TEXT("transient FProperty"),
			Producer.TransientMetadata, Consumer.TransientMetadata);
		return bResult;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheClassGraphReflectionRestoreTests,
	"Angelscript.TestModule.Cache.ClassGraphReflectionRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(UnsupportedDelegateShapeFailsClosedWithoutPartialArtifacts)
	{
		using namespace AngelscriptCacheClassGraphReflectionRestoreTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = ASTEST_AS(R"AS(
			delegate void FCacheV2UnsupportedReflectionDelegate(int Value);

			UCLASS()
			class UCacheV2UnsupportedReflectionCarrier : UObject
			{
				UPROPERTY()
				int Value = 17;
			}
		)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2UnsupportedReflectionShape", Source);
		ASSERT_THAT(IsNotNull(ScriptModule));
		TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		ASSERT_THAT(AreEqual(1, Module->Classes.Num()));
		ASSERT_THAT(AreEqual(1, Module->Delegates.Num()));
		const TSharedPtr<FAngelscriptModuleDesc> ActiveBefore =
			Fixture.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2UnsupportedReflectionShape"));
		ASSERT_THAT(IsTrue(ActiveBefore.IsValid()));

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		Artifacts.CanonicalModuleName = TEXT("SentinelMustBeReset");
		Artifacts.Records.AddDefaulted();
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), MakeCaptureOptions(), Artifacts);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.12 unsupported reflection capture: Error=%u Graph=%u Records=%d ModuleName=%s Detail=%s"),
			static_cast<uint32>(Capture.Error),
			Capture.ValidatedGraphRecordCount,
			Artifacts.Records.Num(),
			*Artifacts.CanonicalModuleName,
			*Capture.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::NotCacheable,
			Capture.Error));
		ASSERT_THAT(AreEqual(uint32(0), Capture.ValidatedGraphRecordCount));
		ASSERT_THAT(AreEqual(0, Artifacts.Records.Num()));
		ASSERT_THAT(IsTrue(Artifacts.CanonicalModuleName.IsEmpty()));
		ASSERT_THAT(IsTrue(Artifacts.ModuleKey.Hash.IsZero()));
		ASSERT_THAT(IsTrue(Artifacts.SourceSnapshot.IsZero()));
		ASSERT_THAT(AreEqual(0,
			Artifacts.ValidatedFunctionArtifactIdentities.Num()));

		const TSharedPtr<FAngelscriptModuleDesc> ActiveAfter =
			Fixture.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2UnsupportedReflectionShape"));
		ASSERT_THAT(IsTrue(ActiveAfter.IsValid()));
		ASSERT_THAT(AreEqual(ActiveBefore.Get(), ActiveAfter.Get()));
		ASSERT_THAT(AreEqual(ScriptModule, ActiveAfter->ScriptModule));
		ASSERT_THAT(AreEqual(1, ActiveAfter->Classes.Num()));
		ASSERT_THAT(AreEqual(1, ActiveAfter->Delegates.Num()));
	}

	TEST_METHOD(SupportedFlagsMetadataDefaultsAndExecutionMatchFreshEngine)
	{
		using namespace AngelscriptCacheClassGraphReflectionRestoreTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		FDescriptorSnapshot ProducerDescriptor;
		FReflectedSnapshot ProducerReflected;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid()));
			const FString Source = ASTEST_AS(R"AS(
				UCLASS(meta=(DisplayName="Cache V2 Reflection Carrier", ToolTip="Cache V2 class tooltip"))
				class UCacheV2ReflectionCarrier : UObject
				{
					UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, SaveGame,
						Category="CacheV2|Data",
						meta=(DisplayName="Cached Editable Value", ToolTip="Editable tooltip", ClampMin="0", ClampMax="100"))
					int EditableValue = 40;

					UPROPERTY(Transient, BlueprintReadWrite,
						meta=(DisplayName="Cached Transient Value", ToolTip="Transient tooltip"))
					int TransientValue = 2;

					UFUNCTION(BlueprintPure, Category="CacheV2|Functions",
						meta=(DisplayName="Compute Cached Value", ToolTip="Function tooltip", Keywords="cache reflection"))
					int ComputeValue(int Delta = 2) const
					{
						return EditableValue + TransientValue + Delta;
					}
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2ClassGraphReflectionRestore", Source);
			ASSERT_THAT(IsNotNull(ScriptModule));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid()));
			const TSharedPtr<FAngelscriptClassDesc> Class =
				Module->GetClass(TEXT("UCacheV2ReflectionCarrier"));
			ASSERT_THAT(IsTrue(Class.IsValid()));
			ASSERT_THAT(IsTrue(CaptureDescriptor(*Class, ProducerDescriptor)));
			ASSERT_THAT(IsTrue(CaptureReflected(*Class->Class, ProducerReflected)));
			ASSERT_THAT(IsTrue(ProducerDescriptor.bBlueprintCallable));
			ASSERT_THAT(IsTrue(ProducerDescriptor.bBlueprintPure));
			ASSERT_THAT(IsTrue(ProducerDescriptor.bConstMethod));
			ASSERT_THAT(AreEqual(1, ProducerDescriptor.ArgumentCount));
			ASSERT_THAT(AreEqual(FString(TEXT("2")),
				ProducerDescriptor.ArgumentDefault));

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("V3.12 reflection capture: Error=%u Records=%d Graph=%u ClassFlags=0x%x FunctionFlags=0x%x EditableFlags=0x%llx TransientFlags=0x%llx Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				ProducerReflected.ClassFlags,
				ProducerReflected.FunctionFlags,
				ProducerReflected.EditableFlags,
				ProducerReflected.TransientFlags,
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess()));
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		ASSERT_THAT(IsTrue(PrepareAngelscriptCacheColdGeneration(
			Artifacts, Options, PackPolicy, Codec, Prepared).IsSuccess()));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		ASSERT_THAT(IsTrue(ValidateAngelscriptCacheGeneration(
			Prepared.EncodedManifest.CompleteBytes,
			Prepared.EncodedManifest.ComputedGenerationId,
			Packs, Limits, Budget, Codec, Validated).IsSuccess()));
		ASSERT_THAT(IsTrue(Validated.IsSet()));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(), Validated.GetValue(),
				Artifacts.ModuleKey, Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.12 reflection restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess()));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2ClassGraphReflectionRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		const TSharedPtr<FAngelscriptClassDesc> RestoredClass =
			RestoredModule->GetClass(TEXT("UCacheV2ReflectionCarrier"));
		ASSERT_THAT(IsTrue(RestoredClass.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredClass->Class));

		FDescriptorSnapshot ConsumerDescriptor;
		FReflectedSnapshot ConsumerReflected;
		ASSERT_THAT(IsTrue(CaptureDescriptor(
			*RestoredClass, ConsumerDescriptor)));
		ASSERT_THAT(IsTrue(CaptureReflected(
			*RestoredClass->Class, ConsumerReflected)));
		ASSERT_THAT(IsTrue(CompareDescriptors(
			*TestRunner, ProducerDescriptor, ConsumerDescriptor)));
		ASSERT_THAT(IsTrue(CompareReflected(
			*TestRunner, ProducerReflected, ConsumerReflected)));

		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.12 restored reflection surface: ClassFlags=0x%x FunctionFlags=0x%x EditableFlags=0x%llx TransientFlags=0x%llx ClassMeta=%d FunctionMeta=%d EditableMeta=%d TransientMeta=%d"),
			ConsumerReflected.ClassFlags,
			ConsumerReflected.FunctionFlags,
			ConsumerReflected.EditableFlags,
			ConsumerReflected.TransientFlags,
			ConsumerReflected.ClassMetadata.Num(),
			ConsumerReflected.FunctionMetadata.Num(),
			ConsumerReflected.EditableMetadata.Num(),
			ConsumerReflected.TransientMetadata.Num()));

		FIntProperty* Editable = FindFProperty<FIntProperty>(
			RestoredClass->Class, TEXT("EditableValue"));
		FIntProperty* Transient = FindFProperty<FIntProperty>(
			RestoredClass->Class, TEXT("TransientValue"));
		ASSERT_THAT(IsNotNull(Editable));
		ASSERT_THAT(IsNotNull(Transient));
		UObject* Object = NewObject<UObject>(
			GetTransientPackage(), RestoredClass->Class);
		ASSERT_THAT(IsNotNull(Object));
		Editable->SetPropertyValue_InContainer(Object, 40);
		Transient->SetPropertyValue_InContainer(Object, 2);
		FFunctionInvoker Invoker(
			*TestRunner, Object, FName(TEXT("ComputeValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid()));
		Invoker.AddParam<int32>(2);
		ASSERT_THAT(AreEqual(
			44,
			Invoker.CallAndReturn<int32>(0),
			TEXT("Restored reflection function should execute against restored properties")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
