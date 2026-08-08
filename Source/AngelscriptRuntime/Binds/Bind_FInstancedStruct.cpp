#include "AngelscriptBinds.h"

#include "StructUtils/InstancedStruct.h"

#include "Bind_FInstancedStruct_Functions.h"

namespace
{
	void BindFInstancedStructFunctions(FAngelscriptBinds& Binds)
	{
		auto FAngelscriptAnyStructParameter_ = Binds.ExistingClassForTarget("FAngelscriptAnyStructParameter");
		FAngelscriptAnyStructParameter_
			.ImplicitConstructor(
				"void f(const ?&in Struct)",
				FUNC(FAngelscriptFInstancedStructBinds::ImplicitConstructAnyStruct))
			.NoDiscard();
		FAngelscriptAnyStructParameter_
			.ImplicitConstructor(
				"void f(const FInstancedStruct& Struct)",
				FUNC(FAngelscriptFInstancedStructBinds::ImplicitConstructAnyStructFromInstancedStruct))
			.NoDiscard();

		auto FInstancedStruct_ = Binds.ExistingClassForTarget("FInstancedStruct");

		FInstancedStruct_
			.Method(
				"bool opEquals(const FInstancedStruct& Other) const",
				METHODPR(bool, FInstancedStruct, operator==, (const FInstancedStruct&) const))
			.Documentation(TEXT("Comparison operators. Deep compares the struct instance when identical."));

		FInstancedStruct_
			.Method(
				"void InitializeAs(const ?&in Struct)",
				FUNC(FAngelscriptFInstancedStructBinds::InitializeAsStruct))
			.Documentation(TEXT("Initializes from struct type and emplace construct."));

		FInstancedStruct_
			.Method(
				"void InitializeAs(const UScriptStruct StructType)",
				FUNC(FAngelscriptFInstancedStructBinds::InitializeAsDefault))
			.Documentation(TEXT("Default initializes a struct of this type"));

		FInstancedStruct_
			.Method(
				"const FScriptStructWildcard& Get(const UScriptStruct StructType) const no_discard",
				FUNC(FAngelscriptFInstancedStructBinds::GetMemory))
			.Documentation(TEXT("Returns struct data of a particular type. Throws an exception if the instanced struct does not contain a struct of this type."))
			.DeterminesOutputType(0);

		FInstancedStruct_
			.Method(
				"FScriptStructWildcard& GetMutable(const UScriptStruct StructType) no_discard",
				FUNC(FAngelscriptFInstancedStructBinds::GetMemory))
			.Documentation(TEXT("Returns struct data of a particular type. Throws an exception if the instanced struct does not contain a struct of this type."))
			.DeterminesOutputType(0);

		FInstancedStruct_
			.Method("void Get(?&out Struct) const", &FAngelscriptFInstancedStructBinds::CopyTo)
			.Documentation(TEXT("Returns a copy of the struct. This getter assumes that all data is valid."))
			.Deprecated("Use Get() or GetMutable() that returns a reference instead of copying");

		FInstancedStruct_.Method("void Reset()", METHOD(FInstancedStruct, Reset));
		FInstancedStruct_
			.Method(
				"bool Contains(const UScriptStruct StructType) const",
				FUNC(FAngelscriptFInstancedStructBinds::Contains))
			.Documentation(TEXT("Check whether the instanced struct contains a struct of this type"));

		FInstancedStruct_
			.Method("bool IsValid() const", METHOD_TRIVIAL(FInstancedStruct, IsValid))
			.Documentation(TEXT("Returns True if the struct is valid."));

		FInstancedStruct_
			.Method("UScriptStruct GetScriptStruct() const", METHOD_TRIVIAL(FInstancedStruct, GetScriptStruct))
			.Documentation(TEXT("Get the type of struct contained within the instanced struct"));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FInstancedStruct");
		Binds.BindGlobalFunctionForTarget(
			"FInstancedStruct Make(const ?&in Struct) no_discard",
			FUNC(FAngelscriptFInstancedStructBinds::Make))
			.Documentation(TEXT("Creates a new FInstancedStruct from struct."));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FInstancedStruct(
	TEXT("FInstancedStruct.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFInstancedStructFunctions);
