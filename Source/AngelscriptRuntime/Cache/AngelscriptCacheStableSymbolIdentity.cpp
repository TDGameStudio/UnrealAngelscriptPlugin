#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptSource.h"

#include "as_module.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"

namespace AngelscriptCacheStableSymbolIdentity_Private
{
	static TOptional<EAngelscriptArtifactEntityKind> MapInvocationKind(
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

	static void SetFailure(FString* OutFailure, const TCHAR* Failure)
	{
		if (OutFailure != nullptr)
		{
			*OutFailure = Failure;
		}
	}
}

bool FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
	const FAngelscriptModuleDesc& Module,
	FAngelscriptStableModuleKey& OutKey,
	FString* OutFailure)
{
	using namespace AngelscriptCacheStableSymbolIdentity_Private;
	OutKey = {};
	if (OutFailure != nullptr)
	{
		OutFailure->Reset();
	}
	if (Module.ModuleName.IsEmpty() || Module.Code.Num() != 1
		|| Module.Code[0].VirtualPath.IsEmpty())
	{
		SetFailure(OutFailure,
			TEXT("Module has no unique logical source coordinate"));
		return false;
	}

	FAngelscriptVirtualPath VirtualPath;
	FString PathFailure;
	if (!FAngelscriptVirtualPath::TryParse(
		Module.Code[0].VirtualPath, VirtualPath, &PathFailure))
	{
		if (OutFailure != nullptr)
		{
			*OutFailure = FString::Printf(
				TEXT("Module virtual path is invalid: %s"), *PathFailure);
		}
		return false;
	}

	FString LogicalMount;
	switch (VirtualPath.GetSourceKind())
	{
	case EAngelscriptSourceKind::Game:
		LogicalMount = FAngelscriptVirtualPath::GameRoot;
		break;
	case EAngelscriptSourceKind::Plugin:
		if (!VirtualPath.GetMountName().IsEmpty())
		{
			LogicalMount = FString(FAngelscriptVirtualPath::PluginRoot)
				/ VirtualPath.GetMountName();
		}
		break;
	case EAngelscriptSourceKind::Memory:
		if (!VirtualPath.GetMountName().IsEmpty())
		{
			LogicalMount = FString(FAngelscriptVirtualPath::MemoryRoot)
				/ VirtualPath.GetMountName();
		}
		break;
	default:
		break;
	}
	if (LogicalMount.IsEmpty())
	{
		SetFailure(OutFailure,
			TEXT("Module source has no stable logical mount"));
		return false;
	}

	const TOptional<FAngelscriptStableModuleKey> ModuleKey =
		FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
			LogicalMount,
			VirtualPath.GetRelativePath(),
			Module.ModuleName);
	if (!ModuleKey.IsSet() || ModuleKey->Hash.IsZero())
	{
		SetFailure(OutFailure,
			TEXT("Module coordinate could not be canonicalized"));
		return false;
	}
	OutKey = ModuleKey.GetValue();
	return true;
}

bool FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
	const FAngelscriptStableModuleKey& ModuleKey,
	const asCTypeInfo& Type,
	FAngelscriptStableTypeKey& OutKey)
{
	OutKey = {};
	if (ModuleKey.Hash.IsZero() || Type.module == nullptr
		|| Type.GetName() == nullptr || Type.GetName()[0] == '\0')
	{
		return false;
	}

	FAngelscriptTypeIdentityDescriptor Identity;
	Identity.ModuleKey = ModuleKey;
	Identity.Namespace = UTF8_TO_TCHAR(Type.GetNamespace());
	const FString Name = UTF8_TO_TCHAR(Type.GetName());
	if ((Type.flags & asOBJ_ENUM) != 0)
	{
		Identity.Kind = EAngelscriptArtifactEntityKind::Enum;
		Identity.CanonicalDeclaration = FString::Printf(TEXT("enum %s"), *Name);
	}
	else if (const asCObjectType* ObjectType = CastToObjectType(
		const_cast<asCTypeInfo*>(&Type));
		ObjectType != nullptr && ObjectType->IsInterface())
	{
		Identity.Kind = EAngelscriptArtifactEntityKind::Interface;
		Identity.CanonicalDeclaration = FString::Printf(
			TEXT("interface %s"), *Name);
	}
	else if ((Type.flags & asOBJ_VALUE) != 0)
	{
		Identity.Kind = EAngelscriptArtifactEntityKind::Struct;
		Identity.CanonicalDeclaration = FString::Printf(TEXT("struct %s"), *Name);
	}
	else
	{
		Identity.Kind = EAngelscriptArtifactEntityKind::Class;
		Identity.CanonicalDeclaration = FString::Printf(TEXT("class %s"), *Name);
	}
	OutKey = FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Identity);
	return !OutKey.Hash.IsZero();
}

