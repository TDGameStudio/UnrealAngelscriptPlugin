#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "GameFramework/InputSettings.h"

/**
 * UInputSettings manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName UInputSettings.GetUniqueActionName(const FName BaseActionMappingName);               | Returns an unused action-mapping name derived from the requested base name.                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FName UInputSettings.GetUniqueAxisName(const FName BaseAxisMappingName);                   | Returns an unused axis-mapping name derived from the requested base name.                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const TArray<FInputActionKeyMapping>& UInputSettings.GetActionMappings() const;            | Returns all configured action key mappings.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const TArray<FInputAxisKeyMapping>& UInputSettings.GetAxisMappings() const;                | Returns all configured axis key mappings.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | const TArray<FInputActionSpeechMapping>& UInputSettings.GetSpeechMappings()                | Returns all configured legacy speech mappings.                                                                       |
 * |     const;                                                                                 |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UInputSettings.DoesActionExist(const FName InActionName);                             | Returns whether an action mapping with the supplied name exists.                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UInputSettings.DoesAxisExist(const FName InAxisName);                                 | Returns whether an axis mapping with the supplied name exists.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UInputSettings.DoesSpeechExist(const FName InSpeechName);                             | Returns whether a legacy speech mapping with the supplied name exists.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_UInputSettings(
	TEXT("UInputSettings"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
