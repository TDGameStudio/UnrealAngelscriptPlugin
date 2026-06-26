#include "ClassGenerator/AngelscriptClassRedirects.h"

#include "AngelscriptEngine.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UObject/CoreRedirects.h"

namespace AngelscriptClassRedirects
{
	FString GCoreRedirectTargetIniOverride;
	const FString CoreRedirectsSection(TEXT("CoreRedirects"));

	FString GetTargetIniPath()
	{
		if (!GCoreRedirectTargetIniOverride.IsEmpty())
		{
			return GCoreRedirectTargetIniOverride;
		}

		return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEngine.ini"));
	}

	FString MakeRedirectValue(const FString& OldClassPath, const FString& NewClassPath)
	{
		return FString::Printf(
			TEXT("(OldName=\"%s\",NewName=\"%s\")"),
			*OldClassPath,
			*NewClassPath);
	}

	bool TryParseRedirectValue(const FString& RedirectValue, FString& OutOldName, FString& OutNewName)
	{
		return FParse::Value(*RedirectValue, TEXT("OldName="), OutOldName)
			&& FParse::Value(*RedirectValue, TEXT("NewName="), OutNewName);
	}

	bool HasRedirectValue(const FConfigFile& ConfigFile, const FString& OldClassPath, const FString& NewClassPath)
	{
		const FConfigSection* RedirectSection = ConfigFile.FindSection(CoreRedirectsSection);
		if (RedirectSection == nullptr)
		{
			return false;
		}

		static const FName ClassRedirectsKey(TEXT("ClassRedirects"));
		for (FConfigSection::TConstKeyIterator It(*RedirectSection, ClassRedirectsKey); It; ++It)
		{
			FString ExistingOldName;
			FString ExistingNewName;
			if (TryParseRedirectValue(It.Value().GetValue(), ExistingOldName, ExistingNewName)
				&& ExistingOldName == OldClassPath
				&& ExistingNewName == NewClassPath)
			{
				return true;
			}
		}

		static const FName PlusClassRedirectsKey(TEXT("+ClassRedirects"));
		for (FConfigSection::TConstKeyIterator It(*RedirectSection, PlusClassRedirectsKey); It; ++It)
		{
			FString ExistingOldName;
			FString ExistingNewName;
			if (TryParseRedirectValue(It.Value().GetValue(), ExistingOldName, ExistingNewName)
				&& ExistingOldName == OldClassPath
				&& ExistingNewName == NewClassPath)
			{
				return true;
			}
		}

		return false;
	}

