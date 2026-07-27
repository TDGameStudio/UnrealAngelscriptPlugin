#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FDefaultSpecialMembers238Tests,
	"Angelscript.TestModule.AngelScriptSDK.Conformance.DefaultSpecialMembers238",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
	TEST_METHOD(GeneratedDefaultAndCopyMembersPreserveValues)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"V238-DESIRED-BEHAVIOR supersedes this explicit default-member predecessor with default_special_members crossed across five evidence layers");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class Sample
			{
				int Value = 42;
				Sample() = default;
				Sample(const Sample& in Other) = default;
			}

			int Entry()
			{
				Sample Original;
				Sample Copy(Original);
				return Copy.Value;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "DefaultSpecialMembers238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Default special-member target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Default special-member target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function), TEXT("Generated special members should execute")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Generated copy member should preserve values")));
	}

	TEST_METHOD(ImplicitCopyMembersAreGeneratedForScriptValues)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"V238-DESIRED-BEHAVIOR owns the selected 2.38 default-special-member feature; this discoverable Disabled fixture retains its earlier implicit-copy metadata detail");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct AutoCopySample
			{
				int Value = 42;
			}

			int Entry()
			{
				AutoCopySample Original;
				AutoCopySample Copy(Original);
				Copy = Original;
				return Copy.Value;
			}
			)AS");
		FScopedNativeModule Module(*TestRunner, Engine, "ImplicitCopyMembers238", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			return;
		}

		asITypeInfo* const Type = Module.Get()->GetTypeInfoByName("AutoCopySample");
		ASSERT_THAT(IsNotNull(Type, TEXT("Implicit-copy target should publish its value type")));
		if (Type == nullptr)
		{
			return;
		}

		bool bHasCopyConstructor = false;
		for (asUINT Index = 0; Index < Type->GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type->GetBehaviourByIndex(Index, &Behaviour);
			bHasCopyConstructor |= Function != nullptr
				&& Behaviour == asBEHAVE_CONSTRUCT
				&& Function->GetParamCount() == 1;
		}
		ASSERT_THAT(IsTrue(bHasCopyConstructor,
			TEXT("Implicit-copy target should publish a generated copy constructor")));

		bool bHasCopyAssignment = false;
		for (asUINT Index = 0; Index < Type->GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Method = Type->GetMethodByIndex(Index);
			bHasCopyAssignment |= Method != nullptr
				&& FCStringAnsi::Strcmp(Method->GetName(), "opAssign") == 0
				&& Method->GetParamCount() == 1;
		}
		ASSERT_THAT(IsTrue(bHasCopyAssignment,
			TEXT("Implicit-copy target should publish a generated copy assignment method")));

		asIScriptFunction* const Function = GetNativeFunctionByDecl(Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Function, TEXT("Implicit-copy target should expose Entry")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Implicit-copy target should create a context")));
		if (Function == nullptr || Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function),
			TEXT("Implicit generated special members should execute")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Implicit generated copy members should preserve values")));
	}

	TEST_METHOD(DeletedDefaultOrCopyMemberRejectsTheMatchingUse)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_NON_PRODUCT("LegacyCompatibility",
			"V238-DESIRED-BEHAVIOR owns selected 2.38 default and deleted special-member syntax across build diagnostics; this Disabled fixture remains a focused predecessor");

		FNativeMessageCollector Messages;
		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class NonDefaultConstructible
			{
				NonDefaultConstructible() = delete;
			}

			void Entry()
			{
				NonDefaultConstructible Value;
			}
			)AS");
		const int Result = CompileSnippet("DeletedSpecialMember238", ScriptSource.c_str(), Messages);
		ASSERT_THAT(IsTrue(Result < 0, TEXT("Deleted default constructor should reject a matching construction")));
		ASSERT_THAT(IsTrue(Messages.Entries.Num() > 0, TEXT("Deleted default constructor should report a diagnostic")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
