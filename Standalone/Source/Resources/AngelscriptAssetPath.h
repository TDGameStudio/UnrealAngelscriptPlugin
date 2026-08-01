#pragma once

#include <string>
#include <string_view>

namespace AngelscriptStandalone
{
	enum class EAssetPathKind
	{
		Package,
		Object,
		GeneratedClass,
		ScriptObject,
	};

	struct FAssetPathResult
	{
		bool bSuccess = false;
		std::string Original;
		std::string Normalized;
		std::string MountPoint;
		EAssetPathKind Kind = EAssetPathKind::Object;
		bool bChanged = false;
		std::string Error;
	};

	FAssetPathResult NormalizeAssetPath(std::string_view Path);
}
