#include "Bind_FApp.h"

#include "Misc/App.h"

FString FAngelscriptFAppBinds::GetProjectName()
{
	return FApp::GetProjectName();
}
