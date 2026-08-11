#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "as_objecttype.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheClassGraphFunctionSignatureRestoreTests_Private
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
			TEXT("CacheV2ClassGraphFunctionSignatureRestore"),
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

	static asCScriptFunction* FindMethod(
		const asCObjectType& Type,
		const char* Name)
	{
		asCScriptFunction* Function = static_cast<asCScriptFunction*>(
			const_cast<asCObjectType&>(Type).GetMethodByName(Name));
		return Function != nullptr && Function->objectType == &Type
			? Function : nullptr;
	}

	static bool CheckVmSignature(
		FAutomationTestBase& Test,
		const TCHAR* Phase,
		const asCObjectType& OwnerType,
		const asCObjectType& PeerType,
		asCScriptFunction*& OutFunction)
	{
		OutFunction = FindMethod(OwnerType, "EchoPeer");
		bool bResult = Test.TestNotNull(
			*FString::Printf(TEXT("%s VM method should exist"), Phase),
			OutFunction);
		if (OutFunction == nullptr)
		{
			return false;
		}

		bResult &= Test.TestEqual(
			*FString::Printf(TEXT("%s method should have one input"), Phase),
			static_cast<int32>(OutFunction->parameterTypes.GetLength()), 1);
		bResult &= Test.TestTrue(
			*FString::Printf(TEXT("%s return type must be the peer type"), Phase),
			OutFunction->returnType.GetTypeInfo() == &PeerType);
		if (OutFunction->parameterTypes.GetLength() == 1)
		{
			bResult &= Test.TestTrue(
				*FString::Printf(TEXT("%s parameter type must be the peer type"), Phase),
				OutFunction->parameterTypes[0].GetTypeInfo() == &PeerType);
		}
		Test.AddInfo(FString::Printf(
			TEXT("V3.11 %s VM signature: Function=%p OwnerType=%p ReturnType=%p ParameterType=%p PeerType=%p Declaration=%s"),
			Phase,
			OutFunction,
			&OwnerType,
			OutFunction->returnType.GetTypeInfo(),
			OutFunction->parameterTypes.GetLength() == 1
				? OutFunction->parameterTypes[0].GetTypeInfo() : nullptr,
			&PeerType,
			UTF8_TO_TCHAR(OutFunction->GetDeclaration(false, false, false))));
		return bResult;
	}

	struct FReflectedParameterSnapshot
	{
		FString Name;
		uint64 Flags = 0;
		bool bReturn = false;
		FString ObjectClass;
	};

	static TArray<FReflectedParameterSnapshot> CaptureReflectedSignature(
		FAutomationTestBase& Test,
		const TCHAR* Phase,
		UFunction& Function)
	{
		TArray<FReflectedParameterSnapshot> Snapshot;
		int32 Ordinal = 0;
		for (TFieldIterator<FProperty> It(&Function);
			It && It->HasAnyPropertyFlags(CPF_Parm); ++It, ++Ordinal)
		{
			const FObjectProperty* ObjectProperty =
				CastField<FObjectProperty>(*It);
			FReflectedParameterSnapshot& Entry = Snapshot.AddDefaulted_GetRef();
			Entry.Name = It->GetName();
			Entry.Flags = static_cast<uint64>(It->GetPropertyFlags());
			Entry.bReturn = It->HasAnyPropertyFlags(CPF_ReturnParm);
			Entry.ObjectClass = ObjectProperty != nullptr
				&& ObjectProperty->PropertyClass != nullptr
					? ObjectProperty->PropertyClass->GetName()
					: TEXT("<none>");
			Test.AddInfo(FString::Printf(
				TEXT("V3.11 %s reflection parameter: Ordinal=%d Name=%s Flags=0x%llx Return=%d ObjectClass=%s"),
				Phase,
				Ordinal,
				*Entry.Name,
				Entry.Flags,
				Entry.bReturn ? 1 : 0,
				*Entry.ObjectClass));
		}
		return Snapshot;
	}

	static bool CompareReflectedSignatures(
		FAutomationTestBase& Test,
		const TConstArrayView<FReflectedParameterSnapshot> Producer,
		const TConstArrayView<FReflectedParameterSnapshot> Consumer)
	{
		bool bResult = Test.TestEqual(
			TEXT("Producer and consumer reflected parameter counts must match"),
			Consumer.Num(), Producer.Num());
		for (int32 Index = 0;
			Index < FMath::Min(Producer.Num(), Consumer.Num()); ++Index)
		{
			bResult &= Test.TestEqual(
				*FString::Printf(TEXT("Parameter %d name must match"), Index),
				Consumer[Index].Name, Producer[Index].Name);
			bResult &= Test.TestEqual(
				*FString::Printf(TEXT("Parameter %d flags must match"), Index),
				Consumer[Index].Flags, Producer[Index].Flags);
			bResult &= Test.TestEqual(
				*FString::Printf(TEXT("Parameter %d return marker must match"), Index),
				Consumer[Index].bReturn, Producer[Index].bReturn);
			bResult &= Test.TestEqual(
				*FString::Printf(TEXT("Parameter %d object class must match"), Index),
				Consumer[Index].ObjectClass, Producer[Index].ObjectClass);
		}
		return bResult;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheClassGraphFunctionSignatureRestoreTests,
	"Angelscript.TestModule.Cache.ClassGraphFunctionSignatureRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CrossClassParameterAndReturnBindOnlyToConsumerTypes)
	{
		using namespace
			AngelscriptCacheClassGraphFunctionSignatureRestoreTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		TArray<FReflectedParameterSnapshot> ProducerReflectedSignature;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid()));
			const FString Source = ASTEST_AS(R"AS(
				UCLASS()
				class UCacheV2SignaturePeer : UObject
				{
					UPROPERTY()
					int Value = 42;
				}

				UCLASS()
				class UCacheV2SignatureOwner : UObject
				{
					UFUNCTION()
					UCacheV2SignaturePeer EchoPeer(UCacheV2SignaturePeer Input)
					{
						return Input;
					}
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2ClassGraphFunctionSignatureRestore", Source);
			ASSERT_THAT(IsNotNull(ScriptModule));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid()));
			ASSERT_THAT(AreEqual(2, Module->Classes.Num()));

			const TSharedRef<FAngelscriptClassDesc>* ProducerOwner =
				FindClass(*Module, TEXT("UCacheV2SignatureOwner"));
			const TSharedRef<FAngelscriptClassDesc>* ProducerPeer =
				FindClass(*Module, TEXT("UCacheV2SignaturePeer"));
			ASSERT_THAT(IsNotNull(ProducerOwner));
			ASSERT_THAT(IsNotNull(ProducerPeer));
			asCScriptFunction* ProducerFunction = nullptr;
			ASSERT_THAT(IsTrue(CheckVmSignature(
				*TestRunner,
				TEXT("producer"),
				*static_cast<asCObjectType*>((*ProducerOwner)->ScriptType),
				*static_cast<asCObjectType*>((*ProducerPeer)->ScriptType),
				ProducerFunction)));
			UFunction* ProducerReflectedFunction =
				(*ProducerOwner)->Class->FindFunctionByName(TEXT("EchoPeer"));
			ASSERT_THAT(IsNotNull(ProducerReflectedFunction));
			ProducerReflectedSignature = CaptureReflectedSignature(
				*TestRunner, TEXT("producer"), *ProducerReflectedFunction);

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("V3.11 cross-class signature capture: Error=%u Records=%d Graph=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
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
			TEXT("V3.11 cross-class signature restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess()));
		ASSERT_THAT(AreEqual(uint32(2), Restore.RestoredTypeCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2ClassGraphFunctionSignatureRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		const TSharedRef<FAngelscriptClassDesc>* ConsumerOwner =
			FindClass(*RestoredModule, TEXT("UCacheV2SignatureOwner"));
		const TSharedRef<FAngelscriptClassDesc>* ConsumerPeer =
			FindClass(*RestoredModule, TEXT("UCacheV2SignaturePeer"));
		ASSERT_THAT(IsNotNull(ConsumerOwner));
		ASSERT_THAT(IsNotNull(ConsumerPeer));
		ASSERT_THAT(IsNotNull((*ConsumerOwner)->ScriptType));
		ASSERT_THAT(IsNotNull((*ConsumerPeer)->ScriptType));
		ASSERT_THAT(IsNotNull((*ConsumerOwner)->Class));
		ASSERT_THAT(IsNotNull((*ConsumerPeer)->Class));

		asCScriptFunction* ConsumerFunction = nullptr;
		ASSERT_THAT(IsTrue(CheckVmSignature(
			*TestRunner,
			TEXT("consumer"),
			*static_cast<asCObjectType*>((*ConsumerOwner)->ScriptType),
			*static_cast<asCObjectType*>((*ConsumerPeer)->ScriptType),
			ConsumerFunction)));

		UFunction* ReflectedFunction =
			(*ConsumerOwner)->Class->FindFunctionByName(TEXT("EchoPeer"));
		ASSERT_THAT(IsNotNull(ReflectedFunction));
		const TArray<FReflectedParameterSnapshot> ConsumerReflectedSignature =
			CaptureReflectedSignature(
				*TestRunner, TEXT("consumer"), *ReflectedFunction);
		ASSERT_THAT(IsTrue(CompareReflectedSignatures(
			*TestRunner,
			ProducerReflectedSignature,
			ConsumerReflectedSignature)));
		FObjectProperty* InputProperty = FindFProperty<FObjectProperty>(
			ReflectedFunction, TEXT("Input"));
		FObjectProperty* ReturnProperty = CastField<FObjectProperty>(
			ReflectedFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(InputProperty));
		ASSERT_THAT(IsNotNull(ReturnProperty));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*ConsumerPeer)->Class),
			InputProperty->PropertyClass));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*ConsumerPeer)->Class),
			ReturnProperty->PropertyClass));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V3.11 consumer reflection signature: UFunction=%p InputProperty=%p InputClass=%p ReturnProperty=%p ReturnClass=%p PeerClass=%p"),
			ReflectedFunction,
			InputProperty,
			InputProperty->PropertyClass.Get(),
			ReturnProperty,
			ReturnProperty->PropertyClass.Get(),
			(*ConsumerPeer)->Class));

		UObject* OwnerObject = NewObject<UObject>(
			GetTransientPackage(), (*ConsumerOwner)->Class);
		UObject* PeerObject = NewObject<UObject>(
			GetTransientPackage(), (*ConsumerPeer)->Class);
		ASSERT_THAT(IsNotNull(OwnerObject));
		ASSERT_THAT(IsNotNull(PeerObject));
		FFunctionInvoker Invoker(
			*TestRunner, OwnerObject, FName(TEXT("EchoPeer")));
		ASSERT_THAT(IsTrue(Invoker.IsValid()));
		Invoker.AddParam<UObject*>(PeerObject);
		ASSERT_THAT(AreEqual(
			PeerObject,
			Invoker.CallAndReturn<UObject*>(nullptr),
			TEXT("The restored reflected method should return the consumer peer object")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
