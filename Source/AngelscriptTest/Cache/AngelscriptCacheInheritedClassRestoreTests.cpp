#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestFixture.h"
#include "UObject/UObjectGlobals.h"

#include "as_module.h"
#include "as_objecttype.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheInheritedClassRestoreTests,
	"Angelscript.TestModule.Cache.InheritedClassRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
			TEXT("CacheV2InheritedClassRestore"),
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
		const FString& ClassName)
	{
		return Module.Classes.FindByPredicate(
			[&ClassName](const TSharedRef<FAngelscriptClassDesc>& Candidate)
			{
				return Candidate->ClassName == ClassName;
			});
	}

public:
	TEST_METHOD(ScriptBaseAndDerivedClassRoundTripIntoFreshEngine)
	{
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Producer.IsValid(),
				TEXT("Producer Engine should initialize")));

			const FString ScriptSource = ASTEST_AS(R"AS(
				UCLASS()
				class UCacheV2RestoredBaseObject : UObject
				{
					int BaseValue()
					{
						return 40;
					}
				}

				UCLASS()
				class UCacheV2RestoredDerivedObject : UCacheV2RestoredBaseObject
				{
					UFUNCTION()
					int ReflectedValue()
					{
						return BaseValue() + 2;
					}
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2InheritedClassRestore", ScriptSource);
			ASSERT_THAT(IsNotNull(ScriptModule,
				TEXT("Inherited-class producer module should compile")));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			ASSERT_THAT(IsTrue(Module.IsValid(),
				TEXT("Producer should retain both class descriptors")));
			ASSERT_THAT(AreEqual(2, Module->Classes.Num()));
			for (const TSharedRef<FAngelscriptClassDesc>& Class
				: Module->Classes)
			{
				const asCObjectType* Type =
					static_cast<const asCObjectType*>(Class->ScriptType);
				TestRunner->AddInfo(FString::Printf(
					TEXT("IC-445 producer type: Name=%s Super=%s CodeSuper=%s SuperIsCode=%d VmDerived=%s VmShadow=%s Size=%d Align=%d BaseBoundary=%d LocalProperties=%u Methods=%u Vft=%u"),
					*Class->ClassName,
					*Class->SuperClass,
					Class->CodeSuperClass != nullptr
						? *Class->CodeSuperClass->GetPathName() : TEXT("<none>"),
					Class->bSuperIsCodeClass ? 1 : 0,
					Type != nullptr && Type->derivedFrom != nullptr
						? UTF8_TO_TCHAR(Type->derivedFrom->GetName()) : TEXT("<none>"),
					Type != nullptr && Type->shadowType != nullptr
						? UTF8_TO_TCHAR(Type->shadowType->GetName()) : TEXT("<none>"),
					Type != nullptr ? Type->size : -1,
					Type != nullptr ? Type->alignment : -1,
					Type != nullptr ? Type->basePropertyOffset : -1,
					Type != nullptr ? Type->localProperties.GetLength() : 0,
					Type != nullptr ? Type->methods.GetLength() : 0,
					Type != nullptr ? Type->virtualFunctionTable.GetLength() : 0));
				if (Type != nullptr)
				{
					for (asUINT MethodIndex = 0;
						MethodIndex < Type->methods.GetLength(); ++MethodIndex)
					{
						const asCScriptFunction* Method =
							Type->engine->GetScriptFunction(Type->methods[MethodIndex]);
						TestRunner->AddInfo(FString::Printf(
							TEXT("IC-448 producer method: Type=%s Slot=%u Declaration=%s Owner=%s Vf=%d"),
							*Class->ClassName, MethodIndex,
							Method != nullptr
								? UTF8_TO_TCHAR(Method->GetDeclaration(true, true, false))
								: TEXT("<null>"),
							Method != nullptr && Method->objectType != nullptr
								? UTF8_TO_TCHAR(Method->objectType->GetName())
								: TEXT("<none>"),
							Method != nullptr ? Method->vfTableIdx : -1));
					}
					for (asUINT VftIndex = 0;
						VftIndex < Type->virtualFunctionTable.GetLength(); ++VftIndex)
					{
						const asCScriptFunction* Method =
							Type->virtualFunctionTable[VftIndex];
						TestRunner->AddInfo(FString::Printf(
							TEXT("IC-448 producer vft: Type=%s Slot=%u Declaration=%s Owner=%s Vf=%d"),
							*Class->ClassName, VftIndex,
							Method != nullptr
								? UTF8_TO_TCHAR(Method->GetDeclaration(true, true, false))
								: TEXT("<null>"),
							Method != nullptr && Method->objectType != nullptr
								? UTF8_TO_TCHAR(Method->objectType->GetName())
								: TEXT("<none>"),
							Method != nullptr ? Method->vfTableIdx : -1));
					}
				}
			}
			asCModule* VmModule = static_cast<asCModule*>(ScriptModule);
			for (asUINT FunctionIndex = 0;
				FunctionIndex < VmModule->scriptFunctions.GetLength();
				++FunctionIndex)
			{
				const asCScriptFunction* Function =
					VmModule->scriptFunctions[FunctionIndex];
				TestRunner->AddInfo(FString::Printf(
					TEXT("IC-445 producer function: Ordinal=%u Declaration=%s Invocation=%u ObjectType=%s OwnerType=%s Traits=0x%08x"),
					FunctionIndex,
					Function != nullptr
						? UTF8_TO_TCHAR(Function->GetDeclaration(true, true, false))
						: TEXT("<null>"),
					Function != nullptr
						? static_cast<uint32>(Function->artifactInvocationKind) : 0,
					Function != nullptr && Function->objectType != nullptr
						? UTF8_TO_TCHAR(Function->objectType->GetName()) : TEXT("<none>"),
					Function != nullptr && Function->artifactOwnerType != nullptr
						? UTF8_TO_TCHAR(Function->artifactOwnerType->GetName()) : TEXT("<none>"),
					Function != nullptr ? Function->traits.traits : 0));
			}

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			TestRunner->AddInfo(FString::Printf(
				TEXT("IC-445 inherited-class capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			ASSERT_THAT(IsTrue(Capture.IsSuccess(),
				TEXT("A complete script inheritance chain should be cacheable")));
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		const FAngelscriptCacheCleanCaptureResult Preparation =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts, Options, PackPolicy, Codec, Prepared);
		ASSERT_THAT(IsTrue(Preparation.IsSuccess(),
			TEXT("Inherited-class artifacts should form a generation")));

		FPackSource Packs(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				Packs,
				Limits,
				Budget,
				Codec,
				Validated);
		ASSERT_THAT(IsTrue(Validation.IsSuccess(),
			TEXT("Inherited-class generation should validate")));
		ASSERT_THAT(IsTrue(Validated.IsSet(),
			TEXT("Validation should promote the inherited-class generation")));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid(),
			TEXT("Consumer Engine should initialize")));
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(),
				Validated.GetValue(),
				Artifacts.ModuleKey,
				Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("IC-445 inherited-class restore: Error=%u Stage=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredTypeCount,
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess(),
			TEXT("A fresh Engine should restore both classes atomically")));
		ASSERT_THAT(AreEqual(uint32(2), Restore.RestoredTypeCount));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2InheritedClassRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		ASSERT_THAT(AreEqual(2, RestoredModule->Classes.Num()));
		const TSharedRef<FAngelscriptClassDesc>* RestoredBase = FindClass(
			*RestoredModule, TEXT("UCacheV2RestoredBaseObject"));
		const TSharedRef<FAngelscriptClassDesc>* RestoredDerived = FindClass(
			*RestoredModule, TEXT("UCacheV2RestoredDerivedObject"));
		ASSERT_THAT(IsNotNull(RestoredBase));
		ASSERT_THAT(IsNotNull(RestoredDerived));
		ASSERT_THAT(IsNotNull((*RestoredBase)->Class));
		ASSERT_THAT(IsNotNull((*RestoredDerived)->Class));
		ASSERT_THAT(AreEqual(
			static_cast<UClass*>((*RestoredBase)->Class),
			(*RestoredDerived)->Class->GetSuperClass(),
			TEXT("The restored UClass hierarchy should retain its script parent")));

		UFunction* ReflectedValue =
			(*RestoredDerived)->Class->FindFunctionByName(
				TEXT("ReflectedValue"));
		ASSERT_THAT(IsNotNull(ReflectedValue,
			TEXT("The derived cached UFUNCTION should be recreated")));
		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), (*RestoredDerived)->Class);
		ASSERT_THAT(IsNotNull(Instance,
			TEXT("The restored derived UClass should create an instance")));
		FFunctionInvoker Invoker(
			*TestRunner, Instance, FName(TEXT("ReflectedValue")));
		ASSERT_THAT(IsTrue(Invoker.IsValid(),
			TEXT("The restored derived UFUNCTION should be invokable")));
		ASSERT_THAT(AreEqual(42,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("The derived body should dispatch its restored base method")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
