#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UObjectGlobals.h"

#include "as_objecttype.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheClassGraphInheritanceRestoreTests_Private
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
			TEXT("CacheV2ClassGraphInheritanceRestore"),
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

	static const TSharedRef<FAngelscriptClassDesc>* FindClass(
		const FAngelscriptModuleDesc& Module,
		const FStringView ClassName)
	{
		return Module.Classes.FindByPredicate(
			[ClassName](const TSharedRef<FAngelscriptClassDesc>& Candidate)
			{
				return Candidate->ClassName == ClassName;
			});
	}

	static const asCScriptFunction* FindVirtualFunction(
		const asCObjectType& Type,
		const char* FunctionName)
	{
		for (asUINT Index = 0; Index < Type.virtualFunctionTable.GetLength(); ++Index)
		{
			const asCScriptFunction* Function = Type.virtualFunctionTable[Index];
			if (Function != nullptr && Function->name == FunctionName)
			{
				return Function;
			}
		}
		return nullptr;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheClassGraphInheritanceRestoreTests,
	"Angelscript.TestModule.Cache.ClassGraphInheritanceRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ThreeLevelOverrideChainRestoresVftOwnersAndSuperCalls)
	{
		using namespace AngelscriptCacheClassGraphInheritanceRestoreTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid()));
			const FString Source = ASTEST_AS(R"AS(
				UCLASS()
				class UCacheV2VftBase : UObject
				{
					UFUNCTION(BlueprintEvent)
					int Compute()
					{
						return 1;
					}

					int BaseOnly()
					{
						return 3;
					}
				}

				UCLASS()
				class UCacheV2VftMiddle : UCacheV2VftBase
				{
					UFUNCTION(BlueprintOverride)
					int Compute()
					{
						return Super::Compute() + 10;
					}

					int MiddleOnly()
					{
						return 5;
					}
				}

				UCLASS()
				class UCacheV2VftLeaf : UCacheV2VftMiddle
				{
					UFUNCTION(BlueprintOverride)
					int Compute()
					{
						return Super::Compute() + 30;
					}

					int LeafOnly()
					{
						return 2;
					}

					UFUNCTION()
					int ReflectedValue()
					{
						return Compute() + BaseOnly() + MiddleOnly() + LeafOnly();
					}
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2ClassGraphInheritanceRestore", Source);
			ASSERT_THAT(IsNotNull(ScriptModule));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(AreEqual(3, Module->Classes.Num()));
			for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
			{
				const asCObjectType* Type =
					static_cast<const asCObjectType*>(Class->ScriptType);
				ASSERT_THAT(IsNotNull(Type));
				for (asUINT MethodIndex = 0;
					MethodIndex < Type->methods.GetLength(); ++MethodIndex)
				{
					const asCScriptFunction* Function =
						Type->engine->GetScriptFunction(Type->methods[MethodIndex]);
					TestRunner->AddInfo(FString::Printf(
						TEXT("Three-level producer method: Type=%s Ordinal=%u Name=%s Owner=%s Vft=%d"),
						*Class->ClassName, MethodIndex,
						Function != nullptr
							? UTF8_TO_TCHAR(Function->GetName()) : TEXT("<null>"),
						Function != nullptr && Function->objectType != nullptr
							? UTF8_TO_TCHAR(Function->objectType->GetName())
							: TEXT("<none>"),
						Function != nullptr ? Function->vfTableIdx : -1));
				}
				for (asUINT VftIndex = 0;
					VftIndex < Type->virtualFunctionTable.GetLength(); ++VftIndex)
				{
					const asCScriptFunction* Function =
						Type->virtualFunctionTable[VftIndex];
					TestRunner->AddInfo(FString::Printf(
						TEXT("Three-level producer VFT: Type=%s Ordinal=%u Name=%s Owner=%s Vft=%d"),
						*Class->ClassName, VftIndex,
						Function != nullptr
							? UTF8_TO_TCHAR(Function->GetName()) : TEXT("<null>"),
						Function != nullptr && Function->objectType != nullptr
							? UTF8_TO_TCHAR(Function->objectType->GetName())
							: TEXT("<none>"),
						Function != nullptr ? Function->vfTableIdx : -1));
				}
			}

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Three-level capture: Error=%u Records=%d Graph=%u Detail=%s"),
				static_cast<uint32>(Capture.Error), Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount, *Capture.Detail));
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
		const FAngelscriptCacheRestoreResult Restore = RestoreAngelscriptCacheModule(
			Consumer.GetEngine(), Validated.GetValue(), Artifacts.ModuleKey, Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Three-level restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error), static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount, Restore.RestoredFunctionCount, *Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(3), Restore.RestoredTypeCount));

		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2ClassGraphInheritanceRestore"));
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		ASSERT_THAT(AreEqual(3, Restored->Classes.Num()));
		const TSharedRef<FAngelscriptClassDesc>* Base = FindClass(
			*Restored, TEXT("UCacheV2VftBase"));
		const TSharedRef<FAngelscriptClassDesc>* Middle = FindClass(
			*Restored, TEXT("UCacheV2VftMiddle"));
		const TSharedRef<FAngelscriptClassDesc>* Leaf = FindClass(
			*Restored, TEXT("UCacheV2VftLeaf"));
		ASSERT_THAT(IsNotNull(Base));
		ASSERT_THAT(IsNotNull(Middle));
		ASSERT_THAT(IsNotNull(Leaf));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*Base)->Class), (*Middle)->Class->GetSuperClass()));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*Middle)->Class), (*Leaf)->Class->GetSuperClass()));

		const asCObjectType* BaseType =
			static_cast<const asCObjectType*>((*Base)->ScriptType);
		const asCObjectType* MiddleType =
			static_cast<const asCObjectType*>((*Middle)->ScriptType);
		const asCObjectType* LeafType =
			static_cast<const asCObjectType*>((*Leaf)->ScriptType);
		ASSERT_THAT(IsNotNull(BaseType));
		ASSERT_THAT(IsNotNull(MiddleType));
		ASSERT_THAT(IsNotNull(LeafType));
		ASSERT_THAT(AreEqual(BaseType, MiddleType->derivedFrom));
		ASSERT_THAT(AreEqual(MiddleType, LeafType->derivedFrom));

		const asCScriptFunction* BaseEvent =
			FindVirtualFunction(*BaseType, "Compute");
		const asCScriptFunction* MiddleEvent =
			FindVirtualFunction(*MiddleType, "Compute");
		const asCScriptFunction* LeafEvent =
			FindVirtualFunction(*LeafType, "Compute");
		const asCScriptFunction* BaseCompute =
			FindVirtualFunction(*BaseType, "Compute_Implementation");
		const asCScriptFunction* MiddleCompute =
			FindVirtualFunction(*MiddleType, "Compute_Implementation");
		const asCScriptFunction* LeafCompute =
			FindVirtualFunction(*LeafType, "Compute_Implementation");
		const asCScriptFunction* BaseOnly =
			FindVirtualFunction(*LeafType, "BaseOnly");
		const asCScriptFunction* MiddleOnly =
			FindVirtualFunction(*LeafType, "MiddleOnly");
		const asCScriptFunction* LeafOnly =
			FindVirtualFunction(*LeafType, "LeafOnly");
		const asCScriptFunction* ReflectedValueFunction =
			FindVirtualFunction(*LeafType, "ReflectedValue");
		ASSERT_THAT(IsNotNull(BaseEvent));
		ASSERT_THAT(IsNotNull(MiddleEvent));
		ASSERT_THAT(IsNotNull(LeafEvent));
		ASSERT_THAT(IsNotNull(BaseCompute));
		ASSERT_THAT(IsNotNull(MiddleCompute));
		ASSERT_THAT(IsNotNull(LeafCompute));
		ASSERT_THAT(IsNotNull(BaseOnly));
		ASSERT_THAT(IsNotNull(MiddleOnly));
		ASSERT_THAT(IsNotNull(LeafOnly));
		ASSERT_THAT(IsNotNull(ReflectedValueFunction));

		// BlueprintEvent keeps the public event wrapper inherited from Base in
		// slot 0. BlueprintOverride replaces the separate implementation slot.
		ASSERT_THAT(AreEqual(BaseEvent, MiddleEvent));
		ASSERT_THAT(AreEqual(MiddleEvent, LeafEvent));
		ASSERT_THAT(AreEqual(BaseType, BaseEvent->objectType));
		ASSERT_THAT(AreEqual(BaseCompute->vfTableIdx, MiddleCompute->vfTableIdx));
		ASSERT_THAT(AreEqual(MiddleCompute->vfTableIdx, LeafCompute->vfTableIdx));
		ASSERT_THAT(AreEqual(BaseType, BaseCompute->objectType));
		ASSERT_THAT(AreEqual(MiddleType, MiddleCompute->objectType));
		ASSERT_THAT(AreEqual(LeafType, LeafCompute->objectType));

		// Every remaining VFT family in Leaf retains its real declaring/
		// implementing type and monotonic ordinal after fresh-engine restore.
		ASSERT_THAT(AreEqual(BaseType, BaseOnly->objectType));
		ASSERT_THAT(AreEqual(MiddleType, MiddleOnly->objectType));
		ASSERT_THAT(AreEqual(LeafType, LeafOnly->objectType));
		ASSERT_THAT(AreEqual(LeafType, ReflectedValueFunction->objectType));
		ASSERT_THAT(AreEqual(0, BaseEvent->vfTableIdx));
		ASSERT_THAT(AreEqual(1, LeafCompute->vfTableIdx));
		ASSERT_THAT(AreEqual(2, BaseOnly->vfTableIdx));
		ASSERT_THAT(AreEqual(3, MiddleOnly->vfTableIdx));
		ASSERT_THAT(AreEqual(4, LeafOnly->vfTableIdx));
		ASSERT_THAT(AreEqual(5, ReflectedValueFunction->vfTableIdx));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Three-level VFT: Slot=%d Owners=%s -> %s -> %s"),
			LeafCompute->vfTableIdx,
			UTF8_TO_TCHAR(BaseCompute->objectType->GetName()),
			UTF8_TO_TCHAR(MiddleCompute->objectType->GetName()),
			UTF8_TO_TCHAR(LeafCompute->objectType->GetName())));

		for (const TSharedRef<FAngelscriptClassDesc>* Class : {Base, Middle, Leaf})
		{
			for (int32 MethodIndex = 0;
				MethodIndex < (*Class)->Methods.Num(); ++MethodIndex)
			{
				const FAngelscriptFunctionDesc& Method =
					(*Class)->Methods[MethodIndex].Get();
				TestRunner->AddInfo(FString::Printf(
					TEXT("Three-level restored reflection: Class=%s Ordinal=%d FunctionName=%s OriginalFunctionName=%s ScriptFunctionName=%s"),
					*(*Class)->ClassName, MethodIndex,
					*Method.FunctionName, *Method.OriginalFunctionName,
					*Method.ScriptFunctionName));
			}
		}

		UFunction* BaseReflectedCompute = (*Base)->Class->FindFunctionByName(
			TEXT("Compute"));
		UFunction* MiddleReflectedCompute = (*Middle)->Class->FindFunctionByName(
			TEXT("Compute"));
		UFunction* LeafReflectedCompute = (*Leaf)->Class->FindFunctionByName(
			TEXT("Compute"));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Three-level reflected lookup: Base=%s Middle=%s Leaf=%s"),
			BaseReflectedCompute != nullptr ? *BaseReflectedCompute->GetName()
				: TEXT("<missing>"),
			MiddleReflectedCompute != nullptr ? *MiddleReflectedCompute->GetName()
				: TEXT("<missing>"),
			LeafReflectedCompute != nullptr ? *LeafReflectedCompute->GetName()
				: TEXT("<missing>")));
		ASSERT_THAT(IsNotNull(BaseReflectedCompute));
		ASSERT_THAT(IsNotNull(MiddleReflectedCompute));
		ASSERT_THAT(IsNotNull(LeafReflectedCompute));
		if (BaseReflectedCompute == nullptr || MiddleReflectedCompute == nullptr
			|| LeafReflectedCompute == nullptr)
		{
			return;
		}

		UFunction* ReflectedValue = (*Leaf)->Class->FindFunctionByName(
			TEXT("ReflectedValue"));
		ASSERT_THAT(IsNotNull(ReflectedValue));
		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), (*Leaf)->Class);
		ASSERT_THAT(IsNotNull(Instance));
		FFunctionInvoker Invoker(
			*TestRunner, Instance, FName(TEXT("ReflectedValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid()));
		ASSERT_THAT(AreEqual(51, Invoker.CallAndReturn<int32>(INDEX_NONE)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
