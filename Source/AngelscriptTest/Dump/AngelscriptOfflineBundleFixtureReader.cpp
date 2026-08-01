#include "AngelscriptOfflineBundleFixtureReader.h"

#include "Dom/JsonObject.h"
#include "Dump/AngelscriptOfflineContractIdentity.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool IsContinuation(const uint8 Value)
	{
		return Value >= 0x80 && Value <= 0xbf;
	}

	bool IsStrictUtf8(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() >= 3
			&& Bytes[0] == 0xef
			&& Bytes[1] == 0xbb
			&& Bytes[2] == 0xbf)
		{
			return false;
		}

		for (int32 Index = 0; Index < Bytes.Num();)
		{
			const uint8 First = Bytes[Index];
			if (First <= 0x7f)
			{
				++Index;
				continue;
			}
			if (First >= 0xc2 && First <= 0xdf)
			{
				if (Index + 1 >= Bytes.Num()
					|| !IsContinuation(Bytes[Index + 1]))
				{
					return false;
				}
				Index += 2;
				continue;
			}
			if (First >= 0xe0 && First <= 0xef)
			{
				if (Index + 2 >= Bytes.Num()
					|| !IsContinuation(Bytes[Index + 2]))
				{
					return false;
				}
				const uint8 Second = Bytes[Index + 1];
				if ((First == 0xe0 && (Second < 0xa0 || Second > 0xbf))
					|| (First == 0xed && (Second < 0x80 || Second > 0x9f))
					|| (First != 0xe0
						&& First != 0xed
						&& !IsContinuation(Second)))
				{
					return false;
				}
				Index += 3;
				continue;
			}
			if (First >= 0xf0 && First <= 0xf4)
			{
				if (Index + 3 >= Bytes.Num()
					|| !IsContinuation(Bytes[Index + 2])
					|| !IsContinuation(Bytes[Index + 3]))
				{
					return false;
				}
				const uint8 Second = Bytes[Index + 1];
				if ((First == 0xf0 && (Second < 0x90 || Second > 0xbf))
					|| (First == 0xf4 && (Second < 0x80 || Second > 0x8f))
					|| (First != 0xf0
						&& First != 0xf4
						&& !IsContinuation(Second)))
				{
					return false;
				}
				Index += 4;
				continue;
			}
			return false;
		}
		return true;
	}

	bool LoadCanonicalUtf8(
		const FString& Filename,
		TArray<uint8>& OutBytes,
		FString& OutText,
		FString& OutError)
	{
		if (!FFileHelper::LoadFileToArray(OutBytes, *Filename))
		{
			OutError = FString::Printf(
				TEXT("Missing required file '%s'"),
				*FPaths::GetCleanFilename(Filename));
			return false;
		}
		if (!IsStrictUtf8(OutBytes))
		{
			OutError = FString::Printf(
				TEXT("File '%s' is not strict UTF-8 without BOM"),
				*FPaths::GetCleanFilename(Filename));
			return false;
		}
		if (OutBytes.Contains(static_cast<uint8>('\r')))
		{
			OutError = FString::Printf(
				TEXT("File '%s' does not use canonical LF endings"),
				*FPaths::GetCleanFilename(Filename));
			return false;
		}
		if (!OutBytes.IsEmpty())
		{
			const FUTF8ToTCHAR Converted(
				reinterpret_cast<const ANSICHAR*>(OutBytes.GetData()),
				OutBytes.Num());
			OutText = FString(Converted.Length(), Converted.Get());
		}
		return true;
	}

	bool ParseObject(
		const FString& Text,
		TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader =
			TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, OutObject)
			&& OutObject.IsValid();
	}

	bool ValidateRecordFile(
		const FString& BundleDirectory,
		const TSharedPtr<FJsonObject>& FileObject,
		int64& OutCount,
		FString& OutError)
	{
		if (!FileObject.IsValid()
			|| !FileObject->HasTypedField<EJson::String>(TEXT("name"))
			|| !FileObject->HasTypedField<EJson::String>(TEXT("sha256"))
			|| !FileObject->HasTypedField<EJson::Number>(TEXT("recordCount"))
			|| !FileObject->HasTypedField<EJson::Number>(TEXT("byteCount")))
		{
			OutError = TEXT("Manifest contains an incomplete file record");
			return false;
		}

		const FString Name = FileObject->GetStringField(TEXT("name"));
		if (Name != TEXT("symbols.jsonl")
			&& Name != TEXT("assets.jsonl"))
		{
			OutError = FString::Printf(
				TEXT("Manifest references unsupported required file '%s'"),
				*Name);
			return false;
		}

		TArray<uint8> Bytes;
		FString Text;
		if (!LoadCanonicalUtf8(
			FPaths::Combine(BundleDirectory, Name),
			Bytes,
			Text,
			OutError))
		{
			return false;
		}
		const int64 ExpectedBytes = static_cast<int64>(
			FileObject->GetNumberField(TEXT("byteCount")));
		if (ExpectedBytes != Bytes.Num())
		{
			OutError = FString::Printf(
				TEXT("Byte count mismatch for '%s'"),
				*Name);
			return false;
		}
		const FString ExpectedHash =
			FileObject->GetStringField(TEXT("sha256"));
		if (ExpectedHash !=
			AngelscriptOfflineContract::Sha256Bytes(Bytes))
		{
			OutError = FString::Printf(
				TEXT("SHA-256 mismatch for '%s'"),
				*Name);
			return false;
		}

		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, true);
		const int64 ExpectedCount = static_cast<int64>(
			FileObject->GetNumberField(TEXT("recordCount")));
		if (ExpectedCount != Lines.Num())
		{
			OutError = FString::Printf(
				TEXT("Record count mismatch for '%s'"),
				*Name);
			return false;
		}

		TMap<FString, FString> PayloadByStableId;
		for (const FString& Line : Lines)
		{
			TSharedPtr<FJsonObject> Record;
			if (!ParseObject(Line, Record)
				|| !Record->HasTypedField<EJson::String>(
					TEXT("stableId")))
			{
				OutError = FString::Printf(
					TEXT("Malformed JSONL record in '%s'"),
					*Name);
				return false;
			}
			const FString StableId =
				Record->GetStringField(TEXT("stableId"));
			if (StableId.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("Empty stable ID in '%s'"),
					*Name);
				return false;
			}
			if (const FString* Existing =
				PayloadByStableId.Find(StableId))
			{
				if (*Existing != Line)
				{
					OutError = FString::Printf(
						TEXT(
							"Inconsistent duplicate stable ID '%s' in '%s'"),
						*StableId,
						*Name);
					return false;
				}
			}
			else
			{
				PayloadByStableId.Add(StableId, Line);
			}
		}
		OutCount = Lines.Num();
		return true;
	}
}

