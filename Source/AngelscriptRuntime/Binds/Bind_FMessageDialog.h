#pragma once

#include "Misc/MessageDialog.h"

struct FAngelscriptFMessageDialogBinds
{
	static EAppReturnType::Type Open(EAppMsgType::Type MessageType, const FText& Message, FText OptionalTitle);
	static EAppReturnType::Type OpenWithCategory(
		EAppMsgCategory MessageCategory,
		EAppMsgType::Type MessageType,
		const FText& Message,
		FText OptionalTitle);
};
