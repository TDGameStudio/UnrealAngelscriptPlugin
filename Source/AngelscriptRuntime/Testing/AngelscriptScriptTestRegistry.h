#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

struct FAngelscriptModuleDesc;

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestId
{
	FString ModuleName;
	FString SuiteName;
	FString MethodName;

	bool operator==(const FAngelscriptScriptTestId& Other) const
	{
		return ModuleName == Other.ModuleName
			&& SuiteName == Other.SuiteName
			&& MethodName == Other.MethodName;
	}

	FString ToCommandString(uint64 Generation) const;
	static bool TryParseCommandString(
		const FString& Command,
		FAngelscriptScriptTestId& OutId,
		uint64& OutGeneration);
};

FORCEINLINE uint32 GetTypeHash(const FAngelscriptScriptTestId& Id)
{
	return HashCombine(
		HashCombine(GetTypeHash(Id.ModuleName), GetTypeHash(Id.SuiteName)),
		GetTypeHash(Id.MethodName));
}

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestDescriptor
{
	FAngelscriptScriptTestId Id;
	FString DisplayName;
	EAutomationTestFlags Flags = EAutomationTestFlags::None;
	uint64 Generation = 0;
	FString SourceFile;
	int32 SourceLine = 1;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestDiagnostic
{
	FString SourceFile;
	int32 SourceLine = 1;
	FString Message;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestRegistrySnapshot
{
	uint64 Generation = 0;
	TArray<FAngelscriptScriptTestDescriptor> Tests;

	const FAngelscriptScriptTestDescriptor* Find(
		const FAngelscriptScriptTestId& Id) const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestRegistryBuildResult
{
	TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot;
	TArray<FAngelscriptScriptTestDiagnostic> Diagnostics;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FAngelscriptScriptTestRegistryChanged,
	uint64);

/**
 * Immutable, generation-tagged registry of reflected script test methods.
 *
 * Descriptors intentionally contain no UClass, UFunction, or AngelScript
 * executable pointer. Runners resolve the current class and method from the
 * stable ID immediately before invocation.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestRegistry
{
public:
	static FAngelscriptScriptTestRegistry& Get();

	static bool ParseAutomationFlags(
		const FString& Value,
		EAutomationTestFlags& OutFlags,
		FString& OutError);

	static FAngelscriptScriptTestRegistryBuildResult BuildSnapshot(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& ActiveModules,
		uint64 Generation,
		bool bDiscoveryEnabled);

	FAngelscriptScriptTestRegistryBuildResult Rebuild(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>& ActiveModules,
		bool bDiscoveryEnabled);

	TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> GetSnapshot() const;
	uint64 GetGeneration() const;

	FAngelscriptScriptTestRegistryChanged& OnChanged()
	{
		return RegistryChanged;
	}

private:
	FAngelscriptScriptTestRegistry();

	mutable FRWLock SnapshotLock;
	TSharedPtr<const FAngelscriptScriptTestRegistrySnapshot> Snapshot;
	uint64 NextGeneration = 1;
	FAngelscriptScriptTestRegistryChanged RegistryChanged;
};
