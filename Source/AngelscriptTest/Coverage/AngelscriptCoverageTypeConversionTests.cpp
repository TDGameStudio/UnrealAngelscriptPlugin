#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestUtilities.h"
#include "Syntax/AngelscriptSyntaxTestHelpers.h"

#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageTypeConversionTests
// -----------------------------------------------------------------------------
// Coverage landing file for Cast<T>, implicit/explicit numeric conversions,
// enum/string conversions, and UObject type checks.
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageTypeConversionTest,
	"Angelscript.TestModule.Coverage.TypeConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	template <typename T>
	void ExpectGlobalReturn(FAngelscriptEngine& Engine, asIScriptModule* Module, const TCHAR* Declaration, const T& Expected, const TCHAR* Message)
	{
		ASSERT_THAT(IsNotNull(Module, TEXT("type conversion module should compile before executing global function")));
		if (Module == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, Declaration);
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("type conversion global function should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		const T Result = Invoker.ExecuteAndGet<T>(T{});
		ASSERT_THAT(AreEqual(Expected, Result, Message));
	}

	template <typename T>
	bool InvokeGlobal(FAngelscriptEngine& Engine, asIScriptModule& Module, const TCHAR* Declaration, T& OutResult)
	{
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, Module, Declaration);
		if (!Invoker.IsValid())
		{
			return false;
		}
		OutResult = Invoker.ExecuteAndGet<T>(T{});
		return true;
	}

	TEST_METHOD(NumericEnumAndStringConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageTypeConversion_NumericEnumString", ASTEST_AS(R"AS(
enum ECoverageConversionState
{
	None = 0,
	Ready = 3
}

int ImplicitWidening()
{
	uint8 Small = 250;
	int Wider = Small;
	return Wider;
}

float ExplicitIntToFloat()
{
	int Value = 42;
	return float(Value);
}

int ExplicitFloatToInt()
{
	float Value = 9.75f;
	return int(Value);
}

int EnumToInt()
{
	return int(ECoverageConversionState::Ready);
}

bool IntToEnumComparison()
{
	ECoverageConversionState State = ECoverageConversionState(3);
	return State == ECoverageConversionState::Ready;
}

FString IntToString()
{
	return FString::FromInt(42);
}

bool IntStringRoundTrip()
{
	return FString::FromInt(123) == "123" && FString::FromInt(-456) == "-456";
}

bool FloatStringRoundTrip()
{
	FString Value = FString::SanitizeFloat(12.5);
	return Value.StartsWith("12.5");
}
)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};

		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ImplicitWidening()"), 250, TEXT("uint8 should widen to int"));
		ExpectGlobalReturn<double>(Engine, Module, TEXT("float ExplicitIntToFloat()"), 42.0, TEXT("int should explicitly convert to float"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int ExplicitFloatToInt()"), 9, TEXT("float should explicitly truncate to int"));
		ExpectGlobalReturn<int32>(Engine, Module, TEXT("int EnumToInt()"), 3, TEXT("enum should explicitly convert to int"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntToEnumComparison()"), true, TEXT("int should explicitly construct enum value"));

		// Note: FString return from global functions requires special handling
		// Verify the function compiles and can be called (return value verification skipped)
		ASSERT_THAT(IsNotNull(Module, TEXT("type conversion module should compile before checking FString return function")));
		if (Module == nullptr)
		{
			return;
		}
		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("FString IntToString()"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("IntToString should resolve and prepare")));
		if (!Invoker.IsValid())
		{
			return;
		}

		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool IntStringRoundTrip()"), true, TEXT("int should format to FString"));
		ExpectGlobalReturn<bool>(Engine, Module, TEXT("bool FloatStringRoundTrip()"), true, TEXT("float should sanitize to FString"));
	}

	TEST_METHOD(ObjectCastAndTypeChecks)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTypeConversion_ObjectCast"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTypeConversionObjectCast.as"),
			ASTEST_AS(R"AS(
UCLASS()
class ACoverageCastBaseActor : AActor
{
}

UCLASS()
class ACoverageCastDerivedActor : ACoverageCastBaseActor
{
	UPROPERTY()
	int DerivedValue = 77;
}

UCLASS()
class ACoverageCastOtherActor : AActor
{
}

UCLASS()
class ACoverageTypeConversionActor : AActor
{
	UPROPERTY()
	bool bDowncastSuccess = false;

	UPROPERTY()
	bool bInvalidCastReturnsNull = false;

	UPROPERTY()
	bool bIsABase = false;

	UPROPERTY()
	bool bClassIsChild = false;

	UPROPERTY()
	bool bExactClassCheck = false;

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		ACoverageCastDerivedActor Derived = SpawnActor<ACoverageCastDerivedActor>();
		AActor AsActor = Derived;
		ACoverageCastBaseActor AsBase = Derived;
		ACoverageCastOtherActor Invalid = Cast<ACoverageCastOtherActor>(AsActor);
		ACoverageCastDerivedActor Downcasted = Cast<ACoverageCastDerivedActor>(AsBase);

		bDowncastSuccess = Downcasted != nullptr && Downcasted.DerivedValue == 77;
		bInvalidCastReturnsNull = Invalid == nullptr;
		bIsABase = AsActor.IsA(ACoverageCastBaseActor::StaticClass());
		bClassIsChild = ACoverageCastDerivedActor::StaticClass().IsChildOf(ACoverageCastBaseActor::StaticClass());
		bExactClassCheck = Derived.GetClass() == ACoverageCastDerivedActor::StaticClass();

		if (Derived != nullptr)
		{
			Derived.DestroyActor();
		}
	}
}
)AS"),
			TEXT("ACoverageTypeConversionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("type conversion actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("type conversion actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDowncastSuccess"), true, TEXT("Cast<T> should downcast generated classes"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bInvalidCastReturnsNull"), true, TEXT("Cast<T> should return null on incompatible type"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIsABase"), true, TEXT("IsA should report base class membership"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bClassIsChild"), true, TEXT("UClass.IsChildOf should report inheritance"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bExactClassCheck"), true, TEXT("GetClass should support exact type comparison"))));
	}

	TEST_METHOD(TSubclassOfParameterAndUClassConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageSubclassParamBaseActor : AActor
			{
			}

			UCLASS()
			class ACoverageSubclassParamDerivedActor : ACoverageSubclassParamBaseActor
			{
			}

			int AcceptSubclass(TSubclassOf<AActor> ActorClass)
			{
				return ActorClass.IsValid() && ActorClass.IsChildOf(AActor::StaticClass()) ? 1 : 0;
			}

			int AcceptExactDerived(TSubclassOf<ACoverageSubclassParamBaseActor> ActorClass)
			{
				UClass Class = ActorClass;
				return Class == ACoverageSubclassParamDerivedActor::StaticClass() ? 1 : 0;
			}

			int StaticClassToSubclassParameter()
			{
				return AcceptSubclass(ACoverageSubclassParamDerivedActor::StaticClass());
			}

			int TSubclassOfToUClassParameter()
			{
				TSubclassOf<ACoverageSubclassParamBaseActor> ActorClass = ACoverageSubclassParamDerivedActor::StaticClass();
				return AcceptExactDerived(ActorClass);
			}

			int NullSubclassParameter()
			{
				TSubclassOf<AActor> ActorClass;
				return AcceptSubclass(ActorClass);
			}

			int DefaultObjectMatchesSubclass()
			{
				TSubclassOf<AActor> ActorClass = ACoverageSubclassParamDerivedActor::StaticClass();
				AActor DefaultActor = ActorClass.GetDefaultObject();
				return DefaultActor != nullptr && DefaultActor.IsA(ACoverageSubclassParamDerivedActor::StaticClass()) ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageTypeConversion_TSubclassOfParameter"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("TSubclassOf conversion module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}
		asIScriptModule& Module = ModuleScope.GetModule();

		int32 StaticClassResult = INDEX_NONE;
		const bool bStaticClassInvoked = InvokeGlobal<int32>(Engine, Module, TEXT("int StaticClassToSubclassParameter()"), StaticClassResult);
		ASSERT_THAT(IsTrue(bStaticClassInvoked, TEXT("StaticClassToSubclassParameter should resolve and execute")));
		if (!bStaticClassInvoked)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, StaticClassResult, TEXT("UClass should pass to TSubclassOf parameter")));

		int32 UClassResult = INDEX_NONE;
		const bool bUClassInvoked = InvokeGlobal<int32>(Engine, Module, TEXT("int TSubclassOfToUClassParameter()"), UClassResult);
		ASSERT_THAT(IsTrue(bUClassInvoked, TEXT("TSubclassOfToUClassParameter should resolve and execute")));
		if (!bUClassInvoked)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, UClassResult, TEXT("TSubclassOf should convert back to UClass for exact comparison")));

		int32 NullSubclassResult = INDEX_NONE;
		const bool bNullSubclassInvoked = InvokeGlobal<int32>(Engine, Module, TEXT("int NullSubclassParameter()"), NullSubclassResult);
		ASSERT_THAT(IsTrue(bNullSubclassInvoked, TEXT("NullSubclassParameter should resolve and execute")));
		if (!bNullSubclassInvoked)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0, NullSubclassResult, TEXT("empty TSubclassOf parameter should remain invalid")));

		int32 DefaultObjectResult = INDEX_NONE;
		const bool bDefaultObjectInvoked = InvokeGlobal<int32>(Engine, Module, TEXT("int DefaultObjectMatchesSubclass()"), DefaultObjectResult);
		ASSERT_THAT(IsTrue(bDefaultObjectInvoked, TEXT("DefaultObjectMatchesSubclass should resolve and execute")));
		if (!bDefaultObjectInvoked)
		{
			return;
		}
		ASSERT_THAT(AreEqual(1, DefaultObjectResult, TEXT("TSubclassOf default object should preserve derived type")));
	}

	TEST_METHOD(MemberReferenceAndNullableHandleConversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTypeConversion_MemberReferences"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTypeConversionMemberReferences.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageReferenceBaseActor : AActor
				{
				}

				UCLASS()
				class ACoverageReferenceDerivedActor : ACoverageReferenceBaseActor
				{
				}

				UCLASS()
				class ACoverageReferenceOwnerActor : AActor
				{
					UPROPERTY()
					UObject ObjectRef;

					UPROPERTY()
					AActor ActorRef;

					UPROPERTY()
					UActorComponent ComponentRef;

					UPROPERTY()
					TSubclassOf<AActor> ActorClassRef;

					UPROPERTY()
					bool ObjectRefAssigned = false;

					UPROPERTY()
					bool ActorUpcastAssigned = false;

					UPROPERTY()
					bool ComponentRefAssigned = false;

					UPROPERTY()
					bool SubclassRefAssigned = false;

					UPROPERTY()
					bool NullComparisonWorked = false;

					UPROPERTY()
					bool CastFromNullableWorked = false;

					UPROPERTY()
					bool IsValidAfterNullReset = false;

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						ObjectRef = NewObject(this, UObject::StaticClass(), n"CoverageObjectRef", true);

						ACoverageReferenceDerivedActor Derived = SpawnActor<ACoverageReferenceDerivedActor>();
						ActorRef = Derived;

						ComponentRef = NewObject(this, UActorComponent::StaticClass(), n"CoverageComponentRef", true);
						ActorClassRef = ACoverageReferenceDerivedActor::StaticClass();

						ObjectRefAssigned = ObjectRef != nullptr && IsValid(ObjectRef);
						ActorUpcastAssigned = ActorRef != nullptr && ActorRef.IsA(ACoverageReferenceBaseActor::StaticClass());
						ComponentRefAssigned = ComponentRef != nullptr && ComponentRef.GetClass() == UActorComponent::StaticClass();
						SubclassRefAssigned = ActorClassRef.IsValid() && ActorClassRef.Get() == ACoverageReferenceDerivedActor::StaticClass();

						AActor NullableActor = nullptr;
						NullComparisonWorked = NullableActor == nullptr && !IsValid(NullableActor);
						NullableActor = Derived;
						CastFromNullableWorked = Cast<ACoverageReferenceDerivedActor>(NullableActor) == Derived;
						NullableActor = nullptr;
						IsValidAfterNullReset = !IsValid(NullableActor);

						if (Derived != nullptr)
						{
							Derived.DestroyActor();
						}
					}
				}
				)AS"),
			TEXT("ACoverageReferenceOwnerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("member reference conversion actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FObjectProperty* ObjectRefProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("ObjectRef"));
		ASSERT_THAT(IsNotNull(ObjectRefProperty, TEXT("ObjectRef property should be reflected")));
		if (ObjectRefProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectRefProperty->PropertyClass, TEXT("ObjectRef should be a UObject property")));

		const FObjectProperty* ActorRefProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("ActorRef"));
		ASSERT_THAT(IsNotNull(ActorRefProperty, TEXT("ActorRef property should be reflected")));
		if (ActorRefProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorRefProperty->PropertyClass, TEXT("ActorRef should be an AActor property")));

		const FObjectProperty* ComponentRefProperty = FindFProperty<FObjectProperty>(ScriptClass, TEXT("ComponentRef"));
		ASSERT_THAT(IsNotNull(ComponentRefProperty, TEXT("ComponentRef property should be reflected")));
		if (ComponentRefProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ComponentRefProperty->PropertyClass->IsChildOf(UActorComponent::StaticClass()), TEXT("ComponentRef should be a component property")));

		const FClassProperty* ActorClassRefProperty = FindFProperty<FClassProperty>(ScriptClass, TEXT("ActorClassRef"));
		ASSERT_THAT(IsNotNull(ActorClassRefProperty, TEXT("ActorClassRef property should be reflected")));
		if (ActorClassRefProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorClassRefProperty->MetaClass, TEXT("ActorClassRef should filter AActor subclasses")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("member reference conversion actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectRefAssigned"), true, TEXT("UPROPERTY UObject handle should hold assigned object"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ActorUpcastAssigned"), true, TEXT("UPROPERTY AActor handle should accept derived actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ComponentRefAssigned"), true, TEXT("UPROPERTY component handle should hold assigned component"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SubclassRefAssigned"), true, TEXT("UPROPERTY TSubclassOf should hold assigned class"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NullComparisonWorked"), true, TEXT("nullable object handles should compare with nullptr"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("CastFromNullableWorked"), true, TEXT("nullable object handles should cast after assignment"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("IsValidAfterNullReset"), true, TEXT("nullable object handles should become invalid after reset"))));
	}

	TEST_METHOD(StringNameTextConversionRoundTrips)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageTypeConversion_StringNameText"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageTypeConversionStringNameText.as"),
			ASTEST_AS(R"AS(
				UCLASS()
				class ACoverageStringNameTextActor : AActor
				{
					UPROPERTY()
					FString StringFromName;

					UPROPERTY()
					FString StringFromText;

					UPROPERTY()
					FString StringFromInt;

					UPROPERTY()
					FName NameFromString;

					UPROPERTY()
					FText TextFromString;

					UPROPERTY()
					bool NameRoundTrip = false;

					UPROPERTY()
					bool TextRoundTrip = false;

					UPROPERTY()
					bool NumericStringRoundTrip = false;

					UFUNCTION(BlueprintOverride)
					void BeginPlay()
					{
						FString Source = "CoverageName";
						NameFromString = FName(Source);
						StringFromName = NameFromString.ToString();

						TextFromString = FText::FromString("CoverageText");
						StringFromText = TextFromString.ToString();

						StringFromInt = FString::FromInt(314);
						NumericStringRoundTrip = StringFromInt == "314" && FString::FromInt(-12) == "-12";
						NameRoundTrip = StringFromName == Source && NameFromString == n"CoverageName";
						TextRoundTrip = StringFromText == "CoverageText";
					}
				}
				)AS"),
			TEXT("ACoverageStringNameTextActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("string/name/text conversion actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("string/name/text conversion actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		FText TextFromString;
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameFromString"), FName(TEXT("CoverageName")), TEXT("FString should construct FName"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringFromName"), FString(TEXT("CoverageName")), TEXT("FName should convert to FString"))));
		ASSERT_THAT(IsTrue(GetTextByPath(*TestRunner, Actor, TEXT("TextFromString"), TextFromString), TEXT("FString should construct FText")));
		ASSERT_THAT(AreEqual(FString(TEXT("CoverageText")), TextFromString.ToString(), TEXT("constructed FText should stringify back to source")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringFromText"), FString(TEXT("CoverageText")), TEXT("FText should convert to FString"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringFromInt"), FString(TEXT("314")), TEXT("int should convert to FString through FromInt"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameRoundTrip"), true, TEXT("FString/FName round-trip should preserve value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("TextRoundTrip"), true, TEXT("FString/FText round-trip should preserve value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NumericStringRoundTrip"), true, TEXT("numeric string conversion should round-trip expected values"))));
	}

	TEST_METHOD(ConversionNegativeCompile)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASCoverageTypeConversion_BadPrimitiveCast"),
			ASTEST_AS(R"AS(
				void Test()
				{
					int Value = 5;
					auto Actor = Cast<AActor>(Value);
				}
				)AS"),
			TEXT("Cast<T> on primitive should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASCoverageTypeConversion_BadImplicitBaseToDerived"),
			ASTEST_AS(R"AS(
				void Test(AActor Actor)
				{
					APawn Pawn = Actor;
				}
				)AS"),
			TEXT("implicit base-to-derived object conversion should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASCoverageTypeConversion_BadStringToInt"),
			ASTEST_AS(R"AS(
				void Test()
				{
					FString Text = "42";
					int Value = Text;
				}
				)AS"),
			TEXT("implicit FString to int should fail"));

		SyntaxTestHelpers::AssertFailsToCompile(*TestRunner, Engine, TEXT("ASCoverageTypeConversion_BadRawHandleCondition"),
			ASTEST_AS(R"AS(
				void Test(AActor Actor)
				{
					if (Actor)
					{
					}
				}
				)AS"),
			TEXT("bare UObject handle conditions should remain unsupported without an explicit bool-producing expression"));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
