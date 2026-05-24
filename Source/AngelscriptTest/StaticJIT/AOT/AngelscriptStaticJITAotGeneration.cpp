#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"

#include "Shared/AngelscriptTestEngineHelper.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/PrecompiledData.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AngelscriptStaticJITAotGeneration
{
	namespace
	{
		bool CompileFixture(FAngelscriptEngine& Engine, FStaticJITAotGenerationResult& Result)
		{
			const bool bCompiled = CompileAnnotatedModuleFromMemory(
				&Engine,
				AngelscriptStaticJITAotFixture::GetModuleName(),
				AngelscriptStaticJITAotFixture::GetSourceFilename(),
				AngelscriptStaticJITAotFixture::GetScriptSource());
			if (!bCompiled)
			{
				Result.Error = TEXT("StaticJIT AOT fixture failed to compile.");
				return false;
			}

			return true;
		}

		FString GetVerifyCacheFilename()
		{
			return FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("StaticJIT/AOT/Verify"),
				TEXT("StaticJITAotFixture.Cache"));
		}

		bool GenerateFilesAndCache(FAngelscriptEngine& Engine, const FString& CacheFilename, TMap<FString, FString>& OutGeneratedFiles, FStaticJITAotGenerationResult& Result)
		{
			TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
			if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
			{
				Result.Error = TEXT("StaticJIT AOT fixture module was not available after compile.");
				return false;
			}

			const FString CacheDirectory = FPaths::GetPath(CacheFilename);
			if (!CacheDirectory.IsEmpty())
			{
				IFileManager::Get().MakeDirectory(*CacheDirectory, true);
			}

			FString GenerationError;
			if (!GenerateStaticJITAotArtifactsForDiagnostics(
				ModuleDesc->ScriptModule,
				CacheFilename,
				AngelscriptStaticJITAotFixture::GetPrecompiledDataGuid(),
				OutGeneratedFiles,
				/*bEmitDebugMetadata=*/true,
				/*bEmitDiagnosticEntryMarkers=*/true,
				&GenerationError))
			{
				Result.Error = GenerationError;
				return false;
			}

			IFileManager::Get().MakeDirectory(*AngelscriptStaticJITAotFixture::GetGeneratedDirectory(), true);
			return true;
		}

		bool WriteGeneratedFiles(const TMap<FString, FString>& GeneratedFiles, FStaticJITAotGenerationResult& Result)
		{
			const FString GeneratedDirectory = AngelscriptStaticJITAotFixture::GetGeneratedDirectory();
			IFileManager::Get().MakeDirectory(*GeneratedDirectory, true);

			for (const TPair<FString, FString>& GeneratedFile : GeneratedFiles)
			{
				const FString OutputPath = FPaths::Combine(GeneratedDirectory, GeneratedFile.Key);
				if (!FFileHelper::SaveStringToFile(GeneratedFile.Value, *OutputPath))
				{
					Result.Error = FString::Printf(TEXT("Failed to write generated StaticJIT AOT file: %s"), *OutputPath);
					return false;
				}
				Result.WrittenFiles.Add(OutputPath);
			}

			Result.WrittenFiles.Add(AngelscriptStaticJITAotFixture::GetPrecompiledCacheFilename());
			return true;
		}

		bool IsSameArchiveString(const FStringInArchive& Left, const FStringInArchive& Right)
		{
			return Left.UnrealString() == Right.UnrealString();
		}

		template <typename StringArrayType>
		bool IsSameArchiveStringArray(const StringArrayType& Left, const StringArrayType& Right)
		{
			if (Left.Num() != Right.Num())
			{
				return false;
			}

			for (int32 Index = 0; Index < Left.Num(); ++Index)
			{
				if (!IsSameArchiveString(Left[Index], Right[Index]))
				{
					return false;
				}
			}

			return true;
		}

		bool IsSamePrecompiledDataType(const FAngelscriptPrecompiledDataType& Left, const FAngelscriptPrecompiledDataType& Right)
		{
			return Left.bIsReference == Right.bIsReference
				&& Left.bIsObjectConst == Right.bIsObjectConst
				&& Left.bIsObjectHandle == Right.bIsObjectHandle
				&& Left.bIsConstHandle == Right.bIsConstHandle
				&& Left.bIsAuto == Right.bIsAuto
				&& Left.bIfHandleThenConst == Right.bIfHandleThenConst
				&& Left.TokenType == Right.TokenType
				&& Left.TypeInfo.IsNull() == Right.TypeInfo.IsNull();
		}

		template <typename TypeArrayType>
		bool IsSamePrecompiledDataTypeArray(const TypeArrayType& Left, const TypeArrayType& Right)
		{
			if (Left.Num() != Right.Num())
			{
				return false;
			}

			for (int32 Index = 0; Index < Left.Num(); ++Index)
			{
				if (!IsSamePrecompiledDataType(Left[Index], Right[Index]))
				{
					return false;
				}
			}

			return true;
		}

		bool IsSamePrecompiledFunction(const FAngelscriptPrecompiledFunction& Left, const FAngelscriptPrecompiledFunction& Right)
		{
			return IsSameArchiveString(Left.FunctionName, Right.FunctionName)
				&& IsSameArchiveString(Left.Namespace, Right.Namespace)
				&& IsSamePrecompiledDataType(Left.ReturnType, Right.ReturnType)
				&& IsSamePrecompiledDataTypeArray(Left.ParameterTypes, Right.ParameterTypes)
				&& IsSameArchiveStringArray(Left.ParameterNames, Right.ParameterNames)
				&& Left.ParameterFlags == Right.ParameterFlags
				&& IsSameArchiveStringArray(Left.ParameterDefaultArgs, Right.ParameterDefaultArgs)
				&& Left.FunctionTraits == Right.FunctionTraits
				&& Left.VariableSpace == Right.VariableSpace
				&& Left.ObjVariablePos == Right.ObjVariablePos
				&& Left.ObjVariablesOnHeap == Right.ObjVariablesOnHeap
				&& Left.VariableInfoProgramPos == Right.VariableInfoProgramPos
				&& Left.VariableInfoOffset == Right.VariableInfoOffset
				&& Left.VariableInfoOption == Right.VariableInfoOption
				&& Left.StackNeeded == Right.StackNeeded
				&& Left.Id == Right.Id
				&& Left.bIsUFunction == Right.bIsUFunction;
		}

		const FAngelscriptPrecompiledFunction* FindFunctionByName(const FAngelscriptPrecompiledModule& Module, const FString& FunctionName)
		{
			for (const FAngelscriptPrecompiledFunction& Function : Module.Functions)
			{
				if (Function.FunctionName.UnrealString() == FunctionName)
				{
					return &Function;
				}
			}

			return nullptr;
		}

		const FAngelscriptPrecompiledClass* FindClassByName(const FAngelscriptPrecompiledModule& Module, const FString& ClassName)
		{
			for (const FAngelscriptPrecompiledClass& Class : Module.Classes)
			{
				if (Class.ClassName.UnrealString() == ClassName)
				{
					return &Class;
				}
			}

			return nullptr;
		}

		const FAngelscriptPrecompiledFunction* FindMethodByName(const FAngelscriptPrecompiledClass& Class, const FString& FunctionName)
		{
			for (const FAngelscriptPrecompiledFunction& Method : Class.Methods)
			{
				if (Method.FunctionName.UnrealString() == FunctionName)
				{
					return &Method;
				}
			}

			return nullptr;
		}

		bool VerifyPrecompiledCacheSemantics(FAngelscriptEngine& Engine, const FString& LocalCacheFilename, const FString& GeneratedCacheFilename, FString& OutError)
		{
			if (!IFileManager::Get().FileExists(*LocalCacheFilename))
			{
				OutError = FString::Printf(
					TEXT("StaticJIT AOT local precompiled cache is missing: %s\n%s"),
					*LocalCacheFilename,
					*AngelscriptStaticJITAotFixture::GetGeneratedSetupInstructions());
				return false;
			}

			if (!IFileManager::Get().FileExists(*GeneratedCacheFilename))
			{
				OutError = FString::Printf(TEXT("StaticJIT AOT generated verification cache is missing: %s"), *GeneratedCacheFilename);
				return false;
			}

			FAngelscriptPrecompiledData LocalCache(Engine.GetScriptEngine());
			FAngelscriptPrecompiledData GeneratedCache(Engine.GetScriptEngine());
			LocalCache.Load(LocalCacheFilename);
			GeneratedCache.Load(GeneratedCacheFilename);

			const FGuid& ExpectedGuid = AngelscriptStaticJITAotFixture::GetPrecompiledDataGuid();
			if (LocalCache.DataGuid != ExpectedGuid || GeneratedCache.DataGuid != ExpectedGuid)
			{
				OutError = TEXT("StaticJIT AOT cache GUID does not match the fixture GUID.");
				return false;
			}

			if (!LocalCache.IsValidForCurrentBuild() || !GeneratedCache.IsValidForCurrentBuild())
			{
				OutError = TEXT("StaticJIT AOT cache build identifier does not match the current build.");
				return false;
			}

			const FString ModuleName = AngelscriptStaticJITAotFixture::GetModuleName().ToString();
			const FAngelscriptPrecompiledModule* LocalModule = LocalCache.Modules.Find(ModuleName);
			const FAngelscriptPrecompiledModule* GeneratedModule = GeneratedCache.Modules.Find(ModuleName);
			if (LocalModule == nullptr || GeneratedModule == nullptr)
			{
				OutError = FString::Printf(TEXT("StaticJIT AOT cache does not contain fixture module '%s'."), *ModuleName);
				return false;
			}

			if (!IsSameArchiveString(LocalModule->ModuleName, GeneratedModule->ModuleName)
				|| LocalModule->Functions.Num() != GeneratedModule->Functions.Num()
				|| LocalModule->Classes.Num() != GeneratedModule->Classes.Num()
				|| LocalModule->Enums.Num() != GeneratedModule->Enums.Num()
				|| LocalModule->GlobalVariables.Num() != GeneratedModule->GlobalVariables.Num()
				|| LocalModule->FunctionImports.Num() != GeneratedModule->FunctionImports.Num()
				|| LocalModule->CodeHash != GeneratedModule->CodeHash
				|| !IsSameArchiveStringArray(LocalModule->ImportedModules, GeneratedModule->ImportedModules)
				|| !IsSameArchiveString(LocalModule->StaticsClassName, GeneratedModule->StaticsClassName)
				|| !IsSameArchiveStringArray(LocalModule->DeclaredEvents, GeneratedModule->DeclaredEvents)
				|| !IsSameArchiveStringArray(LocalModule->DeclaredDelegates, GeneratedModule->DeclaredDelegates)
				|| !IsSameArchiveString(LocalModule->ScriptRelativeFilename, GeneratedModule->ScriptRelativeFilename)
				|| !IsSameArchiveStringArray(LocalModule->PostInitFunctions, GeneratedModule->PostInitFunctions))
			{
				OutError = TEXT("StaticJIT AOT cache fixture module metadata differs from regenerated output.");
				return false;
			}

			const TArray<FString> RequiredFunctions =
			{
				TEXT("AddForAOT"),
				TEXT("Entry"),
				TEXT("StaticWorldContextCheck"),
			};

			for (const FString& FunctionName : RequiredFunctions)
			{
				const FAngelscriptPrecompiledFunction* LocalFunction = FindFunctionByName(*LocalModule, FunctionName);
				const FAngelscriptPrecompiledFunction* GeneratedFunction = FindFunctionByName(*GeneratedModule, FunctionName);
				if (LocalFunction == nullptr || GeneratedFunction == nullptr)
				{
					OutError = FString::Printf(TEXT("StaticJIT AOT cache does not contain fixture function '%s'."), *FunctionName);
					return false;
				}

				if (!IsSamePrecompiledFunction(*LocalFunction, *GeneratedFunction))
				{
					OutError = FString::Printf(TEXT("StaticJIT AOT cache fixture function '%s' differs from regenerated output."), *FunctionName);
					return false;
				}
			}

			const FString RequiredClassName = AngelscriptStaticJITAotFixture::GetGeneratedClassName().ToString();
			const FAngelscriptPrecompiledClass* LocalClass = FindClassByName(*LocalModule, RequiredClassName);
			const FAngelscriptPrecompiledClass* GeneratedClass = FindClassByName(*GeneratedModule, RequiredClassName);
			if (LocalClass == nullptr || GeneratedClass == nullptr)
			{
				OutError = FString::Printf(TEXT("StaticJIT AOT cache does not contain fixture class '%s'."), *RequiredClassName);
				return false;
			}

			const TArray<FString> RequiredMethods =
			{
				TEXT("StorePrimitiveArg"),
				TEXT("ReturnPrimitive"),
				TEXT("BumpReference"),
				TEXT("ReturnSelfObject"),
			};

			for (const FString& MethodName : RequiredMethods)
			{
				const FAngelscriptPrecompiledFunction* LocalMethod = FindMethodByName(*LocalClass, MethodName);
				const FAngelscriptPrecompiledFunction* GeneratedMethod = FindMethodByName(*GeneratedClass, MethodName);
				if (LocalMethod == nullptr || GeneratedMethod == nullptr)
				{
					OutError = FString::Printf(TEXT("StaticJIT AOT cache does not contain fixture method '%s::%s'."), *RequiredClassName, *MethodName);
					return false;
				}

				if (!IsSamePrecompiledFunction(*LocalMethod, *GeneratedMethod))
				{
					OutError = FString::Printf(TEXT("StaticJIT AOT cache fixture method '%s::%s' differs from regenerated output."), *RequiredClassName, *MethodName);
					return false;
				}
			}

			return true;
		}

		void NormalizeConstructorArguments(FString& Content, const FString& Prefix, bool bOnlyFirstArgument)
		{
			int32 SearchIndex = 0;
			while (SearchIndex < Content.Len())
			{
				const int32 PrefixIndex = Content.Find(Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex);
				if (PrefixIndex == INDEX_NONE)
				{
					break;
				}

				const int32 OpenParenIndex = Content.Find(TEXT("("), ESearchCase::CaseSensitive, ESearchDir::FromStart, PrefixIndex + Prefix.Len());
				const int32 CloseParenIndex = OpenParenIndex == INDEX_NONE
					? INDEX_NONE
					: Content.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenParenIndex + 1);
				if (OpenParenIndex == INDEX_NONE || CloseParenIndex == INDEX_NONE)
				{
					SearchIndex = PrefixIndex + Prefix.Len();
					continue;
				}

				int32 ReplaceEndIndex = CloseParenIndex;
				if (bOnlyFirstArgument)
				{
					const int32 CommaIndex = Content.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenParenIndex + 1);
					ReplaceEndIndex = CommaIndex == INDEX_NONE || CommaIndex > CloseParenIndex
						? INDEX_NONE
						: CommaIndex;
				}

				if (ReplaceEndIndex == INDEX_NONE)
				{
					SearchIndex = PrefixIndex + Prefix.Len();
					continue;
				}

				const FString StablePlaceholder = TEXT("STATIC_JIT_REFERENCE");
				Content.RemoveAt(OpenParenIndex + 1, ReplaceEndIndex - OpenParenIndex - 1, EAllowShrinking::No);
				Content.InsertAt(OpenParenIndex + 1, StablePlaceholder);
				SearchIndex = OpenParenIndex + StablePlaceholder.Len() + 2;
			}
		}

		FString NormalizeGeneratedFileForComparison(const FString& Filename, const FString& Content)
		{
			if (!Filename.EndsWith(TEXT(".jit.hpp")))
			{
				return Content;
			}

			FString Normalized = Content;
			const TArray<FString> SingleArgumentRefPrefixes =
			{
				TEXT("AS_FORCE_LINK FJitRef_Function "),
				TEXT("AS_FORCE_LINK FJitRef_SystemFunctionPointer "),
				TEXT("AS_FORCE_LINK FJitRef_Type "),
				TEXT("AS_FORCE_LINK FJitRef_GlobalVar "),
			};

			for (const FString& Prefix : SingleArgumentRefPrefixes)
			{
				NormalizeConstructorArguments(Normalized, Prefix, /*bOnlyFirstArgument=*/false);
			}

			const TArray<FString> FirstArgumentRefPrefixes =
			{
				TEXT("AS_FORCE_LINK FJitVerifyPropertyOffset "),
				TEXT("AS_FORCE_LINK FJitVerifyTypeSize "),
			};

			for (const FString& Prefix : FirstArgumentRefPrefixes)
			{
				NormalizeConstructorArguments(Normalized, Prefix, /*bOnlyFirstArgument=*/true);
			}

			return Normalized;
		}

		bool VerifyGeneratedFiles(FAngelscriptEngine& Engine, const TMap<FString, FString>& GeneratedFiles, const FString& GeneratedCacheFilename, FStaticJITAotGenerationResult& Result)
		{
			const FString GeneratedDirectory = AngelscriptStaticJITAotFixture::GetGeneratedDirectory();
			for (const TPair<FString, FString>& GeneratedFile : GeneratedFiles)
			{
				const FString OutputPath = FPaths::Combine(GeneratedDirectory, GeneratedFile.Key);
				FString ExistingContent;
				if (!FFileHelper::LoadFileToString(ExistingContent, *OutputPath)
					|| NormalizeGeneratedFileForComparison(GeneratedFile.Key, ExistingContent) != NormalizeGeneratedFileForComparison(GeneratedFile.Key, GeneratedFile.Value))
				{
					Result.StaleFiles.Add(OutputPath);
				}
			}

			const FString LocalCacheFilename = AngelscriptStaticJITAotFixture::GetPrecompiledCacheFilename();
			FString CacheError;
			if (!VerifyPrecompiledCacheSemantics(Engine, LocalCacheFilename, GeneratedCacheFilename, CacheError))
			{
				Result.StaleFiles.Add(LocalCacheFilename);
				if (!CacheError.IsEmpty())
				{
					Result.Error = CacheError;
				}
			}

			if (Result.StaleFiles.Num() > 0)
			{
				const FString Detail = Result.Error.IsEmpty()
					? FString()
					: FString::Printf(TEXT(" (%s)"), *Result.Error);
				Result.Error = FString::Printf(TEXT("StaticJIT AOT generated files are stale or incomplete: %s%s"), *FString::Join(Result.StaleFiles, TEXT(", ")), *Detail);
				return false;
			}

			return true;
		}
	}

	FStaticJITAotGenerationResult Run(EStaticJITAotGenerationMode Mode)
	{
		FStaticJITAotGenerationResult Result;

		TUniquePtr<FAngelscriptEngine> Engine = CreateIsolatedFullEngine();
		if (!Engine.IsValid())
		{
			Result.Error = TEXT("Failed to create StaticJIT AOT generation engine.");
			return Result;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		if (!CompileFixture(*Engine, Result))
		{
			return Result;
		}

		const FString CacheFilename = Mode == EStaticJITAotGenerationMode::Generate
			? AngelscriptStaticJITAotFixture::GetPrecompiledCacheFilename()
			: GetVerifyCacheFilename();

		TMap<FString, FString> GeneratedFiles;
		if (!GenerateFilesAndCache(*Engine, CacheFilename, GeneratedFiles, Result))
		{
			return Result;
		}

		Result.bSuccess = Mode == EStaticJITAotGenerationMode::Generate
			? WriteGeneratedFiles(GeneratedFiles, Result)
			: VerifyGeneratedFiles(*Engine, GeneratedFiles, CacheFilename, Result);
		return Result;
	}
}
