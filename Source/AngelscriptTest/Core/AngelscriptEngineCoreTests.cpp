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


TEST_CLASS_WITH_FLAGS(FAngelscriptEngineCoreTests,
	"Angelscript.TestModule.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FCoreTestContextStackGuard
{
	TArray<FAngelscriptEngine*> SavedStack;
	FCoreTestContextStackGuard() { SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear(); }
	~FCoreTestContextStackGuard() { FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack)); }
	void DiscardSavedStack() { SavedStack.Reset(); }
};

public:
	TEST_METHOD(CreateDestroy)
	{
DestroySharedTestEngine();
		FAngelscriptEngineConfig Config;
		FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeEngineForTesting(Config, Dependencies);
		if (!this->Assert.IsNotNull(Engine.Get(), TEXT("Test module should create an angelscript engine instance")))
		{
			return;
		}

		Engine.Reset();
		(void)this->Assert.IsTrue(!Engine.IsValid(), TEXT("Resetting the test-owned engine should clear the pointer"));
	}

	TEST_METHOD(ScanFreeInitializeAcquiresProcessPackages)
	{
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
		if (!this->Assert.IsNotNull(Engine.Get(), TEXT("scan-free initialize should create the script engine wrapper")))
		{
			return;
		}

		bool bOk = this->Assert.IsNotNull(Engine->GetScriptEngine(), TEXT("scan-free initialize should create the script engine"));
		UPackage* Package = Engine->GetPackageInstance();
		if (!this->Assert.IsNotNull(Package, TEXT("scan-free initialize should acquire the process script package during game-thread pre-initialize")))
		{
			return;
		}

		bOk &= this->Assert.IsTrue(Package->IsRooted(), TEXT("scan-free initialize should root the process script package"));
		bOk &= this->Assert.IsTrue(FindPackage(nullptr, TEXT("/Script/Angelscript")) == Package, TEXT("scan-free initialize should leave the process script package discoverable"));
		(void)bOk;
	}

	TEST_METHOD(CompileSnippet)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		if (!this->Assert.IsNotNull(&Engine, TEXT("Compile test should create an initialized engine")))
		{
			return;
		}
		FAngelscriptEngineScope GlobalScope(Engine);

		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("CompileSnippet", asGM_ALWAYS_CREATE);
		if (!this->Assert.IsNotNull(Module, TEXT("Compile test should create a script module")))
		{
			return;
		}

		const char* Source = "int CompileOnly() { return 7; }";
		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction("CompileSnippet", Source, 0, 0, &Function);
		bool bOk = true;
		bOk &= this->Assert.AreEqual(static_cast<int>(asSUCCESS), CompileResult, TEXT("Compile test should compile the snippet successfully"));
		bOk &= this->Assert.IsNotNull(Function, TEXT("Compile test should receive a compiled function"));
		if (Function != nullptr)
		{
			Function->Release();
		}
		(void)bOk;
		}
	}

	TEST_METHOD(ExecuteSnippet)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		if (!this->Assert.IsNotNull(&Engine, TEXT("Execute test should create an initialized engine")))
		{
			return;
		}
		FAngelscriptEngineScope GlobalScope(Engine);

		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule("ExecuteSnippet", asGM_ALWAYS_CREATE);
		if (!this->Assert.IsNotNull(Module, TEXT("Execute test should create a script module")))
		{
			return;
		}

		const char* Source = "int ReturnFortyTwo() { return 42; }";
		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction("ExecuteSnippet", Source, 0, 0, &Function);
		if (!this->Assert.AreEqual(static_cast<int>(asSUCCESS), CompileResult, TEXT("Execute test should compile the snippet successfully")))
		{
			return;
		}

		if (!this->Assert.IsNotNull(Function, TEXT("Execute test should find the compiled function")))
		{
			return;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!this->Assert.IsNotNull(Context, TEXT("Execute test should create a script context")))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		bool bOk = true;
		bOk &= this->Assert.AreEqual(static_cast<int>(asSUCCESS), PrepareResult, TEXT("Execute test should prepare the function successfully"));
		bOk &= this->Assert.AreEqual(static_cast<int>(asEXECUTION_FINISHED), ExecuteResult, TEXT("Execute test should finish successfully"));
		bOk &= this->Assert.AreEqual(42, static_cast<int>(Context->GetReturnDWord()), TEXT("Execute test should receive the script return value"));
		Context->Release();
		Function->Release();
		(void)bOk;
		}
	}

	TEST_METHOD(LastFullDestroyClearsTypeState)
	{
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
		if (!this->Assert.IsNotNull(FullEngine.Get(), TEXT("Last full destroy core test should create a full engine")))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*FullEngine);
			if (!this->Assert.IsTrue(FAngelscriptType::GetTypes().Num() > 0, TEXT("Last full destroy core test should populate type metadata while the full engine is alive")))
			{
				return;
			}
		}

		FullEngine.Reset();
		(void)this->Assert.AreEqual(0, FAngelscriptType::GetTypes().Num(), TEXT("Last full destroy core test should clear type metadata after the final full owner is destroyed"));
	}

	TEST_METHOD(FullDestroyAllowsCleanRecreate)
	{
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
		if (!this->Assert.IsNotNull(FirstEngine.Get(), TEXT("Full destroy recreate core test should create the first full engine")))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*FirstEngine);
			if (!this->Assert.IsTrue(FAngelscriptType::GetTypes().Num() > 0, TEXT("Full destroy recreate core test should populate type metadata during the first epoch")))
			{
				return;
			}
		}

		FirstEngine.Reset();
		if (!this->Assert.AreEqual(0, FAngelscriptType::GetTypes().Num(), TEXT("Full destroy recreate core test should clear type metadata after the first epoch ends")))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(SecondEngine.Get(), TEXT("Full destroy recreate core test should create a second full engine after cleanup")))
		{
			return;
		}

		{
			FAngelscriptEngineScope Scope(*SecondEngine);
			if (!this->Assert.IsTrue(FAngelscriptType::GetTypes().Num() > 0, TEXT("Full destroy recreate core test should repopulate type metadata during the recreated epoch")))
			{
				return;
			}
		}

		const bool bCompiled = CompileModuleFromMemory(
			SecondEngine.Get(),
			TEXT("RecreateCoreSnippet"),
			TEXT("RecreateCoreSnippet.as"),
			TEXT("int Entry() { return 17; }"));
		if (!this->Assert.IsTrue(bCompiled, TEXT("Full destroy recreate core test should compile a trivial module after recreation")))
		{
			return;
		}

		int32 Result = 0;
		if (!this->Assert.IsTrue(ExecuteIntFunction(SecondEngine.Get(), TEXT("RecreateCoreSnippet"), TEXT("int Entry()"), Result), TEXT("Full destroy recreate core test should execute the recreated module entry point")))
		{
			return;
		}

		(void)this->Assert.AreEqual(17, Result, TEXT("Full destroy recreate core test should preserve the expected return value after recreation"));
	}

	TEST_METHOD(FullDestroyAllowsAnnotatedRecreate)
	{
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
			if (!this->Assert.IsTrue(
				CompileAnnotatedModuleFromMemory(Engine, ModuleName, Filename, ScriptSource),
				FString::Printf(TEXT("%s should compile after full-engine setup"), *ModuleName.ToString())))
			{
				return false;
			}

			UClass* GeneratedClass = FindGeneratedClass(Engine, ExpectedClassName);
			return this->Assert.IsNotNull(
				GeneratedClass,
				*FString::Printf(TEXT("%s should resolve the generated class after compile"), ExpectedClassName));
		};

		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(FirstEngine.Get(), TEXT("Annotated recreate test should create the first full engine")))
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

		if (!this->Assert.AreEqual(0, FAngelscriptType::GetTypes().Num(), TEXT("Annotated recreate test should clear type metadata after the first full engine exits")))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(SecondEngine.Get(), TEXT("Annotated recreate test should create the second full engine")))
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
			if (!this->Assert.IsTrue(
				CompileAnnotatedModuleFromMemory(Engine, ModuleName, Filename, ScriptSource),
				FString::Printf(TEXT("%s should compile annotated source"), *ModuleName.ToString())))
			{
				return false;
			}

			UClass* GeneratedClass = FindGeneratedClass(Engine, GeneratedClassName);
			if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Annotated same-name recreate test should resolve the generated class")))
			{
				return false;
			}

			FIntProperty* ValueProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("Value"));
			if (!this->Assert.IsNotNull(ValueProperty, TEXT("Annotated same-name recreate test should expose the generated Value property")))
			{
				return false;
			}

			UObject* DefaultObject = GeneratedClass->GetDefaultObject();
			if (!this->Assert.IsNotNull(DefaultObject, TEXT("Annotated same-name recreate test should expose a generated CDO")))
			{
				return false;
			}

			if (!this->Assert.AreEqual(ExpectedValue, ValueProperty->GetPropertyValue_InContainer(DefaultObject), TEXT("Annotated same-name recreate test should read the expected CDO Value")))
			{
				return false;
			}
			OutGeneratedClass = GeneratedClass;
			return true;
		};

		TUniquePtr<FAngelscriptEngine> FirstEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(FirstEngine.Get(), TEXT("Annotated same-name recreate test should create the first full engine")))
		{
			return;
		}

		UClass* FirstGeneratedClass = nullptr;
		if (!CompileAnnotatedActor(FirstEngine.Get(), FirstEpochSource, 11, FirstGeneratedClass))
		{
			return;
		}

		UPackage* FirstGeneratedPackage = FirstGeneratedClass->GetPackage();
		if (!this->Assert.IsNotNull(FirstGeneratedPackage, TEXT("Annotated same-name recreate test should resolve the first generated package")))
		{
			return;
		}

		const FString FirstClassPath = FirstGeneratedClass->GetPathName();
		const FString FirstPackagePath = FirstGeneratedPackage->GetPathName();
		const uint32 FirstClassUniqueId = FirstGeneratedClass->GetUniqueID();

		{
			FAngelscriptEngineScope Scope(*FirstEngine);
			if (!this->Assert.IsTrue(FirstEngine->DiscardModule(*ModuleNameString), TEXT("Annotated same-name recreate test should discard the first epoch module")))
			{
				return;
			}
		}

		CollectGarbage(RF_NoFlags, true);
		FirstEngine.Reset();
		CollectGarbage(RF_NoFlags, true);

		if (!this->Assert.AreEqual(0, FAngelscriptType::GetTypes().Num(), TEXT("Annotated same-name recreate test should clear type metadata after the first full engine exits")))
		{
			return;
		}

		TUniquePtr<FAngelscriptEngine> SecondEngine = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(SecondEngine.Get(), TEXT("Annotated same-name recreate test should create the second full engine")))
		{
			return;
		}

		UClass* SecondGeneratedClass = nullptr;
		if (!CompileAnnotatedActor(SecondEngine.Get(), SecondEpochSource, 22, SecondGeneratedClass))
		{
			return;
		}

		UPackage* SecondGeneratedPackage = SecondGeneratedClass->GetPackage();
		if (!this->Assert.IsNotNull(SecondGeneratedPackage, TEXT("Annotated same-name recreate test should resolve the recreated generated package")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.AreEqual(FirstClassPath, SecondGeneratedClass->GetPathName(), TEXT("Annotated same-name recreate test should recreate the class at the same object path"));
		bOk &= this->Assert.AreEqual(FirstPackagePath, SecondGeneratedPackage->GetPathName(), TEXT("Annotated same-name recreate test should recreate the package at the same object path"));
		bOk &= this->Assert.AreNotEqual(FirstClassUniqueId, SecondGeneratedClass->GetUniqueID(), TEXT("Annotated same-name recreate test should create a new UObject identity for the recreated class"));
		bOk &= this->Assert.AreEqual(SecondGeneratedClass, FindObject<UClass>(nullptr, *FirstClassPath), TEXT("Annotated same-name recreate test should let global lookup resolve the recreated class"));
		(void)bOk;
	}
};

#endif
