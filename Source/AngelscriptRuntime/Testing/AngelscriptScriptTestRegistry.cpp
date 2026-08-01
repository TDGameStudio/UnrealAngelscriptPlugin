#include "Testing/AngelscriptScriptTestRegistry.h"

#include "Core/AngelscriptEngine.h"
#include "Testing/AngelscriptScriptTestAutomation.h"
#include "Testing/AngelscriptTestSuite.h"

#include "Algo/Sort.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeRWLock.h"

namespace
{
	static const FName AngelscriptTestMetaName(TEXT("AngelscriptTest"));
	static const FName AngelscriptTestFlagsMetaName(TEXT("AngelscriptTestFlags"));

	FString GetSourceFile(
		const FAngelscriptModuleDesc& Module,
		const FAngelscriptFunctionDesc* Method = nullptr)
	{
		if (Method != nullptr && Method->ScriptFunction != nullptr)
		{
			const char* ScriptSectionName =
				Method->ScriptFunction->GetScriptSectionName();
			if (ScriptSectionName != nullptr && ScriptSectionName[0] != '\0')
			{
				return ANSI_TO_TCHAR(ScriptSectionName);
			}
		}

		for (const FAngelscriptModuleDesc::FCodeSection& Section : Module.Code)
		{
			if (!Section.AbsoluteFilename.IsEmpty())
			{
				return Section.AbsoluteFilename;
			}
			if (!Section.VirtualPath.IsEmpty())
			{
				return Section.VirtualPath;
			}
		}
		return FString();
	}

	FString NormalizeAutomationName(const FString& Value)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT(" "), TEXT("_"));
		Result.ToLowerInline();
		return Result;
	}

	bool IsDerivedFromSuite(
		const TSharedRef<FAngelscriptClassDesc>& Class,
		const TMap<FString, TSharedRef<FAngelscriptClassDesc>>& ClassesByName)
	{
		TSet<FString> Visited;
		TSharedPtr<FAngelscriptClassDesc> Current = Class;
		while (Current.IsValid() && !Visited.Contains(Current->ClassName))
		{
			Visited.Add(Current->ClassName);

			if (Current->Class != nullptr
				&& Current->Class->IsChildOf(UAngelscriptTestSuite::StaticClass()))
			{
				return true;
			}
			if (Current->CodeSuperClass != nullptr
				&& Current->CodeSuperClass->IsChildOf(UAngelscriptTestSuite::StaticClass()))
			{
				return true;
			}
			if (Current->SuperClass == UAngelscriptTestSuite::StaticClass()->GetName())
			{
				return true;
			}

			const TSharedRef<FAngelscriptClassDesc>* Parent =
				ClassesByName.Find(Current->SuperClass);
			Current = Parent != nullptr
				? TSharedPtr<FAngelscriptClassDesc>(*Parent)
				: nullptr;
		}
		return false;
	}

	void AddDiagnostic(
		FAngelscriptScriptTestRegistryBuildResult& Result,
		const FString& SourceFile,
		int32 SourceLine,
		const FString& Message)
	{
		FAngelscriptScriptTestDiagnostic& Diagnostic =
			Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.SourceFile = SourceFile;
		Diagnostic.SourceLine = FMath::Max(1, SourceLine);
		Diagnostic.Message = Message;
	}

	const TMap<FString, EAutomationTestFlags>& GetAutomationFlagsByName()
	{
		static const TMap<FString, EAutomationTestFlags> FlagsByName = {
			{ TEXT("EditorContext"), EAutomationTestFlags::EditorContext },
			{ TEXT("ClientContext"), EAutomationTestFlags::ClientContext },
			{ TEXT("ServerContext"), EAutomationTestFlags::ServerContext },
			{ TEXT("CommandletContext"), EAutomationTestFlags::CommandletContext },
			{ TEXT("ProgramContext"), EAutomationTestFlags::ProgramContext },
			{ TEXT("NonNullRHI"), EAutomationTestFlags::NonNullRHI },
			{ TEXT("RequiresUser"), EAutomationTestFlags::RequiresUser },
			{ TEXT("Disabled"), EAutomationTestFlags::Disabled },
			{ TEXT("SupportsAutoRTFM"), EAutomationTestFlags::SupportsAutoRTFM },
			{ TEXT("CriticalPriority"), EAutomationTestFlags::CriticalPriority },
			{ TEXT("HighPriority"), EAutomationTestFlags::HighPriority },
			{ TEXT("MediumPriority"), EAutomationTestFlags::MediumPriority },
			{ TEXT("LowPriority"), EAutomationTestFlags::LowPriority },
			{ TEXT("SmokeFilter"), EAutomationTestFlags::SmokeFilter },
			{ TEXT("EngineFilter"), EAutomationTestFlags::EngineFilter },
			{ TEXT("ProductFilter"), EAutomationTestFlags::ProductFilter },
			{ TEXT("PerfFilter"), EAutomationTestFlags::PerfFilter },
			{ TEXT("StressFilter"), EAutomationTestFlags::StressFilter },
			{ TEXT("NegativeFilter"), EAutomationTestFlags::NegativeFilter },
		};
		return FlagsByName;
	}
}

