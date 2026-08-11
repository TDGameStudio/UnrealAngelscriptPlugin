#include "Cache/AngelscriptCacheDiagnosticsLibrary.h"

#include "Cache/AngelscriptCacheDiagnostics.h"

bool UAngelscriptCacheDiagnosticsLibrary::GetCacheStatusJson(
	FString& OutStatusJson,
	FString& OutError)
{
	const FAngelscriptCacheDiagnosticJsonResult Result =
		CaptureCurrentAngelscriptCacheDiagnosticJson();
	OutStatusJson = Result.Json;
	OutError = Result.Detail;
	return Result.IsSuccess();
}
