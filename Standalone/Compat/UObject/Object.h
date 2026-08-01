#pragma once

#include "UECompat.h"

class UObject
{
};

class UClass
{
public:
	bool HasAnyClassFlags(uint32) const { return false; }
};

inline constexpr uint32 CLASS_Interface = 1u;
