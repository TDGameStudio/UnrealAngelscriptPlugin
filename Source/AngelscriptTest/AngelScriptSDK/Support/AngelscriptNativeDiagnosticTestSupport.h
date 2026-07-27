#pragma once

#include "AngelscriptNativeCoreTestSupport.h"

namespace AngelscriptNativeTestSupport
{
	struct FExpectedNativeDiagnostic
	{
		int ReturnCode = asERROR;
		asEMsgType Type = asMSGTYPE_ERROR;
		const TCHAR* Section = nullptr;
		int32 Row = INDEX_NONE;
		int32 Column = INDEX_NONE;
		const TCHAR* StableText = nullptr;
	};

	inline bool MatchesNativeDiagnostic(
		const int ActualReturnCode,
		const FNativeMessageCollector& Messages,
		const FExpectedNativeDiagnostic& Expected,
		FString& OutDifference)
	{
		if (ActualReturnCode != Expected.ReturnCode)
		{
			OutDifference = FString::Printf(TEXT("ReturnCode expected=%d actual=%d"), Expected.ReturnCode, ActualReturnCode);
			return false;
		}

		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			const bool bSectionMatches = Expected.Section == nullptr || Entry.Section == Expected.Section;
			const bool bRowMatches = Expected.Row == INDEX_NONE || Entry.Row == Expected.Row;
			const bool bColumnMatches = Expected.Column == INDEX_NONE || Entry.Column == Expected.Column;
			const bool bTextMatches = Expected.StableText == nullptr || Entry.Message.Contains(Expected.StableText, ESearchCase::CaseSensitive);
			if (Entry.Type == Expected.Type && bSectionMatches && bRowMatches && bColumnMatches && bTextMatches)
			{
				OutDifference.Reset();
				return true;
			}
		}

		OutDifference = FString::Printf(
			TEXT("No diagnostic matched Type=%s Section='%s' Row=%d Column=%d Text='%s'. Actual={%s}"),
			ToMessageTypeString(Expected.Type),
			Expected.Section != nullptr ? Expected.Section : TEXT("<any>"),
			Expected.Row,
			Expected.Column,
			Expected.StableText != nullptr ? Expected.StableText : TEXT("<any>"),
			*CollectMessages(Messages));
		return false;
	}
}
