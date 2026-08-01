#include "Compiler/AngelscriptStandaloneSemanticObserver.h"

#include <algorithm>
#include <string_view>

namespace AngelscriptStandalone
{
	namespace
	{
		template <typename MapType>
		void InsertDeterministically(
			MapType& Destination,
			const int RuntimeId,
			const std::string& StableId)
		{
			const auto [Iterator, bInserted] =
				Destination.emplace(RuntimeId, StableId);
			if (!bInserted && StableId < Iterator->second)
			{
				Iterator->second = StableId;
			}
		}

		int NormalizeTypeId(const int TypeId)
		{
			return TypeId
				& ~asTYPEID_OBJHANDLE
				& ~asTYPEID_HANDLETOCONST;
		}

		ESemanticObservationKind ConvertKind(
			const asESemanticObservationKind Kind)
		{
			switch (Kind)
			{
			case asSEMANTIC_OBSERVATION_CONSTRUCTOR:
				return ESemanticObservationKind::Constructor;
			case asSEMANTIC_OBSERVATION_ASSIGNMENT:
				return ESemanticObservationKind::Assignment;
			case asSEMANTIC_OBSERVATION_CONSTANT_STRING:
				return ESemanticObservationKind::ConstantString;
			case asSEMANTIC_OBSERVATION_RESOLVED_CALL:
			default:
				return ESemanticObservationKind::ResolvedCall;
			}
		}
	}

	const char* ToString(const ESemanticObservationKind Kind)
	{
		switch (Kind)
		{
		case ESemanticObservationKind::ResolvedCall:
			return "resolved-call";
		case ESemanticObservationKind::Constructor:
			return "constructor";
		case ESemanticObservationKind::Assignment:
			return "assignment";
		case ESemanticObservationKind::ConstantString:
			return "constant-string";
		default:
			return "unknown";
		}
	}

	FStandaloneSemanticObserver::FStandaloneSemanticObserver(
		asIScriptEngine& InEngine,
		const FRegistrationRuntimeMap& RuntimeMap)
		: Engine(InEngine)
	{
		for (const auto& [StableId, RuntimeId]
			: RuntimeMap.FunctionIdByStableId)
		{
			InsertDeterministically(
				StableFunctionIdByRuntimeId,
				RuntimeId,
				StableId);
		}
		for (const auto& [StableId, RuntimeId]
			: RuntimeMap.TypeIdByStableId)
		{
			InsertDeterministically(
				StableTypeIdByRuntimeId,
				NormalizeTypeId(RuntimeId),
				StableId);
		}
	}

	void FStandaloneSemanticObserver::Observe(
		const asSSemanticObservation& Observation)
	{
		FSemanticObservation& Output =
			Observations.emplace_back();
		Output.Kind = ConvertKind(Observation.kind);
		if (Observation.function != nullptr)
		{
			const auto StableFunction =
				StableFunctionIdByRuntimeId.find(
					Observation.function->GetId());
			if (StableFunction
				!= StableFunctionIdByRuntimeId.end())
			{
				Output.StableFunctionId =
					StableFunction->second;
			}
		}
		const FTypeIdentity SourceType =
			ResolveType(Observation.sourceTypeId);
		Output.SourceStableTypeId = SourceType.StableId;
		Output.SourceTypeDeclaration =
			SourceType.Declaration;
		const FTypeIdentity TargetType =
			ResolveType(Observation.targetTypeId);
		Output.TargetStableTypeId = TargetType.StableId;
		Output.TargetTypeDeclaration =
			TargetType.Declaration;
		Output.LogicalPath = Observation.section != nullptr
			? Observation.section
			: "";
		Output.Source = {
			Observation.sourceOffset,
			static_cast<std::size_t>(
				Observation.sourceOffset)
				+ Observation.sourceLength,
		};
		Output.Row = Observation.row;
		Output.Column = Observation.column;
		if (Observation.constantString != nullptr
			&& Observation.constantStringLength <= 4096)
		{
			Output.ConstantString.assign(
				Observation.constantString,
				Observation.constantStringLength);
		}
		Output.Arguments.reserve(Observation.argumentCount);
		for (asUINT Index = 0;
			Index < Observation.argumentCount;
			++Index)
		{
			const asSSemanticArgumentObservation& Input =
				Observation.arguments[Index];
			FSemanticArgumentObservation& Argument =
				Output.Arguments.emplace_back();
			const FTypeIdentity Actual =
				ResolveType(Input.actualTypeId);
			Argument.ActualStableTypeId = Actual.StableId;
			Argument.ActualTypeDeclaration =
				Actual.Declaration;
			const FTypeIdentity Parameter =
				ResolveType(Input.parameterTypeId);
			Argument.ParameterStableTypeId =
				Parameter.StableId;
			Argument.ParameterTypeDeclaration =
				Parameter.Declaration;
			Argument.Source = {
				Input.sourceOffset,
				static_cast<std::size_t>(Input.sourceOffset)
					+ Input.sourceLength,
			};
		}
	}

	const std::vector<FSemanticObservation>&
	FStandaloneSemanticObserver::GetObservations() const
	{
		return Observations;
	}

	FStandaloneSemanticObserver::FTypeIdentity
	FStandaloneSemanticObserver::ResolveType(
		const int RuntimeTypeId) const
	{
		if (RuntimeTypeId == 0)
			return {};
		const int Normalized = NormalizeTypeId(RuntimeTypeId);
		FTypeIdentity Result;
		const auto Stable =
			StableTypeIdByRuntimeId.find(Normalized);
		if (Stable != StableTypeIdByRuntimeId.end())
			Result.StableId = Stable->second;
		const char* Declaration =
			Engine.GetTypeDeclaration(RuntimeTypeId, true);
		if (Declaration == nullptr && Normalized != RuntimeTypeId)
		{
			Declaration =
				Engine.GetTypeDeclaration(Normalized, true);
		}
		if (Declaration != nullptr)
			Result.Declaration = Declaration;
		if (Result.StableId.empty()
			&& !Result.Declaration.empty()
			&& Engine.GetTypeInfoById(Normalized) == nullptr)
		{
			Result.StableId =
				"builtin:" + Result.Declaration;
		}
		return Result;
	}
}
