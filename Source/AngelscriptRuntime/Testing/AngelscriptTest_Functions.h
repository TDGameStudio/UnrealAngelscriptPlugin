#pragma once

#include "CoreMinimal.h"

class UAngelscriptTestSuite;
class ULatentAutomationCommand;

struct FAngelscriptTestBinds
{
	static UAngelscriptTestSuite* GetCurrentSuite(
		const ULatentAutomationCommand* Self);
};
