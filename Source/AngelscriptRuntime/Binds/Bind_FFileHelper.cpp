#include "Bind_FFileHelper.h"

#include "AngelscriptBinds.h"

#include "Misc/FileHelper.h"

/**
 * FFileHelper option enums and file I/O helpers.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum EFileWrite { None, NoFail, NoReplaceExisting, EvenIfReadOnly, Append, AllowRead, Silent };      | Bit flags controlling file writes.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum EFileRead { None, NoFail, Silent, AllowWrite };                                                 | Bit flags controlling file reads.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum FFileHelper::EHashOptions { None, EnableVerify, ErrorMissingHash };                             | Controls hash verification while loading.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | enum FFileHelper::EEncodingOptions { AutoDetect, ForceAnsi, ForceUnicode, ForceUTF8,                 | Selects the text encoding used when saving.                                                                      |
 * |     ForceUTF8WithoutBOM };                                                                           |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FFileHelper::LoadFileToString(FString& Result,                                                  | Loads an entire text file.                                                                                       |
 * |     const FString& Filename,                                                                         | @param Result Receives the loaded text.                                                                          |
 * |     FFileHelper::EHashOptions HashOptions = FFileHelper::EHashOptions::None,                         | @param Filename File to read.                                                                                    |
 * |     uint32 ReadFlags = uint32(EFileRead::None));                                                     | @param HashOptions Optional hash-verification policy.                                                            |
 * |                                                                                                      | @param ReadFlags EFileRead bit mask.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool FFileHelper::SaveStringToFile(const FString& String,                                            | Saves text to a file.                                                                                            |
 * |     const FString& Filename,                                                                         | @param String Text to write.                                                                                     |
 * |     FFileHelper::EEncodingOptions EncodingOptions = FFileHelper::EEncodingOptions::AutoDetect,       | @param Filename Destination file.                                                                                |
 * |     uint32 WriteFlags = uint32(EFileWrite::None));                                                   | @param EncodingOptions Output encoding policy.                                                                   |
 * |                                                                                                      | @param WriteFlags EFileWrite bit mask.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindFFileHelperTypes(FAngelscriptBinds& Binds)
	{
		auto EFileWrite_ = Binds.EnumForTarget("EFileWrite");
		EFileWrite_["None"] = FILEWRITE_None;
		EFileWrite_["NoFail"] = FILEWRITE_NoFail;
		EFileWrite_["NoReplaceExisting"] = FILEWRITE_NoReplaceExisting;
		EFileWrite_["EvenIfReadOnly"] = FILEWRITE_EvenIfReadOnly;
		EFileWrite_["Append"] = FILEWRITE_Append;
		EFileWrite_["AllowRead"] = FILEWRITE_AllowRead;
		EFileWrite_["Silent"] = FILEWRITE_Silent;

		auto EFileRead_ = Binds.EnumForTarget("EFileRead");
		EFileRead_["None"] = FILEREAD_None;
		EFileRead_["NoFail"] = FILEREAD_NoFail;
		EFileRead_["Silent"] = FILEREAD_Silent;
		EFileRead_["AllowWrite"] = FILEREAD_AllowWrite;

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FFileHelper");
		auto EHashOptions_ = Binds.EnumForTarget("EHashOptions");
		EHashOptions_["None"] = FFileHelper::EHashOptions::None;
		EHashOptions_["EnableVerify"] = FFileHelper::EHashOptions::EnableVerify;
		EHashOptions_["ErrorMissingHash"] = FFileHelper::EHashOptions::ErrorMissingHash;

		auto EEncodingOptions_ = Binds.EnumForTarget("EEncodingOptions");
		EEncodingOptions_["AutoDetect"] = FFileHelper::EEncodingOptions::AutoDetect;
		EEncodingOptions_["ForceAnsi"] = FFileHelper::EEncodingOptions::ForceAnsi;
		EEncodingOptions_["ForceUnicode"] = FFileHelper::EEncodingOptions::ForceUnicode;
		EEncodingOptions_["ForceUTF8"] = FFileHelper::EEncodingOptions::ForceUTF8;
		EEncodingOptions_["ForceUTF8WithoutBOM"] = FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;
	}

	void BindFFileHelperFunctions(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FFileHelper");
		Binds.BindGlobalFunctionForTarget(
			"bool LoadFileToString(FString& Result, const FString& Filename, FFileHelper::EHashOptions HashOptions = FFileHelper::EHashOptions::None, uint32 ReadFlags = uint32(EFileRead::None))",
			&FAngelscriptFFileHelperBinds::LoadFileToString);
		Binds.BindGlobalFunctionForTarget(
			"bool SaveStringToFile(const FString& String, const FString& Filename, FFileHelper::EEncodingOptions EncodingOptions = FFileHelper::EEncodingOptions::AutoDetect, uint32 WriteFlags = uint32(EFileWrite::None))",
			&FAngelscriptFFileHelperBinds::SaveStringToFile);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FFileHelper_Types(
	TEXT("FFileHelper.Types"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFFileHelperTypes);

AS_FORCE_LINK const FAngelscriptBind Bind_FFileHelper(
	TEXT("FFileHelper.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFFileHelperFunctions);
