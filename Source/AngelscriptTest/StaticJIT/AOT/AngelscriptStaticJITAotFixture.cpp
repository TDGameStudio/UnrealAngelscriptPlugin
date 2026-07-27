#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"

#include "AngelscriptTestMacros.h"
#include "Core/FunctionCallers.h"
#include "Core/angelscript.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace AngelscriptStaticJITAotFixture
{
	namespace
	{
		struct FObjectLastNativeProbe
		{
			int32 Value = 0;
		};

		struct FObjectLastNativeObservation
		{
			int32 CallCount = 0;
			int32 Left = INDEX_NONE;
			int32 Right = INDEX_NONE;
			int32 ObjectValue = INDEX_NONE;
		};

		FObjectLastNativeObservation ObjectLastNativeObservation;

		void ConstructObjectLastNativeProbe(
			const int32 Left,
			const int32 Right,
			FObjectLastNativeProbe* Address)
		{
			new (Address) FObjectLastNativeProbe();
			Address->Value = Left * 1000 + Right;
			ObjectLastNativeObservation.CallCount++;
			ObjectLastNativeObservation.Left = Left;
			ObjectLastNativeObservation.Right = Right;
			ObjectLastNativeObservation.ObjectValue = Address->Value;
		}

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
		static const FString ScriptSource = ASTEST_AS(R"AS(
			int AddForAOT(int Value)
			{
				return Value + 7;
			}

			int Entry()
			{
				return AddForAOT(35);
			}

			int64 DoubleToInt64ForAOT(double Value)
			{
				return int64(Value);
			}

			uint64 DoubleToUint64ForAOT(double Value)
			{
				return uint64(Value);
			}

			int ObjectLastNativeForAOT()
			{
				FAotObjectLastProbe Value(39, 97);
				return Value.Value;
			}

			UCLASS()
			class UStaticJITAotFunctionCarrier : UObject
			{
				UPROPERTY()
				int StoredValue = 0;

				UFUNCTION()
				void StorePrimitiveArg(int Value)
				{
					StoredValue = Value + 3;
				}

				UFUNCTION()
				int ReturnPrimitive()
				{
					return 61;
				}

				UFUNCTION()
				int BumpReference(int& Value)
				{
					Value += 5;
					StoredValue = Value;
					return Value;
				}

				UFUNCTION()
				UObject ReturnSelfObject()
				{
					return this;
				}
			}

			UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"))
			int StaticWorldContextCheck(UObject WorldContextObject, int Value)
			{
				if (__WorldContext() != WorldContextObject)
				{
					return -1;
				}
				return Value + 2;
			}
			)AS");
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

	const FString& GetDoubleToInt64Declaration()
	{
		static const FString Declaration(TEXT("int64 DoubleToInt64ForAOT(double)"));
		return Declaration;
	}

	const FString& GetDoubleToUint64Declaration()
	{
		static const FString Declaration(TEXT("uint64 DoubleToUint64ForAOT(double)"));
		return Declaration;
	}

	const FString& GetObjectLastNativeEntryDeclaration()
	{
		static const FString Declaration(TEXT("int ObjectLastNativeForAOT()"));
		return Declaration;
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

	int32 GetExpectedObjectLastNativeResult()
	{
		return 39097;
	}

	bool RegisterObjectLastNativeSurface(asIScriptEngine& ScriptEngine)
	{
		if (ScriptEngine.GetTypeInfoByDecl("FAotObjectLastProbe") != nullptr)
		{
			return true;
		}

		const ASAutoCaller::FunctionCaller ConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructObjectLastNativeProbe);
		return ScriptEngine.RegisterObjectType(
				"FAotObjectLastProbe",
				sizeof(FObjectLastNativeProbe),
				asOBJ_VALUE
					| asOBJ_POD
					| asGetTypeTraits<FObjectLastNativeProbe>()
					| asOBJ_APP_CLASS_ALLINTS) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FAotObjectLastProbe",
				asBEHAVE_CONSTRUCT,
				"void f(int Left, int Right)",
				asFUNCTION(ConstructObjectLastNativeProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) >= 0
			&& ScriptEngine.RegisterObjectProperty(
				"FAotObjectLastProbe",
				"int Value",
				asOFFSET(FObjectLastNativeProbe, Value)) >= 0;
	}

	void ResetObjectLastNativeObservation()
	{
		ObjectLastNativeObservation = FObjectLastNativeObservation();
	}

	int32 GetObjectLastNativeCallCount()
	{
		return ObjectLastNativeObservation.CallCount;
	}

	int32 GetObjectLastNativeLeftSentinel()
	{
		return ObjectLastNativeObservation.Left;
	}

	int32 GetObjectLastNativeRightSentinel()
	{
		return ObjectLastNativeObservation.Right;
	}

	int32 GetObjectLastNativeObjectValue()
	{
		return ObjectLastNativeObservation.ObjectValue;
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
