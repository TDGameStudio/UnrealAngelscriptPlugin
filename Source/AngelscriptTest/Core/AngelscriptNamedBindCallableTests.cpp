#include "CQTest.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "AngelscriptTestEngine.h"
#include "Misc/Guid.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	struct FNamedBindCallableFixture
	{
		int32 Value = 0;

		int32 GetValue() const
		{
			return Value;
		}

		int32 Add(int32 Offset) const
		{
			return Value + Offset;
		}

		float Add(float Offset) const
		{
			return static_cast<float>(Value) + Offset;
		}
	};

	static int32 CDECL ReadExternal(const FNamedBindCallableFixture& Fixture)
	{
		return Fixture.Value;
	}

	static int32 CDECL GlobalPlain()
	{
		return 11;
	}

	static int32 CDECL GlobalTrivial()
	{
		return 12;
	}

	static int32 CDECL GlobalOverload(int32 Value)
	{
		return Value;
	}

	static float CDECL GlobalOverload(float Value)
	{
		return Value;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptNamedBindCallableTests,
	"Angelscript.TestModule.Engine.BindingArchitecture.Fluent.CallableForms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitFacadeAcceptsSupportedCallableForms)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Callable-form engine should be created"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptBinds Binds(*Engine);
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		const FString TypeName = FString::Printf(
			TEXT("FNamedBindCallableFixture_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		FAngelscriptBinds Type = Binds.ValueClassForTarget<FNamedBindCallableFixture>(TypeName, TypeFlags);

		FAngelscriptBoundFunction MemberPointer = Type.Method("int32 GetValue() const", &FNamedBindCallableFixture::GetValue);
		FAngelscriptBoundFunction ExplicitOverload = Type.Method(
			"float32 AddFloat(float32 Offset) const",
			static_cast<float (FNamedBindCallableFixture::*)(float) const>(&FNamedBindCallableFixture::Add));
		FAngelscriptBoundFunction MethodMacro = Type.Method("int32 AddInt(int32 Offset) const", METHODPR(int32, FNamedBindCallableFixture, Add, (int32) const));
		FAngelscriptBoundFunction TrivialMethodMacro = Type.Method("int32 GetValueTrivial() const", METHOD_TRIVIAL(FNamedBindCallableFixture, GetValue));
		FAngelscriptBoundFunction ExternalPointer = Type.Method("int32 ReadExternal() const", &ReadExternal);
		FAngelscriptBoundFunction MethodLambda = Type.Method(
			"int32 ReadLambda() const",
			[](const FNamedBindCallableFixture& Fixture)
			{
				return Fixture.Value;
			});
		FAngelscriptBoundFunction ConstructorLambda = Type.Constructor(
			"void f(int32 Value)",
			[](FNamedBindCallableFixture* Address, int32 Value)
			{
				new (Address) FNamedBindCallableFixture();
				Address->Value = Value;
			});

		FAngelscriptBoundFunction FunctionMacro = Binds.BindGlobalFunctionForTarget("int32 NamedCallableGlobalPlain()", FUNC(GlobalPlain));
		FAngelscriptBoundFunction TrivialFunctionMacro = Binds.BindGlobalFunctionForTarget("int32 NamedCallableGlobalTrivial()", FUNC_TRIVIAL(GlobalTrivial));
		FAngelscriptBoundFunction OverloadMacro = Binds.BindGlobalFunctionForTarget(
			"int32 NamedCallableGlobalOverload(int32 Value)",
			FUNCPR(int32, GlobalOverload, (int32)));
		FAngelscriptBoundFunction TrivialOverloadMacro = Binds.BindGlobalFunctionForTarget(
			"float32 NamedCallableGlobalOverloadFloat(float32 Value)",
			FUNCPR_TRIVIAL(float, GlobalOverload, (float)));
		FAngelscriptBoundFunction GlobalLambda = Binds.BindGlobalFunctionForTarget(
			"int32 NamedCallableGlobalLambda()",
			[]()
			{
				return 13;
			});

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("Member/free pointers and explicit overloads should register"), MemberPointer.IsValid() && ExplicitOverload.IsValid() && ExternalPointer.IsValid());
		bPassed &= TestRunner->TestTrue(TEXT("METHOD and METHODPR forms should register"), MethodMacro.IsValid() && TrivialMethodMacro.IsValid());
		bPassed &= TestRunner->TestTrue(TEXT("FUNC and FUNCPR forms should register through the explicit facade"), FunctionMacro.IsValid() && TrivialFunctionMacro.IsValid() && OverloadMacro.IsValid() && TrivialOverloadMacro.IsValid());
		bPassed &= TestRunner->TestTrue(TEXT("Supported non-capturing method/global/constructor lambdas should register"), MethodLambda.IsValid() && GlobalLambda.IsValid() && ConstructorLambda.IsValid());
		bPassed &= TestRunner->TestFalse(TEXT("Supported callable forms should not fail the explicit context"), Binds.HasRegistrationFailure());
		TestRunner->TestTrue(TEXT("The explicit facade should retain the supported callable authoring surface"), bPassed);
	}
};

#endif
