#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_builder.h"
#include "as_module.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheBuilderInvocationTests_Private
{
	struct FObservedInvocation final
	{
		asEBuildArtifactInvocationKind Kind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		asEBuildArtifactIneligibleReason IneligibleReason =
			asBUILD_ARTIFACT_INELIGIBLE_INVALID_INVOCATION_KIND;
		FString ModuleName;
		FString Namespace;
		FString OwnerName;
		FString FunctionName;
		FString Declaration;
		FString CanonicalSource;
		FString SourceSection;
		uint32 Traits = 0;
		bool bGenerated = false;
		bool bHasNode = false;
	};

	struct FInvocationLog final
	{
		TArray<FObservedInvocation> Values;
	};

	static void ObserveInvocation(
		const asSBuildArtifactInvocation* Invocation,
		void* UserData)
	{
		if (Invocation == nullptr || UserData == nullptr)
		{
			return;
		}
		FInvocationLog& Log = *static_cast<FInvocationLog*>(UserData);
		FObservedInvocation& Value = Log.Values.AddDefaulted_GetRef();
		Value.Kind = Invocation->kind;
		Value.IneligibleReason = Invocation->ineligibleReason;
		Value.ModuleName = UTF8_TO_TCHAR(Invocation->moduleName.AddressOf());
		Value.Namespace = UTF8_TO_TCHAR(Invocation->nameSpace.AddressOf());
		Value.OwnerName = UTF8_TO_TCHAR(Invocation->ownerName.AddressOf());
		Value.FunctionName = UTF8_TO_TCHAR(Invocation->functionName.AddressOf());
		Value.Declaration = UTF8_TO_TCHAR(Invocation->declaration.AddressOf());
		Value.CanonicalSource =
			UTF8_TO_TCHAR(Invocation->canonicalSource.AddressOf());
		Value.SourceSection =
			UTF8_TO_TCHAR(Invocation->sourceSection.AddressOf());
		Value.Traits = Invocation->traits;
		Value.bGenerated = Invocation->isGenerated;
		Value.bHasNode = Invocation->hasNode;
	}

	static const FObservedInvocation* FindKind(
		const FInvocationLog& Log,
		const asEBuildArtifactInvocationKind Kind)
	{
		return Log.Values.FindByPredicate(
			[Kind](const FObservedInvocation& Value)
			{
				return Value.Kind == Kind;
			});
	}

	static int32 CountKind(
		const FInvocationLog& Log,
		const asEBuildArtifactInvocationKind Kind)
	{
		int32 Count = 0;
		for (const FObservedInvocation& Value : Log.Values)
		{
			if (Value.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	static void LogInvocations(
		FAutomationTestBase& Test,
		const FInvocationLog& Log)
	{
		for (int32 Index = 0; Index < Log.Values.Num(); ++Index)
		{
			const FObservedInvocation& Value = Log.Values[Index];
			Test.AddInfo(FString::Printf(
				TEXT("Builder artifact invocation[%d]: Kind=%u Ineligible=%u Module=%s Namespace=%s Owner=%s Function=%s Generated=%d HasNode=%d Section=%s Declaration=%s Source=%s"),
				Index,
				static_cast<uint32>(Value.Kind),
				static_cast<uint32>(Value.IneligibleReason),
				*Value.ModuleName,
				*Value.Namespace,
				*Value.OwnerName,
				*Value.FunctionName,
				Value.bGenerated ? 1 : 0,
				Value.bHasNode ? 1 : 0,
				*Value.SourceSection,
				*Value.Declaration,
				*Value.CanonicalSource));
		}
	}

	static asSBuildArtifactInvocation MakeSyntheticInvocation(
		const asEBuildArtifactInvocationKind Kind)
	{
		asSBuildArtifactInvocation Invocation;
		Invocation.kind = Kind;
		Invocation.moduleName = "ASCacheV2BuilderInvocation";
		Invocation.ownerName = "ASCacheV2BuilderInvocation";
		Invocation.functionName = "Synthetic";
		Invocation.declaration = "int Synthetic()";
		Invocation.canonicalSource = "intSynthetic(){return1;}";
		Invocation.sourceSection = "Synthetic.as";
		Invocation.hasNode = true;
		return Invocation;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheBuilderInvocationTests,
	"Angelscript.TestModule.Cache.BuilderInvocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ModuleBuildEmitsEverySupportedStableInvocationFamily)
	{
		using namespace AngelscriptCacheBuilderInvocationTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptEngine* ScriptEngine = Fixture.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		asCModule* Module = static_cast<asCModule*>(ScriptEngine->GetModule(
			"ASCacheV2BuilderInvocation", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FInvocationLog Log;
		Module->SetBuildArtifactInvocationCallback(&ObserveInvocation, &Log);
		const char* Source = R"AS(
class FInvocationLeaf
{
}

class FGeneratedInvocation
{
	FInvocationLeaf Child;
	int Value = 3;
	default Value = 5;

	int Read()
	{
		return Value;
	}
}

class FExplicitInvocation
{
	int Value;

	FExplicitInvocation(int InValue)
	{
		Value = InValue;
	}

	~FExplicitInvocation()
	{
		Value = 0;
	}
}

int GlobalInvocation()
{
	return 7;
}
)AS";
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"BuilderInvocation.as", Source, FCStringAnsi::Strlen(Source), 0)));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));
		LogInvocations(*TestRunner, Log);

		const asEBuildArtifactInvocationKind RequiredKinds[] = {
			asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION,
			asBUILD_ARTIFACT_INVOCATION_METHOD,
			asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR,
			asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR,
			asBUILD_ARTIFACT_INVOCATION_FACTORY,
			asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR,
			asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR,
			asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS,
		};
		for (const asEBuildArtifactInvocationKind Kind : RequiredKinds)
		{
			const FObservedInvocation* Value = FindKind(Log, Kind);
			ASSERT_THAT(IsNotNull(Value));
			ASSERT_THAT(AreEqual(
				asBUILD_ARTIFACT_INELIGIBLE_NONE, Value->IneligibleReason));
			ASSERT_THAT(IsFalse(Value->ModuleName.IsEmpty()));
			ASSERT_THAT(IsFalse(Value->OwnerName.IsEmpty()));
			ASSERT_THAT(IsFalse(Value->FunctionName.IsEmpty()));
			ASSERT_THAT(IsFalse(Value->Declaration.IsEmpty()));
			ASSERT_THAT(IsFalse(Value->CanonicalSource.IsEmpty()));
			ASSERT_THAT(IsFalse(Value->SourceSection.IsEmpty()));
		}
		ASSERT_THAT(IsTrue(CountKind(
			Log, asBUILD_ARTIFACT_INVOCATION_FACTORY) >= 2));
	}

	TEST_METHOD(PublicSingleFunctionEmitsExplicitNotCacheableCoordinates)
	{
		using namespace AngelscriptCacheBuilderInvocationTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptEngine* ScriptEngine = Fixture.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		asCModule* Module = static_cast<asCModule*>(ScriptEngine->GetModule(
			"ASCacheV2PublicSingleInvocation", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FInvocationLog Log;
		Module->SetBuildArtifactInvocationCallback(&ObserveInvocation, &Log);
		asIScriptFunction* Function = nullptr;
		ASSERT_THAT(AreEqual(asSUCCESS, Module->CompileFunction(
			"DebugSnippet.as",
			"int DebugSnippet() { return 11; }",
			0,
			0,
			&Function)));
		ASSERT_THAT(IsNotNull(Function));
		Function->Release();
		LogInvocations(*TestRunner, Log);

		ASSERT_THAT(AreEqual(1, CountKind(
			Log, asBUILD_ARTIFACT_INVOCATION_PUBLIC_SINGLE_FUNCTION)));
		const FObservedInvocation* PublicSingle = FindKind(
			Log, asBUILD_ARTIFACT_INVOCATION_PUBLIC_SINGLE_FUNCTION);
		ASSERT_THAT(IsNotNull(PublicSingle));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_INELIGIBLE_PUBLIC_SINGLE_UNSTABLE_COORDINATE,
			PublicSingle->IneligibleReason));
		ASSERT_THAT(IsFalse(PublicSingle->ModuleName.IsEmpty()));
		ASSERT_THAT(IsFalse(PublicSingle->Declaration.IsEmpty()));
		ASSERT_THAT(IsFalse(PublicSingle->CanonicalSource.IsEmpty()));
	}

	TEST_METHOD(UnstableLambdaAndInvalidKindsAreTypedNotCacheable)
	{
		using namespace AngelscriptCacheBuilderInvocationTests_Private;
		asSBuildArtifactInvocation Lambda = MakeSyntheticInvocation(
			asBUILD_ARTIFACT_INVOCATION_LAMBDA);
		asCBuilder::FinalizeBuildArtifactInvocation(Lambda);
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_INELIGIBLE_LAMBDA_UNSTABLE_COORDINATE,
			Lambda.ineligibleReason));

		asSBuildArtifactInvocation Invalid = MakeSyntheticInvocation(
			asBUILD_ARTIFACT_INVOCATION_INVALID);
		asCBuilder::FinalizeBuildArtifactInvocation(Invalid);
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_INELIGIBLE_INVALID_INVOCATION_KIND,
			Invalid.ineligibleReason));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