FString FAngelscriptScriptTestId::ToCommandString(uint64 Generation) const
{
	return FString::Printf(
		TEXT("%llu|%s|%s|%s"),
		static_cast<unsigned long long>(Generation),
		*ModuleName,
		*SuiteName,
		*MethodName);
}

bool FAngelscriptScriptTestId::TryParseCommandString(
	const FString& Command,
	FAngelscriptScriptTestId& OutId,
	uint64& OutGeneration)
{
	TArray<FString> Parts;
	Command.ParseIntoArray(Parts, TEXT("|"), false);
	if (Parts.Num() != 4 || !LexTryParseString(OutGeneration, *Parts[0]))
	{
		return false;
	}

	OutId.ModuleName = Parts[1];
	OutId.SuiteName = Parts[2];
	OutId.MethodName = Parts[3];
	return !OutId.ModuleName.IsEmpty()
		&& !OutId.SuiteName.IsEmpty()
		&& !OutId.MethodName.IsEmpty();
}

const FAngelscriptScriptTestDescriptor*
FAngelscriptScriptTestRegistrySnapshot::Find(
	const FAngelscriptScriptTestId& Id) const
{
	return Tests.FindByPredicate(
		[&Id](const FAngelscriptScriptTestDescriptor& Descriptor)
		{
			return Descriptor.Id == Id;
		});
}

FAngelscriptScriptTestRegistry::FAngelscriptScriptTestRegistry()
{
	TSharedRef<FAngelscriptScriptTestRegistrySnapshot> Initial =
		MakeShared<FAngelscriptScriptTestRegistrySnapshot>();
	Snapshot = Initial;
}

FAngelscriptScriptTestRegistry& FAngelscriptScriptTestRegistry::Get()
{
	static FAngelscriptScriptTestRegistry Registry;
	return Registry;
}

