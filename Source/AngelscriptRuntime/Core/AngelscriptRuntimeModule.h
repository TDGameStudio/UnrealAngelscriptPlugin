#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct FAngelscriptEngine;

class ANGELSCRIPTRUNTIME_API FAngelscriptRuntimeModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

	static void InitializeAngelscript();

private:
	friend struct FAngelscriptRuntimeModuleTickTestAccess;
	#if WITH_DEV_AUTOMATION_TESTS
	static void SetInitializeOverrideForTesting(TFunction<FAngelscriptEngine*()> InOverride);
	static void ResetInitializeStateForTesting();
	static TFunction<FAngelscriptEngine*()> InitializeOverrideForTesting;
	static FAngelscriptEngine* InitializedOverrideEngineForTesting;
	#endif
	static bool bInitializeAngelscriptCalled;
	static TUniquePtr<FAngelscriptEngine> OwnedPrimaryEngine;

};
