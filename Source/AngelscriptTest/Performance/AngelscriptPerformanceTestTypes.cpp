#include "Performance/AngelscriptPerformanceTestTypes.h"

#include "AngelscriptBinds.h"

#if WITH_DEV_AUTOMATION_TESTS

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AngelscriptPerformanceTestTargetObject(
	TEXT("AngelscriptPerformanceTestTargetObject"),
	(int32)FAngelscriptBinds::EOrder::Late + 101,
	[]
	{
		auto MarkPreviousBindEditorOnly = []
		{
			FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
		};

		FAngelscriptBinds TargetObject_ = FAngelscriptBinds::ExistingClass("UAngelscriptPerformanceTestTargetObject");
		if (TargetObject_.GetTypeInfo() == nullptr)
		{
			return;
		}

		{
			FAngelscriptBinds::FNamespace Namespace("UAngelscriptPerformanceTestTargetObject");

			FAngelscriptBinds::BindGlobalFunction(
				"void StaticNoOp()",
				FUNC_TRIVIAL(UAngelscriptPerformanceTestTargetObject::StaticNoOp));
			MarkPreviousBindEditorOnly();

			FAngelscriptBinds::BindGlobalFunction(
				"int32 StaticAdd(int32 A, int32 B)",
				FUNCPR_TRIVIAL(int32, UAngelscriptPerformanceTestTargetObject::StaticAdd, (int32, int32)));
			MarkPreviousBindEditorOnly();

			FAngelscriptBinds::BindGlobalFunction(
				"int32 StaticSubtract(int32 A, int32 B)",
				FUNCPR_TRIVIAL(int32, UAngelscriptPerformanceTestTargetObject::StaticSubtract, (int32, int32)));
			MarkPreviousBindEditorOnly();

			FAngelscriptBinds::BindGlobalFunction(
				"int32 StaticMultiply(int32 A, int32 B)",
				FUNCPR_TRIVIAL(int32, UAngelscriptPerformanceTestTargetObject::StaticMultiply, (int32, int32)));
			MarkPreviousBindEditorOnly();

			FAngelscriptBinds::BindGlobalFunction(
				"int32 StaticDivide(int32 A, int32 B)",
				FUNCPR_TRIVIAL(int32, UAngelscriptPerformanceTestTargetObject::StaticDivide, (int32, int32)));
			MarkPreviousBindEditorOnly();
		}

		TargetObject_.Method(
			"void MemberNoOp() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, MemberNoOp));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetInt32ValueFunction(int32 InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetInt32ValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"int32 GetInt32ValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetInt32ValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetDoubleValueFunction(double InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetDoubleValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"double GetDoubleValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetDoubleValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetStringValueFunction(const FString& InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetStringValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"FString GetStringValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetStringValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetArrayValueFunction(const TArray<int32>& InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetArrayValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"TArray<int32> GetArrayValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetArrayValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetSetValueFunction(const TSet<int32>& InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetSetValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"TSet<int32> GetSetValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetSetValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetMapValueFunction(const TMap<FName, int32>& InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetMapValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"TMap<FName, int32> GetMapValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetMapValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetBoolValueFunction(bool InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetBoolValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"bool GetBoolValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetBoolValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetNameValueFunction(FName InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetNameValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"FName GetNameValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetNameValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetTextValueFunction(const FText& InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetTextValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"FText GetTextValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetTextValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetEnumValueFunction(EAngelscriptPerformanceTestEnum InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetEnumValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"EAngelscriptPerformanceTestEnum GetEnumValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetEnumValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetStructValueFunction(const FAngelscriptPerformanceTestStruct& InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetStructValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"FAngelscriptPerformanceTestStruct GetStructValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetStructValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetObjectValueFunction(UObject InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetObjectValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"UObject GetObjectValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetObjectValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"void SetClassValueFunction(UClass InValue)",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, SetClassValueFunction));
		MarkPreviousBindEditorOnly();

		TargetObject_.Method(
			"UClass GetClassValueFunction() const",
			METHOD_TRIVIAL(UAngelscriptPerformanceTestTargetObject, GetClassValueFunction));
		MarkPreviousBindEditorOnly();
	});
#endif

void UAngelscriptPerformanceTestTargetObject::StaticNoOp()
{
}

int32 UAngelscriptPerformanceTestTargetObject::StaticAdd(int32 A, int32 B)
{
	return A + B;
}

int32 UAngelscriptPerformanceTestTargetObject::StaticSubtract(int32 A, int32 B)
{
	return A - B;
}

int32 UAngelscriptPerformanceTestTargetObject::StaticMultiply(int32 A, int32 B)
{
	return A * B;
}

int32 UAngelscriptPerformanceTestTargetObject::StaticDivide(int32 A, int32 B)
{
	return B != 0 ? A / B : 0;
}

void UAngelscriptPerformanceTestTargetObject::MemberNoOp() const
{
}

void UAngelscriptPerformanceTestTargetObject::SetBoolValueFunction(bool InValue)
{
	BoolValue = InValue;
}

bool UAngelscriptPerformanceTestTargetObject::GetBoolValueFunction() const
{
	return BoolValue;
}

void UAngelscriptPerformanceTestTargetObject::SetInt32ValueFunction(int32 InValue)
{
	Int32Value = InValue;
}

int32 UAngelscriptPerformanceTestTargetObject::GetInt32ValueFunction() const
{
	return Int32Value;
}

void UAngelscriptPerformanceTestTargetObject::SetDoubleValueFunction(double InValue)
{
	DoubleValue = InValue;
}

double UAngelscriptPerformanceTestTargetObject::GetDoubleValueFunction() const
{
	return DoubleValue;
}

void UAngelscriptPerformanceTestTargetObject::SetNameValueFunction(FName InValue)
{
	NameValue = InValue;
}

FName UAngelscriptPerformanceTestTargetObject::GetNameValueFunction() const
{
	return NameValue;
}

void UAngelscriptPerformanceTestTargetObject::SetTextValueFunction(const FText& InValue)
{
	TextValue = InValue;
}

FText UAngelscriptPerformanceTestTargetObject::GetTextValueFunction() const
{
	return TextValue;
}

void UAngelscriptPerformanceTestTargetObject::SetStringValueFunction(const FString& InValue)
{
	StringValue = InValue;
}

FString UAngelscriptPerformanceTestTargetObject::GetStringValueFunction() const
{
	return StringValue;
}

void UAngelscriptPerformanceTestTargetObject::SetEnumValueFunction(EAngelscriptPerformanceTestEnum InValue)
{
	EnumValue = InValue;
}

EAngelscriptPerformanceTestEnum UAngelscriptPerformanceTestTargetObject::GetEnumValueFunction() const
{
	return EnumValue;
}

void UAngelscriptPerformanceTestTargetObject::SetStructValueFunction(const FAngelscriptPerformanceTestStruct& InValue)
{
	StructValue = InValue;
}

FAngelscriptPerformanceTestStruct UAngelscriptPerformanceTestTargetObject::GetStructValueFunction() const
{
	return StructValue;
}

void UAngelscriptPerformanceTestTargetObject::SetObjectValueFunction(UObject* InValue)
{
	ObjectValue = InValue;
}

UObject* UAngelscriptPerformanceTestTargetObject::GetObjectValueFunction() const
{
	return ObjectValue;
}

void UAngelscriptPerformanceTestTargetObject::SetClassValueFunction(UClass* InValue)
{
	ClassValue = InValue;
}

UClass* UAngelscriptPerformanceTestTargetObject::GetClassValueFunction() const
{
	return ClassValue;
}

void UAngelscriptPerformanceTestTargetObject::SetArrayValueFunction(const TArray<int32>& InValue)
{
	ArrayValue = InValue;
}

TArray<int32> UAngelscriptPerformanceTestTargetObject::GetArrayValueFunction() const
{
	return ArrayValue;
}

void UAngelscriptPerformanceTestTargetObject::SetSetValueFunction(const TSet<int32>& InValue)
{
	SetValue = InValue;
}

TSet<int32> UAngelscriptPerformanceTestTargetObject::GetSetValueFunction() const
{
	return SetValue;
}

void UAngelscriptPerformanceTestTargetObject::SetMapValueFunction(const TMap<FName, int32>& InValue)
{
	MapValue = InValue;
}

TMap<FName, int32> UAngelscriptPerformanceTestTargetObject::GetMapValueFunction() const
{
	return MapValue;
}

void UAngelscriptPerformanceTestTargetObject::ResetValues()
{
	BoolValue = false;
	Int32Value = 0;
	DoubleValue = 0.0;
	NameValue = NAME_None;
	TextValue = FText::GetEmpty();
	StringValue.Reset();
	EnumValue = EAngelscriptPerformanceTestEnum::Zero;
	StructValue.Value = 0;
	ObjectValue = nullptr;
	ClassValue = nullptr;
	ArrayValue.Reset();
	SetValue.Reset();
	MapValue.Reset();
}

int32 UAngelscriptPerformanceTestTargetObject::RunNativeScalarPropertyLoop(int32 Iterations)
{
	int32 Checksum = 0;
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		BoolValue = (Index & 1) == 0;
		Int32Value = Index;
		DoubleValue = static_cast<double>(Index) * 0.5;
		NameValue = TEXT("RuntimePerformance");
		StringValue = TEXT("RuntimePerformance");
		Checksum += BoolValue ? 1 : 0;
		Checksum += Int32Value;
		Checksum += static_cast<int32>(DoubleValue);
		Checksum += StringValue.Len();
	}
	return Checksum;
}

int32 UAngelscriptPerformanceTestTargetObject::RunNativeContainerPropertyLoop(int32 Iterations)
{
	int32 Checksum = 0;
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		ArrayValue.Empty();
		ArrayValue.Add(Index);
		SetValue.Empty();
		SetValue.Add(Index);
		MapValue.Empty();
		MapValue.Add(TEXT("Value"), Index);
		Checksum += ArrayValue[0];
		Checksum += SetValue.Contains(Index) ? 1 : 0;
		Checksum += MapValue.FindRef(TEXT("Value"));
	}
	return Checksum;
}

int32 UAngelscriptPerformanceTestTargetObject::RunNativeScalarFunctionLoop(int32 Iterations)
{
	int32 Checksum = 0;
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		StaticNoOp();
		Checksum += StaticAdd(Index, 3);
		MemberNoOp();
		SetInt32ValueFunction(Index);
		Checksum += GetInt32ValueFunction();
		SetDoubleValueFunction(static_cast<double>(Index) * 0.5);
		Checksum += static_cast<int32>(GetDoubleValueFunction());
		SetStringValueFunction(TEXT("RuntimePerformance"));
		Checksum += GetStringValueFunction().Len();
	}
	return Checksum;
}

int32 UAngelscriptPerformanceTestTargetObject::RunNativeContainerFunctionLoop(int32 Iterations)
{
	int32 Checksum = 0;
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		TArray<int32> LocalArray;
		LocalArray.Add(Index);
		SetArrayValueFunction(LocalArray);
		Checksum += GetArrayValueFunction()[0];

		TSet<int32> LocalSet;
		LocalSet.Add(Index);
		SetSetValueFunction(LocalSet);
		Checksum += GetSetValueFunction().Contains(Index) ? 1 : 0;

		TMap<FName, int32> LocalMap;
		LocalMap.Add(TEXT("Value"), Index);
		SetMapValueFunction(LocalMap);
		Checksum += GetMapValueFunction().FindRef(TEXT("Value"));
	}
	return Checksum;
}
