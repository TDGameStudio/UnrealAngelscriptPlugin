#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadDependencyTests,
	"Angelscript.TestModule.HotReload.Dependency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ProviderModuleName = FName(TEXT("HotReload.Dependency.HotReloadDependencyProvider"));
	inline static const FName ConsumerModuleName = FName(TEXT("HotReload.Dependency.HotReloadDependencyConsumer"));
	inline static const FString ProviderRelativeScriptPath = FString(TEXT("HotReload/Dependency/HotReloadDependencyProvider.as"));
	inline static const FString ConsumerRelativeScriptPath = FString(TEXT("HotReload/Dependency/HotReloadDependencyConsumer.as"));

	static FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("HotReloadDependency"));
	}

	static FString WriteFixture(const FString& RelativePath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetFixtureRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	static bool CompileFiles(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		ECompileType CompileType,
		const TArray<TPair<FString, FString>>& Files,
		int32 ExpectedCompiledModuleCount)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FAngelscriptPreprocessor Preprocessor;
		for (const TPair<FString, FString>& File : Files)
		{
			Preprocessor.AddFile(File.Key, File.Value);
		}

		if (!LocalAssert.IsTrue(Preprocessor.Preprocess(), TEXT("HotReload dependency test should preprocess fixture files")))
		{
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(CompileType, Preprocessor.GetModulesToCompile(), CompiledModules);
		if (!LocalAssert.IsTrue(
				CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled,
				TEXT("HotReload dependency test compile should be handled")))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedCompiledModuleCount,
			CompiledModules.Num(),
			TEXT("HotReload dependency test should compile the expected module count"));
	}

	static void CleanupFixtureFiles(const TArray<FString>& AbsolutePaths)
	{
		for (const FString& AbsolutePath : AbsolutePaths)
		{
			IFileManager::Get().Delete(*AbsolutePath, false, true);
		}
		IFileManager::Get().DeleteDirectory(*GetFixtureRoot(), false, true);
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

	TEST_METHOD(ProviderSoftReloadRebindsDeclaredImportConsumer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FString ProviderV1Source = ASTEST_AS(R"AS(
			int SharedValue()
			{
				return 11;
			}
			)AS");

		const FString ProviderV2Source = ASTEST_AS(R"AS(
			int SharedValue()
			{
				return 29;
			}
			)AS");

		const FString ConsumerSource = ASTEST_AS(R"AS(
			import int SharedValue() from "HotReload.Dependency.HotReloadDependencyProvider";

			int Entry()
			{
				return SharedValue();
			}
			)AS");

		const FString ProviderAbsolutePath = WriteFixture(ProviderRelativeScriptPath, ProviderV1Source);
		const FString ConsumerAbsolutePath = WriteFixture(ConsumerRelativeScriptPath, ConsumerSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ConsumerModuleName.ToString());
			Engine.DiscardModule(*ProviderModuleName.ToString());
			CleanupFixtureFiles({ ProviderAbsolutePath, ConsumerAbsolutePath });
		};

		ASSERT_THAT(IsTrue(
			CompileFiles(
				*TestRunner,
				Engine,
				ECompileType::SoftReloadOnly,
				{
					{ ProviderRelativeScriptPath, ProviderAbsolutePath },
					{ ConsumerRelativeScriptPath, ConsumerAbsolutePath }
				},
				2),
			TEXT("Initial provider and consumer compile should succeed")));

		TSharedPtr<FAngelscriptModuleDesc> ConsumerModuleBeforeReload = Engine.GetModule(ConsumerModuleName.ToString());
		ASSERT_THAT(IsTrue(ConsumerModuleBeforeReload.IsValid(), TEXT("Consumer module should be active before provider reload")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ConsumerModuleBeforeReload->ScriptModule->GetImportedFunctionCount()), TEXT("Consumer should declare one provider import before reload")));

		int32 ResultBeforeReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, ConsumerRelativeScriptPath, ConsumerModuleName, TEXT("int Entry()"), ResultBeforeReload),
			TEXT("Consumer entry should execute before provider reload")));
		ASSERT_THAT(AreEqual(11, ResultBeforeReload, TEXT("Consumer should call provider V1 before reload")));

		WriteFixture(ProviderRelativeScriptPath, ProviderV2Source);
		ASSERT_THAT(IsTrue(
			CompileFiles(
				*TestRunner,
				Engine,
				ECompileType::SoftReloadOnly,
				{ { ProviderRelativeScriptPath, ProviderAbsolutePath } },
				1),
			TEXT("Provider-only soft reload should compile")));

		TSharedPtr<FAngelscriptModuleDesc> ConsumerModuleAfterReload = Engine.GetModule(ConsumerModuleName.ToString());
		ASSERT_THAT(IsTrue(ConsumerModuleAfterReload.IsValid(), TEXT("Consumer module should remain active after provider reload")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ConsumerModuleAfterReload->ScriptModule->GetImportedFunctionCount()), TEXT("Consumer should keep one declared import after provider reload")));
		ASSERT_THAT(AreEqual(
			ProviderModuleName.ToString(),
			FString(UTF8_TO_TCHAR(ConsumerModuleAfterReload->ScriptModule->GetImportedFunctionSourceModule(0))),
			TEXT("Consumer import source module should remain bound to the provider")));

		int32 ResultAfterReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, ConsumerRelativeScriptPath, ConsumerModuleName, TEXT("int Entry()"), ResultAfterReload),
			TEXT("Consumer entry should execute after provider reload")));
		ASSERT_THAT(AreEqual(29, ResultAfterReload, TEXT("Consumer should call provider V2 after provider-only reload")));
	}

	TEST_METHOD(ProviderStructFullReloadRetargetsConsumerFunctionParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ConsumerModuleName.ToString());
			Engine.DiscardModule(*ProviderModuleName.ToString());
		};

		const FString ProviderV1Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadDependencyPayload
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		const FString ConsumerV1Source = ASTEST_AS(R"AS(
			import HotReload.Dependency.HotReloadDependencyProvider;

			UCLASS()
			class UHotReloadDependencyConsumer : UObject
			{
				UFUNCTION()
				int ReadPayload(FHotReloadDependencyPayload Payload)
				{
					return Payload.Value;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ProviderModuleName, ProviderRelativeScriptPath, ProviderV1Source),
			TEXT("Initial dependency provider struct module should compile")));
		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ConsumerModuleName, ConsumerRelativeScriptPath, ConsumerV1Source),
			TEXT("Initial dependency consumer module should compile")));

		const TSharedPtr<FAngelscriptClassDesc> StructDescBeforeReload = Engine.GetClass(TEXT("FHotReloadDependencyPayload"));
		UClass* ConsumerClassBeforeReload = FindGeneratedClass(&Engine, TEXT("UHotReloadDependencyConsumer"));
		ASSERT_THAT(IsTrue(StructDescBeforeReload.IsValid(), TEXT("Provider struct should be registered before reload")));
		ASSERT_THAT(IsNotNull(StructDescBeforeReload->Struct, TEXT("Provider struct object should exist before reload")));
		ASSERT_THAT(IsNotNull(ConsumerClassBeforeReload, TEXT("Consumer class should exist before provider reload")));

		UFunction* ReadPayloadBeforeReload = FindGeneratedFunction(ConsumerClassBeforeReload, TEXT("ReadPayload"));
		ASSERT_THAT(IsNotNull(ReadPayloadBeforeReload, TEXT("Consumer ReadPayload should exist before provider reload")));
		FStructProperty* PayloadParameterBeforeReload = FindFProperty<FStructProperty>(ReadPayloadBeforeReload, TEXT("Payload"));
		ASSERT_THAT(IsNotNull(PayloadParameterBeforeReload, TEXT("Consumer payload parameter should exist before reload")));
		ASSERT_THAT(AreEqual(StructDescBeforeReload->Struct, PayloadParameterBeforeReload->Struct, TEXT("Consumer parameter should target provider struct before reload")));

		const FString ProviderV2Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadDependencyPayload
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int Bonus = 2;
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ProviderModuleName, ProviderRelativeScriptPath, ProviderV2Source, ReloadResult),
			TEXT("Provider struct full reload should compile")));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			TEXT("Provider struct full reload should be handled")));

		const TSharedPtr<FAngelscriptClassDesc> StructDescAfterReload = Engine.GetClass(TEXT("FHotReloadDependencyPayload"));
		ASSERT_THAT(IsTrue(StructDescAfterReload.IsValid(), TEXT("Provider struct should be registered after reload")));
		ASSERT_THAT(IsNotNull(StructDescAfterReload->Struct, TEXT("Provider struct object should exist after reload")));
		ASSERT_THAT(AreNotEqual(StructDescBeforeReload->Struct, StructDescAfterReload->Struct, TEXT("Provider full reload should replace the struct object")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StructDescAfterReload->Struct, TEXT("Bonus")), TEXT("Provider struct reload should expose the new Bonus field")));

		const FString ConsumerV2Source = ASTEST_AS(R"AS(
			import HotReload.Dependency.HotReloadDependencyProvider;

			UCLASS()
			class UHotReloadDependencyConsumer : UObject
			{
				UFUNCTION()
				int ReadPayload(FHotReloadDependencyPayload Payload)
				{
					return Payload.Value + Payload.Bonus;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ConsumerModuleName, ConsumerRelativeScriptPath, ConsumerV2Source, ReloadResult),
			TEXT("Consumer full reload against provider V2 should compile")));
		ASSERT_THAT(IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			TEXT("Consumer full reload should be handled")));

		UClass* ConsumerClassAfterReload = FindGeneratedClass(&Engine, TEXT("UHotReloadDependencyConsumer"));
		UFunction* ReadPayloadAfterReload = FindGeneratedFunction(ConsumerClassAfterReload, TEXT("ReadPayload"));
		FStructProperty* PayloadParameterAfterReload = ReadPayloadAfterReload != nullptr ? FindFProperty<FStructProperty>(ReadPayloadAfterReload, TEXT("Payload")) : nullptr;
		ASSERT_THAT(IsNotNull(ConsumerClassAfterReload, TEXT("Consumer class should exist after provider reload")));
		ASSERT_THAT(IsNotNull(ReadPayloadAfterReload, TEXT("Consumer ReadPayload should exist after provider reload")));
		ASSERT_THAT(IsNotNull(PayloadParameterAfterReload, TEXT("Consumer payload parameter should exist after provider reload")));
		ASSERT_THAT(AreEqual(StructDescAfterReload->Struct, PayloadParameterAfterReload->Struct, TEXT("Consumer parameter should retarget to the reloaded provider struct")));
		ASSERT_THAT(AreNotEqual(PayloadParameterBeforeReload->Struct, PayloadParameterAfterReload->Struct, TEXT("Consumer parameter should no longer target the old provider struct")));
	}
};

#endif
