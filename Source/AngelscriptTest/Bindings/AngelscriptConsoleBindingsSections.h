#pragma once

#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#if WITH_ANGELSCRIPT_UNITTESTS


bool VerifyConsoleVariableTypes(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleVariableExisting(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleVariableIdentity(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandBasic(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandArgumentEmpty(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandArgumentContent(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandReplacement(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandLifecycle(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandMissingHandler(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleCommandWrongSignature(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool VerifyConsoleLeakSelfCheck(FAutomationTestBase& Test, FAngelscriptEngine& Engine);


#endif
