// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/LearningTraceEvent.h"

#include "Dom/JsonObject.h"

namespace AngelscriptEditor::LearningTrace
{
	TSharedRef<FJsonObject> FLearningTraceEvent::ToJson() const
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("seq"), Seq);
		Object->SetStringField(TEXT("phase"), Phase);
		Object->SetStringField(TEXT("type"), Type);
		if (Pos != INDEX_NONE)
		{
			Object->SetNumberField(TEXT("pos"), Pos);
		}
		if (Data.IsValid())
		{
			Object->SetObjectField(TEXT("data"), Data);
		}
		return Object;
	}
}
