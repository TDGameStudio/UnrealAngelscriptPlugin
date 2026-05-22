#include "AngelscriptBindDatabase.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "StaticJIT/StaticJITConfig.h"
#include "AngelscriptEngine.h"
#include "UObject/Class.h"

#if WITH_EDITOR
#include "SourceCodeNavigation.h"
#endif

FAngelscriptBindDatabase& FAngelscriptBindDatabase::Get()
{
	if (FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		if (FAngelscriptBindDatabase* DB = Engine->GetBindDatabase())
		{
			return *DB;
		}
	}
	static FAngelscriptBindDatabase LegacyBindDatabase;
	return LegacyBindDatabase;
}

void FAngelscriptBindDatabase::Serialize(FArchive& Archive)
{
	Archive << Structs;
	Archive << Classes;
}

void FAngelscriptBindDatabase::Clear()
{
	Structs.Empty();
	Classes.Empty();
	BoundEnums.Empty();
	BoundDelegateFunctions.Empty();
	HeaderLinks.Empty();
}

void FAngelscriptBindDatabase::Save(const FString& Path)
{
	{
		TArray<uint8> Data;
		FMemoryWriter Writer(Data);

		uint32 Magic = CacheMagic;
		int32 Version = CacheVersion;
		Writer << Magic;
		Writer << Version;
		Serialize(Writer);

		bool bSaveSuccess = FFileHelper::SaveArrayToFile(Data, *Path);
		if (IsRunningCookCommandlet())
		{
			if (!bSaveSuccess)
			{
				UE_LOG(Angelscript, Error, TEXT("Unable to write the Script/Binds.Cache file during cook"));
			}
		}
	}

#if WITH_EDITOR
	{
		TArray<uint8> HeaderData;
		FMemoryWriter Writer(HeaderData);

		TArray<FAngelscriptClassHeader> Headers;
		for (auto& Bind : Classes)
		{
			UClass* Class = FindObject<UClass>(nullptr, *Bind.UnrealPath);
			FString HeaderPath;
			if (FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath) && IFileManager::Get().FileSize(*HeaderPath) != INDEX_NONE)
				Headers.Add(FAngelscriptClassHeader{Bind.UnrealPath, HeaderPath});
		}

		for (auto& Bind : Structs)
		{
			UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Bind.UnrealPath);
			FString HeaderPath;
			if (FSourceCodeNavigation::FindClassHeaderPath(Struct, HeaderPath) && IFileManager::Get().FileSize(*HeaderPath) != INDEX_NONE)
				Headers.Add(FAngelscriptClassHeader{Bind.UnrealPath, HeaderPath});
		}

		for (UEnum* Enum : BoundEnums)
		{
			FString HeaderPath;
			if (FSourceCodeNavigation::FindClassHeaderPath(Enum, HeaderPath) && IFileManager::Get().FileSize(*HeaderPath) != INDEX_NONE)
				Headers.Add(FAngelscriptClassHeader{Enum->GetPathName(), HeaderPath});
		}

		for (UDelegateFunction* DelegateFunction : BoundDelegateFunctions)
		{
			FString HeaderPath;
			if (FSourceCodeNavigation::FindClassHeaderPath(DelegateFunction, HeaderPath) && IFileManager::Get().FileSize(*HeaderPath) != INDEX_NONE)
				Headers.Add(FAngelscriptClassHeader{ DelegateFunction->GetPathName(), HeaderPath });
		}

		Writer << Headers;

		FFileHelper::SaveArrayToFile(HeaderData, *(Path + TEXT(".Headers")));
	}
#endif
}

bool FAngelscriptBindDatabase::TryLoad(const FString& Path, bool bGeneratingPrecompiledData, FString* OutErrorMessage)
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *Path))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = FString::Printf(TEXT("Unable to load script bind database '%s': file is missing."), *Path);
		}
		return false;
	}

	if (Data.Num() < static_cast<int32>(sizeof(uint32) + sizeof(int32)))
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = FString::Printf(TEXT("Unable to load script bind database '%s': cache is empty or truncated. Regenerate Script/Binds.Cache."), *Path);
		}
		return false;
	}

	FMemoryReader Reader(Data);
	uint32 Magic = 0;
	int32 Version = 0;
	Reader << Magic;
	Reader << Version;
	if (Magic != CacheMagic || Version != CacheVersion)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = FString::Printf(
				TEXT("Unable to load script bind database '%s': unsupported cache version (magic=0x%016llx version=%d expected=0x%016llx version=%d). Regenerate Script/Binds.Cache."),
				*Path,
				static_cast<unsigned long long>(Magic),
				Version,
				static_cast<unsigned long long>(CacheMagic),
				CacheVersion);
		}
		return false;
	}

	FAngelscriptBindDatabase LoadedDatabase;
	LoadedDatabase.Serialize(Reader);

	if (LoadedDatabase.Classes.Num() == 0 && LoadedDatabase.Structs.Num() == 0)
	{
		if (OutErrorMessage != nullptr)
		{
			*OutErrorMessage = FString::Printf(TEXT("Unable to load script bind database '%s': cache contained no class or struct binds."), *Path);
		}
		return false;
	}

	Structs = MoveTemp(LoadedDatabase.Structs);
	Classes = MoveTemp(LoadedDatabase.Classes);

#if AS_CAN_GENERATE_JIT
	if (bGeneratingPrecompiledData)
	{
		HeaderLinks.Empty();

		TArray<uint8> HeaderData;
		if (FFileHelper::LoadFileToArray(HeaderData, *(Path + TEXT(".Headers"))))
		{
			FMemoryReader HeaderReader(HeaderData);

			TArray<FAngelscriptClassHeader> Headers;
			HeaderReader << Headers;

			for (const auto& Header : Headers)
			{
				UObject* Field = FindObject<UObject>(nullptr, *Header.UnrealPath);
				if (Field == nullptr)
					continue;
				HeaderLinks.Add(Field, Header.Header);
			}
		}
	}
#endif

	return true;
}

void FAngelscriptBindDatabase::Load(const FString& Path, bool bGeneratingPrecompiledData)
{
	FString ErrorMessage;
	if (!TryLoad(Path, bGeneratingPrecompiledData, &ErrorMessage))
	{
		UE_LOG(Angelscript, Fatal, TEXT("%s"), *ErrorMessage);
	}
}

FString FAngelscriptBindDatabase::GetSourceHeader(UField* Field)
{
#if WITH_EDITOR
	FString HeaderPath;
	if (FSourceCodeNavigation::FindClassHeaderPath(Field, HeaderPath) && IFileManager::Get().FileSize(*HeaderPath) != INDEX_NONE)
		return HeaderPath;
#else
	FString* Found = FAngelscriptBindDatabase::Get().HeaderLinks.Find(Field);
	if (Found != nullptr)
		return *Found;
#endif

	return TEXT("");
}
