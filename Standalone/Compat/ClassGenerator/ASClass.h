#pragma once

#include <cstddef>

class asIScriptEngine;
class asIScriptObject;
class asITypeInfo;

class UASClass
{
public:
	static void* AllocScriptObject(asITypeInfo* ScriptType, std::size_t Size);
	static void FinishConstructObject(asIScriptObject* ScriptObject, asITypeInfo* ScriptType);
	static void RegisterRawScriptObject(void* ScriptObject, asITypeInfo* ScriptType);
	static bool AddRawScriptObjectReference(
		const void* ScriptObject,
		asITypeInfo* ExpectedScriptType);
	static bool BeginReleaseRawScriptObjectReference(
		const void* ScriptObject,
		asITypeInfo* ExpectedScriptType,
		bool& bOutRunDestructor,
		bool& bOutFreeWithoutDestructor);
	static bool FinishReleaseRawScriptObjectReference(
		const void* ScriptObject,
		asITypeInfo* ExpectedScriptType);
	static void UnregisterRawScriptObject(const void* ScriptObject);
	static void UnregisterRawScriptObjectsForEngine(const asIScriptEngine* ScriptEngine);
	static asITypeInfo* GetRawScriptObjectType(const void* ScriptObject);
};
