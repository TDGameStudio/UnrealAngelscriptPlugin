#include "Artifacts/AngelscriptArtifactIdentity.h"

#include "Containers/StringConv.h"

namespace AngelscriptArtifactIdentity_Private
{
	static uint32 ReadUInt32LittleEndian(const uint8* Bytes)
	{
		return static_cast<uint32>(Bytes[0])
			| (static_cast<uint32>(Bytes[1]) << 8)
			| (static_cast<uint32>(Bytes[2]) << 16)
			| (static_cast<uint32>(Bytes[3]) << 24);
	}

	static void WriteSortedStrings(
		FAngelscriptArtifactCanonicalWriter& Writer,
		const TArray<FString>& Values)
	{
		TArray<FString> SortedValues = Values;
		SortedValues.Sort([](const FString& A, const FString& B)
		{
			return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(A, B) < 0;
		});

		Writer.WriteUInt32(static_cast<uint32>(SortedValues.Num()));
		for (const FString& Value : SortedValues)
		{
			Writer.WriteString(Value);
		}
	}

	static FAngelscriptHash256 HashPayload(FStringView Domain, TConstArrayView<uint8> Payload)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteBytes(Payload);
		return Writer.FinalizeHash();
	}
}

bool FAngelscriptHash256::IsZero() const
{
	return Value.IsZero();
}

FString FAngelscriptHash256::ToHexString() const
{
	return LexToString(Value);
}

FGuid FAngelscriptHash256::ToDisplayGuid() const
{
	const uint8* Bytes = Value.GetBytes();
	return FGuid(
		AngelscriptArtifactIdentity_Private::ReadUInt32LittleEndian(Bytes),
		AngelscriptArtifactIdentity_Private::ReadUInt32LittleEndian(Bytes + 4),
		AngelscriptArtifactIdentity_Private::ReadUInt32LittleEndian(Bytes + 8),
		AngelscriptArtifactIdentity_Private::ReadUInt32LittleEndian(Bytes + 12));
}

FAngelscriptArtifactCanonicalWriter::FAngelscriptArtifactCanonicalWriter(const FStringView Domain)
{
	static constexpr uint8 Prefix[] = {
		'U', 'E', 'A', 'S', '-', 'A', 'R', 'T', 'I', 'F', 'A', 'C', 'T'};
	Bytes.Append(Prefix, UE_ARRAY_COUNT(Prefix));
	Bytes.Add(0);
	WriteUInt32(IdentitySchemaVersion);
	WriteString(Domain);
}

void FAngelscriptArtifactCanonicalWriter::WriteUInt8(const uint8 Value)
{
	Bytes.Add(Value);
}

void FAngelscriptArtifactCanonicalWriter::WriteUInt16(const uint16 Value)
{
	Bytes.Add(static_cast<uint8>(Value));
	Bytes.Add(static_cast<uint8>(Value >> 8));
}

void FAngelscriptArtifactCanonicalWriter::WriteUInt32(const uint32 Value)
{
	Bytes.Add(static_cast<uint8>(Value));
	Bytes.Add(static_cast<uint8>(Value >> 8));
	Bytes.Add(static_cast<uint8>(Value >> 16));
	Bytes.Add(static_cast<uint8>(Value >> 24));
}

void FAngelscriptArtifactCanonicalWriter::WriteUInt64(const uint64 Value)
{
	for (uint32 Shift = 0; Shift < 64; Shift += 8)
	{
		Bytes.Add(static_cast<uint8>(Value >> Shift));
	}
}

void FAngelscriptArtifactCanonicalWriter::WriteBool(const bool bValue)
{
	WriteUInt8(bValue ? 1 : 0);
}

void FAngelscriptArtifactCanonicalWriter::WriteHash(const FAngelscriptHash256& Value)
{
	Bytes.Append(Value.Value.GetBytes(), sizeof(FBlake3Hash::ByteArray));
}

void FAngelscriptArtifactCanonicalWriter::WriteBytes(const TConstArrayView<uint8> Value)
{
	checkf(Value.Num() >= 0, TEXT("Canonical byte array length must be non-negative"));
	WriteUInt32(static_cast<uint32>(Value.Num()));
	Bytes.Append(Value.GetData(), Value.Num());
}

