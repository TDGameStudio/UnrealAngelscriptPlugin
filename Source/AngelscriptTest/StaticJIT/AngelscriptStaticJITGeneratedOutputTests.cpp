#include "Misc/AutomationTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITGeneratedOutputTests_Private
{
	constexpr TCHAR SourceFilename[] = TEXT("StaticJITGeneratedOutputDebugMetadata.as");
	const FName ModuleName(TEXT("ASStaticJITGeneratedOutputDebugMetadata"));

	FString MakeScriptSource()
	{
		return
			TEXT("int AddOne(int Value)\n")
			TEXT("{\n")
			TEXT("    return Value + 1;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int AddTwo(int Value)\n")
			TEXT("{\n")
			TEXT("    return Value + 2;\n")
			TEXT("}\n")
			TEXT("\n")
			TEXT("int Entry()\n")
			TEXT("{\n")
			TEXT("    int First = AddOne(1);\n")
			TEXT("    int Second = AddTwo(First);\n")
			TEXT("    return Second;\n")
			TEXT("}\n");
	}

	int32 FindScriptLineNumberContaining(const FString& ScriptSource, const FString& Needle)
	{
		TArray<FString> Lines;
		ScriptSource.ParseIntoArrayLines(Lines, false);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(Needle))
			{
				return Index + 1;
			}
		}

		return INDEX_NONE;
	}

	TArray<int32> ExtractGeneratedLineMarkers(const FString& GeneratedSource)
	{
		TArray<int32> LineMarkers;
		const FString Needle = TEXT("SCRIPT_DEBUG_CALLSTACK_LINE(");
		int32 SearchFrom = 0;
		while (SearchFrom < GeneratedSource.Len())
		{
			int32 MarkerStart = GeneratedSource.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (MarkerStart == INDEX_NONE)
			{
				break;
			}

			const int32 ValueStart = MarkerStart + Needle.Len();
			int32 ValueEnd = GeneratedSource.Find(TEXT(");"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
			if (ValueEnd == INDEX_NONE)
			{
				break;
			}

			const FString ValueText = GeneratedSource.Mid(ValueStart, ValueEnd - ValueStart);
			LineMarkers.Add(FCString::Atoi(*ValueText));
			SearchFrom = ValueEnd + 2;
		}

		return LineMarkers;
	}

	FString JoinLineMarkers(const TArray<int32>& LineMarkers)
	{
		TArray<FString> MarkerStrings;
		MarkerStrings.Reserve(LineMarkers.Num());
		for (int32 Marker : LineMarkers)
		{
			MarkerStrings.Add(LexToString(Marker));
		}

		return FString::Join(MarkerStrings, TEXT(", "));
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStaticJITGeneratedOutputDebugMetadataHooksTest,
	"Angelscript.TestModule.StaticJIT.GeneratedOutput.DebugMetadataHooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptStaticJITGeneratedOutputDebugMetadataHooksTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_StaticJIT_AngelscriptStaticJITGeneratedOutputTests_Private;
	const FString ScriptSource = MakeScriptSource();
	const int32 FirstCallLine = FindScriptLineNumberContaining(ScriptSource, TEXT("AddOne(1);"));
	const int32 SecondCallLine = FindScriptLineNumberContaining(ScriptSource, TEXT("AddTwo(First);"));
	if (!TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should locate the first call line marker"), FirstCallLine != INDEX_NONE)
		|| !TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should locate the second call line marker"), SecondCallLine != INDEX_NONE))
	{
		return false;
	}

	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	do
	{
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			ModuleName,
			SourceFilename,
			ScriptSource);
		if (!TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should compile the fixture module"), bCompiled))
		{
			break;
		}

		FString SourceWithDebugMetadata;
		FString WithDebugError;
		const bool bGeneratedWithDebugMetadata = GenerateStaticJITSourceText(
			&Engine,
			ModuleName,
			SourceWithDebugMetadata,
			/*bEmitDebugMetadata=*/true,
			&WithDebugError);
		if (!TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should generate source text with debug metadata enabled"), bGeneratedWithDebugMetadata))
		{
			if (!WithDebugError.IsEmpty())
			{
				AddError(WithDebugError);
			}
			break;
		}

		const FString FrameNeedle = TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME(\"int Entry()\"");
		const FString FirstCallLineNeedle = FString::Printf(TEXT("SCRIPT_DEBUG_CALLSTACK_LINE(%d);"), FirstCallLine);
		const FString SecondCallLineNeedle = FString::Printf(TEXT("SCRIPT_DEBUG_CALLSTACK_LINE(%d);"), SecondCallLine);
		const TArray<int32> ObservedLineMarkers = ExtractGeneratedLineMarkers(SourceWithDebugMetadata);

		if (!TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should emit a frame macro when debug metadata is enabled"), SourceWithDebugMetadata.Contains(FrameNeedle))
			|| !TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should emit the first call line marker when debug metadata is enabled"), SourceWithDebugMetadata.Contains(FirstCallLineNeedle))
			|| !TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should emit the second call line marker when debug metadata is enabled"), SourceWithDebugMetadata.Contains(SecondCallLineNeedle)))
		{
			AddInfo(FString::Printf(TEXT("Observed generated line markers: [%s]"), *JoinLineMarkers(ObservedLineMarkers)));
			break;
		}

		FString SourceWithoutDebugMetadata;
		FString WithoutDebugError;
		const bool bGeneratedWithoutDebugMetadata = GenerateStaticJITSourceText(
			&Engine,
			ModuleName,
			SourceWithoutDebugMetadata,
			/*bEmitDebugMetadata=*/false,
			&WithoutDebugError);
		if (!TestTrue(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should generate source text with debug metadata disabled"), bGeneratedWithoutDebugMetadata))
		{
			if (!WithoutDebugError.IsEmpty())
			{
				AddError(WithoutDebugError);
			}
			break;
		}

		if (!TestFalse(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should omit frame macros when debug metadata is disabled"), SourceWithoutDebugMetadata.Contains(TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME(")))
			|| !TestFalse(TEXT("StaticJIT.GeneratedOutput.DebugMetadataHooks should omit line markers when debug metadata is disabled"), SourceWithoutDebugMetadata.Contains(TEXT("SCRIPT_DEBUG_CALLSTACK_LINE("))))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

#endif
