#include "AngelscriptConstructionContextProbe.h"

#include "ClassGenerator/ASClass.h"
#include "UObject/WeakObjectPtrTemplates.h"

static TWeakObjectPtr<UObject> GAngelscriptConstructionContextProbeCapturedObject;
static int32 GAngelscriptConstructionContextProbeCaptureCount = 0;

UObject* UAngelscriptConstructionContextProbe::CaptureConstructingObject()
{
	UObject* ConstructingObject = UASClass::GetConstructingASObject();
	GAngelscriptConstructionContextProbeCapturedObject = ConstructingObject;
	++GAngelscriptConstructionContextProbeCaptureCount;
	return ConstructingObject;
}

UObject* UAngelscriptConstructionContextProbe::GetCapturedObject() const
{
	return GAngelscriptConstructionContextProbeCapturedObject.Get();
}

int32 UAngelscriptConstructionContextProbe::GetCaptureCount() const
{
	return GAngelscriptConstructionContextProbeCaptureCount;
}

void UAngelscriptConstructionContextProbe::ResetCapturedObject()
{
	ResetCaptureState();
}

UObject* UAngelscriptConstructionContextProbe::GetLastCapturedObject()
{
	return GAngelscriptConstructionContextProbeCapturedObject.Get();
}

int32 UAngelscriptConstructionContextProbe::GetLastCaptureCount()
{
	return GAngelscriptConstructionContextProbeCaptureCount;
}

void UAngelscriptConstructionContextProbe::ResetCaptureState()
{
	GAngelscriptConstructionContextProbeCapturedObject.Reset();
	GAngelscriptConstructionContextProbeCaptureCount = 0;
}
