#include "AngelscriptSubsystem.h"
#include "Cache/AngelscriptRuntimeReload.h"

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheRuntimeReloadApiTests,
	"Angelscript.TestModule.Cache.RuntimeReloadApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SubsystemExposesBlueprintRequestAndCompletionDelegate)
	{
		UClass* SubsystemClass = UAngelscriptSubsystem::StaticClass();
		ASSERT_THAT(IsNotNull(SubsystemClass));
		UFunction* RequestFunction = SubsystemClass->FindFunctionByName(
			TEXT("RequestRuntimeReload"));
		ASSERT_THAT(IsNotNull(RequestFunction));
		ASSERT_THAT(IsTrue(RequestFunction->HasAnyFunctionFlags(
			FUNC_BlueprintCallable)));
		FMulticastDelegateProperty* CompletionProperty =
			FindFProperty<FMulticastDelegateProperty>(
				SubsystemClass, TEXT("OnRuntimeReloadCompleted"));
		ASSERT_THAT(IsNotNull(CompletionProperty));
		ASSERT_THAT(IsTrue(CompletionProperty->HasAnyPropertyFlags(
			CPF_BlueprintAssignable)));
	}

	TEST_METHOD(ConsoleExposesRuntimeReloadCommand)
	{
		ASSERT_THAT(IsNotNull(IConsoleManager::Get().FindConsoleObject(
			TEXT("as.ReloadScripts"))));
	}
};

#endif
