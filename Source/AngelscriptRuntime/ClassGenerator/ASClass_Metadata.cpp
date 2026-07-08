#include "ClassGenerator/ASClass.h"

#include "AngelscriptEngine.h"

#include "UObject/CoreNet.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptobject.h"
#include "EndAngelscriptHeaders.h"

void UASClass::GetLifetimeScriptReplicationList(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	for (TFieldIterator<FProperty> It(this, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		FProperty * Prop = *It;
		if (Prop != NULL && Prop->GetPropertyFlags() & CPF_Net)
		{
			OutLifetimeProps.AddUnique(FLifetimeProperty(Prop->RepIndex, Prop->GetBlueprintReplicationCondition()));
		}
	}

	UASClass* SuperScriptClass = Cast<UASClass>(GetSuperStruct());
	if (SuperScriptClass != NULL)
	{
		SuperScriptClass->GetLifetimeScriptReplicationList(OutLifetimeProps);
	}
}

UClass* UASClass::GetMostUpToDateClass()
{
#if !AS_CAN_HOTRELOAD
	return this;
#else
	if (NewerVersion == nullptr)
		return this;

	UASClass* NewerClass = NewerVersion;
	while (NewerClass->NewerVersion != nullptr)
		NewerClass = NewerClass->NewerVersion;
	return NewerClass;
#endif
}

UASClass* UASClass::GetFirstASClass(UClass* Class)
{
	UClass* Parent = Class;
	while (Parent != nullptr)
	{
		if (Cast<UASClass>(Parent) != nullptr)
			return (UASClass*)Parent;
		Parent = Parent->GetSuperClass();
	}
	return nullptr;
}

UASClass* UASClass::GetFirstASClass(UObject* Object)
{
	UClass* Parent = Object->GetClass();
	while (Parent != nullptr)
	{
		if (Cast<UASClass>(Parent) != nullptr)
			return (UASClass*)Parent;
		Parent = Parent->GetSuperClass();
	}
	return nullptr;
}

UClass* UASClass::GetFirstASOrNativeClass(UClass* Class)
{
	UClass* Parent = Class;
	while (Parent != nullptr)
	{
		if (Cast<UASClass>(Parent) != nullptr)
			return Parent;
		if (Parent->HasAnyClassFlags(CLASS_Native))
			return Parent;
		Parent = Parent->GetSuperClass();
	}
	return nullptr;
}

void UASClass::RuntimeDestroyObject(UObject* Object)
{
#if WITH_AS_DEBUGVALUES
	if (Object->Debug != nullptr)
		DebugValues.Free(Object->Debug);
#endif

	if (ScriptTypePtr == nullptr)
		return;

	auto* ScriptObject = (asCScriptObject*)(Object);
	ScriptObject->CallDestructor((asCObjectType*)ScriptTypePtr);
}

bool UASClass::IsFunctionImplementedInScript(FName InFunctionName) const
{
	UFunction* Function = FindFunctionByName(InFunctionName);
	//return Function && Function->GetOuterUClass() && Function->GetOuterUClass()->bIsScriptClass;
	UASFunction* asFunction = Cast<UASFunction>(Function);
	// UE 5.7+: after DiscardModule, UASFunction objects remain in the FuncMap
	// but with ScriptFunction set to nullptr. Check that the backing script
	// function is still alive before reporting as implemented.
	return asFunction && asFunction->ScriptFunction != nullptr && asFunction->GetOuterUClass();
}

FString UASClass::GetSourceFilePath() const
{
	if (ScriptTypePtr == nullptr)
		return TEXT("");
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(((asITypeInfo*)ScriptTypePtr)->GetModule());
	if (!Module.IsValid())
		return TEXT("");
	if (Module->Code.Num() == 0)
		return TEXT("");
	return Module->Code[0].AbsoluteFilename;
}

FString UASClass::GetRelativeSourceFilePath() const
{
	if (ScriptTypePtr == nullptr)
		return TEXT("");
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(((asITypeInfo*)ScriptTypePtr)->GetModule());
	if (!Module.IsValid())
		return TEXT("");
	if (Module->Code.Num() == 0)
		return TEXT("");
	return Module->Code[0].RelativeFilename;
}

bool UASClass::IsDeveloperOnly() const
{
	if (ScriptTypePtr == nullptr)
		return false;
	auto& Manager = FAngelscriptEngine::Get();
	auto Module = Manager.GetModule(((asITypeInfo*)ScriptTypePtr)->GetModule());
	if (!Module.IsValid())
		return false;
	const FString& ModuleName = Module->ModuleName;
	return ModuleName.Equals(TEXT("Dev"))
		|| ModuleName.StartsWith(TEXT("Dev."))
		|| ModuleName.Equals(TEXT("Editor"))
		|| ModuleName.StartsWith(TEXT("Editor."))
		|| ModuleName.EndsWith(TEXT(".Editor"))
		|| ModuleName.Contains(TEXT(".Editor."));
}
