#pragma once

#include "AngelscriptOfflineContractTypes.h"

namespace AngelscriptOfflineContract
{
	struct FSymbolIdentityInput
	{
		ESymbolKind Kind = ESymbolKind::Type;
		FString Namespace;
		FString OwnerStableId;
		FString CompleteDeclaration;
	};

	ANGELSCRIPTRUNTIME_API FString NormalizeNamespace(FStringView Value);
	ANGELSCRIPTRUNTIME_API FString NormalizeDeclaration(FStringView Value);
	ANGELSCRIPTRUNTIME_API FString NormalizeSemanticPath(FStringView Value);
	ANGELSCRIPTRUNTIME_API FString MakeCanonicalSymbolIdentity(
		const FSymbolIdentityInput& Input);
	ANGELSCRIPTRUNTIME_API FString MakeStableSymbolId(
		const FSymbolIdentityInput& Input);
	ANGELSCRIPTRUNTIME_API FString MakeStableModuleId(
		FStringView LogicalModuleName,
		FStringView VirtualSourceIdentity);
	ANGELSCRIPTRUNTIME_API FString MakeStableAdapterId(
		FStringView AdapterName,
		FStringView AdapterVersion);
	ANGELSCRIPTRUNTIME_API FString MakeStableAssetId(
		FStringView NormalizedObjectPath);
	ANGELSCRIPTRUNTIME_API FString Sha256Utf8(FStringView Value);
	ANGELSCRIPTRUNTIME_API FString Sha256Bytes(
		const TArray<uint8>& Value);
}
