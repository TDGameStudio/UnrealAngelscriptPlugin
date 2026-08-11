#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_property.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheGeneratedDependencyCaptureTests_Private
{
	struct FDependency final
	{
		asEBuildArtifactDependencyKind Kind =
			asBUILD_ARTIFACT_DEPENDENCY_INVALID;
		asEBuildArtifactDependencyReferenceKind ReferenceKind =
			asBUILD_ARTIFACT_REFERENCE_INVALID;
		FString Name;
	};

	struct FResult final
	{
		asEBuildArtifactInvocationKind InvocationKind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		FString OwnerName;
		FString FunctionName;
		bool bSucceeded = false;
		TArray<FDependency> Dependencies;
	};

	struct FLog final
	{
		TArray<FResult> Values;
	};

	static FString DependencyName(
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
			return {};
		}
	}

	static void Observe(
		const asSBuildArtifactInvocation* Invocation,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Invocation == nullptr || Result == nullptr || UserData == nullptr)
		{
			return;
		}
		FResult& Value = static_cast<FLog*>(UserData)->Values.
			AddDefaulted_GetRef();
		Value.InvocationKind = Invocation->kind;
		Value.OwnerName = UTF8_TO_TCHAR(Invocation->ownerName.AddressOf());
		Value.FunctionName =
			UTF8_TO_TCHAR(Invocation->functionName.AddressOf());
		Value.bSucceeded = Result->succeeded;
		for (asUINT Index = 0; Index < Result->dependencyCount; ++Index)
		{
			const asSBuildArtifactDependency& Dependency =
				Result->dependencies[Index];
			Value.Dependencies.Add({
				Dependency.kind,
				Dependency.referenceKind,
				DependencyName(Dependency),
			});
		}
	}

	static const FResult* Find(
		const FLog& Log,
		const asEBuildArtifactInvocationKind InvocationKind,
		const FStringView OwnerName)
	{
		return Log.Values.FindByPredicate(
			[InvocationKind, OwnerName](const FResult& Value)
			{
				return Value.InvocationKind == InvocationKind
					&& Value.OwnerName == OwnerName;
			});
	}

	static bool Has(
		const FResult& Result,
		const asEBuildArtifactDependencyKind Kind,
		const asEBuildArtifactDependencyReferenceKind ReferenceKind,
		const FStringView Name)
	{
		return Result.Dependencies.ContainsByPredicate(
			[Kind, ReferenceKind, Name](const FDependency& Dependency)
			{
				return Dependency.Kind == Kind
					&& Dependency.ReferenceKind == ReferenceKind
					&& Dependency.Name == Name;
			});
	}

	static void LogResult(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const FResult& Result)
	{
		Test.AddInfo(FString::Printf(
			TEXT("Generated dependency result: Label=%s Kind=%u Owner=%s Function=%s Succeeded=%d Dependencies=%d"),
			Label,
			static_cast<uint32>(Result.InvocationKind),
			*Result.OwnerName,
			*Result.FunctionName,
			Result.bSucceeded ? 1 : 0,
			Result.Dependencies.Num()));
		for (int32 Index = 0; Index < Result.Dependencies.Num(); ++Index)
		{
			const FDependency& Dependency = Result.Dependencies[Index];
			Test.AddInfo(FString::Printf(
				TEXT("Generated actual dependency[%d]: Kind=%u Reference=%u Name=%s"),
				Index,
				static_cast<uint32>(Dependency.Kind),
				static_cast<uint32>(Dependency.ReferenceKind),
				*Dependency.Name));
		}
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheGeneratedDependencyCaptureTests,
	"Angelscript.TestModule.Cache.GeneratedDependencyCapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedFactoryConstructorAndDestructorPublishEmbeddedAuthorities)
	{
		using namespace AngelscriptCacheGeneratedDependencyCaptureTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptEngine* ScriptEngine = Fixture.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		asCModule* Module = static_cast<asCModule*>(ScriptEngine->GetModule(
			"ASCacheV2GeneratedDependencies", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FLog Log;
		Module->SetBuildArtifactCompileResultCallback(&Observe, &Log);
		const char* Source = R"AS(
class FGeneratedDependencyChild
{
	~FGeneratedDependencyChild()
	{
	}
}

class FGeneratedDependencyOwner
{
	FGeneratedDependencyChild Child;
	int Value = 3;
	default Value = 5;
}

class FGeneratedDependencyBase
{
	FGeneratedDependencyBase()
	{
	}

	~FGeneratedDependencyBase()
	{
	}
}

class FGeneratedDependencyDerived : FGeneratedDependencyBase
{
}
)AS";
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"GeneratedDependencies.as", Source, FCStringAnsi::Strlen(Source), 0)));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

		const FResult* Factory = Find(Log,
			asBUILD_ARTIFACT_INVOCATION_FACTORY,
			TEXT("FGeneratedDependencyOwner"));
		const FResult* Constructor = Find(Log,
			asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR,
			TEXT("FGeneratedDependencyOwner"));
		const FResult* Destructor = Find(Log,
			asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR,
			TEXT("FGeneratedDependencyOwner"));
		const FResult* InitDefaults = Find(Log,
			asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS,
			TEXT("FGeneratedDependencyOwner"));
		const FResult* DerivedConstructor = Find(Log,
			asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR,
			TEXT("FGeneratedDependencyDerived"));
		const FResult* DerivedDestructor = Find(Log,
			asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR,
			TEXT("FGeneratedDependencyDerived"));
		ASSERT_THAT(IsNotNull(Factory));
		ASSERT_THAT(IsNotNull(Constructor));
		ASSERT_THAT(IsNotNull(Destructor));
		ASSERT_THAT(IsNotNull(InitDefaults));
		ASSERT_THAT(IsNotNull(DerivedConstructor));
		ASSERT_THAT(IsNotNull(DerivedDestructor));
		LogResult(*TestRunner, TEXT("Factory"), *Factory);
		LogResult(*TestRunner, TEXT("Constructor"), *Constructor);
		LogResult(*TestRunner, TEXT("Destructor"), *Destructor);
		LogResult(*TestRunner, TEXT("InitDefaults"), *InitDefaults);
		LogResult(*TestRunner, TEXT("DerivedConstructor"), *DerivedConstructor);
		LogResult(*TestRunner, TEXT("DerivedDestructor"), *DerivedDestructor);
		ASSERT_THAT(IsTrue(Factory->bSucceeded));
		ASSERT_THAT(IsTrue(Constructor->bSucceeded));
		ASSERT_THAT(IsTrue(Destructor->bSucceeded));

		ASSERT_THAT(IsTrue(Has(*Factory,
			asBUILD_ARTIFACT_DEPENDENCY_DECLARATION,
			asBUILD_ARTIFACT_REFERENCE_TYPE,
			TEXT("FGeneratedDependencyOwner"))));
		ASSERT_THAT(IsTrue(Has(*Factory,
			asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE,
			asBUILD_ARTIFACT_REFERENCE_FUNCTION,
			TEXT("FGeneratedDependencyOwner"))));
		ASSERT_THAT(IsTrue(Has(*Constructor,
			asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT,
			asBUILD_ARTIFACT_REFERENCE_PROPERTY,
			TEXT("Child"))));
		ASSERT_THAT(IsTrue(Has(*Destructor,
			asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT,
			asBUILD_ARTIFACT_REFERENCE_PROPERTY,
			TEXT("Child"))));
		ASSERT_THAT(IsTrue(Has(*Destructor,
			asBUILD_ARTIFACT_DEPENDENCY_DECLARATION,
			asBUILD_ARTIFACT_REFERENCE_TYPE,
			TEXT("FGeneratedDependencyChild"))));
		ASSERT_THAT(IsTrue(Has(*InitDefaults,
			asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT,
			asBUILD_ARTIFACT_REFERENCE_PROPERTY,
			TEXT("Value"))));
		ASSERT_THAT(IsTrue(Has(*DerivedConstructor,
			asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE,
			asBUILD_ARTIFACT_REFERENCE_FUNCTION,
			TEXT("FGeneratedDependencyBase"))));
		ASSERT_THAT(IsTrue(Has(*DerivedConstructor,
			asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT,
			asBUILD_ARTIFACT_REFERENCE_FUNCTION,
			TEXT("FGeneratedDependencyBase"))));
		ASSERT_THAT(IsTrue(Has(*DerivedDestructor,
			asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE,
			asBUILD_ARTIFACT_REFERENCE_FUNCTION,
			TEXT("~FGeneratedDependencyBase"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
