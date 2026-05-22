#include "Testing/AngelscriptPropertyAccessorRemovalTestTypes.h"

int32 AAngelscriptPropertyAccessorCarrier::FetchScore() const
{
	return Score;
}

void AAngelscriptPropertyAccessorCarrier::SetField(int32 InField)
{
	Field = InField;
}
