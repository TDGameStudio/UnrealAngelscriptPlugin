#include "Bind_FMessageDialog.h"

EAppReturnType::Type FAngelscriptFMessageDialogBinds::Open(
	EAppMsgType::Type MessageType,
	const FText& Message,
	FText OptionalTitle)
{
	return FMessageDialog::Open(MessageType, Message, OptionalTitle);
}

EAppReturnType::Type FAngelscriptFMessageDialogBinds::OpenWithCategory(
	EAppMsgCategory MessageCategory,
	EAppMsgType::Type MessageType,
	const FText& Message,
	FText OptionalTitle)
{
	return FMessageDialog::Open(MessageCategory, MessageType, Message, OptionalTitle);
}
