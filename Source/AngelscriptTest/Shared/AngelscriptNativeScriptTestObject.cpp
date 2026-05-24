#include "AngelscriptNativeScriptTestObject.h"

float UAngelscriptNativeScriptTestObject::NativeEventWithParameter_Implementation(float Value)
{
	return Value + 1.0f;
}

int32 UAngelscriptNativeScriptTestObject::NativeIntEventWithParameter(int32 Value)
{
	return Value + 2;
}

int32 UAngelscriptNativeScriptTestObject::NativeRefIntEvent(int32& Value) const
{
	Value = 42;
	return 7;
}

int32 UAngelscriptNativeScriptTestObject::NativeNoArgValue() const
{
	return 42;
}

int32 UAngelscriptNativeScriptTestObject::NativeIntStringEvent(int32 Value, const FString& Label) const
{
	return Value + Label.Len();
}

void UAngelscriptNativeScriptTestObject::SetIntStringFromDelegate(int32 Value, const FString& Label)
{
	NameCounts.FindOrAdd(FName(*Label)) = Value;
}

int32 UAngelscriptNativeScriptTestObject::NativeIntStringNameEvent(int32 Value, const FString& Label, FName Name) const
{
	return Value + Label.Len() + Name.ToString().Len();
}

void UAngelscriptNativeScriptTestObject::SetIntStringNameFromDelegate(int32 Value, const FString& Label, FName Name)
{
	NameCounts.FindOrAdd(Name) = Value + Label.Len();
}

FString UAngelscriptNativeScriptTestObject::NativeStringEventWithParameter(const FString& Value) const
{
	return Value + TEXT("_native");
}

FString UAngelscriptNativeScriptTestObject::NativeRefStringEvent(FString& Value) const
{
	Value = TEXT("updated");
	return TEXT("native_return");
}

FName UAngelscriptNativeScriptTestObject::NativeNameEventWithParameter(FName Value) const
{
	return Value;
}

FName UAngelscriptNativeScriptTestObject::NativeRefNameEvent(FName& Value) const
{
	Value = TEXT("Renamed");
	return Value;
}

void UAngelscriptNativeScriptTestObject::SetPlainClassFromDelegate(UClass* Value)
{
	PlainClass = Value;
}

void UAngelscriptNativeScriptTestObject::SetExtensionPointFromDelegate(FName Value)
{
	ExtensionPoint = Value;
}

void UAngelscriptNativeScriptTestObject::MarkNativeFlagFromDelegate()
{
	bNativeFlag = true;
}

void UAngelscriptNativeScriptTestObject::SetPreciseValueFromDelegate(float Value)
{
	PreciseValue = Value;
}

void UAngelscriptNativeScriptTestObject::SetLargeCountFromDelegate(int32 Value)
{
	LargeCount = Value;
}

float UAngelscriptNativeScriptTestObject::GetPreciseValueFromDelegate() const
{
	return static_cast<float>(PreciseValue);
}
