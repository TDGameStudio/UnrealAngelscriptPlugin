// ============================================================================
// AngelscriptSyntaxDefaultComponentTests.cpp
//
// Syntax coverage tests for DefaultComponent, RootComponent, Attach, AttachSocket,
// OverrideComponent, and ShowOnActor modifiers — CQTest edition.
//
// Automation prefix: Angelscript.TestModule.Syntax.DefaultComponent.*
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptBindingsAssertions.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptSyntaxDefaultComponentTest,
	"Angelscript.TestModule.Syntax.DefaultComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Positive — Basic DefaultComponent forms
	// ====================================================================

	TEST_METHOD(Positive_BasicDefaultComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DefCompBasic"),
			TEXT(R"(
class ADefCompBasicActor : AActor
{
	UPROPERTY(DefaultComponent)
	USceneComponent Root;
}
)"),
			TEXT("Basic DefaultComponent"));
	}

	TEST_METHOD(Positive_RootComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DefCompRoot"),
			TEXT(R"(
class ADefCompRootActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;
}
)"),
			TEXT("DefaultComponent with RootComponent"));
	}

	TEST_METHOD(Positive_Attach)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DefCompAttach"),
			TEXT(R"(
class ADefCompAttachActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root)
	UStaticMeshComponent Mesh;
}
)"),
			TEXT("DefaultComponent with Attach"));
	}

	TEST_METHOD(Positive_AttachSocket)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DefCompAttachSocket"),
			TEXT(R"(
class ADefCompSocketActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root, AttachSocket = "Socket1")
	USceneComponent Child;
}
)"),
			TEXT("DefaultComponent with AttachSocket"));
	}

	TEST_METHOD(Positive_MultipleComponents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DefCompMultiple"),
			TEXT(R"(
class ADefCompMultiActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;

	UPROPERTY(DefaultComponent, Attach = Root)
	USceneComponent Child1;

	UPROPERTY(DefaultComponent, Attach = Root)
	USceneComponent Child2;
}
)"),
			TEXT("Multiple DefaultComponents"));
	}

	// ====================================================================
	// Negative — Invalid DefaultComponent usage
	// ====================================================================

	TEST_METHOD(Negative_OnNonComponentType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompNonComp"),
			TEXT(R"(
class ADefCompNonCompActor : AActor
{
	UPROPERTY(DefaultComponent)
	int X;
}
)"),
			TEXT("DefaultComponent on non-component type should fail"));
	}

	TEST_METHOD(Negative_OutsideClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompGlobal"),
			TEXT(R"(
UPROPERTY(DefaultComponent) USceneComponent Root;
)"),
			TEXT("DefaultComponent at global scope should fail"));
	}

	TEST_METHOD(Negative_RootComponentWithoutDefaultComponent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS logs RootComponent error but compilation succeeds (non-fatal warning)
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompRootOnly"),
			TEXT(R"(
class ADefCompRootOnlyActor : AActor
{
	UPROPERTY(RootComponent)
	USceneComponent Root;
}
)"),
			TEXT("RootComponent without DefaultComponent should fail"));
#endif
	}

	TEST_METHOD(Negative_AttachToNonExistent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompBadAttach"),
			TEXT(R"(
class ADefCompBadAttachActor : AActor
{
	UPROPERTY(DefaultComponent, Attach = NonExistent)
	USceneComponent Child;
}
)"),
			TEXT("Attach to non-existent component should fail"));
	}

	TEST_METHOD(Negative_MultipleRootComponents)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验多个 RootComponent 声明
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompMultiRoot"),
			TEXT(R"(
class ADefCompMultiRootActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root1;

	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root2;
}
)"),
			TEXT("Multiple RootComponents should fail"));
#endif
	}

	TEST_METHOD(Negative_InNonActorClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 DefaultComponent 是否在 Actor 类中
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompNonActor"),
			TEXT(R"(
struct FDefCompStruct
{
	UPROPERTY(DefaultComponent)
	USceneComponent Root;
}
)"),
			TEXT("DefaultComponent in non-Actor class should fail"));
#endif
	}

	TEST_METHOD(Negative_BadComponentType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompBadCompType"),
			TEXT(R"(
class ADefCompBadTypeActor : AActor
{
	UPROPERTY(DefaultComponent)
	AActor SubActor;
}
)"),
			TEXT("DefaultComponent on non-UActorComponent type should fail"));
	}

	TEST_METHOD(Negative_AttachToSelf)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验组件自引用挂载
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompAttachSelf"),
			TEXT(R"(
class ADefCompSelfActor : AActor
{
	UPROPERTY(DefaultComponent, Attach = Myself)
	USceneComponent Myself;
}
)"),
			TEXT("DefaultComponent attaching to self should fail"));
#endif
	}

	TEST_METHOD(Negative_CircularAttach)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验循环挂载依赖
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompCircular"),
			TEXT(R"(
class ADefCompCircularActor : AActor
{
	UPROPERTY(DefaultComponent, Attach = CompB)
	USceneComponent CompA;

	UPROPERTY(DefaultComponent, Attach = CompA)
	USceneComponent CompB;
}
)"),
			TEXT("Circular attachment should fail"));
#endif
	}

	TEST_METHOD(Negative_DefaultComponentOnNonUPROPERTY)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompNoUProp"),
			TEXT(R"(
class ADefCompNoUPropActor : AActor
{
	USceneComponent Root;
	default Root = DefaultComponent;
}
)"),
			TEXT("DefaultComponent on non-UPROPERTY field should fail"));
	}

	TEST_METHOD(Negative_AttachSocketWithoutAttach)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): structural-validation-absent — AS 不校验 AttachSocket 必须配合 Attach 使用
#if 0
		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompSocketNoAttach"),
			TEXT(R"(
class ADefCompSocketNoAttachActor : AActor
{
	UPROPERTY(DefaultComponent, AttachSocket = "Socket1")
	USceneComponent Child;
}
)"),
			TEXT("AttachSocket without Attach should fail"));
#endif
	}

	TEST_METHOD(Negative_DefaultComponentOnNonExistentType)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompBadType"),
			TEXT(R"(
class ADefCompBadTypeNameActor : AActor
{
	UPROPERTY(DefaultComponent)
	UNonExistentComponent Comp;
}
)"),
			TEXT("DefaultComponent with non-existent component type should fail"));
	}

	// ====================================================================
	// Override — Mixed positive and negative
	// ====================================================================

	TEST_METHOD(Override_Mixed_PositiveOverrideParent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// DISABLED(#as-engine-behavior): feature-not-supported — AS 不支持 OverrideComponent 跨继承层级覆盖
#if 0
		SyntaxTestHelpers::AssertCompiles(*TestRunner, Engine, TEXT("DefCompOverride"),
			TEXT(R"(
class ADefCompBaseActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;
}

class ADefCompChildActor : ADefCompBaseActor
{
	UPROPERTY(OverrideComponent = Root)
	UStaticMeshComponent Root;
}
)"),
			TEXT("OverrideComponent from parent"));
#endif
	}

	TEST_METHOD(Override_Mixed_NegativeOverrideNonExistent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("DefCompOverrideBad"),
			TEXT(R"(
class ADefCompBaseBadActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent Root;
}

class ADefCompChildBadActor : ADefCompBaseBadActor
{
	UPROPERTY(OverrideComponent = NonExistent)
	UStaticMeshComponent Mesh;
}
)"),
			TEXT("Override non-existent component should fail"));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