void FAngelscriptArtifactCanonicalWriter::WriteString(const FStringView Value)
{
	const FTCHARToUTF8 Utf8(Value.GetData(), Value.Len());
	WriteUInt32(static_cast<uint32>(Utf8.Length()));
	Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

int32 FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
	const FStringView A,
	const FStringView B)
{
	const FTCHARToUTF8 AUtf8(A.GetData(), A.Len());
	const FTCHARToUTF8 BUtf8(B.GetData(), B.Len());
	const int32 CommonLength = FMath::Min(AUtf8.Length(), BUtf8.Length());
	if (CommonLength > 0)
	{
		const int32 ByteCompare = FMemory::Memcmp(AUtf8.Get(), BUtf8.Get(), CommonLength);
		if (ByteCompare != 0)
		{
			return ByteCompare < 0 ? -1 : 1;
		}
	}
	if (AUtf8.Length() == BUtf8.Length())
	{
		return 0;
	}
	return AUtf8.Length() < BUtf8.Length() ? -1 : 1;
}

TConstArrayView<uint8> FAngelscriptArtifactCanonicalWriter::GetBytes() const
{
	return Bytes;
}

FAngelscriptHash256 FAngelscriptArtifactCanonicalWriter::FinalizeHash() const
{
	return FAngelscriptHash256{FBlake3::HashBuffer(Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
}

FAngelscriptLogicalVirtualPath::FAngelscriptLogicalVirtualPath(FString&& InCanonicalPath)
	: CanonicalPath(MoveTemp(InCanonicalPath))
{
}

const FString& FAngelscriptLogicalVirtualPath::GetCanonicalPath() const
{
	return CanonicalPath;
}

FAngelscriptModuleIdentityDescriptor::FAngelscriptModuleIdentityDescriptor(
	const FStringView InLogicalMount,
	const FAngelscriptLogicalVirtualPath& InVirtualPath,
	const FStringView InModuleName)
	: LogicalMount(InLogicalMount)
	, VirtualPath(InVirtualPath)
	, ModuleName(InModuleName)
{
}

TOptional<FAngelscriptLogicalVirtualPath> FAngelscriptArtifactIdentityBuilder::TryCreateLogicalVirtualPath(
	const FStringView VirtualPath)
{
	FString Normalized(VirtualPath);
	Normalized.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	if (Normalized.IsEmpty()
		|| Normalized.StartsWith(TEXT("/"), ESearchCase::CaseSensitive)
		|| (Normalized.Len() >= 2 && Normalized[1] == TEXT(':')))
	{
		return {};
	}

	TArray<FString> RawSegments;
	Normalized.ParseIntoArray(RawSegments, TEXT("/"), true);
	TArray<FString> CanonicalSegments;
	for (FString& Segment : RawSegments)
	{
		if (Segment == TEXT("."))
		{
			continue;
		}

		if (Segment == TEXT(".."))
		{
			if (!CanonicalSegments.IsEmpty() && CanonicalSegments.Last() != TEXT(".."))
			{
				CanonicalSegments.Pop(EAllowShrinking::No);
			}
			else
			{
				return {};
			}
			continue;
		}

		CanonicalSegments.Add(MoveTemp(Segment));
	}

	FString Result = FString::Join(CanonicalSegments, TEXT("/"));
	if (Result.IsEmpty())
	{
		return {};
	}

	FAngelscriptLogicalVirtualPath LogicalPath(MoveTemp(Result));
	return TOptional<FAngelscriptLogicalVirtualPath>(MoveTemp(LogicalPath));
}

TOptional<FAngelscriptStableModuleKey> FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
	const FStringView LogicalMount,
	const FStringView VirtualPath,
	const FStringView ModuleName)
{
	TOptional<FAngelscriptLogicalVirtualPath> LogicalPath = TryCreateLogicalVirtualPath(VirtualPath);
	if (!LogicalPath.IsSet())
	{
		return {};
	}

	return BuildModuleKey(FAngelscriptModuleIdentityDescriptor(
		LogicalMount,
		LogicalPath.GetValue(),
		ModuleName));
}

FAngelscriptStableModuleKey FAngelscriptArtifactIdentityBuilder::BuildModuleKey(
	const FAngelscriptModuleIdentityDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("module"));
	Writer.WriteString(Descriptor.LogicalMount);
	Writer.WriteString(Descriptor.VirtualPath.GetCanonicalPath());
	Writer.WriteString(Descriptor.ModuleName);
	return FAngelscriptStableModuleKey{Writer.FinalizeHash()};
}

