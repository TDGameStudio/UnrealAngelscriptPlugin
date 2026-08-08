#include "CQTest.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptBindsInternal.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptEngineExtensionRegistry.h"

#if WITH_ANGELSCRIPT_UNITTESTS

ANGELSCRIPTRUNTIME_API bool GAngelscriptRunThreadedInitializationResultTransportForTesting(
	bool bWorkerResult);

namespace
{
	using UE::Angelscript::Private::FAngelscriptBindCollection;

	static void NoOpBind(FAngelscriptBinds& Binds)
	{
		(void)Binds;
	}

	static FAngelscriptBindRecord MakeLifecycleRecord(const FName OwnerModule, const FName BindName)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = OwnerModule;
		Record.BindName = BindName;
		Record.Phase = EAngelscriptBindPhase::GeneratedBindings;
		Record.SourceFile = "GeneratedLifecycleTest.cpp";
		Record.SourceLine = 19;
		Record.Callback = &NoOpBind;
		return Record;
	}

	struct FFailedInitializationExtension : IAngelscriptExtension
	{
		int32 AttachCount = 0;
		int32 DetachCount = 0;

		void OnEngineAttached(FAngelscriptEngine&) override
		{
			++AttachCount;
		}

		void OnEngineDetached(FAngelscriptEngine&) override
		{
			++DetachCount;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptManualBindingLifecycleTests,
	"Angelscript.TestModule.Subsystem.BindLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedModulesLoadBeforeCollectionFinalization)
	{
		FAngelscriptBindCollection Collection;
		TArray<FName> Events;
		FString Diagnostic;
		const TArray<FString> ModuleNames = {TEXT("GeneratedOne"), TEXT("GeneratedTwo")};

		const bool bPrepared = Collection.PrepareForEngineInitialization(
			ModuleNames,
			[&Collection, &Events](const FName ModuleName, FString& OutDiagnostic)
			{
				Events.Add(ModuleName);
				if (ModuleName == TEXT("GeneratedOne"))
				{
					FString AppendDiagnostic;
					if (!Collection.Append(MakeLifecycleRecord(ModuleName, TEXT("RuntimeLinked")), AppendDiagnostic))
					{
						OutDiagnostic = MoveTemp(AppendDiagnostic);
						return false;
					}
				}
				return true;
			},
			Diagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("All generated modules should load and finalize"), bPrepared);
		bPassed &= TestRunner->TestTrue(TEXT("Preparation should seal the collection after module loading"), Collection.IsSealed());
		bPassed &= TestRunner->TestTrue(TEXT("Generated modules should load in cache order"), Events == TArray<FName>({TEXT("GeneratedOne"), TEXT("GeneratedTwo")}));
		bPassed &= TestRunner->TestEqual(TEXT("A generated module should be able to append before the seal"), Collection.GetRecords().Num(), 1);
		bPassed &= TestRunner->TestTrue(TEXT("Successful preparation should clear diagnostics"), Diagnostic.IsEmpty());
		TestRunner->TestTrue(TEXT("Generated module loading should precede direct-bind finalization"), bPassed);
	}

	TEST_METHOD(MissingGeneratedModuleFailsWithoutSealing)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		bool bEngineInitializationStarted = false;
		const TArray<FString> ModuleNames = {TEXT("Present"), TEXT("Missing")};
		const bool bPrepared = Collection.PrepareForEngineInitialization(
			ModuleNames,
			[](const FName ModuleName, FString& OutDiagnostic)
			{
				if (ModuleName == TEXT("Missing"))
				{
					OutDiagnostic = TEXT("fixture module is unavailable");
					return false;
				}
				return true;
			},
			Diagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("A missing generated module must fail preparation"), bPrepared);
		bPassed &= TestRunner->TestFalse(TEXT("A failed module load must leave the collection unsealed"), Collection.IsSealed());
		bPassed &= TestRunner->TestTrue(TEXT("The failure should identify the module and loader reason"), Diagnostic.Contains(TEXT("Missing")) && Diagnostic.Contains(TEXT("fixture module is unavailable")));
		if (bPrepared)
		{
			bEngineInitializationStarted = true;
		}
		bPassed &= TestRunner->TestFalse(
			TEXT("Primary engine initialization must not start after generated-module preparation fails"),
			bEngineInitializationStarted);
		TestRunner->TestTrue(TEXT("Missing generated modules should fail closed"), bPassed);
	}

	TEST_METHOD(InvalidDuplicateAndLateRecordsBlockPrimaryEngineStartup)
	{
		auto PreparationFailureBlocksStartup = [this](
			FAngelscriptBindCollection& Collection,
			const FString& ExpectedDiagnostic,
			const TCHAR* FailureDescription)
		{
			FString Diagnostic;
			bool bEngineInitializationStarted = false;
			const bool bPrepared = Collection.PrepareForEngineInitialization(
				TConstArrayView<FString>(),
				[](FName ModuleName, FString& OutDiagnostic)
				{
					(void)ModuleName;
					(void)OutDiagnostic;
					return true;
				},
				Diagnostic);
			if (bPrepared)
			{
				bEngineInitializationStarted = true;
			}

			bool bPassed = true;
			bPassed &= TestRunner->TestFalse(FailureDescription, bPrepared);
			bPassed &= TestRunner->TestFalse(
				TEXT("A rejected binding collection must remain unpublished"),
				bEngineInitializationStarted);
			bPassed &= TestRunner->TestTrue(
				TEXT("Preparation should retain the binding failure diagnostic"),
				Diagnostic.Contains(ExpectedDiagnostic));
			return bPassed;
		};

		bool bPassed = true;
		FString AppendDiagnostic;

		FAngelscriptBindCollection InvalidCollection;
		FAngelscriptBindRecord InvalidRecord = MakeLifecycleRecord(TEXT("InvalidOwner"), TEXT("InvalidRecord"));
		InvalidRecord.Callback = nullptr;
		InvalidCollection.Append(MoveTemp(InvalidRecord), AppendDiagnostic);
		bPassed &= PreparationFailureBlocksStartup(
			InvalidCollection,
			TEXT("callback is required"),
			TEXT("Invalid binding metadata must fail preparation"));

		FAngelscriptBindCollection DuplicateCollection;
		DuplicateCollection.Append(
			MakeLifecycleRecord(TEXT("DuplicateOwner"), TEXT("DuplicateRecord")),
			AppendDiagnostic);
		FAngelscriptBindRecord DuplicateRecord = MakeLifecycleRecord(
			TEXT("DuplicateOwner"),
			TEXT("DuplicateRecord"));
		DuplicateRecord.SourceLine = 29;
		DuplicateCollection.Append(MoveTemp(DuplicateRecord), AppendDiagnostic);
		bPassed &= PreparationFailureBlocksStartup(
			DuplicateCollection,
			TEXT("Duplicate AngelScript bind identity"),
			TEXT("Duplicate binding identities must fail preparation"));

		FAngelscriptBindCollection LateCollection;
		LateCollection.Append(MakeLifecycleRecord(TEXT("InitialOwner"), TEXT("InitialRecord")), AppendDiagnostic);
		FString Diagnostic;
		bPassed &= TestRunner->TestTrue(
			TEXT("The late-native-module fixture should initially seal"),
			LateCollection.Finalize(Diagnostic));
		FAngelscriptBindRecord LateRecord = MakeLifecycleRecord(TEXT("LateNativeModule"), TEXT("LateRecord"));
		LateRecord.SourceFile = "LateNativeModule.cpp";
		LateRecord.SourceLine = 41;
		bPassed &= TestRunner->TestFalse(
			TEXT("A native module arriving after collection sealing must be rejected"),
			LateCollection.Append(MoveTemp(LateRecord), Diagnostic));
		bPassed &= PreparationFailureBlocksStartup(
			LateCollection,
			TEXT("restart the process"),
			TEXT("A late native module must block every later engine preparation attempt"));

		TestRunner->TestTrue(
			TEXT("Invalid, duplicate, and late binding records should fail before primary engine startup"),
			bPassed);
	}

	TEST_METHOD(RepeatedPreparationDoesNotReloadModules)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		int32 LoadCount = 0;
		const TArray<FString> ModuleNames = {TEXT("Generated")};
		auto Loader = [&LoadCount](const FName ModuleName, FString& OutDiagnostic)
		{
			(void)ModuleName;
			(void)OutDiagnostic;
			++LoadCount;
			return true;
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("The first preparation should succeed"), Collection.PrepareForEngineInitialization(ModuleNames, Loader, Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("The compatibility preparation should be idempotent"), Collection.PrepareForEngineInitialization(ModuleNames, Loader, Diagnostic));
		bPassed &= TestRunner->TestEqual(TEXT("Generated modules should load only before the first seal"), LoadCount, 1);
		TestRunner->TestTrue(TEXT("Subsystem and compatibility bootstrap should share one preparation operation"), bPassed);
	}

	TEST_METHOD(DirectBindFailurePreventsPostInitializationAndFactoryPublication)
	{
		FScopedAngelscriptEngineResolutionSuppressionForTesting NoAmbientEngineScope;
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bSkipThreadedInitialize = true;
		Config.bInjectDirectBindFailureForTesting = true;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TSharedRef<FFailedInitializationExtension> Extension = MakeShared<FFailedInitializationExtension>();
		const FDelegateHandle ExtensionHandle =
			FAngelscriptEngineExtensionRegistry::Get().RegisterExtension(Extension);

		TestRunner->AddExpectedErrorPlain(
			TEXT("Failed in call to function 'RegisterGlobalFunction' with 'void __InjectedDirectBindPublicationFailure()'"),
			EAutomationExpectedErrorFlags::Contains,
			2);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Direct AngelScript binding execution failed:"),
			EAutomationExpectedErrorFlags::Contains,
			2);

		FAngelscriptEngine DirectEngine(Config, Dependencies);
		int32 PostInitializeBroadcastCount = 0;
		DirectEngine.GetOnInitialCompileFinished().AddLambda([&PostInitializeBroadcastCount]()
		{
			++PostInitializeBroadcastCount;
		});

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("Injected direct-bind failure should fail full initialization"), DirectEngine.Initialize());
		bPassed &= TestRunner->TestFalse(TEXT("A failed engine should not reach its publication-ready state"), DirectEngine.IsReadyForPublication());
		bPassed &= TestRunner->TestEqual(TEXT("PostInitialize_GameThread should not run after a binding failure"), PostInitializeBroadcastCount, 0);
		DirectEngine.Shutdown();
		bPassed &= TestRunner->TestEqual(
			TEXT("A failed engine should not attach registered extensions"),
			Extension->AttachCount,
			0);
		bPassed &= TestRunner->TestEqual(
			TEXT("Shutdown should not detach extensions that never attached"),
			Extension->DetachCount,
			0);

		Config.bSkipInitialCompile = true;
		TUniquePtr<FAngelscriptEngine> FactoryEngine = FAngelscriptEngine::Create(Config, Dependencies);
		bPassed &= TestRunner->TestTrue(TEXT("The engine factory should not publish a partially bound engine"), !FactoryEngine.IsValid());
		bPassed &= TestRunner->TestEqual(
			TEXT("Factory cleanup should preserve extension attach/detach pairing"),
			Extension->DetachCount,
			0);
		FAngelscriptEngineExtensionRegistry::Get().UnregisterExtension(ExtensionHandle);
		TestRunner->TestTrue(TEXT("Direct binding failures should stop initialization before publication"), bPassed);
	}

	TEST_METHOD(ThreadedInitializationPublishesSuccessAndFailureAtomically)
	{
		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(
			TEXT("The Game Thread should observe a successful worker result"),
			GAngelscriptRunThreadedInitializationResultTransportForTesting(true));
		bPassed &= TestRunner->TestFalse(
			TEXT("The Game Thread should observe a failed worker result"),
			GAngelscriptRunThreadedInitializationResultTransportForTesting(false));

		TestRunner->TestTrue(
			TEXT("Threaded initialization should atomically publish both success and failure results"),
			bPassed);
	}
};

#endif
