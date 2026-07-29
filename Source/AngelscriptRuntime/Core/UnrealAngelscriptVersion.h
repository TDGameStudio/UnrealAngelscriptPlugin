#pragma once

#define UNREAL_ANGELSCRIPT_VERSION_MAJOR 1
#define UNREAL_ANGELSCRIPT_VERSION_MINOR 0
#define UNREAL_ANGELSCRIPT_VERSION_PATCH 0

#define UNREAL_ANGELSCRIPT_VERSION 10000

#define UNREAL_ANGELSCRIPT_PRODUCT_NAME "Unreal AngelScript"
#define UNREAL_ANGELSCRIPT_VERSION_STRING "1.0.0"
#define UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING "Unreal AngelScript 1.0.0"

#define UNREAL_ANGELSCRIPT_UPSTREAM_BASE_VERSION 23300
#define UNREAL_ANGELSCRIPT_UPSTREAM_BASE_VERSION_STRING "2.33.0 WIP"
#define UNREAL_ANGELSCRIPT_UPSTREAM_LINEAGE_STRING \
	"AngelScript 2.33.0 WIP lineage + selective 2.38 backports"

namespace UnrealAngelscriptVersion
{
	constexpr unsigned Encode(unsigned Major, unsigned Minor, unsigned Patch)
	{
		return Major * 10000u + Minor * 100u + Patch;
	}

	constexpr unsigned GetMajor(unsigned Version)
	{
		return Version / 10000u;
	}

	constexpr bool IsCompatible(unsigned RequestedVersion, unsigned AvailableVersion)
	{
		return RequestedVersion != 0u
			&& AvailableVersion != 0u
			&& GetMajor(RequestedVersion) == GetMajor(AvailableVersion)
			&& RequestedVersion <= AvailableVersion;
	}
}

static_assert(UNREAL_ANGELSCRIPT_VERSION_MINOR <= 99, "Unreal AngelScript minor version must fit the public two-digit encoding");
static_assert(UNREAL_ANGELSCRIPT_VERSION_PATCH <= 99, "Unreal AngelScript patch version must fit the public two-digit encoding");
static_assert(
	UNREAL_ANGELSCRIPT_VERSION
		== UnrealAngelscriptVersion::Encode(
			UNREAL_ANGELSCRIPT_VERSION_MAJOR,
			UNREAL_ANGELSCRIPT_VERSION_MINOR,
			UNREAL_ANGELSCRIPT_VERSION_PATCH),
	"Unreal AngelScript encoded version must match its semantic version components");
