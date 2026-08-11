#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_property.h"
#include "as_restore.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheInvocationFamilyArtifactTests_Private
{
	class FArtifactStream final : public asIBinaryStream
	{
	public:
		virtual int Read(void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| ReadOffset > Bytes.Num()
				|| Size > static_cast<asUINT>(Bytes.Num() - ReadOffset))
			{
				return asERROR;
			}
			if (Size != 0)
			{
				FMemory::Memcpy(Data, Bytes.GetData() + ReadOffset, Size);
				ReadOffset += static_cast<int32>(Size);
			}
			return asSUCCESS;
		}

		virtual int Write(const void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| Size > static_cast<asUINT>(MAX_int32 - Bytes.Num()))
			{
				return asOUT_OF_MEMORY;
			}
			if (Size != 0)
			{
				const int32 Offset = Bytes.AddUninitialized(
					static_cast<int32>(Size));
				FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
			}
			return asSUCCESS;
		}

		void ResetRead()
		{
			ReadOffset = 0;
		}

		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

	struct FCompiledInvocation final
	{
		struct FDependency final
		{
			asEBuildArtifactDependencyKind Kind =
				asBUILD_ARTIFACT_DEPENDENCY_INVALID;
			asEBuildArtifactDependencyReferenceKind ReferenceKind =
				asBUILD_ARTIFACT_REFERENCE_INVALID;
			FString Name;
		};

		asEBuildArtifactInvocationKind Kind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		asEBuildArtifactIneligibleReason IneligibleReason =
			asBUILD_ARTIFACT_INELIGIBLE_INVALID_INVOCATION_KIND;
		FString OwnerName;
		FString FunctionName;
		FString Declaration;
		bool bGenerated = false;
		bool bSucceeded = false;
		asCScriptFunction* Function = nullptr;
		TArray<FDependency> Dependencies;
	};

	struct FCompileLog final
	{
		TArray<FCompiledInvocation> Values;
	};

	struct FArtifactProbe final
	{
		asEBuildArtifactInvocationKind Kind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		FString OwnerName;
		FString Declaration;
		int32 WriteResult = asERROR;
		int32 ValidationResult = asERROR;
		int32 PayloadBytes = 0;
		asSFunctionArtifactWriteDiagnostics Diagnostics{};
		asSFunctionArtifactValidationDiagnostics ValidationDiagnostics{};
	};

	static void ObserveCompileResult(
		const asSBuildArtifactInvocation* Invocation,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Invocation == nullptr || Result == nullptr || UserData == nullptr)
		{
			return;
		}
		FCompiledInvocation& Value =
			static_cast<FCompileLog*>(UserData)->Values.AddDefaulted_GetRef();
		Value.Kind = Invocation->kind;
		Value.IneligibleReason = Invocation->ineligibleReason;
		Value.OwnerName = UTF8_TO_TCHAR(Invocation->ownerName.AddressOf());
		Value.FunctionName = UTF8_TO_TCHAR(
			Invocation->functionName.AddressOf());
		Value.Declaration = UTF8_TO_TCHAR(
			Invocation->declaration.AddressOf());
		Value.bGenerated = Invocation->isGenerated;
		Value.bSucceeded = Result->succeeded;
		Value.Function = Result->function;
		for (asUINT Index = 0; Index < Result->dependencyCount; ++Index)
		{
			const asSBuildArtifactDependency& Raw = Result->dependencies[Index];
			FCompiledInvocation::FDependency& Dependency =
				Value.Dependencies.AddDefaulted_GetRef();
			Dependency.Kind = Raw.kind;
			Dependency.ReferenceKind = Raw.referenceKind;
			switch (Raw.referenceKind)
			{
			case asBUILD_ARTIFACT_REFERENCE_TYPE:
				Dependency.Name = Raw.type != nullptr
					? UTF8_TO_TCHAR(Raw.type->GetName()) : FString();
				break;
			case asBUILD_ARTIFACT_REFERENCE_FUNCTION:
				Dependency.Name = Raw.function != nullptr
					? UTF8_TO_TCHAR(Raw.function->GetName()) : FString();
				break;
			case asBUILD_ARTIFACT_REFERENCE_GLOBAL:
				Dependency.Name = Raw.globalProperty != nullptr
					? UTF8_TO_TCHAR(Raw.globalProperty->name.AddressOf())
					: FString();
				break;
			case asBUILD_ARTIFACT_REFERENCE_PROPERTY:
				Dependency.Name = Raw.objectProperty != nullptr
					? UTF8_TO_TCHAR(Raw.objectProperty->name.AddressOf())
					: FString();
				break;
			default:
				break;
			}
		}
	}

	static const FCompiledInvocation* FindSuccessful(
		const FCompileLog& Log,
		const asEBuildArtifactInvocationKind Kind)
	{
		return Log.Values.FindByPredicate(
			[Kind](const FCompiledInvocation& Value)
			{
				return Value.Kind == Kind
					&& Value.IneligibleReason
						== asBUILD_ARTIFACT_INELIGIBLE_NONE
					&& Value.bSucceeded
					&& Value.Function != nullptr;
			});
	}

	static const TCHAR* KindName(
		const asEBuildArtifactInvocationKind Kind)
	{
		switch (Kind)
		{
		case asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION:
			return TEXT("GlobalFunction");
		case asBUILD_ARTIFACT_INVOCATION_METHOD:
			return TEXT("Method");
		case asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR:
			return TEXT("Constructor");
		case asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR:
			return TEXT("Destructor");
		case asBUILD_ARTIFACT_INVOCATION_FACTORY:
			return TEXT("Factory");
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR:
			return TEXT("GeneratedDefaultConstructor");
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR:
			return TEXT("GeneratedDefaultDestructor");
		case asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS:
			return TEXT("InitDefaults");
		default:
			return TEXT("Invalid");
		}
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheInvocationFamilyArtifactTests,
	"Angelscript.TestModule.Cache.InvocationFamilyArtifact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EveryStableInvocationFamilyProducesACompleteArtifact)
	{
		using namespace AngelscriptCacheInvocationFamilyArtifactTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptEngine* ScriptEngine = Fixture.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine));
		asCModule* Module = static_cast<asCModule*>(ScriptEngine->GetModule(
			"ASCacheV2InvocationFamilyArtifact", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FCompileLog CompileLog;
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &CompileLog);
		const char* Source = R"AS(
class FArtifactLeaf
{
}

class FGeneratedArtifactOwner
{
	FArtifactLeaf Child;
	int Value = 3;
	default Value = 5;

	int Read()
	{
		return Value;
	}
}

class FExplicitArtifactOwner
{
	int Value;

	FExplicitArtifactOwner(int InValue)
	{
		Value = InValue;
	}

	~FExplicitArtifactOwner()
	{
		Value = 0;
	}
}

int GlobalArtifactFunction()
{
	return 7;
}
)AS";
		ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
			"InvocationFamilyArtifact.as", Source,
			FCStringAnsi::Strlen(Source), 0)));
		ASSERT_THAT(AreEqual(asSUCCESS, Module->Build()));

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

		TArray<FArtifactProbe> Probes;
		for (const asEBuildArtifactInvocationKind Kind : RequiredKinds)
		{
			FArtifactProbe& Probe = Probes.AddDefaulted_GetRef();
			Probe.Kind = Kind;
			const FCompiledInvocation* Compiled = FindSuccessful(CompileLog, Kind);
			if (Compiled == nullptr)
			{
				TestRunner->AddInfo(FString::Printf(
					TEXT("Invocation artifact probe: Kind=%s(%u) MissingSuccessfulCompile=1"),
					KindName(Kind), static_cast<uint32>(Kind)));
				continue;
			}

			Probe.OwnerName = Compiled->OwnerName;
			Probe.Declaration = Compiled->Declaration;
			FArtifactStream Stream;
			asCWriter Writer(Module, &Stream, Module->engine, true);
			Probe.WriteResult = Writer.WriteFunctionArtifact(
				Compiled->Function, &Probe.Diagnostics);
			Probe.PayloadBytes = Stream.Bytes.Num();
			if (Probe.WriteResult == asSUCCESS)
			{
				Stream.ResetRead();
				asCReader Reader(Module, &Stream, Module->engine);
				Probe.ValidationResult = Reader.ValidateFunctionArtifact(
					static_cast<asUINT>(Stream.Bytes.Num()),
					&Probe.ValidationDiagnostics);
			}
			TestRunner->AddInfo(FString::Printf(
				TEXT("Invocation artifact probe: Kind=%s(%u) Owner=%s Generated=%d Result=%d Validation=%d Bytes=%d WriteStage=%u ValidationStage=%u ValidationRead=%u TypeIds=%u Types=%u Functions=%u Globals=%u Strings=%u Properties=%u Declaration=%s"),
				KindName(Kind),
				static_cast<uint32>(Kind),
				*Compiled->OwnerName,
				Compiled->bGenerated ? 1 : 0,
				Probe.WriteResult,
				Probe.ValidationResult,
				Probe.PayloadBytes,
				Probe.Diagnostics.stage,
				Probe.ValidationDiagnostics.stage,
				Probe.ValidationDiagnostics.bytesRead,
				Probe.Diagnostics.usedTypeIdCount,
				Probe.Diagnostics.usedTypeCount,
				Probe.Diagnostics.usedFunctionCount,
				Probe.Diagnostics.usedGlobalPropertyCount,
				Probe.Diagnostics.usedStringConstantCount,
				Probe.Diagnostics.usedObjectPropertyCount,
				*Compiled->Declaration));
			for (int32 DependencyIndex = 0;
				DependencyIndex < Compiled->Dependencies.Num(); ++DependencyIndex)
			{
				const FCompiledInvocation::FDependency& Dependency =
					Compiled->Dependencies[DependencyIndex];
				TestRunner->AddInfo(FString::Printf(
					TEXT("Invocation dependency: Kind=%s(%u) Index=%d DependencyKind=%u ReferenceKind=%u Name=%s"),
					KindName(Kind),
					static_cast<uint32>(Kind),
					DependencyIndex,
					static_cast<uint32>(Dependency.Kind),
					static_cast<uint32>(Dependency.ReferenceKind),
					*Dependency.Name));
			}
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(UE_ARRAY_COUNT(RequiredKinds)), Probes.Num()));
		for (const FArtifactProbe& Probe : Probes)
		{
			ASSERT_THAT(AreEqual(asSUCCESS, Probe.WriteResult));
			ASSERT_THAT(AreEqual(asSUCCESS, Probe.ValidationResult));
			ASSERT_THAT(IsTrue(Probe.PayloadBytes > 9));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
