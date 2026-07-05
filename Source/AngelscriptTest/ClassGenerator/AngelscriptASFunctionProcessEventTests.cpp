#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace ASFunctionProcessEventTests
{
	static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bActual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreEqual(Expected, Actual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const FString& Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		return CheckEqual(Test, *Message, Actual, Expected);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Value, Message);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const FString& Message, const ValueType& Value)
	{
		return CheckNotNull(Test, *Message, Value);
	}

	static const FName ModuleName(TEXT("ASFunctionProcessEvent"));
	static const FString ScriptFilename(TEXT("ASFunctionProcessEvent.as"));
	static const FName GeneratedClassName(TEXT("UProcessEventCarrier"));
	static const FName VirtualParentClassName(TEXT("UProcessEventVirtualParent"));
	static const FName VirtualChildClassName(TEXT("UProcessEventVirtualChild"));
	static const FName StoredValuePropertyName(TEXT("StoredValue"));
	static const FName ByteSeenPropertyName(TEXT("ByteSeen"));
	static const FName BoolSeenPropertyName(TEXT("bBoolSeen"));
	static const FName DWordSeenPropertyName(TEXT("DWordSeen"));
	static const FName QWordSeenPropertyName(TEXT("QWordSeen"));
	static const FName FloatHundredthsPropertyName(TEXT("FloatHundredths"));
	static const FName DoubleHundredthsPropertyName(TEXT("DoubleHundredths"));
	static const FName ReferenceSeenPropertyName(TEXT("ReferenceSeen"));

	enum class EInvocationPath
	{
		ProcessEvent,
		RuntimeCallEvent
	};

	const TCHAR* DescribeInvocationPath(EInvocationPath Path)
	{
		return Path == EInvocationPath::ProcessEvent ? TEXT("ProcessEvent") : TEXT("RuntimeCallEvent");
	}

	UASClass* CompileProcessEventCarrier(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UProcessEventCarrier : UObject
			{
				UPROPERTY()
				int StoredValue = 0;

				UPROPERTY()
				int ByteSeen = 0;

				UPROPERTY()
				bool bBoolSeen = false;

				UPROPERTY()
				uint32 DWordSeen = 0;

				UPROPERTY()
				uint64 QWordSeen = 0;

				UPROPERTY()
				int FloatHundredths = 0;

				UPROPERTY()
				int DoubleHundredths = 0;

				UPROPERTY()
				int ReferenceSeen = 0;

				UFUNCTION()
				int AddTen(int Input)
				{
					return Input + 10;
				}

				UFUNCTION()
				void SetStoredValue(int Input)
				{
					StoredValue = Input;
				}

				UFUNCTION()
				void TakeByte(uint8 Value)
				{
					ByteSeen = Value;
				}

				UFUNCTION()
				void TakeBool(bool bValue)
				{
					bBoolSeen = bValue;
				}

				UFUNCTION()
				void TakeDWord(uint32 Value)
				{
					DWordSeen = Value;
				}

				UFUNCTION()
				void TakeQWord(uint64 Value)
				{
					QWordSeen = Value;
				}

				UFUNCTION()
				void TakeFloat(float32 Value)
				{
					FloatHundredths = int(Value * 100.0f);
				}

				UFUNCTION()
				void TakeDouble(float64 Value)
				{
					DoubleHundredths = int(Value * 100.0);
				}

				UFUNCTION()
				void BumpReference(int& Value)
				{
					Value += 11;
					ReferenceSeen = Value;
				}

				UFUNCTION()
				uint8 ReturnByte()
				{
					return 213;
				}

				UFUNCTION()
				int ReturnInt()
				{
					return 31415;
				}

				UFUNCTION()
				float32 ReturnFloat()
				{
					return 12.25f;
				}

				UFUNCTION()
				float64 ReturnDouble()
				{
					return 24.5;
				}

				UFUNCTION()
				UObject ReturnSelfObject()
				{
					return this;
				}
			}

			UCLASS()
			class UProcessEventVirtualParent : UObject
			{
				UPROPERTY()
				int ParentValue = 21;

				UFUNCTION(BlueprintEvent)
				int GetVirtualValue()
				{
					return ParentValue;
				}
			}

			UCLASS()
			class UProcessEventVirtualChild : UProcessEventVirtualParent
			{
				UFUNCTION(BlueprintOverride)
				int GetVirtualValue()
				{
					return 217;
				}
			}
			)AS");

		UClass* GeneratedClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
		if (GeneratedClass == nullptr)
		{
			return nullptr;
		}

		return Cast<UASClass>(GeneratedClass);
	}

	bool InvokeIntFunctionThroughProcessEvent(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		int32 InputValue,
		int32& OutReturnValue)
	{
		FIntProperty* InputProperty = FindFProperty<FIntProperty>(Function, TEXT("Input"));
		FIntProperty* ReturnProperty = FindFProperty<FIntProperty>(Function, TEXT("ReturnValue"));
		if (!CheckNotNull(Test, TEXT("ProcessEvent thunk test case should expose the Input property"), InputProperty)
			|| !CheckNotNull(Test, TEXT("ProcessEvent thunk test case should expose the ReturnValue property"), ReturnProperty))
		{
			return false;
		}

		FStructOnScope Params(Function);
		void* ParamsMemory = Params.GetStructMemory();
		if (!CheckNotNull(Test, TEXT("ProcessEvent thunk test case should allocate a reflected parameter buffer"), ParamsMemory))
		{
			return false;
		}

		InputProperty->SetPropertyValue_InContainer(ParamsMemory, InputValue);

		FAngelscriptEngineScope FunctionScope(Engine, Object);
		Object->ProcessEvent(Function, ParamsMemory);

		OutReturnValue = ReturnProperty->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	bool InvokeVoidFunctionThroughProcessEvent(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		int32 InputValue)
	{
		FIntProperty* InputProperty = FindFProperty<FIntProperty>(Function, TEXT("Input"));
		if (!CheckNotNull(Test, TEXT("ProcessEvent thunk test case should expose the Input property on the void function"), InputProperty))
		{
			return false;
		}

		FStructOnScope Params(Function);
		void* ParamsMemory = Params.GetStructMemory();
		if (!CheckNotNull(Test, TEXT("ProcessEvent thunk test case should allocate a reflected parameter buffer for the void function"), ParamsMemory))
		{
			return false;
		}

		InputProperty->SetPropertyValue_InContainer(ParamsMemory, InputValue);

		FAngelscriptEngineScope FunctionScope(Engine, Object);
		Object->ProcessEvent(Function, ParamsMemory);
		return true;
	}

	UASFunction* RequireScriptFunction(
		FAutomationTestBase& Test,
		UClass* OwnerClass,
		const TCHAR* FunctionName)
	{
		UASFunction* ScriptFunction = Cast<UASFunction>(::FindGeneratedFunction(OwnerClass, FunctionName));
		CheckNotNull(Test,
			*FString::Printf(TEXT("ASFunction ABI test case should expose '%s' as a UASFunction"), FunctionName),
			ScriptFunction);
		return ScriptFunction;
	}

	bool InvokeWithParams(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		void* ParamsMemory,
		EInvocationPath Path)
	{
		if (!CheckNotNull(Test, TEXT("ASFunction ABI test case should have a valid target object"), Object)
			|| !CheckNotNull(Test, TEXT("ASFunction ABI test case should have a valid function"), Function)
			|| !CheckNotNull(Test, TEXT("ASFunction ABI test case should have reflected parameter memory"), ParamsMemory))
		{
			return false;
		}

		FAngelscriptEngineScope FunctionScope(Engine, Object);
		if (Path == EInvocationPath::ProcessEvent)
		{
			Object->ProcessEvent(Function, ParamsMemory);
			return true;
		}

		UASFunction* ScriptFunction = Cast<UASFunction>(Function);
		if (!CheckNotNull(Test, TEXT("ASFunction ABI test case should dispatch generated functions directly as UASFunction"), ScriptFunction))
		{
			return false;
		}

		ScriptFunction->RuntimeCallEvent(Object, ParamsMemory);
		return true;
	}

	template <typename PropertyType, typename ValueType>
	bool SetParameterValue(
		FAutomationTestBase& Test,
		UFunction* Function,
		void* ParamsMemory,
		FName PropertyName,
		ValueType Value)
	{
		PropertyType* Property = FindFProperty<PropertyType>(Function, PropertyName);
		if (!CheckNotNull(Test,
				*FString::Printf(TEXT("ASFunction ABI test case should expose parameter '%s'"), *PropertyName.ToString()),
				Property))
		{
			return false;
		}

		Property->SetPropertyValue_InContainer(ParamsMemory, Value);
		return true;
	}

	template <typename PropertyType, typename ValueType>
	bool InvokeSingleParameterFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		FName PropertyName,
		ValueType Value,
		EInvocationPath Path)
	{
		FStructOnScope Params(Function);
		void* ParamsMemory = Params.GetStructMemory();
		return SetParameterValue<PropertyType>(Test, Function, ParamsMemory, PropertyName, Value)
			&& InvokeWithParams(Test, Engine, Object, Function, ParamsMemory, Path);
	}

	template <typename PropertyType, typename ValueType>
	bool InvokeReturnFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		EInvocationPath Path,
		ValueType& OutValue)
	{
		FStructOnScope Params(Function);
		void* ParamsMemory = Params.GetStructMemory();
		if (!InvokeWithParams(Test, Engine, Object, Function, ParamsMemory, Path))
		{
			return false;
		}

		PropertyType* ReturnProperty = CastField<PropertyType>(Function->GetReturnProperty());
		if (!CheckNotNull(Test, TEXT("ASFunction ABI test case should expose the expected return property type"), ReturnProperty))
		{
			return false;
		}

		OutValue = ReturnProperty->GetPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	bool InvokeObjectReturnFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UFunction* Function,
		EInvocationPath Path,
		UObject*& OutValue)
	{
		FStructOnScope Params(Function);
		void* ParamsMemory = Params.GetStructMemory();
		if (!InvokeWithParams(Test, Engine, Object, Function, ParamsMemory, Path))
		{
			return false;
		}

		FObjectProperty* ReturnProperty = CastField<FObjectProperty>(Function->GetReturnProperty());
		if (!CheckNotNull(Test, TEXT("ASFunction ABI test case should expose an object return property"), ReturnProperty))
		{
			return false;
		}

		OutValue = ReturnProperty->GetObjectPropertyValue_InContainer(ParamsMemory);
		return true;
	}

	bool ExerciseWrapperAbiFixture(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UClass* ScriptClass,
		UObject* Instance,
		EInvocationPath Path)
	{
		UASFunction* TakeByteFunction = RequireScriptFunction(Test, ScriptClass, TEXT("TakeByte"));
		UASFunction* TakeBoolFunction = RequireScriptFunction(Test, ScriptClass, TEXT("TakeBool"));
		UASFunction* TakeDWordFunction = RequireScriptFunction(Test, ScriptClass, TEXT("TakeDWord"));
		UASFunction* TakeQWordFunction = RequireScriptFunction(Test, ScriptClass, TEXT("TakeQWord"));
		UASFunction* TakeFloatFunction = RequireScriptFunction(Test, ScriptClass, TEXT("TakeFloat"));
		UASFunction* TakeDoubleFunction = RequireScriptFunction(Test, ScriptClass, TEXT("TakeDouble"));
		UASFunction* BumpReferenceFunction = RequireScriptFunction(Test, ScriptClass, TEXT("BumpReference"));
		UASFunction* ReturnByteFunction = RequireScriptFunction(Test, ScriptClass, TEXT("ReturnByte"));
		UASFunction* ReturnIntFunction = RequireScriptFunction(Test, ScriptClass, TEXT("ReturnInt"));
		UASFunction* ReturnFloatFunction = RequireScriptFunction(Test, ScriptClass, TEXT("ReturnFloat"));
		UASFunction* ReturnDoubleFunction = RequireScriptFunction(Test, ScriptClass, TEXT("ReturnDouble"));
		UASFunction* ReturnSelfObjectFunction = RequireScriptFunction(Test, ScriptClass, TEXT("ReturnSelfObject"));
		if (TakeByteFunction == nullptr
			|| TakeBoolFunction == nullptr
			|| TakeDWordFunction == nullptr
			|| TakeQWordFunction == nullptr
			|| TakeFloatFunction == nullptr
			|| TakeDoubleFunction == nullptr
			|| BumpReferenceFunction == nullptr
			|| ReturnByteFunction == nullptr
			|| ReturnIntFunction == nullptr
			|| ReturnFloatFunction == nullptr
			|| ReturnDoubleFunction == nullptr
			|| ReturnSelfObjectFunction == nullptr)
		{
			return false;
		}

		if (!InvokeSingleParameterFunction<FByteProperty>(Test, Engine, Instance, TakeByteFunction, TEXT("Value"), static_cast<uint8>(201), Path))
		{
			return false;
		}
		int32 ByteSeen = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Instance, ByteSeenPropertyName, ByteSeen)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve byte argument values"), DescribeInvocationPath(Path)), ByteSeen, 201))
		{
			return false;
		}

		if (!InvokeSingleParameterFunction<FBoolProperty>(Test, Engine, Instance, TakeBoolFunction, TEXT("bValue"), true, Path))
		{
			return false;
		}
		bool bBoolSeen = false;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FBoolProperty>(Test, Instance, BoolSeenPropertyName, bBoolSeen)
			|| !CheckTrue(Test, *FString::Printf(TEXT("%s should preserve bool argument values"), DescribeInvocationPath(Path)), bBoolSeen))
		{
			return false;
		}

		const uint32 DWordValue = 4000000000u;
		if (!InvokeSingleParameterFunction<FUInt32Property>(Test, Engine, Instance, TakeDWordFunction, TEXT("Value"), DWordValue, Path))
		{
			return false;
		}
		uint32 DWordSeen = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FUInt32Property>(Test, Instance, DWordSeenPropertyName, DWordSeen)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve dword argument values"), DescribeInvocationPath(Path)), DWordSeen, DWordValue))
		{
			return false;
		}

		const uint64 QWordValue = 0x12345678ABCDEF01ull;
		if (!InvokeSingleParameterFunction<FUInt64Property>(Test, Engine, Instance, TakeQWordFunction, TEXT("Value"), QWordValue, Path))
		{
			return false;
		}
		uint64 QWordSeen = 0;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FUInt64Property>(Test, Instance, QWordSeenPropertyName, QWordSeen)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve qword argument values"), DescribeInvocationPath(Path)), QWordSeen, QWordValue))
		{
			return false;
		}

		if (!InvokeSingleParameterFunction<FFloatProperty>(Test, Engine, Instance, TakeFloatFunction, TEXT("Value"), 12.25f, Path))
		{
			return false;
		}
		int32 FloatHundredths = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Instance, FloatHundredthsPropertyName, FloatHundredths)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve float argument values"), DescribeInvocationPath(Path)), FloatHundredths, 1225))
		{
			return false;
		}

		if (!InvokeSingleParameterFunction<FDoubleProperty>(Test, Engine, Instance, TakeDoubleFunction, TEXT("Value"), 24.5, Path))
		{
			return false;
		}
		int32 DoubleHundredths = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Instance, DoubleHundredthsPropertyName, DoubleHundredths)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve double argument values"), DescribeInvocationPath(Path)), DoubleHundredths, 2450))
		{
			return false;
		}

		FStructOnScope ReferenceParams(BumpReferenceFunction);
		void* ReferenceParamsMemory = ReferenceParams.GetStructMemory();
		if (!SetParameterValue<FIntProperty>(Test, BumpReferenceFunction, ReferenceParamsMemory, TEXT("Value"), 31)
			|| !InvokeWithParams(Test, Engine, Instance, BumpReferenceFunction, ReferenceParamsMemory, Path))
		{
			return false;
		}
		FIntProperty* ReferenceParamProperty = FindFProperty<FIntProperty>(BumpReferenceFunction, TEXT("Value"));
		int32 ReferenceSeen = INDEX_NONE;
		if (!CheckNotNull(Test, TEXT("ASFunction ABI reference test should expose the Value parameter"), ReferenceParamProperty)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should write back reflected reference parameter values"), DescribeInvocationPath(Path)), ReferenceParamProperty->GetPropertyValue_InContainer(ReferenceParamsMemory), 42)
			|| !AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Instance, ReferenceSeenPropertyName, ReferenceSeen)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should expose reference writeback inside script state"), DescribeInvocationPath(Path)), ReferenceSeen, 42))
		{
			return false;
		}

		uint8 ByteReturnValue = 0;
		int32 IntReturnValue = 0;
		float FloatReturnValue = 0.0f;
		double DoubleReturnValue = 0.0;
		UObject* ObjectReturnValue = nullptr;
		if (!InvokeReturnFunction<FByteProperty>(Test, Engine, Instance, ReturnByteFunction, Path, ByteReturnValue)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve byte return values"), DescribeInvocationPath(Path)), static_cast<int32>(ByteReturnValue), 213)
			|| !InvokeReturnFunction<FIntProperty>(Test, Engine, Instance, ReturnIntFunction, Path, IntReturnValue)
			|| !CheckEqual(Test, *FString::Printf(TEXT("%s should preserve dword primitive return values"), DescribeInvocationPath(Path)), IntReturnValue, 31415)
			|| !InvokeReturnFunction<FFloatProperty>(Test, Engine, Instance, ReturnFloatFunction, Path, FloatReturnValue)
			|| !FNoDiscardAsserter(Test).IsNear(12.25f, FloatReturnValue, UE_KINDA_SMALL_NUMBER, *FString::Printf(TEXT("%s should preserve float return values"), DescribeInvocationPath(Path)))
			|| !InvokeReturnFunction<FDoubleProperty>(Test, Engine, Instance, ReturnDoubleFunction, Path, DoubleReturnValue)
			|| !FNoDiscardAsserter(Test).IsNear(24.5, DoubleReturnValue, static_cast<double>(UE_KINDA_SMALL_NUMBER), *FString::Printf(TEXT("%s should preserve double return values"), DescribeInvocationPath(Path)))
			|| !InvokeObjectReturnFunction(Test, Engine, Instance, ReturnSelfObjectFunction, Path, ObjectReturnValue)
			|| !CheckTrue(Test, *FString::Printf(TEXT("%s should preserve object return identity"), DescribeInvocationPath(Path)), ObjectReturnValue == Instance))
		{
			return false;
		}

		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASFunctionProcessEventTests,
	"Angelscript.TestModule.ClassGenerator.ASFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(ProcessEventDispatchesThroughNativeThunk)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASFunctionProcessEventTests::ModuleName.ToString());
		};

		UASClass* ScriptClass = ASFunctionProcessEventTests::CompileProcessEventCarrier(*TestRunner, Engine);
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent thunk test case should compile to a UASClass"), ScriptClass))
		{
			return;
		}

		UFunction* AddTenFunction = ::FindGeneratedFunction(ScriptClass, TEXT("AddTen"));
		UFunction* SetStoredValueFunction = ::FindGeneratedFunction(ScriptClass, TEXT("SetStoredValue"));
		UASFunction* AddTenScriptFunction = Cast<UASFunction>(AddTenFunction);
		UASFunction* SetStoredValueScriptFunction = Cast<UASFunction>(SetStoredValueFunction);
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent thunk test case should generate AddTen"), AddTenFunction)
			|| !ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent thunk test case should generate SetStoredValue"), SetStoredValueFunction)
			|| !ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent thunk test case should expose AddTen as a UASFunction"), AddTenScriptFunction)
			|| !ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent thunk test case should expose SetStoredValue as a UASFunction"), SetStoredValueScriptFunction))
		{
			return;
		}

		ASFunctionProcessEventTests::CheckTrue(*TestRunner, TEXT("ProcessEvent thunk test case should route AddTen through UASFunctionNativeThunk"), AddTenFunction->GetNativeFunc() == &UASFunctionNativeThunk);
		ASFunctionProcessEventTests::CheckTrue(*TestRunner, TEXT("ProcessEvent thunk test case should route SetStoredValue through UASFunctionNativeThunk"), SetStoredValueFunction->GetNativeFunc() == &UASFunctionNativeThunk);

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("ProcessEventCarrierInstance"));
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent thunk test case should instantiate the generated UObject"), Instance))
		{
			return;
		}

		int32 AddTenResult = INDEX_NONE;
		if (!ASFunctionProcessEventTests::CheckTrue(*TestRunner,
				TEXT("ProcessEvent thunk test case should execute AddTen via ProcessEvent"),
				ASFunctionProcessEventTests::InvokeIntFunctionThroughProcessEvent(*TestRunner, Engine, Instance, AddTenFunction, 5, AddTenResult))
			|| !ASFunctionProcessEventTests::CheckEqual(*TestRunner, TEXT("ProcessEvent thunk test case should return 15 when AddTen receives 5"), AddTenResult, 15))
		{
			return;
		}

		if (!ASFunctionProcessEventTests::CheckTrue(*TestRunner,
				TEXT("ProcessEvent thunk test case should execute SetStoredValue via ProcessEvent"),
				ASFunctionProcessEventTests::InvokeVoidFunctionThroughProcessEvent(*TestRunner, Engine, Instance, SetStoredValueFunction, 17)))
		{
			return;
		}

		int32 StoredValue = INDEX_NONE;
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(*TestRunner, Instance, ASFunctionProcessEventTests::StoredValuePropertyName, StoredValue))
		{
			return;
		}

		(void)ASFunctionProcessEventTests::CheckEqual(*TestRunner, TEXT("ProcessEvent thunk test case should write StoredValue through RuntimeCallFunction"), StoredValue, 17);
	}

	TEST_METHOD(ProcessEventPreservesWrapperAbiShapes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASFunctionProcessEventTests::ModuleName.ToString());
		};

		UASClass* ScriptClass = ASFunctionProcessEventTests::CompileProcessEventCarrier(*TestRunner, Engine);
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent ABI test case should compile to a UASClass"), ScriptClass))
		{
			return;
		}

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("ProcessEventAbiCarrierInstance"));
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ProcessEvent ABI test case should instantiate the generated UObject"), Instance))
		{
			return;
		}

		(void)ASFunctionProcessEventTests::CheckTrue(*TestRunner,
			TEXT("ProcessEvent should preserve reflected wrapper ABI shapes"),
			ASFunctionProcessEventTests::ExerciseWrapperAbiFixture(
				*TestRunner,
				Engine,
				ScriptClass,
				Instance,
				ASFunctionProcessEventTests::EInvocationPath::ProcessEvent));
	}

	TEST_METHOD(RuntimeCallEventPreservesWrapperAbiShapes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASFunctionProcessEventTests::ModuleName.ToString());
		};

		UASClass* ScriptClass = ASFunctionProcessEventTests::CompileProcessEventCarrier(*TestRunner, Engine);
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("RuntimeCallEvent ABI test case should compile to a UASClass"), ScriptClass))
		{
			return;
		}

		UObject* Instance = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("RuntimeCallEventAbiCarrierInstance"));
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("RuntimeCallEvent ABI test case should instantiate the generated UObject"), Instance))
		{
			return;
		}

		(void)ASFunctionProcessEventTests::CheckTrue(*TestRunner,
			TEXT("RuntimeCallEvent should preserve reflected wrapper ABI shapes"),
			ASFunctionProcessEventTests::ExerciseWrapperAbiFixture(
				*TestRunner,
				Engine,
				ScriptClass,
				Instance,
				ASFunctionProcessEventTests::EInvocationPath::RuntimeCallEvent));
	}

	TEST_METHOD(ParentRuntimeCallEventOnChildResolvesScriptOverride)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASFunctionProcessEventTests::ModuleName.ToString());
		};

		if (ASFunctionProcessEventTests::CompileProcessEventCarrier(*TestRunner, Engine) == nullptr)
		{
			return;
		}

		UClass* ParentClass = ::FindGeneratedClass(&Engine, ASFunctionProcessEventTests::VirtualParentClassName);
		UClass* ChildClass = ::FindGeneratedClass(&Engine, ASFunctionProcessEventTests::VirtualChildClassName);
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ASFunction virtual dispatch test case should compile the parent class"), ParentClass)
			|| !ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ASFunction virtual dispatch test case should generate the child class"), ChildClass))
		{
			return;
		}

		UASFunction* ParentFunction = ASFunctionProcessEventTests::RequireScriptFunction(*TestRunner, ParentClass, TEXT("GetVirtualValue"));
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ASFunction virtual dispatch test case should expose the parent virtual function"), ParentFunction))
		{
			return;
		}

		UObject* ChildInstance = NewObject<UObject>(GetTransientPackage(), ChildClass, TEXT("ProcessEventVirtualChildInstance"));
		if (!ASFunctionProcessEventTests::CheckNotNull(*TestRunner, TEXT("ASFunction virtual dispatch test case should instantiate the generated child UObject"), ChildInstance))
		{
			return;
		}

		int32 ReturnValue = 0;
		if (!ASFunctionProcessEventTests::InvokeReturnFunction<FIntProperty>(
				*TestRunner,
				Engine,
				ChildInstance,
				ParentFunction,
				ASFunctionProcessEventTests::EInvocationPath::RuntimeCallEvent,
				ReturnValue))
		{
			return;
		}

		(void)ASFunctionProcessEventTests::CheckEqual(*TestRunner, TEXT("Parent UASFunction invoked on a child object should execute the child script override"), ReturnValue, 217);
	}
};

#endif
