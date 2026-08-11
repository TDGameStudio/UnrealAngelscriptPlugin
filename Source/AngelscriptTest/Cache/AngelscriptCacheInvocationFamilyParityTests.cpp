#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "CQTest.h"
#include "Core/FunctionCallers.h"
#include "Misc/ScopeExit.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_restore.h"
#include "as_scriptfunction.h"
#include "as_typeinfo.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheInvocationFamilyParityTests_Private
{
	static constexpr const char* ModuleName =
		"ASCacheV2InvocationFamilyParity";
	static constexpr const char* SourceSection =
		"InvocationFamilyParity.as";
	static constexpr const TCHAR* CanonicalSourceSection =
		TEXT("/Angelscript/Game/InvocationFamilyParity.as");
	static constexpr const char* Source = R"AS(
class FParityLeaf
{
}

class FGeneratedParityOwner
{
	FParityLeaf Child;
	int Value = 3;
	default Value = 5;

	int Read()
	{
		return Value;
	}
}

struct FExecutableGeneratedValue
{
	int Value = 5;

	int Read()
	{
		return Value;
	}
}

struct FExecutableExplicitValue
{
	int Value;

	FExecutableExplicitValue()
	{
		Value = 4;
	}

	~FExecutableExplicitValue()
	{
		RecordParityDestructor(Value);
	}

	int Read()
	{
		return Value;
	}
}

class ExplicitParityOwner
{
	int Value;

	ExplicitParityOwner()
	{
		Value = 4;
	}

	~ExplicitParityOwner()
	{
		RecordParityDestructor(Value);
	}

	int Read()
	{
		return Value;
	}
}

int GlobalParityValue()
{
	return 7;
}

int RunGeneratedParityBehavior()
{
	FExecutableGeneratedValue Generated;
	return GlobalParityValue() + Generated.Read();
}

int RunExplicitParityBehavior()
{
	FExecutableExplicitValue Explicit;
	return Explicit.Read();
}
)AS";

	static constexpr asPWORD BehaviorObservationUserDataSlot =
		static_cast<asPWORD>(0x4341434845563535ull);

	struct FBehaviorObservation final
	{
		int32 DestructorValue = 0;
	};

	static void RecordParityDestructor(const int32 Value)
	{
		asIScriptContext* Context = asGetActiveContext();
		asIScriptEngine* Engine = Context != nullptr
			? Context->GetEngine() : nullptr;
		FBehaviorObservation* Observation = Engine != nullptr
			? static_cast<FBehaviorObservation*>(Engine->GetUserData(
				BehaviorObservationUserDataSlot)) : nullptr;
		if (Observation != nullptr)
		{
			Observation->DestructorValue += Value;
		}
	}

	static bool RegisterBehaviorObservation(
		asIScriptEngine& Engine,
		FBehaviorObservation& Observation)
	{
		Engine.SetUserData(&Observation, BehaviorObservationUserDataSlot);
		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(RecordParityDestructor);
		return Engine.RegisterGlobalFunction(
			"void RecordParityDestructor(int Value)",
			asFUNCTION(RecordParityDestructor),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller) >= 0;
	}

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

		TArray<uint8> Bytes;
		int32 ReadOffset = 0;
	};

	struct FObservedCompile final
	{
		asEBuildArtifactInvocationKind Kind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		asEBuildArtifactIneligibleReason IneligibleReason =
			asBUILD_ARTIFACT_INELIGIBLE_INVALID_INVOCATION_KIND;
		FString Namespace;
		FString OwnerName;
		FString FunctionName;
		FString Declaration;
		FString CanonicalSource;
		FString SourceSectionName;
		bool bGenerated = false;
		bool bSucceeded = false;
		bool bCompilerInvoked = false;
		asEBuildArtifactRestoreResult RestoreResult =
			asBUILD_ARTIFACT_RESTORE_MISS;
		asCScriptFunction* Function = nullptr;
	};

	struct FCompileLog final
	{
		TArray<FObservedCompile> Values;
	};

	struct FPersistedArtifact final
	{
		asEBuildArtifactInvocationKind Kind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		FString Namespace;
		FString OwnerName;
		FString Declaration;
		FAngelscriptCachedFunctionBody Body;
		FAngelscriptCachedDebugSidecar Debug;
		TArray<uint8> RawExecutionPayload;
		FAngelscriptHash256 VmStateHash;
		FString VmStateDescription;
		int32 ProducerFunctionId = -1;
	};

	struct FRestoreObservation final
	{
		asEBuildArtifactInvocationKind Kind =
			asBUILD_ARTIFACT_INVOCATION_INVALID;
		FString OwnerName;
		FString Declaration;
		asEBuildArtifactRestoreResult Result =
			asBUILD_ARTIFACT_RESTORE_MISS;
		int32 ByteCodeWordsBefore = -1;
		int32 ByteCodeWordsAfter = -1;
		FString Detail;
	};

	struct FRestoreContext final
	{
		const TArray<FPersistedArtifact>* Artifacts = nullptr;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TArray<FRestoreObservation> Values;
		int32 FactoryForwardReferenceChecks = 0;
		int32 FactoryForwardReferenceFailures = 0;
		TArray<FString> FactoryForwardReferenceDetails;
	};

	struct FArtifactKeys final
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptArtifactProfileKey Profile;
		FAngelscriptCachedSourceFileKey SourceFileKey;
	};

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

	static TOptional<EAngelscriptArtifactEntityKind> MapKind(
		const asEBuildArtifactInvocationKind Kind)
	{
		switch (Kind)
		{
		case asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION:
			return EAngelscriptArtifactEntityKind::GlobalFunction;
		case asBUILD_ARTIFACT_INVOCATION_METHOD:
			return EAngelscriptArtifactEntityKind::Method;
		case asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR:
			return EAngelscriptArtifactEntityKind::Constructor;
		case asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR:
			return EAngelscriptArtifactEntityKind::Destructor;
		case asBUILD_ARTIFACT_INVOCATION_FACTORY:
			return EAngelscriptArtifactEntityKind::Factory;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR:
			return EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR:
			return EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor;
		case asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS:
			return EAngelscriptArtifactEntityKind::InitDefaults;
		default:
			return {};
		}
	}

	static FAngelscriptHash256 HashCoordinate(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	class FStableDependencyResolver final
		: public IAngelscriptCacheBuildDependencyResolver
	{
	public:
		FStableDependencyResolver(
			const asCModule& InModule,
			const FAngelscriptStableModuleKey& InModuleKey)
			: Module(InModule)
			, ModuleKey(InModuleKey)
		{
		}

		virtual bool Resolve(
			const asSBuildArtifactDependency& RawDependency,
			FAngelscriptCacheSemanticDependency& OutDependency) const override
		{
			OutDependency = {};
			if (RawDependency.descriptorVersion != 1)
			{
				return false;
			}

			switch (RawDependency.referenceKind)
			{
			case asBUILD_ARTIFACT_REFERENCE_TYPE:
				return ResolveType(RawDependency, OutDependency);
			case asBUILD_ARTIFACT_REFERENCE_PROPERTY:
				return ResolveProperty(RawDependency, OutDependency);
			case asBUILD_ARTIFACT_REFERENCE_GLOBAL:
				return ResolveGlobal(RawDependency, OutDependency);
			case asBUILD_ARTIFACT_REFERENCE_FUNCTION:
				return ResolveFunction(RawDependency, OutDependency);
			default:
				return false;
			}
		}

	private:
		FAngelscriptHash256 BuildFingerprint(
			const FStringView Domain,
			const FAngelscriptHash256& StableKey) const
		{
			return HashCoordinate(Domain, StableKey.ToHexString());
		}

		bool TryBuildLocalTypeReference(
			const asCTypeInfo* Type,
			FAngelscriptCacheStableReference& OutReference) const
		{
			OutReference = {};
			FAngelscriptStableTypeKey TypeKey;
			if (Type == nullptr || Type->module != &Module
				|| !FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
					ModuleKey, *Type, TypeKey))
			{
				return false;
			}
			OutReference.Kind = EAngelscriptCacheReferenceKind::ScriptType;
			OutReference.StableKey = TypeKey.Hash;
			OutReference.ExpectedAbi = BuildFingerprint(
				TEXT("cache-v2-v55-test-type-abi"), TypeKey.Hash);
			return true;
		}

		bool ResolveType(
			const asSBuildArtifactDependency& Raw,
			FAngelscriptCacheSemanticDependency& Out) const
		{
			if (Raw.type == nullptr || Raw.function != nullptr
				|| Raw.globalProperty != nullptr
				|| Raw.propertyOwnerType != nullptr
				|| Raw.objectProperty != nullptr)
			{
				return false;
			}
			if (Raw.type->module == nullptr)
			{
				if ((Raw.kind != asBUILD_ARTIFACT_DEPENDENCY_DECLARATION
						&& Raw.kind != asBUILD_ARTIFACT_DEPENDENCY_VALUE_LAYOUT)
					|| !FAngelscriptCacheEnvironmentIdentity::TryBuildTypeReference(
						*Raw.type, Out.Target))
				{
					return false;
				}
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
				return true;
			}
			if (!TryBuildLocalTypeReference(Raw.type, Out.Target))
			{
				return false;
			}
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_DECLARATION)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
				return true;
			}
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_VALUE_LAYOUT)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::ValueLayout;
				Out.ExpectedContentOrValue = BuildFingerprint(
					TEXT("cache-v2-v55-test-type-layout"),
					Out.Target.StableKey);
				return true;
			}
			return false;
		}

		bool ResolveProperty(
			const asSBuildArtifactDependency& Raw,
			FAngelscriptCacheSemanticDependency& Out) const
		{
			if (Raw.kind != asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT
				|| Raw.type != nullptr || Raw.function != nullptr
				|| Raw.globalProperty != nullptr
				|| Raw.propertyOwnerType == nullptr
				|| Raw.objectProperty == nullptr
				|| Raw.propertyOwnerType->module != &Module)
			{
				return false;
			}
			FAngelscriptStableTypeKey OwnerTypeKey;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				ModuleKey, *Raw.propertyOwnerType, OwnerTypeKey))
			{
				return false;
			}
			FAngelscriptPropertyIdentityDescriptor Identity;
			Identity.OwnerTypeKey = OwnerTypeKey;
			Identity.Kind = EAngelscriptArtifactEntityKind::Property;
			Identity.Name = UTF8_TO_TCHAR(
				Raw.objectProperty->name.AddressOf());
			Identity.CanonicalType = UTF8_TO_TCHAR(
				Raw.objectProperty->type.Format(
					Raw.propertyOwnerType->nameSpace, false, false).AddressOf());
			const FAngelscriptStablePropertyKey PropertyKey =
				FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(Identity);
			if (PropertyKey.Hash.IsZero())
			{
				return false;
			}
			Out.Kind = EAngelscriptCacheSemanticDependencyKind::PropertyLayout;
			Out.Target.Kind = EAngelscriptCacheReferenceKind::ScriptProperty;
			Out.Target.StableKey = PropertyKey.Hash;
			Out.Target.ExpectedAbi = BuildFingerprint(
				TEXT("cache-v2-v55-test-property-abi"), PropertyKey.Hash);
			Out.ExpectedContentOrValue = BuildFingerprint(
				TEXT("cache-v2-v55-test-property-layout"), PropertyKey.Hash);
			return true;
		}

		bool ResolveGlobal(
			const asSBuildArtifactDependency& Raw,
			FAngelscriptCacheSemanticDependency& Out) const
		{
			if (Raw.type != nullptr || Raw.function != nullptr
				|| Raw.globalProperty == nullptr
				|| Raw.globalProperty->module != &Module
				|| Raw.propertyOwnerType != nullptr
				|| Raw.objectProperty != nullptr)
			{
				return false;
			}
			FAngelscriptGlobalIdentityDescriptor Identity;
			Identity.ModuleKey = ModuleKey;
			Identity.Namespace = Raw.globalProperty->nameSpace != nullptr
				? UTF8_TO_TCHAR(Raw.globalProperty->nameSpace->name.AddressOf())
				: FString();
			Identity.Kind = EAngelscriptArtifactEntityKind::GlobalVariable;
			Identity.Name = UTF8_TO_TCHAR(Raw.globalProperty->name.AddressOf());
			Identity.CanonicalType = UTF8_TO_TCHAR(
				Raw.globalProperty->type.Format(
					Raw.globalProperty->nameSpace, false, false).AddressOf());
			const FAngelscriptStableGlobalKey GlobalKey =
				FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(Identity);
			if (GlobalKey.Hash.IsZero())
			{
				return false;
			}
			Out.Target.Kind = EAngelscriptCacheReferenceKind::ScriptGlobal;
			Out.Target.StableKey = GlobalKey.Hash;
			Out.Target.ExpectedAbi = BuildFingerprint(
				TEXT("cache-v2-v55-test-global-abi"), GlobalKey.Hash);
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_GLOBAL_STORAGE)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::GlobalStorage;
				Out.ExpectedContentOrValue = BuildFingerprint(
					TEXT("cache-v2-v55-test-global-layout"), GlobalKey.Hash);
				return true;
			}
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_HARD_VALUE)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::HardValue;
				Out.ExpectedContentOrValue = BuildFingerprint(
					TEXT("cache-v2-v55-test-global-value"), GlobalKey.Hash);
				return true;
			}
			return false;
		}

		bool ResolveFunction(
			const asSBuildArtifactDependency& Raw,
			FAngelscriptCacheSemanticDependency& Out) const
		{
			if (Raw.type != nullptr || Raw.function == nullptr
				|| Raw.globalProperty != nullptr
				|| Raw.propertyOwnerType != nullptr
				|| Raw.objectProperty != nullptr)
			{
				return false;
			}
			if (Raw.function->module == nullptr)
			{
				if ((Raw.kind != asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE
						&& Raw.kind != asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT)
					|| !FAngelscriptCacheEnvironmentIdentity::TryBuildFunctionReference(
						*Raw.function, Out.Target))
				{
					return false;
				}
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi;
				return true;
			}
			if (Raw.function->module != &Module)
			{
				return false;
			}
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE
				&& Raw.function->artifactInvocationKind
					== asBUILD_ARTIFACT_INVOCATION_FACTORY
				&& Raw.function->GetParamCount() == 0
				&& Raw.function->artifactOwnerType != nullptr
				&& Raw.function->returnType.GetTypeInfo()
					== Raw.function->artifactOwnerType)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::Declaration;
				return TryBuildLocalTypeReference(
					Raw.function->artifactOwnerType, Out.Target);
			}
			FAngelscriptStableFunctionKey FunctionKey;
			if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Raw.function, FunctionKey)
				|| FunctionKey.Hash.IsZero())
			{
				return false;
			}
			Out.Target.Kind = EAngelscriptCacheReferenceKind::ScriptFunction;
			Out.Target.StableKey = FunctionKey.Hash;
			Out.Target.ExpectedAbi = BuildFingerprint(
				TEXT("cache-v2-v55-test-function-abi"), FunctionKey.Hash);
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::Signature;
				return true;
			}
			if (Raw.kind == asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT)
			{
				Out.Kind = EAngelscriptCacheSemanticDependencyKind::FunctionContent;
				Out.ExpectedContentOrValue = BuildFingerprint(
					TEXT("cache-v2-v55-test-function-content"), FunctionKey.Hash);
				return true;
			}
			return false;
		}

		const asCModule& Module;
		FAngelscriptStableModuleKey ModuleKey;
	};

	static FArtifactKeys MakeKeys()
	{
		FArtifactKeys Keys;
		const TOptional<FAngelscriptStableModuleKey> ModuleKey =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("/Angelscript/Game"),
				TEXT("InvocationFamilyParity.as"),
				ANSI_TO_TCHAR(ModuleName));
		check(ModuleKey.IsSet());
		Keys.ModuleKey = ModuleKey.GetValue();

		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2InvocationFamilyParity"),
			TEXT("VmExecutionCodec=3"),
		};
		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("InvocationFamilyParity"),
			TEXT("DebugSidecar=Enabled"),
		};
		Keys.Profile = FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility),
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context));
		Keys.SourceFileKey.Hash = HashCoordinate(
			TEXT("cache-v2-v55-source-file"), CanonicalSourceSection);
		return Keys;
	}

	static FAngelscriptStableFunctionKey BuildStableFunctionKey(
		const FObservedCompile& Value,
		const FArtifactKeys& Keys)
	{
		check(Value.Function != nullptr);
		FAngelscriptStableFunctionKey Key;
		FString Failure;
		checkf(FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			Keys.ModuleKey, *Value.Function, Key, &Failure),
			TEXT("Production stable FunctionKey failed: %s"), *Failure);
		return Key;
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
		FObservedCompile& Value =
			static_cast<FCompileLog*>(UserData)->Values.AddDefaulted_GetRef();
		Value.Kind = Invocation->kind;
		Value.IneligibleReason = Invocation->ineligibleReason;
		Value.Namespace = UTF8_TO_TCHAR(Invocation->nameSpace.AddressOf());
		Value.OwnerName = UTF8_TO_TCHAR(Invocation->ownerName.AddressOf());
		Value.FunctionName = UTF8_TO_TCHAR(
			Invocation->functionName.AddressOf());
		Value.Declaration = UTF8_TO_TCHAR(
			Invocation->declaration.AddressOf());
		Value.CanonicalSource = UTF8_TO_TCHAR(
			Invocation->canonicalSource.AddressOf());
		Value.SourceSectionName = UTF8_TO_TCHAR(
			Invocation->sourceSection.AddressOf());
		Value.bGenerated = Invocation->isGenerated;
		Value.bSucceeded = Result->succeeded;
		Value.bCompilerInvoked = Result->compilerInvoked;
		Value.RestoreResult = Result->restoreResult;
		Value.Function = Result->function;
	}

	static bool WriteExecutionArtifact(
		asCModule& Module,
		asCScriptFunction& Function,
		TArray<uint8>& OutPayload,
		asSFunctionArtifactWriteDiagnostics* OutDiagnostics = nullptr)
	{
		FArtifactStream Stream;
		asCWriter Writer(&Module, &Stream, Module.engine, true);
		if (Writer.WriteFunctionArtifact(&Function, OutDiagnostics) != asSUCCESS)
		{
			return false;
		}
		OutPayload = MoveTemp(Stream.Bytes);
		return !OutPayload.IsEmpty();
	}

	static bool BuildDebugArtifact(
		const asCScriptFunction& Function,
		const FArtifactKeys& Keys,
		TArray<uint8>& OutPayload)
	{
		if (Function.scriptData == nullptr)
		{
			return false;
		}
		FAngelscriptFunctionDebugArtifact Artifact;
		FAngelscriptCachedDebugSourceReference& SourceReference =
			Artifact.Sources.AddDefaulted_GetRef();
		SourceReference.SourceFileKey = Keys.SourceFileKey;
		SourceReference.CanonicalLogicalSection = CanonicalSourceSection;
		if (!FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
				SourceReference.SourceFileKey,
				SourceReference.CanonicalLogicalSection,
				SourceReference.LogicalSectionKey).IsSuccess())
		{
			return false;
		}

		Artifact.DeclaredAt = static_cast<uint32>(Function.scriptData->declaredAt);
		for (asUINT Index = 0;
			Index < Function.scriptData->lineNumbers.GetLength(); ++Index)
		{
			Artifact.LineNumbers.Add(static_cast<uint32>(
				Function.scriptData->lineNumbers[Index]));
		}
		if ((Function.scriptData->sectionIdxs.GetLength() & 1u) != 0)
		{
			return false;
		}
		for (asUINT Index = 0;
			Index < Function.scriptData->sectionIdxs.GetLength(); Index += 2)
		{
			Artifact.SectionTransitions.Add({
				static_cast<uint32>(Function.scriptData->sectionIdxs[Index]), 0});
		}
		for (asUINT Index = 0; Index < Function.parameterNames.GetLength(); ++Index)
		{
			Artifact.ParameterNames.Add(UTF8_TO_TCHAR(
				Function.parameterNames[Index].AddressOf()));
		}
		for (asUINT Index = 0;
			Index < Function.scriptData->temporaryVariables.GetLength(); ++Index)
		{
			Artifact.TemporaryVariables.Add({
				static_cast<uint32>(
					Function.scriptData->temporaryVariables[Index].Offset),
				static_cast<uint32>(
					Function.scriptData->temporaryVariables[Index].Token)});
		}
		for (asUINT Index = 0;
			Index < Function.scriptData->variables.GetLength(); ++Index)
		{
			const asSScriptVariable* Variable = Function.scriptData->variables[Index];
			if (Variable == nullptr)
			{
				return false;
			}
			Artifact.LocalVariables.Add({
				UTF8_TO_TCHAR(Variable->name.AddressOf()),
				Variable->declaredAtProgramPos});
		}
		return FAngelscriptFunctionArtifactCodec::EncodeDebugArtifact(
			Artifact, OutPayload).IsSuccess();
	}

	static FAngelscriptHash256 BuildVmStateHash(
		const asCScriptFunction& Function,
		const TConstArrayView<uint8> ExecutionPayload,
		const TConstArrayView<uint8> DebugPayload)
	{
		check(Function.scriptData != nullptr);
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-v55-complete-vm-state"));
		Writer.WriteBytes(ExecutionPayload);
		Writer.WriteBytes(DebugPayload);
		Writer.WriteUInt32(Function.traits.traits);
		Writer.WriteBool(Function.dontCleanUpOnException);
		Writer.WriteUInt32(Function.scriptData->variableSpace);
		Writer.WriteUInt32(static_cast<uint32>(Function.scriptData->stackNeeded));
		Writer.WriteUInt32(Function.scriptData->objVariablesOnHeap);

		Writer.WriteUInt32(Function.scriptData->objVariableTypes.GetLength());
		for (asUINT Index = 0;
			Index < Function.scriptData->objVariableTypes.GetLength(); ++Index)
		{
			const asCTypeInfo* Type = Function.scriptData->objVariableTypes[Index];
			Writer.WriteString(Type != nullptr
				? FString(UTF8_TO_TCHAR(Type->GetNamespace())) : FString());
			Writer.WriteString(Type != nullptr
				? FString(UTF8_TO_TCHAR(Type->GetName())) : FString());
		}
		Writer.WriteUInt32(Function.scriptData->objVariablePos.GetLength());
		for (asUINT Index = 0;
			Index < Function.scriptData->objVariablePos.GetLength(); ++Index)
		{
			Writer.WriteUInt32(static_cast<uint32>(
				Function.scriptData->objVariablePos[Index]));
		}

		Writer.WriteUInt32(Function.scriptData->objVariableInfo.GetLength());
		for (asUINT Index = 0;
			Index < Function.scriptData->objVariableInfo.GetLength(); ++Index)
		{
			const asSObjectVariableInfo& Value =
				Function.scriptData->objVariableInfo[Index];
			Writer.WriteUInt32(Value.programPos);
			Writer.WriteUInt32(static_cast<uint32>(Value.variableOffset));
			Writer.WriteUInt32(static_cast<uint32>(Value.option));
		}

		Writer.WriteUInt32(Function.scriptData->tryCatchInfo.GetLength());
		for (asUINT Index = 0;
			Index < Function.scriptData->tryCatchInfo.GetLength(); ++Index)
		{
			const asSTryCatchInfo& Value = Function.scriptData->tryCatchInfo[Index];
			Writer.WriteUInt32(Value.tryPos);
			Writer.WriteUInt32(Value.catchPos);
			Writer.WriteUInt32(static_cast<uint32>(Value.stackOffset));
		}

		Writer.WriteUInt32(Function.scriptData->variables.GetLength());
		for (asUINT Index = 0;
			Index < Function.scriptData->variables.GetLength(); ++Index)
		{
			const asSScriptVariable* Variable = Function.scriptData->variables[Index];
			Writer.WriteBool(Variable != nullptr);
			if (Variable != nullptr)
			{
				Writer.WriteString(UTF8_TO_TCHAR(Variable->name.AddressOf()));
				Writer.WriteString(UTF8_TO_TCHAR(
					Variable->type.Format(Function.nameSpace).AddressOf()));
				Writer.WriteUInt32(static_cast<uint32>(Variable->stackOffset));
				Writer.WriteBool(Variable->onHeap);
				Writer.WriteUInt32(Variable->declaredAtProgramPos);
			}
		}
		return Writer.FinalizeHash();
	}

	static FString DescribeVmState(const asCScriptFunction& Function)
	{
		check(Function.scriptData != nullptr);
		const asCScriptFunction::ScriptFunctionData& Data = *Function.scriptData;
		TArray<FString> ObjectTypes;
		for (asUINT Index = 0; Index < Data.objVariableTypes.GetLength(); ++Index)
		{
			const asCTypeInfo* Type = Data.objVariableTypes[Index];
			ObjectTypes.Add(Type != nullptr
				? FString::Printf(TEXT("%s::%s"),
					UTF8_TO_TCHAR(Type->GetNamespace()),
					UTF8_TO_TCHAR(Type->GetName()))
				: TEXT("<null>"));
		}
		TArray<FString> ObjectPositions;
		for (asUINT Index = 0; Index < Data.objVariablePos.GetLength(); ++Index)
		{
			ObjectPositions.Add(FString::FromInt(Data.objVariablePos[Index]));
		}
		TArray<FString> ObjectInfo;
		for (asUINT Index = 0; Index < Data.objVariableInfo.GetLength(); ++Index)
		{
			const asSObjectVariableInfo& Value = Data.objVariableInfo[Index];
			ObjectInfo.Add(FString::Printf(TEXT("%u:%d:%u"),
				Value.programPos, Value.variableOffset,
				static_cast<uint32>(Value.option)));
		}
		TArray<FString> TryCatch;
		for (asUINT Index = 0; Index < Data.tryCatchInfo.GetLength(); ++Index)
		{
			const asSTryCatchInfo& Value = Data.tryCatchInfo[Index];
			TryCatch.Add(FString::Printf(TEXT("%u:%u:%d"),
				Value.tryPos, Value.catchPos, Value.stackOffset));
		}
		TArray<FString> Variables;
		for (asUINT Index = 0; Index < Data.variables.GetLength(); ++Index)
		{
			const asSScriptVariable* Variable = Data.variables[Index];
			Variables.Add(Variable != nullptr
				? FString::Printf(TEXT("%s:%s:%d:%d:%u"),
					UTF8_TO_TCHAR(Variable->name.AddressOf()),
					UTF8_TO_TCHAR(Variable->type.Format(
						Function.nameSpace).AddressOf()),
					Variable->stackOffset, Variable->onHeap ? 1 : 0,
					Variable->declaredAtProgramPos)
				: TEXT("<null>"));
		}
		return FString::Printf(
			TEXT("Traits=0x%08x DontCleanup=%d VariableSpace=%u StackNeeded=%d ObjOnHeap=%u ObjTypes=[%s] ObjPos=[%s] ObjInfo=[%s] TryCatch=[%s] Variables=[%s]"),
			Function.traits.traits,
			Function.dontCleanUpOnException ? 1 : 0,
			Data.variableSpace,
			Data.stackNeeded,
			Data.objVariablesOnHeap,
			*FString::Join(ObjectTypes, TEXT(",")),
			*FString::Join(ObjectPositions, TEXT(",")),
			*FString::Join(ObjectInfo, TEXT(",")),
			*FString::Join(TryCatch, TEXT(",")),
			*FString::Join(Variables, TEXT(",")));
	}

	static bool SameCoordinate(
		const FPersistedArtifact& Artifact,
		const asSBuildArtifactInvocation& Invocation)
	{
		return Artifact.Kind == Invocation.kind
			&& Artifact.Namespace == UTF8_TO_TCHAR(
				Invocation.nameSpace.AddressOf())
			&& Artifact.OwnerName == UTF8_TO_TCHAR(
				Invocation.ownerName.AddressOf())
			&& Artifact.Declaration == UTF8_TO_TCHAR(
				Invocation.declaration.AddressOf());
	}

	static const FPersistedArtifact* FindArtifact(
		const TArray<FPersistedArtifact>& Artifacts,
		const asSBuildArtifactInvocation& Invocation)
	{
		return Artifacts.FindByPredicate(
			[&Invocation](const FPersistedArtifact& Artifact)
			{
				return SameCoordinate(Artifact, Invocation);
			});
	}

	static asEBuildArtifactRestoreResult RestoreArtifact(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FRestoreContext& Context = *static_cast<FRestoreContext*>(UserData);
		FRestoreObservation& Observation =
			Context.Values.AddDefaulted_GetRef();
		Observation.Kind = Invocation->kind;
		Observation.OwnerName = UTF8_TO_TCHAR(Invocation->ownerName.AddressOf());
		Observation.Declaration = UTF8_TO_TCHAR(
			Invocation->declaration.AddressOf());
		Observation.ByteCodeWordsBefore = Function->scriptData != nullptr
			? static_cast<int32>(Function->scriptData->byteCode.GetLength()) : -1;

		const FPersistedArtifact* Artifact = Context.Artifacts != nullptr
			? FindArtifact(*Context.Artifacts, *Invocation) : nullptr;
		if (Artifact == nullptr)
		{
			Observation.Result = asBUILD_ARTIFACT_RESTORE_MISS;
			Observation.Detail = TEXT("No exact pointer-free artifact coordinate");
			Observation.ByteCodeWordsAfter = Observation.ByteCodeWordsBefore;
			return Observation.Result;
		}

		if (Invocation->kind == asBUILD_ARTIFACT_INVOCATION_FACTORY)
		{
			++Context.FactoryForwardReferenceChecks;
			const asCObjectType* OwnerType =
				static_cast<const asCObjectType*>(Function->artifactOwnerType);
			const asCScriptFunction* Constructor = OwnerType != nullptr
				&& OwnerType->beh.construct > 0
				&& Function->module != nullptr
				&& Function->module->engine != nullptr
					? Function->module->engine->scriptFunctions[
						OwnerType->beh.construct]
					: nullptr;
			FAngelscriptStableFunctionKey ConstructorKey;
			FString KeyFailure;
			const bool bStableKeyBuilt = Constructor != nullptr
				&& FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
					Artifact->Body.ModuleKey,
					*Constructor,
					ConstructorKey,
					&KeyFailure);
			const bool bConstructorKind = Constructor != nullptr
				&& (Constructor->artifactInvocationKind
						== asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR
					|| Constructor->artifactInvocationKind
						== asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR);
			const bool bOwnerMatches = Constructor != nullptr
				&& Constructor->artifactOwnerType == OwnerType
				&& Constructor->objectType == OwnerType;
			Context.FactoryForwardReferenceDetails.Add(FString::Printf(
				TEXT("Factory forward reference: Factory=%s Owner=%s Constructor=%s Kind=%u OwnerMatches=%d StableKeyBuilt=%d StableKey=%s Failure=%s"),
				UTF8_TO_TCHAR(Function->GetDeclaration(false, false, false)),
				OwnerType != nullptr
					? UTF8_TO_TCHAR(OwnerType->GetName()) : TEXT("<none>"),
				Constructor != nullptr
					? UTF8_TO_TCHAR(Constructor->GetDeclaration(
						false, false, false)) : TEXT("<none>"),
				Constructor != nullptr
					? static_cast<uint32>(Constructor->artifactInvocationKind) : 0u,
				bOwnerMatches ? 1 : 0,
				bStableKeyBuilt ? 1 : 0,
				bStableKeyBuilt ? *ConstructorKey.Hash.ToHexString() : TEXT("<none>"),
				KeyFailure.IsEmpty() ? TEXT("<none>") : *KeyFailure));
			if (!bConstructorKind || !bOwnerMatches || !bStableKeyBuilt)
			{
				++Context.FactoryForwardReferenceFailures;
			}
		}

		Observation.Result =
			FAngelscriptCacheCompilerBridge::TryRestoreFunctionArtifact(
				*Invocation,
				*Function,
				Artifact->Body,
				Artifact->Debug,
				Context.Limits,
				Context.Budget,
				Observation.Detail);
		Observation.ByteCodeWordsAfter = Function->scriptData != nullptr
			? static_cast<int32>(Function->scriptData->byteCode.GetLength()) : -1;
		return Observation.Result;
	}

	static bool BuildRawModule(
		asCModule& Module,
		FCompileLog& OutLog)
	{
		Module.SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &OutLog);
		return Module.AddScriptSection(
			SourceSection, Source, FCStringAnsi::Strlen(Source), 0) >= 0
			&& Module.Build() >= 0;
	}

	static bool ExecuteBehavior(
		FAngelscriptTestFixture& Fixture,
		asCModule& Module,
		FBehaviorObservation& Observation,
		int32& OutRunValue,
		int32& OutDestructorValue)
	{
		Observation.DestructorValue = 0;
		asIScriptFunction* RunGenerated = Module.GetFunctionByDecl(
			"int RunGeneratedParityBehavior()");
		asIScriptFunction* RunExplicit = Module.GetFunctionByDecl(
			"int RunExplicitParityBehavior()");
		int32 GeneratedValue = 0;
		int32 ExplicitValue = 0;
		const bool bExecuted = RunGenerated != nullptr
			&& RunExplicit != nullptr
			&& Fixture.ExecuteInt(*RunGenerated, GeneratedValue)
			&& Fixture.ExecuteInt(*RunExplicit, ExplicitValue);
		OutRunValue = GeneratedValue + ExplicitValue;
		OutDestructorValue = Observation.DestructorValue;
		return bExecuted;
	}

	static bool CaptureProducerArtifacts(
		FAutomationTestBase& Test,
		const FArtifactKeys& Keys,
		TArray<FPersistedArtifact>& OutArtifacts,
		int32& OutRunValue,
		int32& OutDestructorValue)
	{
		OutArtifacts.Reset();
		FAngelscriptTestFixture Producer(Test, ETestEngineMode::IsolatedFull);
		if (!Producer.IsValid())
		{
			return false;
		}
		asIScriptEngine* Engine = Producer.GetEngine().GetScriptEngine();
		FBehaviorObservation BehaviorObservation;
		if (Engine == nullptr
			|| !RegisterBehaviorObservation(*Engine, BehaviorObservation))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Engine->SetUserData(nullptr, BehaviorObservationUserDataSlot);
		};
		asCModule* Module = Engine != nullptr
			? static_cast<asCModule*>(Engine->GetModule(
				ModuleName, asGM_ALWAYS_CREATE)) : nullptr;
		FCompileLog CompileLog;
		if (Module == nullptr || !BuildRawModule(*Module, CompileLog))
		{
			return false;
		}
		FStableDependencyResolver DependencyResolver(*Module, Keys.ModuleKey);

		for (const FObservedCompile& Compile : CompileLog.Values)
		{
			if (Compile.IneligibleReason != asBUILD_ARTIFACT_INELIGIBLE_NONE
				|| !Compile.bSucceeded || !Compile.bCompilerInvoked
				|| Compile.Function == nullptr || !MapKind(Compile.Kind).IsSet())
			{
				continue;
			}
			FPersistedArtifact& Artifact = OutArtifacts.AddDefaulted_GetRef();
			Artifact.Kind = Compile.Kind;
			Artifact.Namespace = Compile.Namespace;
			Artifact.OwnerName = Compile.OwnerName;
			Artifact.Declaration = Compile.Declaration;
			Artifact.ProducerFunctionId = Compile.Function->id;

			Artifact.Body.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::
					FunctionBodyPayloadSchemaVersion;
			Artifact.Body.ModuleKey = Keys.ModuleKey;
			Artifact.Body.Identity.FunctionKey =
				BuildStableFunctionKey(Compile, Keys);
			Artifact.Body.Identity.Profile = Keys.Profile;
			Artifact.Body.ExpectedDeclarationAbi = HashCoordinate(
				TEXT("cache-v2-v55-declaration-abi"), Compile.Declaration);
			Artifact.Body.FunctionSourceDigest.Hash = HashCoordinate(
				TEXT("cache-v2-v55-function-source"), Compile.CanonicalSource);
			Artifact.Body.FunctionInputDigest.Hash = HashCoordinate(
				TEXT("cache-v2-v55-function-input"), Compile.Declaration);
			Artifact.Body.InvocationKind =
				static_cast<EAngelscriptCachedFunctionInvocationKind>(
					static_cast<uint8>(Compile.Kind));
			Artifact.Body.VmExecutionCodecVersion =
				FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion;
			if (!WriteExecutionArtifact(
					*Module, *Compile.Function,
					Artifact.RawExecutionPayload))
			{
				return false;
			}
			const FAngelscriptCacheValidationResult DependencyResult =
				FAngelscriptCacheCompilerBridge::
					CaptureSuccessfulActualDependencies(
						*Compile.Function,
						DependencyResolver,
						Artifact.Body.ActualDependencies);
			if (!DependencyResult.IsSuccess())
			{
				Test.AddInfo(FString::Printf(
					TEXT("Invocation dependency capture failed: Kind=%s Owner=%s Declaration=%s Error=%u"),
					KindName(Compile.Kind), *Compile.OwnerName,
					*Compile.Declaration,
					static_cast<uint32>(DependencyResult.Error)));
				return false;
			}
			FAngelscriptFunctionArtifactCodec ExecutionCodec(
				*Module, *Module->engine);
			FAngelscriptHash256 ExecutionContentHash;
			const FAngelscriptCacheValidationResult EncodeResult =
				ExecutionCodec.EncodeExecutionArtifact(
					Artifact.RawExecutionPayload,
					Keys.ModuleKey,
					Artifact.Body.ActualDependencies,
					Artifact.Body.CanonicalExecutionPayload,
					ExecutionContentHash);
			if (!EncodeResult.IsSuccess())
			{
				Test.AddInfo(FString::Printf(
					TEXT("Invocation execution envelope failed: Kind=%s Owner=%s Declaration=%s Error=%u Offset=%llu Detail=%s"),
					KindName(Compile.Kind), *Compile.OwnerName,
					*Compile.Declaration,
					static_cast<uint32>(EncodeResult.Error),
					EncodeResult.ByteOffset,
					*ExecutionCodec.GetLastExecutionFailureDetail()));
				return false;
			}

			Artifact.Debug.PayloadSchemaVersion =
				FAngelscriptCacheRemainingRecordArchive::
					DebugSidecarPayloadSchemaVersion;
			Artifact.Debug.FunctionKey = Artifact.Body.Identity.FunctionKey;
			Artifact.Debug.Profile = Keys.Profile;
			Artifact.Debug.VmDebugCodecVersion =
				FAngelscriptFunctionArtifactCodec::DebugCodecVersion;
			FAngelscriptCachedDebugSourceReference& DebugSource =
				Artifact.Debug.Sources.AddDefaulted_GetRef();
			DebugSource.SourceFileKey = Keys.SourceFileKey;
			DebugSource.CanonicalLogicalSection = CanonicalSourceSection;
			if (!FAngelscriptCacheRemainingRecordArchive::TryBuildLogicalSectionKey(
					DebugSource.SourceFileKey,
					DebugSource.CanonicalLogicalSection,
					DebugSource.LogicalSectionKey).IsSuccess()
				|| !BuildDebugArtifact(
					*Compile.Function, Keys,
					Artifact.Debug.CanonicalDebugPayload))
			{
				return false;
			}
			Artifact.Body.Identity.Content.Execution = ExecutionContentHash;
			Artifact.Body.Identity.Content.Debug =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					{}, Artifact.Debug.CanonicalDebugPayload).Debug;
			Artifact.Debug.DebugHash = Artifact.Body.Identity.Content.Debug;
			Artifact.VmStateHash = BuildVmStateHash(
				*Compile.Function,
				Artifact.RawExecutionPayload,
				Artifact.Debug.CanonicalDebugPayload);
			Artifact.VmStateDescription = DescribeVmState(*Compile.Function);
		}

		return !OutArtifacts.IsEmpty()
			&& ExecuteBehavior(
				Producer,
				*Module,
				BehaviorObservation,
				OutRunValue,
				OutDestructorValue);
	}

	static int32 CountArtifactsOfKind(
		const TArray<FPersistedArtifact>& Artifacts,
		const asEBuildArtifactInvocationKind Kind)
	{
		int32 Count = 0;
		for (const FPersistedArtifact& Artifact : Artifacts)
		{
			if (Artifact.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheInvocationFamilyParityTests,
	"Angelscript.TestModule.Cache.InvocationFamilyParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ForcedCleanAndCachedArtifactsMatchAcrossTwoEngines)
	{
		using namespace AngelscriptCacheInvocationFamilyParityTests_Private;
		const FArtifactKeys Keys = MakeKeys();
		TArray<FPersistedArtifact> Artifacts;
		int32 ProducerRunValue = 0;
		int32 ProducerDestructorValue = 0;
		ASSERT_THAT(IsTrue(CaptureProducerArtifacts(
			*TestRunner,
			Keys,
			Artifacts,
			ProducerRunValue,
			ProducerDestructorValue)));
		ASSERT_THAT(AreEqual(16, ProducerRunValue));
		ASSERT_THAT(AreEqual(4, ProducerDestructorValue));

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
			const int32 Count = CountArtifactsOfKind(Artifacts, Kind);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Producer invocation family: Kind=%s Count=%d"),
				KindName(Kind),
				Count));
			ASSERT_THAT(IsTrue(Count > 0));
		}

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		asIScriptEngine* Engine = Consumer.GetEngine().GetScriptEngine();
		ASSERT_THAT(IsNotNull(Engine));
		FBehaviorObservation BehaviorObservation;
		ASSERT_THAT(IsTrue(RegisterBehaviorObservation(
			*Engine, BehaviorObservation)));
		ON_SCOPE_EXIT
		{
			Engine->SetUserData(nullptr, BehaviorObservationUserDataSlot);
		};
		asIScriptModule* Padding = Engine->GetModule(
			"ASCacheV2InvocationFamilyParityPadding", asGM_ALWAYS_CREATE);
		const char* PaddingSource = "int Padding() { return 1; }";
		ASSERT_THAT(IsNotNull(Padding));
		ASSERT_THAT(AreEqual(asSUCCESS, Padding->AddScriptSection(
			"Padding.as", PaddingSource,
			FCStringAnsi::Strlen(PaddingSource), 0)));
		ASSERT_THAT(AreEqual(asSUCCESS, Padding->Build()));

		asCModule* Module = static_cast<asCModule*>(Engine->GetModule(
			ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		FRestoreContext Restore;
		Restore.Artifacts = &Artifacts;
		FCompileLog ConsumerCompileLog;
		Module->SetBuildArtifactRestoreCallback(&RestoreArtifact, &Restore);
		ASSERT_THAT(IsTrue(BuildRawModule(*Module, ConsumerCompileLog)));
		for (const FString& Detail : Restore.FactoryForwardReferenceDetails)
		{
			TestRunner->AddInfo(Detail);
		}
		ASSERT_THAT(AreEqual(3, Restore.FactoryForwardReferenceChecks));
		ASSERT_THAT(AreEqual(0, Restore.FactoryForwardReferenceFailures));

		ASSERT_THAT(AreEqual(Artifacts.Num(), ConsumerCompileLog.Values.Num()));
		ASSERT_THAT(AreEqual(Artifacts.Num(), Restore.Values.Num()));
		for (const FRestoreObservation& Observation : Restore.Values)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Invocation restore before parity assertions: Kind=%s Owner=%s Declaration=%s Result=%u BeforeWords=%d AfterWords=%d Detail=%s"),
				KindName(Observation.Kind),
				*Observation.OwnerName,
				*Observation.Declaration,
				static_cast<uint32>(Observation.Result),
				Observation.ByteCodeWordsBefore,
				Observation.ByteCodeWordsAfter,
				*Observation.Detail));
		}
		for (const FObservedCompile& Compile : ConsumerCompileLog.Values)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Invocation compile before parity assertions: Kind=%s Namespace=%s Owner=%s Declaration=%s Result=%u CompilerInvoked=%d Succeeded=%d FunctionId=%d"),
				KindName(Compile.Kind),
				*Compile.Namespace,
				*Compile.OwnerName,
				*Compile.Declaration,
				static_cast<uint32>(Compile.RestoreResult),
				Compile.bCompilerInvoked ? 1 : 0,
				Compile.bSucceeded ? 1 : 0,
				Compile.Function != nullptr ? Compile.Function->id : -1));
		}
		for (const FObservedCompile& Compile : ConsumerCompileLog.Values)
		{
			const FPersistedArtifact* Artifact = Artifacts.FindByPredicate(
				[&Compile](const FPersistedArtifact& Candidate)
				{
					return Candidate.Kind == Compile.Kind
						&& Candidate.Namespace == Compile.Namespace
						&& Candidate.OwnerName == Compile.OwnerName
						&& Candidate.Declaration == Compile.Declaration;
				});
			ASSERT_THAT(IsNotNull(Artifact));
			ASSERT_THAT(AreEqual(
				asBUILD_ARTIFACT_RESTORE_RESTORED, Compile.RestoreResult));
			ASSERT_THAT(IsFalse(Compile.bCompilerInvoked));
			ASSERT_THAT(IsTrue(Compile.bSucceeded));
			ASSERT_THAT(IsNotNull(Compile.Function));
			ASSERT_THAT(IsTrue(
				Artifact->ProducerFunctionId != Compile.Function->id));

			TArray<uint8> CurrentRawExecution;
			TArray<uint8> CurrentExecutionEnvelope;
			TArray<uint8> CurrentDebug;
			ASSERT_THAT(IsTrue(WriteExecutionArtifact(
				*Module, *Compile.Function, CurrentRawExecution)));
			ASSERT_THAT(IsTrue(BuildDebugArtifact(
				*Compile.Function, Keys, CurrentDebug)));
			FAngelscriptFunctionArtifactCodec ExecutionCodec(
				*Module, *Module->engine);
			FAngelscriptHash256 CurrentEncodedExecutionHash;
			const FAngelscriptCacheValidationResult CurrentEncodeResult =
				ExecutionCodec.EncodeExecutionArtifact(
					CurrentRawExecution,
					Keys.ModuleKey,
					Artifact->Body.ActualDependencies,
					CurrentExecutionEnvelope,
					CurrentEncodedExecutionHash);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Invocation re-encode: Kind=%s Owner=%s Error=%u Offset=%llu Dependencies=%d RawBytes=%d EnvelopeBytes=%d Detail=%s"),
				KindName(Compile.Kind), *Compile.OwnerName,
				static_cast<uint32>(CurrentEncodeResult.Error),
				CurrentEncodeResult.ByteOffset,
				Artifact->Body.ActualDependencies.Num(),
				CurrentRawExecution.Num(), CurrentExecutionEnvelope.Num(),
				*ExecutionCodec.GetLastExecutionFailureDetail()));
			ASSERT_THAT(IsTrue(CurrentEncodeResult.IsSuccess()));
			const FAngelscriptFunctionContentHash CurrentContent =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					CurrentRawExecution, CurrentDebug);
			const FAngelscriptHash256 CurrentVmState = BuildVmStateHash(
				*Compile.Function, CurrentRawExecution, CurrentDebug);
			const FAngelscriptStableFunctionKey CurrentStableKey =
				BuildStableFunctionKey(Compile, Keys);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Invocation parity: Kind=%s Owner=%s ProducerId=%d ConsumerId=%d CompilerInvoked=%d RawExecutionEqual=%d EnvelopeEqual=%d DebugEqual=%d VmStateEqual=%d StableKeyEqual=%d"),
				KindName(Compile.Kind),
				*Compile.OwnerName,
				Artifact->ProducerFunctionId,
				Compile.Function->id,
				Compile.bCompilerInvoked ? 1 : 0,
				CurrentRawExecution == Artifact->RawExecutionPayload ? 1 : 0,
				CurrentExecutionEnvelope
					== Artifact->Body.CanonicalExecutionPayload ? 1 : 0,
				CurrentDebug == Artifact->Debug.CanonicalDebugPayload ? 1 : 0,
				CurrentVmState == Artifact->VmStateHash ? 1 : 0,
				CurrentStableKey == Artifact->Body.Identity.FunctionKey ? 1 : 0));
			if (!(CurrentVmState == Artifact->VmStateHash))
			{
				TestRunner->AddError(FString::Printf(
					TEXT("Invocation VM-state mismatch: Kind=%s Owner=%s Producer={%s} Consumer={%s}"),
					KindName(Compile.Kind),
					*Compile.OwnerName,
					*Artifact->VmStateDescription,
					*DescribeVmState(*Compile.Function)));
			}
			ASSERT_THAT(IsTrue(
				CurrentRawExecution == Artifact->RawExecutionPayload));
			ASSERT_THAT(IsTrue(
				CurrentExecutionEnvelope
					== Artifact->Body.CanonicalExecutionPayload));
			ASSERT_THAT(IsTrue(
				CurrentDebug == Artifact->Debug.CanonicalDebugPayload));
			ASSERT_THAT(IsTrue(
				CurrentEncodedExecutionHash == CurrentContent.Execution));
			ASSERT_THAT(IsTrue(CurrentContent.Execution
				== Artifact->Body.Identity.Content.Execution));
			ASSERT_THAT(IsTrue(CurrentContent.Debug
				== Artifact->Body.Identity.Content.Debug));
			ASSERT_THAT(IsTrue(CurrentVmState == Artifact->VmStateHash));
			ASSERT_THAT(IsTrue(
				CurrentStableKey == Artifact->Body.Identity.FunctionKey));
		}

		for (const FRestoreObservation& Observation : Restore.Values)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Invocation restore: Kind=%s Owner=%s Result=%u BeforeWords=%d AfterWords=%d Detail=%s"),
				KindName(Observation.Kind),
				*Observation.OwnerName,
				static_cast<uint32>(Observation.Result),
				Observation.ByteCodeWordsBefore,
				Observation.ByteCodeWordsAfter,
				*Observation.Detail));
			ASSERT_THAT(AreEqual(
				asBUILD_ARTIFACT_RESTORE_RESTORED, Observation.Result));
			ASSERT_THAT(AreEqual(0, Observation.ByteCodeWordsBefore));
			ASSERT_THAT(IsTrue(Observation.ByteCodeWordsAfter > 0));
		}

		int32 ConsumerRunValue = 0;
		int32 ConsumerDestructorValue = 0;
		ASSERT_THAT(IsTrue(ExecuteBehavior(
			Consumer,
			*Module,
			BehaviorObservation,
			ConsumerRunValue,
			ConsumerDestructorValue)));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Invocation observable parity: ProducerRun=%d ConsumerRun=%d ProducerDestructor=%d ConsumerDestructor=%d Artifacts=%d"),
			ProducerRunValue,
			ConsumerRunValue,
			ProducerDestructorValue,
			ConsumerDestructorValue,
			Artifacts.Num()));
		ASSERT_THAT(AreEqual(ProducerRunValue, ConsumerRunValue));
		ASSERT_THAT(AreEqual(
			ProducerDestructorValue, ConsumerDestructorValue));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
