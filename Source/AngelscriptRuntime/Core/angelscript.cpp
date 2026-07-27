#include "angelscript.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

asITypeInfo* asIScriptObject::GetObjectType() const
{
	if (asITypeInfo* RawScriptType = UASClass::GetRawScriptObjectType(this))
		return RawScriptType;

	if (asITypeInfo* ScriptStructType = UASStruct::GetScriptTypeFromValue(this))
		return ScriptStructType;

	//return (asITypeInfo*)((UObject*)this)->GetClass()->ScriptTypePtr;
	UASClass* asClass = UASClass::GetFirstASClass((UObject*)this);
	if (asClass)
		return (asITypeInfo*)asClass->ScriptTypePtr;
	else
		return nullptr;
}
