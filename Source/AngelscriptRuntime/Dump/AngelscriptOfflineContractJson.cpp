#include "AngelscriptOfflineContractJson.h"

#include "Math/UnrealMathUtility.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		void AppendUtf8(const FStringView Value, TArray<uint8>& Output)
		{
			const FString Owned(Value);
			const FTCHARToUTF8 Utf8(*Owned, Owned.Len());
			Output.Append(
				reinterpret_cast<const uint8*>(Utf8.Get()),
				Utf8.Length());
		}

		FString SerializeNumber(const double Value)
		{
			if (!FMath::IsFinite(Value))
			{
				return TEXT("null");
			}
			if (Value == 0.0)
			{
				return TEXT("0");
			}
			return FString::SanitizeFloat(Value, 0);
		}
	}

	FCanonicalJsonValue FCanonicalJsonValue::Null()
	{
		return FCanonicalJsonValue();
	}

	FCanonicalJsonValue FCanonicalJsonValue::Boolean(const bool Value)
	{
		FCanonicalJsonValue Result;
		Result.Kind = EKind::Boolean;
		Result.BooleanValue = Value;
		return Result;
	}

	FCanonicalJsonValue FCanonicalJsonValue::Integer(const int64 Value)
	{
		FCanonicalJsonValue Result;
		Result.Kind = EKind::Integer;
		Result.IntegerValue = Value;
		return Result;
	}

	FCanonicalJsonValue FCanonicalJsonValue::Number(const double Value)
	{
		FCanonicalJsonValue Result;
		Result.Kind = EKind::Number;
		Result.NumberValue = Value;
		return Result;
	}

	FCanonicalJsonValue FCanonicalJsonValue::String(FString Value)
	{
		FCanonicalJsonValue Result;
		Result.Kind = EKind::String;
		Result.StringValue = MoveTemp(Value);
		return Result;
	}

	FCanonicalJsonValue FCanonicalJsonValue::Array()
	{
		FCanonicalJsonValue Result;
		Result.Kind = EKind::Array;
		return Result;
	}

	FCanonicalJsonValue FCanonicalJsonValue::Object()
	{
		FCanonicalJsonValue Result;
		Result.Kind = EKind::Object;
		return Result;
	}

	FCanonicalJsonValue::EKind FCanonicalJsonValue::GetKind() const
	{
		return Kind;
	}

	void FCanonicalJsonValue::Add(FCanonicalJsonValue Value)
	{
		check(Kind == EKind::Array);
		ArrayValue.Add(MakeShared<FCanonicalJsonValue>(MoveTemp(Value)));
	}

	void FCanonicalJsonValue::Set(FString Key, FCanonicalJsonValue Value)
	{
		check(Kind == EKind::Object);
		ObjectValue.Add(
			MoveTemp(Key),
			MakeShared<FCanonicalJsonValue>(MoveTemp(Value)));
	}

	FString EscapeCanonicalJsonString(const FStringView Value)
	{
		FString Result;
		Result.Reserve(Value.Len() + 2);
		Result.AppendChar(TEXT('"'));
		for (const TCHAR Character : Value)
		{
			switch (Character)
			{
			case TEXT('"'): Result.Append(TEXT("\\\"")); break;
			case TEXT('\\'): Result.Append(TEXT("\\\\")); break;
			case TEXT('\b'): Result.Append(TEXT("\\b")); break;
			case TEXT('\f'): Result.Append(TEXT("\\f")); break;
			case TEXT('\n'): Result.Append(TEXT("\\n")); break;
			case TEXT('\r'): Result.Append(TEXT("\\r")); break;
			case TEXT('\t'): Result.Append(TEXT("\\t")); break;
			default:
				if (Character >= 0 && Character < 0x20)
				{
					Result.Appendf(TEXT("\\u%04x"), static_cast<uint32>(Character));
				}
				else
				{
					Result.AppendChar(Character);
				}
				break;
			}
		}
		Result.AppendChar(TEXT('"'));
		return Result;
	}

	FString SerializeCanonicalJson(const FCanonicalJsonValue& Value)
	{
		switch (Value.Kind)
		{
		case FCanonicalJsonValue::EKind::Null:
			return TEXT("null");
		case FCanonicalJsonValue::EKind::Boolean:
			return Value.BooleanValue ? TEXT("true") : TEXT("false");
		case FCanonicalJsonValue::EKind::Integer:
			return FString::Printf(TEXT("%lld"), Value.IntegerValue);
		case FCanonicalJsonValue::EKind::Number:
			return SerializeNumber(Value.NumberValue);
		case FCanonicalJsonValue::EKind::String:
			return EscapeCanonicalJsonString(Value.StringValue);
		case FCanonicalJsonValue::EKind::Array:
		{
			FString Result(TEXT("["));
			for (int32 Index = 0; Index < Value.ArrayValue.Num(); ++Index)
			{
				if (Index != 0)
				{
					Result.AppendChar(TEXT(','));
				}
				Result.Append(SerializeCanonicalJson(*Value.ArrayValue[Index]));
			}
			Result.AppendChar(TEXT(']'));
			return Result;
		}
		case FCanonicalJsonValue::EKind::Object:
		{
			TArray<FString> Keys;
			Value.ObjectValue.GenerateKeyArray(Keys);
			Keys.Sort([](const FString& Left, const FString& Right)
			{
				return Left.Compare(Right, ESearchCase::CaseSensitive) < 0;
			});

			FString Result(TEXT("{"));
			for (int32 Index = 0; Index < Keys.Num(); ++Index)
			{
				if (Index != 0)
				{
					Result.AppendChar(TEXT(','));
				}
				Result.Append(EscapeCanonicalJsonString(Keys[Index]));
				Result.AppendChar(TEXT(':'));
				Result.Append(SerializeCanonicalJson(
					*Value.ObjectValue.FindChecked(Keys[Index])));
			}
			Result.AppendChar(TEXT('}'));
			return Result;
		}
		default:
			checkNoEntry();
			return TEXT("null");
		}
	}

	TArray<uint8> SerializeCanonicalJsonDocument(const FCanonicalJsonValue& Value)
	{
		TArray<uint8> Result;
		AppendUtf8(SerializeCanonicalJson(Value), Result);
		Result.Add(static_cast<uint8>('\n'));
		return Result;
	}

	TArray<uint8> SerializeCanonicalJsonLines(const TArray<FCanonicalJsonLine>& Lines)
	{
		struct FOrderedLine
		{
			FString StableId;
			FString Payload;
		};

		TArray<FOrderedLine> Ordered;
		Ordered.Reserve(Lines.Num());
		for (const FCanonicalJsonLine& Line : Lines)
		{
			Ordered.Add({Line.StableId, SerializeCanonicalJson(Line.Payload)});
		}
		Ordered.Sort([](const FOrderedLine& Left, const FOrderedLine& Right)
		{
			const int32 StableIdOrder =
				Left.StableId.Compare(Right.StableId, ESearchCase::CaseSensitive);
			if (StableIdOrder != 0)
			{
				return StableIdOrder < 0;
			}
			return Left.Payload.Compare(Right.Payload, ESearchCase::CaseSensitive) < 0;
		});

		TArray<uint8> Result;
		for (const FOrderedLine& Line : Ordered)
		{
			AppendUtf8(Line.Payload, Result);
			Result.Add(static_cast<uint8>('\n'));
		}
		return Result;
	}
}
