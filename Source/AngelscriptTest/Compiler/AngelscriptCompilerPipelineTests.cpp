#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerPipelineEndToEndTest,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DelegateEnumClassCompile)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerDelegateEnumClassCompile"),
			TEXT("CompilerDelegateEnumClassCompile.as"),
			TEXT(R"(
	UENUM(BlueprintType)
	enum class ECompilerTransferState : uint16
	{
		Alpha,
		Beta = 4,
		Gamma
	}

	delegate void FCompilerTransferDelegate(int Value);
	event void FCompilerTransferEvent(UClass TypeValue, FString Label);

	UCLASS(Abstract, BlueprintType)
	class UCompilerTransferObject : UObject
	{
		UPROPERTY()
		int Score;

		UFUNCTION()
		int GetScore()
		{
			return Score;
		}
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Fallback compiler validation should compile delegate/enum/class transfer input"), bCompiled))
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> SimpleDelegate = Engine.GetDelegate(TEXT("FCompilerTransferDelegate"));
		const TSharedPtr<FAngelscriptDelegateDesc> MultiDelegate = Engine.GetDelegate(TEXT("FCompilerTransferEvent"));
		if (!TestRunner->TestTrue(TEXT("Simple delegate metadata should be registered after compile"), SimpleDelegate.IsValid()))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Multicast delegate metadata should be registered after compile"), MultiDelegate.IsValid()))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("Simple delegate should remain single-cast"), SimpleDelegate->bIsMulticast);
		TestRunner->TestTrue(TEXT("Event delegate should remain multicast"), MultiDelegate->bIsMulticast);

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerTransferObject"));
		if (!TestRunner->TestNotNull(TEXT("Generated class should exist for delegate/enum/class transfer input"), GeneratedClass))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Generated class should preserve abstract flag"), GeneratedClass->HasAnyClassFlags(CLASS_Abstract));
		TestRunner->TestNotNull(TEXT("Generated Score property should exist"), FindFProperty<FProperty>(GeneratedClass, TEXT("Score")));
		TestRunner->TestNotNull(TEXT("Generated GetScore function should exist"), FindGeneratedFunction(GeneratedClass, TEXT("GetScore")));
	}
	}

	TEST_METHOD(FunctionDefaultsAndClassLikeCompile)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerFunctionDefaultsAndClassLikeCompile"),
			TEXT("CompilerFunctionDefaultsAndClassLikeCompile.as"),
			TEXT(R"(
	int SumWithDefault(int Value = 21, int Extra = 21)
	{
		return Value + Extra;
	}

	int Entry()
	{
		return SumWithDefault();
	}

	UCLASS()
	class UCompilerFunctionCarrier : UObject
	{

		UFUNCTION()
		UClass EchoPlainClass(UClass Value)
		{
			return Value;
		}

		UFUNCTION()
		TSubclassOf<AActor> EchoActorClass(TSubclassOf<AActor> Value)
		{
			return Value;
		}

		UFUNCTION()
		TSoftClassPtr<AActor> EchoSoftActorClass(TSoftClassPtr<AActor> Value)
		{
			return Value;
		}
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Fallback compiler validation should compile function defaults and class-like signatures"), bCompiled))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteIntFunction(&Engine, TEXT("CompilerFunctionDefaultsAndClassLikeCompile"), TEXT("int Entry()"), Result);
		if (!TestRunner->TestTrue(TEXT("Function default validation should execute compiled entry point"), bExecuted))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Function default values should be honored at runtime"), Result, 42);

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerFunctionCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Generated class should exist for class-like signature input"), GeneratedClass))
		{
			return;
		}

		TestRunner->TestNotNull(TEXT("EchoPlainClass function should exist"), FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass")));
		TestRunner->TestNotNull(TEXT("EchoActorClass function should exist"), FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass")));
		TestRunner->TestNotNull(TEXT("EchoSoftActorClass function should exist"), FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass")));
	}
	}

	TEST_METHOD(PropertyDefaultsCompile)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerPropertyDefaultsCompile"),
			TEXT("CompilerPropertyDefaultsCompile.as"),
			TEXT(R"(
	UCLASS()
	class UCompilerDefaultsCarrier : UObject
	{
		UPROPERTY()
		int Score = 7;

		UPROPERTY()
		TArray<FName> Tags;

		default Score = 21;
		default Tags.Add(n"Alpha");
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Fallback compiler validation should compile property default input"), bCompiled))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerDefaultsCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Generated class should exist for property default input"), GeneratedClass))
		{
			return;
		}

		UObject* DefaultObject = GeneratedClass->GetDefaultObject();
		if (!TestRunner->TestNotNull(TEXT("Generated class default object should exist"), DefaultObject))
		{
			return;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("Score"));
		if (!TestRunner->TestNotNull(TEXT("Generated Score property should exist"), ScoreProperty))
		{
			return;
		}

		const int32 ScoreValue = ScoreProperty->GetPropertyValue_InContainer(DefaultObject);
		TestRunner->TestEqual(TEXT("Generated default object should materialize overridden int default"), ScoreValue, 21);
	}
	}

	TEST_METHOD(GeneratedClassConsistency)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerGeneratedClassConsistency"),
			TEXT("CompilerGeneratedClassConsistency.as"),
			TEXT(R"(
	UCLASS(Abstract, BlueprintType)
	class UCompilerConsistencyCarrier : UObject
	{
		UPROPERTY()
		int Score;

		UFUNCTION()
		int GetScore()
		{
			return Score;
		}
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Generated class consistency input should compile"), bCompiled))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerConsistencyCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Generated consistency class should exist"), GeneratedClass))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Generated consistency class should preserve abstract flag"), GeneratedClass->HasAnyClassFlags(CLASS_Abstract));
		TestRunner->TestNotNull(TEXT("Generated consistency class should expose Score property"), FindFProperty<FProperty>(GeneratedClass, TEXT("Score")));
		TestRunner->TestNotNull(TEXT("Generated consistency class should expose GetScore function"), FindGeneratedFunction(GeneratedClass, TEXT("GetScore")));
	}
	}

	TEST_METHOD(ModuleFunctionInspection)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"CompilerModuleFunctionInspection",
			TEXT(R"(
	int SumWithDefault(int Value = 21, int Extra = 21)
	{
		return Value + Extra;
	}

	int Entry()
	{
		return SumWithDefault();
	}
	)"));
		if (!TestRunner->TestNotNull(TEXT("Fallback compiler inspection module should build"), Module))
		{
			return;
		}

		asIScriptFunction* SumWithDefault = GetFunctionByDecl(*TestRunner, *Module, TEXT("int SumWithDefault(int, int)"));
		if (!TestRunner->TestNotNull(TEXT("Compiled module should expose SumWithDefault"), SumWithDefault))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Compiled module should expose exactly two parameters for SumWithDefault"), static_cast<int32>(SumWithDefault->GetParamCount()), 2);

		int32 Result = 0;
		if (!TestRunner->TestTrue(TEXT("Compiled inspection module entry should execute"), ExecuteIntFunction(&Engine, TEXT("CompilerModuleFunctionInspection"), TEXT("int Entry()"), Result)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Compiled inspection module should preserve executable default values"), Result, 42);
	}
	}

	TEST_METHOD(EnumAvailability)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerEnumAvailability"),
			TEXT("CompilerEnumAvailability.as"),
			TEXT(R"(
	UENUM(BlueprintType)
	enum class ECompilerAvailabilityState : uint16
	{
		Alpha,
		Beta = 4,
		Gamma
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Enum availability input should compile"), bCompiled))
		{
			return;
		}

		const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(TEXT("ECompilerAvailabilityState"));
		if (!TestRunner->TestTrue(TEXT("Compiled enum metadata should be registered"), EnumDesc.IsValid()))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Compiled enum should have 3 declared values"), EnumDesc->ValueNames.Num(), 3);
		TestRunner->TestEqual(TEXT("Beta should have explicit value 4"), static_cast<int32>(EnumDesc->EnumValues[1]), 4);
	}
	}

	TEST_METHOD(DelegateSignatureConsistency)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerDelegateSignatureConsistency"),
			TEXT("CompilerDelegateSignatureConsistency.as"),
			TEXT(R"(
	delegate void FCompilerSingleCastSignature(int Value);
	event void FCompilerMultiCastSignature(UClass TypeValue, FString Label);

	UCLASS()
	class UCompilerDelegateCarrier : UObject
	{
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Delegate signature consistency input should compile"), bCompiled))
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> SingleCast = Engine.GetDelegate(TEXT("FCompilerSingleCastSignature"));
		const TSharedPtr<FAngelscriptDelegateDesc> MultiCast = Engine.GetDelegate(TEXT("FCompilerMultiCastSignature"));
		if (!TestRunner->TestTrue(TEXT("Single-cast delegate metadata should exist"), SingleCast.IsValid()))
		{
			return;
		}
		if (!TestRunner->TestTrue(TEXT("Multicast delegate metadata should exist"), MultiCast.IsValid()))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("Single-cast delegate should not be marked multicast"), SingleCast->bIsMulticast);
		TestRunner->TestTrue(TEXT("Multicast delegate should be marked multicast"), MultiCast->bIsMulticast);
		TestRunner->TestNotNull(TEXT("Single-cast delegate should materialize a UDelegateFunction"), SingleCast->Function);
		TestRunner->TestNotNull(TEXT("Multicast delegate should materialize a UDelegateFunction"), MultiCast->Function);
	}
	}

	TEST_METHOD(ClassLikeReflectionShape)
	{
		FAngelscriptEngine& EngineOwner = ASTEST_CREATE_ENGINE();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			TEXT("CompilerClassLikeReflectionShape"),
			TEXT("CompilerClassLikeReflectionShape.as"),
			TEXT(R"(
	UCLASS()
	class UCompilerClassLikeShapeCarrier : UObject
	{
		UFUNCTION()
		UClass EchoPlainClass(UClass Value)
		{
			return Value;
		}

		UFUNCTION()
		TSubclassOf<AActor> EchoActorClass(TSubclassOf<AActor> Value)
		{
			return Value;
		}

		UFUNCTION()
		TSoftClassPtr<AActor> EchoSoftActorClass(TSoftClassPtr<AActor> Value)
		{
			return Value;
		}
	}
	)"));

		if (!TestRunner->TestTrue(TEXT("Class-like reflection shape input should compile"), bCompiled))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerClassLikeShapeCarrier"));
		if (!TestRunner->TestNotNull(TEXT("Generated class should exist for class-like reflection shape input"), GeneratedClass))
		{
			return;
		}

		UFunction* EchoPlainClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass"));
		UFunction* EchoActorClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass"));
		UFunction* EchoSoftActorClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass"));
		if (!TestRunner->TestNotNull(TEXT("EchoPlainClass should exist"), EchoPlainClass) ||
			!TestRunner->TestNotNull(TEXT("EchoActorClass should exist"), EchoActorClass) ||
			!TestRunner->TestNotNull(TEXT("EchoSoftActorClass should exist"), EchoSoftActorClass))
		{
			return;
		}

		TestRunner->TestNotNull(TEXT("Plain class return should materialize as FClassProperty"), CastField<FClassProperty>(EchoPlainClass->GetReturnProperty()));
		TestRunner->TestNotNull(TEXT("Plain class parameter should materialize as FClassProperty"), CastField<FClassProperty>(FindFProperty<FProperty>(EchoPlainClass, TEXT("Value"))));

		FClassProperty* ActorReturnProperty = CastField<FClassProperty>(EchoActorClass->GetReturnProperty());
		FClassProperty* ActorParamProperty = CastField<FClassProperty>(FindFProperty<FProperty>(EchoActorClass, TEXT("Value")));
		if (!TestRunner->TestNotNull(TEXT("Subclass return should materialize as FClassProperty"), ActorReturnProperty) ||
			!TestRunner->TestNotNull(TEXT("Subclass parameter should materialize as FClassProperty"), ActorParamProperty))
		{
			return;
		}
		TestRunner->TestTrue(TEXT("Subclass return MetaClass should be AActor"), ActorReturnProperty->MetaClass == AActor::StaticClass());
		TestRunner->TestTrue(TEXT("Subclass parameter MetaClass should be AActor"), ActorParamProperty->MetaClass == AActor::StaticClass());

		FSoftClassProperty* SoftReturnProperty = CastField<FSoftClassProperty>(EchoSoftActorClass->GetReturnProperty());
		FSoftClassProperty* SoftParamProperty = CastField<FSoftClassProperty>(FindFProperty<FProperty>(EchoSoftActorClass, TEXT("Value")));
		if (!TestRunner->TestNotNull(TEXT("Soft class return should materialize as FSoftClassProperty"), SoftReturnProperty) ||
			!TestRunner->TestNotNull(TEXT("Soft class parameter should materialize as FSoftClassProperty"), SoftParamProperty))
		{
			return;
		}
		TestRunner->TestTrue(TEXT("Soft class return MetaClass should be AActor"), SoftReturnProperty->MetaClass == AActor::StaticClass());
		TestRunner->TestTrue(TEXT("Soft class parameter MetaClass should be AActor"), SoftParamProperty->MetaClass == AActor::StaticClass());
	}
	}
};

#endif
