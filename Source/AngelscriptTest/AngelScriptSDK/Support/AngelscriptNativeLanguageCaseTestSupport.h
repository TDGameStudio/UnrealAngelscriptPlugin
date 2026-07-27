#pragma once

#include "AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeCaseTestSupport.h"

namespace AngelscriptNativeTestSupport
{
	struct FNativeDirectionCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* DeclarationSuffix;
		asDWORD TypeModifier;
		bool bWritesValue;
		bool bReadsValue;
	};

	inline constexpr FNativeDirectionCase NativeDirectionCases[] =
	{
		{ "value", "", asTM_NONE, false, true },
		{ "in", "& in", asTM_INREF, false, true },
		{ "out", "& out", asTM_OUTREF, true, false },
		{ "inout", "& inout", asTM_INOUTREF, true, true },
	};

	inline bool IsCoreValueTypeCase(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category != ENativeValueCategory::ScriptReference
			&& TypeCase.Category != ENativeValueCategory::NativeReference
			&& TypeCase.Category != ENativeValueCategory::Null;
	}

	inline bool IsObjectValueTypeCase(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	inline void AppendGeneratedAsLine(FString& Source, const FString& Line = FString())
	{
		Source += Line;
		Source.AppendChar(TEXT('\n'));
	}

	inline void PrintGeneratedAsSource(
		FAutomationTestBase& Test,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source)
	{
		auto ReportLine = [&Test](
			const FString& Message,
			const bool bRetainInAutomationResult)
		{
			if (bRetainInAutomationResult)
			{
				Test.AddInfo(Message);
			}
			UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
		};

		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		ReportLine(FString::Printf(
			TEXT("[AS-SOURCE-BEGIN] Id=%s Module=%s Lines=%d"),
			*SourceId,
			*ModuleName,
			Lines.Num()),
			true);
		FString NumberedSource;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			NumberedSource += FString::Printf(
				TEXT("%04d | %s\n"),
				LineIndex + 1,
				*Lines[LineIndex]);
		}
		// Keep every generated line in the persistent Automation log, but emit
		// one log event per source and do not retain the corpus in CQTest's
		// result payload. Large data-driven products otherwise exceed the UE 5.8
		// structured-log event path while the source text remains unchanged.
		ReportLine(FString::Printf(
			TEXT("[AS-SOURCE-CONTENT] Id=%s Module=%s\n%s"),
			*SourceId,
			*ModuleName,
			*NumberedSource),
			false);
		ReportLine(FString::Printf(
			TEXT("[AS-SOURCE-END] Id=%s Module=%s"),
			*SourceId,
			*ModuleName),
			true);
	}

	inline FString MakeTypeReadExpression(
		const FNativeTypeCase& TypeCase,
		const TCHAR* VariableName,
		const ANSICHAR* ExpectedLiteral = nullptr)
	{
		const ANSICHAR* Literal = ExpectedLiteral != nullptr ? ExpectedLiteral : TypeCase.OneLiteral;
		if (IsObjectValueTypeCase(TypeCase))
		{
			return FString::Printf(TEXT("%s.Value == %hs"), VariableName, Literal);
		}
		return FString::Printf(TEXT("%s == %hs"), VariableName, Literal);
	}

	inline FString MakeTypeAssignStatement(
		const FNativeTypeCase& TypeCase,
		const TCHAR* VariableName,
		const ANSICHAR* AssignedLiteral = nullptr)
	{
		const ANSICHAR* Literal = AssignedLiteral != nullptr ? AssignedLiteral : TypeCase.OneLiteral;
		if (IsObjectValueTypeCase(TypeCase))
		{
			return FString::Printf(TEXT("%s.Value = %hs;"), VariableName, Literal);
		}
		return FString::Printf(TEXT("%s = %hs;"), VariableName, Literal);
	}

	inline FString MakeTypeInitialStatement(
		const FNativeTypeCase& TypeCase,
		const TCHAR* VariableName,
		const ANSICHAR* InitialLiteral = nullptr)
	{
		const ANSICHAR* Literal = InitialLiteral != nullptr ? InitialLiteral : TypeCase.OneLiteral;
		if (IsObjectValueTypeCase(TypeCase))
		{
			return FString::Printf(TEXT("%hs %s(%hs);"), TypeCase.ScriptType, VariableName, Literal);
		}
		return FString::Printf(TEXT("%hs %s = %hs;"), TypeCase.ScriptType, VariableName, Literal);
	}

	inline void AppendCoreLanguageTypeDeclarations(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("enum ENativeCaseEnum"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tMinimum = -1,"));
		AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
		AppendGeneratedAsLine(Source, TEXT("\tOne = 1,"));
		// The fork's enum storage is a signed byte. Keep explicit near-boundary
		// values representable while retaining negative/zero/positive coverage.
		AppendGeneratedAsLine(Source, TEXT("\tNearMaximum = 126,"));
		AppendGeneratedAsLine(Source, TEXT("\tMaximum = 127"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FScriptCaseValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	inline bool RegisterCoreLanguageTypedef(asIScriptEngine& Engine)
	{
		// The fork intentionally disables the script-level typedef token. Keep
		// the alias product active through the core embedding API instead of
		// emitting syntax that the current parser cannot recognize.
		return Engine.RegisterTypedef("NativeCaseAlias", "int") >= 0;
	}
}
