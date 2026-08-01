#include "Resources/AngelscriptResourceDiagnostics.h"

namespace AngelscriptStandalone
{
	bool IsResourceFailure(
		const FResourceValidation& Validation,
		const bool bStrictResources)
	{
		if (Validation.State == EResourceState::Incompatible)
			return true;
		if (Validation.State == EResourceState::Missing)
			return bStrictResources || !Validation.Context.bSoft;
		return false;
	}

	FDiagnostic MakeResourceDiagnostic(
		const FResourceValidation& Validation,
		const bool bStrictResources)
	{
		FDiagnostic Diagnostic;
		Diagnostic.Code = std::string("AS-RESOURCE-")
			+ (Validation.State == EResourceState::Found
				? "FOUND"
				: Validation.State == EResourceState::Redirected
					? "REDIRECTED"
					: Validation.State == EResourceState::Missing
						? "MISSING"
						: Validation.State
								== EResourceState::Incompatible
							? "INCOMPATIBLE"
							: "UNKNOWN");
		Diagnostic.Section = Validation.Context.LogicalPath;
		Diagnostic.Row = 1;
		Diagnostic.Column = static_cast<int>(
			Validation.Context.Span.Begin + 1);
		if (IsResourceFailure(Validation, bStrictResources))
			Diagnostic.Severity = EDiagnosticSeverity::Error;
		else if (Validation.State == EResourceState::Missing
			|| Validation.State == EResourceState::Redirected)
			Diagnostic.Severity = EDiagnosticSeverity::Warning;
		else
			Diagnostic.Severity = EDiagnosticSeverity::Info;
		Diagnostic.Message = Validation.Reason;
		Diagnostic.Evidence = {
			{"contextId", Validation.Context.ContextId},
			{"contextSymbolId",
				Validation.Context.ContextStableSymbolId},
			{"diagnosticId", Validation.DiagnosticId},
			{"finalPath", Validation.FinalPath},
			{"normalizedPath", Validation.NormalizedPath},
			{"originalPath", Validation.Context.ConstantPath},
			{"requestedTypeId",
				Validation.Context.RequestedStableTypeId},
			{"resolvedAssetId",
				Validation.ResolvedAssetStableId},
			{"resolvedTypePath",
				Validation.ResolvedTypePath},
			{"state", ToString(Validation.State)},
		};
		return Diagnostic;
	}
}
