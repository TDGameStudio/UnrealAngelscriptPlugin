#pragma once

#include "CoreMinimal.h"
#include "Core/AngelscriptEngine.h"
#include "StaticJIT/StaticJITConfig.h"

class asCScriptFunction;
class asIScriptFunction;
struct FAngelscriptPrecompiledData;

#if AS_WITH_STATIC_JIT_DIAGNOSTICS
struct ANGELSCRIPTRUNTIME_API FStaticJITDiagnostics
{
	struct FSnapshot
	{
		bool bHasCurrentEngine = false;
		bool bHasScriptEngine = false;
		bool bHasPrecompiledData = false;
		bool bHasCompiledInfo = false;
		bool bCompiledInfoMatchesPrecompiledData = false;
		int32 RegisteredFunctionCount = 0;
		int32 EntryCounterCount = 0;
		int32 FunctionLookupCount = 0;
		int32 SystemFunctionPointerLookupCount = 0;
		int32 GlobalVarLookupCount = 0;
		int32 TypeInfoLookupCount = 0;
		int32 PropertyOffsetLookupCount = 0;
		FGuid PrecompiledDataGuid;
		FGuid CompiledInfoGuid;
	};

	static bool LoadPrecompiledData(FAngelscriptEngine& Engine, const FString& Filename, FString* OutError = nullptr);
	static bool CompileLoadedPrecompiledData(FAngelscriptEngine& Engine, ECompileType CompileType, FString* OutError = nullptr);
	static bool ResolveFunctionId(const FAngelscriptEngine& Engine, asIScriptFunction* Function, uint32& OutId);
	static bool ResolveFunctionId(const FAngelscriptEngine& Engine, asCScriptFunction* Function, uint32& OutId);
	static bool IsFunctionRegistered(uint32 FunctionId);
	static bool HasJitFunction(asCScriptFunction* Function);
	static int32 GetEntryCount(uint32 FunctionId);
	static void ResetEntryCounters();
	static void MarkEntry(uint32 FunctionId);
	static int32 GetEntryCounterCount();
	static FSnapshot CaptureSnapshot(const FAngelscriptEngine* Engine = nullptr);
	static void DumpDiagnostics(const TArray<FString>& Args);

	static bool ReferenceGlobalVariableTwice(FAngelscriptPrecompiledData& Data, void* GlobalPtr, int64& OutFirstReference, int64& OutReusedReference, FString& OutFirstName, FString& OutReusedName);
	static bool ExerciseRepeatedGlobalReferenceLoad(FAngelscriptPrecompiledData& Data, const FString& CacheFilename, int64 GlobalReference, void*& OutFirstResolvedAddress, void*& OutSecondResolvedAddress, bool& bOutCacheClearedAfterLoad);

private:
	static asCScriptFunction* ResolveFunctionArgument(FAngelscriptEngine& Engine, const FString& FunctionArgument, FString& OutError);
};
#endif
