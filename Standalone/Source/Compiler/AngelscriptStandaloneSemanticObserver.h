#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneSource.h"
#include "Registration/AngelscriptRegistrationLoader.h"

#include "angelscript.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace AngelscriptStandalone
{
	enum class ESemanticObservationKind
	{
		ResolvedCall,
		Constructor,
		Assignment,
		ConstantString,
	};

	struct FSemanticArgumentObservation
	{
		std::string ActualStableTypeId;
		std::string ActualTypeDeclaration;
		std::string ParameterStableTypeId;
		std::string ParameterTypeDeclaration;
		AngelscriptStandalone::Frontend::FSourceSpan Source;
	};

	struct FSemanticObservation
	{
		ESemanticObservationKind Kind =
			ESemanticObservationKind::ResolvedCall;
		std::string StableFunctionId;
		std::string SourceStableTypeId;
		std::string SourceTypeDeclaration;
		std::string TargetStableTypeId;
		std::string TargetTypeDeclaration;
		std::string LogicalPath;
		AngelscriptStandalone::Frontend::FSourceSpan Source;
		int Row = 0;
		int Column = 0;
		std::string ConstantString;
		std::vector<FSemanticArgumentObservation> Arguments;
	};

	const char* ToString(ESemanticObservationKind Kind);

	class FStandaloneSemanticObserver final
		: public asISemanticObserver
	{
	public:
		FStandaloneSemanticObserver(
			asIScriptEngine& Engine,
			const FRegistrationRuntimeMap& RuntimeMap);

		void Observe(
			const asSSemanticObservation& Observation) override;

		const std::vector<FSemanticObservation>&
			GetObservations() const;

	private:
		struct FTypeIdentity
		{
			std::string StableId;
			std::string Declaration;
		};

		FTypeIdentity ResolveType(int RuntimeTypeId) const;

		asIScriptEngine& Engine;
		std::unordered_map<int, std::string>
			StableFunctionIdByRuntimeId;
		std::unordered_map<int, std::string>
			StableTypeIdByRuntimeId;
		std::vector<FSemanticObservation> Observations;
	};
}
