#pragma once

#include "Containers/Map.h"

class asITypeInfo;
class asIScriptEngine;

using FAngelscriptStaticTypeInfoClearer = void (*)(asIScriptEngine* ScriptEngine);

struct ANGELSCRIPTRUNTIME_API FAngelscriptStaticTypeInfoRegistry
{
	static void RegisterClearer(FAngelscriptStaticTypeInfoClearer Clearer);
	static void ClearForEngine(asIScriptEngine* ScriptEngine);
};

template <typename T>
struct TGetStaticTypeInfo
{
	static void SetForEngine(asIScriptEngine* ScriptEngine, asITypeInfo* TypeInfo)
	{
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FAngelscriptStaticTypeInfoRegistry::RegisterClearer(&ClearForEngine);

		if (TypeInfo == nullptr)
		{
			GetTypeInfosByEngine().Remove(ScriptEngine);
			return;
		}

		GetTypeInfosByEngine().Add(ScriptEngine, TypeInfo);
	}

	static asITypeInfo* GetForEngine(asIScriptEngine* ScriptEngine)
	{
		if (ScriptEngine == nullptr)
		{
			return nullptr;
		}

		asITypeInfo** TypeInfo = GetTypeInfosByEngine().Find(ScriptEngine);
		return TypeInfo != nullptr ? *TypeInfo : nullptr;
	}

	static bool IsForEngine(asIScriptEngine* ScriptEngine, asITypeInfo* TypeInfo)
	{
		return TypeInfo != nullptr && GetForEngine(ScriptEngine) == TypeInfo;
	}

	static void ClearForEngine(asIScriptEngine* ScriptEngine)
	{
		if (ScriptEngine != nullptr)
		{
			GetTypeInfosByEngine().Remove(ScriptEngine);
		}
	}

private:
	static TMap<asIScriptEngine*, asITypeInfo*>& GetTypeInfosByEngine()
	{
		static TMap<asIScriptEngine*, asITypeInfo*> TypeInfosByEngine;
		return TypeInfosByEngine;
	}
};
