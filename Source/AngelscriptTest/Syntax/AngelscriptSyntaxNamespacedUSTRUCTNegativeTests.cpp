// ============================================================================
// AngelscriptSyntaxNamespacedUSTRUCTNegativeTests.cpp
//
// Negative syntax coverage for namespaced USTRUCT declarations.
//
// Automation prefix: Angelscript.TestModule.Syntax.NamespacedUSTRUCTNegative.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxNamespacedUSTRUCTNegativeTest,
	"Angelscript.TestModule.Syntax.NamespacedUSTRUCTNegative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(DuplicateShortNameConflictsOnUnrealStructName)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// This locks the current boundary between AS namespaces and UE reflection.
		//
		// AS namespaces and UE reflected object names are different namespaces. Pure AS
		// script symbols can be qualified as First::FSharedSyntaxStruct and
		// Second::FSharedSyntaxStruct, but USTRUCT() declarations also create UE
		// reflected UScriptStruct objects. Those reflected objects live in the UObject
		// naming system, not in AS namespace scope.
		//
		// The current class generator maps a script struct to its UE backing name by
		// using the short script type name and stripping the leading F. It does not
		// include the AS namespace, module name, or any stable namespace-derived suffix
		// in the reflected name. Both declarations below therefore want the same UE
		// reflected name: "SharedSyntaxStruct".
		//
		// Rejecting this is deliberate for now. If the compiler accepted it, later
		// behavior could become dependent on load order, reload order, generated object
		// reuse, or UObject lookup behavior. That would make script recompiles and test
		// runs unstable: one namespace could accidentally shadow the other's reflected
		// struct even though the AS names look distinct.
		//
		// Supporting namespaced USTRUCTs later is therefore larger than allowing this
		// syntax. It needs an explicit reflected-name policy, reference resolution rules
		// for UPROPERTY/UFUNCTION signatures, hot-reload behavior, Blueprint-visible
		// names, serialization compatibility for existing generated names, and a clear
		// story for how users refer to the reflected type from both AS and UE systems.
		//
		// This is also separate from the foreach parsing issue discussed with declarations
		// like:
		//
		//     for (const SomeNamespace::FSomeStruct& Elem : Elems)
		//
		// The preprocessor/parser can currently treat the first ':' in '::' as the foreach
		// separator. That is a colon-token parsing limitation. This test intentionally
		// covers the different problem: two valid-looking namespaced USTRUCT declarations
		// colliding when projected into UE reflection.
		SyntaxTestHelpers::AssertFailsWithError(*TestRunner, Engine, TEXT("NamespacedUSTRUCTN_DuplicateUnrealName"),
			TEXT(R"(
namespace First
{
	USTRUCT()
	struct FSharedSyntaxStruct
	{
		UPROPERTY()
		int A;
	}
}

namespace Second
{
	USTRUCT()
	struct FSharedSyntaxStruct
	{
		UPROPERTY()
		int B;
	}
}
)"),
			TEXT("Name conflict: unreal name SharedSyntaxStruct"),
			TEXT("Namespaced USTRUCTs with the same reflected backing name"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
