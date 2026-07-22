#pragma once

// Non-registering fixture types for AS-struct C++ operation coverage.

#include "CoreMinimal.h"

#include "AngelscriptStructCppOpsTestTypes.generated.h"

USTRUCT()
struct alignas(16) FAngelscriptStructCppOpsLifecycleFixture
{
	GENERATED_BODY()

	static constexpr int32 DefaultSentinelValue = 1337;
	static constexpr int32 DefaultPayloadValue = 77;

	static int32 DefaultConstructorCount;
	static int32 CopyCount;
	static int32 DestructorCount;

	UPROPERTY()
	int32 SentinelValue = DefaultSentinelValue;

	UPROPERTY()
	int32 PayloadValue = DefaultPayloadValue;

	FAngelscriptStructCppOpsLifecycleFixture();
	FAngelscriptStructCppOpsLifecycleFixture(const FAngelscriptStructCppOpsLifecycleFixture& Other);
	FAngelscriptStructCppOpsLifecycleFixture& operator=(const FAngelscriptStructCppOpsLifecycleFixture& Other);
	~FAngelscriptStructCppOpsLifecycleFixture();

	static void ResetCounters();

private:
	void CopyFrom(const FAngelscriptStructCppOpsLifecycleFixture& Other);
};

template<>
struct TStructOpsTypeTraits<FAngelscriptStructCppOpsLifecycleFixture> : public TStructOpsTypeTraitsBase2<FAngelscriptStructCppOpsLifecycleFixture>
{
	enum
	{
		WithCopy = true,
	};
};
