#pragma once

#include "CoreMinimal.h"
#include "Hash/Blake3.h"

struct ANGELSCRIPTRUNTIME_API FAngelscriptHash256
{
	FBlake3Hash Value;

	bool IsZero() const;
	FString ToHexString() const;
	FGuid ToDisplayGuid() const;

	friend bool operator==(const FAngelscriptHash256& A, const FAngelscriptHash256& B)
	{
		return A.Value == B.Value;
	}

	friend bool operator<(const FAngelscriptHash256& A, const FAngelscriptHash256& B)
	{
		return A.Value < B.Value;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStableModuleKey
{
	FAngelscriptHash256 Hash;

	friend bool operator==(const FAngelscriptStableModuleKey& A, const FAngelscriptStableModuleKey& B)
	{
		return A.Hash == B.Hash;
	}
	friend bool operator!=(const FAngelscriptStableModuleKey& A, const FAngelscriptStableModuleKey& B)
	{
		return !(A == B);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStableTypeKey
{
	FAngelscriptHash256 Hash;

	friend bool operator==(const FAngelscriptStableTypeKey& A, const FAngelscriptStableTypeKey& B)
	{
		return A.Hash == B.Hash;
	}
	friend bool operator!=(const FAngelscriptStableTypeKey& A, const FAngelscriptStableTypeKey& B)
	{
		return !(A == B);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStableFunctionKey
{
	FAngelscriptHash256 Hash;

	friend bool operator==(const FAngelscriptStableFunctionKey& A, const FAngelscriptStableFunctionKey& B)
	{
		return A.Hash == B.Hash;
	}
	friend bool operator!=(const FAngelscriptStableFunctionKey& A, const FAngelscriptStableFunctionKey& B)
	{
		return !(A == B);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStableGlobalKey
{
	FAngelscriptHash256 Hash;

	friend bool operator==(const FAngelscriptStableGlobalKey& A, const FAngelscriptStableGlobalKey& B)
	{
		return A.Hash == B.Hash;
	}
	friend bool operator!=(const FAngelscriptStableGlobalKey& A, const FAngelscriptStableGlobalKey& B)
	{
		return !(A == B);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStablePropertyKey
{
	FAngelscriptHash256 Hash;

	friend bool operator==(const FAngelscriptStablePropertyKey& A, const FAngelscriptStablePropertyKey& B)
	{
		return A.Hash == B.Hash;
	}
	friend bool operator!=(const FAngelscriptStablePropertyKey& A, const FAngelscriptStablePropertyKey& B)
	{
		return !(A == B);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionSourceDigest
{
	FAngelscriptHash256 Hash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionInputDigest
{
	FAngelscriptHash256 Hash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionContentHash
{
	FAngelscriptHash256 Execution;
	FAngelscriptHash256 Debug;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompatibilityKey
{
	FAngelscriptHash256 Hash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheContextKey
{
	FAngelscriptHash256 Hash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptArtifactProfileKey
{
	FAngelscriptHash256 Hash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionArtifactIdentity
{
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptFunctionContentHash Content;
	FAngelscriptArtifactProfileKey Profile;
};

enum class EAngelscriptArtifactEntityKind : uint8
{
	Class = 1,
	Struct = 2,
	Interface = 3,
	Enum = 4,
	Delegate = 5,
	Typedef = 6,
	Funcdef = 7,
	GlobalVariable = 16,
	Property = 17,
	GlobalFunction = 32,
	Method = 33,
	Constructor = 34,
	Destructor = 35,
	Factory = 36,
	DelegateSignature = 37,
	ModuleInitializer = 38,
	GlobalInitializer = 39,
	GeneratedDefaultConstructor = 40,
	InitDefaults = 41,
	GeneratedDefaultDestructor = 42,
};

enum class EAngelscriptFunctionOwnerKind : uint8
{
	Module = 1,
	Type = 2,
	Global = 3,
	Property = 4,
};

class FAngelscriptArtifactIdentityBuilder;

class ANGELSCRIPTRUNTIME_API FAngelscriptLogicalVirtualPath
{
public:
	FAngelscriptLogicalVirtualPath(const FAngelscriptLogicalVirtualPath&) = default;
	FAngelscriptLogicalVirtualPath(FAngelscriptLogicalVirtualPath&&) = default;
	FAngelscriptLogicalVirtualPath& operator=(const FAngelscriptLogicalVirtualPath&) = default;
	FAngelscriptLogicalVirtualPath& operator=(FAngelscriptLogicalVirtualPath&&) = default;

	const FString& GetCanonicalPath() const;

private:
	explicit FAngelscriptLogicalVirtualPath(FString&& InCanonicalPath);

	FString CanonicalPath;

	friend class FAngelscriptArtifactIdentityBuilder;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptModuleIdentityDescriptor
{
	FAngelscriptModuleIdentityDescriptor(
		FStringView InLogicalMount,
		const FAngelscriptLogicalVirtualPath& InVirtualPath,
		FStringView InModuleName);

	FString LogicalMount;
	FAngelscriptLogicalVirtualPath VirtualPath;
	FString ModuleName;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypeIdentityDescriptor
{
	FAngelscriptStableModuleKey ModuleKey;
	FString Namespace;
	EAngelscriptArtifactEntityKind Kind = EAngelscriptArtifactEntityKind::Class;
	FString CanonicalDeclaration;
	TArray<FString> CanonicalTraits;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionIdentityDescriptor
{
	EAngelscriptFunctionOwnerKind OwnerKind = EAngelscriptFunctionOwnerKind::Module;
	FAngelscriptHash256 OwnerKey;
	FString Namespace;
	EAngelscriptArtifactEntityKind Kind = EAngelscriptArtifactEntityKind::GlobalFunction;
	FString CanonicalDeclaration;
	TArray<FString> CanonicalTraits;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptGlobalIdentityDescriptor
{
	FAngelscriptStableModuleKey ModuleKey;
	FString Namespace;
	EAngelscriptArtifactEntityKind Kind = EAngelscriptArtifactEntityKind::GlobalVariable;
	FString Name;
	FString CanonicalType;
	TArray<FString> CanonicalTraits;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptPropertyIdentityDescriptor
{
	FAngelscriptStableTypeKey OwnerTypeKey;
	EAngelscriptArtifactEntityKind Kind = EAngelscriptArtifactEntityKind::Property;
	FString Name;
	FString CanonicalType;
	TArray<FString> CanonicalTraits;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionSourceDescriptor
{
	EAngelscriptArtifactEntityKind Kind = EAngelscriptArtifactEntityKind::GlobalFunction;
	FString CanonicalSource;
	TArray<FString> CanonicalOptions;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionInputDescriptor
{
	FAngelscriptFunctionSourceDigest SourceDigest;
	TArray<FAngelscriptHash256> OrderedDependencyFingerprints;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCompatibilityDescriptor
{
	TArray<FString> CanonicalInputs;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptContextDescriptor
{
	TArray<FString> CanonicalInputs;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptArtifactCanonicalWriter
{
public:
	static constexpr uint32 IdentitySchemaVersion = 1;

	explicit FAngelscriptArtifactCanonicalWriter(FStringView Domain);

	void WriteUInt8(uint8 Value);
	void WriteUInt16(uint16 Value);
	void WriteUInt32(uint32 Value);
	void WriteUInt64(uint64 Value);
	void WriteBool(bool bValue);
	void WriteHash(const FAngelscriptHash256& Value);
	void WriteBytes(TConstArrayView<uint8> Value);
	void WriteString(FStringView Value);
	static int32 CompareCanonicalUtf8Strings(FStringView A, FStringView B);

	TConstArrayView<uint8> GetBytes() const;
	FAngelscriptHash256 FinalizeHash() const;

private:
	TArray<uint8> Bytes;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptArtifactIdentityBuilder
{
public:
	static TOptional<FAngelscriptLogicalVirtualPath> TryCreateLogicalVirtualPath(FStringView VirtualPath);
	static TOptional<FAngelscriptStableModuleKey> TryBuildModuleKey(
		FStringView LogicalMount,
		FStringView VirtualPath,
		FStringView ModuleName);

	static FAngelscriptStableModuleKey BuildModuleKey(const FAngelscriptModuleIdentityDescriptor& Descriptor);
	static FAngelscriptStableTypeKey BuildTypeKey(const FAngelscriptTypeIdentityDescriptor& Descriptor);
	static FAngelscriptStableFunctionKey BuildFunctionKey(const FAngelscriptFunctionIdentityDescriptor& Descriptor);
	static FAngelscriptStableGlobalKey BuildGlobalKey(const FAngelscriptGlobalIdentityDescriptor& Descriptor);
	static FAngelscriptStablePropertyKey BuildPropertyKey(const FAngelscriptPropertyIdentityDescriptor& Descriptor);

	static FAngelscriptFunctionSourceDigest BuildFunctionSourceDigest(
		const FAngelscriptFunctionSourceDescriptor& Descriptor);
	static FAngelscriptFunctionInputDigest BuildFunctionInputDigest(
		const FAngelscriptFunctionInputDescriptor& Descriptor);
	static FAngelscriptFunctionContentHash BuildFunctionContentHash(
		TConstArrayView<uint8> ExecutionPayload,
		TConstArrayView<uint8> DebugPayload);
	static FAngelscriptHash256 BuildFunctionDebugAbsentHash(
		const FAngelscriptArtifactProfileKey& Profile);

	static FAngelscriptCacheCompatibilityKey BuildCompatibilityKey(
		const FAngelscriptCompatibilityDescriptor& Descriptor);
	static FAngelscriptCacheContextKey BuildContextKey(const FAngelscriptContextDescriptor& Descriptor);
	static FAngelscriptArtifactProfileKey BuildArtifactProfileKey(
		const FAngelscriptCacheCompatibilityKey& Compatibility,
		const FAngelscriptCacheContextKey& Context);
};
