#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "Bindings/AngelscriptDataTableBindingTestTypes.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptUStructBindingsTests,
	"Angelscript.TestModule.Bindings.UStruct",
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

	TEST_METHOD(NativeGenericStructSpecialMembersExecute)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int NativeGenericStructSpecialMembers()
			{
				FAngelscriptBindingDataTableRow Source;
				Source.Category = n"Source";
				Source.Count = 42;
				Source.Label = "Original";

				FAngelscriptBindingDataTableRow Copy(Source);

				FAngelscriptBindingDataTableRow Assigned;
				Assigned = Source;

				Source.Category = n"Mutated";
				Source.Count = -1;
				Source.Label = "Changed";

				bool bCopyPreserved =
					Copy.Category == n"Source"
					&& Copy.Count == 42
					&& Copy.Label == "Original";
				bool bAssignmentPreserved =
					Assigned.Category == n"Source"
					&& Assigned.Count == 42
					&& Assigned.Label == "Original";

				return bCopyPreserved && bAssignmentPreserved ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASUStruct_NativeGenericSpecialMembers"),
			ScriptSource);
		ASSERT_THAT(IsTrue(
			ModuleScope.IsValid(),
			TEXT("Native generic USTRUCT special-member fixture should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(
				*TestRunner,
				Engine,
				ModuleScope.GetModule(),
				TEXT("int NativeGenericStructSpecialMembers()"),
				TEXT("Native generic USTRUCT default, copy, assignment, and destruction should execute"),
				1),
			TEXT("Native generic USTRUCT copies should preserve independent reflected values")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
