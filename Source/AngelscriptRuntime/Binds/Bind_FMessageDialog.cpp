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

#include "AngelscriptBinds.h"

/**
 * FMessageDialog namespace helpers.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | EAppReturnType FMessageDialog::Open(EAppMsgType MessageType,                                         | Opens a modal message dialog with an optional title.                                                             |
 * |     const FText& Message,                                                                            | @param MessageType Selects the available buttons.                                                                |
 * |     FText OptionalTitle = FText());                                                                  | @param Message Text displayed in the dialog.                                                                     |
 * |                                                                                                      | @param OptionalTitle Title text; an empty value uses the platform default.                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | EAppReturnType FMessageDialog::Open(EAppMsgCategory MessageCategory,                                 | Opens a categorized modal message dialog with an optional title.                                                 |
 * |     EAppMsgType MessageType,                                                                         | @param MessageCategory Categorizes the message for platform handling.                                            |
 * |     const FText& Message,                                                                            | @param MessageType Selects the available buttons.                                                                |
 * |     FText OptionalTitle = FText());                                                                  | @param Message Text displayed in the dialog.                                                                     |
 * |                                                                                                      | @param OptionalTitle Title text; an empty value uses the platform default.                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FMessageDialog(
	TEXT("FMessageDialog"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FMessageDialog");
		Binds.BindGlobalFunctionForTarget(
			"EAppReturnType Open(EAppMsgType MessageType, const FText& Message, FText OptionalTitle = FText())",
			&FAngelscriptFMessageDialogBinds::Open);
		Binds.BindGlobalFunctionForTarget(
			"EAppReturnType Open(EAppMsgCategory MessageCategory, EAppMsgType MessageType, const FText& Message, FText OptionalTitle = FText())",
			&FAngelscriptFMessageDialogBinds::OpenWithCategory);
	});

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
