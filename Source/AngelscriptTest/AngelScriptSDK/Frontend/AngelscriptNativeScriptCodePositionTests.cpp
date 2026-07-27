#include "AngelscriptTestMacros.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptcode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FParserScriptCodePositionTests,
	"Angelscript.TestModule.AngelScriptSDK.Frontend.ParserCartesianDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ScriptCodeRowColumnCartesianProduct)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("FRONTEND-SCRIPT-CODE-ROW-COLUMN",
			ENativeEvidence::Metadata);

		struct FLineEndingCase
		{
			const TCHAR* Id;
			FString Value;
			int32 Length;
		};
		const FLineEndingCase LineEndings[] =
		{
			{ TEXT("lf"), FString::Chr(10), 1 },
			{ TEXT("crlf"), FString::Chr(13) + FString::Chr(10), 2 },
		};
		struct FPositionCase
		{
			const TCHAR* Id;
			int32 ExpectedRow;
			int32 ExpectedColumn;
		};
		const FPositionCase Positions[] =
		{
			{ TEXT("line1_start"), 1, 1 },
			{ TEXT("line1_end"), 1, 5 },
			{ TEXT("line2_start"), 2, 1 },
			{ TEXT("line2_middle"), 2, 3 },
			{ TEXT("line3_end"), 3, 5 },
			{ TEXT("eof"), 3, 6 },
		};

		int32 ObservedCaseCount = 0;
		for (const FLineEndingCase& LineEnding : LineEndings)
		{
			const FString Source = FString(TEXT("Alpha")) + LineEnding.Value + TEXT("Beta") + LineEnding.Value + TEXT("Gamma");
			const int32 SecondLineStart = 5 + LineEnding.Length;
			const int32 PositionOffsets[] =
			{
				0,
				4,
				SecondLineStart,
				SecondLineStart + 2,
				Source.Len() - 1,
				Source.Len(),
			};

			for (int32 PositionIndex = 0; PositionIndex < UE_ARRAY_COUNT(Positions); ++PositionIndex)
			{
				const FPositionCase& Position = Positions[PositionIndex];
				const FString SourceId = FString::Printf(TEXT("FRONTEND-SCRIPT-CODE-ROW-COLUMN-%s-%s"), LineEnding.Id, Position.Id);
				const FString ModuleName = FString::Printf(TEXT("ParserCodePosition_%s_%s"), LineEnding.Id, Position.Id);
				PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);

				const FTCHARToUTF8 SourceUtf8(*Source);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				asCScriptCode Code;
				ASSERT_THAT(AreEqual(0, Code.SetCode(ModuleNameUtf8.Get(), SourceUtf8.Get(), true),
					FString::Printf(TEXT("Script-code cell %s should accept its source"), *SourceId)));
				int Row = 0;
				int Column = 0;
				Code.ConvertPosToRowCol(static_cast<size_t>(PositionOffsets[PositionIndex]), &Row, &Column);
				ASSERT_THAT(AreEqual(Position.ExpectedRow, Row,
					FString::Printf(TEXT("Script-code cell %s should retain its row"), *SourceId)));
				ASSERT_THAT(AreEqual(Position.ExpectedColumn, Column,
					FString::Printf(TEXT("Script-code cell %s should retain its column"), *SourceId)));
				++ObservedCaseCount;
			}
		}

		ASSERT_THAT(AreEqual(12, ObservedCaseCount,
			TEXT("Line-ending × source-position should execute every row/column cell")));
	}

};

#endif // WITH_ANGELSCRIPT_UNITTESTS
