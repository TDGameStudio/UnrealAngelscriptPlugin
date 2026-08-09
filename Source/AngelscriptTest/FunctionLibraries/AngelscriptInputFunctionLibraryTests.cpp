#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "GameFramework/PlayerInput.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptInputFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ParameterlessMappingGettersDispatchWithNativeParity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASInput_ParameterlessMappings"), ASTEST_AS(R"AS(
			int GetActionMappingCount(UPlayerInput PlayerInput)
			{
				return PlayerInput.GetEngineDefinedActionMappings().Num();
			}

			int GetAxisMappingCount(UPlayerInput PlayerInput)
			{
				return PlayerInput.GetEngineDefinedAxisMappings().Num();
			}
			)AS"));
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("Parameterless UPlayerInput mapping getters should compile")));

		UPlayerInput* PlayerInput = NewObject<UPlayerInput>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(PlayerInput));

		FAngelscriptTestExecutor ActionExecutor(
			*TestRunner, Engine, Mod.GetModule(), TEXT("int GetActionMappingCount(UPlayerInput)"));
		ActionExecutor.AddArgObject(PlayerInput);
		const int32 ScriptActionCount = ActionExecutor.ExecuteAndGet<int32>(INDEX_NONE);
		ASSERT_THAT(IsTrue(ActionExecutor.HasRun()));
		ASSERT_THAT(AreEqual(UPlayerInput::GetEngineDefinedActionMappings().Num(), ScriptActionCount));

		FAngelscriptTestExecutor AxisExecutor(
			*TestRunner, Engine, Mod.GetModule(), TEXT("int GetAxisMappingCount(UPlayerInput)"));
		AxisExecutor.AddArgObject(PlayerInput);
		const int32 ScriptAxisCount = AxisExecutor.ExecuteAndGet<int32>(INDEX_NONE);
		ASSERT_THAT(IsTrue(AxisExecutor.HasRun()));
		ASSERT_THAT(AreEqual(UPlayerInput::GetEngineDefinedAxisMappings().Num(), ScriptAxisCount));
	}

	TEST_METHOD(LegacyMappingGetterArgumentsDoNotCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FString Source = ASTEST_AS(R"AS(
			void UseLegacyMappingArguments(UPlayerInput PlayerInput)
			{
				PlayerInput.GetEngineDefinedActionMappings(n"LegacyAction");
				PlayerInput.GetEngineDefinedAxisMappings(n"LegacyAxis");
			}
			)AS");
		const FString ExpectedFragments[] = {
			TEXT("GetEngineDefinedActionMappings"),
			TEXT("GetEngineDefinedAxisMappings"),
		};

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASInput_LegacyMappingArguments"),
			*Source,
			TEXT("Legacy UPlayerInput mapping getter arguments should be rejected"),
			MakeArrayView(ExpectedFragments))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
