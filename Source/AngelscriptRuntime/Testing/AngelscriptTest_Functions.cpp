#include "Testing/AngelscriptTest_Functions.h"

#include "Testing/LatentAutomationCommand.h"

UAngelscriptTestSuite* FAngelscriptTestBinds::GetCurrentSuite(
	const ULatentAutomationCommand* Self)
{
	return Self != nullptr
		? Self->GetCurrentSuite()
		: nullptr;
}
