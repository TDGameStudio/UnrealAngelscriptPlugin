// ============================================================================
// AngelscriptGlobalBindingsTests.cpp
//
// Global utility function binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Global.FAngelscriptGlobalBindingsTest.*
//
// Sections:
//   GlobalVariables     — CollisionProfile, FComponentQueryParams globals
//   CommandletGlobals   — IsRunningCommandlet, IsRunningCookCommandlet, GetRunningCommandletClass
//
// CQTest adaptation notes:
//   CommandletGlobals requires runtime template substitution for expected values.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGlobalBindingsTest,
	"Angelscript.TestModule.Bindings.Global",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: GlobalVariables
	// ====================================================================

	TEST_METHOD(GlobalVariables)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGlobal_GlobalVar"), TEXT(R"(
int GlobalVar_CollisionProfileBlockAllDynamic()
{
	return (CollisionProfile::BlockAllDynamic.Compare(FName("BlockAllDynamic")) == 0) ? 1 : 0;
}

int GlobalVar_DefaultComponentQueryParams()
{
	FComponentQueryParams FreshParams;
	return (FComponentQueryParams::DefaultComponentQueryParams.ShapeCollisionMask.Bits == FreshParams.ShapeCollisionMask.Bits) ? 1 : 0;
}

)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GlobalVar_CollisionProfileBlockAllDynamic()"), TEXT("CollisionProfile::BlockAllDynamic should match FName"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GlobalVar_DefaultComponentQueryParams()"), TEXT("DefaultComponentQueryParams should match fresh default"), 1);
	}

	// ====================================================================
	// Section: CommandletGlobals
	// ====================================================================

	TEST_METHOD(CommandletGlobals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bExpectedRunningCommandlet = ::IsRunningCommandlet();
		const bool bExpectedRunningCookCommandlet = ::IsRunningCookCommandlet();
		const bool bExpectedRunningDLCCookCommandlet = ::IsRunningDLCCookCommandlet();
		UClass* ExpectedCommandletClass = ::GetRunningCommandletClass();

		FString Script = TEXT(R"(
int CommandletGlobals_IsRunningCommandlet()
{
	return (IsRunningCommandlet() == $IS_RUNNING_COMMANDLET$) ? 1 : 0;
}

int CommandletGlobals_IsRunningCookCommandlet()
{
	return (IsRunningCookCommandlet() == $IS_RUNNING_COOK_COMMANDLET$) ? 1 : 0;
}

int CommandletGlobals_IsRunningDLCCookCommandlet()
{
	return (IsRunningDLCCookCommandlet() == $IS_RUNNING_DLC_COOK_COMMANDLET$) ? 1 : 0;
}

int CommandletGlobals_GetRunningCommandletClass()
{
	UClass RunningCommandletClass = GetRunningCommandletClass();
	if ($EXPECTS_NULL_COMMANDLET_CLASS$)
	{
		return (RunningCommandletClass == null) ? 1 : 0;
	}
	else
	{
		if (!IsValid(RunningCommandletClass))
			return 0;
		return (RunningCommandletClass.GetName() == "$RUNNING_COMMANDLET_CLASS_NAME$") ? 1 : 0;
	}
}
)");
		Script.ReplaceInline(TEXT("$IS_RUNNING_COMMANDLET$"), bExpectedRunningCommandlet ? TEXT("true") : TEXT("false"));
		Script.ReplaceInline(TEXT("$IS_RUNNING_COOK_COMMANDLET$"), bExpectedRunningCookCommandlet ? TEXT("true") : TEXT("false"));
		Script.ReplaceInline(TEXT("$IS_RUNNING_DLC_COOK_COMMANDLET$"), bExpectedRunningDLCCookCommandlet ? TEXT("true") : TEXT("false"));
		Script.ReplaceInline(TEXT("$EXPECTS_NULL_COMMANDLET_CLASS$"), ExpectedCommandletClass == nullptr ? TEXT("true") : TEXT("false"));
		Script.ReplaceInline(
			TEXT("$RUNNING_COMMANDLET_CLASS_NAME$"),
			ExpectedCommandletClass != nullptr
				? *ExpectedCommandletClass->GetName().ReplaceCharWithEscapedChar()
				: TEXT(""));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGlobal_Commandlet"), Script);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_IsRunningCommandlet()"), TEXT("IsRunningCommandlet should match native value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_IsRunningCookCommandlet()"), TEXT("IsRunningCookCommandlet should match native value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_IsRunningDLCCookCommandlet()"), TEXT("IsRunningDLCCookCommandlet should match native value"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_GetRunningCommandletClass()"), TEXT("GetRunningCommandletClass should match native value"), 1);
	}
};

#endif
