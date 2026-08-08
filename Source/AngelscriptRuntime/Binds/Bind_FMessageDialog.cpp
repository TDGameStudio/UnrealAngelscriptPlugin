#include "AngelscriptBinds.h"

#include "Bind_FMessageDialog_Functions.h"

namespace
{
	void BindFMessageDialog(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FMessageDialog");
		Binds.BindGlobalFunctionForTarget(
			"EAppReturnType Open(EAppMsgType MessageType, const FText& Message, FText OptionalTitle = FText())",
			&FAngelscriptFMessageDialogBinds::Open);
		Binds.BindGlobalFunctionForTarget(
			"EAppReturnType Open(EAppMsgCategory MessageCategory, EAppMsgType MessageType, const FText& Message, FText OptionalTitle = FText())",
			&FAngelscriptFMessageDialogBinds::OpenWithCategory);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FMessageDialog(
	TEXT("FMessageDialog"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFMessageDialog);
