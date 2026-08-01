#include "Compiler/AngelscriptStandaloneSemanticObserver.h"

#include "angelscript.h"
#include "scriptstdstring/scriptstdstring.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	class FMemoryByteCodeStream final : public asIBinaryStream
	{
	public:
		int Write(const void* Data, asUINT Size) override
		{
			const auto* Bytes = static_cast<const std::uint8_t*>(Data);
			Buffer.insert(Buffer.end(), Bytes, Bytes + Size);
			return 0;
		}

		int Read(void*, asUINT) override
		{
			return -1;
		}

		std::vector<std::uint8_t> Buffer;
	};

	struct FObservedArgument
	{
		int ActualTypeId = 0;
		int ParameterTypeId = 0;
		asUINT SourceOffset = 0;
		asUINT SourceLength = 0;
	};

	struct FObservedEvent
	{
		asESemanticObservationKind Kind =
			asSEMANTIC_OBSERVATION_RESOLVED_CALL;
		int FunctionId = -1;
		int SourceTypeId = 0;
		int TargetTypeId = 0;
		std::string Section;
		asUINT SourceOffset = 0;
		asUINT SourceLength = 0;
		int Row = 0;
		int Column = 0;
		std::string ConstantString;
		std::vector<FObservedArgument> Arguments;
	};

	class FObserver final : public asISemanticObserver
	{
	public:
		void Observe(
			const asSSemanticObservation& Observation) override
		{
			FObservedEvent Event;
			Event.Kind = Observation.kind;
			Event.FunctionId = Observation.function != nullptr
				? Observation.function->GetId()
				: -1;
			Event.SourceTypeId = Observation.sourceTypeId;
			Event.TargetTypeId = Observation.targetTypeId;
			Event.Section = Observation.section != nullptr
				? Observation.section
				: "";
			Event.SourceOffset = Observation.sourceOffset;
			Event.SourceLength = Observation.sourceLength;
			Event.Row = Observation.row;
			Event.Column = Observation.column;
			if (Observation.constantString != nullptr)
			{
				Event.ConstantString.assign(
					Observation.constantString,
					Observation.constantStringLength);
			}
			for (asUINT Index = 0;
				Index < Observation.argumentCount;
				++Index)
			{
				const asSSemanticArgumentObservation& Source =
					Observation.arguments[Index];
				Event.Arguments.push_back({
					Source.actualTypeId,
					Source.parameterTypeId,
					Source.sourceOffset,
					Source.sourceLength,
				});
			}
			Events.push_back(std::move(Event));
		}

		std::vector<FObservedEvent> Events;
	};

	void CompileOnlyTrap(asIScriptGeneric*)
	{
	}

	bool Require(
		const bool Condition,
		const std::string_view Message)
	{
		if (!Condition)
		{
			std::cerr << Message << '\n';
		}
		return Condition;
	}

	bool ConfigureEngine(
		asIScriptEngine& Engine,
		int& OutIntOverload,
		int& OutUIntOverload,
		int& OutConstructor)
	{
		RegisterStdString(&Engine);
		OutIntOverload = Engine.RegisterGlobalFunction(
			"int Select(int Value)",
			asFUNCTION(CompileOnlyTrap),
			asCALL_GENERIC);
		OutUIntOverload = Engine.RegisterGlobalFunction(
			"int Select(uint Value)",
			asFUNCTION(CompileOnlyTrap),
			asCALL_GENERIC);
		if (Engine.RegisterObjectType(
				"FObserved",
				4,
				asOBJ_VALUE | asOBJ_APP_PRIMITIVE) < 0)
		{
			return false;
		}
		OutConstructor = Engine.RegisterObjectBehaviour(
			"FObserved",
			asBEHAVE_CONSTRUCT,
			"void f(int Value)",
			asFUNCTION(CompileOnlyTrap),
			asCALL_GENERIC);
		return OutIntOverload >= 0
			&& OutUIntOverload >= 0
			&& OutConstructor >= 0;
	}

	bool Compile(
		asIScriptEngine& Engine,
		const char* ModuleName,
		FMemoryByteCodeStream& OutByteCode)
	{
		static constexpr const char* Source =
			"int Observe()\n"
			"{\n"
			"    int Value = Select(7);\n"
			"    FObserved Constructed(9);\n"
			"    Value = 3;\n"
			"    string Path = \"/Game/Observed.Asset\";\n"
			"    return Value;\n"
			"}\n";
		asIScriptModule* Module =
			Engine.GetModule(ModuleName, asGM_ALWAYS_CREATE);
		return Module != nullptr
			&& Module->AddScriptSection(
				"SemanticObserver.as",
				Source) >= 0
			&& Module->Build() >= 0
			&& Module->SaveByteCode(&OutByteCode) >= 0;
	}
}

