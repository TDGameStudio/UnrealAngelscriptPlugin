#pragma once

#include "Core/AngelscriptBinds.h"

struct FAngelscriptBindRecord
{
	FName OwnerModule;
	FName BindName;
	EAngelscriptBindPhase Phase = EAngelscriptBindPhase::ManualBindings;
	const ANSICHAR* SourceFile = nullptr;
	int32 SourceLine = 0;
	FAngelscriptBindCallback Callback = nullptr;
};

namespace UE::Angelscript::Private
{
	class ANGELSCRIPTRUNTIME_API FAngelscriptBindCollection
	{
	public:
		bool Append(FAngelscriptBindRecord Record, FString& OutDiagnostic);
		bool Finalize(FString& OutDiagnostic);
		bool PrepareForEngineInitialization(
			TConstArrayView<FString> ModuleNames,
			TFunctionRef<bool(FName, FString&)> LoadModule,
			FString& OutDiagnostic);
		bool Execute(FAngelscriptBinds& Binds, FString& OutDiagnostic) const;
		bool Execute(
			FAngelscriptBinds& Binds,
			EAngelscriptBindPhase FirstPhase,
			EAngelscriptBindPhase LastPhase,
			FString& OutDiagnostic) const;

		TConstArrayView<FAngelscriptBindRecord> GetRecords() const
		{
			return Records;
		}

		bool IsSealed() const
		{
			return bSealed;
		}

	private:
		TArray<FAngelscriptBindRecord> Records;
		FString LateRegistrationFailureDiagnostic;
		bool bSealed = false;
		bool bPreparing = false;
	};
}
