#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_property.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheCompilerDependencyCaptureTests_Private
{
	struct FObservedDependency final
	{
		asEBuildArtifactDependencyKind Kind =
			asBUILD_ARTIFACT_DEPENDENCY_INVALID;
		asEBuildArtifactDependencyReferenceKind ReferenceKind =
			asBUILD_ARTIFACT_REFERENCE_INVALID;
		FString Name;
	};

	struct FObservedCompileResult final
	{
		FString FunctionName;
		int32 CompileResult = asERROR;
		bool bSucceeded = false;
		TArray<FObservedDependency> Dependencies;
	};

	struct FCompileResultLog final
	{
		TArray<FObservedCompileResult> Values;
	};

	static FString GetDependencyName(
		const asSBuildArtifactDependency& Dependency)
	{
		switch (Dependency.referenceKind)
		{
		case asBUILD_ARTIFACT_REFERENCE_TYPE:
			return Dependency.type != nullptr
				? UTF8_TO_TCHAR(Dependency.type->GetName()) : FString();
		case asBUILD_ARTIFACT_REFERENCE_FUNCTION:
			return Dependency.function != nullptr
				? UTF8_TO_TCHAR(Dependency.function->GetName()) : FString();
		case asBUILD_ARTIFACT_REFERENCE_GLOBAL:
			return Dependency.globalProperty != nullptr
				? UTF8_TO_TCHAR(Dependency.globalProperty->name.AddressOf())
				: FString();
		case asBUILD_ARTIFACT_REFERENCE_PROPERTY:
			return Dependency.objectProperty != nullptr
				? UTF8_TO_TCHAR(Dependency.objectProperty->name.AddressOf())
				: FString();
		default:
			return FString();
		}
	}

	static void ObserveCompileResult(
		const asSBuildArtifactInvocation* Invocation,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Invocation == nullptr || Result == nullptr || UserData == nullptr)
		{
			return;
		}
		FCompileResultLog& Log = *static_cast<FCompileResultLog*>(UserData);
		FObservedCompileResult& Value = Log.Values.AddDefaulted_GetRef();
		Value.FunctionName = UTF8_TO_TCHAR(Invocation->functionName.AddressOf());
		Value.CompileResult = Result->compileResult;
		Value.bSucceeded = Result->succeeded;
		for (asUINT Index = 0; Index < Result->dependencyCount; ++Index)
		{
			const asSBuildArtifactDependency& Dependency =
				Result->dependencies[Index];
			Value.Dependencies.Add({
				Dependency.kind,
				Dependency.referenceKind,
				GetDependencyName(Dependency),
			});
		}
	}

	static const FObservedCompileResult* FindResult(
		const FCompileResultLog& Log,
		const FStringView FunctionName)
	{
		return Log.Values.FindByPredicate(
			[FunctionName](const FObservedCompileResult& Value)
			{
				return Value.FunctionName == FunctionName;
			});
	}

	static bool HasDependency(
		const FObservedCompileResult& Result,
		const asEBuildArtifactDependencyKind Kind,
		const asEBuildArtifactDependencyReferenceKind ReferenceKind,
		const FStringView Name)
	{
		return Result.Dependencies.ContainsByPredicate(
			[Kind, ReferenceKind, Name](const FObservedDependency& Dependency)
			{
				return Dependency.Kind == Kind
					&& Dependency.ReferenceKind == ReferenceKind
					&& Dependency.Name == Name;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheCompilerDependencyCaptureTests,
	"Angelscript.TestModule.Cache.CompilerDependencyCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SuccessfulCompilePublishesTypedActualDependencies)
	{
		using namespace AngelscriptCacheCompilerDependencyCaptureTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptEngine* ScriptEngine = Fixture.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		asCModule* Module = static_cast<asCModule*>(ScriptEngine->GetModule(
			"ASCacheV2CompilerDependencies", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FCompileResultLog Log;
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &Log);
		const char* Source = R"AS(
class FDependencyValue
{
	int Count;
}

const int GHardValue = 5;

int DependencyCallee(int Value)
{
	return Value + 1;
}

int DependencyConsumer(FDependencyValue Value)
{
	return DependencyCallee(Value.Count) + GHardValue;
}
)AS";
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"CompilerDependencies.as", Source, FCStringAnsi::Strlen(Source), 0)));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		const FObservedCompileResult* Consumer = FindResult(
			Log, TEXT("DependencyConsumer"));
		ASSERT_THAT(IsNotNull(Consumer));
		for (int32 Index = 0; Index < Consumer->Dependencies.Num(); ++Index)
		{
			const FObservedDependency& Dependency = Consumer->Dependencies[Index];
			TestRunner->AddInfo(FString::Printf(
				TEXT("Compiler actual dependency[%d]: Kind=%u Reference=%u Name=%s"),
				Index,
				static_cast<uint32>(Dependency.Kind),
				static_cast<uint32>(Dependency.ReferenceKind),
				*Dependency.Name));
		}
		ASSERT_THAT(IsTrue(Consumer->bSucceeded));
		ASSERT_THAT(AreEqual(0, Consumer->CompileResult));
		ASSERT_THAT(IsTrue(HasDependency(*Consumer,
			asBUILD_ARTIFACT_DEPENDENCY_DECLARATION,
			asBUILD_ARTIFACT_REFERENCE_TYPE,
			TEXT("FDependencyValue"))));
		ASSERT_THAT(IsTrue(HasDependency(*Consumer,
			asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE,
			asBUILD_ARTIFACT_REFERENCE_FUNCTION,
			TEXT("DependencyCallee"))));
		ASSERT_THAT(IsTrue(HasDependency(*Consumer,
			asBUILD_ARTIFACT_DEPENDENCY_HARD_VALUE,
			asBUILD_ARTIFACT_REFERENCE_GLOBAL,
			TEXT("GHardValue"))));
		ASSERT_THAT(IsTrue(HasDependency(*Consumer,
			asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT,
			asBUILD_ARTIFACT_REFERENCE_PROPERTY,
			TEXT("Count"))));

		asCScriptFunction* ConsumerFunction = static_cast<asCScriptFunction*>(
			Module->GetFunctionByName("DependencyConsumer"));
		ASSERT_THAT(IsNotNull(ConsumerFunction));
		ASSERT_THAT(IsNotNull(ConsumerFunction->scriptData));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(Consumer->Dependencies.Num()),
			static_cast<int32>(
				ConsumerFunction->scriptData->artifactDependencies.GetLength())));
	}

	TEST_METHOD(FailedCompilePublishesNoActualDependencies)
	{
		using namespace AngelscriptCacheCompilerDependencyCaptureTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptEngine* ScriptEngine = Fixture.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		asCModule* Module = static_cast<asCModule*>(ScriptEngine->GetModule(
			"ASCacheV2FailedCompilerDependencies", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FCompileResultLog Log;
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &Log);
		TestRunner->AddExpectedErrorPlain(
			TEXT("FailedCompilerDependencies.as:"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		TestRunner->AddExpectedErrorPlain(
			TEXT("'MissingAfterObservedDependency' is not declared"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		const char* Source = R"AS(
const int GObservedBeforeFailure = 9;

int BrokenDependencyConsumer()
{
	int Observed = GObservedBeforeFailure;
	return Observed + MissingAfterObservedDependency;
}
)AS";
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"FailedCompilerDependencies.as", Source,
			FCStringAnsi::Strlen(Source), 0)));
		ASSERT_THAT(IsTrue(Module->Build() < 0));

		const FObservedCompileResult* Broken = FindResult(
			Log, TEXT("BrokenDependencyConsumer"));
		ASSERT_THAT(IsNotNull(Broken));
		ASSERT_THAT(IsFalse(Broken->bSucceeded));
		ASSERT_THAT(IsTrue(Broken->CompileResult < 0));
		ASSERT_THAT(AreEqual(0, Broken->Dependencies.Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
