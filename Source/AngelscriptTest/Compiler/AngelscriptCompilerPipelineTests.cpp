#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Fallback compiler validation should compile delegate/enum/class transfer input")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> SimpleDelegate = Engine.GetDelegate(TEXT("FCompilerTransferDelegate"));
		const TSharedPtr<FAngelscriptDelegateDesc> MultiDelegate = Engine.GetDelegate(TEXT("FCompilerTransferEvent"));
		if (!this->Assert.IsTrue(SimpleDelegate.IsValid(), TEXT("Simple delegate metadata should be registered after compile")))
		{
			return;
		}
		if (!this->Assert.IsTrue(MultiDelegate.IsValid(), TEXT("Multicast delegate metadata should be registered after compile")))
		{
			return;
		}

		ASSERT_THAT(IsFalse(SimpleDelegate->bIsMulticast, TEXT("Simple delegate should remain single-cast")));
		ASSERT_THAT(IsTrue(MultiDelegate->bIsMulticast, TEXT("Event delegate should remain multicast")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerTransferObject"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Generated class should exist for delegate/enum/class transfer input")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(GeneratedClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Generated class should preserve abstract flag")));
		ASSERT_THAT(IsNotNull(FindFProperty<FProperty>(GeneratedClass, TEXT("Score")), TEXT("Generated Score property should exist")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(GeneratedClass, TEXT("GetScore")), TEXT("Generated GetScore function should exist")));
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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Fallback compiler validation should compile function defaults and class-like signatures")))
		{
			return;
		}

		int32 Result = 0;
		const bool bExecuted = ExecuteIntFunction(&Engine, TEXT("CompilerFunctionDefaultsAndClassLikeCompile"), TEXT("int Entry()"), Result);
		if (!this->Assert.IsTrue(bExecuted, TEXT("Function default validation should execute compiled entry point")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, Result, TEXT("Function default values should be honored at runtime")));

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerFunctionCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Generated class should exist for class-like signature input")))
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass")), TEXT("EchoPlainClass function should exist")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass")), TEXT("EchoActorClass function should exist")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass")), TEXT("EchoSoftActorClass function should exist")));
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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Fallback compiler validation should compile property default input")))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerDefaultsCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Generated class should exist for property default input")))
		{
			return;
		}

		UObject* DefaultObject = GeneratedClass->GetDefaultObject();
		if (!this->Assert.IsNotNull(DefaultObject, TEXT("Generated class default object should exist")))
		{
			return;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("Score"));
		if (!this->Assert.IsNotNull(ScoreProperty, TEXT("Generated Score property should exist")))
		{
			return;
		}

		const int32 ScoreValue = ScoreProperty->GetPropertyValue_InContainer(DefaultObject);
		ASSERT_THAT(AreEqual(21, ScoreValue, TEXT("Generated default object should materialize overridden int default")));
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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Generated class consistency input should compile")))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerConsistencyCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Generated consistency class should exist")))
		{
			return;
		}

		ASSERT_THAT(IsTrue(GeneratedClass->HasAnyClassFlags(CLASS_Abstract), TEXT("Generated consistency class should preserve abstract flag")));
		ASSERT_THAT(IsNotNull(FindFProperty<FProperty>(GeneratedClass, TEXT("Score")), TEXT("Generated consistency class should expose Score property")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(GeneratedClass, TEXT("GetScore")), TEXT("Generated consistency class should expose GetScore function")));
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
		if (!this->Assert.IsNotNull(Module, TEXT("Fallback compiler inspection module should build")))
		{
			return;
		}

		asIScriptFunction* SumWithDefault = GetFunctionByDecl(*TestRunner, *Module, TEXT("int SumWithDefault(int, int)"));
		if (!this->Assert.IsNotNull(SumWithDefault, TEXT("Compiled module should expose SumWithDefault")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, static_cast<int32>(SumWithDefault->GetParamCount()), TEXT("Compiled module should expose exactly two parameters for SumWithDefault")));

		int32 Result = 0;
		if (!this->Assert.IsTrue(ExecuteIntFunction(&Engine, TEXT("CompilerModuleFunctionInspection"), TEXT("int Entry()"), Result), TEXT("Compiled inspection module entry should execute")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, Result, TEXT("Compiled inspection module should preserve executable default values")));
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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Enum availability input should compile")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptEnumDesc> EnumDesc = Engine.GetEnum(TEXT("ECompilerAvailabilityState"));
		if (!this->Assert.IsTrue(EnumDesc.IsValid(), TEXT("Compiled enum metadata should be registered")))
		{
			return;
		}

		ASSERT_THAT(AreEqual(3, EnumDesc->ValueNames.Num(), TEXT("Compiled enum should have 3 declared values")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EnumDesc->EnumValues[1]), TEXT("Beta should have explicit value 4")));
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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Delegate signature consistency input should compile")))
		{
			return;
		}

		const TSharedPtr<FAngelscriptDelegateDesc> SingleCast = Engine.GetDelegate(TEXT("FCompilerSingleCastSignature"));
		const TSharedPtr<FAngelscriptDelegateDesc> MultiCast = Engine.GetDelegate(TEXT("FCompilerMultiCastSignature"));
		if (!this->Assert.IsTrue(SingleCast.IsValid(), TEXT("Single-cast delegate metadata should exist")))
		{
			return;
		}
		if (!this->Assert.IsTrue(MultiCast.IsValid(), TEXT("Multicast delegate metadata should exist")))
		{
			return;
		}

		ASSERT_THAT(IsFalse(SingleCast->bIsMulticast, TEXT("Single-cast delegate should not be marked multicast")));
		ASSERT_THAT(IsTrue(MultiCast->bIsMulticast, TEXT("Multicast delegate should be marked multicast")));
		ASSERT_THAT(IsNotNull(SingleCast->Function, TEXT("Single-cast delegate should materialize a UDelegateFunction")));
		ASSERT_THAT(IsNotNull(MultiCast->Function, TEXT("Multicast delegate should materialize a UDelegateFunction")));
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

		if (!this->Assert.IsTrue(bCompiled, TEXT("Class-like reflection shape input should compile")))
		{
			return;
		}

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UCompilerClassLikeShapeCarrier"));
		if (!this->Assert.IsNotNull(GeneratedClass, TEXT("Generated class should exist for class-like reflection shape input")))
		{
			return;
		}

		UFunction* EchoPlainClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoPlainClass"));
		UFunction* EchoActorClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoActorClass"));
		UFunction* EchoSoftActorClass = FindGeneratedFunction(GeneratedClass, TEXT("EchoSoftActorClass"));
		if (!this->Assert.IsNotNull(EchoPlainClass, TEXT("EchoPlainClass should exist")) ||
			!this->Assert.IsNotNull(EchoActorClass, TEXT("EchoActorClass should exist")) ||
			!this->Assert.IsNotNull(EchoSoftActorClass, TEXT("EchoSoftActorClass should exist")))
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FClassProperty>(EchoPlainClass->GetReturnProperty()), TEXT("Plain class return should materialize as FClassProperty")));
		ASSERT_THAT(IsNotNull(CastField<FClassProperty>(FindFProperty<FProperty>(EchoPlainClass, TEXT("Value"))), TEXT("Plain class parameter should materialize as FClassProperty")));

		FClassProperty* ActorReturnProperty = CastField<FClassProperty>(EchoActorClass->GetReturnProperty());
		FClassProperty* ActorParamProperty = CastField<FClassProperty>(FindFProperty<FProperty>(EchoActorClass, TEXT("Value")));
		if (!this->Assert.IsNotNull(ActorReturnProperty, TEXT("Subclass return should materialize as FClassProperty")) ||
			!this->Assert.IsNotNull(ActorParamProperty, TEXT("Subclass parameter should materialize as FClassProperty")))
		{
			return;
		}
		ASSERT_THAT(IsTrue(ActorReturnProperty->MetaClass == AActor::StaticClass(), TEXT("Subclass return MetaClass should be AActor")));
		ASSERT_THAT(IsTrue(ActorParamProperty->MetaClass == AActor::StaticClass(), TEXT("Subclass parameter MetaClass should be AActor")));

		FSoftClassProperty* SoftReturnProperty = CastField<FSoftClassProperty>(EchoSoftActorClass->GetReturnProperty());
		FSoftClassProperty* SoftParamProperty = CastField<FSoftClassProperty>(FindFProperty<FProperty>(EchoSoftActorClass, TEXT("Value")));
		if (!this->Assert.IsNotNull(SoftReturnProperty, TEXT("Soft class return should materialize as FSoftClassProperty")) ||
			!this->Assert.IsNotNull(SoftParamProperty, TEXT("Soft class parameter should materialize as FSoftClassProperty")))
		{
			return;
		}
		ASSERT_THAT(IsTrue(SoftReturnProperty->MetaClass == AActor::StaticClass(), TEXT("Soft class return MetaClass should be AActor")));
		ASSERT_THAT(IsTrue(SoftParamProperty->MetaClass == AActor::StaticClass(), TEXT("Soft class parameter MetaClass should be AActor")));
	}
	}
};

#endif