FAngelscriptOfflineBundleFixtureReadResult
FAngelscriptOfflineBundleFixtureReader::Read(
	const FString& BundleDirectory)
{
	FAngelscriptOfflineBundleFixtureReadResult Result;
	const FString ManifestFilename =
		FPaths::Combine(BundleDirectory, TEXT("manifest.json"));
	TArray<uint8> ManifestBytes;
	FString ManifestText;
	if (!LoadCanonicalUtf8(
		ManifestFilename,
		ManifestBytes,
		ManifestText,
		Result.Error))
	{
		return Result;
	}

	TSharedPtr<FJsonObject> Manifest;
	if (!ParseObject(ManifestText, Manifest))
	{
		Result.Error = TEXT("Malformed manifest JSON");
		return Result;
	}
	const TSharedPtr<FJsonObject>* Schema = nullptr;
	if (!Manifest->TryGetObjectField(TEXT("schema"), Schema)
		|| Schema == nullptr
		|| !Schema->IsValid()
		|| static_cast<int32>((*Schema)->GetNumberField(TEXT("major")))
			!= AngelscriptOfflineContract::SchemaMajorVersion
		|| static_cast<int32>((*Schema)->GetNumberField(TEXT("minor")))
			> AngelscriptOfflineContract::SchemaMinorVersion)
	{
		Result.Error = TEXT("Unsupported offline bundle schema");
		return Result;
	}

	const TSharedPtr<FJsonObject>* SymbolScope = nullptr;
	if (!Manifest->TryGetObjectField(
		TEXT("symbolScope"),
		SymbolScope)
		|| SymbolScope == nullptr
		|| !SymbolScope->IsValid()
		|| !(*SymbolScope)->GetBoolField(TEXT("complete")))
	{
		Result.Error = TEXT("Offline bundle symbol scope is incomplete");
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* RequiredFields = nullptr;
	if (Manifest->TryGetArrayField(
		TEXT("requiredFields"),
		RequiredFields)
		&& RequiredFields != nullptr)
	{
		static const TSet<FString> SupportedRequiredFields = {
			TEXT("manifest.schema"),
			TEXT("manifest.symbolScope"),
			TEXT("records.stableId"),
		};
		for (const TSharedPtr<FJsonValue>& Value : *RequiredFields)
		{
			FString RequiredField;
			if (!Value.IsValid()
				|| !Value->TryGetString(RequiredField)
				|| !SupportedRequiredFields.Contains(RequiredField))
			{
				Result.Error = TEXT(
					"Manifest requires an unsupported field");
				return Result;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
	if (!Manifest->TryGetArrayField(TEXT("files"), Files)
		|| Files == nullptr)
	{
		Result.Error = TEXT("Manifest does not contain required file records");
		return Result;
	}

	TSet<FString> SeenFiles;
	for (const TSharedPtr<FJsonValue>& Value : *Files)
	{
		const TSharedPtr<FJsonObject> FileObject =
			Value.IsValid() ? Value->AsObject() : nullptr;
		if (!FileObject.IsValid()
			|| !FileObject->HasTypedField<EJson::String>(TEXT("name")))
		{
			Result.Error = TEXT("Manifest contains an invalid file record");
			return Result;
		}
		const FString Name = FileObject->GetStringField(TEXT("name"));
		if (SeenFiles.Contains(Name))
		{
			Result.Error = FString::Printf(
				TEXT("Manifest repeats file record '%s'"),
				*Name);
			return Result;
		}
		SeenFiles.Add(Name);

		int64 Count = 0;
		if (!ValidateRecordFile(
			BundleDirectory,
			FileObject,
			Count,
			Result.Error))
		{
			return Result;
		}
		if (Name == TEXT("symbols.jsonl"))
		{
			Result.SymbolCount = Count;
		}
		else
		{
			Result.AssetCount = Count;
		}
	}
	if (!SeenFiles.Contains(TEXT("symbols.jsonl"))
		|| !SeenFiles.Contains(TEXT("assets.jsonl")))
	{
		Result.Error = TEXT("Manifest omits a required record file");
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
