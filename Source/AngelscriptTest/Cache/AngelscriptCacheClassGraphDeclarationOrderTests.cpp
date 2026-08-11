#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheClassGraphDeclarationOrderTests_Private
{
	constexpr const TCHAR* ModuleName =
		TEXT("ASCacheV2ClassGraphDeclarationOrder");
	constexpr const TCHAR* AlphaName = TEXT("UCacheV2OrderAlpha");
	constexpr const TCHAR* BetaName = TEXT("UCacheV2OrderBeta");

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
			TEXT("CacheV2ClassGraphDeclarationOrder"),
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

	static const FString& GetAlphaFirstSource()
	{
		static const FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheV2OrderAlpha : UObject
			{
				UPROPERTY()
					UCacheV2OrderBeta Peer;

				UPROPERTY()
					int Value = 14;

				UFUNCTION()
					int ReadPeerValue()
				{
					return Peer.Value;
				}
			}

			UCLASS()
			class UCacheV2OrderBeta : UObject
			{
				UPROPERTY()
					UCacheV2OrderAlpha Peer;

				UPROPERTY()
					int Value = 28;

				UFUNCTION()
					int ReadPeerValue()
				{
					return Peer.Value;
				}
			}
		)AS");
		return Source;
	}

	static const FString& GetBetaFirstSource()
	{
		static const FString Source = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheV2OrderBeta : UObject
			{
				UPROPERTY()
					UCacheV2OrderAlpha Peer;

				UPROPERTY()
					int Value = 28;

				UFUNCTION()
					int ReadPeerValue()
				{
					return Peer.Value;
				}
			}

			UCLASS()
			class UCacheV2OrderAlpha : UObject
			{
				UPROPERTY()
					UCacheV2OrderBeta Peer;

				UPROPERTY()
					int Value = 14;

				UFUNCTION()
					int ReadPeerValue()
				{
					return Peer.Value;
				}
			}
		)AS");
		return Source;
	}

	struct FTypeObservation
	{
		FAngelscriptStableTypeKey TypeKey;
		FAngelscriptCacheRecordId RecordId;
	};

	struct FDebugObservation
	{
		FAngelscriptHash256 DebugHash;
		FAngelscriptCacheRecordId RecordId;
	};

	struct FFunctionObservation
	{
		FAngelscriptCacheRecordId BodyRecordId;
		FAngelscriptCacheRecordId DebugRecordId;
		FAngelscriptHash256 ExpectedDeclarationAbi;
		FAngelscriptHash256 FunctionSource;
		FAngelscriptHash256 FunctionInput;
		FAngelscriptHash256 Execution;
		FAngelscriptHash256 Debug;
		FAngelscriptArtifactProfileKey Profile;
		FString CanonicalDeclaration;
	};

	struct FArtifactObservation
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptCacheRecordId SourceIndexRecordId;
		FAngelscriptCacheRecordId ModuleInterfaceRecordId;
		FAngelscriptCacheRecordId ModuleStateRecordId;
		FAngelscriptCacheRecordId ModuleSnapshotRecordId;
		TMap<FString, FTypeObservation> TypesByName;
		TMap<FString, FFunctionObservation> FunctionsByKey;
	};

	static bool CaptureArtifacts(
		FAutomationTestBase& Test,
		const FString& Source,
		const TCHAR* Label,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
	{
		FAngelscriptTestFixture Producer(
			Test, ETestEngineMode::IsolatedFull);
		if (!Producer.IsValid())
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s producer Engine is invalid"), Label));
			return false;
		}
		asIScriptModule* ScriptModule = Producer.BuildModule(
			"ASCacheV2ClassGraphDeclarationOrder", Source);
		if (ScriptModule == nullptr)
		{
			return false;
		}
		TSharedPtr<FAngelscriptModuleDesc> Module =
			Producer.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid() || Module->Classes.Num() != 2)
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s producer did not expose two classes"), Label));
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("V3.14 %s capture: Error=%u Records=%d Graph=%u Routes=%d ModuleKey=%s Source=%s Detail=%s"),
			Label,
			static_cast<uint32>(Capture.Error),
			OutArtifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			OutArtifacts.ValidatedFunctionArtifactIdentities.Num(),
			*OutArtifacts.ModuleKey.Hash.ToHexString(),
			*OutArtifacts.SourceSnapshot.ToHexString(),
			*Capture.Detail));
		return Capture.IsSuccess();
	}

	static bool ObserveArtifacts(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const TCHAR* Label,
		FArtifactObservation& Out)
	{
		Out = {};
		Out.ModuleKey = Artifacts.ModuleKey;
		TMap<FString, FDebugObservation> DebugByFunctionKey;
		TMap<FString, FString> DeclarationsByStableKey;
		int32 SourceCount = 0;
		int32 InterfaceCount = 0;
		int32 StateCount = 0;
		int32 SnapshotCount = 0;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;

		for (const FAngelscriptPreparedRecord& Prepared : Artifacts.Records)
		{
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			const FAngelscriptCacheValidationResult Decode =
				FAngelscriptDecodedCacheRecord::TryDecode(
					Prepared.RecordId,
					Prepared.CanonicalPayload,
					Limits,
					Budget,
					Decoded);
			if (!Decode.IsSuccess() || !Decoded.IsSet())
			{
				Test.AddError(FString::Printf(
					TEXT("V3.14 %s failed to decode kind %u at %llu"),
					Label,
					static_cast<uint32>(Prepared.RecordId.Kind),
					Decode.ByteOffset));
				return false;
			}

			const FAngelscriptDecodedCacheRecord& Record = Decoded.GetValue().Get();
			if (Record.TryGetSourceIndex() != nullptr)
			{
				++SourceCount;
				Out.SourceIndexRecordId = Prepared.RecordId;
			}
			if (const FAngelscriptCachedModuleInterface* Interface =
				Record.TryGetModuleInterface())
			{
				++InterfaceCount;
				Out.ModuleInterfaceRecordId = Prepared.RecordId;
				for (const FAngelscriptCachedDeclaration& Declaration
					: Interface->Declarations)
				{
					if (Declaration.DeclarationKind
						== EAngelscriptCacheDeclarationKind::Function)
					{
						DeclarationsByStableKey.Add(
							Declaration.StableKey.ToHexString(),
							Declaration.CanonicalDeclaration);
					}
				}
			}
			if (const FAngelscriptCachedTypeSchema* Type =
				Record.TryGetTypeSchema())
			{
				FTypeObservation TypeObservation;
				TypeObservation.TypeKey = Type->TypeKey;
				TypeObservation.RecordId = Prepared.RecordId;
				if (Out.TypesByName.Contains(Type->CanonicalName))
				{
					Test.AddError(FString::Printf(
						TEXT("V3.14 %s contains duplicate type %s"),
						Label, *Type->CanonicalName));
					return false;
				}
				Out.TypesByName.Add(Type->CanonicalName, TypeObservation);
			}
			if (Record.TryGetModuleState() != nullptr)
			{
				++StateCount;
				Out.ModuleStateRecordId = Prepared.RecordId;
			}
			if (const FAngelscriptCachedFunctionBody* Function =
				Record.TryGetFunctionBody())
			{
				const FString Key =
					Function->Identity.FunctionKey.Hash.ToHexString();
				FFunctionObservation FunctionObservation;
				FunctionObservation.BodyRecordId = Prepared.RecordId;
				FunctionObservation.ExpectedDeclarationAbi =
					Function->ExpectedDeclarationAbi;
				FunctionObservation.FunctionSource =
					Function->FunctionSourceDigest.Hash;
				FunctionObservation.FunctionInput =
					Function->FunctionInputDigest.Hash;
				FunctionObservation.Execution =
					Function->Identity.Content.Execution;
				FunctionObservation.Debug =
					Function->Identity.Content.Debug;
				FunctionObservation.Profile = Function->Identity.Profile;
				if (Out.FunctionsByKey.Contains(Key))
				{
					Test.AddError(FString::Printf(
						TEXT("V3.14 %s contains duplicate function key %s"),
						Label, *Key));
					return false;
				}
				Out.FunctionsByKey.Add(Key, MoveTemp(FunctionObservation));
			}
			if (const FAngelscriptCachedDebugSidecar* Debug =
				Record.TryGetDebugSidecar())
			{
				const FString Key = Debug->FunctionKey.Hash.ToHexString();
				FDebugObservation DebugObservation;
				DebugObservation.DebugHash = Debug->DebugHash;
				DebugObservation.RecordId = Prepared.RecordId;
				if (DebugByFunctionKey.Contains(Key))
				{
					Test.AddError(FString::Printf(
						TEXT("V3.14 %s contains duplicate debug key %s"),
						Label, *Key));
					return false;
				}
				DebugByFunctionKey.Add(Key, DebugObservation);
			}
			if (Record.TryGetModuleSnapshot() != nullptr)
			{
				++SnapshotCount;
				Out.ModuleSnapshotRecordId = Prepared.RecordId;
			}
		}

		for (TPair<FString, FFunctionObservation>& Pair
			: Out.FunctionsByKey)
		{
			const FDebugObservation* Debug =
				DebugByFunctionKey.Find(Pair.Key);
			const FString* Declaration =
				DeclarationsByStableKey.Find(Pair.Key);
			if (Debug == nullptr || Declaration == nullptr
				|| Debug->DebugHash != Pair.Value.Debug)
			{
				Test.AddError(FString::Printf(
					TEXT("V3.14 %s function %s lacks a matching declaration/debug record"),
					Label, *Pair.Key));
				return false;
			}
			Pair.Value.DebugRecordId = Debug->RecordId;
			Pair.Value.CanonicalDeclaration = *Declaration;
		}

		const bool bComplete = SourceCount == 1
			&& InterfaceCount == 1
			&& StateCount == 1
			&& SnapshotCount == 1
			&& Out.TypesByName.Num() == 2
			&& Out.FunctionsByKey.Num()
				== Artifacts.ValidatedFunctionArtifactIdentities.Num()
			&& DebugByFunctionKey.Num() == Out.FunctionsByKey.Num();
		if (!bComplete)
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s inventory incomplete: Source=%d Interface=%d Types=%d State=%d Functions=%d Debug=%d Snapshot=%d Identities=%d"),
				Label, SourceCount, InterfaceCount, Out.TypesByName.Num(),
				StateCount, Out.FunctionsByKey.Num(),
				DebugByFunctionKey.Num(), SnapshotCount,
				Artifacts.ValidatedFunctionArtifactIdentities.Num()));
		}
		return bComplete;
	}

	static bool TestRecordEqual(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const FAngelscriptCacheRecordId& Expected,
		const FAngelscriptCacheRecordId& Actual)
	{
		return Test.TestTrue(
			*FString::Printf(
				TEXT("%s expected=%u:%s actual=%u:%s"),
				Label,
				static_cast<uint32>(Expected.Kind),
				*Expected.ContentHash.ToHexString(),
				static_cast<uint32>(Actual.Kind),
				*Actual.ContentHash.ToHexString()),
			Expected == Actual);
	}

	static bool CompareArtifacts(
		FAutomationTestBase& Test,
		const FArtifactObservation& AlphaFirst,
		const FArtifactObservation& BetaFirst,
		int32& OutChangedDebugCount,
		int32& OutChangedBodyRecordCount)
	{
		bool bMatches = true;
		OutChangedDebugCount = 0;
		OutChangedBodyRecordCount = 0;
		bMatches &= Test.TestTrue(
			TEXT("ModuleKey must ignore sibling declaration order"),
			AlphaFirst.ModuleKey == BetaFirst.ModuleKey);
		bMatches &= Test.TestTrue(
			TEXT("SourceIndex must observe reordered source bytes"),
			AlphaFirst.SourceIndexRecordId
				!= BetaFirst.SourceIndexRecordId);
		bMatches &= TestRecordEqual(
			Test, TEXT("ModuleInterface RecordId"),
			AlphaFirst.ModuleInterfaceRecordId,
			BetaFirst.ModuleInterfaceRecordId);
		bMatches &= TestRecordEqual(
			Test, TEXT("ModuleState RecordId"),
			AlphaFirst.ModuleStateRecordId,
			BetaFirst.ModuleStateRecordId);
		bMatches &= Test.TestTrue(
			TEXT("ModuleSnapshot must change when its source/debug graph changes"),
			AlphaFirst.ModuleSnapshotRecordId
				!= BetaFirst.ModuleSnapshotRecordId);

		TArray<FString> TypeNames;
		AlphaFirst.TypesByName.GetKeys(TypeNames);
		TypeNames.Sort();
		bMatches &= Test.TestEqual(
			TEXT("Type count must ignore declaration order"),
			BetaFirst.TypesByName.Num(), TypeNames.Num());
		for (const FString& TypeName : TypeNames)
		{
			const FTypeObservation* Expected =
				AlphaFirst.TypesByName.Find(TypeName);
			const FTypeObservation* Actual =
				BetaFirst.TypesByName.Find(TypeName);
			bMatches &= Test.TestNotNull(
				*FString::Printf(TEXT("Reordered type %s"), *TypeName), Actual);
			if (Expected == nullptr || Actual == nullptr)
			{
				continue;
			}
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s StableTypeKey"), *TypeName),
				Expected->TypeKey == Actual->TypeKey);
			bMatches &= TestRecordEqual(
				Test,
				*FString::Printf(TEXT("%s TypeSchema RecordId"), *TypeName),
				Expected->RecordId,
				Actual->RecordId);
		}

		TArray<FString> FunctionKeys;
		AlphaFirst.FunctionsByKey.GetKeys(FunctionKeys);
		FunctionKeys.Sort();
		bMatches &= Test.TestEqual(
			TEXT("Function count must ignore declaration order"),
			BetaFirst.FunctionsByKey.Num(), FunctionKeys.Num());
		for (const FString& FunctionKey : FunctionKeys)
		{
			const FFunctionObservation* Expected =
				AlphaFirst.FunctionsByKey.Find(FunctionKey);
			const FFunctionObservation* Actual =
				BetaFirst.FunctionsByKey.Find(FunctionKey);
			bMatches &= Test.TestNotNull(
				*FString::Printf(TEXT("Reordered function %s"), *FunctionKey),
				Actual);
			if (Expected == nullptr || Actual == nullptr)
			{
				continue;
			}
			bMatches &= Test.TestEqual(
				*FString::Printf(TEXT("%s declaration"), *FunctionKey),
				Actual->CanonicalDeclaration,
				Expected->CanonicalDeclaration);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s declaration ABI"), *FunctionKey),
				Actual->ExpectedDeclarationAbi
					== Expected->ExpectedDeclarationAbi);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s function source digest"), *FunctionKey),
				Actual->FunctionSource == Expected->FunctionSource);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s function input digest"), *FunctionKey),
				Actual->FunctionInput == Expected->FunctionInput);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s execution content"), *FunctionKey),
				Actual->Execution == Expected->Execution);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s profile"), *FunctionKey),
				Actual->Profile.Hash == Expected->Profile.Hash);

			const bool bDebugRecordSame =
				Actual->DebugRecordId == Expected->DebugRecordId;
			const bool bBodyRecordSame =
				Actual->BodyRecordId == Expected->BodyRecordId;
			if (!bDebugRecordSame)
			{
				++OutChangedDebugCount;
			}
			if (!bBodyRecordSame)
			{
				++OutChangedBodyRecordCount;
			}
			bMatches &= Test.TestEqual(
				*FString::Printf(
					TEXT("%s FunctionBody must change iff its owned DebugSidecar changes"),
					*FunctionKey),
				bBodyRecordSame,
				bDebugRecordSame);
			bMatches &= Test.TestEqual(
				*FString::Printf(TEXT("%s debug-hash/link agreement"), *FunctionKey),
				Actual->Debug == Expected->Debug,
				bDebugRecordSame);
		}

		bMatches &= Test.TestTrue(
			TEXT("Opposite declaration order must exercise changed debug sidecars"),
			OutChangedDebugCount > 0);
		bMatches &= Test.TestEqual(
			TEXT("Every debug-driven FunctionBody RecordId change must be accounted for"),
			OutChangedBodyRecordCount,
			OutChangedDebugCount);
		return bMatches;
	}

	struct FRouteObservation
	{
		FString CanonicalDeclaration;
		FAngelscriptHash256 Execution;
		FAngelscriptArtifactProfileKey Profile;
		EAngelscriptCacheFunctionExecutionRoute Route =
			EAngelscriptCacheFunctionExecutionRoute::Vm;
		int32 NumericFunctionId = -1;
		bool bVerified = false;
	};

	struct FRestoreObservation
	{
		TMap<FString, FRouteObservation> RoutesByKey;
		int32 AlphaResult = 0;
		int32 BetaResult = 0;
	};

	static bool PrepareValidatedGeneration(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCachePreparedColdGeneration& OutPrepared,
		TOptional<FAngelscriptValidatedGeneration>& OutValidated)
	{
		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		if (!PrepareAngelscriptCacheColdGeneration(
			Artifacts, Options, PackPolicy, Codec, OutPrepared).IsSuccess())
		{
			return false;
		}

		FPackSource Packs(OutPrepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		return ValidateAngelscriptCacheGeneration(
			OutPrepared.EncodedManifest.CompleteBytes,
			OutPrepared.EncodedManifest.ComputedGenerationId,
			Packs, Limits, Budget, Codec, OutValidated).IsSuccess()
			&& OutValidated.IsSet();
	}

	static bool RestoreAndObserve(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptValidatedGeneration& Validated,
		FRestoreObservation& Out)
	{
		Out = {};
		FAngelscriptTestFixture Consumer(
			Test, ETestEngineMode::IsolatedFull);
		if (!Consumer.IsValid())
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s consumer Engine is invalid"), Label));
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(), Validated,
				Artifacts.ModuleKey, Limits);
		Test.AddInfo(FString::Printf(
			TEXT("V3.14 %s restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			Label,
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		if (!Restore.IsSuccess() || Restore.RestoredTypeCount != 2
			|| Restore.RestoredFunctionCount
				!= static_cast<uint32>(
					Artifacts.ValidatedFunctionArtifactIdentities.Num()))
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s restore did not publish the complete graph"), Label));
			return false;
		}

		TSharedPtr<FAngelscriptModuleDesc> Module =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		if (!Module.IsValid() || Module->ScriptModule == nullptr
			|| Module->Classes.Num() != 2)
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s restored module is incomplete"), Label));
			return false;
		}
		TSharedPtr<FAngelscriptClassDesc> Alpha = Module->GetClass(AlphaName);
		TSharedPtr<FAngelscriptClassDesc> Beta = Module->GetClass(BetaName);
		if (!Alpha.IsValid() || !Beta.IsValid()
			|| Alpha->Class == nullptr || Beta->Class == nullptr
			|| Alpha->ScriptType == nullptr || Beta->ScriptType == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s restored classes are incomplete"), Label));
			return false;
		}

		FObjectProperty* AlphaPeer = CastField<FObjectProperty>(
			Alpha->Class->FindPropertyByName(TEXT("Peer")));
		FObjectProperty* BetaPeer = CastField<FObjectProperty>(
			Beta->Class->FindPropertyByName(TEXT("Peer")));
		if (AlphaPeer == nullptr || BetaPeer == nullptr
			|| AlphaPeer->PropertyClass != Beta->Class
			|| BetaPeer->PropertyClass != Alpha->Class)
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s restored cross-links are not consumer-owned"),
				Label));
			return false;
		}

		UObject* AlphaObject = NewObject<UObject>(
			GetTransientPackage(), Alpha->Class);
		UObject* BetaObject = NewObject<UObject>(
			GetTransientPackage(), Beta->Class);
		if (AlphaObject == nullptr || BetaObject == nullptr)
		{
			return false;
		}
		AlphaPeer->SetObjectPropertyValue_InContainer(AlphaObject, BetaObject);
		BetaPeer->SetObjectPropertyValue_InContainer(BetaObject, AlphaObject);
		FFunctionInvoker AlphaInvoker(
			Test, AlphaObject, FName(TEXT("ReadPeerValue")));
		FFunctionInvoker BetaInvoker(
			Test, BetaObject, FName(TEXT("ReadPeerValue")));
		if (!AlphaInvoker.IsValid() || !BetaInvoker.IsValid())
		{
			return false;
		}
		Out.AlphaResult = AlphaInvoker.CallAndReturn<int32>(INDEX_NONE);
		Out.BetaResult = BetaInvoker.CallAndReturn<int32>(INDEX_NONE);
		if (Out.AlphaResult != 28 || Out.BetaResult != 14)
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s execution mismatch: Alpha=%d Beta=%d"),
				Label, Out.AlphaResult, Out.BetaResult));
			return false;
		}

		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes =
			Consumer.GetEngine().GetFunctionRouteSnapshot();
		if (!Routes.IsValid())
		{
			return false;
		}
		for (const FAngelscriptCacheLiveFunctionRoute& Route
			: Routes->FunctionRoutes)
		{
			if (Route.ModuleKey != Artifacts.ModuleKey)
			{
				continue;
			}
			const FString Key = Route.Identity.FunctionKey.Hash.ToHexString();
			if (Route.Function == nullptr
				|| Route.NumericFunctionId != Route.Function->GetId()
				|| Route.Function->GetEngine()
					!= Consumer.GetEngine().GetScriptEngine()
				|| Out.RoutesByKey.Contains(Key))
			{
				Test.AddError(FString::Printf(
					TEXT("V3.14 %s route %s is invalid or duplicated"),
					Label, *Key));
				return false;
			}
			FRouteObservation Observation;
			Observation.CanonicalDeclaration = Route.CanonicalDeclaration;
			Observation.Execution = Route.Identity.Content.Execution;
			Observation.Profile = Route.Identity.Profile;
			Observation.Route = Route.SelectedExecutionRoute;
			Observation.NumericFunctionId = Route.NumericFunctionId;
			Observation.bVerified = Route.bHasVerifiedArtifactIdentity;
			Out.RoutesByKey.Add(Key, MoveTemp(Observation));
		}

		if (Out.RoutesByKey.Num()
			!= Artifacts.ValidatedFunctionArtifactIdentities.Num())
		{
			Test.AddError(FString::Printf(
				TEXT("V3.14 %s route count mismatch: Expected=%d Actual=%d"),
				Label,
				Artifacts.ValidatedFunctionArtifactIdentities.Num(),
				Out.RoutesByKey.Num()));
			return false;
		}
		return true;
	}

	static bool CompareRoutes(
		FAutomationTestBase& Test,
		const FRestoreObservation& AlphaFirst,
		const FRestoreObservation& BetaFirst,
		int32& OutDifferentNumericIdCount)
	{
		bool bMatches = Test.TestEqual(
			TEXT("Restored route counts must ignore declaration order"),
			BetaFirst.RoutesByKey.Num(), AlphaFirst.RoutesByKey.Num());
		OutDifferentNumericIdCount = 0;
		TArray<FString> Keys;
		AlphaFirst.RoutesByKey.GetKeys(Keys);
		Keys.Sort();
		for (const FString& Key : Keys)
		{
			const FRouteObservation* Expected =
				AlphaFirst.RoutesByKey.Find(Key);
			const FRouteObservation* Actual =
				BetaFirst.RoutesByKey.Find(Key);
			bMatches &= Test.TestNotNull(
				*FString::Printf(TEXT("Restored route %s"), *Key), Actual);
			if (Expected == nullptr || Actual == nullptr)
			{
				continue;
			}
			bMatches &= Test.TestEqual(
				*FString::Printf(TEXT("%s route declaration"), *Key),
				Actual->CanonicalDeclaration,
				Expected->CanonicalDeclaration);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s route execution identity"), *Key),
				Actual->Execution == Expected->Execution);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s route profile"), *Key),
				Actual->Profile.Hash == Expected->Profile.Hash);
			bMatches &= Test.TestEqual(
				*FString::Printf(TEXT("%s selected route"), *Key),
				Actual->Route, Expected->Route);
			bMatches &= Test.TestTrue(
				*FString::Printf(TEXT("%s verified route"), *Key),
				Actual->bVerified && Expected->bVerified);
			if (Actual->NumericFunctionId != Expected->NumericFunctionId)
			{
				++OutDifferentNumericIdCount;
			}
		}
		return bMatches;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheClassGraphDeclarationOrderTests,
	"Angelscript.TestModule.Cache.ClassGraphDeclarationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OppositeSiblingOrderPreservesStableRecordsRoutesAndCrossLinks)
	{
		using namespace
			AngelscriptCacheClassGraphDeclarationOrderTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts AlphaFirstArtifacts;
		FAngelscriptCacheCleanModuleArtifacts BetaFirstArtifacts;
		ASSERT_THAT(IsTrue(CaptureArtifacts(
			*TestRunner, GetAlphaFirstSource(), TEXT("alpha-first"),
			Options, AlphaFirstArtifacts)));
		ASSERT_THAT(IsTrue(CaptureArtifacts(
			*TestRunner, GetBetaFirstSource(), TEXT("beta-first"),
			Options, BetaFirstArtifacts)));

		FArtifactObservation AlphaFirstObservation;
		FArtifactObservation BetaFirstObservation;
		ASSERT_THAT(IsTrue(ObserveArtifacts(
			*TestRunner, AlphaFirstArtifacts, TEXT("alpha-first"),
			AlphaFirstObservation)));
		ASSERT_THAT(IsTrue(ObserveArtifacts(
			*TestRunner, BetaFirstArtifacts, TEXT("beta-first"),
			BetaFirstObservation)));
		int32 ChangedDebugCount = 0;
		int32 ChangedBodyRecordCount = 0;
		ASSERT_THAT(IsTrue(CompareArtifacts(
			*TestRunner,
			AlphaFirstObservation,
			BetaFirstObservation,
			ChangedDebugCount,
			ChangedBodyRecordCount)));

		FAngelscriptCachePreparedColdGeneration AlphaFirstPrepared;
		FAngelscriptCachePreparedColdGeneration BetaFirstPrepared;
		TOptional<FAngelscriptValidatedGeneration> AlphaFirstValidated;
		TOptional<FAngelscriptValidatedGeneration> BetaFirstValidated;
		ASSERT_THAT(IsTrue(PrepareValidatedGeneration(
			AlphaFirstArtifacts, Options,
			AlphaFirstPrepared, AlphaFirstValidated)));
		ASSERT_THAT(IsTrue(PrepareValidatedGeneration(
			BetaFirstArtifacts, Options,
			BetaFirstPrepared, BetaFirstValidated)));

		FRestoreObservation AlphaFirstRestore;
		FRestoreObservation BetaFirstRestore;
		ASSERT_THAT(IsTrue(RestoreAndObserve(
			*TestRunner, TEXT("alpha-first"), AlphaFirstArtifacts,
			AlphaFirstValidated.GetValue(), AlphaFirstRestore)));
		ASSERT_THAT(IsTrue(RestoreAndObserve(
			*TestRunner, TEXT("beta-first"), BetaFirstArtifacts,
			BetaFirstValidated.GetValue(), BetaFirstRestore)));
		int32 DifferentNumericIdCount = 0;
		ASSERT_THAT(IsTrue(CompareRoutes(
			*TestRunner,
			AlphaFirstRestore,
			BetaFirstRestore,
			DifferentNumericIdCount)));

		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.14 declaration-order parity: Types=%d Functions=%d DebugChanged=%d BodyRecordsChanged=%d Routes=%d NumericIdsDifferent=%d Results=%d/%d,%d/%d Interface=%s State=%s"),
			AlphaFirstObservation.TypesByName.Num(),
			AlphaFirstObservation.FunctionsByKey.Num(),
			ChangedDebugCount,
			ChangedBodyRecordCount,
			AlphaFirstRestore.RoutesByKey.Num(),
			DifferentNumericIdCount,
			AlphaFirstRestore.AlphaResult,
			AlphaFirstRestore.BetaResult,
			BetaFirstRestore.AlphaResult,
			BetaFirstRestore.BetaResult,
			*AlphaFirstObservation.ModuleInterfaceRecordId.ContentHash.ToHexString(),
			*AlphaFirstObservation.ModuleStateRecordId.ContentHash.ToHexString()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
