#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace AngelscriptStaticJITAotFixture
{
	namespace
	{
		bool SetGeneratedOutputError(FString* OutError, const FString& Detail)
		{
			if (OutError != nullptr)
			{
				*OutError = FString::Printf(
					TEXT("%s\nStaticJIT AOT test artifacts are generated locally; run the setup workflow before running the AOT tests:\n%s"),
					*Detail,
					*GetGeneratedSetupInstructions());
			}
			return false;
		}
	}

	const FName& GetModuleName()
	{
		static const FName ModuleName(TEXT("ASStaticJITAotFixture"));
		return ModuleName;
	}

	const FString& GetSourceFilename()
	{
		static const FString SourceFilename(TEXT("ASStaticJITAotFixture.as"));
		return SourceFilename;
	}

	const FString& GetScriptSource()
	{
		static const FString ScriptSource =
			TEXT("int AddForAOT(int Value)\n")
			TEXT("{\n")
			TEXT("\treturn Value + 7;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("\treturn AddForAOT(35);\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("UCLASS()\n")
			TEXT("class UStaticJITAotFunctionCarrier : UObject\n")
			TEXT("{\n")
			TEXT("\tUPROPERTY()\n")
			TEXT("\tint StoredValue = 0;\n")
			TEXT("\n")
			TEXT("\tUFUNCTION()\n")
			TEXT("\tvoid StorePrimitiveArg(int Value)\n")
			TEXT("\t{\n")
			TEXT("\t\tStoredValue = Value + 3;\n")
			TEXT("\t}\n")
			TEXT("\n")
			TEXT("\tUFUNCTION()\n")
			TEXT("\tint ReturnPrimitive()\n")
			TEXT("\t{\n")
			TEXT("\t\treturn 61;\n")
			TEXT("\t}\n")
			TEXT("\n")
			TEXT("\tUFUNCTION()\n")
			TEXT("\tint BumpReference(int& Value)\n")
			TEXT("\t{\n")
			TEXT("\t\tValue += 5;\n")
			TEXT("\t\tStoredValue = Value;\n")
			TEXT("\t\treturn Value;\n")
			TEXT("\t}\n")
			TEXT("\n")
			TEXT("\tUFUNCTION()\n")
			TEXT("\tUObject ReturnSelfObject()\n")
			TEXT("\t{\n")
			TEXT("\t\treturn this;\n")
			TEXT("\t}\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("UFUNCTION(BlueprintCallable, meta = (WorldContext = \"WorldContextObject\"))\n")
			TEXT("int StaticWorldContextCheck(UObject WorldContextObject, int Value)\n")
			TEXT("{\n")
			TEXT("\tif (__WorldContext() != WorldContextObject)\n")
			TEXT("\t\treturn -1;\n")
			TEXT("\treturn Value + 2;\n")
			TEXT("}\n");
		return ScriptSource;
	}

	const FName& GetGeneratedClassName()
	{
		static const FName ClassName(TEXT("UStaticJITAotFunctionCarrier"));
		return ClassName;
	}

	const FString& GetEntryDeclaration()
	{
		static const FString EntryDeclaration(TEXT("int Entry()"));
		return EntryDeclaration;
	}

	const FString& GetMethodPrimitiveArgDeclaration()
	{
		static const FString Declaration(TEXT("void StorePrimitiveArg(int)"));
		return Declaration;
	}

	const FString& GetMethodPrimitiveReturnDeclaration()
	{
		static const FString Declaration(TEXT("int ReturnPrimitive()"));
		return Declaration;
	}

	const FString& GetMethodReferenceDeclaration()
	{
		static const FString Declaration(TEXT("int BumpReference(int&inout)"));
		return Declaration;
	}

	const FString& GetMethodObjectReturnDeclaration()
	{
		static const FString Declaration(TEXT("UObject@ ReturnSelfObject()"));
		return Declaration;
	}

	const FString& GetStaticWorldContextDeclaration()
	{
		static const FString Declaration(TEXT("int StaticWorldContextCheck(UObject, int)"));
		return Declaration;
	}

	const FGuid& GetPrecompiledDataGuid()
	{
		static const FGuid Guid(0x2f90f2a1, 0x6df34d23, 0x9bc4474a, 0xa05de08c);
		return Guid;
	}

	int32 GetExpectedEntryResult()
	{
		return 42;
	}

	int32 GetExpectedPrimitiveArgStoredValue()
	{
		return 44;
	}

	int32 GetExpectedPrimitiveReturnValue()
	{
		return 61;
	}

	int32 GetExpectedReferenceReturnValue()
	{
		return 18;
	}

	int32 GetExpectedStaticWorldContextResult()
	{
		return 33;
	}

	FString GetGeneratedDirectory()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("Plugins/Angelscript/Source/AngelscriptTest/StaticJIT/AOT/Generated")));
	}

	FString GetPrecompiledCacheFilename()
	{
		return FPaths::Combine(GetGeneratedDirectory(), TEXT("StaticJITAotFixture.Cache"));
	}

	const FString& GetGeneratedSetupInstructions()
	{
		static const FString Instructions =
			TEXT("powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\\RunStaticJITTests.ps1 -LabelPrefix staticjit-aot\n")
			TEXT("The AOT cache and generated .jit.cpp/.jit.hpp files are a matched build pair; do not regenerate the cache at runtime or skip the generated-source rebuild.");
		return Instructions;
	}

	bool IsGeneratedOutputAvailable(FString* OutError)
	{
		const FString GeneratedDirectory = GetGeneratedDirectory();
		if (!IFileManager::Get().DirectoryExists(*GeneratedDirectory))
		{
			return SetGeneratedOutputError(OutError, FString::Printf(TEXT("StaticJIT AOT generated directory does not exist: %s"), *GeneratedDirectory));
		}

		const TArray<FString> RequiredGeneratedFiles =
		{
			TEXT("ASStaticJITAotFixture.as.jit.hpp"),
			TEXT("AngelscriptJitCode_0.jit.cpp"),
			TEXT("AngelscriptJitInfo.jit.cpp"),
		};

		for (const FString& RequiredGeneratedFile : RequiredGeneratedFiles)
		{
			const FString GeneratedFile = FPaths::Combine(GeneratedDirectory, RequiredGeneratedFile);
			if (!IFileManager::Get().FileExists(*GeneratedFile))
			{
				return SetGeneratedOutputError(OutError, FString::Printf(TEXT("StaticJIT AOT generated file does not exist: %s"), *GeneratedFile));
			}
		}

		const FString PrecompiledCache = GetPrecompiledCacheFilename();
		if (!IFileManager::Get().FileExists(*PrecompiledCache))
		{
			return SetGeneratedOutputError(OutError, FString::Printf(TEXT("StaticJIT AOT local precompiled cache does not exist: %s"), *PrecompiledCache));
		}

		return true;
	}
}
