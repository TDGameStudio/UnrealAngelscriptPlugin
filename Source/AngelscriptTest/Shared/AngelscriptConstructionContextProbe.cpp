#include "AngelscriptConstructionContextProbe.h"

#include "ClassGenerator/ASClass.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace AngelscriptTest_Shared_AngelscriptConstructionContextProbe_Private
{
	TWeakObjectPtr<UObject> GCapturedConstructingObject;
	int32 GConstructingObjectCaptureCount = 0;
}

using namespace AngelscriptTest_Shared_AngelscriptConstructionContextProbe_Private;

UObject* UAngelscriptConstructionContextProbe::CaptureConstructingObject()
{
	UObject* ConstructingObject = UASClass::GetConstructingASObject();
	GCapturedConstructingObject = ConstructingObject;
	++GConstructingObjectCaptureCount;
	return ConstructingObject;
}

UObject* UAngelscriptConstructionContextProbe::GetCapturedObject() const
{
	return GCapturedConstructingObject.Get();
}

int32 UAngelscriptConstructionContextProbe::GetCaptureCount() const
{
	return GConstructingObjectCaptureCount;
}

void UAngelscriptConstructionContextProbe::ResetCapturedObject()
{
	ResetCaptureState();
}

UObject* UAngelscriptConstructionContextProbe::GetLastCapturedObject()
{
	return GCapturedConstructingObject.Get();
}

int32 UAngelscriptConstructionContextProbe::GetLastCaptureCount()
{
	return GConstructingObjectCaptureCount;
}

void UAngelscriptConstructionContextProbe::ResetCaptureState()
{
	GCapturedConstructingObject.Reset();
	GConstructingObjectCaptureCount = 0;
}
