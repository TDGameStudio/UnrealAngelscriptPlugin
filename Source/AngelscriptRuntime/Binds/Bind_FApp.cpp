#include "AngelscriptBinds.h"

#include "Misc/App.h"

#include "Bind_FApp_Functions.h"

namespace
{
	void BindFApp(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FApp");
		Binds.BindGlobalFunctionForTarget("bool CanEverRender()", &FApp::CanEverRender)
			.NativeFunction("FApp::CanEverRender", true);
		Binds.BindGlobalFunctionForTarget("FString GetProjectName()", &FAngelscriptFAppBinds::GetProjectName);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FApp(
	TEXT("FApp"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFApp);