int main()
{
	bool bPassed = true;
	asIScriptEngine* ObservedEngine = asCreateScriptEngine();
	asIScriptEngine* BaselineEngine = asCreateScriptEngine();
	asIScriptEngine* StableEngine = asCreateScriptEngine();
	bPassed &= Require(
		ObservedEngine != nullptr
			&& BaselineEngine != nullptr
			&& StableEngine != nullptr,
		"failed to create semantic-observer test engines");
	if (!bPassed)
	{
		return 1;
	}

	int ObservedInt = -1;
	int ObservedUInt = -1;
	int ObservedConstructor = -1;
	int BaselineInt = -1;
	int BaselineUInt = -1;
	int BaselineConstructor = -1;
	int StableInt = -1;
	int StableUInt = -1;
	int StableConstructor = -1;
	bPassed &= Require(
		ConfigureEngine(
			*ObservedEngine,
			ObservedInt,
			ObservedUInt,
			ObservedConstructor)
			&& ConfigureEngine(
				*BaselineEngine,
				BaselineInt,
				BaselineUInt,
				BaselineConstructor)
			&& ConfigureEngine(
				*StableEngine,
				StableInt,
				StableUInt,
				StableConstructor),
		"failed to configure semantic-observer test engines");

	FObserver Observer;
	ObservedEngine->SetUserData(
		&Observer,
		asSEMANTIC_OBSERVER_USER_DATA_ID);
	FMemoryByteCodeStream ObservedByteCode;
	FMemoryByteCodeStream BaselineByteCode;
	bPassed &= Require(
		Compile(*ObservedEngine, "semantic", ObservedByteCode),
		"observed compilation failed");
	bPassed &= Require(
		Compile(*BaselineEngine, "semantic", BaselineByteCode),
		"baseline compilation failed");
	bPassed &= Require(
		ObservedByteCode.Buffer == BaselineByteCode.Buffer,
		"installing an observer changed compiler bytecode");

	const auto HasEvent = [&](const asESemanticObservationKind Kind)
	{
		return std::any_of(
			Observer.Events.begin(),
			Observer.Events.end(),
			[Kind](const FObservedEvent& Event)
			{
				return Event.Kind == Kind;
			});
	};
	bPassed &= Require(
		HasEvent(asSEMANTIC_OBSERVATION_RESOLVED_CALL),
		"resolved call observation is missing");
	bPassed &= Require(
		HasEvent(asSEMANTIC_OBSERVATION_CONSTRUCTOR),
		"constructor observation is missing");
	bPassed &= Require(
		HasEvent(asSEMANTIC_OBSERVATION_ASSIGNMENT),
		"assignment observation is missing");
	bPassed &= Require(
		HasEvent(asSEMANTIC_OBSERVATION_CONSTANT_STRING),
		"constant-string observation is missing");

	const auto ResolvedCall = std::find_if(
		Observer.Events.begin(),
		Observer.Events.end(),
		[ObservedInt](const FObservedEvent& Event)
		{
			return Event.Kind
					== asSEMANTIC_OBSERVATION_RESOLVED_CALL
				&& Event.FunctionId == ObservedInt;
		});
	bPassed &= Require(
		ResolvedCall != Observer.Events.end()
			&& ResolvedCall->FunctionId != ObservedUInt
			&& ResolvedCall->Arguments.size() == 1
			&& ResolvedCall->Arguments[0].ActualTypeId
				== ObservedEngine->GetTypeIdByDecl("int")
			&& ResolvedCall->Arguments[0].ParameterTypeId
				== ObservedEngine->GetTypeIdByDecl("int"),
		"observer did not expose the selected overload and argument types");
	if (ResolvedCall != Observer.Events.end())
	{
		bPassed &= Require(
			ResolvedCall->Section == "SemanticObserver.as"
				&& ResolvedCall->Row == 3
				&& ResolvedCall->Column > 0
				&& ResolvedCall->SourceLength > 0,
			"resolved-call source location is incomplete");
	}

	const auto Constructor = std::find_if(
		Observer.Events.begin(),
		Observer.Events.end(),
		[ObservedConstructor](const FObservedEvent& Event)
		{
			return Event.Kind
					== asSEMANTIC_OBSERVATION_CONSTRUCTOR
				&& Event.FunctionId == ObservedConstructor;
		});
	bPassed &= Require(
		Constructor != Observer.Events.end()
			&& Constructor->TargetTypeId
				== ObservedEngine->GetTypeIdByDecl("FObserved"),
		"observer did not expose the resolved constructor type");

	const auto Assignment = std::find_if(
		Observer.Events.begin(),
		Observer.Events.end(),
		[](const FObservedEvent& Event)
		{
			return Event.Kind
					== asSEMANTIC_OBSERVATION_ASSIGNMENT
				&& Event.SourceTypeId == Event.TargetTypeId
				&& Event.TargetTypeId == asTYPEID_INT32;
		});
	bPassed &= Require(
		Assignment != Observer.Events.end(),
		"observer did not expose resolved assignment types");

	const auto Constant = std::find_if(
		Observer.Events.begin(),
		Observer.Events.end(),
		[](const FObservedEvent& Event)
		{
			return Event.Kind
					== asSEMANTIC_OBSERVATION_CONSTANT_STRING
				&& Event.ConstantString
					== "/Game/Observed.Asset";
		});
	bPassed &= Require(
		Constant != Observer.Events.end(),
		"observer did not expose the bounded constant string");

	AngelscriptStandalone::FRegistrationRuntimeMap RuntimeMap;
	RuntimeMap.FunctionIdByStableId.emplace(
		"callable:Select:int",
		StableInt);
	RuntimeMap.FunctionIdByStableId.emplace(
		"callable:Select:uint",
		StableUInt);
	RuntimeMap.FunctionIdByStableId.emplace(
		"behaviour:FObserved:construct:int",
		StableConstructor);
	RuntimeMap.TypeIdByStableId.emplace(
		"type:FObserved",
		StableEngine->GetTypeIdByDecl("FObserved"));
	AngelscriptStandalone::FStandaloneSemanticObserver StableObserver(
		*StableEngine,
		RuntimeMap);
	StableEngine->SetUserData(
		&StableObserver,
		asSEMANTIC_OBSERVER_USER_DATA_ID);
	FMemoryByteCodeStream StableByteCode;
	bPassed &= Require(
		Compile(*StableEngine, "semantic", StableByteCode),
		"stable semantic observation compilation failed");
	const auto& StableObservations =
		StableObserver.GetObservations();
	const auto StableCall = std::find_if(
		StableObservations.begin(),
		StableObservations.end(),
		[](const AngelscriptStandalone::FSemanticObservation& Event)
		{
			return Event.Kind
					== AngelscriptStandalone::
						ESemanticObservationKind::ResolvedCall
				&& Event.StableFunctionId
					== "callable:Select:int";
		});
	bPassed &= Require(
		StableCall != StableObservations.end()
			&& StableCall->Arguments.size() == 1
			&& StableCall->Arguments[0].ActualStableTypeId
				== "builtin:int"
			&& StableCall->Arguments[0].ParameterStableTypeId
				== "builtin:int",
		"runtime descriptors were not translated to stable identities");
	const auto StableConstruct = std::find_if(
		StableObservations.begin(),
		StableObservations.end(),
		[](const AngelscriptStandalone::FSemanticObservation& Event)
		{
			return Event.Kind
					== AngelscriptStandalone::
						ESemanticObservationKind::Constructor
				&& Event.StableFunctionId
					== "behaviour:FObserved:construct:int"
				&& Event.TargetStableTypeId
					== "type:FObserved";
		});
	bPassed &= Require(
		StableConstruct != StableObservations.end(),
		"constructor descriptor was not translated to stable identities");
	std::string AddressFreeEvidence;
	for (const auto& Event : StableObservations)
	{
		AddressFreeEvidence +=
			AngelscriptStandalone::ToString(Event.Kind);
		AddressFreeEvidence += Event.StableFunctionId;
		AddressFreeEvidence += Event.SourceStableTypeId;
		AddressFreeEvidence += Event.TargetStableTypeId;
		AddressFreeEvidence += Event.LogicalPath;
		AddressFreeEvidence += Event.ConstantString;
	}
	bPassed &= Require(
		AddressFreeEvidence.find("0x")
			== std::string::npos,
		"normalized semantic evidence contains a process address");

	ObservedEngine->SetUserData(
		nullptr,
		asSEMANTIC_OBSERVER_USER_DATA_ID);
	StableEngine->SetUserData(
		nullptr,
		asSEMANTIC_OBSERVER_USER_DATA_ID);
	ObservedEngine->ShutDownAndRelease();
	BaselineEngine->ShutDownAndRelease();
	StableEngine->ShutDownAndRelease();
	asThreadCleanup();
	return bPassed ? 0 : 1;
}
