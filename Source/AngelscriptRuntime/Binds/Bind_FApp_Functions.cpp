#include "Bind_FApp_Functions.h"

#include "Misc/App.h"

FString FAngelscriptFAppBinds::GetProjectName()
{
	return FApp::GetProjectName();
}
