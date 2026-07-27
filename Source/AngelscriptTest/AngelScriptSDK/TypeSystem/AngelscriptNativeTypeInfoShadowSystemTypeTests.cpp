#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypeInfoShadowSystemTypeTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.TypeInfoShadowSystemType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void NoOpGeneric(asIScriptGeneric*)
	{
	}

	static FString BuildReviewSource(
		const TCHAR* Operation,
		const TCHAR* Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Operation: %s"), Operation));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Expected: %s"), Expected));
		return Source;
	}

	static bool RegisterTypes(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine)
	{
		FNoDiscardAsserter Assert(Test);
		bool bSuccess = true;

		for (const char* TypeName :
			{ "ShadowBase", "ShadowChild", "ShadowGrandchild", "ShadowReplacement" })
		{
			bSuccess &= Assert.IsTrue(
				ScriptEngine.RegisterObjectType(
					TypeName,
					0,
					asOBJ_REF | asOBJ_NOCOUNT) >= 0,
				*FString::Printf(
					TEXT("TypeInfo shadow product should register %s"),
					UTF8_TO_TCHAR(TypeName)));
		}
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectMethod(
				"ShadowBase",
				"void BaseOnly()",
				asFUNCTION(NoOpGeneric),
				asCALL_GENERIC) >= 0,
			TEXT("TypeInfo shadow product should register the base-only method"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectMethod(
				"ShadowReplacement",
				"void ReplacementOnly()",
				asFUNCTION(NoOpGeneric),
				asCALL_GENERIC) >= 0,
			TEXT("TypeInfo shadow product should register the replacement-only method"));
		return bSuccess;
	}

