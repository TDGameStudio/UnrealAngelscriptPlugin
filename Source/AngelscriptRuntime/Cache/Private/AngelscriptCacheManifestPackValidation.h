#pragma once

#include "Cache/AngelscriptCacheManifestPack.h"

namespace AngelscriptCacheManifestPack_Private
{
	// Move-only internal transaction that preserves one decoded Manifest and its
	// temporary Budget reservations between locked Pack discovery and validation
	// through already pinned immutable handles.
	class FPreparedGenerationValidation final
	{
	public:
		FPreparedGenerationValidation();
		~FPreparedGenerationValidation();

		FPreparedGenerationValidation(
			const FPreparedGenerationValidation&) = delete;
		FPreparedGenerationValidation& operator=(
			const FPreparedGenerationValidation&) = delete;
		FPreparedGenerationValidation(
			FPreparedGenerationValidation&& Other) noexcept;
		FPreparedGenerationValidation& operator=(
			FPreparedGenerationValidation&& Other) noexcept;

		bool IsPrepared() const;
		const FAngelscriptCacheGenerationManifest& GetManifest() const;
		TConstArrayView<FAngelscriptHash256> GetDistinctPackIds() const;

	private:
		struct FState;
		TUniquePtr<FState> State;

		friend FAngelscriptCacheValidationResult PrepareGenerationValidation(
			TConstArrayView<uint8>,
			const FAngelscriptHash256&,
			const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&,
			FPreparedGenerationValidation&);
		friend FAngelscriptCacheValidationResult CompleteGenerationValidation(
			FPreparedGenerationValidation&&,
			IAngelscriptCachePackSource&,
			IAngelscriptCacheStorageCodec&,
			TOptional<FAngelscriptValidatedGeneration>&);
	};

	FAngelscriptCacheValidationResult PrepareGenerationValidation(
		TConstArrayView<uint8> CompleteManifestBytes,
		const FAngelscriptHash256& ExpectedGenerationId,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FPreparedGenerationValidation& OutPrepared);

	FAngelscriptCacheValidationResult CompleteGenerationValidation(
		FPreparedGenerationValidation&& Prepared,
		IAngelscriptCachePackSource& Packs,
		IAngelscriptCacheStorageCodec& Codec,
		TOptional<FAngelscriptValidatedGeneration>& OutGeneration);
}