bool FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
	const FAngelscriptStableModuleKey& ModuleKey,
	const asCScriptFunction& Function,
	FAngelscriptStableFunctionKey& OutKey,
	FString* OutFailure)
{
	using namespace AngelscriptCacheStableSymbolIdentity_Private;
	OutKey = {};
	if (OutFailure != nullptr)
	{
		OutFailure->Reset();
	}
	if (ModuleKey.Hash.IsZero() || Function.module == nullptr
		|| Function.funcType != asFUNC_SCRIPT)
	{
		SetFailure(OutFailure,
			TEXT("Function has no cacheable local script/module identity"));
		return false;
	}

	const TOptional<EAngelscriptArtifactEntityKind> EntityKind =
		MapInvocationKind(Function.artifactInvocationKind);
	if (!EntityKind.IsSet())
	{
		SetFailure(OutFailure,
			TEXT("Function has no stable cacheable invocation kind"));
		return false;
	}

	FAngelscriptFunctionIdentityDescriptor Identity;
	Identity.Namespace = UTF8_TO_TCHAR(Function.GetNamespace());
	Identity.Kind = EntityKind.GetValue();
	Identity.CanonicalDeclaration = UTF8_TO_TCHAR(
		Function.GetDeclaration(false, false, false));
	if (Identity.CanonicalDeclaration.IsEmpty())
	{
		SetFailure(OutFailure,
			TEXT("Function canonical declaration is empty"));
		return false;
	}

	if (Function.artifactInvocationKind
		== asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION)
	{
		if (Function.artifactOwnerType != nullptr
			|| Function.objectType != nullptr)
		{
			SetFailure(OutFailure,
				TEXT("Global function unexpectedly has a type owner"));
			return false;
		}
		Identity.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Identity.OwnerKey = ModuleKey.Hash;
	}
	else
	{
		const asCTypeInfo* OwnerType = Function.artifactOwnerType;
		FAngelscriptStableTypeKey OwnerKey;
		if (OwnerType == nullptr || OwnerType->module != Function.module
			|| !TryBuildLocalTypeKey(ModuleKey, *OwnerType, OwnerKey))
		{
			SetFailure(OutFailure,
				TEXT("Function type owner cannot resolve to one local stable TypeKey"));
			return false;
		}

		bool bSemanticOwnerMatches = false;
		if (Function.artifactInvocationKind
			== asBUILD_ARTIFACT_INVOCATION_FACTORY)
		{
			// Script factories are global-shaped functions owned by the type they
			// construct. Depending on the maintained-fork declaration path they
			// may retain that owner as objectType or solely in the return type, but
			// neither representation may authorize an unrelated builder owner.
			const asCTypeInfo* ReturnType = Function.returnType.GetTypeInfo();
			bSemanticOwnerMatches = Function.objectType != nullptr
				? Function.objectType == OwnerType
				: ReturnType == OwnerType;
		}
		else
		{
			bSemanticOwnerMatches = Function.objectType == OwnerType;
		}
		if (!bSemanticOwnerMatches)
		{
			SetFailure(OutFailure,
				TEXT("Function semantic owner does not match its artifact type owner"));
			return false;
		}
		Identity.OwnerKind = EAngelscriptFunctionOwnerKind::Type;
		Identity.OwnerKey = OwnerKey.Hash;
	}

	OutKey = FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity);
	return !OutKey.Hash.IsZero();
}
