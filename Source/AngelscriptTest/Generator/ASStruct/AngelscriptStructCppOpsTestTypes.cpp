#include "AngelscriptStructCppOpsTestTypes.h"

int32 FAngelscriptStructCppOpsLifecycleFixture::DefaultConstructorCount = 0;
int32 FAngelscriptStructCppOpsLifecycleFixture::CopyCount = 0;
int32 FAngelscriptStructCppOpsLifecycleFixture::DestructorCount = 0;

FAngelscriptStructCppOpsLifecycleFixture::FAngelscriptStructCppOpsLifecycleFixture()
	: SentinelValue(DefaultSentinelValue)
	, PayloadValue(DefaultPayloadValue)
{
	++DefaultConstructorCount;
}

FAngelscriptStructCppOpsLifecycleFixture::FAngelscriptStructCppOpsLifecycleFixture(const FAngelscriptStructCppOpsLifecycleFixture& Other)
{
	CopyFrom(Other);
	++CopyCount;
}

FAngelscriptStructCppOpsLifecycleFixture& FAngelscriptStructCppOpsLifecycleFixture::operator=(const FAngelscriptStructCppOpsLifecycleFixture& Other)
{
	if (this != &Other)
	{
		CopyFrom(Other);
		++CopyCount;
	}

	return *this;
}

FAngelscriptStructCppOpsLifecycleFixture::~FAngelscriptStructCppOpsLifecycleFixture()
{
	++DestructorCount;
}

void FAngelscriptStructCppOpsLifecycleFixture::ResetCounters()
{
	DefaultConstructorCount = 0;
	CopyCount = 0;
	DestructorCount = 0;
}

void FAngelscriptStructCppOpsLifecycleFixture::CopyFrom(const FAngelscriptStructCppOpsLifecycleFixture& Other)
{
	SentinelValue = Other.SentinelValue;
	PayloadValue = Other.PayloadValue;
}
