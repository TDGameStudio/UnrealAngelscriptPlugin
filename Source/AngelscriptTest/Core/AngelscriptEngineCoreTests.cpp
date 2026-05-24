#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private
{
	struct FCoreTestContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;
		FCoreTestContextStackGuard() { SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear(); }
		~FCoreTestContextStackGuard() { FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack)); }
		void DiscardSavedStack() { SavedStack.Reset(); }
	};
}


TEST_CLASS_WITH_FLAGS(FAngelscriptEngineCoreTests,
	"Angelscript.TestModule.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CreateDestroy)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		DestroySharedTestEngine();
		FAngelscriptEngineConfig Config;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
		if (!TestRunner->TestNotNull(TEXT("Test module should create an angelscript engine instance"), Engine.Get()))
		{
			return;
		}

		Engine.Reset();
		TestRunner->TestTrue(TEXT("Resetting the test-owned engine should clear the pointer"), !Engine.IsValid());
	}

	TEST_METHOD(ScanFreeInitializeAcquiresProcessPackages)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
		if (!TestRunner->TestNotNull(TEXT("scan-free initialize should create the script engine wrapper"), Engine.Get()))
		{
			return;
		}

		TestRunner->TestNotNull(TEXT("scan-free initialize should create the script engine"), Engine->GetScriptEngine());
		UPackage* Package = Engine->GetPackageInstance();
		if (!TestRunner->TestNotNull(TEXT("scan-free initialize should acquire the process script package during game-thread pre-initialize"), Package))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("scan-free initialize should root the process script package"), Package->IsRooted());
		TestRunner->TestTrue(TEXT("scan-free initialize should leave the process script package discoverable"), FindPackage(nullptr, TEXT("/Script/Angelscript")) == Package);
	}

	TEST_METHOD(CompileSnippet)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		if (!TestRunner->TestNotNull(TEXT("Compile test should create an initialized engine"), &Engine))
		{
			return;
		}
		FAngelscriptEngineScope GlobalScope(Engine);

		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("CompileSnippet", asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("Compile test should create a script module"), Module))
		{
			return;
		}

		const char* Source = "int CompileOnly() { return 7; }";
		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction("CompileSnippet", Source, 0, 0, &Function);
		TestRunner->TestEqual(TEXT("Compile test should compile the snippet successfully"), CompileResult, asSUCCESS);
		TestRunner->TestNotNull(TEXT("Compile test should receive a compiled function"), Function);
		if (Function != nullptr)
		{
			Function->Release();
		}
		}
	}

	TEST_METHOD(ExecuteSnippet)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		if (!TestRunner->TestNotNull(TEXT("Execute test should create an initialized engine"), &Engine))
		{
			return;
		}
		FAngelscriptEngineScope GlobalScope(Engine);

		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("ExecuteSnippet", asGM_ALWAYS_CREATE);
		if (!TestRunner->TestNotNull(TEXT("Execute test should create a script module"), Module))
		{
			return;
		}

		const char* Source = "int ReturnFortyTwo() { return 42; }";
		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction("ExecuteSnippet", Source, 0, 0, &Function);
		if (!TestRunner->TestEqual(TEXT("Execute test should compile the snippet successfully"), CompileResult, asSUCCESS))
		{
			return;
		}

		if (!TestRunner->TestNotNull(TEXT("Execute test should find the compiled function"), Function))
		{
			return;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!TestRunner->TestNotNull(TEXT("Execute test should create a script context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		TestRunner->TestEqual(TEXT("Execute test should prepare the function successfully"), PrepareResult, asSUCCESS);
		TestRunner->TestEqual(TEXT("Execute test should finish successfully"), ExecuteResult, asEXECUTION_FINISHED);
		TestRunner->TestEqual(TEXT("Execute test should receive the script return value"), static_cast<int>(Context->GetReturnDWord()), 42);
		Context->Release();
		Function->Release();
		}
	}

	TEST_METHOD(LastFullDestroyClearsTypeState)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> FullEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Last full destroy core test should create a full engine"), FullEngine.Get()))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*FullEngine);
			if (!TestRunner->TestTrue(TEXT("Last full destroy core test should populate type metadata while the full engine is alive"), FAngelscriptType::GetTypes().Num() > 0))
			{
				return;
			}
		}

		FullEngine.Reset();
		TestRunner->TestEqual(TEXT("Last full destroy core test should clear type metadata after the final full owner is destroyed"), FAngelscriptType::GetTypes().Num(), 0);
	}

	TEST_METHOD(FullDestroyAllowsCleanRecreate)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Full destroy recreate core test should create the first full engine"), FirstEngine.Get()))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*FirstEngine);
			if (!TestRunner->TestTrue(TEXT("Full destroy recreate core test should populate type metadata during the first epoch"), FAngelscriptType::GetTypes().Num() > 0))
			{
				return;
			}
		}

		FirstEngine.Reset();
		if (!TestRunner->TestEqual(TEXT("Full destroy recreate core test should clear type metadata after the first epoch ends"), FAngelscriptType::GetTypes().Num(), 0))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Full destroy recreate core test should create a second full engine after cleanup"), SecondEngine.Get()))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*SecondEngine);
			if (!TestRunner->TestTrue(TEXT("Full destroy recreate core test should repopulate type metadata during the recreated epoch"), FAngelscriptType::GetTypes().Num() > 0))
			{
				return;
			}
		}

		const bool bCompiled = CompileModuleFromMemory(
			SecondEngine.Get(),
			TEXT("RecreateCoreSnippet"),
			TEXT("RecreateCoreSnippet.as"),
			TEXT("int Entry() { return 17; }"));
		if (!TestRunner->TestTrue(TEXT("Full destroy recreate core test should compile a trivial module after recreation"), bCompiled))
		{
			return;
		}

		int32 Result = 0;
		if (!TestRunner->TestTrue(TEXT("Full destroy recreate core test should execute the recreated module entry point"), ExecuteIntFunction(SecondEngine.Get(), TEXT("RecreateCoreSnippet"), TEXT("int Entry()"), Result)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Full destroy recreate core test should preserve the expected return value after recreation"), Result, 17);
	}

	TEST_METHOD(FullDestroyAllowsAnnotatedRecreate)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		auto CompileAnnotatedActor = [this](FAngelscriptEngine* Engine, FName ModuleName, const TCHAR* Filename, const TCHAR* ScriptSource, const TCHAR* ExpectedClassName)
		{
			FAngelscriptEngineScope Scope(*Engine);
			if (!TestRunner->TestTrue(
				FString::Printf(TEXT("%s should compile after full-engine setup"), *ModuleName.ToString()),
				CompileAnnotatedModuleFromMemory(Engine, ModuleName, Filename, ScriptSource)))
			{
				return false;
			}

			UClass* GeneratedClass = FindGeneratedClass(Engine, ExpectedClassName);
			return TestRunner->TestNotNull(
				*FString::Printf(TEXT("%s should resolve the generated class after compile"), ExpectedClassName),
				GeneratedClass);
		};

		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Annotated recreate test should create the first full engine"), FirstEngine.Get()))
		{
			return;
		}

		if (!CompileAnnotatedActor(
			FirstEngine.Get(),
			TEXT("RecreateAnnotatedActorA"),
			TEXT("RecreateAnnotatedActorA.as"),
			TEXT(R"(
UCLASS()
class ARecreateAnnotatedActorA : AActor
{
	UPROPERTY()
	int Value = 11;
}
)"),
			TEXT("ARecreateAnnotatedActorA")))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*FirstEngine);
			FirstEngine->DiscardModule(TEXT("RecreateAnnotatedActorA"));
		}
		CollectGarbage(RF_NoFlags, true);
		FirstEngine.Reset();

		if (!TestRunner->TestEqual(TEXT("Annotated recreate test should clear type metadata after the first full engine exits"), FAngelscriptType::GetTypes().Num(), 0))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Annotated recreate test should create the second full engine"), SecondEngine.Get()))
		{
			return;
		}

		CompileAnnotatedActor(
			SecondEngine.Get(),
			TEXT("RecreateAnnotatedActorB"),
			TEXT("RecreateAnnotatedActorB.as"),
			TEXT(R"(
UCLASS()
class ARecreateAnnotatedActorB : AActor
{
	UPROPERTY()
	int Value = 22;
}
)"),
			TEXT("ARecreateAnnotatedActorB"));
	}

	TEST_METHOD(FullDestroyAllowsAnnotatedSameNameRecreate)
	{
		using namespace AngelscriptTest_Core_AngelscriptEngineCoreTests_Private;
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		const FName ModuleName(TEXT("RecreateAnnotatedActor"));
		const FString ModuleNameString = ModuleName.ToString();
		const TCHAR* Filename = TEXT("RecreateAnnotatedActor.as");
		const FName GeneratedClassName(TEXT("ARecreateAnnotatedActor"));
		const FString FirstEpochSource = TEXT(R"(
UCLASS()
class ARecreateAnnotatedActor : AActor
{
	UPROPERTY()
	int Value = 11;
}
)");
		const FString SecondEpochSource = TEXT(R"(
UCLASS()
class ARecreateAnnotatedActor : AActor
{
	UPROPERTY()
	int Value = 22;
}
)");

		auto CompileAnnotatedActor = [this, ModuleName, Filename, GeneratedClassName](FAngelscriptEngine* Engine, const FString& ScriptSource, int32 ExpectedValue, UClass*& OutGeneratedClass)
		{
			FAngelscriptEngineScope Scope(*Engine);
			if (!TestRunner->TestTrue(
				FString::Printf(TEXT("%s should compile annotated source"), *ModuleName.ToString()),
				CompileAnnotatedModuleFromMemory(Engine, ModuleName, Filename, ScriptSource)))
			{
				return false;
			}

			UClass* GeneratedClass = FindGeneratedClass(Engine, GeneratedClassName);
			if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should resolve the generated class"), GeneratedClass))
			{
				return false;
			}

			FIntProperty* ValueProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("Value"));
			if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should expose the generated Value property"), ValueProperty))
			{
				return false;
			}

			UObject* DefaultObject = GeneratedClass->GetDefaultObject();
			if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should expose a generated CDO"), DefaultObject))
			{
				return false;
			}

			TestRunner->TestEqual(TEXT("Annotated same-name recreate test should read the expected CDO Value"), ValueProperty->GetPropertyValue_InContainer(DefaultObject), ExpectedValue);
			OutGeneratedClass = GeneratedClass;
			return true;
		};

		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should create the first full engine"), FirstEngine.Get()))
		{
			return;
		}

		UClass* FirstGeneratedClass = nullptr;
		if (!CompileAnnotatedActor(FirstEngine.Get(), FirstEpochSource, 11, FirstGeneratedClass))
		{
			return;
		}

		UPackage* FirstGeneratedPackage = FirstGeneratedClass->GetPackage();
		if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should resolve the first generated package"), FirstGeneratedPackage))
		{
			return;
		}

		const FString FirstClassPath = FirstGeneratedClass->GetPathName();
		const FString FirstPackagePath = FirstGeneratedPackage->GetPathName();
		const uint32 FirstClassUniqueId = FirstGeneratedClass->GetUniqueID();

		{
			FAngelscriptEngineScope Scope(*FirstEngine);
			if (!TestRunner->TestTrue(TEXT("Annotated same-name recreate test should discard the first epoch module"), FirstEngine->DiscardModule(*ModuleNameString)))
			{
				return;
			}
		}

		CollectGarbage(RF_NoFlags, true);
		FirstEngine.Reset();
		CollectGarbage(RF_NoFlags, true);

		if (!TestRunner->TestEqual(TEXT("Annotated same-name recreate test should clear type metadata after the first full engine exits"), FAngelscriptType::GetTypes().Num(), 0))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should create the second full engine"), SecondEngine.Get()))
		{
			return;
		}

		UClass* SecondGeneratedClass = nullptr;
		if (!CompileAnnotatedActor(SecondEngine.Get(), SecondEpochSource, 22, SecondGeneratedClass))
		{
			return;
		}

		UPackage* SecondGeneratedPackage = SecondGeneratedClass->GetPackage();
		if (!TestRunner->TestNotNull(TEXT("Annotated same-name recreate test should resolve the recreated generated package"), SecondGeneratedPackage))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Annotated same-name recreate test should recreate the class at the same object path"), SecondGeneratedClass->GetPathName(), FirstClassPath);
		TestRunner->TestEqual(TEXT("Annotated same-name recreate test should recreate the package at the same object path"), SecondGeneratedPackage->GetPathName(), FirstPackagePath);
		TestRunner->TestNotEqual(TEXT("Annotated same-name recreate test should create a new UObject identity for the recreated class"), SecondGeneratedClass->GetUniqueID(), FirstClassUniqueId);
		TestRunner->TestEqual(TEXT("Annotated same-name recreate test should let global lookup resolve the recreated class"), FindObject<UClass>(nullptr, *FirstClassPath), SecondGeneratedClass);
	}
};

#endif
