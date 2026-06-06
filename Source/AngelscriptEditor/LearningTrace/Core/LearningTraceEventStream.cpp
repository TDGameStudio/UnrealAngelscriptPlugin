// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/LearningTraceEventStream.h"

namespace AngelscriptEditor::LearningTrace
{
	void FLearningTraceEventStream::Append(FLearningTraceEvent&& Event)
	{
		Event.Seq = NextSeq++;
		Events.Add(MoveTemp(Event));
	}

	void FLearningTraceEventStream::Emit(
		const TCHAR* Phase,
		const TCHAR* Type,
		int32 Pos,
		TSharedPtr<FJsonObject> Data)
	{
		FLearningTraceEvent Event;
		Event.Phase = Phase;
		Event.Type = Type;
		Event.Pos = Pos;
		Event.Data = MoveTemp(Data);
		Append(MoveTemp(Event));
	}

	TArray<TSharedPtr<FJsonValue>> FLearningTraceEventStream::ToJsonArray() const
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		Array.Reserve(Events.Num());
		for (const FLearningTraceEvent& Event : Events)
		{
			Array.Add(MakeShared<FJsonValueObject>(Event.ToJson()));
		}
		return Array;
	}
}