	bool IniContainsRedirectValue(const FString& IniPath, const FString& OldClassPath, const FString& NewClassPath)
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(IniPath);
		return HasRedirectValue(ConfigFile, OldClassPath, NewClassPath);
	}

	void CollectRedirectsToReplace(
		const FConfigFile& ConfigFile,
		const FString& OldClassPath,
		const FString& NewClassPath,
		TArray<FString>& OutRedirectValues,
		TArray<FCoreRedirect>& OutRedirects)
	{
		const FConfigSection* RedirectSection = ConfigFile.FindSection(CoreRedirectsSection);
		if (RedirectSection == nullptr)
		{
			return;
		}

		const auto CollectForKey = [&](const FName ClassRedirectsKey)
		{
			for (FConfigSection::TConstKeyIterator It(*RedirectSection, ClassRedirectsKey); It; ++It)
			{
				const FString& RedirectValue = It.Value().GetValue();

				FString ExistingOldName;
				FString ExistingNewName;
				if (!TryParseRedirectValue(RedirectValue, ExistingOldName, ExistingNewName))
				{
					continue;
				}

				const bool bConflictsWithNewRedirect = ExistingOldName == OldClassPath && ExistingNewName != NewClassPath;
				const bool bIsReverseOfNewRedirect = ExistingOldName == NewClassPath && ExistingNewName == OldClassPath;
				if (!bConflictsWithNewRedirect && !bIsReverseOfNewRedirect)
				{
					continue;
				}

				OutRedirectValues.AddUnique(RedirectValue);
				OutRedirects.AddUnique(FCoreRedirect(ECoreRedirectFlags::Type_Class, ExistingOldName, ExistingNewName));
			}
		};

		CollectForKey(FName(TEXT("ClassRedirects")));
		CollectForKey(FName(TEXT("+ClassRedirects")));
	}

	void RemoveRedirectValues(FConfigFile& ConfigFile, const TArray<FString>& RedirectValues)
	{
		for (const FString& RedirectValue : RedirectValues)
		{
			ConfigFile.RemoveFromSection(*CoreRedirectsSection, TEXT("+ClassRedirects"), RedirectValue);
			ConfigFile.RemoveFromSection(*CoreRedirectsSection, TEXT("ClassRedirects"), RedirectValue);
		}
	}

	bool PreviousNamesContain(const TArray<FCoreRedirectObjectName>& PreviousClassNames, const FString& OldClassName, const FString& OldClassPath)
	{
		for (const FCoreRedirectObjectName& PreviousClassName : PreviousClassNames)
		{
			const FString PreviousClassPath = PreviousClassName.ToString();
			if (PreviousClassName.ObjectName == FName(*OldClassName)
				|| PreviousClassPath == OldClassName
				|| PreviousClassPath == OldClassPath)
			{
				return true;
			}
		}

		return false;
	}

	bool HasExistingRedirect(const FString& OldClassName, const FString& OldClassPath, const FString& NewClassName, const FString& NewClassPath)
	{
		TArray<FCoreRedirectObjectName> PreviousClassNames;
		if (FCoreRedirects::FindPreviousNames(
			ECoreRedirectFlags::Type_Class,
			FCoreRedirectObjectName(NewClassPath),
			PreviousClassNames)
			&& PreviousNamesContain(PreviousClassNames, OldClassName, OldClassPath))
		{
			return true;
		}

		PreviousClassNames.Reset();
		return FCoreRedirects::FindPreviousNames(
			ECoreRedirectFlags::Type_Class,
			FCoreRedirectObjectName(NewClassName),
			PreviousClassNames)
			&& PreviousNamesContain(PreviousClassNames, OldClassName, OldClassPath);
	}

	bool AppendRedirectLineToIni(const FString& IniPath, const FString& OldClassPath, const FString& NewClassPath)
	{
		const FString IniDirectory = FPaths::GetPath(IniPath);
		if (!IFileManager::Get().MakeDirectory(*IniDirectory, true))
		{
			UE_LOG(Angelscript, Warning, TEXT("Could not create CoreRedirect config directory '%s'."), *IniDirectory);
			return false;
		}

		TArray<FCoreRedirect> RedirectsToRemove;
		if (GCoreRedirectTargetIniOverride.IsEmpty())
		{
			FConfigCacheIni Config(EConfigCacheType::Temporary);
			FConfigFile& ConfigFile = Config.Add(IniPath, FConfigFile());
			FConfigCacheIni::LoadLocalIniFile(ConfigFile, TEXT("DefaultEngine"), false);

			TArray<FString> RedirectValuesToRemove;
			CollectRedirectsToReplace(ConfigFile, OldClassPath, NewClassPath, RedirectValuesToRemove, RedirectsToRemove);
			const bool bAlreadyContainsRedirect = HasRedirectValue(ConfigFile, OldClassPath, NewClassPath);
			if (RedirectValuesToRemove.Num() == 0 && bAlreadyContainsRedirect)
			{
				return true;
			}

			RemoveRedirectValues(ConfigFile, RedirectValuesToRemove);

			if (!bAlreadyContainsRedirect)
			{
				ConfigFile.AddToSection(
					*CoreRedirectsSection,
					TEXT("+ClassRedirects"),
					MakeRedirectValue(OldClassPath, NewClassPath));
			}

			ConfigFile.UpdateSections(*IniPath, *CoreRedirectsSection);
		}
		else
		{
			FConfigFile ConfigFile;
			ConfigFile.Read(IniPath);

			TArray<FString> RedirectValuesToRemove;
			CollectRedirectsToReplace(ConfigFile, OldClassPath, NewClassPath, RedirectValuesToRemove, RedirectsToRemove);
			const bool bAlreadyContainsRedirect = HasRedirectValue(ConfigFile, OldClassPath, NewClassPath);
			if (RedirectValuesToRemove.Num() == 0 && bAlreadyContainsRedirect)
			{
				return true;
			}

			RemoveRedirectValues(ConfigFile, RedirectValuesToRemove);

			if (!bAlreadyContainsRedirect)
			{
				ConfigFile.AddToSection(
					*CoreRedirectsSection,
					TEXT("+ClassRedirects"),
					MakeRedirectValue(OldClassPath, NewClassPath));
			}

			ConfigFile.Write(IniPath);
		}

		if (RedirectsToRemove.Num() != 0)
		{
			FCoreRedirects::RemoveRedirectList(MakeArrayView(RedirectsToRemove), FAngelscriptClassRedirects::GeneratedRedirectSource);
		}

		if (!IniContainsRedirectValue(IniPath, OldClassPath, NewClassPath))
		{
			UE_LOG(Angelscript, Warning, TEXT("Could not write generated CoreRedirect to '%s'."), *IniPath);
			return false;
		}

		return true;
	}
}

FString FAngelscriptClassRedirects::MakeScriptClassPath(const FString& ClassName)
{
	return FString::Printf(TEXT("/Script/Angelscript.%s"), *ClassName);
}

bool FAngelscriptClassRedirects::TryAddGeneratedCoreRedirect(const FString& OldClassName, const FString& NewClassName)
{
#if WITH_EDITOR
	if (OldClassName.IsEmpty() || NewClassName.IsEmpty() || OldClassName == NewClassName)
	{
		return false;
	}

	const FString OldClassPath = MakeScriptClassPath(OldClassName);
	const FString NewClassPath = MakeScriptClassPath(NewClassName);

	const FString IniPath = AngelscriptClassRedirects::GetTargetIniPath();
	if (!AngelscriptClassRedirects::AppendRedirectLineToIni(IniPath, OldClassPath, NewClassPath))
	{
		return false;
	}

	TArray<FCoreRedirect> Redirects;
	Redirects.Add(FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassPath, NewClassPath));
	return FCoreRedirects::AddRedirectList(MakeArrayView(Redirects), GeneratedRedirectSource)
		|| AngelscriptClassRedirects::HasExistingRedirect(OldClassName, OldClassPath, NewClassName, NewClassPath);
#else
	return false;
#endif
}

#if WITH_AUTOMATION_TESTS
void FAngelscriptClassRedirects::SetCoreRedirectTargetIniOverrideForTesting(const FString& InIniPath)
{
	AngelscriptClassRedirects::GCoreRedirectTargetIniOverride = InIniPath;
}

void FAngelscriptClassRedirects::ResetCoreRedirectTargetIniOverrideForTesting()
{
	AngelscriptClassRedirects::GCoreRedirectTargetIniOverride.Reset();
}
#endif
