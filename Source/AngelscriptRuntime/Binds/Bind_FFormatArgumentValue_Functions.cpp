#include "Bind_FFormatArgumentValue.h"

void FAngelscriptFFormatArgumentValueBinds::ConstructDefault(FFormatArgumentValue* Address)
{
	new (Address) FFormatArgumentValue();
}

void FAngelscriptFFormatArgumentValueBinds::ConstructInt32(FFormatArgumentValue* Address, const int32 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructUInt32(FFormatArgumentValue* Address, const uint32 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructInt64(FFormatArgumentValue* Address, const int64 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructUInt64(FFormatArgumentValue* Address, const uint64 Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructFloat(FFormatArgumentValue* Address, const float Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructDouble(FFormatArgumentValue* Address, const double Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructText(FFormatArgumentValue* Address, const FText& Value)
{
	new (Address) FFormatArgumentValue(Value);
}

void FAngelscriptFFormatArgumentValueBinds::ConstructGender(FFormatArgumentValue* Address, const ETextGender Value)
{
	new (Address) FFormatArgumentValue(Value);
}