bool FAngelscriptScriptTestRegistry::ParseAutomationFlags(
	const FString& Value,
	EAutomationTestFlags& OutFlags,
	FString& OutError)
{
	OutFlags = EAutomationTestFlags::None;
	OutError.Reset();

	if (Value.IsEmpty())
	{
		OutError = TEXT("AngelscriptTestFlags cannot be empty.");
		return false;
	}

	TArray<FString> Tokens;
	Value.ParseIntoArray(Tokens, TEXT(";"), false);
	TSet<FString> SeenTokens;
	for (FString Token : Tokens)
	{
		Token.TrimStartAndEndInline();
		if (Token.IsEmpty())
		{
			OutError = TEXT("AngelscriptTestFlags contains an empty token.");
			return false;
		}
		if (SeenTokens.Contains(Token))
		{
			OutError = FString::Printf(
				TEXT("AngelscriptTestFlags contains duplicate token '%s'."),
				*Token);
			return false;
		}

		const EAutomationTestFlags* Flag = GetAutomationFlagsByName().Find(Token);
		if (Flag == nullptr)
		{
			OutError = FString::Printf(
				TEXT("AngelscriptTestFlags contains unknown token '%s'."),
				*Token);
			return false;
		}

		SeenTokens.Add(Token);
		OutFlags |= *Flag;
	}

	if (!EnumHasAnyFlags(OutFlags, EAutomationTestFlags_ApplicationContextMask))
	{
		OutError =
			TEXT("AngelscriptTestFlags requires at least one Application Context flag.");
		return false;
	}

	const uint32 FilterBits = static_cast<uint32>(
		OutFlags & EAutomationTestFlags_FilterMask);
	if (FPlatformMath::CountBits(FilterBits) != 1)
	{
		OutError =
			TEXT("AngelscriptTestFlags requires exactly one Filter flag.");
		return false;
	}

	return true;
}

