#pragma once

#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#if WITH_ANGELSCRIPT_UNITTESTS


bool RunConsoleVariableTypesSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleVariableExistingSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleVariableIdentitySection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandBasicSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandArgumentEmptySection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandArgumentContentSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandReplacementSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandLifecycleSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandMissingHandlerSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleCommandWrongSignatureSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);
bool RunConsoleLeakSelfCheckSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine);


#endif
