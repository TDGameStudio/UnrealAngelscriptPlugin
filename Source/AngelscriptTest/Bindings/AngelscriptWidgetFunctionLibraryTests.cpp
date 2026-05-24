// ============================================================================
// AngelscriptWidgetFunctionLibraryTests.cpp
//
// Widget function library binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.FunctionLibraries.Widget.FAngelscriptWidgetFunctionLibraryTest.*
//
// Sections:
//   RenderTransformNullGuard — GetRenderTransform with valid and null widgets
//
// CQTest adaptation notes:
//   The valid-widget path passes a UButton to the script function via
//   FASGlobalFunctionInvoker::AddArgObject. The null-widget path calls a
//   no-arg wrapper that passes null from script, tested via
//   ExecuteFunctionExpectingScriptException.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Components/Button.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptWidgetFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: RenderTransformNullGuard
	// ====================================================================

	TEST_METHOD(RenderTransformNullGuard)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = TEXT(R"(
int ReadWidgetTransform(UWidget Widget)
{
	const FWidgetTransform Transform = Widget.GetRenderTransform();
	if (Transform.Translation.X != 13.5f || Transform.Translation.Y != -9.25f)
		return 10;
	if (Transform.Scale.X != 1.25f || Transform.Scale.Y != 0.75f)
		return 20;
	if (Transform.Angle != 42.0f)
		return 30;
	return 1;
}

void ReadWidgetTransformNull()
{
	UWidget Widget;
	ReadWidgetTransform(Widget);
}
)");

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASWidget_RenderTransform"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// ---- Create the concrete test widget ----
		UButton* TestWidget = NewObject<UButton>(GetTransientPackage(), TEXT("FunctionLibraryWidget"));
		if (!TestRunner->TestNotNull(TEXT("Widget function library test should create a concrete widget"), TestWidget))
		{
			return;
		}

		FWidgetTransform ExpectedTransform;
		ExpectedTransform.Translation = FVector2D(13.5f, -9.25f);
		ExpectedTransform.Scale = FVector2D(1.25f, 0.75f);
		ExpectedTransform.Angle = 42.0f;
		TestWidget->SetRenderTransform(ExpectedTransform);

		// ---- Valid widget: pass via FASGlobalFunctionInvoker ----
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, M, TEXT("int ReadWidgetTransform(UWidget)"));
			if (!Invoker.IsValid()) return;
			Invoker.AddArgObject(TestWidget);
			const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestEqual(
				TEXT("GetRenderTransform should preserve the native translation, scale, and angle for a valid widget"),
				Result,
				1);
		}

		// ---- Null widget: expect script exception ----
		TestRunner->AddExpectedError(TEXT("Null pointer access"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(*Mod.GetModuleName(), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("int ReadWidgetTransform(UWidget) | Line 4 | Col 2"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestRunner->AddExpectedError(TEXT("void ReadWidgetTransformNull() | Line 17 | Col 2"), EAutomationExpectedErrorFlags::Contains, 1, false);

		ExecuteFunctionExpectingScriptException(*TestRunner, Engine, M, 
			TEXT("void ReadWidgetTransformNull()"),
			TEXT("GetRenderTransform should report a null-pointer diagnostic for a null widget receiver"),
			TEXT("Null pointer access"));
	}
};

#endif