FAngelscriptStableTypeKey FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
	const FAngelscriptTypeIdentityDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("type"));
	Writer.WriteHash(Descriptor.ModuleKey.Hash);
	Writer.WriteString(Descriptor.Namespace);
	Writer.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
	Writer.WriteString(Descriptor.CanonicalDeclaration);
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalTraits);
	return FAngelscriptStableTypeKey{Writer.FinalizeHash()};
}

FAngelscriptStableFunctionKey FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(
	const FAngelscriptFunctionIdentityDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("function"));
	Writer.WriteUInt8(static_cast<uint8>(Descriptor.OwnerKind));
	Writer.WriteHash(Descriptor.OwnerKey);
	Writer.WriteString(Descriptor.Namespace);
	Writer.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
	Writer.WriteString(Descriptor.CanonicalDeclaration);
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalTraits);
	return FAngelscriptStableFunctionKey{Writer.FinalizeHash()};
}

FAngelscriptStableGlobalKey FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(
	const FAngelscriptGlobalIdentityDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("global"));
	Writer.WriteHash(Descriptor.ModuleKey.Hash);
	Writer.WriteString(Descriptor.Namespace);
	Writer.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
	Writer.WriteString(Descriptor.Name);
	Writer.WriteString(Descriptor.CanonicalType);
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalTraits);
	return FAngelscriptStableGlobalKey{Writer.FinalizeHash()};
}

FAngelscriptStablePropertyKey FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(
	const FAngelscriptPropertyIdentityDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("property"));
	Writer.WriteHash(Descriptor.OwnerTypeKey.Hash);
	Writer.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
	Writer.WriteString(Descriptor.Name);
	Writer.WriteString(Descriptor.CanonicalType);
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalTraits);
	return FAngelscriptStablePropertyKey{Writer.FinalizeHash()};
}

FAngelscriptFunctionSourceDigest FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(
	const FAngelscriptFunctionSourceDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("function-source"));
	Writer.WriteUInt8(static_cast<uint8>(Descriptor.Kind));
	Writer.WriteString(Descriptor.CanonicalSource);
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalOptions);
	return FAngelscriptFunctionSourceDigest{Writer.FinalizeHash()};
}

FAngelscriptFunctionInputDigest FAngelscriptArtifactIdentityBuilder::BuildFunctionInputDigest(
	const FAngelscriptFunctionInputDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("function-input"));
	Writer.WriteHash(Descriptor.SourceDigest.Hash);
	Writer.WriteUInt32(static_cast<uint32>(Descriptor.OrderedDependencyFingerprints.Num()));
	for (const FAngelscriptHash256& DependencyFingerprint : Descriptor.OrderedDependencyFingerprints)
	{
		Writer.WriteHash(DependencyFingerprint);
	}
	return FAngelscriptFunctionInputDigest{Writer.FinalizeHash()};
}

FAngelscriptFunctionContentHash FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
	const TConstArrayView<uint8> ExecutionPayload,
	const TConstArrayView<uint8> DebugPayload)
{
	return FAngelscriptFunctionContentHash{
		AngelscriptArtifactIdentity_Private::HashPayload(TEXT("function-execution"), ExecutionPayload),
		AngelscriptArtifactIdentity_Private::HashPayload(TEXT("function-debug"), DebugPayload)};
}

FAngelscriptHash256 FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(
	const FAngelscriptArtifactProfileKey& Profile)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("function-debug-absent"));
	Writer.WriteHash(Profile.Hash);
	return Writer.FinalizeHash();
}

FAngelscriptCacheCompatibilityKey FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
	const FAngelscriptCompatibilityDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("compatibility"));
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalInputs);
	return FAngelscriptCacheCompatibilityKey{Writer.FinalizeHash()};
}

FAngelscriptCacheContextKey FAngelscriptArtifactIdentityBuilder::BuildContextKey(
	const FAngelscriptContextDescriptor& Descriptor)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("context"));
	AngelscriptArtifactIdentity_Private::WriteSortedStrings(Writer, Descriptor.CanonicalInputs);
	return FAngelscriptCacheContextKey{Writer.FinalizeHash()};
}

FAngelscriptArtifactProfileKey FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
	const FAngelscriptCacheCompatibilityKey& Compatibility,
	const FAngelscriptCacheContextKey& Context)
{
	FAngelscriptArtifactCanonicalWriter Writer(TEXT("profile"));
	Writer.WriteHash(Compatibility.Hash);
	Writer.WriteHash(Context.Hash);
	return FAngelscriptArtifactProfileKey{Writer.FinalizeHash()};
}
