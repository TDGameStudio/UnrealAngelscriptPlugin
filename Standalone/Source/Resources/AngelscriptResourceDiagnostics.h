#pragma once

#include "Host/AngelscriptStandaloneDiagnosticSink.h"
#include "Resources/AngelscriptResourceValidator.h"

namespace AngelscriptStandalone
{
	FDiagnostic MakeResourceDiagnostic(
		const FResourceValidation& Validation,
		bool bStrictResources);

	bool IsResourceFailure(
		const FResourceValidation& Validation,
		bool bStrictResources);
}
