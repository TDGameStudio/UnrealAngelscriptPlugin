#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "GameFramework/InputSettings.h"

namespace
{
	void BindUInputSettings(FAngelscriptBinds& Binds)
	{
		auto UInputSettings_ = Binds.ExistingClassForTarget("UInputSettings");

		UInputSettings_.Method("FName GetUniqueActionName(const FName BaseActionMappingName)", METHODPR_TRIVIAL(FName, UInputSettings, GetUniqueActionName, (const FName)));
		UInputSettings_.Method("FName GetUniqueAxisName(const FName BaseAxisMappingName)", METHODPR_TRIVIAL(FName, UInputSettings, GetUniqueAxisName, (const FName)));

		UInputSettings_.Method("const TArray<FInputActionKeyMapping>& GetActionMappings() const", METHODPR_TRIVIAL(const TArray<FInputActionKeyMapping>&, UInputSettings, GetActionMappings, ()));
		UInputSettings_.Method("const TArray<FInputAxisKeyMapping>& GetAxisMappings() const", METHODPR_TRIVIAL(const TArray<FInputAxisKeyMapping>&, UInputSettings, GetAxisMappings, ()));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UInputSettings_.Method("const TArray<FInputActionSpeechMapping>& GetSpeechMappings() const", METHODPR_TRIVIAL(const TArray<FInputActionSpeechMapping>&, UInputSettings, GetSpeechMappings, ()));
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		UInputSettings_.Method("bool DoesActionExist(const FName InActionName)", METHODPR_TRIVIAL(bool, UInputSettings, DoesActionExist, (const FName)));
		UInputSettings_.Method("bool DoesAxisExist(const FName InAxisName)", METHODPR_TRIVIAL(bool, UInputSettings, DoesAxisExist, (const FName)));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		UInputSettings_.Method("bool DoesSpeechExist(const FName InSpeechName)", METHODPR_TRIVIAL(bool, UInputSettings, DoesSpeechExist, (const FName)));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UInputSettings(
	TEXT("UInputSettings"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUInputSettings);
