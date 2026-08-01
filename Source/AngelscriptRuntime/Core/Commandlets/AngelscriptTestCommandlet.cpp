#include "AngelscriptTestCommandlet.h"
#include "AngelscriptEngine.h"
#include "Testing/AngelscriptScriptTestHotReloadRunner.h"

int32 UAngelscriptTestCommandlet::Main(const FString& Params)
{
	if (!FAngelscriptEngine::Get().bDidInitialCompileSucceed)
	{
		return 1;
	}

	if (!RunAngelscriptScriptTests(FAngelscriptEngine::Get()))
	{
		return 2;
	}

#if WITH_EDITOR
	if (FStructUtils::AttemptToFindUninitializedScriptStructMembers() != 0)
	{
		return 3;
	}
#endif

	return 0;
}