public:
	TEST_METHOD(CopyReplaceTraverseAndClearShadowType)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("TYPE-TYPEINFO-SHADOW-SYSTEM-TYPE",
			ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("TypeInfo shadow product should create a raw SDK engine")));
		if (ScriptEngine == nullptr
			|| !RegisterTypes(*TestRunner, *ScriptEngine))
		{
			return;
		}

		asITypeInfo* const Base =
			ScriptEngine->GetTypeInfoByName("ShadowBase");
		asITypeInfo* const Child =
			ScriptEngine->GetTypeInfoByName("ShadowChild");
		asITypeInfo* const Grandchild =
			ScriptEngine->GetTypeInfoByName("ShadowGrandchild");
		asITypeInfo* const Replacement =
			ScriptEngine->GetTypeInfoByName("ShadowReplacement");
		ASSERT_THAT(IsNotNull(
			Base,
			TEXT("TypeInfo shadow product should resolve ShadowBase")));
		ASSERT_THAT(IsNotNull(
			Child,
			TEXT("TypeInfo shadow product should resolve ShadowChild")));
		ASSERT_THAT(IsNotNull(
			Grandchild,
			TEXT("TypeInfo shadow product should resolve ShadowGrandchild")));
		ASSERT_THAT(IsNotNull(
			Replacement,
			TEXT("TypeInfo shadow product should resolve ShadowReplacement")));
		if (Base == nullptr
			|| Child == nullptr
			|| Grandchild == nullptr
			|| Replacement == nullptr)
		{
			return;
		}

		asIScriptFunction* const BaseMethod =
			Base->GetMethodByName("BaseOnly");
		asIScriptFunction* const ReplacementMethod =
			Replacement->GetMethodByName("ReplacementOnly");
		ASSERT_THAT(IsNotNull(
			BaseMethod,
			TEXT("TypeInfo shadow product should resolve the exact base method")));
		ASSERT_THAT(IsNotNull(
			ReplacementMethod,
			TEXT("TypeInfo shadow product should resolve the exact replacement method")));
		if (BaseMethod == nullptr || ReplacementMethod == nullptr)
		{
			return;
		}

		struct FReviewCase
		{
			const TCHAR* Id;
			const TCHAR* Operation;
			const TCHAR* Expected;
		};
		const FReviewCase ReviewCases[] =
		{
			{ TEXT("initial"), TEXT("query clean child"), TEXT("no shadow relation or inherited method") },
			{ TEXT("copy_base"), TEXT("child.CopySystemType(base)"), TEXT("direct relation and base method") },
			{ TEXT("copy_child"), TEXT("grandchild.CopySystemType(child)"), TEXT("direct child and transitive base relation") },
			{ TEXT("transitive_method"), TEXT("grandchild.GetMethodByName(BaseOnly)"), TEXT("exact base method identity") },
			{ TEXT("replace"), TEXT("child.CopySystemType(replacement)"), TEXT("old relation removed and replacement relation visible") },
			{ TEXT("replace_transitive"), TEXT("query grandchild after child replacement"), TEXT("transitive chain follows replacement") },
			{ TEXT("clear_grandchild"), TEXT("grandchild.CopySystemType(null)"), TEXT("grandchild relation and methods cleared") },
			{ TEXT("clear_child"), TEXT("child.CopySystemType(null)"), TEXT("child relation and methods cleared") },
		};
		for (const FReviewCase& ReviewCase : ReviewCases)
		{
			PrintGeneratedAsSource(
				*TestRunner,
				MakeNativeCaseId(
					"TYPE-TYPEINFO-SHADOW-SYSTEM-TYPE",
					{ ReviewCase.Id }),
				TEXT("NativeTypeInfoShadowSystemType"),
				BuildReviewSource(
					ReviewCase.Operation,
					ReviewCase.Expected));
		}

		ASSERT_THAT(IsFalse(
			Child->ShadowsFrom(Base),
			TEXT("Clean child should not initially shadow the base")));
		ASSERT_THAT(IsNull(
			Child->GetMethodByName("BaseOnly"),
			TEXT("Clean child should not initially inherit the base method")));

		Child->CopySystemType(Base);
		ASSERT_THAT(IsTrue(
			Child->ShadowsFrom(Base),
			TEXT("Child should directly shadow the copied base type")));
		ASSERT_THAT(AreEqual(
			BaseMethod,
			Child->GetMethodByName("BaseOnly"),
			TEXT("Child should resolve the exact base method through its shadow type")));
		ASSERT_THAT(IsFalse(
			Base->ShadowsFrom(Child),
			TEXT("Base should not gain a reverse shadow relation")));
		ASSERT_THAT(IsFalse(
			Child->ShadowsFrom(Replacement),
			TEXT("Child should not shadow an unrelated replacement type")));

		Grandchild->CopySystemType(Child);
		ASSERT_THAT(IsTrue(
			Grandchild->ShadowsFrom(Child),
			TEXT("Grandchild should directly shadow the child")));
		ASSERT_THAT(IsTrue(
			Grandchild->ShadowsFrom(Base),
			TEXT("Grandchild should transitively shadow the base")));
		ASSERT_THAT(AreEqual(
			BaseMethod,
			Grandchild->GetMethodByName("BaseOnly"),
			TEXT("Grandchild should resolve the exact base method transitively")));

		Child->CopySystemType(Replacement);
		ASSERT_THAT(IsFalse(
			Child->ShadowsFrom(Base),
			TEXT("Replacing the child shadow should remove its old base relation")));
		ASSERT_THAT(IsNull(
			Child->GetMethodByName("BaseOnly"),
			TEXT("Replacing the child shadow should remove the old base method")));
		ASSERT_THAT(IsTrue(
			Child->ShadowsFrom(Replacement),
			TEXT("Child should shadow the replacement type")));
		ASSERT_THAT(AreEqual(
			ReplacementMethod,
			Child->GetMethodByName("ReplacementOnly"),
			TEXT("Child should resolve the exact replacement method")));
		ASSERT_THAT(IsFalse(
			Grandchild->ShadowsFrom(Base),
			TEXT("Grandchild should stop shadowing the old base after child replacement")));
		ASSERT_THAT(IsTrue(
			Grandchild->ShadowsFrom(Replacement),
			TEXT("Grandchild should transitively follow the child replacement")));
		ASSERT_THAT(AreEqual(
			ReplacementMethod,
			Grandchild->GetMethodByName("ReplacementOnly"),
			TEXT("Grandchild should resolve the exact replacement method transitively")));

		Grandchild->CopySystemType(nullptr);
		ASSERT_THAT(IsFalse(
			Grandchild->ShadowsFrom(Child),
			TEXT("Clearing grandchild should remove the direct child relation")));
		ASSERT_THAT(IsFalse(
			Grandchild->ShadowsFrom(Replacement),
			TEXT("Clearing grandchild should remove the transitive replacement relation")));
		ASSERT_THAT(IsNull(
			Grandchild->GetMethodByName("ReplacementOnly"),
			TEXT("Clearing grandchild should remove inherited replacement methods")));

		Child->CopySystemType(nullptr);
		ASSERT_THAT(IsFalse(
			Child->ShadowsFrom(Replacement),
			TEXT("Clearing child should remove its replacement relation")));
		ASSERT_THAT(IsNull(
			Child->GetMethodByName("ReplacementOnly"),
			TEXT("Clearing child should remove inherited replacement methods")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
