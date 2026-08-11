#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheClassGraphRollbackTests_Private
{
	constexpr const TCHAR* ModuleName =
		TEXT("ASCacheV2ClassGraphRollback");
	constexpr const TCHAR* LeftClassName =
		TEXT("UCacheV2RollbackLeft");
	constexpr const TCHAR* RightClassName =
		TEXT("UCacheV2RollbackRight");

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
			TEXT("CacheV2ClassGraphRollback"),
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

	static TArray<FString> CaptureActiveModules(
		const FAngelscriptEngine& Engine)
	{
		TArray<FString> Result;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module
			: Engine.GetActiveModules())
		{
			Result.Add(Module->ModuleName);
		}
		Result.Sort();
		return Result;
	}

	static TArray<FString> CaptureVmModules(asIScriptEngine& ScriptEngine)
	{
		TArray<FString> Result;
		for (asUINT Index = 0; Index < ScriptEngine.GetModuleCount(); ++Index)
		{
			if (asIScriptModule* Module = ScriptEngine.GetModuleByIndex(Index))
			{
				Result.Add(UTF8_TO_TCHAR(Module->GetName()));
			}
		}
		Result.Sort();
		return Result;
	}

	static TArray<FString> CaptureVmObjectTypes(asIScriptEngine& ScriptEngine)
	{
		TArray<FString> Result;
		for (asUINT Index = 0;
			Index < ScriptEngine.GetObjectTypeCount(); ++Index)
		{
			if (asITypeInfo* Type = ScriptEngine.GetObjectTypeByIndex(Index))
			{
				Result.Add(FString::Printf(
					TEXT("%s::%s"),
					UTF8_TO_TCHAR(Type->GetNamespace()),
					UTF8_TO_TCHAR(Type->GetName())));
			}
		}
		Result.Sort();
		return Result;
	}

	static bool CompareInventory(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const TConstArrayView<FString> Expected,
		const TConstArrayView<FString> Actual)
	{
		bool bMatches = Test.TestEqual(
			*FString::Printf(TEXT("%s count"), Label),
			Actual.Num(), Expected.Num());
		for (int32 Index = 0;
			Index < FMath::Min(Expected.Num(), Actual.Num()); ++Index)
		{
			bMatches &= Test.TestEqual(
				*FString::Printf(TEXT("%s entry %d"), Label, Index),
				Actual[Index], Expected[Index]);
		}
		return bMatches;
	}

	class FStopAfterPrepared final
		: public IAngelscriptCacheRestoreFaultInjector
	{
	public:
		virtual bool ShouldStopAt(
			const EAngelscriptCacheRestoreFaultPoint Point,
			const uint32 ModuleOrdinal,
			const FAngelscriptStableModuleKey& ModuleKey) override
		{
			if (Point != EAngelscriptCacheRestoreFaultPoint::AfterModulePrepared
				|| bStopped)
			{
				return false;
			}

			bStopped = true;
			ObservedOrdinal = ModuleOrdinal;
			ObservedModuleKey = ModuleKey;
			if (ObservedEngine != nullptr)
			{
				ActiveModuleCountAtStop =
					ObservedEngine->GetActiveModules().Num();
				RawVmModuleCountAtStop = static_cast<int32>(
					ObservedEngine->GetScriptEngine()->GetModuleCount());
				bDescriptorsAbsentAtStop =
					!ObservedEngine->GetClass(LeftClassName).IsValid()
					&& !ObservedEngine->GetClass(RightClassName).IsValid();
				const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
					ESPMode::ThreadSafe> RoutesAtStop =
					ObservedEngine->GetFunctionRouteSnapshot();
				bRoutePublicationUnchangedAtStop =
					RoutesAtStop.Get() == RoutesBefore.Get();
			}
			return true;
		}

		FAngelscriptEngine* ObservedEngine = nullptr;
		TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesBefore;
		FAngelscriptStableModuleKey ObservedModuleKey;
		uint32 ObservedOrdinal = MAX_uint32;
		int32 ActiveModuleCountAtStop = -1;
		int32 RawVmModuleCountAtStop = -1;
		bool bDescriptorsAbsentAtStop = false;
		bool bRoutePublicationUnchangedAtStop = false;
		bool bStopped = false;
	};

	static const FString& GetSource()
	{
		static const FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheV2RollbackLeft : UObject
			{
				UPROPERTY()
					UCacheV2RollbackRight Right;

				UFUNCTION()
					int ReadRightValue()
				{
					return Right.Value;
				}
			}

			UCLASS()
			class UCacheV2RollbackRight : UObject
			{
				UPROPERTY()
					UCacheV2RollbackLeft Left;

				UPROPERTY()
					int Value = 73;

				UFUNCTION()
					UCacheV2RollbackLeft EchoLeft(
						UCacheV2RollbackLeft Input)
				{
					return Input;
				}
			}
		)AS");
		return Source;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheClassGraphRollbackTests,
	"Angelscript.TestModule.Cache.ClassGraphRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LatePreparedFailureRollsBackCrossLinkedClassModuleAtomically)
	{
		using namespace AngelscriptCacheClassGraphRollbackTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid()));
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2ClassGraphRollback", GetSource());
			ASSERT_THAT(IsNotNull(ScriptModule));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(AreEqual(2, Module->Classes.Num()));

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("V3.13 multi-class capture: Error=%u Records=%d Graph=%u Routes=%d Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				Artifacts.ValidatedFunctionArtifactIdentities.Num(),
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess()));
			ASSERT_THAT(AreEqual(2, Module->Classes.Num()));
			ASSERT_THAT(IsTrue(
				Artifacts.ValidatedFunctionArtifactIdentities.Num() >= 2));
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
		FAngelscriptEngine& Engine = Consumer.GetEngine();
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));

		const TArray<FString> ActiveModulesBefore =
			CaptureActiveModules(Engine);
		const TArray<FString> VmModulesBefore =
			CaptureVmModules(*ScriptEngine);
		const TArray<FString> VmTypesBefore =
			CaptureVmObjectTypes(*ScriptEngine);
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesBefore =
			Engine.GetFunctionRouteSnapshot();
		UClass* const LeftUClassBefore = FindObject<UClass>(
			Engine.GetPackageInstance(), LeftClassName);
		UClass* const RightUClassBefore = FindObject<UClass>(
			Engine.GetPackageInstance(), RightClassName);
		ASSERT_THAT(IsFalse(Engine.GetModuleByModuleName(ModuleName).IsValid()));
		ASSERT_THAT(IsFalse(Engine.GetClass(LeftClassName).IsValid()));
		ASSERT_THAT(IsFalse(Engine.GetClass(RightClassName).IsValid()));

		FStopAfterPrepared Injector;
		Injector.ObservedEngine = &Engine;
		Injector.RoutesBefore = RoutesBefore;
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Engine, Validated.GetValue(), Artifacts.ModuleKey,
				Limits, &Injector);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.13 injected rollback: Error=%u Stage=%u Stopped=%d Ordinal=%u ActiveAtStop=%d RawVmAtStop=%d ActiveBefore=%d ActiveAfter=%d VmBefore=%d VmAfter=%u TypesBefore=%d TypesAfter=%u RoutesBefore=%d RoutesAfter=%d Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Injector.bStopped ? 1 : 0,
			Injector.ObservedOrdinal,
			Injector.ActiveModuleCountAtStop,
			Injector.RawVmModuleCountAtStop,
			ActiveModulesBefore.Num(),
			Engine.GetActiveModules().Num(),
			VmModulesBefore.Num(),
			ScriptEngine->GetModuleCount(),
			VmTypesBefore.Num(),
			ScriptEngine->GetObjectTypeCount(),
			RoutesBefore.IsValid()
				? RoutesBefore->FunctionRoutes.Num() : 0,
			Engine.GetFunctionRouteSnapshot().IsValid()
				? Engine.GetFunctionRouteSnapshot()->FunctionRoutes.Num() : 0,
			*Restore.Detail));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRestoreError::ActivationFailed,
			Restore.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRestoreStage::FinalizeModule,
			Restore.Stage));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredModuleCount));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredTypeCount));
		ASSERT_THAT(AreEqual(uint32(0), Restore.RestoredFunctionCount));
		ASSERT_THAT(IsTrue(Injector.bStopped));
		ASSERT_THAT(AreEqual(uint32(0), Injector.ObservedOrdinal));
		ASSERT_THAT(IsTrue(
			Injector.ObservedModuleKey == Artifacts.ModuleKey));
		ASSERT_THAT(AreEqual(
			ActiveModulesBefore.Num(), Injector.ActiveModuleCountAtStop));
		ASSERT_THAT(IsTrue(Injector.bDescriptorsAbsentAtStop));
		ASSERT_THAT(IsTrue(Injector.bRoutePublicationUnchangedAtStop));

		ASSERT_THAT(IsTrue(CompareInventory(
			*TestRunner, TEXT("active-module rollback"),
			ActiveModulesBefore, CaptureActiveModules(Engine))));
		ASSERT_THAT(IsTrue(CompareInventory(
			*TestRunner, TEXT("raw VM module rollback"),
			VmModulesBefore, CaptureVmModules(*ScriptEngine))));
		ASSERT_THAT(IsTrue(CompareInventory(
			*TestRunner, TEXT("VM object-type rollback"),
			VmTypesBefore, CaptureVmObjectTypes(*ScriptEngine))));
		ASSERT_THAT(IsFalse(Engine.GetModuleByModuleName(ModuleName).IsValid()));
		ASSERT_THAT(IsFalse(Engine.GetClass(LeftClassName).IsValid()));
		ASSERT_THAT(IsFalse(Engine.GetClass(RightClassName).IsValid()));
		ASSERT_THAT(IsTrue(FindObject<UClass>(
			Engine.GetPackageInstance(), LeftClassName) == LeftUClassBefore));
		ASSERT_THAT(IsTrue(FindObject<UClass>(
			Engine.GetPackageInstance(), RightClassName) == RightUClassBefore));
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> RoutesAfterFailure =
			Engine.GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(RoutesAfterFailure.Get() == RoutesBefore.Get()));
		for (const FAngelscriptFunctionArtifactIdentity& Identity
			: Artifacts.ValidatedFunctionArtifactIdentities)
		{
			FAngelscriptCacheLiveFunctionRoute LeakedRoute;
			ASSERT_THAT(IsFalse(Engine.ResolveCacheFunctionRoute(
				Identity.FunctionKey, LeakedRoute)));
		}

		asIScriptModule* Authoritative = Consumer.BuildModule(
			"ASCacheV2ClassGraphRollback", GetSource());
		ASSERT_THAT(IsNotNull(Authoritative));
		ASSERT_THAT(IsNotNull(Authoritative->GetTypeInfoByDecl(
			"UCacheV2RollbackLeft")));
		ASSERT_THAT(IsNotNull(Authoritative->GetTypeInfoByDecl(
			"UCacheV2RollbackRight")));
		TSharedPtr<FAngelscriptModuleDesc> CompiledModule =
			Engine.GetModule(Authoritative);
		ASSERT_THAT(IsTrue(CompiledModule.IsValid()));
		ASSERT_THAT(AreEqual(2, CompiledModule->Classes.Num()));
		const TSharedPtr<FAngelscriptClassDesc> Left =
			CompiledModule->GetClass(LeftClassName);
		const TSharedPtr<FAngelscriptClassDesc> Right =
			CompiledModule->GetClass(RightClassName);
		ASSERT_THAT(IsTrue(Left.IsValid()));
		ASSERT_THAT(IsTrue(Right.IsValid()));
		ASSERT_THAT(IsNotNull(Left->Class));
		ASSERT_THAT(IsNotNull(Right->Class));

		FObjectProperty* RightProperty = CastField<FObjectProperty>(
			Left->Class->FindPropertyByName(TEXT("Right")));
		FObjectProperty* LeftProperty = CastField<FObjectProperty>(
			Right->Class->FindPropertyByName(TEXT("Left")));
		ASSERT_THAT(IsNotNull(RightProperty));
		ASSERT_THAT(IsNotNull(LeftProperty));
		ASSERT_THAT(AreEqual(Right->Class, RightProperty->PropertyClass));
		ASSERT_THAT(AreEqual(Left->Class, LeftProperty->PropertyClass));

		UObject* LeftObject = NewObject<UObject>(
			GetTransientPackage(), Left->Class);
		UObject* RightObject = NewObject<UObject>(
			GetTransientPackage(), Right->Class);
		ASSERT_THAT(IsNotNull(LeftObject));
		ASSERT_THAT(IsNotNull(RightObject));
		RightProperty->SetObjectPropertyValue_InContainer(
			LeftObject, RightObject);
		LeftProperty->SetObjectPropertyValue_InContainer(
			RightObject, LeftObject);
		FFunctionInvoker Invoker(
			*TestRunner, LeftObject, FName(TEXT("ReadRightValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid()));
		const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
		ASSERT_THAT(AreEqual(73, Result));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.13 authoritative same-name compile: Module=%s Classes=%d VmTypes=%u Result=%d"),
			*CompiledModule->ModuleName,
			CompiledModule->Classes.Num(),
			Authoritative->GetObjectTypeCount(),
			Result));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
