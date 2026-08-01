#include "Adapters/AngelscriptAdapterRegistry.h"
#include "Adapters/AngelscriptTemplateTraits.h"
#include "Contract/AngelscriptOfflineIndices.h"
#include "Host/AngelscriptStandaloneDiagnosticSink.h"
#include "Registration/AngelscriptRegistrationLoader.h"
#include "Registration/AngelscriptRegistrationPlan.h"
#include "Registration/AngelscriptScriptBaselinePlan.h"

#include "angelscript.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
	using namespace AngelscriptStandalone;

	bool Require(
		const bool bCondition,
		const std::string_view Message)
	{
		if (!bCondition)
		{
			std::cerr << Message << '\n';
		}
		return bCondition;
	}
}

int main()
{
	using namespace AngelscriptStandalone;
	bool bPassed = true;

	FOfflineManifest Manifest;
	Manifest.EngineProperties.emplace(
		"angelscript.fork",
		"2.33+selective-2.38");
	Manifest.EngineProperties.emplace("unreal.major", "5");
	Manifest.EngineProperties.emplace("unreal.minor", "8");
	FOfflineAdapterDescriptor Array;
	Array.StableId =
		"8a3420687583f868141cd298e66ba350de2399125d7f7c939f58d5e31a8977d3";
	Array.Name = "TArray";
	Array.Version = "1";
	Array.SurfaceHash =
		"bfb612167131d4d6ca05323d7802bdd03cb5d44f3d317157898951e38f29351f";
	Array.RequiredTraits = {
		"element.construct",
		"element.destruct",
		"element.copy",
	};
	Array.RequiredEngineProperties = {
		"angelscript.fork",
		"unreal.major",
		"unreal.minor",
	};
	Array.bDeclarativeOnly = false;
	Manifest.Adapters.push_back(Array);
	bPassed &= Require(
		ValidateAdapterHandshake(Manifest).bSuccess,
		"known adapter handshake was rejected");

	Manifest.Adapters[0].SurfaceHash = std::string(64, '0');
	bPassed &= Require(
		!ValidateAdapterHandshake(Manifest).bSuccess,
		"changed adapter surface hash was accepted");
	Manifest.Adapters[0] = Array;
	Manifest.Adapters[0].bDeclarativeOnly = true;
	bPassed &= Require(
		!ValidateAdapterHandshake(Manifest).bSuccess,
		"non-declarative adapter was accepted as declarative-only");
	Manifest.Adapters[0] = Array;
	Manifest.Adapters[0].StableId = std::string(64, 'f');
	bPassed &= Require(
		!ValidateAdapterHandshake(Manifest).bSuccess,
		"unknown adapter ID was accepted");

	FOfflineTypeRecord Type;
	Type.Kind = "value";
	Type.CompileSize = 12;
	Type.CompileAlignment = 4;
	Type.Traits.bConstructible = true;
	Type.Traits.bDestructible = true;
	Type.Traits.bCopyConstructible = true;
	Type.Traits.bCopyAssignable = true;
	Type.Traits.bComparable = true;
	Type.Traits.bHashable = true;
	Type.Traits.bTemplateEligible = true;
	const FTemplateTraits Traits = DeriveTemplateTraits(Type);
	bPassed &= Require(
		ValidateTemplateTraits(
			ETemplateFamily::Array,
			Traits).bSuccess
			&& ValidateTemplateTraits(
				ETemplateFamily::MapKey,
				Traits).bSuccess
			&& ValidateTemplateTraits(
				ETemplateFamily::Set,
				Traits).bSuccess,
		"complete value traits were rejected");
	bPassed &= Require(
		ValidateAdapterTemplateTraits(
			"TArray",
			{Traits}).bSuccess
			&& ValidateAdapterTemplateTraits(
				"TMap",
				{Traits, Traits}).bSuccess
			&& ValidateAdapterTemplateTraits(
				"TSet",
				{Traits}).bSuccess
			&& ValidateAdapterTemplateTraits(
				"TOptional",
				{Traits}).bSuccess,
		"container adapter family traits were not applied");

	Type.Traits.bHashable = false;
	const FTemplateTraitValidation MissingHash =
		ValidateTemplateTraits(
			ETemplateFamily::Set,
			DeriveTemplateTraits(Type));
	bPassed &= Require(
		!MissingHash.bSuccess
			&& MissingHash.MissingTrait == "hash",
		"missing hash trait was not diagnosed");
	bPassed &= Require(
		!ValidateAdapterTemplateTraits(
			"TMap",
			{DeriveTemplateTraits(Type), Traits}).bSuccess
			&& !ValidateAdapterTemplateTraits(
				"TSet",
				{DeriveTemplateTraits(Type)}).bSuccess,
		"map/set adapter validation ignored key hash traits");

	std::uint64_t Size = 0;
	std::uint64_t Alignment = 0;
	bPassed &= Require(
		ComputeOptionalCompileLayout(
			Traits,
			Size,
			Alignment)
			&& Size == 16
			&& Alignment == 4,
		"optional compile-only layout is not deterministic");
	FTemplateTraits InvalidAlignment = Traits;
	InvalidAlignment.ValueAlignment = 3;
	bPassed &= Require(
		!ComputeOptionalCompileLayout(
			InvalidAlignment,
			Size,
			Alignment),
		"invalid optional alignment was accepted");
	FTemplateTraits Overflow = Traits;
	Overflow.ValueSize =
		std::numeric_limits<std::uint64_t>::max();
	bPassed &= Require(
		!ComputeOptionalCompileLayout(
			Overflow,
			Size,
			Alignment),
		"overflowing optional layout was accepted");

	FOfflineTypeRecord Object;
	Object.Kind = "reference";
	Object.bHandle = true;
	Object.Traits.bComparable = true;
	Object.Traits.bHashable = true;
	const FTemplateTraits ObjectHandleTraits =
		DeriveTemplateTraits(Object);
	bPassed &= Require(
		ValidateTemplateTraits(
			ETemplateFamily::ObjectWrapper,
			ObjectHandleTraits).bSuccess,
		"reference subtype was rejected by object wrapper traits");
	bPassed &= Require(
		ValidateTemplateTraits(
			ETemplateFamily::Set,
			ObjectHandleTraits).bSuccess
			&& ValidateTemplateTraits(
				ETemplateFamily::MapKey,
				ObjectHandleTraits).bSuccess,
		"object handles should use pointer value semantics as set/map keys");
	bPassed &= Require(
		!ValidateTemplateTraits(
			ETemplateFamily::ObjectWrapper,
			Traits).bSuccess,
		"value subtype was accepted by object wrapper traits");
	FOfflineTypeRecord NonHandleReference = Object;
	NonHandleReference.bHandle = false;
	bPassed &= Require(
		!ValidateTemplateTraits(
			ETemplateFamily::ObjectWrapper,
			DeriveTemplateTraits(NonHandleReference)).bSuccess
			&& !ValidateTemplateTraits(
				ETemplateFamily::Set,
				DeriveTemplateTraits(NonHandleReference)).bSuccess,
		"non-handle references were accepted as container values");
	bPassed &= Require(
		ValidateAdapterTemplateTraits(
			"TObjectPtr",
			{ObjectHandleTraits}).bSuccess
			&& ValidateAdapterTemplateTraits(
				"TSubclassOf",
				{ObjectHandleTraits}).bSuccess
			&& !ValidateAdapterTemplateTraits(
				"TSoftClassPtr",
				{Traits}).bSuccess,
		"object/class wrapper adapter traits are inconsistent");
	bPassed &= Require(
		IsNestedTemplateAllowed("TArray", "TSubclassOf")
			&& IsNestedTemplateAllowed("TArray", "TArray")
			&& IsNestedTemplateAllowed("TSet", "TObjectPtr")
			&& IsNestedTemplateAllowed("TMap", "TOptional")
			&& IsNestedTemplateAllowed("TOptional", "TSoftObjectPtr")
			&& !IsNestedTemplateAllowed("TObjectPtr", "TArray")
			&& !IsNestedTemplateAllowed("TArray", "UnknownTemplate"),
		"nested-template composition policy is inconsistent");
	const FTemplateTraits ObjectPointerInstance =
		DeriveAdapterInstanceTraits("TObjectPtr", 8, 8);
	const FTemplateTraits ArrayInstance =
		DeriveAdapterInstanceTraits("TArray", 24, 8);
	bPassed &= Require(
		ValidateTemplateTraits(
			ETemplateFamily::Set,
			ObjectPointerInstance).bSuccess
			&& ValidateTemplateTraits(
				ETemplateFamily::MapKey,
				ObjectPointerInstance).bSuccess,
		"object-wrapper instances should be valid set/map keys");
	bPassed &= Require(
		ValidateTemplateTraits(
			ETemplateFamily::MapValue,
			ArrayInstance).bSuccess
			&& !ValidateTemplateTraits(
				ETemplateFamily::Set,
				ArrayInstance).bSuccess,
		"nested containers should be copyable values but not implicitly hashable keys");

	FOfflineSymbolRecord ArrayType;
	ArrayType.StableId = std::string(64, 'a');
	ArrayType.Kind = "type";
	ArrayType.Type.StableId = ArrayType.StableId;
	ArrayType.Type.Kind = "template";
	ArrayType.Type.Name = "TArray";
	ArrayType.Type.CompleteDeclaration = "TArray<class T>";
	ArrayType.Type.AdapterStableId = Array.StableId;
	ArrayType.Type.bTemplateDefinition = true;
	ArrayType.Type.TemplateSubtypeDeclarations = {"class T"};
	ArrayType.Origin.Layer = "host-surface";
	ArrayType.Origin.Kind = "manual";
	FOfflineSymbolRecord Constructor;
	Constructor.StableId = std::string(64, 'b');
	Constructor.Kind = "callable";
	Constructor.Callable.StableId = Constructor.StableId;
	Constructor.Callable.OwnerStableId = ArrayType.StableId;
	Constructor.Callable.Kind = "constructor";
	Constructor.Callable.Name = "$beh0";
	Constructor.Callable.Declaration = "$beh0()";
	Constructor.Callable.Behavior = "construct";
	Constructor.Origin = ArrayType.Origin;
	FOfflineSymbolRecord Destructor = Constructor;
	Destructor.StableId = std::string(64, 'c');
	Destructor.Callable.StableId = Destructor.StableId;
	Destructor.Callable.Kind = "destructor";
	Destructor.Callable.Name = "$beh2";
	Destructor.Callable.Declaration = "$beh2()";
	Destructor.Callable.Behavior = "destruct";
	FOfflineSymbolRecord ObjectType;
	ObjectType.StableId = std::string(64, 'd');
	ObjectType.Kind = "type";
	ObjectType.Type.StableId = ObjectType.StableId;
	ObjectType.Type.Kind = "reference";
	ObjectType.Type.Name = "UObject";
	ObjectType.Type.CompleteDeclaration = "UObject";
	ObjectType.Origin = ArrayType.Origin;
	FOfflineSymbolRecord ObjectArrayFunction;
	ObjectArrayFunction.StableId = std::string(64, 'e');
	ObjectArrayFunction.Kind = "callable";
	ObjectArrayFunction.Callable.StableId =
		ObjectArrayFunction.StableId;
	ObjectArrayFunction.Callable.OwnerStableId = ObjectType.StableId;
	ObjectArrayFunction.Callable.Kind = "method";
	ObjectArrayFunction.Callable.Name = "Gather";
	ObjectArrayFunction.Callable.Declaration =
		"void Gather(UObject[]&out Values)";
	ObjectArrayFunction.Origin = ArrayType.Origin;
	FOfflineSymbolRecord EarlyArrayFunction;
	EarlyArrayFunction.StableId = std::string(64, '0');
	EarlyArrayFunction.Kind = "callable";
	EarlyArrayFunction.Callable.StableId =
		EarlyArrayFunction.StableId;
	EarlyArrayFunction.Callable.OwnerStableId = ObjectType.StableId;
	EarlyArrayFunction.Callable.Kind = "method";
	EarlyArrayFunction.Callable.Name = "Consume";
	EarlyArrayFunction.Callable.Declaration =
		"void Consume(const TArray<int>&in Values)";
	EarlyArrayFunction.Origin = ArrayType.Origin;
	FOfflineSymbolRecord FloatMethod;
	FloatMethod.StableId = std::string(64, 'f');
	FloatMethod.Kind = "callable";
	FloatMethod.Callable.StableId = FloatMethod.StableId;
	FloatMethod.Callable.OwnerStableId = ObjectType.StableId;
	FloatMethod.Callable.Kind = "method";
	FloatMethod.Callable.Name = "Scale";
	FloatMethod.Callable.Declaration =
		"float Scale(float Value) final";
	FloatMethod.Origin = ArrayType.Origin;
	FOfflineSymbolRecord ProtectedProperty;
	ProtectedProperty.StableId = std::string(64, '1');
	ProtectedProperty.Kind = "property";
	ProtectedProperty.Property.StableId = ProtectedProperty.StableId;
	ProtectedProperty.Property.OwnerStableId = ObjectType.StableId;
	ProtectedProperty.Property.Name = "Restricted";
	ProtectedProperty.Property.Declaration =
		"protected UObject Restricted";
	ProtectedProperty.Origin = ArrayType.Origin;
	FOfflineSymbolRecord StringType;
	StringType.StableId = std::string(64, '2');
	StringType.Kind = "type";
	StringType.Type.StableId = StringType.StableId;
	StringType.Type.Kind = "value";
	StringType.Type.Name = "FString";
	StringType.Type.CompleteDeclaration = "FString";
	StringType.Type.CompileSize = 16;
	StringType.Type.CompileAlignment = 8;
	StringType.Origin = ArrayType.Origin;
	FOfflineSymbolRecord PrintFunction;
	PrintFunction.StableId = std::string(64, '3');
	PrintFunction.Kind = "callable";
	PrintFunction.Callable.StableId = PrintFunction.StableId;
	PrintFunction.Callable.Kind = "global-function";
	PrintFunction.Callable.Name = "Print";
	PrintFunction.Callable.Declaration =
		"void Print(const FString&in Value)";
	PrintFunction.Origin = ArrayType.Origin;
	FOfflineSymbolRecord ArrayIteratorType;
	ArrayIteratorType.StableId = std::string(64, '4');
	ArrayIteratorType.Kind = "type";
	ArrayIteratorType.Type.StableId = ArrayIteratorType.StableId;
	ArrayIteratorType.Type.Kind = "template";
	ArrayIteratorType.Type.Name = "TArrayIterator";
	ArrayIteratorType.Type.CompleteDeclaration =
		"TArrayIterator<class T>";
	ArrayIteratorType.Type.AdapterStableId = Array.StableId;
	ArrayIteratorType.Type.CompileSize = 24;
	ArrayIteratorType.Type.CompileAlignment = 8;
	ArrayIteratorType.Type.bTemplateDefinition = true;
	ArrayIteratorType.Type.TemplateSubtypeDeclarations = {"T"};
	ArrayIteratorType.Origin = ArrayType.Origin;
	FOfflineSymbolRecord IteratorCanProceed;
	IteratorCanProceed.StableId = std::string(64, '5');
	IteratorCanProceed.Kind = "property";
	IteratorCanProceed.Property.StableId =
		IteratorCanProceed.StableId;
	IteratorCanProceed.Property.OwnerStableId =
		ArrayIteratorType.StableId;
	IteratorCanProceed.Property.Name = "CanProceed";
	IteratorCanProceed.Property.Declaration = "bool CanProceed";
	IteratorCanProceed.Property.AdapterStableId = Array.StableId;
	IteratorCanProceed.Origin = ArrayType.Origin;
	FOfflineSymbolRecord IteratorProceed;
	IteratorProceed.StableId = std::string(64, '6');
	IteratorProceed.Kind = "callable";
	IteratorProceed.Callable.StableId = IteratorProceed.StableId;
	IteratorProceed.Callable.OwnerStableId =
		ArrayIteratorType.StableId;
	IteratorProceed.Callable.Kind = "method";
	IteratorProceed.Callable.Name = "Proceed";
	IteratorProceed.Callable.Declaration = "T& Proceed()";
	IteratorProceed.Callable.AdapterStableId = Array.StableId;
	IteratorProceed.Origin = ArrayType.Origin;
	FOfflineSymbolRecord ArrayIteratorMethod;
	ArrayIteratorMethod.StableId = std::string(64, '7');
	ArrayIteratorMethod.Kind = "callable";
	ArrayIteratorMethod.Callable.StableId =
		ArrayIteratorMethod.StableId;
	ArrayIteratorMethod.Callable.OwnerStableId = ArrayType.StableId;
	ArrayIteratorMethod.Callable.Kind = "method";
	ArrayIteratorMethod.Callable.Name = "Iterator";
	ArrayIteratorMethod.Callable.Declaration =
		"TArrayIterator<T> Iterator()";
	ArrayIteratorMethod.Callable.AdapterStableId = Array.StableId;
	ArrayIteratorMethod.Origin = ArrayType.Origin;
	FOfflineSymbolRecord StringAddObject;
	StringAddObject.StableId = std::string(64, '8');
	StringAddObject.Kind = "callable";
	StringAddObject.Callable.StableId = StringAddObject.StableId;
	StringAddObject.Callable.OwnerStableId = StringType.StableId;
	StringAddObject.Callable.Kind = "method";
	StringAddObject.Callable.Name = "opAdd";
	StringAddObject.Callable.Declaration =
		"FString opAdd(const UObject Value) const";
	StringAddObject.Origin = ArrayType.Origin;
	FOfflineSymbolRecord ActorType;
	ActorType.StableId = std::string(64, '9');
	ActorType.Kind = "type";
	ActorType.Type.StableId = ActorType.StableId;
	ActorType.Type.Kind = "reference";
	ActorType.Type.Name = "AActor";
	ActorType.Type.CompleteDeclaration = "AActor";
	ActorType.Type.BaseStableId = ObjectType.StableId;
	ActorType.Type.bHandle = true;
	ActorType.Origin = ArrayType.Origin;
	FOfflineSymbolRecord StringConstructor;
	StringConstructor.StableId = std::string(64, 'A');
	StringConstructor.Kind = "callable";
	StringConstructor.Callable.StableId =
		StringConstructor.StableId;
	StringConstructor.Callable.OwnerStableId = StringType.StableId;
	StringConstructor.Callable.Kind = "constructor";
	StringConstructor.Callable.Name = "$beh0";
	StringConstructor.Callable.Declaration = "$beh0()";
	StringConstructor.Callable.Behavior = "construct";
	StringConstructor.Origin = ArrayType.Origin;
	FOfflineSymbolRecord CharacterType;
	CharacterType.StableId = std::string(64, 'B');
	CharacterType.Kind = "type";
	CharacterType.Type.StableId = CharacterType.StableId;
	CharacterType.Type.Kind = "reference";
	CharacterType.Type.Name = "ACharacter";
	CharacterType.Type.CompleteDeclaration = "ACharacter";
	CharacterType.Type.BaseStableId = ActorType.StableId;
	CharacterType.Type.bHandle = true;
	CharacterType.Origin = ArrayType.Origin;
	std::string IndexError;
	const auto Indices = FOfflineBundleIndices::Build(
		{
			std::move(ArrayType),
			std::move(Constructor),
			std::move(Destructor),
			std::move(ObjectType),
			std::move(ObjectArrayFunction),
			std::move(EarlyArrayFunction),
			std::move(FloatMethod),
			std::move(ProtectedProperty),
			std::move(StringType),
			std::move(PrintFunction),
			std::move(ArrayIteratorType),
			std::move(IteratorCanProceed),
			std::move(IteratorProceed),
			std::move(ArrayIteratorMethod),
			std::move(StringAddObject),
			std::move(ActorType),
			std::move(StringConstructor),
			std::move(CharacterType),
		},
		{},
		{},
		IndexError);
	bPassed &= Require(
		Indices != nullptr,
		"adapter template fixture index failed");
	if (Indices != nullptr)
	{
		const FScriptBaselinePlan Baseline =
			BuildScriptBaselinePlan(*Indices, {});
		Manifest.Adapters = {Array};
		const FRegistrationPlan Plan =
			BuildRegistrationPlan(Manifest, Baseline, {});
		asIScriptEngine* Engine = asCreateScriptEngine();
		FDiagnosticSink DiagnosticSink;
		if (Engine != nullptr)
		{
			Engine->SetMessageCallback(
				asFUNCTION(FDiagnosticSink::MessageCallback),
				&DiagnosticSink,
				asCALL_CDECL);
		}
		const FRegistrationLoadResult Loaded =
			ApplyRegistrationPlan(Engine, Plan, Manifest);
		asIScriptModule* Module = Engine != nullptr
			? Engine->GetModule("adapter", asGM_ALWAYS_CREATE)
			: nullptr;
		const char* Source =
			"void Validate(const TArray<int>&in Values) {}\n"
			"void ValidateObjects(const TArray<UObject>&in Values) {}\n"
			"void Construct() { TArray<int> Values; }\n"
			"void CallNamed(UObject Value) { Value.Scale(Value=1.0); }\n"
			"void CallString() { Print(\"ok\"); }\n"
			"void ConcatObject(ACharacter Value) { Print(\"Object: \" + Value); }\n"
			"void Iterate(TArray<int>& Values) {\n"
			"  for (auto It = Values.Iterator(); It.CanProceed;) {\n"
			"    int Value = It.Proceed();\n"
			"  }\n"
			"}\n";
		const bool bCompiled = Loaded.bSuccess
			&& Module != nullptr
			&& Module->AddScriptSection("adapter.as", Source) >= 0
			&& Module->Build() >= 0;
		if (!bCompiled)
		{
			for (const FDiagnostic& Diagnostic
				: DiagnosticSink.GetDiagnostics())
			{
				std::cerr << Diagnostic.Section << ":"
					<< Diagnostic.Row << ":"
					<< Diagnostic.Column << ": "
					<< Diagnostic.Message << '\n';
			}
		}
		bPassed &= Require(
			bCompiled,
			Loaded.bSuccess
				? (DiagnosticSink.GetDiagnostics().empty()
					? "known adapter template did not instantiate for compilation"
					: DiagnosticSink.GetDiagnostics().back().Message)
				: Loaded.Error);
		if (Engine != nullptr)
		{
			Engine->ShutDownAndRelease();
		}
		asThreadCleanup();
	}

	return bPassed ? 0 : 1;
}