FAngelscriptScriptTestRegistryBuildResult
FAngelscriptScriptTestRegistry::BuildSnapshot(
	const TArray<TSharedRef<FAngelscriptModuleDesc>>& ActiveModules,
	uint64 Generation,
	bool bDiscoveryEnabled)
{
	FAngelscriptScriptTestRegistryBuildResult Result;
	TSharedRef<FAngelscriptScriptTestRegistrySnapshot> MutableSnapshot =
		MakeShared<FAngelscriptScriptTestRegistrySnapshot>();
	MutableSnapshot->Generation = Generation;
	Result.Snapshot = MutableSnapshot;

	if (!bDiscoveryEnabled)
	{
		return Result;
	}

	TMap<FString, TSharedRef<FAngelscriptClassDesc>> ClassesByName;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
		{
			ClassesByName.Add(Class->ClassName, Class);
		}
	}

	TArray<FAngelscriptScriptTestDescriptor> Candidates;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		const FString ModuleSourceFile = GetSourceFile(*Module);
		for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
		{
			if (Class->bIsStaticsClass
				|| Class->bIsStruct
				|| Class->bAbstract
				|| !IsDerivedFromSuite(Class, ClassesByName))
			{
				continue;
			}

			const FString* FlagsValue = Class->Meta.Find(AngelscriptTestFlagsMetaName);
			EAutomationTestFlags Flags = EAutomationTestFlags::None;
			FString FlagsError;
			const bool bValidFlags = FlagsValue != nullptr
				&& ParseAutomationFlags(*FlagsValue, Flags, FlagsError);
			if (!bValidFlags)
			{
				if (FlagsValue == nullptr)
				{
					FlagsError =
						TEXT("Concrete script test suites require UCLASS meta AngelscriptTestFlags.");
				}
				AddDiagnostic(
					Result,
					ModuleSourceFile,
					Class->LineNumber + 1,
					FString::Printf(TEXT("%s: %s"), *Class->ClassName, *FlagsError));
			}

			for (const TSharedRef<FAngelscriptFunctionDesc>& Method : Class->Methods)
			{
				if (!Method->Meta.Contains(AngelscriptTestMetaName))
				{
					continue;
				}

				bool bValidMethod = true;
				if (Method->bIsStatic)
				{
					AddDiagnostic(
						Result,
						GetSourceFile(*Module, &*Method),
						Method->LineNumber + 1,
						FString::Printf(
							TEXT("%s::%s must be a non-static instance method."),
							*Class->ClassName,
							*Method->FunctionName));
					bValidMethod = false;
				}
				if (Method->Arguments.Num() != 0)
				{
					AddDiagnostic(
						Result,
						GetSourceFile(*Module, &*Method),
						Method->LineNumber + 1,
						FString::Printf(
							TEXT("%s::%s must not declare parameters."),
							*Class->ClassName,
							*Method->FunctionName));
					bValidMethod = false;
				}
				if (Method->ReturnType.IsValid())
				{
					AddDiagnostic(
						Result,
						GetSourceFile(*Module, &*Method),
						Method->LineNumber + 1,
						FString::Printf(
							TEXT("%s::%s must return void."),
							*Class->ClassName,
							*Method->FunctionName));
					bValidMethod = false;
				}

				if (!bValidFlags || !bValidMethod)
				{
					continue;
				}

				FAngelscriptScriptTestDescriptor& Descriptor =
					Candidates.AddDefaulted_GetRef();
				Descriptor.Id.ModuleName = Module->ModuleName;
				Descriptor.Id.SuiteName = Class->ClassName;
				Descriptor.Id.MethodName = Method->FunctionName;
				Descriptor.DisplayName = FString::Printf(
					TEXT("Angelscript.ScriptTests.%s.%s.%s"),
					*Descriptor.Id.ModuleName,
					*Descriptor.Id.SuiteName,
					*Descriptor.Id.MethodName);
				Descriptor.Flags = Flags;
				Descriptor.Generation = Generation;
				Descriptor.SourceFile = GetSourceFile(*Module, &*Method);
				Descriptor.SourceLine = FMath::Max(1, Method->LineNumber + 1);
			}
		}
	}

	TMap<FString, TArray<int32>> CollisionGroups;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		CollisionGroups.FindOrAdd(
			NormalizeAutomationName(Candidates[Index].DisplayName)).Add(Index);
	}

	for (const TPair<FString, TArray<int32>>& Group : CollisionGroups)
	{
		if (Group.Value.Num() <= 1)
		{
			MutableSnapshot->Tests.Add(Candidates[Group.Value[0]]);
			continue;
		}

		for (int32 CandidateIndex : Group.Value)
		{
			const FAngelscriptScriptTestDescriptor& Descriptor =
				Candidates[CandidateIndex];
			AddDiagnostic(
				Result,
				Descriptor.SourceFile,
				Descriptor.SourceLine,
				FString::Printf(
					TEXT("Script test Automation name collides after normalization: %s"),
					*Descriptor.DisplayName));
		}
	}

	Algo::Sort(
		MutableSnapshot->Tests,
		[](const FAngelscriptScriptTestDescriptor& Left,
		   const FAngelscriptScriptTestDescriptor& Right)
		{
			if (Left.Id.ModuleName != Right.Id.ModuleName)
			{
				return Left.Id.ModuleName < Right.Id.ModuleName;
			}
			if (Left.Id.SuiteName != Right.Id.SuiteName)
			{
				return Left.Id.SuiteName < Right.Id.SuiteName;
			}
			if (Left.SourceLine != Right.SourceLine)
			{
				return Left.SourceLine < Right.SourceLine;
			}
			return Left.Id.MethodName < Right.Id.MethodName;
		});

	return Result;
}

FAngelscriptScriptTestRegistryBuildResult
FAngelscriptScriptTestRegistry::Rebuild(
	const TArray<TSharedRef<FAngelscriptModuleDesc>>& ActiveModules,
	bool bDiscoveryEnabled)
{
	check(IsInGameThread());

	const uint64 Generation = NextGeneration++;
	FAngelscriptScriptTestRegistryBuildResult Result =
		BuildSnapshot(ActiveModules, Generation, bDiscoveryEnabled);

	{
		FWriteScopeLock Lock(SnapshotLock);
		Snapshot = Result.Snapshot;
	}

	FAngelscriptScriptTestAutomation::Get().EnsureBridges(*Result.Snapshot);
	RegistryChanged.Broadcast(Generation);
	return Result;
}

TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot>
FAngelscriptScriptTestRegistry::GetSnapshot() const
{
	FReadScopeLock Lock(SnapshotLock);
	return Snapshot;
}

uint64 FAngelscriptScriptTestRegistry::GetGeneration() const
{
	TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Current =
		GetSnapshot();
	return Current.IsValid() ? Current->Generation : 0;
}
