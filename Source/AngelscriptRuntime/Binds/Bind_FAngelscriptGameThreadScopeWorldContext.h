#pragma once

class UObject;
struct FAngelscriptGameThreadScopeWorldContext;

struct FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds
{
	static void Construct(FAngelscriptGameThreadScopeWorldContext* Address, UObject* WorldContext);
	static void Destruct(FAngelscriptGameThreadScopeWorldContext& Scope);
};
