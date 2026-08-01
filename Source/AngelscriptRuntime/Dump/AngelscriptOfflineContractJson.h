#pragma once

#include "CoreMinimal.h"

namespace AngelscriptOfflineContract
{
	class ANGELSCRIPTRUNTIME_API FCanonicalJsonValue
	{
	public:
		enum class EKind : uint8
		{
			Null,
			Boolean,
			Integer,
			Number,
			String,
			Array,
			Object,
		};

		static FCanonicalJsonValue Null();
		static FCanonicalJsonValue Boolean(bool Value);
		static FCanonicalJsonValue Integer(int64 Value);
		static FCanonicalJsonValue Number(double Value);
		static FCanonicalJsonValue String(FString Value);
		static FCanonicalJsonValue Array();
		static FCanonicalJsonValue Object();

		EKind GetKind() const;
		void Add(FCanonicalJsonValue Value);
		void Set(FString Key, FCanonicalJsonValue Value);

	private:
		EKind Kind = EKind::Null;
		bool BooleanValue = false;
		int64 IntegerValue = 0;
		double NumberValue = 0.0;
		FString StringValue;
		TArray<TSharedPtr<FCanonicalJsonValue>> ArrayValue;
		TMap<FString, TSharedPtr<FCanonicalJsonValue>> ObjectValue;

		friend ANGELSCRIPTRUNTIME_API FString SerializeCanonicalJson(
			const FCanonicalJsonValue& Value);
	};

	struct FCanonicalJsonLine
	{
		FString StableId;
		FCanonicalJsonValue Payload;
	};

	ANGELSCRIPTRUNTIME_API FString EscapeCanonicalJsonString(FStringView Value);
	ANGELSCRIPTRUNTIME_API FString SerializeCanonicalJson(
		const FCanonicalJsonValue& Value);
	ANGELSCRIPTRUNTIME_API TArray<uint8> SerializeCanonicalJsonDocument(
		const FCanonicalJsonValue& Value);
	ANGELSCRIPTRUNTIME_API TArray<uint8> SerializeCanonicalJsonLines(
		const TArray<FCanonicalJsonLine>& Lines);
}
