#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeInterfaceTestHelpers.h"
#include "AngelscriptNativeInterfaceTestTypes.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorComponent.h"
#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/PropertyOptional.h"
#include "UObject/ScriptInterface.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

// -----------------------------------------------------------------------------
// AngelscriptCoverageUClassPropertyTests
// -----------------------------------------------------------------------------
// UCLASS-owned member/property coverage. AngelscriptCoverageUClassTests.cpp keeps
// the class declaration, specifier, lifecycle, and component surfaces; this file
// keeps the reflected member type and runtime property matrix in one place.
//
// Coverage matrix index:
//
// | TEST_METHOD | Coverage axis | Execution surface | Notes |
// |---|---|---|---|
// | UClassScalarTextStructMemberMatrix | bool, numeric, string/name/text, math structs, enum | Actor BeginPlay + property reads | Core reflected member types |
// | UClassReferenceMemberMatrix | UObject/TObjectPtr/instanced/script object/actor/component/class/weak/soft refs | Actor BeginPlay + reflection path reads | Reference property permutations |
// | UClassReferenceContainerMemberMatrix | Arrays, sets, and maps of object/class/weak/soft/instanced refs | Actor BeginPlay + container reads | Reference container permutations |
// | UClassInterfaceMemberMatrix | Native interface refs/arrays and C++ TScriptInterface readback | Actor BeginPlay + interface dispatch | Interface property baseline |
// | UClassContainerMemberMatrix | TArray/TSet/TMap scalar, string/name/text, struct, actor refs | Actor BeginPlay + container reads | UCLASS-owned containers |
// | UClassEnumContainerMemberMatrix | TArray/TSet/TMap with UENUM keys and values | Actor BeginPlay + container reads | Enum container permutations |
// | UClassScriptStructMemberContainerMatrix | AS USTRUCT direct, array, map value, set element members | Actor BeginPlay + nested property reads | Script struct member/container baseline |
// | UClassOptionalMemberMatrix | TOptional bool/numeric/name/enum/string/struct/object members | Actor BeginPlay + optional reads | Optional property baseline |
// | UClassDelegateMemberMatrix | Single-cast delegate, plain/BlueprintAssignable/BlueprintCallable event members | BindUFunction + Execute/Broadcast | Delegate property runtime path |
// | UClassDelegateReturnMemberMatrix | Single-cast delegate members with bool/int/float/FString/FVector returns | BindUFunction + Execute | Delegate member return matrix |
// | UClassDelegateParameterMemberMatrix | UCLASS delegate members passed to UFUNCTION value and const-ref parameters | UFUNCTION invocation + callback execution | Delegate member parameter path |
// | UClassDelegateTypedPayloadMemberMatrix | Delegate members with bool/float/string/name/vector/object/enum payloads | BindUFunction + Execute/Broadcast | Delegate member parameter matrix |
// | UClassDelegateStructPayloadMemberMatrix | Single-cast and multicast delegate members with AS USTRUCT payloads | BindUFunction + Execute/Broadcast | Delegate property struct argument path |
// | UClassDelegateContainerPayloadMemberMatrix | Single-cast and multicast delegate members with container payloads | BindUFunction + Execute/Broadcast | Delegate container argument path |
// | UClassDefaultValueAndCDOMatrix | Member initializers, uninitialized defaults, inherited defaults, CDO state | CDO + spawned actor | Default value coverage |
// | UClassAccessAndBlueprintVisibilityMatrix | public/protected/private members with BlueprintReadWrite/ReadOnly/Hidden/AllowPrivateAccess | UObject invocation + property flags | Access/specifier interaction |
// | UClassNonUPropertyMemberMatrix | Script members without UPROPERTY | Runtime function + reflected result fields | Explicit-UPROPERTY flag boundary |
// | UClassPropertySpecifierAndMetadataMatrix | Edit/visibility/config/save/transient/advanced metadata flags | FProperty flags + metadata | Property specifier matrix |
//
TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUClassPropertyTest,
	"Angelscript.TestModule.Coverage.UClass.Property",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CompileUClassPropertyFixture(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName, const FString& Filename, const FString& ScriptSource)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptSource),
			*FString::Printf(TEXT("UCLASS property coverage module '%s' should compile"), *ModuleName.ToString()));
	}

	static bool HasAllFlags(const FProperty* Property, EPropertyFlags RequiredFlags)
	{
		return Property != nullptr && Property->HasAllPropertyFlags(RequiredFlags);
	}

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

	TEST_METHOD(UClassScalarTextStructMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_ScalarTextStructMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM()
			enum EUClassPropertyScalarState
			{
				Idle,
				Armed,
				Fired
			}

			UCLASS()
			class ACoverageUClassScalarTextStructActor : AActor
			{
				UPROPERTY()
				bool bBoolValue = true;

				UPROPERTY()
				int8 Int8Value = -8;

				UPROPERTY()
				int16 Int16Value = -1600;

				UPROPERTY()
				int IntValue = 3200;

				UPROPERTY()
				int64 Int64Value = -6400000000;

				UPROPERTY()
				uint8 UInt8Value = 8;

				UPROPERTY()
				uint16 UInt16Value = 1600;

				UPROPERTY()
				uint UIntValue = 3200;

				UPROPERTY()
				uint64 UInt64Value = 6400000000;

				UPROPERTY()
				float FloatValue = 1.25f;

				UPROPERTY()
				double DoubleValue = 2.5;

				UPROPERTY()
				FString StringValue = "MemberString";

				UPROPERTY()
				FName NameValue = n"MemberName";

				UPROPERTY()
				FText TextValue;

				UPROPERTY()
				FVector VectorValue = FVector(1, 2, 3);

				UPROPERTY()
				FVector2D Vector2DValue = FVector2D(4, 5);

				UPROPERTY()
				FIntPoint IntPointValue = FIntPoint(6, 7);

				UPROPERTY()
				FRotator RotatorValue = FRotator(10, 20, 30);

				UPROPERTY()
				FQuat QuatValue;

				UPROPERTY()
				FTransform TransformValue;

				UPROPERTY()
				FLinearColor LinearColorValue = FLinearColor(0.1, 0.2, 0.3, 0.4);

				UPROPERTY()
				FColor ColorValue = FColor(10, 20, 30, 40);

				UPROPERTY()
				EUClassPropertyScalarState EnumValue = EUClassPropertyScalarState::Armed;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TextValue = FText::FromString("Member Text");
					QuatValue = FQuat(FRotator(0, 90, 0));
					TransformValue = FTransform(FRotator(0, 45, 0), FVector(3, 4, 5), FVector(2, 2, 2));
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyScalarTextStructMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassScalarTextStructActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Scalar/text/struct member actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(ScriptClass, TEXT("bBoolValue")), TEXT("bool member should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FInt8Property>(ScriptClass, TEXT("Int8Value")), TEXT("int8 member should reflect as FInt8Property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FInt16Property>(ScriptClass, TEXT("Int16Value")), TEXT("int16 member should reflect as FInt16Property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ScriptClass, TEXT("IntValue")), TEXT("int member should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FInt64Property>(ScriptClass, TEXT("Int64Value")), TEXT("int64 member should reflect as FInt64Property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FByteProperty>(ScriptClass, TEXT("UInt8Value")), TEXT("uint8 member should reflect as FByteProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FUInt16Property>(ScriptClass, TEXT("UInt16Value")), TEXT("uint16 member should reflect as FUInt16Property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FUInt32Property>(ScriptClass, TEXT("UIntValue")), TEXT("uint member should reflect as FUInt32Property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FUInt64Property>(ScriptClass, TEXT("UInt64Value")), TEXT("uint64 member should reflect as FUInt64Property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(ScriptClass, TEXT("FloatValue")), TEXT("float member should reflect as FDoubleProperty in this AS configuration")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(ScriptClass, TEXT("DoubleValue")), TEXT("double member should reflect as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(ScriptClass, TEXT("StringValue")), TEXT("FString member should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FNameProperty>(ScriptClass, TEXT("NameValue")), TEXT("FName member should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FTextProperty>(ScriptClass, TEXT("TextValue")), TEXT("FText member should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("VectorValue")), TEXT("FVector member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("Vector2DValue")), TEXT("FVector2D member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("IntPointValue")), TEXT("FIntPoint member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("RotatorValue")), TEXT("FRotator member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("QuatValue")), TEXT("FQuat member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("TransformValue")), TEXT("FTransform member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("LinearColorValue")), TEXT("FLinearColor member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ScriptClass, TEXT("ColorValue")), TEXT("FColor member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FEnumProperty>(ScriptClass, TEXT("EnumValue")), TEXT("UENUM member should reflect as FEnumProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Scalar/text/struct member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBoolValue"), true, TEXT("bool member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt8Property, int8>(*TestRunner, Actor, TEXT("Int8Value"), static_cast<int8>(-8), TEXT("int8 member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt16Property, int16>(*TestRunner, Actor, TEXT("Int16Value"), static_cast<int16>(-1600), TEXT("int16 member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntValue"), 3200, TEXT("int member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FInt64Property, int64>(*TestRunner, Actor, TEXT("Int64Value"), static_cast<int64>(-6400000000LL), TEXT("int64 member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FByteProperty, uint8>(*TestRunner, Actor, TEXT("UInt8Value"), static_cast<uint8>(8), TEXT("uint8 member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt16Property, uint16>(*TestRunner, Actor, TEXT("UInt16Value"), static_cast<uint16>(1600), TEXT("uint16 member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt32Property, uint32>(*TestRunner, Actor, TEXT("UIntValue"), static_cast<uint32>(3200), TEXT("uint member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FUInt64Property, uint64>(*TestRunner, Actor, TEXT("UInt64Value"), static_cast<uint64>(6400000000ull), TEXT("uint64 member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FloatValue"), 1.25, TEXT("float member should round-trip through FDoubleProperty"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleValue"), 2.5, TEXT("double member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("MemberString")), TEXT("FString member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("MemberName")), TEXT("FName member should round-trip"))));

		FText TextValue;
		ASSERT_THAT(IsTrue(GetTextByPath(*TestRunner, Actor, TEXT("TextValue"), TextValue), TEXT("FText member should be readable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Member Text")), TextValue.ToString(), TEXT("FText member should round-trip")));

		FVector VectorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorValue"), VectorValue), TEXT("FVector member should be readable")));
		ASSERT_THAT(IsTrue(VectorValue.Equals(FVector(1, 2, 3), 0.001), TEXT("FVector member should round-trip")));

		FVector2D Vector2DValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector2D>(*TestRunner, Actor, TEXT("Vector2DValue"), Vector2DValue), TEXT("FVector2D member should be readable")));
		ASSERT_THAT(IsTrue(Vector2DValue.Equals(FVector2D(4, 5), 0.001), TEXT("FVector2D member should round-trip")));

		FIntPoint IntPointValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FIntPoint>(*TestRunner, Actor, TEXT("IntPointValue"), IntPointValue), TEXT("FIntPoint member should be readable")));
		ASSERT_THAT(AreEqual(FIntPoint(6, 7), IntPointValue, TEXT("FIntPoint member should round-trip")));

		FRotator RotatorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("RotatorValue"), RotatorValue), TEXT("FRotator member should be readable")));
		ASSERT_THAT(IsTrue(RotatorValue.Equals(FRotator(10, 20, 30), 0.001), TEXT("FRotator member should round-trip")));

		FQuat QuatValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FQuat>(*TestRunner, Actor, TEXT("QuatValue"), QuatValue), TEXT("FQuat member should be readable")));
		ASSERT_THAT(IsTrue(QuatValue.Equals(FQuat(FRotator(0, 90, 0)), 0.001), TEXT("FQuat member should round-trip")));

		FTransform TransformValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FTransform>(*TestRunner, Actor, TEXT("TransformValue"), TransformValue), TEXT("FTransform member should be readable")));
		ASSERT_THAT(IsTrue(TransformValue.Equals(FTransform(FRotator(0, 45, 0), FVector(3, 4, 5), FVector(2, 2, 2)), 0.001), TEXT("FTransform member should round-trip")));

		FLinearColor LinearColorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FLinearColor>(*TestRunner, Actor, TEXT("LinearColorValue"), LinearColorValue), TEXT("FLinearColor member should be readable")));
		ASSERT_THAT(IsTrue(LinearColorValue.Equals(FLinearColor(0.1f, 0.2f, 0.3f, 0.4f), 0.001f), TEXT("FLinearColor member should round-trip")));

		FColor ColorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FColor>(*TestRunner, Actor, TEXT("ColorValue"), ColorValue), TEXT("FColor member should be readable")));
		ASSERT_THAT(AreEqual(FColor(10, 20, 30, 40), ColorValue, TEXT("FColor member should round-trip")));

		int64 EnumValue = INDEX_NONE;
		ASSERT_THAT(IsTrue(GetEnumByPath(*TestRunner, Actor, TEXT("EnumValue"), EnumValue), TEXT("UENUM member should be readable")));
		ASSERT_THAT(AreEqual(1LL, EnumValue, TEXT("UENUM member should round-trip the Armed value")));
	}

	TEST_METHOD(UClassReferenceMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_ReferenceMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPropertyReferenceObject : UObject
			{
				UPROPERTY()
				int ObjectValue = 17;
			}

			UCLASS()
			class UCoverageUClassPropertyReferenceComponent : UActorComponent
			{
				UPROPERTY()
				int ComponentValue = 23;
			}

			UCLASS()
			class ACoverageUClassPropertyReferenceActor : AActor
			{
				UPROPERTY()
				UObject ObjectRef;

				UPROPERTY()
				TObjectPtr<UObject> SmartObjectRef;

				UPROPERTY()
				TObjectPtr<UCoverageUClassPropertyReferenceObject> SmartScriptObjectRef;

				UPROPERTY(Instanced)
				UCoverageUClassPropertyReferenceObject InstancedObjectRef;

				UPROPERTY()
				UCoverageUClassPropertyReferenceObject ScriptObjectRef;

				UPROPERTY()
				AActor ActorRef;

				UPROPERTY()
				TObjectPtr<AActor> SmartActorRef;

				UPROPERTY()
				ACoverageUClassPropertyReferenceActor ScriptActorRef;

				UPROPERTY()
				UActorComponent ComponentRef;

				UPROPERTY()
				TObjectPtr<UCoverageUClassPropertyReferenceComponent> SmartScriptComponentRef;

				UPROPERTY()
				UCoverageUClassPropertyReferenceComponent ScriptComponentRef;

				UPROPERTY()
				TSubclassOf<AActor> ActorClassRef;

				UPROPERTY()
				TSubclassOf<ACoverageUClassPropertyReferenceActor> ScriptActorClassRef;

				UPROPERTY()
				TWeakObjectPtr<AActor> WeakActorRef;

				UPROPERTY()
				TWeakObjectPtr<ACoverageUClassPropertyReferenceActor> WeakScriptActorRef;

				UPROPERTY()
				TWeakObjectPtr<UCoverageUClassPropertyReferenceObject> WeakScriptObjectRef;

				UPROPERTY()
				TSoftObjectPtr<AActor> SoftActorRef;

				UPROPERTY()
				TSoftObjectPtr<ACoverageUClassPropertyReferenceActor> SoftScriptActorRef;

				UPROPERTY()
				TSoftObjectPtr<UCoverageUClassPropertyReferenceObject> SoftScriptObjectRef;

				UPROPERTY()
				TSoftClassPtr<AActor> SoftActorClassRef;

				UPROPERTY()
				TSoftClassPtr<ACoverageUClassPropertyReferenceActor> SoftScriptActorClassRef;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ScriptObjectRef = Cast<UCoverageUClassPropertyReferenceObject>(
						NewObject(this, UCoverageUClassPropertyReferenceObject::StaticClass(), n"CoverageUClassPropertyReferenceObject"));
					InstancedObjectRef = Cast<UCoverageUClassPropertyReferenceObject>(
						NewObject(this, UCoverageUClassPropertyReferenceObject::StaticClass(), n"CoverageUClassPropertyInstancedObject"));
					ObjectRef = ScriptObjectRef;
					SmartObjectRef = ScriptObjectRef;
					SmartScriptObjectRef = ScriptObjectRef;
					ActorRef = this;
					SmartActorRef = this;
					ScriptActorRef = this;
					ScriptComponentRef = Cast<UCoverageUClassPropertyReferenceComponent>(
						NewObject(this, UCoverageUClassPropertyReferenceComponent::StaticClass(), n"CoverageUClassPropertyReferenceComponent", true));
					ComponentRef = ScriptComponentRef;
					SmartScriptComponentRef = ScriptComponentRef;
					ActorClassRef = ACoverageUClassPropertyReferenceActor::StaticClass();
					ScriptActorClassRef = ACoverageUClassPropertyReferenceActor::StaticClass();
					WeakActorRef = this;
					WeakScriptActorRef = this;
					WeakScriptObjectRef = ScriptObjectRef;
					SoftActorRef = this;
					SoftScriptActorRef = this;
					SoftScriptObjectRef = ScriptObjectRef;
					SoftActorClassRef = ACoverageUClassPropertyReferenceActor::StaticClass();
					SoftScriptActorClassRef = ACoverageUClassPropertyReferenceActor::StaticClass();
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyReferenceMemberMatrix.as"), ScriptSource)));

		UClass* ObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPropertyReferenceObject"));
		UClass* ComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPropertyReferenceComponent"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPropertyReferenceActor"));
		ASSERT_THAT(IsNotNull(ObjectClass, TEXT("reference UObject class should be generated")));
		ASSERT_THAT(IsNotNull(ComponentClass, TEXT("reference component class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("reference actor class should be generated")));
		if (ObjectClass == nullptr || ComponentClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* ObjectRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ObjectRef"));
		FObjectPropertyBase* SmartObjectRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SmartObjectRef"));
		FObjectPropertyBase* SmartScriptObjectRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SmartScriptObjectRef"));
		FObjectPropertyBase* InstancedObjectRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("InstancedObjectRef"));
		FObjectPropertyBase* ScriptObjectRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ScriptObjectRef"));
		FObjectPropertyBase* ActorRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ActorRef"));
		FObjectPropertyBase* SmartActorRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SmartActorRef"));
		FObjectPropertyBase* ScriptActorRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ScriptActorRef"));
		FObjectPropertyBase* ComponentRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ComponentRef"));
		FObjectPropertyBase* SmartScriptComponentRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SmartScriptComponentRef"));
		FObjectPropertyBase* ScriptComponentRefProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ScriptComponentRef"));
		FClassProperty* ActorClassRefProperty = FindFProperty<FClassProperty>(ActorClass, TEXT("ActorClassRef"));
		FClassProperty* ScriptActorClassRefProperty = FindFProperty<FClassProperty>(ActorClass, TEXT("ScriptActorClassRef"));
		FWeakObjectProperty* WeakActorRefProperty = FindFProperty<FWeakObjectProperty>(ActorClass, TEXT("WeakActorRef"));
		FWeakObjectProperty* WeakScriptActorRefProperty = FindFProperty<FWeakObjectProperty>(ActorClass, TEXT("WeakScriptActorRef"));
		FWeakObjectProperty* WeakScriptObjectRefProperty = FindFProperty<FWeakObjectProperty>(ActorClass, TEXT("WeakScriptObjectRef"));
		FSoftObjectProperty* SoftActorRefProperty = FindFProperty<FSoftObjectProperty>(ActorClass, TEXT("SoftActorRef"));
		FSoftObjectProperty* SoftScriptActorRefProperty = FindFProperty<FSoftObjectProperty>(ActorClass, TEXT("SoftScriptActorRef"));
		FSoftObjectProperty* SoftScriptObjectRefProperty = FindFProperty<FSoftObjectProperty>(ActorClass, TEXT("SoftScriptObjectRef"));
		FSoftClassProperty* SoftActorClassRefProperty = FindFProperty<FSoftClassProperty>(ActorClass, TEXT("SoftActorClassRef"));
		FSoftClassProperty* SoftScriptActorClassRefProperty = FindFProperty<FSoftClassProperty>(ActorClass, TEXT("SoftScriptActorClassRef"));

		ASSERT_THAT(IsNotNull(ObjectRefProperty, TEXT("UObject reference member should reflect")));
		ASSERT_THAT(IsNotNull(SmartObjectRefProperty, TEXT("TObjectPtr<UObject> reference member should reflect")));
		ASSERT_THAT(IsNotNull(SmartScriptObjectRefProperty, TEXT("TObjectPtr<script UObject> reference member should reflect")));
		ASSERT_THAT(IsNotNull(InstancedObjectRefProperty, TEXT("Instanced UObject reference member should reflect")));
		ASSERT_THAT(IsNotNull(ScriptObjectRefProperty, TEXT("script UObject reference member should reflect")));
		ASSERT_THAT(IsNotNull(ActorRefProperty, TEXT("AActor reference member should reflect")));
		ASSERT_THAT(IsNotNull(SmartActorRefProperty, TEXT("TObjectPtr<AActor> reference member should reflect")));
		ASSERT_THAT(IsNotNull(ScriptActorRefProperty, TEXT("script actor reference member should reflect")));
		ASSERT_THAT(IsNotNull(ComponentRefProperty, TEXT("UActorComponent reference member should reflect")));
		ASSERT_THAT(IsNotNull(SmartScriptComponentRefProperty, TEXT("TObjectPtr<script component> reference member should reflect")));
		ASSERT_THAT(IsNotNull(ScriptComponentRefProperty, TEXT("script component reference member should reflect")));
		ASSERT_THAT(IsNotNull(ActorClassRefProperty, TEXT("TSubclassOf<AActor> member should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(ScriptActorClassRefProperty, TEXT("TSubclassOf<script actor> member should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(WeakActorRefProperty, TEXT("TWeakObjectPtr<AActor> member should reflect as FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(WeakScriptActorRefProperty, TEXT("TWeakObjectPtr<script actor> member should reflect as FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(WeakScriptObjectRefProperty, TEXT("TWeakObjectPtr<script UObject> member should reflect as FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftActorRefProperty, TEXT("TSoftObjectPtr<AActor> member should reflect as FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftScriptActorRefProperty, TEXT("TSoftObjectPtr<script actor> member should reflect as FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftScriptObjectRefProperty, TEXT("TSoftObjectPtr<script UObject> member should reflect as FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftActorClassRefProperty, TEXT("TSoftClassPtr<AActor> member should reflect as FSoftClassProperty")));
		ASSERT_THAT(IsNotNull(SoftScriptActorClassRefProperty, TEXT("TSoftClassPtr<script actor> member should reflect as FSoftClassProperty")));
		if (ObjectRefProperty == nullptr || SmartObjectRefProperty == nullptr || SmartScriptObjectRefProperty == nullptr || InstancedObjectRefProperty == nullptr
			|| ScriptObjectRefProperty == nullptr || ActorRefProperty == nullptr || SmartActorRefProperty == nullptr || ScriptActorRefProperty == nullptr
			|| ComponentRefProperty == nullptr || SmartScriptComponentRefProperty == nullptr || ScriptComponentRefProperty == nullptr || ActorClassRefProperty == nullptr
			|| ScriptActorClassRefProperty == nullptr || WeakActorRefProperty == nullptr || WeakScriptActorRefProperty == nullptr
			|| WeakScriptObjectRefProperty == nullptr || SoftActorRefProperty == nullptr || SoftScriptActorRefProperty == nullptr
			|| SoftScriptObjectRefProperty == nullptr || SoftActorClassRefProperty == nullptr || SoftScriptActorClassRefProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(UObject::StaticClass(), ObjectRefProperty->PropertyClass, TEXT("UObject member should preserve UObject as its property class")));
		ASSERT_THAT(AreEqual(UObject::StaticClass(), SmartObjectRefProperty->PropertyClass, TEXT("TObjectPtr<UObject> member should preserve UObject as its property class")));
		ASSERT_THAT(AreEqual(ObjectClass, SmartScriptObjectRefProperty->PropertyClass, TEXT("TObjectPtr<script UObject> member should preserve generated object class")));
		ASSERT_THAT(AreEqual(ObjectClass, InstancedObjectRefProperty->PropertyClass, TEXT("Instanced script UObject member should preserve generated object class")));
		ASSERT_THAT(AreEqual(ObjectClass, ScriptObjectRefProperty->PropertyClass, TEXT("script UObject member should preserve generated object class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorRefProperty->PropertyClass, TEXT("AActor member should preserve AActor as its property class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), SmartActorRefProperty->PropertyClass, TEXT("TObjectPtr<AActor> member should preserve AActor as its property class")));
		ASSERT_THAT(AreEqual(ActorClass, ScriptActorRefProperty->PropertyClass, TEXT("script actor member should preserve generated actor class")));
		ASSERT_THAT(AreEqual(UActorComponent::StaticClass(), ComponentRefProperty->PropertyClass, TEXT("component member should preserve UActorComponent as its property class")));
		ASSERT_THAT(AreEqual(ComponentClass, SmartScriptComponentRefProperty->PropertyClass, TEXT("TObjectPtr<script component> member should preserve generated component class")));
		ASSERT_THAT(AreEqual(ComponentClass, ScriptComponentRefProperty->PropertyClass, TEXT("script component member should preserve generated component class")));
		ASSERT_THAT(IsTrue(SmartObjectRefProperty->HasAnyPropertyFlags(CPF_TObjectPtr), TEXT("TObjectPtr<UObject> should preserve CPF_TObjectPtr")));
		ASSERT_THAT(IsTrue(SmartScriptObjectRefProperty->HasAnyPropertyFlags(CPF_TObjectPtr), TEXT("TObjectPtr<script UObject> should preserve CPF_TObjectPtr")));
		ASSERT_THAT(IsTrue(SmartActorRefProperty->HasAnyPropertyFlags(CPF_TObjectPtr), TEXT("TObjectPtr<AActor> should preserve CPF_TObjectPtr")));
		ASSERT_THAT(IsTrue(SmartScriptComponentRefProperty->HasAnyPropertyFlags(CPF_TObjectPtr), TEXT("TObjectPtr<script component> should preserve CPF_TObjectPtr")));
		ASSERT_THAT(IsTrue(InstancedObjectRefProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject | CPF_PersistentInstance), TEXT("Instanced UObject member should be an exported persistent instance")));
		ASSERT_THAT(IsTrue(ActorClassRefProperty->MetaClass != nullptr && ActorClassRefProperty->MetaClass->IsChildOf(AActor::StaticClass()), TEXT("TSubclassOf<AActor> should constrain to actors")));
		ASSERT_THAT(AreEqual(ActorClass, ScriptActorClassRefProperty->MetaClass, TEXT("TSubclassOf<script actor> should constrain to the generated actor class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), WeakActorRefProperty->PropertyClass, TEXT("TWeakObjectPtr<AActor> should preserve AActor as its property class")));
		ASSERT_THAT(AreEqual(ActorClass, WeakScriptActorRefProperty->PropertyClass, TEXT("TWeakObjectPtr<script actor> should preserve generated actor class")));
		ASSERT_THAT(AreEqual(ObjectClass, WeakScriptObjectRefProperty->PropertyClass, TEXT("TWeakObjectPtr<script UObject> should preserve generated object class")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), SoftActorRefProperty->PropertyClass, TEXT("TSoftObjectPtr<AActor> should preserve AActor as its property class")));
		ASSERT_THAT(AreEqual(ActorClass, SoftScriptActorRefProperty->PropertyClass, TEXT("TSoftObjectPtr<script actor> should preserve generated actor class")));
		ASSERT_THAT(AreEqual(ObjectClass, SoftScriptObjectRefProperty->PropertyClass, TEXT("TSoftObjectPtr<script UObject> should preserve generated object class")));
		ASSERT_THAT(IsTrue(SoftActorClassRefProperty->MetaClass != nullptr && SoftActorClassRefProperty->MetaClass->IsChildOf(AActor::StaticClass()), TEXT("TSoftClassPtr<AActor> should constrain to actors")));
		ASSERT_THAT(AreEqual(ActorClass, SoftScriptActorClassRefProperty->MetaClass, TEXT("TSoftClassPtr<script actor> should constrain to the generated actor class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("reference member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		UObject* ObjectRef = nullptr;
		UObject* SmartObjectRef = nullptr;
		UObject* SmartScriptObjectRef = nullptr;
		UObject* InstancedObjectRef = nullptr;
		UObject* ScriptObjectRef = nullptr;
		UObject* ActorRef = nullptr;
		UObject* SmartActorRef = nullptr;
		UObject* ScriptActorRef = nullptr;
		UObject* ComponentRef = nullptr;
		UObject* SmartScriptComponentRef = nullptr;
		UObject* ScriptComponentRef = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ObjectRef"), ObjectRef), TEXT("UObject member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("SmartObjectRef"), SmartObjectRef), TEXT("TObjectPtr<UObject> member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("SmartScriptObjectRef"), SmartScriptObjectRef), TEXT("TObjectPtr<script UObject> member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("InstancedObjectRef"), InstancedObjectRef), TEXT("Instanced UObject member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ScriptObjectRef"), ScriptObjectRef), TEXT("script UObject member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ActorRef"), ActorRef), TEXT("AActor member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("SmartActorRef"), SmartActorRef), TEXT("TObjectPtr<AActor> member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ScriptActorRef"), ScriptActorRef), TEXT("script actor member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ComponentRef"), ComponentRef), TEXT("component member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("SmartScriptComponentRef"), SmartScriptComponentRef), TEXT("TObjectPtr<script component> member should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ScriptComponentRef"), ScriptComponentRef), TEXT("script component member should be readable")));
		ASSERT_THAT(IsNotNull(ObjectRef, TEXT("UObject member should store a script object")));
		ASSERT_THAT(AreEqual(ObjectRef, ScriptObjectRef, TEXT("base UObject and script UObject members should point at the same object")));
		ASSERT_THAT(AreEqual(ObjectRef, SmartObjectRef, TEXT("TObjectPtr<UObject> and UObject members should point at the same object")));
		ASSERT_THAT(AreEqual(ObjectRef, SmartScriptObjectRef, TEXT("TObjectPtr<script UObject> and UObject members should point at the same object")));
		ASSERT_THAT(IsNotNull(InstancedObjectRef, TEXT("Instanced UObject member should store a script object")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), ActorRef, TEXT("AActor member should store this actor")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), SmartActorRef, TEXT("TObjectPtr<AActor> member should store this actor")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), ScriptActorRef, TEXT("script actor member should store this actor")));
		ASSERT_THAT(IsNotNull(ComponentRef, TEXT("component member should store a script component")));
		ASSERT_THAT(AreEqual(ComponentRef, SmartScriptComponentRef, TEXT("TObjectPtr<script component> and component members should point at the same component")));
		ASSERT_THAT(AreEqual(ComponentRef, ScriptComponentRef, TEXT("base component and script component members should point at the same component")));

		UClass* ActorClassRef = nullptr;
		UClass* ScriptActorClassRef = nullptr;
		ASSERT_THAT(IsTrue(GetClassByPath(*TestRunner, Actor, TEXT("ActorClassRef"), ActorClassRef), TEXT("TSubclassOf<AActor> member should be readable")));
		ASSERT_THAT(IsTrue(GetClassByPath(*TestRunner, Actor, TEXT("ScriptActorClassRef"), ScriptActorClassRef), TEXT("TSubclassOf<script actor> member should be readable")));
		ASSERT_THAT(AreEqual(ActorClass, ActorClassRef, TEXT("TSubclassOf<AActor> member should store the generated actor class")));
		ASSERT_THAT(AreEqual(ActorClass, ScriptActorClassRef, TEXT("TSubclassOf<script actor> member should store the generated actor class")));

		UObject* WeakActorRef = nullptr;
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("WeakActorRef"), WeakActorRef), TEXT("TWeakObjectPtr member should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), WeakActorRef, TEXT("TWeakObjectPtr member should resolve to this actor")));
		UObject* WeakScriptActorRef = nullptr;
		UObject* WeakScriptObjectRef = nullptr;
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("WeakScriptActorRef"), WeakScriptActorRef), TEXT("TWeakObjectPtr<script actor> member should be readable")));
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("WeakScriptObjectRef"), WeakScriptObjectRef), TEXT("TWeakObjectPtr<script UObject> member should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), WeakScriptActorRef, TEXT("TWeakObjectPtr<script actor> member should resolve to this actor")));
		ASSERT_THAT(AreEqual(ObjectRef, WeakScriptObjectRef, TEXT("TWeakObjectPtr<script UObject> member should resolve to the script object")));

		FSoftObjectPath SoftActorPath;
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("SoftActorRef"), SoftActorPath), TEXT("TSoftObjectPtr member should expose a path")));
		ASSERT_THAT(IsFalse(SoftActorPath.IsNull(), TEXT("TSoftObjectPtr member should not be null after assignment")));
		FSoftObjectPath SoftScriptActorPath;
		FSoftObjectPath SoftScriptObjectPath;
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("SoftScriptActorRef"), SoftScriptActorPath), TEXT("TSoftObjectPtr<script actor> member should expose a path")));
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("SoftScriptObjectRef"), SoftScriptObjectPath), TEXT("TSoftObjectPtr<script UObject> member should expose a path")));
		ASSERT_THAT(IsFalse(SoftScriptActorPath.IsNull(), TEXT("TSoftObjectPtr<script actor> member should not be null after assignment")));
		ASSERT_THAT(IsFalse(SoftScriptObjectPath.IsNull(), TEXT("TSoftObjectPtr<script UObject> member should not be null after assignment")));

		FSoftObjectPath SoftActorClassPath;
		ASSERT_THAT(IsTrue(GetSoftClassPathByPath(*TestRunner, Actor, TEXT("SoftActorClassRef"), SoftActorClassPath), TEXT("TSoftClassPtr member should expose a path")));
		ASSERT_THAT(IsFalse(SoftActorClassPath.IsNull(), TEXT("TSoftClassPtr member should not be null after assignment")));
		FSoftObjectPath SoftScriptActorClassPath;
		ASSERT_THAT(IsTrue(GetSoftClassPathByPath(*TestRunner, Actor, TEXT("SoftScriptActorClassRef"), SoftScriptActorClassPath), TEXT("TSoftClassPtr<script actor> member should expose a path")));
		ASSERT_THAT(IsFalse(SoftScriptActorClassPath.IsNull(), TEXT("TSoftClassPtr<script actor> member should not be null after assignment")));
	}

	TEST_METHOD(UClassReferenceContainerMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_ReferenceContainerMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPropertyReferenceContainerObject : UObject
			{
				UPROPERTY()
				int ObjectValue = 0;
			}

			UCLASS()
			class ACoverageUClassPropertyReferenceContainerActor : AActor
			{
				UPROPERTY()
				TArray<AActor> ActorArray;

				UPROPERTY()
				TArray<UCoverageUClassPropertyReferenceContainerObject> ObjectArray;

				UPROPERTY(Instanced)
				TArray<UCoverageUClassPropertyReferenceContainerObject> InstancedObjectArray;

				UPROPERTY()
				TArray<TSubclassOf<AActor>> ActorClassArray;

				UPROPERTY()
				TArray<TWeakObjectPtr<AActor>> WeakActorArray;

				UPROPERTY()
				TArray<TWeakObjectPtr<ACoverageUClassPropertyReferenceContainerActor>> WeakScriptActorArray;

				UPROPERTY()
				TArray<TWeakObjectPtr<UCoverageUClassPropertyReferenceContainerObject>> WeakScriptObjectArray;

				UPROPERTY()
				TArray<TSoftObjectPtr<AActor>> SoftActorArray;

				UPROPERTY()
				TArray<TSoftObjectPtr<ACoverageUClassPropertyReferenceContainerActor>> SoftScriptActorArray;

				UPROPERTY()
				TArray<TSoftObjectPtr<UCoverageUClassPropertyReferenceContainerObject>> SoftScriptObjectArray;

				UPROPERTY()
				TArray<TSoftClassPtr<AActor>> SoftActorClassArray;

				UPROPERTY()
				TArray<TSoftClassPtr<ACoverageUClassPropertyReferenceContainerActor>> SoftScriptActorClassArray;

				UPROPERTY()
				TSet<AActor> ActorSet;

				UPROPERTY()
				TSet<TSubclassOf<AActor>> ActorClassSet;

				UPROPERTY()
				TMap<FName, AActor> NameToActorMap;

				UPROPERTY()
				TMap<FName, UCoverageUClassPropertyReferenceContainerObject> NameToObjectMap;

				UPROPERTY(Instanced)
				TMap<FName, UCoverageUClassPropertyReferenceContainerObject> NameToInstancedObjectMap;

				UPROPERTY()
				TMap<FName, TSubclassOf<AActor>> NameToClassMap;

				UPROPERTY()
				TMap<int, TWeakObjectPtr<AActor>> IntToWeakActorMap;

				UPROPERTY()
				TMap<FName, TWeakObjectPtr<ACoverageUClassPropertyReferenceContainerActor>> NameToWeakScriptActorMap;

				UPROPERTY()
				TMap<FName, TWeakObjectPtr<UCoverageUClassPropertyReferenceContainerObject>> NameToWeakScriptObjectMap;

				UPROPERTY()
				TMap<int, TSoftObjectPtr<AActor>> IntToSoftActorMap;

				UPROPERTY()
				TMap<FName, TSoftObjectPtr<ACoverageUClassPropertyReferenceContainerActor>> NameToSoftScriptActorMap;

				UPROPERTY()
				TMap<FName, TSoftObjectPtr<UCoverageUClassPropertyReferenceContainerObject>> NameToSoftScriptObjectMap;

				UPROPERTY()
				TMap<FName, TSoftClassPtr<ACoverageUClassPropertyReferenceContainerActor>> NameToSoftScriptClassMap;

				UPROPERTY()
				int ObjectArrayValueSum = 0;

				UPROPERTY()
				int InstancedObjectArrayValueSum = 0;

				UPROPERTY()
				bool bActorSetDeduplicated = false;

				UPROPERTY()
				bool bActorClassSetContainsScriptClass = false;

				UPROPERTY()
				bool bNameToActorFoundSelf = false;

				UPROPERTY()
				bool bNameToObjectFound = false;

				UPROPERTY()
				bool bNameToInstancedObjectFound = false;

				UPROPERTY()
				bool bNameToClassFound = false;

				UPROPERTY()
				bool bNameToWeakFound = false;

				UPROPERTY()
				bool bNameToWeakScriptFound = false;

				UPROPERTY()
				bool bNameToSoftFound = false;

				UPROPERTY()
				bool bNameToSoftScriptFound = false;

				UPROPERTY()
				bool bNameToSoftScriptClassFound = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UCoverageUClassPropertyReferenceContainerObject MakeObject(FName ObjectName, int Value)
				{
					UCoverageUClassPropertyReferenceContainerObject ObjectValue =
						Cast<UCoverageUClassPropertyReferenceContainerObject>(
							NewObject(this, UCoverageUClassPropertyReferenceContainerObject::StaticClass(), ObjectName));
					ObjectValue.ObjectValue = Value;
					return ObjectValue;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					AActor OtherActor = SpawnActor(AActor::StaticClass());
					UCoverageUClassPropertyReferenceContainerObject FirstObject = MakeObject(n"ReferenceContainerFirstObject", 19);
					UCoverageUClassPropertyReferenceContainerObject SecondObject = MakeObject(n"ReferenceContainerSecondObject", 23);
					UCoverageUClassPropertyReferenceContainerObject FirstInstancedObject = MakeObject(n"ReferenceContainerFirstInstancedObject", 41);
					UCoverageUClassPropertyReferenceContainerObject SecondInstancedObject = MakeObject(n"ReferenceContainerSecondInstancedObject", 43);

					ActorArray.Add(this);
					ActorArray.Add(OtherActor);
					ObjectArray.Add(FirstObject);
					ObjectArray.Add(SecondObject);
					InstancedObjectArray.Add(FirstInstancedObject);
					InstancedObjectArray.Add(SecondInstancedObject);
					ActorClassArray.Add(AActor::StaticClass());
					ActorClassArray.Add(ACoverageUClassPropertyReferenceContainerActor::StaticClass());
					WeakActorArray.Add(this);
					WeakActorArray.Add(OtherActor);
					WeakScriptActorArray.Add(this);
					WeakScriptObjectArray.Add(FirstObject);
					SoftActorArray.Add(this);
					SoftActorArray.Add(OtherActor);
					SoftScriptActorArray.Add(this);
					SoftScriptObjectArray.Add(FirstObject);
					SoftActorClassArray.Add(AActor::StaticClass());
					SoftActorClassArray.Add(ACoverageUClassPropertyReferenceContainerActor::StaticClass());
					SoftScriptActorClassArray.Add(ACoverageUClassPropertyReferenceContainerActor::StaticClass());

					ActorSet.Add(this);
					ActorSet.Add(this);
					ActorSet.Add(OtherActor);
					bActorSetDeduplicated = ActorSet.Num() == 2 && ActorSet.Contains(this) && ActorSet.Contains(OtherActor);
					ActorClassSet.Add(AActor::StaticClass());
					ActorClassSet.Add(ACoverageUClassPropertyReferenceContainerActor::StaticClass());
					bActorClassSetContainsScriptClass = ActorClassSet.Contains(ACoverageUClassPropertyReferenceContainerActor::StaticClass());

					NameToActorMap.Add(n"Self", this);
					NameToActorMap.Add(n"Other", OtherActor);
					NameToObjectMap.Add(n"First", FirstObject);
					NameToObjectMap.Add(n"Second", SecondObject);
					NameToInstancedObjectMap.Add(n"First", FirstInstancedObject);
					NameToInstancedObjectMap.Add(n"Second", SecondInstancedObject);
					NameToClassMap.Add(n"NativeActor", AActor::StaticClass());
					NameToClassMap.Add(n"ScriptActor", ACoverageUClassPropertyReferenceContainerActor::StaticClass());
					IntToWeakActorMap.Add(1, this);
					IntToWeakActorMap.Add(2, OtherActor);
					NameToWeakScriptActorMap.Add(n"Self", this);
					NameToWeakScriptObjectMap.Add(n"Object", FirstObject);
					IntToSoftActorMap.Add(1, this);
					IntToSoftActorMap.Add(2, OtherActor);
					NameToSoftScriptActorMap.Add(n"Self", this);
					NameToSoftScriptObjectMap.Add(n"Object", FirstObject);
					NameToSoftScriptClassMap.Add(n"ScriptActor", ACoverageUClassPropertyReferenceContainerActor::StaticClass());

					ObjectArrayValueSum = ObjectArray[0].ObjectValue + ObjectArray[1].ObjectValue;
					InstancedObjectArrayValueSum = InstancedObjectArray[0].ObjectValue + InstancedObjectArray[1].ObjectValue;

					AActor FoundActor;
					UCoverageUClassPropertyReferenceContainerObject FoundObject;
					UCoverageUClassPropertyReferenceContainerObject FoundInstancedObject;
					TSubclassOf<AActor> FoundClass;
					TWeakObjectPtr<AActor> FoundWeakActor;
					TWeakObjectPtr<ACoverageUClassPropertyReferenceContainerActor> FoundWeakScriptActor;
					TWeakObjectPtr<UCoverageUClassPropertyReferenceContainerObject> FoundWeakScriptObject;
					TSoftObjectPtr<AActor> FoundSoftActor;
					TSoftObjectPtr<ACoverageUClassPropertyReferenceContainerActor> FoundSoftScriptActor;
					TSoftObjectPtr<UCoverageUClassPropertyReferenceContainerObject> FoundSoftScriptObject;
					TSoftClassPtr<ACoverageUClassPropertyReferenceContainerActor> FoundSoftScriptClass;
					bNameToActorFoundSelf = NameToActorMap.Find(n"Self", FoundActor) && FoundActor == this;
					bNameToObjectFound = NameToObjectMap.Find(n"Second", FoundObject) && FoundObject.ObjectValue == 23;
					bNameToInstancedObjectFound = NameToInstancedObjectMap.Find(n"Second", FoundInstancedObject) && FoundInstancedObject.ObjectValue == 43;
					bNameToClassFound = NameToClassMap.Find(n"ScriptActor", FoundClass) && FoundClass == ACoverageUClassPropertyReferenceContainerActor::StaticClass();
					bNameToWeakFound = IntToWeakActorMap.Find(2, FoundWeakActor) && FoundWeakActor.IsValid();
					bNameToWeakScriptFound =
						NameToWeakScriptActorMap.Find(n"Self", FoundWeakScriptActor) && FoundWeakScriptActor.IsValid()
						&& NameToWeakScriptObjectMap.Find(n"Object", FoundWeakScriptObject) && FoundWeakScriptObject.IsValid();
					bNameToSoftFound = IntToSoftActorMap.Find(2, FoundSoftActor) && FoundSoftActor.IsValid();
					bNameToSoftScriptFound =
						NameToSoftScriptActorMap.Find(n"Self", FoundSoftScriptActor) && FoundSoftScriptActor.IsValid()
						&& NameToSoftScriptObjectMap.Find(n"Object", FoundSoftScriptObject) && FoundSoftScriptObject.IsValid();
					bNameToSoftScriptClassFound =
						NameToSoftScriptClassMap.Find(n"ScriptActor", FoundSoftScriptClass)
						&& FoundSoftScriptClass == ACoverageUClassPropertyReferenceContainerActor::StaticClass();
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyReferenceContainerMemberMatrix.as"), ScriptSource)));

		UClass* ObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPropertyReferenceContainerObject"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPropertyReferenceContainerActor"));
		ASSERT_THAT(IsNotNull(ObjectClass, TEXT("reference container object class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("reference container actor class should be generated")));
		if (ObjectClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		FArrayProperty* ActorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("ActorArray"));
		FArrayProperty* ObjectArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("ObjectArray"));
		FArrayProperty* InstancedObjectArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("InstancedObjectArray"));
		FArrayProperty* ActorClassArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("ActorClassArray"));
		FArrayProperty* WeakActorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("WeakActorArray"));
		FArrayProperty* WeakScriptActorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("WeakScriptActorArray"));
		FArrayProperty* WeakScriptObjectArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("WeakScriptObjectArray"));
		FArrayProperty* SoftActorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("SoftActorArray"));
		FArrayProperty* SoftScriptActorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("SoftScriptActorArray"));
		FArrayProperty* SoftScriptObjectArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("SoftScriptObjectArray"));
		FArrayProperty* SoftActorClassArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("SoftActorClassArray"));
		FArrayProperty* SoftScriptActorClassArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("SoftScriptActorClassArray"));
		FSetProperty* ActorSetProperty = FindFProperty<FSetProperty>(ActorClass, TEXT("ActorSet"));
		FSetProperty* ActorClassSetProperty = FindFProperty<FSetProperty>(ActorClass, TEXT("ActorClassSet"));
		FMapProperty* NameToActorMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToActorMap"));
		FMapProperty* NameToObjectMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToObjectMap"));
		FMapProperty* NameToInstancedObjectMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToInstancedObjectMap"));
		FMapProperty* NameToClassMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToClassMap"));
		FMapProperty* IntToWeakActorMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("IntToWeakActorMap"));
		FMapProperty* NameToWeakScriptActorMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToWeakScriptActorMap"));
		FMapProperty* NameToWeakScriptObjectMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToWeakScriptObjectMap"));
		FMapProperty* IntToSoftActorMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("IntToSoftActorMap"));
		FMapProperty* NameToSoftScriptActorMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToSoftScriptActorMap"));
		FMapProperty* NameToSoftScriptObjectMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToSoftScriptObjectMap"));
		FMapProperty* NameToSoftScriptClassMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToSoftScriptClassMap"));
		ASSERT_THAT(IsNotNull(ActorArrayProperty, TEXT("TArray<AActor> member should reflect")));
		ASSERT_THAT(IsNotNull(ObjectArrayProperty, TEXT("TArray<script UObject> member should reflect")));
		ASSERT_THAT(IsNotNull(InstancedObjectArrayProperty, TEXT("Instanced TArray<script UObject> member should reflect")));
		ASSERT_THAT(IsNotNull(ActorClassArrayProperty, TEXT("TArray<TSubclassOf<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(WeakActorArrayProperty, TEXT("TArray<TWeakObjectPtr<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(WeakScriptActorArrayProperty, TEXT("TArray<TWeakObjectPtr<script actor>> member should reflect")));
		ASSERT_THAT(IsNotNull(WeakScriptObjectArrayProperty, TEXT("TArray<TWeakObjectPtr<script UObject>> member should reflect")));
		ASSERT_THAT(IsNotNull(SoftActorArrayProperty, TEXT("TArray<TSoftObjectPtr<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(SoftScriptActorArrayProperty, TEXT("TArray<TSoftObjectPtr<script actor>> member should reflect")));
		ASSERT_THAT(IsNotNull(SoftScriptObjectArrayProperty, TEXT("TArray<TSoftObjectPtr<script UObject>> member should reflect")));
		ASSERT_THAT(IsNotNull(SoftActorClassArrayProperty, TEXT("TArray<TSoftClassPtr<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(SoftScriptActorClassArrayProperty, TEXT("TArray<TSoftClassPtr<script actor>> member should reflect")));
		ASSERT_THAT(IsNotNull(ActorSetProperty, TEXT("TSet<AActor> member should reflect")));
		ASSERT_THAT(IsNotNull(ActorClassSetProperty, TEXT("TSet<TSubclassOf<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToActorMapProperty, TEXT("TMap<FName, AActor> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToObjectMapProperty, TEXT("TMap<FName, script UObject> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToInstancedObjectMapProperty, TEXT("Instanced TMap<FName, script UObject> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToClassMapProperty, TEXT("TMap<FName, TSubclassOf<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(IntToWeakActorMapProperty, TEXT("TMap<int, TWeakObjectPtr<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToWeakScriptActorMapProperty, TEXT("TMap<FName, TWeakObjectPtr<script actor>> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToWeakScriptObjectMapProperty, TEXT("TMap<FName, TWeakObjectPtr<script UObject>> member should reflect")));
		ASSERT_THAT(IsNotNull(IntToSoftActorMapProperty, TEXT("TMap<int, TSoftObjectPtr<AActor>> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToSoftScriptActorMapProperty, TEXT("TMap<FName, TSoftObjectPtr<script actor>> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToSoftScriptObjectMapProperty, TEXT("TMap<FName, TSoftObjectPtr<script UObject>> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToSoftScriptClassMapProperty, TEXT("TMap<FName, TSoftClassPtr<script actor>> member should reflect")));
		if (ActorArrayProperty == nullptr || ObjectArrayProperty == nullptr || InstancedObjectArrayProperty == nullptr || ActorClassArrayProperty == nullptr
			|| WeakActorArrayProperty == nullptr || WeakScriptActorArrayProperty == nullptr || WeakScriptObjectArrayProperty == nullptr || SoftActorArrayProperty == nullptr
			|| SoftScriptActorArrayProperty == nullptr || SoftScriptObjectArrayProperty == nullptr || SoftActorClassArrayProperty == nullptr || SoftScriptActorClassArrayProperty == nullptr
			|| ActorSetProperty == nullptr || ActorClassSetProperty == nullptr || NameToActorMapProperty == nullptr || NameToObjectMapProperty == nullptr
			|| NameToInstancedObjectMapProperty == nullptr || NameToClassMapProperty == nullptr || IntToWeakActorMapProperty == nullptr
			|| NameToWeakScriptActorMapProperty == nullptr || NameToWeakScriptObjectMapProperty == nullptr || IntToSoftActorMapProperty == nullptr
			|| NameToSoftScriptActorMapProperty == nullptr || NameToSoftScriptObjectMapProperty == nullptr || NameToSoftScriptClassMapProperty == nullptr)
		{
			return;
		}

		const FObjectPropertyBase* ActorArrayInnerProperty = CastField<FObjectPropertyBase>(ActorArrayProperty->Inner);
		const FObjectPropertyBase* ObjectArrayInnerProperty = CastField<FObjectPropertyBase>(ObjectArrayProperty->Inner);
		const FObjectPropertyBase* InstancedObjectArrayInnerProperty = CastField<FObjectPropertyBase>(InstancedObjectArrayProperty->Inner);
		const FClassProperty* ActorClassArrayInnerProperty = CastField<FClassProperty>(ActorClassArrayProperty->Inner);
		const FObjectPropertyBase* NameToInstancedObjectMapValueProperty = CastField<FObjectPropertyBase>(NameToInstancedObjectMapProperty->ValueProp);
		const FWeakObjectProperty* WeakScriptActorArrayInnerProperty = CastField<FWeakObjectProperty>(WeakScriptActorArrayProperty->Inner);
		const FWeakObjectProperty* WeakScriptObjectArrayInnerProperty = CastField<FWeakObjectProperty>(WeakScriptObjectArrayProperty->Inner);
		const FSoftObjectProperty* SoftScriptActorArrayInnerProperty = CastField<FSoftObjectProperty>(SoftScriptActorArrayProperty->Inner);
		const FSoftObjectProperty* SoftScriptObjectArrayInnerProperty = CastField<FSoftObjectProperty>(SoftScriptObjectArrayProperty->Inner);
		const FSoftClassProperty* SoftScriptActorClassArrayInnerProperty = CastField<FSoftClassProperty>(SoftScriptActorClassArrayProperty->Inner);
		const FWeakObjectProperty* NameToWeakScriptActorMapValueProperty = CastField<FWeakObjectProperty>(NameToWeakScriptActorMapProperty->ValueProp);
		const FWeakObjectProperty* NameToWeakScriptObjectMapValueProperty = CastField<FWeakObjectProperty>(NameToWeakScriptObjectMapProperty->ValueProp);
		const FSoftObjectProperty* NameToSoftScriptActorMapValueProperty = CastField<FSoftObjectProperty>(NameToSoftScriptActorMapProperty->ValueProp);
		const FSoftObjectProperty* NameToSoftScriptObjectMapValueProperty = CastField<FSoftObjectProperty>(NameToSoftScriptObjectMapProperty->ValueProp);
		const FSoftClassProperty* NameToSoftScriptClassMapValueProperty = CastField<FSoftClassProperty>(NameToSoftScriptClassMapProperty->ValueProp);
		ASSERT_THAT(IsNotNull(ActorArrayInnerProperty, TEXT("TArray<AActor> inner should be object-backed")));
		ASSERT_THAT(IsNotNull(ObjectArrayInnerProperty, TEXT("TArray<script UObject> inner should be object-backed")));
		ASSERT_THAT(IsNotNull(InstancedObjectArrayInnerProperty, TEXT("Instanced TArray<script UObject> inner should be object-backed")));
		ASSERT_THAT(IsNotNull(ActorClassArrayInnerProperty, TEXT("TArray<TSubclassOf<AActor>> inner should be class-backed")));
		ASSERT_THAT(IsNotNull(CastField<FWeakObjectProperty>(WeakActorArrayProperty->Inner), TEXT("TArray<TWeakObjectPtr<AActor>> inner should be FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(WeakScriptActorArrayInnerProperty, TEXT("TArray<TWeakObjectPtr<script actor>> inner should be FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(WeakScriptObjectArrayInnerProperty, TEXT("TArray<TWeakObjectPtr<script UObject>> inner should be FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FSoftObjectProperty>(SoftActorArrayProperty->Inner), TEXT("TArray<TSoftObjectPtr<AActor>> inner should be FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftScriptActorArrayInnerProperty, TEXT("TArray<TSoftObjectPtr<script actor>> inner should be FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(SoftScriptObjectArrayInnerProperty, TEXT("TArray<TSoftObjectPtr<script UObject>> inner should be FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FSoftClassProperty>(SoftActorClassArrayProperty->Inner), TEXT("TArray<TSoftClassPtr<AActor>> inner should be FSoftClassProperty")));
		ASSERT_THAT(IsNotNull(SoftScriptActorClassArrayInnerProperty, TEXT("TArray<TSoftClassPtr<script actor>> inner should be FSoftClassProperty")));
		ASSERT_THAT(IsNotNull(CastField<FObjectPropertyBase>(ActorSetProperty->ElementProp), TEXT("TSet<AActor> element should be object-backed")));
		ASSERT_THAT(IsNotNull(CastField<FClassProperty>(ActorClassSetProperty->ElementProp), TEXT("TSet<TSubclassOf<AActor>> element should be FClassProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToActorMapProperty->KeyProp), TEXT("TMap<FName, AActor> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FObjectPropertyBase>(NameToActorMapProperty->ValueProp), TEXT("TMap<FName, AActor> value should be object-backed")));
		ASSERT_THAT(IsNotNull(CastField<FObjectPropertyBase>(NameToObjectMapProperty->ValueProp), TEXT("TMap<FName, script UObject> value should be object-backed")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToInstancedObjectMapProperty->KeyProp), TEXT("Instanced TMap<FName, script UObject> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(NameToInstancedObjectMapValueProperty, TEXT("Instanced TMap<FName, script UObject> value should be object-backed")));
		ASSERT_THAT(IsNotNull(CastField<FClassProperty>(NameToClassMapProperty->ValueProp), TEXT("TMap<FName, TSubclassOf<AActor>> value should be FClassProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(IntToWeakActorMapProperty->KeyProp), TEXT("TMap<int, TWeakObjectPtr<AActor>> key should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FWeakObjectProperty>(IntToWeakActorMapProperty->ValueProp), TEXT("TMap<int, TWeakObjectPtr<AActor>> value should be FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToWeakScriptActorMapProperty->KeyProp), TEXT("TMap<FName, TWeakObjectPtr<script actor>> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(NameToWeakScriptActorMapValueProperty, TEXT("TMap<FName, TWeakObjectPtr<script actor>> value should be FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToWeakScriptObjectMapProperty->KeyProp), TEXT("TMap<FName, TWeakObjectPtr<script UObject>> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(NameToWeakScriptObjectMapValueProperty, TEXT("TMap<FName, TWeakObjectPtr<script UObject>> value should be FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(IntToSoftActorMapProperty->KeyProp), TEXT("TMap<int, TSoftObjectPtr<AActor>> key should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FSoftObjectProperty>(IntToSoftActorMapProperty->ValueProp), TEXT("TMap<int, TSoftObjectPtr<AActor>> value should be FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToSoftScriptActorMapProperty->KeyProp), TEXT("TMap<FName, TSoftObjectPtr<script actor>> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(NameToSoftScriptActorMapValueProperty, TEXT("TMap<FName, TSoftObjectPtr<script actor>> value should be FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToSoftScriptObjectMapProperty->KeyProp), TEXT("TMap<FName, TSoftObjectPtr<script UObject>> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(NameToSoftScriptObjectMapValueProperty, TEXT("TMap<FName, TSoftObjectPtr<script UObject>> value should be FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToSoftScriptClassMapProperty->KeyProp), TEXT("TMap<FName, TSoftClassPtr<script actor>> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(NameToSoftScriptClassMapValueProperty, TEXT("TMap<FName, TSoftClassPtr<script actor>> value should be FSoftClassProperty")));
		if (ActorArrayInnerProperty == nullptr || ObjectArrayInnerProperty == nullptr || InstancedObjectArrayInnerProperty == nullptr
			|| ActorClassArrayInnerProperty == nullptr || NameToInstancedObjectMapValueProperty == nullptr || WeakScriptActorArrayInnerProperty == nullptr
			|| WeakScriptObjectArrayInnerProperty == nullptr || SoftScriptActorArrayInnerProperty == nullptr || SoftScriptObjectArrayInnerProperty == nullptr
			|| SoftScriptActorClassArrayInnerProperty == nullptr || NameToWeakScriptActorMapValueProperty == nullptr || NameToWeakScriptObjectMapValueProperty == nullptr
			|| NameToSoftScriptActorMapValueProperty == nullptr || NameToSoftScriptObjectMapValueProperty == nullptr || NameToSoftScriptClassMapValueProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(AActor::StaticClass(), ActorArrayInnerProperty->PropertyClass, TEXT("TArray<AActor> should preserve AActor element class")));
		ASSERT_THAT(AreEqual(ObjectClass, ObjectArrayInnerProperty->PropertyClass, TEXT("TArray<script UObject> should preserve script object element class")));
		ASSERT_THAT(AreEqual(ObjectClass, InstancedObjectArrayInnerProperty->PropertyClass, TEXT("Instanced TArray<script UObject> should preserve script object element class")));
		ASSERT_THAT(AreEqual(ObjectClass, NameToInstancedObjectMapValueProperty->PropertyClass, TEXT("Instanced TMap<FName, script UObject> should preserve script object value class")));
		ASSERT_THAT(IsTrue(ActorClassArrayInnerProperty->MetaClass != nullptr && ActorClassArrayInnerProperty->MetaClass->IsChildOf(AActor::StaticClass()), TEXT("TArray<TSubclassOf<AActor>> should constrain class elements to actors")));
		ASSERT_THAT(AreEqual(ActorClass, WeakScriptActorArrayInnerProperty->PropertyClass, TEXT("TArray<TWeakObjectPtr<script actor>> should preserve generated actor class")));
		ASSERT_THAT(AreEqual(ObjectClass, WeakScriptObjectArrayInnerProperty->PropertyClass, TEXT("TArray<TWeakObjectPtr<script UObject>> should preserve generated object class")));
		ASSERT_THAT(AreEqual(ActorClass, SoftScriptActorArrayInnerProperty->PropertyClass, TEXT("TArray<TSoftObjectPtr<script actor>> should preserve generated actor class")));
		ASSERT_THAT(AreEqual(ObjectClass, SoftScriptObjectArrayInnerProperty->PropertyClass, TEXT("TArray<TSoftObjectPtr<script UObject>> should preserve generated object class")));
		ASSERT_THAT(AreEqual(ActorClass, SoftScriptActorClassArrayInnerProperty->MetaClass, TEXT("TArray<TSoftClassPtr<script actor>> should constrain to the generated actor class")));
		ASSERT_THAT(AreEqual(ActorClass, NameToWeakScriptActorMapValueProperty->PropertyClass, TEXT("TMap<FName, TWeakObjectPtr<script actor>> should preserve generated actor class")));
		ASSERT_THAT(AreEqual(ObjectClass, NameToWeakScriptObjectMapValueProperty->PropertyClass, TEXT("TMap<FName, TWeakObjectPtr<script UObject>> should preserve generated object class")));
		ASSERT_THAT(AreEqual(ActorClass, NameToSoftScriptActorMapValueProperty->PropertyClass, TEXT("TMap<FName, TSoftObjectPtr<script actor>> should preserve generated actor class")));
		ASSERT_THAT(AreEqual(ObjectClass, NameToSoftScriptObjectMapValueProperty->PropertyClass, TEXT("TMap<FName, TSoftObjectPtr<script UObject>> should preserve generated object class")));
		ASSERT_THAT(AreEqual(ActorClass, NameToSoftScriptClassMapValueProperty->MetaClass, TEXT("TMap<FName, TSoftClassPtr<script actor>> should constrain to the generated actor class")));
		ASSERT_THAT(IsTrue(ActorClass->HasAnyClassFlags(CLASS_HasInstancedReference), TEXT("Instanced containers should mark the generated class as containing instanced references")));
		ASSERT_THAT(IsTrue(InstancedObjectArrayProperty->HasAnyPropertyFlags(CPF_ContainsInstancedReference), TEXT("Instanced object array should mark the container as containing instanced references")));
		ASSERT_THAT(IsTrue(InstancedObjectArrayInnerProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject | CPF_PersistentInstance), TEXT("Instanced object array inner should be an exported persistent instance")));
		ASSERT_THAT(IsTrue(NameToInstancedObjectMapProperty->HasAnyPropertyFlags(CPF_ContainsInstancedReference), TEXT("Instanced object map should mark the container as containing instanced references")));
		ASSERT_THAT(IsTrue(NameToInstancedObjectMapValueProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject | CPF_PersistentInstance), TEXT("Instanced object map value should be an exported persistent instance")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("reference container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ActorArray"), Count), TEXT("TArray<AActor> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<AActor> should hold two actors")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ObjectArray"), Count), TEXT("TArray<script UObject> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<script UObject> should hold two objects")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("InstancedObjectArray"), Count), TEXT("Instanced TArray<script UObject> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("Instanced TArray<script UObject> should hold two objects")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ActorClassArray"), Count), TEXT("TArray<TSubclassOf<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<TSubclassOf<AActor>> should hold two classes")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("WeakActorArray"), Count), TEXT("TArray<TWeakObjectPtr<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<TWeakObjectPtr<AActor>> should hold two weak refs")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("WeakScriptActorArray"), Count), TEXT("TArray<TWeakObjectPtr<script actor>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TArray<TWeakObjectPtr<script actor>> should hold one weak ref")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("WeakScriptObjectArray"), Count), TEXT("TArray<TWeakObjectPtr<script UObject>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TArray<TWeakObjectPtr<script UObject>> should hold one weak ref")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("SoftActorArray"), Count), TEXT("TArray<TSoftObjectPtr<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<TSoftObjectPtr<AActor>> should hold two soft refs")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("SoftScriptActorArray"), Count), TEXT("TArray<TSoftObjectPtr<script actor>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TArray<TSoftObjectPtr<script actor>> should hold one soft ref")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("SoftScriptObjectArray"), Count), TEXT("TArray<TSoftObjectPtr<script UObject>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TArray<TSoftObjectPtr<script UObject>> should hold one soft ref")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("SoftActorClassArray"), Count), TEXT("TArray<TSoftClassPtr<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<TSoftClassPtr<AActor>> should hold two soft class refs")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("SoftScriptActorClassArray"), Count), TEXT("TArray<TSoftClassPtr<script actor>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TArray<TSoftClassPtr<script actor>> should hold one soft class ref")));

		UObject* ActorArraySelf = nullptr;
		UObject* ActorArrayOther = nullptr;
		UObject* ObjectArrayFirst = nullptr;
		UObject* ObjectArraySecond = nullptr;
		UObject* InstancedObjectArrayFirst = nullptr;
		UObject* InstancedObjectArraySecond = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ActorArray[0]"), ActorArraySelf), TEXT("TArray<AActor>[0] should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ActorArray[1]"), ActorArrayOther), TEXT("TArray<AActor>[1] should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ObjectArray[0]"), ObjectArrayFirst), TEXT("TArray<script UObject>[0] should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ObjectArray[1]"), ObjectArraySecond), TEXT("TArray<script UObject>[1] should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("InstancedObjectArray[0]"), InstancedObjectArrayFirst), TEXT("Instanced TArray<script UObject>[0] should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("InstancedObjectArray[1]"), InstancedObjectArraySecond), TEXT("Instanced TArray<script UObject>[1] should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), ActorArraySelf, TEXT("TArray<AActor>[0] should store this actor")));
		ASSERT_THAT(IsTrue(ActorArrayOther != nullptr && ActorArrayOther->IsA(AActor::StaticClass()), TEXT("TArray<AActor>[1] should store a spawned actor")));
		ASSERT_THAT(IsTrue(ObjectArrayFirst != nullptr && ObjectArrayFirst->IsA(ObjectClass), TEXT("TArray<script UObject>[0] should store a script object")));
		ASSERT_THAT(IsTrue(ObjectArraySecond != nullptr && ObjectArraySecond->IsA(ObjectClass), TEXT("TArray<script UObject>[1] should store a script object")));
		ASSERT_THAT(IsTrue(InstancedObjectArrayFirst != nullptr && InstancedObjectArrayFirst->IsA(ObjectClass), TEXT("Instanced TArray<script UObject>[0] should store a script object")));
		ASSERT_THAT(IsTrue(InstancedObjectArraySecond != nullptr && InstancedObjectArraySecond->IsA(ObjectClass), TEXT("Instanced TArray<script UObject>[1] should store a script object")));
		FIntProperty* ObjectValueProperty = FindFProperty<FIntProperty>(ObjectClass, TEXT("ObjectValue"));
		ASSERT_THAT(IsNotNull(ObjectValueProperty, TEXT("script object array element class should expose ObjectValue")));
		if (ObjectValueProperty == nullptr || ObjectArraySecond == nullptr || InstancedObjectArraySecond == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(23, ObjectValueProperty->GetPropertyValue_InContainer(ObjectArraySecond), TEXT("TArray<script UObject>[1] field should round-trip")));
		ASSERT_THAT(AreEqual(43, ObjectValueProperty->GetPropertyValue_InContainer(InstancedObjectArraySecond), TEXT("Instanced TArray<script UObject>[1] field should round-trip")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectArrayValueSum"), 42, TEXT("script object array runtime access should sum values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InstancedObjectArrayValueSum"), 84, TEXT("instanced script object array runtime access should sum values"))));

		UClass* NativeClassValue = nullptr;
		UClass* ScriptClassValue = nullptr;
		ASSERT_THAT(IsTrue(GetClassByPath(*TestRunner, Actor, TEXT("ActorClassArray[0]"), NativeClassValue), TEXT("TArray<TSubclassOf<AActor>>[0] should be readable")));
		ASSERT_THAT(IsTrue(GetClassByPath(*TestRunner, Actor, TEXT("ActorClassArray[1]"), ScriptClassValue), TEXT("TArray<TSubclassOf<AActor>>[1] should be readable")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), NativeClassValue, TEXT("TArray<TSubclassOf<AActor>>[0] should store native AActor")));
		ASSERT_THAT(AreEqual(ActorClass, ScriptClassValue, TEXT("TArray<TSubclassOf<AActor>>[1] should store generated actor class")));

		UObject* WeakActorSelf = nullptr;
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("WeakActorArray[0]"), WeakActorSelf), TEXT("TArray<TWeakObjectPtr<AActor>>[0] should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), WeakActorSelf, TEXT("TArray<TWeakObjectPtr<AActor>>[0] should resolve to this actor")));
		UObject* WeakScriptActorSelf = nullptr;
		UObject* WeakScriptObject = nullptr;
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("WeakScriptActorArray[0]"), WeakScriptActorSelf), TEXT("TArray<TWeakObjectPtr<script actor>>[0] should be readable")));
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("WeakScriptObjectArray[0]"), WeakScriptObject), TEXT("TArray<TWeakObjectPtr<script UObject>>[0] should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), WeakScriptActorSelf, TEXT("TArray<TWeakObjectPtr<script actor>>[0] should resolve to this actor")));
		ASSERT_THAT(IsTrue(WeakScriptObject != nullptr && WeakScriptObject->IsA(ObjectClass), TEXT("TArray<TWeakObjectPtr<script UObject>>[0] should resolve to a script object")));

		FSoftObjectPath SoftActorPath;
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("SoftActorArray[0]"), SoftActorPath), TEXT("TArray<TSoftObjectPtr<AActor>>[0] should expose a path")));
		ASSERT_THAT(IsFalse(SoftActorPath.IsNull(), TEXT("TArray<TSoftObjectPtr<AActor>>[0] should not be null")));
		FSoftObjectPath SoftScriptActorPath;
		FSoftObjectPath SoftScriptObjectPath;
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("SoftScriptActorArray[0]"), SoftScriptActorPath), TEXT("TArray<TSoftObjectPtr<script actor>>[0] should expose a path")));
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("SoftScriptObjectArray[0]"), SoftScriptObjectPath), TEXT("TArray<TSoftObjectPtr<script UObject>>[0] should expose a path")));
		ASSERT_THAT(IsFalse(SoftScriptActorPath.IsNull(), TEXT("TArray<TSoftObjectPtr<script actor>>[0] should not be null")));
		ASSERT_THAT(IsFalse(SoftScriptObjectPath.IsNull(), TEXT("TArray<TSoftObjectPtr<script UObject>>[0] should not be null")));

		FSoftObjectPath SoftActorClassPath;
		ASSERT_THAT(IsTrue(GetSoftClassPathByPath(*TestRunner, Actor, TEXT("SoftActorClassArray[1]"), SoftActorClassPath), TEXT("TArray<TSoftClassPtr<AActor>>[1] should expose a path")));
		ASSERT_THAT(IsFalse(SoftActorClassPath.IsNull(), TEXT("TArray<TSoftClassPtr<AActor>>[1] should not be null")));
		FSoftObjectPath SoftScriptActorClassPath;
		ASSERT_THAT(IsTrue(GetSoftClassPathByPath(*TestRunner, Actor, TEXT("SoftScriptActorClassArray[0]"), SoftScriptActorClassPath), TEXT("TArray<TSoftClassPtr<script actor>>[0] should expose a path")));
		ASSERT_THAT(IsFalse(SoftScriptActorClassPath.IsNull(), TEXT("TArray<TSoftClassPtr<script actor>>[0] should not be null")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("ActorSet"), Count), TEXT("TSet<AActor> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<AActor> should deduplicate repeated actor refs")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bActorSetDeduplicated"), true, TEXT("TSet<AActor> should deduplicate object refs at runtime"))));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("ActorClassSet"), Count), TEXT("TSet<TSubclassOf<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<TSubclassOf<AActor>> should hold two class refs")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bActorClassSetContainsScriptClass"), true, TEXT("TSet<TSubclassOf<AActor>> should support script class lookup at runtime"))));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToActorMap"), Count), TEXT("TMap<FName, AActor> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName, AActor> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToObjectMap"), Count), TEXT("TMap<FName, script UObject> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName, script UObject> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToInstancedObjectMap"), Count), TEXT("Instanced TMap<FName, script UObject> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("Instanced TMap<FName, script UObject> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToClassMap"), Count), TEXT("TMap<FName, TSubclassOf<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName, TSubclassOf<AActor>> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToWeakActorMap"), Count), TEXT("TMap<int, TWeakObjectPtr<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, TWeakObjectPtr<AActor>> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToWeakScriptActorMap"), Count), TEXT("TMap<FName, TWeakObjectPtr<script actor>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, TWeakObjectPtr<script actor>> should hold one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToWeakScriptObjectMap"), Count), TEXT("TMap<FName, TWeakObjectPtr<script UObject>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, TWeakObjectPtr<script UObject>> should hold one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToSoftActorMap"), Count), TEXT("TMap<int, TSoftObjectPtr<AActor>> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, TSoftObjectPtr<AActor>> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToSoftScriptActorMap"), Count), TEXT("TMap<FName, TSoftObjectPtr<script actor>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, TSoftObjectPtr<script actor>> should hold one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToSoftScriptObjectMap"), Count), TEXT("TMap<FName, TSoftObjectPtr<script UObject>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, TSoftObjectPtr<script UObject>> should hold one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToSoftScriptClassMap"), Count), TEXT("TMap<FName, TSoftClassPtr<script actor>> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, TSoftClassPtr<script actor>> should hold one entry")));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToActorFoundSelf"), true, TEXT("TMap<FName, AActor>.Find should return this actor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToObjectFound"), true, TEXT("TMap<FName, script UObject>.Find should return script object value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToInstancedObjectFound"), true, TEXT("Instanced TMap<FName, script UObject>.Find should return script object value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToClassFound"), true, TEXT("TMap<FName, TSubclassOf<AActor>>.Find should return script class value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToWeakFound"), true, TEXT("TMap<int, TWeakObjectPtr<AActor>>.Find should return a valid weak ref"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToWeakScriptFound"), true, TEXT("script weak object maps should return valid refs"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToSoftFound"), true, TEXT("TMap<int, TSoftObjectPtr<AActor>>.Find should return a valid soft ref"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToSoftScriptFound"), true, TEXT("script soft object maps should return valid refs"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNameToSoftScriptClassFound"), true, TEXT("script soft class map should return a valid class ref"))));
	}

	TEST_METHOD(UClassInterfaceMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		AngelscriptNativeInterfaceTestHelpers::EnsureNativeInterfaceBound(UAngelscriptNativeParentInterface::StaticClass());

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_InterfaceMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassInterfaceMemberActor : AActor, UAngelscriptNativeParentInterface
			{
				UPROPERTY()
				UAngelscriptNativeParentInterface InterfaceRef;

				UPROPERTY()
				UAngelscriptNativeParentInterface ClearedInterfaceRef;

				UPROPERTY()
				TArray<UAngelscriptNativeParentInterface> InterfaceRefs;

				UPROPERTY()
				int NativeValue = 37;

				UPROPERTY()
				int DispatchValue = 0;

				UPROPERTY()
				int AdjustedValue = 0;

				UPROPERTY()
				FName NativeMarker = NAME_None;

				UPROPERTY()
				bool bDefaultNull = false;

				UPROPERTY()
				bool bAssignmentWorked = false;

				UPROPERTY()
				bool bDispatchWorked = false;

				UPROPERTY()
				bool bArrayStoredInterfaces = false;

				UPROPERTY()
				bool bNullResetWorked = false;

				UFUNCTION()
				int GetNativeValue() const
				{
					return NativeValue;
				}

				UFUNCTION()
				void SetNativeMarker(FName Marker)
				{
					NativeMarker = Marker;
				}

				UFUNCTION()
				void AdjustNativeValue(int Delta, int& Value)
				{
					Value += Delta;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					UAngelscriptNativeParentInterface EmptyRef;
					bDefaultNull = EmptyRef == nullptr;

					UObject SelfObject = this;
					InterfaceRef = Cast<UAngelscriptNativeParentInterface>(SelfObject);
					bAssignmentWorked = InterfaceRef != nullptr;

					if (InterfaceRef != nullptr)
					{
						DispatchValue = InterfaceRef.GetNativeValue();
						AdjustedValue = 40;
						InterfaceRef.AdjustNativeValue(2, AdjustedValue);
						InterfaceRef.SetNativeMarker(n"FromUClassPropertyInterface");
						bDispatchWorked = DispatchValue == 37 && AdjustedValue == 42;
					}

					InterfaceRefs.Add(InterfaceRef);
					InterfaceRefs.Add(Cast<UAngelscriptNativeParentInterface>(SelfObject));
					bArrayStoredInterfaces = InterfaceRefs.Num() == 2 &&
						InterfaceRefs[0] != nullptr &&
						InterfaceRefs[1].GetNativeValue() == 37;

					ClearedInterfaceRef = InterfaceRef;
					ClearedInterfaceRef = nullptr;
					bNullResetWorked = ClearedInterfaceRef == nullptr;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyInterfaceMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassInterfaceMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("interface member actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->ImplementsInterface(UAngelscriptNativeParentInterface::StaticClass()), TEXT("interface member actor should implement the native parent interface")));

		FInterfaceProperty* InterfaceRefProperty = FindFProperty<FInterfaceProperty>(ScriptClass, TEXT("InterfaceRef"));
		FInterfaceProperty* ClearedInterfaceRefProperty = FindFProperty<FInterfaceProperty>(ScriptClass, TEXT("ClearedInterfaceRef"));
		FArrayProperty* InterfaceRefsProperty = FindFProperty<FArrayProperty>(ScriptClass, TEXT("InterfaceRefs"));
		ASSERT_THAT(IsNotNull(InterfaceRefProperty, TEXT("native interface member should reflect as FInterfaceProperty")));
		ASSERT_THAT(IsNotNull(ClearedInterfaceRefProperty, TEXT("cleared native interface member should reflect as FInterfaceProperty")));
		ASSERT_THAT(IsNotNull(InterfaceRefsProperty, TEXT("native interface array member should reflect as FArrayProperty")));
		if (InterfaceRefProperty == nullptr || ClearedInterfaceRefProperty == nullptr || InterfaceRefsProperty == nullptr)
		{
			return;
		}

		FInterfaceProperty* InterfaceRefsInnerProperty = CastField<FInterfaceProperty>(InterfaceRefsProperty->Inner);
		ASSERT_THAT(AreEqual(UAngelscriptNativeParentInterface::StaticClass(), InterfaceRefProperty->InterfaceClass, TEXT("native interface member should target UAngelscriptNativeParentInterface")));
		ASSERT_THAT(AreEqual(UAngelscriptNativeParentInterface::StaticClass(), ClearedInterfaceRefProperty->InterfaceClass, TEXT("cleared native interface member should target UAngelscriptNativeParentInterface")));
		ASSERT_THAT(IsNotNull(InterfaceRefsInnerProperty, TEXT("native interface array should store FInterfaceProperty elements")));
		if (InterfaceRefsInnerProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(UAngelscriptNativeParentInterface::StaticClass(), InterfaceRefsInnerProperty->InterfaceClass, TEXT("native interface array inner should target UAngelscriptNativeParentInterface")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("interface member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDefaultNull"), true, TEXT("native interface member should default to null"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bAssignmentWorked"), true, TEXT("native interface member assignment should work"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDispatchWorked"), true, TEXT("native interface member should dispatch calls"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DispatchValue"), 37, TEXT("native interface member should return interface call values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AdjustedValue"), 42, TEXT("native interface member should pass ref parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bArrayStoredInterfaces"), true, TEXT("native interface array member should store callable entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNullResetWorked"), true, TEXT("native interface member should reset to null"))));

		FName NativeMarker = NAME_None;
		ASSERT_THAT(IsTrue(GetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NativeMarker"), NativeMarker), TEXT("native interface setter result should be readable")));
		ASSERT_THAT(AreEqual(FName(TEXT("FromUClassPropertyInterface")), NativeMarker, TEXT("native interface setter should mutate actor state")));

		int32 InterfaceRefCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("InterfaceRefs"), InterfaceRefCount), TEXT("native interface array member count should be readable")));
		ASSERT_THAT(AreEqual(2, InterfaceRefCount, TEXT("native interface array member should keep two entries")));

		FScriptInterface* InterfaceValue = InterfaceRefProperty->ContainerPtrToValuePtr<FScriptInterface>(Actor);
		ASSERT_THAT(IsNotNull(InterfaceValue, TEXT("C++ should read the native interface member value")));
		if (InterfaceValue != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), InterfaceValue->GetObject(), TEXT("native interface member should expose the script actor object")));
			ASSERT_THAT(IsNotNull(InterfaceValue->GetInterface(), TEXT("native interface member should expose a native interface pointer")));

			TScriptInterface<IAngelscriptNativeParentInterface> TypedInterfaceValue;
			TypedInterfaceValue.SetObject(InterfaceValue->GetObject());
			TypedInterfaceValue.SetInterface(static_cast<IAngelscriptNativeParentInterface*>(InterfaceValue->GetInterface()));
			ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), static_cast<UObject*>(TypedInterfaceValue.GetObject()), TEXT("native interface member should be readable as TScriptInterface object state")));
			ASSERT_THAT(IsNotNull(TypedInterfaceValue.GetInterface(), TEXT("native interface member should be readable as a typed TScriptInterface")));
			ASSERT_THAT(AreEqual(37, IAngelscriptNativeParentInterface::Execute_GetNativeValue(TypedInterfaceValue.GetObject()), TEXT("typed TScriptInterface object should dispatch through Execute_")));
		}

		FScriptArrayHelper InterfaceArrayHelper(InterfaceRefsProperty, InterfaceRefsProperty->ContainerPtrToValuePtr<void>(Actor));
		ASSERT_THAT(AreEqual(2, InterfaceArrayHelper.Num(), TEXT("C++ should observe two stored native interface array entries")));
		if (InterfaceArrayHelper.Num() > 0)
		{
			const FScriptInterface* FirstArrayInterface = reinterpret_cast<const FScriptInterface*>(InterfaceArrayHelper.GetRawPtr(0));
			ASSERT_THAT(IsNotNull(FirstArrayInterface, TEXT("first native interface array entry should be readable")));
			if (FirstArrayInterface != nullptr)
			{
				ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), FirstArrayInterface->GetObject(), TEXT("native interface array entry should expose the script actor object")));
				ASSERT_THAT(IsNotNull(FirstArrayInterface->GetInterface(), TEXT("native interface array entry should expose a native interface pointer")));
			}
		}

		FScriptInterface* ClearedInterfaceValue = ClearedInterfaceRefProperty->ContainerPtrToValuePtr<FScriptInterface>(Actor);
		ASSERT_THAT(IsNotNull(ClearedInterfaceValue, TEXT("C++ should read the cleared native interface member value")));
		if (ClearedInterfaceValue != nullptr)
		{
			ASSERT_THAT(IsNull(ClearedInterfaceValue->GetObject(), TEXT("cleared native interface member should have no object")));
			ASSERT_THAT(IsNull(ClearedInterfaceValue->GetInterface(), TEXT("cleared native interface member should have no interface pointer")));
		}
	}

	TEST_METHOD(UClassContainerMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_ContainerMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassContainerTargetActor : AActor
			{
			}

			UCLASS()
			class ACoverageUClassContainerMemberActor : AActor
			{
				UPROPERTY()
				TArray<int> IntArray;

				UPROPERTY()
				TArray<bool> BoolArray;

				UPROPERTY()
				TArray<double> DoubleArray;

				UPROPERTY()
				TArray<FString> StringArray;

				UPROPERTY()
				TArray<FName> NameArray;

				UPROPERTY()
				TArray<FText> TextArray;

				UPROPERTY()
				TArray<FVector> VectorArray;

				UPROPERTY()
				TArray<AActor> ActorArray;

				UPROPERTY()
				TSet<int> IntSet;

				UPROPERTY()
				TSet<FString> StringSet;

				UPROPERTY()
				TSet<FName> NameSet;

				UPROPERTY()
				TSet<FVector> VectorSet;

				UPROPERTY()
				TMap<int, FString> IntToStringMap;

				UPROPERTY()
				TMap<int, FName> IntToNameMap;

				UPROPERTY()
				TMap<int, FText> IntToTextMap;

				UPROPERTY()
				TMap<FName, int> NameToIntMap;

				UPROPERTY()
				TMap<FString, int> StringToIntMap;

				UPROPERTY()
				TMap<FString, FName> StringToNameMap;

				UPROPERTY()
				TMap<FString, FText> StringToTextMap;

				UPROPERTY()
				TMap<FName, FString> NameToStringMap;

				UPROPERTY()
				TMap<FName, FText> NameToTextMap;

				UPROPERTY()
				TMap<FName, FName> NameToNameMap;

				UPROPERTY()
				TMap<int, FVector> IntToVectorMap;

				UPROPERTY()
				double VectorMapSecondZ = 0.0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					IntArray.Add(7);
					IntArray.Add(11);
					BoolArray.Add(true);
					BoolArray.Add(false);
					DoubleArray.Add(2.25);
					DoubleArray.Add(4.5);
					StringArray.Add("First");
					StringArray.Add("Second");
					NameArray.Add(n"NameArrayFirst");
					NameArray.Add(n"NameArraySecond");
					TextArray.Add(FText::FromString("TextFirst"));
					TextArray.Add(FText::FromString("TextSecond"));
					VectorArray.Add(FVector(1, 2, 3));
					VectorArray.Add(FVector(4, 5, 6));
					ActorArray.Add(this);
					ActorArray.Add(SpawnActor(ACoverageUClassContainerTargetActor::StaticClass()));
					IntSet.Add(13);
					IntSet.Add(17);
					StringSet.Add("SetAlpha");
					StringSet.Add("SetAlpha");
					StringSet.Add("SetBeta");
					NameSet.Add(n"FirstName");
					NameSet.Add(n"SecondName");
					VectorSet.Add(FVector::ForwardVector);
					VectorSet.Add(FVector::RightVector);
					IntToStringMap.Add(19, "Nineteen");
					IntToStringMap.Add(23, "TwentyThree");
					IntToNameMap.Add(53, n"FiftyThreeName");
					IntToNameMap.Add(59, n"FiftyNineName");
					IntToTextMap.Add(37, FText::FromString("ThirtySevenText"));
					IntToTextMap.Add(41, FText::FromString("FortyOneText"));
					NameToIntMap.Add(n"Alpha", 29);
					NameToIntMap.Add(n"Beta", 31);
					StringToIntMap.Add("ScoreA", 43);
					StringToIntMap.Add("ScoreB", 47);
					StringToNameMap.Add("StringNameKey", n"StringNameValue");
					StringToTextMap.Add("StringTextKey", FText::FromString("String Text Value"));
					NameToStringMap.Add(n"NameStringKey", "Name String Value");
					NameToTextMap.Add(n"NameTextKey", FText::FromString("Name Text Value"));
					NameToNameMap.Add(n"OuterName", n"InnerName");
					IntToVectorMap.Add(1, FVector::ForwardVector);
					IntToVectorMap.Add(2, FVector(6, 7, 8));
					VectorMapSecondZ = IntToVectorMap[2].Z;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyContainerMemberMatrix.as"), ScriptSource)));

		UClass* TargetActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassContainerTargetActor"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassContainerMemberActor"));
		ASSERT_THAT(IsNotNull(TargetActorClass, TEXT("container target actor class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("container member actor class should be generated")));
		if (TargetActorClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		FArrayProperty* IntArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("IntArray"));
		FArrayProperty* BoolArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("BoolArray"));
		FArrayProperty* DoubleArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("DoubleArray"));
		FArrayProperty* StringArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("StringArray"));
		FArrayProperty* NameArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("NameArray"));
		FArrayProperty* TextArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("TextArray"));
		FArrayProperty* VectorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("VectorArray"));
		FArrayProperty* ActorArrayProperty = FindFProperty<FArrayProperty>(ActorClass, TEXT("ActorArray"));
		FSetProperty* IntSetProperty = FindFProperty<FSetProperty>(ActorClass, TEXT("IntSet"));
		FSetProperty* StringSetProperty = FindFProperty<FSetProperty>(ActorClass, TEXT("StringSet"));
		FSetProperty* NameSetProperty = FindFProperty<FSetProperty>(ActorClass, TEXT("NameSet"));
		FSetProperty* VectorSetProperty = FindFProperty<FSetProperty>(ActorClass, TEXT("VectorSet"));
		FMapProperty* IntToStringMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("IntToStringMap"));
		FMapProperty* IntToNameMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("IntToNameMap"));
		FMapProperty* IntToTextMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("IntToTextMap"));
		FMapProperty* NameToIntMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToIntMap"));
		FMapProperty* StringToIntMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("StringToIntMap"));
		FMapProperty* StringToNameMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("StringToNameMap"));
		FMapProperty* StringToTextMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("StringToTextMap"));
		FMapProperty* NameToStringMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToStringMap"));
		FMapProperty* NameToTextMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToTextMap"));
		FMapProperty* NameToNameMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("NameToNameMap"));
		FMapProperty* IntToVectorMapProperty = FindFProperty<FMapProperty>(ActorClass, TEXT("IntToVectorMap"));

		ASSERT_THAT(IsNotNull(IntArrayProperty, TEXT("TArray<int> member should reflect")));
		ASSERT_THAT(IsNotNull(BoolArrayProperty, TEXT("TArray<bool> member should reflect")));
		ASSERT_THAT(IsNotNull(DoubleArrayProperty, TEXT("TArray<double> member should reflect")));
		ASSERT_THAT(IsNotNull(StringArrayProperty, TEXT("TArray<FString> member should reflect")));
		ASSERT_THAT(IsNotNull(NameArrayProperty, TEXT("TArray<FName> member should reflect")));
		ASSERT_THAT(IsNotNull(TextArrayProperty, TEXT("TArray<FText> member should reflect")));
		ASSERT_THAT(IsNotNull(VectorArrayProperty, TEXT("TArray<FVector> member should reflect")));
		ASSERT_THAT(IsNotNull(ActorArrayProperty, TEXT("TArray<AActor> member should reflect")));
		ASSERT_THAT(IsNotNull(IntSetProperty, TEXT("TSet<int> member should reflect")));
		ASSERT_THAT(IsNotNull(StringSetProperty, TEXT("TSet<FString> member should reflect")));
		ASSERT_THAT(IsNotNull(NameSetProperty, TEXT("TSet<FName> member should reflect")));
		ASSERT_THAT(IsNotNull(VectorSetProperty, TEXT("TSet<FVector> member should reflect")));
		ASSERT_THAT(IsNotNull(IntToStringMapProperty, TEXT("TMap<int, FString> member should reflect")));
		ASSERT_THAT(IsNotNull(IntToNameMapProperty, TEXT("TMap<int, FName> member should reflect")));
		ASSERT_THAT(IsNotNull(IntToTextMapProperty, TEXT("TMap<int, FText> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToIntMapProperty, TEXT("TMap<FName, int> member should reflect")));
		ASSERT_THAT(IsNotNull(StringToIntMapProperty, TEXT("TMap<FString, int> member should reflect")));
		ASSERT_THAT(IsNotNull(StringToNameMapProperty, TEXT("TMap<FString, FName> member should reflect")));
		ASSERT_THAT(IsNotNull(StringToTextMapProperty, TEXT("TMap<FString, FText> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToStringMapProperty, TEXT("TMap<FName, FString> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToTextMapProperty, TEXT("TMap<FName, FText> member should reflect")));
		ASSERT_THAT(IsNotNull(NameToNameMapProperty, TEXT("TMap<FName, FName> member should reflect")));
		ASSERT_THAT(IsNotNull(IntToVectorMapProperty, TEXT("TMap<int, FVector> member should reflect")));
		if (IntArrayProperty == nullptr || BoolArrayProperty == nullptr || DoubleArrayProperty == nullptr || StringArrayProperty == nullptr || NameArrayProperty == nullptr
			|| TextArrayProperty == nullptr || VectorArrayProperty == nullptr || ActorArrayProperty == nullptr || IntSetProperty == nullptr
			|| StringSetProperty == nullptr || NameSetProperty == nullptr || VectorSetProperty == nullptr || IntToStringMapProperty == nullptr
			|| IntToNameMapProperty == nullptr || IntToTextMapProperty == nullptr || NameToIntMapProperty == nullptr || StringToIntMapProperty == nullptr
			|| StringToNameMapProperty == nullptr || StringToTextMapProperty == nullptr || NameToStringMapProperty == nullptr || NameToTextMapProperty == nullptr
			|| NameToNameMapProperty == nullptr || IntToVectorMapProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(IntArrayProperty->Inner->IsA<FIntProperty>(), TEXT("TArray<int> should use FIntProperty inner")));
		ASSERT_THAT(IsTrue(BoolArrayProperty->Inner->IsA<FBoolProperty>(), TEXT("TArray<bool> should use FBoolProperty inner")));
		ASSERT_THAT(IsTrue(DoubleArrayProperty->Inner->IsA<FDoubleProperty>(), TEXT("TArray<double> should use FDoubleProperty inner")));
		ASSERT_THAT(IsTrue(StringArrayProperty->Inner->IsA<FStrProperty>(), TEXT("TArray<FString> should use FStrProperty inner")));
		ASSERT_THAT(IsTrue(NameArrayProperty->Inner->IsA<FNameProperty>(), TEXT("TArray<FName> should use FNameProperty inner")));
		ASSERT_THAT(IsTrue(TextArrayProperty->Inner->IsA<FTextProperty>(), TEXT("TArray<FText> should use FTextProperty inner")));
		ASSERT_THAT(IsTrue(VectorArrayProperty->Inner->IsA<FStructProperty>(), TEXT("TArray<FVector> should use FStructProperty inner")));
		ASSERT_THAT(IsTrue(ActorArrayProperty->Inner->IsA<FObjectPropertyBase>(), TEXT("TArray<AActor> should use object inner")));
		ASSERT_THAT(IsTrue(IntSetProperty->ElementProp->IsA<FIntProperty>(), TEXT("TSet<int> should use FIntProperty element")));
		ASSERT_THAT(IsTrue(StringSetProperty->ElementProp->IsA<FStrProperty>(), TEXT("TSet<FString> should use FStrProperty element")));
		ASSERT_THAT(IsTrue(NameSetProperty->ElementProp->IsA<FNameProperty>(), TEXT("TSet<FName> should use FNameProperty element")));
		ASSERT_THAT(IsTrue(VectorSetProperty->ElementProp->IsA<FStructProperty>(), TEXT("TSet<FVector> should use FStructProperty element")));
		ASSERT_THAT(IsTrue(IntToStringMapProperty->KeyProp->IsA<FIntProperty>(), TEXT("TMap<int, FString> should use FIntProperty key")));
		ASSERT_THAT(IsTrue(IntToStringMapProperty->ValueProp->IsA<FStrProperty>(), TEXT("TMap<int, FString> should use FStrProperty value")));
		ASSERT_THAT(IsTrue(IntToNameMapProperty->KeyProp->IsA<FIntProperty>(), TEXT("TMap<int, FName> should use FIntProperty key")));
		ASSERT_THAT(IsTrue(IntToNameMapProperty->ValueProp->IsA<FNameProperty>(), TEXT("TMap<int, FName> should use FNameProperty value")));
		ASSERT_THAT(IsTrue(IntToTextMapProperty->KeyProp->IsA<FIntProperty>(), TEXT("TMap<int, FText> should use FIntProperty key")));
		ASSERT_THAT(IsTrue(IntToTextMapProperty->ValueProp->IsA<FTextProperty>(), TEXT("TMap<int, FText> should use FTextProperty value")));
		ASSERT_THAT(IsTrue(NameToIntMapProperty->KeyProp->IsA<FNameProperty>(), TEXT("TMap<FName, int> should use FNameProperty key")));
		ASSERT_THAT(IsTrue(NameToIntMapProperty->ValueProp->IsA<FIntProperty>(), TEXT("TMap<FName, int> should use FIntProperty value")));
		ASSERT_THAT(IsTrue(StringToIntMapProperty->KeyProp->IsA<FStrProperty>(), TEXT("TMap<FString, int> should use FStrProperty key")));
		ASSERT_THAT(IsTrue(StringToIntMapProperty->ValueProp->IsA<FIntProperty>(), TEXT("TMap<FString, int> should use FIntProperty value")));
		ASSERT_THAT(IsTrue(StringToNameMapProperty->KeyProp->IsA<FStrProperty>(), TEXT("TMap<FString, FName> should use FStrProperty key")));
		ASSERT_THAT(IsTrue(StringToNameMapProperty->ValueProp->IsA<FNameProperty>(), TEXT("TMap<FString, FName> should use FNameProperty value")));
		ASSERT_THAT(IsTrue(StringToTextMapProperty->KeyProp->IsA<FStrProperty>(), TEXT("TMap<FString, FText> should use FStrProperty key")));
		ASSERT_THAT(IsTrue(StringToTextMapProperty->ValueProp->IsA<FTextProperty>(), TEXT("TMap<FString, FText> should use FTextProperty value")));
		ASSERT_THAT(IsTrue(NameToStringMapProperty->KeyProp->IsA<FNameProperty>(), TEXT("TMap<FName, FString> should use FNameProperty key")));
		ASSERT_THAT(IsTrue(NameToStringMapProperty->ValueProp->IsA<FStrProperty>(), TEXT("TMap<FName, FString> should use FStrProperty value")));
		ASSERT_THAT(IsTrue(NameToTextMapProperty->KeyProp->IsA<FNameProperty>(), TEXT("TMap<FName, FText> should use FNameProperty key")));
		ASSERT_THAT(IsTrue(NameToTextMapProperty->ValueProp->IsA<FTextProperty>(), TEXT("TMap<FName, FText> should use FTextProperty value")));
		ASSERT_THAT(IsTrue(NameToNameMapProperty->KeyProp->IsA<FNameProperty>(), TEXT("TMap<FName, FName> should use FNameProperty key")));
		ASSERT_THAT(IsTrue(NameToNameMapProperty->ValueProp->IsA<FNameProperty>(), TEXT("TMap<FName, FName> should use FNameProperty value")));
		ASSERT_THAT(IsTrue(IntToVectorMapProperty->KeyProp->IsA<FIntProperty>(), TEXT("TMap<int, FVector> should use FIntProperty key")));
		ASSERT_THAT(IsTrue(IntToVectorMapProperty->ValueProp->IsA<FStructProperty>(), TEXT("TMap<int, FVector> should use FStructProperty value")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("container member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("IntArray"), Count), TEXT("TArray<int> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<int> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[0]"), 7, TEXT("TArray<int>[0] should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntArray[1]"), 11, TEXT("TArray<int>[1] should round-trip"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("BoolArray"), Count), TEXT("TArray<bool> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<bool> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[0]"), true, TEXT("TArray<bool>[0] should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolArray[1]"), false, TEXT("TArray<bool>[1] should round-trip"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("DoubleArray"), Count), TEXT("TArray<double> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<double> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleArray[0]"), 2.25, TEXT("TArray<double>[0] should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("DoubleArray[1]"), 4.5, TEXT("TArray<double>[1] should round-trip"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StringArray"), Count), TEXT("TArray<FString> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FString> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[0]"), FString(TEXT("First")), TEXT("TArray<FString>[0] should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[1]"), FString(TEXT("Second")), TEXT("TArray<FString>[1] should round-trip"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("NameArray"), Count), TEXT("TArray<FName> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FName> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[0]"), FName(TEXT("NameArrayFirst")), TEXT("TArray<FName>[0] should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[1]"), FName(TEXT("NameArraySecond")), TEXT("TArray<FName>[1] should round-trip"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("TextArray"), Count), TEXT("TArray<FText> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FText> should hold two values")));
		FText TextValue;
		ASSERT_THAT(IsTrue(GetTextByPath(*TestRunner, Actor, TEXT("TextArray[0]"), TextValue), TEXT("TArray<FText>[0] should be readable")));
		ASSERT_THAT(AreEqual(FString(TEXT("TextFirst")), TextValue.ToString(), TEXT("TArray<FText>[0] should round-trip")));
		ASSERT_THAT(IsTrue(GetTextByPath(*TestRunner, Actor, TEXT("TextArray[1]"), TextValue), TEXT("TArray<FText>[1] should be readable")));
		ASSERT_THAT(AreEqual(FString(TEXT("TextSecond")), TextValue.ToString(), TEXT("TArray<FText>[1] should round-trip")));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("VectorArray"), Count), TEXT("TArray<FVector> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FVector> should hold two values")));
		FVector VectorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorArray[0]"), VectorValue), TEXT("TArray<FVector>[0] should be readable")));
		ASSERT_THAT(IsTrue(VectorValue.Equals(FVector(1, 2, 3), 0.001), TEXT("TArray<FVector>[0] should round-trip")));
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorArray[1]"), VectorValue), TEXT("TArray<FVector>[1] should be readable")));
		ASSERT_THAT(IsTrue(VectorValue.Equals(FVector(4, 5, 6), 0.001), TEXT("TArray<FVector>[1] should round-trip")));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ActorArray"), Count), TEXT("TArray<AActor> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<AActor> should hold two values")));
		UObject* ActorArrayFirst = nullptr;
		UObject* ActorArraySecond = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ActorArray[0]"), ActorArrayFirst), TEXT("TArray<AActor>[0] should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ActorArray[1]"), ActorArraySecond), TEXT("TArray<AActor>[1] should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), ActorArrayFirst, TEXT("TArray<AActor>[0] should store this actor")));
		ASSERT_THAT(IsTrue(ActorArraySecond != nullptr && ActorArraySecond->IsA(TargetActorClass), TEXT("TArray<AActor>[1] should store the spawned target actor")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("IntSet"), Count), TEXT("TSet<int> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<int> should hold two values")));
		ASSERT_THAT(IsTrue(SetContainsByPath<int32>(*TestRunner, Actor, TEXT("IntSet"), 13), TEXT("TSet<int> should contain 13")));
		ASSERT_THAT(IsTrue(SetContainsByPath<int32>(*TestRunner, Actor, TEXT("IntSet"), 17), TEXT("TSet<int> should contain 17")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StringSet"), Count), TEXT("TSet<FString> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FString> should deduplicate repeated values")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FString>(*TestRunner, Actor, TEXT("StringSet"), FString(TEXT("SetAlpha"))), TEXT("TSet<FString> should contain SetAlpha")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FString>(*TestRunner, Actor, TEXT("StringSet"), FString(TEXT("SetBeta"))), TEXT("TSet<FString> should contain SetBeta")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("NameSet"), Count), TEXT("TSet<FName> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FName> should hold two values")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FName>(*TestRunner, Actor, TEXT("NameSet"), FName(TEXT("FirstName"))), TEXT("TSet<FName> should contain FirstName")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FName>(*TestRunner, Actor, TEXT("NameSet"), FName(TEXT("SecondName"))), TEXT("TSet<FName> should contain SecondName")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("VectorSet"), Count), TEXT("TSet<FVector> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FVector> should hold two values")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FVector>(*TestRunner, Actor, TEXT("VectorSet"), FVector::ForwardVector), TEXT("TSet<FVector> should contain ForwardVector")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FVector>(*TestRunner, Actor, TEXT("VectorSet"), FVector::RightVector), TEXT("TSet<FVector> should contain RightVector")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToStringMap"), Count), TEXT("TMap<int, FString> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, FString> should hold two values")));
		FString StringMapValue;
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("IntToStringMap"), 19, StringMapValue), TEXT("TMap<int, FString> should contain key 19")));
		ASSERT_THAT(AreEqual(FString(TEXT("Nineteen")), StringMapValue, TEXT("TMap<int, FString>[19] should round-trip")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("IntToStringMap"), 23, StringMapValue), TEXT("TMap<int, FString> should contain key 23")));
		ASSERT_THAT(AreEqual(FString(TEXT("TwentyThree")), StringMapValue, TEXT("TMap<int, FString>[23] should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToNameMap"), Count), TEXT("TMap<int, FName> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, FName> should hold two values")));
		FName NameMapValue;
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FNameProperty, FName>(*TestRunner, Actor, TEXT("IntToNameMap"), 53, NameMapValue), TEXT("TMap<int, FName> should contain key 53")));
		ASSERT_THAT(AreEqual(FName(TEXT("FiftyThreeName")), NameMapValue, TEXT("TMap<int, FName>[53] should round-trip")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FNameProperty, FName>(*TestRunner, Actor, TEXT("IntToNameMap"), 59, NameMapValue), TEXT("TMap<int, FName> should contain key 59")));
		ASSERT_THAT(AreEqual(FName(TEXT("FiftyNineName")), NameMapValue, TEXT("TMap<int, FName>[59] should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToTextMap"), Count), TEXT("TMap<int, FText> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, FText> should hold two values")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FTextProperty, FText>(*TestRunner, Actor, TEXT("IntToTextMap"), 37, TextValue), TEXT("TMap<int, FText> should contain key 37")));
		ASSERT_THAT(AreEqual(FString(TEXT("ThirtySevenText")), TextValue.ToString(), TEXT("TMap<int, FText>[37] should round-trip")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FTextProperty, FText>(*TestRunner, Actor, TEXT("IntToTextMap"), 41, TextValue), TEXT("TMap<int, FText> should contain key 41")));
		ASSERT_THAT(AreEqual(FString(TEXT("FortyOneText")), TextValue.ToString(), TEXT("TMap<int, FText>[41] should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToIntMap"), Count), TEXT("TMap<FName, int> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName, int> should hold two values")));
		int32 IntMapValue = 0;
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, Actor, TEXT("NameToIntMap"), FName(TEXT("Alpha")), IntMapValue), TEXT("TMap<FName, int> should contain key Alpha")));
		ASSERT_THAT(AreEqual(29, IntMapValue, TEXT("TMap<FName, int>[Alpha] should round-trip")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, Actor, TEXT("NameToIntMap"), FName(TEXT("Beta")), IntMapValue), TEXT("TMap<FName, int> should contain key Beta")));
		ASSERT_THAT(AreEqual(31, IntMapValue, TEXT("TMap<FName, int>[Beta] should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToIntMap"), Count), TEXT("TMap<FString, int> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString, int> should hold two values")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToIntMap"), FString(TEXT("ScoreA")), IntMapValue), TEXT("TMap<FString, int> should contain key ScoreA")));
		ASSERT_THAT(AreEqual(43, IntMapValue, TEXT("TMap<FString, int>[ScoreA] should round-trip")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToIntMap"), FString(TEXT("ScoreB")), IntMapValue), TEXT("TMap<FString, int> should contain key ScoreB")));
		ASSERT_THAT(AreEqual(47, IntMapValue, TEXT("TMap<FString, int>[ScoreB] should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToNameMap"), Count), TEXT("TMap<FString, FName> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FString, FName> should hold one value")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FNameProperty, FName>(*TestRunner, Actor, TEXT("StringToNameMap"), FString(TEXT("StringNameKey")), NameMapValue), TEXT("TMap<FString, FName> should contain StringNameKey")));
		ASSERT_THAT(AreEqual(FName(TEXT("StringNameValue")), NameMapValue, TEXT("TMap<FString, FName> value should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToTextMap"), Count), TEXT("TMap<FString, FText> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FString, FText> should hold one value")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FTextProperty, FText>(*TestRunner, Actor, TEXT("StringToTextMap"), FString(TEXT("StringTextKey")), TextValue), TEXT("TMap<FString, FText> should contain StringTextKey")));
		ASSERT_THAT(AreEqual(FString(TEXT("String Text Value")), TextValue.ToString(), TEXT("TMap<FString, FText> value should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToStringMap"), Count), TEXT("TMap<FName, FString> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, FString> should hold one value")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FStrProperty, FString>(*TestRunner, Actor, TEXT("NameToStringMap"), FName(TEXT("NameStringKey")), StringMapValue), TEXT("TMap<FName, FString> should contain NameStringKey")));
		ASSERT_THAT(AreEqual(FString(TEXT("Name String Value")), StringMapValue, TEXT("TMap<FName, FString> value should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToTextMap"), Count), TEXT("TMap<FName, FText> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, FText> should hold one value")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FTextProperty, FText>(*TestRunner, Actor, TEXT("NameToTextMap"), FName(TEXT("NameTextKey")), TextValue), TEXT("TMap<FName, FText> should contain NameTextKey")));
		ASSERT_THAT(AreEqual(FString(TEXT("Name Text Value")), TextValue.ToString(), TEXT("TMap<FName, FText> value should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToNameMap"), Count), TEXT("TMap<FName, FName> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName, FName> should hold one value")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FNameProperty, FName>(*TestRunner, Actor, TEXT("NameToNameMap"), FName(TEXT("OuterName")), NameMapValue), TEXT("TMap<FName, FName> should contain OuterName")));
		ASSERT_THAT(AreEqual(FName(TEXT("InnerName")), NameMapValue, TEXT("TMap<FName, FName> value should round-trip")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToVectorMap"), Count), TEXT("TMap<int, FVector> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, FVector> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[1].X"), 1.0, TEXT("TMap<int, FVector>[1].X should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("IntToVectorMap[2].Y"), 7.0, TEXT("TMap<int, FVector>[2].Y should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("VectorMapSecondZ"), 8.0, TEXT("TMap<int, FVector> runtime index read should round-trip"))));
	}

	TEST_METHOD(UClassEnumContainerMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_EnumContainerMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EUClassPropertyContainerState
			{
				Idle,
				Armed,
				Fired
			}

			UCLASS()
			class ACoverageUClassEnumContainerMemberActor : AActor
			{
				UPROPERTY()
				TArray<EUClassPropertyContainerState> StateArray;

				UPROPERTY()
				TSet<EUClassPropertyContainerState> StateSet;

				UPROPERTY()
				TMap<EUClassPropertyContainerState, int> StateToScore;

				UPROPERTY()
				TMap<int, EUClassPropertyContainerState> ScoreToState;

				UPROPERTY()
				bool bSetContainsArmed = false;

				UPROPERTY()
				int ArmedScore = 0;

				UPROPERTY()
				bool bScoreToStateFired = false;

				UPROPERTY()
				int ArraySecondValue = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StateArray.Add(EUClassPropertyContainerState::Idle);
					StateArray.Add(EUClassPropertyContainerState::Fired);
					StateArray.Add(EUClassPropertyContainerState::Armed);
					ArraySecondValue = int(StateArray[1]);

					StateSet.Add(EUClassPropertyContainerState::Idle);
					StateSet.Add(EUClassPropertyContainerState::Armed);
					StateSet.Add(EUClassPropertyContainerState::Armed);
					bSetContainsArmed = StateSet.Contains(EUClassPropertyContainerState::Armed);

					StateToScore.Add(EUClassPropertyContainerState::Idle, 10);
					StateToScore.Add(EUClassPropertyContainerState::Armed, 20);
					StateToScore.Find(EUClassPropertyContainerState::Armed, ArmedScore);

					ScoreToState.Add(1, EUClassPropertyContainerState::Idle);
					ScoreToState.Add(2, EUClassPropertyContainerState::Fired);
					bScoreToStateFired = ScoreToState[2] == EUClassPropertyContainerState::Fired;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyEnumContainerMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassEnumContainerMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("enum container member actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FArrayProperty* StateArrayProperty = FindFProperty<FArrayProperty>(ScriptClass, TEXT("StateArray"));
		FSetProperty* StateSetProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("StateSet"));
		FMapProperty* StateToScoreProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StateToScore"));
		FMapProperty* ScoreToStateProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("ScoreToState"));
		ASSERT_THAT(IsNotNull(StateArrayProperty, TEXT("TArray<UENUM> member should reflect")));
		ASSERT_THAT(IsNotNull(StateSetProperty, TEXT("TSet<UENUM> member should reflect")));
		ASSERT_THAT(IsNotNull(StateToScoreProperty, TEXT("TMap<UENUM,int> member should reflect")));
		ASSERT_THAT(IsNotNull(ScoreToStateProperty, TEXT("TMap<int,UENUM> member should reflect")));
		if (StateArrayProperty == nullptr || StateSetProperty == nullptr || StateToScoreProperty == nullptr || ScoreToStateProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FEnumProperty>(StateArrayProperty->Inner), TEXT("TArray<UENUM> inner should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(CastField<FEnumProperty>(StateSetProperty->ElementProp), TEXT("TSet<UENUM> element should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(CastField<FEnumProperty>(StateToScoreProperty->KeyProp), TEXT("TMap<UENUM,int> key should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(StateToScoreProperty->ValueProp), TEXT("TMap<UENUM,int> value should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(ScoreToStateProperty->KeyProp), TEXT("TMap<int,UENUM> key should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FEnumProperty>(ScoreToStateProperty->ValueProp), TEXT("TMap<int,UENUM> value should be FEnumProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("enum container member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StateArray"), Count), TEXT("TArray<UENUM> count should be readable")));
		ASSERT_THAT(AreEqual(3, Count, TEXT("TArray<UENUM> should hold three entries")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StateSet"), Count), TEXT("TSet<UENUM> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<UENUM> should deduplicate duplicate entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StateToScore"), Count), TEXT("TMap<UENUM,int> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<UENUM,int> should hold two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ScoreToState"), Count), TEXT("TMap<int,UENUM> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,UENUM> should hold two entries")));

		int64 EnumValue = INDEX_NONE;
		ASSERT_THAT(IsTrue(GetEnumByPath(*TestRunner, Actor, TEXT("StateArray[1]"), EnumValue), TEXT("TArray<UENUM>[1] should be readable")));
		ASSERT_THAT(AreEqual(2LL, EnumValue, TEXT("TArray<UENUM>[1] should preserve Fired")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArraySecondValue"), 2, TEXT("AS runtime should read enum array values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetContainsArmed"), true, TEXT("AS runtime should find enum set values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArmedScore"), 20, TEXT("AS runtime should find enum-keyed map values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bScoreToStateFired"), true, TEXT("AS runtime should read enum map values"))));
	}

	TEST_METHOD(UClassScriptStructMemberContainerMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_ScriptStructMemberContainerMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUClassPropertyStructPayload
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				UPROPERTY()
				FString Label;

				bool opEquals(const FUClassPropertyStructPayload& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 613) + Tag.GetHash();
				}
			}

			UCLASS()
			class ACoverageUClassScriptStructMemberActor : AActor
			{
				UPROPERTY()
				FUClassPropertyStructPayload DirectPayload;

				UPROPERTY()
				TArray<FUClassPropertyStructPayload> PayloadArray;

				UPROPERTY()
				TMap<int, FUClassPropertyStructPayload> PayloadMap;

				UPROPERTY()
				TSet<FUClassPropertyStructPayload> PayloadSet;

				UPROPERTY()
				bool bSetDeduplicated = false;

				UPROPERTY()
				bool bSetContainsDuplicate = false;

				FUClassPropertyStructPayload MakePayload(int ID, FName Tag, FString Label)
				{
					FUClassPropertyStructPayload Payload;
					Payload.ID = ID;
					Payload.Tag = Tag;
					Payload.Label = Label;
					return Payload;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					DirectPayload = MakePayload(11, n"Direct", "DirectLabel");
					PayloadArray.Add(MakePayload(21, n"ArrayA", "ArrayLabelA"));
					PayloadArray.Add(MakePayload(22, n"ArrayB", "ArrayLabelB"));
					PayloadMap.Add(31, MakePayload(31, n"MapA", "MapLabelA"));
					PayloadMap.Add(32, MakePayload(32, n"MapB", "MapLabelB"));

					FUClassPropertyStructPayload SetA = MakePayload(41, n"SetA", "SetLabelA");
					FUClassPropertyStructPayload SetADuplicate = MakePayload(41, n"SetA", "SetLabelADuplicate");
					FUClassPropertyStructPayload SetB = MakePayload(42, n"SetB", "SetLabelB");
					PayloadSet.Add(SetA);
					PayloadSet.Add(SetADuplicate);
					PayloadSet.Add(SetB);
					bSetDeduplicated = PayloadSet.Num() == 2;
					bSetContainsDuplicate = PayloadSet.Contains(SetADuplicate);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyScriptStructMemberContainerMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassScriptStructMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("script USTRUCT member actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DirectPayloadProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("DirectPayload"));
		FArrayProperty* PayloadArrayProperty = FindFProperty<FArrayProperty>(ScriptClass, TEXT("PayloadArray"));
		FMapProperty* PayloadMapProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("PayloadMap"));
		FSetProperty* PayloadSetProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("PayloadSet"));
		ASSERT_THAT(IsNotNull(DirectPayloadProperty, TEXT("script USTRUCT direct member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(PayloadArrayProperty, TEXT("TArray<script USTRUCT> member should reflect as FArrayProperty")));
		ASSERT_THAT(IsNotNull(PayloadMapProperty, TEXT("TMap<int, script USTRUCT> member should reflect as FMapProperty")));
		ASSERT_THAT(IsNotNull(PayloadSetProperty, TEXT("TSet<script USTRUCT> member should reflect as FSetProperty")));
		if (DirectPayloadProperty == nullptr || PayloadArrayProperty == nullptr || PayloadMapProperty == nullptr || PayloadSetProperty == nullptr)
		{
			return;
		}

		FStructProperty* ArrayInnerProperty = CastField<FStructProperty>(PayloadArrayProperty->Inner);
		FStructProperty* MapValueProperty = CastField<FStructProperty>(PayloadMapProperty->ValueProp);
		FStructProperty* SetElementProperty = CastField<FStructProperty>(PayloadSetProperty->ElementProp);
		ASSERT_THAT(IsNotNull(ArrayInnerProperty, TEXT("TArray<script USTRUCT> inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(MapValueProperty, TEXT("TMap<int, script USTRUCT> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(SetElementProperty, TEXT("TSet<script USTRUCT> element should be FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(PayloadMapProperty->KeyProp), TEXT("TMap<int, script USTRUCT> key should be FIntProperty")));
		if (ArrayInnerProperty == nullptr || MapValueProperty == nullptr || SetElementProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(DirectPayloadProperty->Struct, ArrayInnerProperty->Struct, TEXT("direct and array script USTRUCT members should share type identity")));
		ASSERT_THAT(AreEqual(DirectPayloadProperty->Struct, MapValueProperty->Struct, TEXT("direct and map-value script USTRUCT members should share type identity")));
		ASSERT_THAT(AreEqual(DirectPayloadProperty->Struct, SetElementProperty->Struct, TEXT("direct and set-element script USTRUCT members should share type identity")));
		UScriptStruct::ICppStructOps* StructOps = DirectPayloadProperty->Struct != nullptr ? DirectPayloadProperty->Struct->GetCppStructOps() : nullptr;
		ASSERT_THAT(IsNotNull(StructOps, TEXT("hashable script USTRUCT member should expose CppStructOps")));
		if (StructOps == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructOps->HasGetTypeHash(), TEXT("hashable script USTRUCT member should expose GetTypeHash for set membership")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("script USTRUCT member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DirectPayload.ID"), 11, TEXT("direct script USTRUCT int member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("DirectPayload.Tag"), FName(TEXT("Direct")), TEXT("direct script USTRUCT FName member should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("DirectPayload.Label"), FString(TEXT("DirectLabel")), TEXT("direct script USTRUCT FString member should round-trip"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("PayloadArray"), Count), TEXT("TArray<script USTRUCT> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<script USTRUCT> should hold two values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PayloadArray[0].ID"), 21, TEXT("TArray<script USTRUCT>[0].ID should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("PayloadArray[1].Tag"), FName(TEXT("ArrayB")), TEXT("TArray<script USTRUCT>[1].Tag should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("PayloadArray[1].Label"), FString(TEXT("ArrayLabelB")), TEXT("TArray<script USTRUCT>[1].Label should round-trip"))));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("PayloadMap"), Count), TEXT("TMap<int, script USTRUCT> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int, script USTRUCT> should hold two values")));
		FPropertyBindingPathIndirection MapLeaf;
		ASSERT_THAT(IsTrue(ResolvePathOnObject(*TestRunner, Actor, TEXT("PayloadMap"), MapLeaf), TEXT("PayloadMap path should resolve")));
		if (MapLeaf.GetPropertyAddress() == nullptr)
		{
			return;
		}
		FScriptMapHelper MapHelper(PayloadMapProperty, MapLeaf.GetPropertyAddress());
		const void* MapValueAddress = nullptr;
		for (int32 SparseIndex = 0; SparseIndex < MapHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!MapHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (*reinterpret_cast<const int32*>(MapHelper.GetKeyPtr(SparseIndex)) == 32)
			{
				MapValueAddress = MapHelper.GetValuePtr(SparseIndex);
				break;
			}
		}
		ASSERT_THAT(IsNotNull(MapValueAddress, TEXT("TMap<int, script USTRUCT> should contain key 32")));
		if (MapValueAddress == nullptr || MapValueProperty->Struct == nullptr)
		{
			return;
		}

		FIntProperty* IDProperty = FindFProperty<FIntProperty>(MapValueProperty->Struct, TEXT("ID"));
		FNameProperty* TagProperty = FindFProperty<FNameProperty>(MapValueProperty->Struct, TEXT("Tag"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(MapValueProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(IDProperty, TEXT("script USTRUCT map value should expose ID")));
		ASSERT_THAT(IsNotNull(TagProperty, TEXT("script USTRUCT map value should expose Tag")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("script USTRUCT map value should expose Label")));
		if (IDProperty == nullptr || TagProperty == nullptr || LabelProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(32, IDProperty->GetPropertyValue_InContainer(MapValueAddress), TEXT("TMap<int, script USTRUCT> should preserve ID")));
		ASSERT_THAT(AreEqual(FName(TEXT("MapB")), TagProperty->GetPropertyValue_InContainer(MapValueAddress), TEXT("TMap<int, script USTRUCT> should preserve Tag")));
		ASSERT_THAT(AreEqual(FString(TEXT("MapLabelB")), LabelProperty->GetPropertyValue_InContainer(MapValueAddress), TEXT("TMap<int, script USTRUCT> should preserve Label")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("PayloadSet"), Count), TEXT("TSet<script USTRUCT> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<script USTRUCT> should deduplicate equivalent entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetDeduplicated"), true, TEXT("TSet<script USTRUCT> should deduplicate by opEquals and Hash"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetContainsDuplicate"), true, TEXT("TSet<script USTRUCT> should find equivalent duplicate values"))));
	}

	TEST_METHOD(UClassOptionalMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_OptionalMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EUClassPropertyOptionalState
			{
				Idle,
				Armed,
				Fired
			}

			USTRUCT(BlueprintType)
			struct FUClassPropertyOptionalPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageUClassOptionalMemberActor : AActor
			{
				UPROPERTY()
				TOptional<bool> MaybeBool;

				UPROPERTY()
				TOptional<int> MaybeCount;

				UPROPERTY()
				TOptional<float> MaybeFloat;

				UPROPERTY()
				TOptional<FName> MaybeName;

				UPROPERTY()
				TOptional<EUClassPropertyOptionalState> MaybeState;

				UPROPERTY()
				TOptional<FString> MaybeLabel;

				UPROPERTY()
				TOptional<FVector> MaybeVector;

				UPROPERTY()
				TOptional<FUClassPropertyOptionalPayload> MaybePayload;

				UPROPERTY()
				TOptional<UObject> MaybeObject;

				UPROPERTY()
				TOptional<int> EmptyCount;

				UPROPERTY()
				TOptional<UObject> NullObject;

				UPROPERTY()
				bool bMaybeBoolSet = false;

				UPROPERTY()
				bool bBoolValue = false;

				UPROPERTY()
				bool bMaybeCountSet = false;

				UPROPERTY()
				int CountValue = 0;

				UPROPERTY()
				bool bMaybeFloatSet = false;

				UPROPERTY()
				float FloatValue = 0.0;

				UPROPERTY()
				bool bMaybeNameSet = false;

				UPROPERTY()
				FName NameValue;

				UPROPERTY()
				bool bMaybeStateSet = false;

				UPROPERTY()
				int StateValue = 0;

				UPROPERTY()
				bool bMaybeLabelSet = false;

				UPROPERTY()
				FString LabelValue;

				UPROPERTY()
				bool bMaybeVectorSet = false;

				UPROPERTY()
				FVector VectorValue;

				UPROPERTY()
				bool bMaybePayloadSet = false;

				UPROPERTY()
				int PayloadCountValue = 0;

				UPROPERTY()
				FString PayloadLabelValue;

				UPROPERTY()
				bool bMaybeObjectSet = false;

				UPROPERTY()
				bool bMaybeObjectIsSelf = false;

				UPROPERTY()
				bool bEmptyCountSet = true;

				UPROPERTY()
				bool bNullObjectSet = false;

				UPROPERTY()
				bool bNullObjectValueIsNull = false;

				UObject GetNullObject()
				{
					return nullptr;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					MaybeBool.Set(false);
					MaybeCount.Set(71);
					MaybeFloat.Set(12.5f);
					MaybeName.Set(n"OptionalName");
					MaybeState.Set(EUClassPropertyOptionalState::Fired);
					MaybeLabel.Set(FString("OptionalLabel"));
					MaybeVector.Set(FVector(7, 8, 9));
					MaybeObject.Set(this);
					NullObject.Set(GetNullObject());

					FUClassPropertyOptionalPayload Payload;
					Payload.Count = 83;
					Payload.Label = "PayloadLabel";
					MaybePayload.Set(Payload);

					EmptyCount.Reset();

					bMaybeBoolSet = MaybeBool.IsSet();
					bBoolValue = MaybeBool.GetValue();
					bMaybeCountSet = MaybeCount.IsSet();
					CountValue = MaybeCount.GetValue();
					bMaybeFloatSet = MaybeFloat.IsSet();
					FloatValue = MaybeFloat.GetValue();
					bMaybeNameSet = MaybeName.IsSet();
					NameValue = MaybeName.GetValue();
					bMaybeStateSet = MaybeState.IsSet();
					StateValue = int(MaybeState.GetValue());
					bMaybeLabelSet = MaybeLabel.IsSet();
					LabelValue = MaybeLabel.GetValue();
					bMaybeVectorSet = MaybeVector.IsSet();
					VectorValue = MaybeVector.GetValue();
					bMaybePayloadSet = MaybePayload.IsSet();
					PayloadCountValue = MaybePayload.GetValue().Count;
					PayloadLabelValue = MaybePayload.GetValue().Label;
					bMaybeObjectSet = MaybeObject.IsSet();
					bMaybeObjectIsSelf = MaybeObject.GetValue() == this;
					bEmptyCountSet = EmptyCount.IsSet();
					bNullObjectSet = NullObject.IsSet();
					bNullObjectValueIsNull = NullObject.GetValue() == nullptr;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyOptionalMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassOptionalMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("optional member actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FOptionalProperty* MaybeBoolProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeBool"));
		FOptionalProperty* MaybeCountProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeCount"));
		FOptionalProperty* MaybeFloatProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeFloat"));
		FOptionalProperty* MaybeNameProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeName"));
		FOptionalProperty* MaybeStateProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeState"));
		FOptionalProperty* MaybeLabelProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeLabel"));
		FOptionalProperty* MaybeVectorProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeVector"));
		FOptionalProperty* MaybePayloadProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybePayload"));
		FOptionalProperty* MaybeObjectProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("MaybeObject"));
		FOptionalProperty* EmptyCountProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("EmptyCount"));
		FOptionalProperty* NullObjectProperty = FindFProperty<FOptionalProperty>(ScriptClass, TEXT("NullObject"));
		ASSERT_THAT(IsNotNull(MaybeBoolProperty, TEXT("TOptional<bool> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeCountProperty, TEXT("TOptional<int> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeFloatProperty, TEXT("TOptional<float> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeNameProperty, TEXT("TOptional<FName> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeStateProperty, TEXT("TOptional<UENUM> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeLabelProperty, TEXT("TOptional<FString> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeVectorProperty, TEXT("TOptional<FVector> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybePayloadProperty, TEXT("TOptional<USTRUCT> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(MaybeObjectProperty, TEXT("TOptional<UObject> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(EmptyCountProperty, TEXT("empty TOptional<int> member should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(NullObjectProperty, TEXT("TOptional<UObject> null member should reflect as FOptionalProperty")));
		if (MaybeBoolProperty == nullptr || MaybeCountProperty == nullptr || MaybeFloatProperty == nullptr || MaybeNameProperty == nullptr
			|| MaybeStateProperty == nullptr || MaybeLabelProperty == nullptr || MaybeVectorProperty == nullptr
			|| MaybePayloadProperty == nullptr || MaybeObjectProperty == nullptr || EmptyCountProperty == nullptr || NullObjectProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(MaybeBoolProperty->GetValueProperty()), TEXT("TOptional<bool> inner should be FBoolProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(MaybeCountProperty->GetValueProperty()), TEXT("TOptional<int> inner should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FDoubleProperty>(MaybeFloatProperty->GetValueProperty()), TEXT("TOptional<float> inner should be FDoubleProperty in this AS configuration")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(MaybeNameProperty->GetValueProperty()), TEXT("TOptional<FName> inner should be FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FEnumProperty>(MaybeStateProperty->GetValueProperty()), TEXT("TOptional<UENUM> inner should be FEnumProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(MaybeLabelProperty->GetValueProperty()), TEXT("TOptional<FString> inner should be FStrProperty")));
		FStructProperty* VectorInnerProperty = CastField<FStructProperty>(MaybeVectorProperty->GetValueProperty());
		FStructProperty* PayloadInnerProperty = CastField<FStructProperty>(MaybePayloadProperty->GetValueProperty());
		ASSERT_THAT(IsNotNull(VectorInnerProperty, TEXT("TOptional<FVector> inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(PayloadInnerProperty, TEXT("TOptional<USTRUCT> inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FObjectPropertyBase>(MaybeObjectProperty->GetValueProperty()), TEXT("TOptional<UObject> inner should be object-backed")));
		ASSERT_THAT(IsNotNull(CastField<FObjectPropertyBase>(NullObjectProperty->GetValueProperty()), TEXT("TOptional<UObject> null inner should be object-backed")));
		if (VectorInnerProperty == nullptr || PayloadInnerProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VectorInnerProperty->Struct != nullptr && VectorInnerProperty->Struct->IsChildOf(TBaseStructure<FVector>::Get()), TEXT("TOptional<FVector> inner should keep FVector identity")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(PayloadInnerProperty->Struct, TEXT("Count")), TEXT("TOptional<USTRUCT> payload should reflect Count")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(PayloadInnerProperty->Struct, TEXT("Label")), TEXT("TOptional<USTRUCT> payload should reflect Label")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("optional member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeBoolSet"), true, TEXT("TOptional<bool> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBoolValue"), false, TEXT("TOptional<bool> should preserve false values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeCountSet"), true, TEXT("TOptional<int> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CountValue"), 71, TEXT("TOptional<int> value should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeFloatSet"), true, TEXT("TOptional<float> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FloatValue"), 12.5, TEXT("TOptional<float> value should round-trip through FDoubleProperty"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeNameSet"), true, TEXT("TOptional<FName> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("OptionalName")), TEXT("TOptional<FName> value should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeStateSet"), true, TEXT("TOptional<UENUM> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StateValue"), 2, TEXT("TOptional<UENUM> value should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeLabelSet"), true, TEXT("TOptional<FString> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LabelValue"), FString(TEXT("OptionalLabel")), TEXT("TOptional<FString> value should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeVectorSet"), true, TEXT("TOptional<FVector> should be set in AS runtime"))));

		FVector VectorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorValue"), VectorValue), TEXT("TOptional<FVector> runtime value should be readable")));
		ASSERT_THAT(IsTrue(VectorValue.Equals(FVector(7, 8, 9), 0.001), TEXT("TOptional<FVector> value should round-trip")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybePayloadSet"), true, TEXT("TOptional<USTRUCT> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PayloadCountValue"), 83, TEXT("TOptional<USTRUCT> int field should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("PayloadLabelValue"), FString(TEXT("PayloadLabel")), TEXT("TOptional<USTRUCT> string field should round-trip"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeObjectSet"), true, TEXT("TOptional<UObject> should be set in AS runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMaybeObjectIsSelf"), true, TEXT("TOptional<UObject> should preserve object identity"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bEmptyCountSet"), false, TEXT("empty TOptional<int> should report unset after Reset"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNullObjectSet"), true, TEXT("TOptional<UObject> should treat an explicit null assignment as set"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bNullObjectValueIsNull"), true, TEXT("TOptional<UObject> should preserve explicit null values"))));

		bool bIsSet = true;
		ASSERT_THAT(IsTrue(GetOptionalIsSetByPath(*TestRunner, Actor, TEXT("MaybeCount"), bIsSet), TEXT("TOptional<int> reflected state should be readable")));
		ASSERT_THAT(IsTrue(bIsSet, TEXT("TOptional<int> reflected state should be set")));
		int32 OptionalCountValue = 0;
		ASSERT_THAT(IsTrue(GetOptionalValueByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MaybeCount"), OptionalCountValue), TEXT("TOptional<int> reflected value should be readable")));
		ASSERT_THAT(AreEqual(71, OptionalCountValue, TEXT("TOptional<int> reflected value should match AS runtime")));
		ASSERT_THAT(IsTrue(GetOptionalIsSetByPath(*TestRunner, Actor, TEXT("NullObject"), bIsSet), TEXT("TOptional<UObject> null reflected state should be readable")));
		ASSERT_THAT(IsTrue(bIsSet, TEXT("TOptional<UObject> explicit null reflected state should be set")));
		ASSERT_THAT(IsTrue(GetOptionalIsSetByPath(*TestRunner, Actor, TEXT("EmptyCount"), bIsSet), TEXT("empty TOptional<int> reflected state should be readable")));
		ASSERT_THAT(IsFalse(bIsSet, TEXT("empty TOptional<int> reflected state should be unset")));
	}

	TEST_METHOD(UClassDelegateMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DelegateMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			delegate int FUClassPropertyComputeDelegate(int Value);
			event void FUClassPropertySignalEvent(int Value);

			UCLASS()
			class ACoverageUClassDelegateMemberActor : AActor
			{
				UPROPERTY()
				FUClassPropertyComputeDelegate OnCompute;

				UPROPERTY()
				FUClassPropertySignalEvent OnPlainSignal;

				UPROPERTY(BlueprintAssignable)
				FUClassPropertySignalEvent OnSignal;

				UPROPERTY(BlueprintCallable)
				FUClassPropertySignalEvent OnCallableSignal;

				UPROPERTY()
				bool bComputeBound = false;

				UPROPERTY()
				bool bPlainSignalBound = false;

				UPROPERTY()
				bool bSignalBound = false;

				UPROPERTY()
				bool bCallableSignalBound = false;

				UPROPERTY()
				int ComputeInput = 0;

				UPROPERTY()
				int ComputeResult = 0;

				UPROPERTY()
				int PlainSignalCount = 0;

				UPROPERTY()
				int SignalCountA = 0;

				UPROPERTY()
				int SignalCountB = 0;

				UPROPERTY()
				int SignalTotal = 0;

				UPROPERTY()
				int CallableSignalCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnCompute.BindUFunction(this, n"HandleCompute");
					bComputeBound = OnCompute.IsBound();
					ComputeResult = OnCompute.Execute(5);

					OnPlainSignal.AddUFunction(this, n"HandlePlainSignal");
					bPlainSignalBound = OnPlainSignal.IsBound();
					OnPlainSignal.Broadcast(2);

					OnSignal.AddUFunction(this, n"HandleSignalA");
					OnSignal.AddUFunction(this, n"HandleSignalB");
					bSignalBound = OnSignal.IsBound();
					OnSignal.Broadcast(3);
					SignalTotal = SignalCountA + SignalCountB;

					OnCallableSignal.AddUFunction(this, n"HandleCallableSignal");
					bCallableSignalBound = OnCallableSignal.IsBound();
					OnCallableSignal.Broadcast(4);
				}

				UFUNCTION()
				int HandleCompute(int Value)
				{
					ComputeInput = Value;
					return Value + 37;
				}

				UFUNCTION()
				void HandleSignalA(int Value)
				{
					SignalCountA += Value;
				}

				UFUNCTION()
				void HandlePlainSignal(int Value)
				{
					PlainSignalCount += Value;
				}

				UFUNCTION()
				void HandleSignalB(int Value)
				{
					SignalCountB += Value * 10;
				}

				UFUNCTION()
				void HandleCallableSignal(int Value)
				{
					CallableSignalCount += Value * 100;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDelegateMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDelegateMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate member actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* ComputeProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnCompute"));
		FMulticastDelegateProperty* PlainSignalProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnPlainSignal"));
		FMulticastDelegateProperty* SignalProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnSignal"));
		FMulticastDelegateProperty* CallableSignalProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnCallableSignal"));
		ASSERT_THAT(IsNotNull(ComputeProperty, TEXT("single-cast delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(PlainSignalProperty, TEXT("plain multicast event member should reflect as FMulticastDelegateProperty")));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("multicast event member should reflect as FMulticastDelegateProperty")));
		ASSERT_THAT(IsNotNull(CallableSignalProperty, TEXT("BlueprintCallable multicast event member should reflect as FMulticastDelegateProperty")));
		if (ComputeProperty == nullptr || PlainSignalProperty == nullptr || SignalProperty == nullptr || CallableSignalProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(ComputeProperty->SignatureFunction, TEXT("single-cast delegate member should keep a signature function")));
		ASSERT_THAT(IsNotNull(PlainSignalProperty->SignatureFunction, TEXT("plain multicast event member should keep a signature function")));
		ASSERT_THAT(IsNotNull(SignalProperty->SignatureFunction, TEXT("multicast event member should keep a signature function")));
		ASSERT_THAT(IsNotNull(CallableSignalProperty->SignatureFunction, TEXT("BlueprintCallable multicast event member should keep a signature function")));
		ASSERT_THAT(IsTrue(PlainSignalProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("plain event member should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(PlainSignalProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable | CPF_BlueprintCallable), TEXT("plain event member should not implicitly gain assignable/callable flags")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("BlueprintAssignable event member should carry CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintAssignable event member should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintCallable), TEXT("BlueprintAssignable-only event member should not implicitly become BlueprintCallable")));
		ASSERT_THAT(IsTrue(CallableSignalProperty->HasAnyPropertyFlags(CPF_BlueprintCallable), TEXT("BlueprintCallable event member should carry CPF_BlueprintCallable")));
		ASSERT_THAT(IsTrue(CallableSignalProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintCallable event member should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(CallableSignalProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("BlueprintCallable-only event member should not implicitly become BlueprintAssignable")));
		if (ComputeProperty->SignatureFunction == nullptr || PlainSignalProperty->SignatureFunction == nullptr
			|| SignalProperty->SignatureFunction == nullptr || CallableSignalProperty->SignatureFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ComputeProperty->SignatureFunction, TEXT("Value")), TEXT("single-cast delegate signature should expose the Value parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(PlainSignalProperty->SignatureFunction, TEXT("Value")), TEXT("plain multicast event signature should expose the Value parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(SignalProperty->SignatureFunction, TEXT("Value")), TEXT("multicast event signature should expose the Value parameter")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(CallableSignalProperty->SignatureFunction, TEXT("Value")), TEXT("BlueprintCallable event signature should expose the Value parameter")));
		FProperty* ComputeReturnProperty = ComputeProperty->SignatureFunction->GetReturnProperty();
		ASSERT_THAT(IsNotNull(ComputeReturnProperty, TEXT("single-cast delegate signature should expose its return property")));
		if (ComputeReturnProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ComputeReturnProperty->IsA<FIntProperty>(), TEXT("single-cast delegate return should reflect as FIntProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComputeBound"), true, TEXT("single-cast delegate member should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeInput"), 5, TEXT("single-cast delegate member should pass its input parameter"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeResult"), 42, TEXT("single-cast delegate member should return the handler result"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPlainSignalBound"), true, TEXT("plain multicast event member should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PlainSignalCount"), 2, TEXT("plain multicast event member should broadcast to its handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSignalBound"), true, TEXT("multicast event member should bind to AS receivers"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalCountA"), 3, TEXT("multicast event member should call the first handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalCountB"), 30, TEXT("multicast event member should call the second handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalTotal"), 33, TEXT("multicast event member should execute the broadcast path"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bCallableSignalBound"), true, TEXT("BlueprintCallable event member should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CallableSignalCount"), 400, TEXT("BlueprintCallable event member should broadcast to its handler"))));
	}

	TEST_METHOD(UClassDelegateReturnMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DelegateReturnMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			delegate bool FUClassPropertyBoolReturnDelegate();
			delegate int FUClassPropertyIntReturnDelegate(int Value);
			delegate float FUClassPropertyFloatReturnDelegate(float Value);
			delegate FString FUClassPropertyStringReturnDelegate();
			delegate FVector FUClassPropertyVectorReturnDelegate();

			UCLASS()
			class ACoverageUClassDelegateReturnActor : AActor
			{
				UPROPERTY()
				FUClassPropertyBoolReturnDelegate BoolReturn;

				UPROPERTY()
				FUClassPropertyIntReturnDelegate IntReturn;

				UPROPERTY()
				FUClassPropertyFloatReturnDelegate FloatReturn;

				UPROPERTY()
				FUClassPropertyStringReturnDelegate StringReturn;

				UPROPERTY()
				FUClassPropertyVectorReturnDelegate VectorReturn;

				UPROPERTY()
				bool bBoolReturnBound = false;

				UPROPERTY()
				bool bBoolReturnValue = false;

				UPROPERTY()
				bool bIntReturnBound = false;

				UPROPERTY()
				int IntReturnInput = 0;

				UPROPERTY()
				int IntReturnValue = 0;

				UPROPERTY()
				bool bFloatReturnBound = false;

				UPROPERTY()
				float FloatReturnInput = 0.0f;

				UPROPERTY()
				float FloatReturnValue = 0.0f;

				UPROPERTY()
				bool bStringReturnBound = false;

				UPROPERTY()
				FString StringReturnValue;

				UPROPERTY()
				bool bVectorReturnBound = false;

				UPROPERTY()
				FVector VectorReturnValue;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BoolReturn.BindUFunction(this, n"HandleBoolReturn");
					bBoolReturnBound = BoolReturn.IsBound();
					bBoolReturnValue = BoolReturn.Execute();

					IntReturn.BindUFunction(this, n"HandleIntReturn");
					bIntReturnBound = IntReturn.IsBound();
					IntReturnValue = IntReturn.Execute(12);

					FloatReturn.BindUFunction(this, n"HandleFloatReturn");
					bFloatReturnBound = FloatReturn.IsBound();
					FloatReturnValue = FloatReturn.Execute(2.0f);

					StringReturn.BindUFunction(this, n"HandleStringReturn");
					bStringReturnBound = StringReturn.IsBound();
					StringReturnValue = StringReturn.Execute();

					VectorReturn.BindUFunction(this, n"HandleVectorReturn");
					bVectorReturnBound = VectorReturn.IsBound();
					VectorReturnValue = VectorReturn.Execute();
				}

				UFUNCTION()
				bool HandleBoolReturn()
				{
					return true;
				}

				UFUNCTION()
				int HandleIntReturn(int Value)
				{
					IntReturnInput = Value;
					return Value * 3;
				}

				UFUNCTION()
				float HandleFloatReturn(float Value)
				{
					FloatReturnInput = Value;
					return Value + 0.75f;
				}

				UFUNCTION()
				FString HandleStringReturn()
				{
					return "UClassDelegateString";
				}

				UFUNCTION()
				FVector HandleVectorReturn()
				{
					return FVector(7.0f, 8.0f, 9.0f);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDelegateReturnMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDelegateReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate return actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* BoolReturnProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("BoolReturn"));
		FDelegateProperty* IntReturnProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("IntReturn"));
		FDelegateProperty* FloatReturnProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("FloatReturn"));
		FDelegateProperty* StringReturnProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("StringReturn"));
		FDelegateProperty* VectorReturnProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("VectorReturn"));
		ASSERT_THAT(IsNotNull(BoolReturnProperty, TEXT("bool-return delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(IntReturnProperty, TEXT("int-return delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(FloatReturnProperty, TEXT("float-return delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(StringReturnProperty, TEXT("FString-return delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(VectorReturnProperty, TEXT("FVector-return delegate member should reflect as FDelegateProperty")));
		if (BoolReturnProperty == nullptr || IntReturnProperty == nullptr || FloatReturnProperty == nullptr
			|| StringReturnProperty == nullptr || VectorReturnProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(BoolReturnProperty->SignatureFunction, TEXT("bool-return delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(IntReturnProperty->SignatureFunction, TEXT("int-return delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(FloatReturnProperty->SignatureFunction, TEXT("float-return delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(StringReturnProperty->SignatureFunction, TEXT("FString-return delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(VectorReturnProperty->SignatureFunction, TEXT("FVector-return delegate should keep a signature function")));
		if (BoolReturnProperty->SignatureFunction == nullptr || IntReturnProperty->SignatureFunction == nullptr
			|| FloatReturnProperty->SignatureFunction == nullptr || StringReturnProperty->SignatureFunction == nullptr
			|| VectorReturnProperty->SignatureFunction == nullptr)
		{
			return;
		}

		FProperty* BoolReturnValueProperty = BoolReturnProperty->SignatureFunction->GetReturnProperty();
		FProperty* IntReturnValueProperty = IntReturnProperty->SignatureFunction->GetReturnProperty();
		FProperty* FloatReturnValueProperty = FloatReturnProperty->SignatureFunction->GetReturnProperty();
		FProperty* StringReturnValueProperty = StringReturnProperty->SignatureFunction->GetReturnProperty();
		FProperty* VectorReturnValueProperty = VectorReturnProperty->SignatureFunction->GetReturnProperty();
		ASSERT_THAT(IsNotNull(BoolReturnValueProperty, TEXT("bool-return delegate signature should expose ReturnValue")));
		ASSERT_THAT(IsNotNull(IntReturnValueProperty, TEXT("int-return delegate signature should expose ReturnValue")));
		ASSERT_THAT(IsNotNull(FloatReturnValueProperty, TEXT("float-return delegate signature should expose ReturnValue")));
		ASSERT_THAT(IsNotNull(StringReturnValueProperty, TEXT("FString-return delegate signature should expose ReturnValue")));
		ASSERT_THAT(IsNotNull(VectorReturnValueProperty, TEXT("FVector-return delegate signature should expose ReturnValue")));
		if (BoolReturnValueProperty == nullptr || IntReturnValueProperty == nullptr || FloatReturnValueProperty == nullptr
			|| StringReturnValueProperty == nullptr || VectorReturnValueProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BoolReturnValueProperty->IsA<FBoolProperty>(), TEXT("bool-return delegate should reflect ReturnValue as FBoolProperty")));
		ASSERT_THAT(IsTrue(IntReturnValueProperty->IsA<FIntProperty>(), TEXT("int-return delegate should reflect ReturnValue as FIntProperty")));
		ASSERT_THAT(IsTrue(FloatReturnValueProperty->IsA<FDoubleProperty>(), TEXT("float-return delegate should reflect ReturnValue as FDoubleProperty")));
		ASSERT_THAT(IsTrue(StringReturnValueProperty->IsA<FStrProperty>(), TEXT("FString-return delegate should reflect ReturnValue as FStrProperty")));
		const FStructProperty* VectorReturnStructProperty = CastField<FStructProperty>(VectorReturnValueProperty);
		ASSERT_THAT(IsNotNull(VectorReturnStructProperty, TEXT("FVector-return delegate should reflect ReturnValue as FStructProperty")));
		if (VectorReturnStructProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VectorReturnStructProperty->Struct != nullptr && VectorReturnStructProperty->Struct->IsChildOf(TBaseStructure<FVector>::Get()), TEXT("FVector-return delegate should target FVector")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBoolReturnBound"), true, TEXT("bool-return delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bBoolReturnValue"), true, TEXT("bool-return delegate should return true"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bIntReturnBound"), true, TEXT("int-return delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntReturnInput"), 12, TEXT("int-return delegate should pass its input"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IntReturnValue"), 36, TEXT("int-return delegate should return the handler result"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bFloatReturnBound"), true, TEXT("float-return delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FloatReturnInput"), 2.0, TEXT("float-return delegate should pass its input"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("FloatReturnValue"), 2.75, TEXT("float-return delegate should return the handler result"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStringReturnBound"), true, TEXT("FString-return delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringReturnValue"), FString(TEXT("UClassDelegateString")), TEXT("FString-return delegate should return the handler result"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bVectorReturnBound"), true, TEXT("FVector-return delegate should bind to the AS receiver"))));

		FVector VectorReturnValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("VectorReturnValue"), VectorReturnValue), TEXT("FVector-return delegate result should be readable")));
		ASSERT_THAT(IsTrue(VectorReturnValue.Equals(FVector(7, 8, 9), 0.001), TEXT("FVector-return delegate should return the handler result")));
	}

	TEST_METHOD(UClassDelegateParameterMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DelegateParameterMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			delegate int FUClassPropertyDelegateParameterCallback(int Value, const FString& Label);

			UCLASS()
			class ACoverageUClassDelegateParameterActor : AActor
			{
				UPROPERTY()
				FUClassPropertyDelegateParameterCallback StoredCallback;

				UPROPERTY()
				FUClassPropertyDelegateParameterCallback CopiedCallback;

				UPROPERTY()
				int ValueParameterResult = 0;

				UPROPERTY()
				int ConstRefParameterResult = 0;

				UPROPERTY()
				int CopiedCallbackResult = 0;

				UPROPERTY()
				int HandlerInputTotal = 0;

				UPROPERTY()
				FString LastHandlerLabel;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StoredCallback.BindUFunction(this, n"HandleCallback");
					CopiedCallback = StoredCallback;
					ValueParameterResult = ConsumeCallbackByValue(StoredCallback);
					ConstRefParameterResult = ConsumeCallbackByConstRef(StoredCallback);
					CopiedCallbackResult = ConsumeCallbackByValue(CopiedCallback);
				}

				UFUNCTION(BlueprintCallable)
				int ConsumeCallbackByValue(FUClassPropertyDelegateParameterCallback Callback)
				{
					if (!Callback.IsBound())
					{
						return -1;
					}

					return Callback.Execute(11, "ValueParameter");
				}

				UFUNCTION(BlueprintCallable)
				int ConsumeCallbackByConstRef(const FUClassPropertyDelegateParameterCallback&in Callback)
				{
					if (!Callback.IsBound())
					{
						return -2;
					}

					return Callback.Execute(17, "ConstRefParameter");
				}

				UFUNCTION(BlueprintCallable)
				int ExerciseUnboundCallback()
				{
					FUClassPropertyDelegateParameterCallback EmptyCallback;
					return ConsumeCallbackByValue(EmptyCallback);
				}

				UFUNCTION()
				int HandleCallback(int Value, const FString&in Label)
				{
					HandlerInputTotal += Value;
					LastHandlerLabel = Label;
					return Value + Label.Len();
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDelegateParameterMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDelegateParameterActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate parameter actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* StoredCallbackProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("StoredCallback"));
		FDelegateProperty* CopiedCallbackProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("CopiedCallback"));
		UFunction* ConsumeByValueFunction = ScriptClass->FindFunctionByName(TEXT("ConsumeCallbackByValue"));
		UFunction* ConsumeByConstRefFunction = ScriptClass->FindFunctionByName(TEXT("ConsumeCallbackByConstRef"));
		UFunction* HandlerFunction = ScriptClass->FindFunctionByName(TEXT("HandleCallback"));
		UFunction* ExerciseUnboundFunction = ScriptClass->FindFunctionByName(TEXT("ExerciseUnboundCallback"));
		ASSERT_THAT(IsNotNull(StoredCallbackProperty, TEXT("stored delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(CopiedCallbackProperty, TEXT("copied delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(ConsumeByValueFunction, TEXT("delegate value-parameter consumer should generate a UFUNCTION")));
		ASSERT_THAT(IsNotNull(ConsumeByConstRefFunction, TEXT("delegate const-ref consumer should generate a UFUNCTION")));
		ASSERT_THAT(IsNotNull(HandlerFunction, TEXT("delegate callback handler should generate a UFUNCTION")));
		ASSERT_THAT(IsNotNull(ExerciseUnboundFunction, TEXT("unbound callback exercise function should generate a UFUNCTION")));
		if (StoredCallbackProperty == nullptr || CopiedCallbackProperty == nullptr || ConsumeByValueFunction == nullptr
			|| ConsumeByConstRefFunction == nullptr || HandlerFunction == nullptr || ExerciseUnboundFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(StoredCallbackProperty->SignatureFunction, TEXT("stored delegate member should keep a signature function")));
		ASSERT_THAT(IsNotNull(CopiedCallbackProperty->SignatureFunction, TEXT("copied delegate member should keep a signature function")));
		if (StoredCallbackProperty->SignatureFunction == nullptr || CopiedCallbackProperty->SignatureFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(StoredCallbackProperty->SignatureFunction, CopiedCallbackProperty->SignatureFunction,
			TEXT("delegate members of the same AS delegate type should share the signature function")));

		FDelegateProperty* ValueCallbackParam = FindFProperty<FDelegateProperty>(ConsumeByValueFunction, TEXT("Callback"));
		FDelegateProperty* ConstRefCallbackParam = FindFProperty<FDelegateProperty>(ConsumeByConstRefFunction, TEXT("Callback"));
		FIntProperty* ValueReturnProperty = FindFProperty<FIntProperty>(ConsumeByValueFunction, TEXT("ReturnValue"));
		FIntProperty* ConstRefReturnProperty = FindFProperty<FIntProperty>(ConsumeByConstRefFunction, TEXT("ReturnValue"));
		FIntProperty* HandlerValueParam = FindFProperty<FIntProperty>(HandlerFunction, TEXT("Value"));
		FStrProperty* HandlerLabelParam = FindFProperty<FStrProperty>(HandlerFunction, TEXT("Label"));
		FIntProperty* HandlerReturnProperty = FindFProperty<FIntProperty>(HandlerFunction, TEXT("ReturnValue"));
		FIntProperty* UnboundReturnProperty = FindFProperty<FIntProperty>(ExerciseUnboundFunction, TEXT("ReturnValue"));
		ASSERT_THAT(IsNotNull(ValueCallbackParam, TEXT("delegate by-value UFUNCTION parameter should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(ConstRefCallbackParam, TEXT("delegate const-ref UFUNCTION parameter should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(ValueReturnProperty, TEXT("delegate by-value consumer should expose int ReturnValue")));
		ASSERT_THAT(IsNotNull(ConstRefReturnProperty, TEXT("delegate const-ref consumer should expose int ReturnValue")));
		ASSERT_THAT(IsNotNull(HandlerValueParam, TEXT("delegate handler should expose int Value")));
		ASSERT_THAT(IsNotNull(HandlerLabelParam, TEXT("delegate handler should expose FString Label")));
		ASSERT_THAT(IsNotNull(HandlerReturnProperty, TEXT("delegate handler should expose int ReturnValue")));
		ASSERT_THAT(IsNotNull(UnboundReturnProperty, TEXT("unbound callback exercise should expose int ReturnValue")));
		if (ValueCallbackParam == nullptr || ConstRefCallbackParam == nullptr || HandlerLabelParam == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(StoredCallbackProperty->SignatureFunction, ValueCallbackParam->SignatureFunction,
			TEXT("delegate by-value parameter should reuse the member delegate signature function")));
		ASSERT_THAT(AreEqual(StoredCallbackProperty->SignatureFunction, ConstRefCallbackParam->SignatureFunction,
			TEXT("delegate const-ref parameter should reuse the member delegate signature function")));
		ASSERT_THAT(IsTrue(ConstRefCallbackParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("delegate const-ref parameter should carry const/out/reference flags")));
		ASSERT_THAT(IsFalse(ValueCallbackParam->HasAnyPropertyFlags(CPF_ReferenceParm),
			TEXT("delegate by-value parameter should not carry reference flags")));
		ASSERT_THAT(IsTrue(HandlerLabelParam->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm | CPF_ReferenceParm),
			TEXT("const FString&in delegate handler parameter should carry const/out/reference flags")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StoredCallbackProperty->SignatureFunction, TEXT("Value")),
			TEXT("delegate member signature should expose int Value")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(StoredCallbackProperty->SignatureFunction, TEXT("Label")),
			TEXT("delegate member signature should expose FString Label")));
		ASSERT_THAT(IsNotNull(StoredCallbackProperty->SignatureFunction->GetReturnProperty(),
			TEXT("delegate member signature should expose ReturnValue")));
		ASSERT_THAT(IsTrue(StoredCallbackProperty->SignatureFunction->GetReturnProperty()->IsA<FIntProperty>(),
			TEXT("delegate member signature ReturnValue should be FIntProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ValueParameterResult"), 25,
			TEXT("delegate member passed by value should execute the bound callback"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ConstRefParameterResult"), 34,
			TEXT("delegate member passed by const-ref should execute the bound callback"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopiedCallbackResult"), 25,
			TEXT("copied delegate member should remain bound when passed by value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("HandlerInputTotal"), 39,
			TEXT("delegate parameter executions should reach the same AS handler"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastHandlerLabel"), FString(TEXT("ValueParameter")),
			TEXT("delegate parameter execution should pass the latest FString payload"))));

		FFunctionInvoker UnboundInvoker(*TestRunner, Actor, TEXT("ExerciseUnboundCallback"));
		ASSERT_THAT(IsTrue(UnboundInvoker.IsValid(), TEXT("unbound callback exercise should be invokable")));
		if (!UnboundInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(-1, UnboundInvoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("unbound delegate parameter should follow the guarded failure path")));
	}

	TEST_METHOD(UClassDelegateTypedPayloadMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DelegateTypedPayloadMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EUClassPropertyDelegateTypedState
			{
				Idle,
				Armed,
				Fired
			}

			delegate int FUClassPropertyTypedComputeDelegate(bool bEnabled, float Weight, const FString& Label, FName Tag, FVector Location, AActor ActorValue, EUClassPropertyDelegateTypedState State);
			event void FUClassPropertyTypedSignal(bool bEnabled, float Weight, const FString& Label, FName Tag, FVector Location, AActor ActorValue, EUClassPropertyDelegateTypedState State);

			UCLASS()
			class ACoverageUClassDelegateTypedPayloadActor : AActor
			{
				UPROPERTY()
				FUClassPropertyTypedComputeDelegate OnTypedCompute;

				UPROPERTY(BlueprintAssignable)
				FUClassPropertyTypedSignal OnTypedSignal;

				UPROPERTY()
				bool bComputeBound = false;

				UPROPERTY()
				bool bSignalBound = false;

				UPROPERTY()
				bool bComputeEnabled = false;

				UPROPERTY()
				float ComputeWeight = 0.0;

				UPROPERTY()
				FString ComputeLabel;

				UPROPERTY()
				FName ComputeTag;

				UPROPERTY()
				FVector ComputeLocation;

				UPROPERTY()
				bool bComputeActorWasSelf = false;

				UPROPERTY()
				int ComputeState = 0;

				UPROPERTY()
				int ComputeResult = 0;

				UPROPERTY()
				bool bSignalEnabled = false;

				UPROPERTY()
				float SignalWeight = 0.0;

				UPROPERTY()
				FString SignalLabel;

				UPROPERTY()
				FName SignalTag;

				UPROPERTY()
				FVector SignalLocation;

				UPROPERTY()
				bool bSignalActorWasSelf = false;

				UPROPERTY()
				int SignalState = 0;

				UPROPERTY()
				int SignalResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnTypedCompute.BindUFunction(this, n"HandleTypedCompute");
					bComputeBound = OnTypedCompute.IsBound();
					ComputeResult = OnTypedCompute.Execute(
						true,
						2.5f,
						"ComputeLabel",
						n"ComputeTag",
						FVector(1, 2, 3),
						this,
						EUClassPropertyDelegateTypedState::Armed);

					OnTypedSignal.AddUFunction(this, n"HandleTypedSignal");
					bSignalBound = OnTypedSignal.IsBound();
					OnTypedSignal.Broadcast(
						false,
						4.25f,
						"SignalLabel",
						n"SignalTag",
						FVector(4, 5, 6),
						this,
						EUClassPropertyDelegateTypedState::Fired);
				}

				UFUNCTION()
				int HandleTypedCompute(bool bEnabled, float Weight, const FString& Label, FName Tag, FVector Location, AActor ActorValue, EUClassPropertyDelegateTypedState State)
				{
					bComputeEnabled = bEnabled;
					ComputeWeight = Weight;
					ComputeLabel = Label;
					ComputeTag = Tag;
					ComputeLocation = Location;
					bComputeActorWasSelf = ActorValue == this;
					ComputeState = int(State);
					return int(Weight * 10.0f) + int(State);
				}

				UFUNCTION()
				void HandleTypedSignal(bool bEnabled, float Weight, const FString& Label, FName Tag, FVector Location, AActor ActorValue, EUClassPropertyDelegateTypedState State)
				{
					bSignalEnabled = bEnabled;
					SignalWeight = Weight;
					SignalLabel = Label;
					SignalTag = Tag;
					SignalLocation = Location;
					bSignalActorWasSelf = ActorValue == this;
					SignalState = int(State);
					SignalResult = int(Weight * 10.0f) + int(State);
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDelegateTypedPayloadMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDelegateTypedPayloadActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate typed-payload actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* ComputeProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnTypedCompute"));
		FMulticastDelegateProperty* SignalProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnTypedSignal"));
		ASSERT_THAT(IsNotNull(ComputeProperty, TEXT("typed single-cast delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("typed multicast event member should reflect as FMulticastDelegateProperty")));
		if (ComputeProperty == nullptr || SignalProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(ComputeProperty->SignatureFunction, TEXT("typed single-cast delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(SignalProperty->SignatureFunction, TEXT("typed multicast event should keep a signature function")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("typed multicast event should carry CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("typed multicast event should be Blueprint-visible")));
		if (ComputeProperty->SignatureFunction == nullptr || SignalProperty->SignatureFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(ComputeProperty->SignatureFunction, TEXT("bEnabled")), TEXT("typed single-cast delegate signature should expose bool bEnabled")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(ComputeProperty->SignatureFunction, TEXT("Weight")), TEXT("typed single-cast delegate signature should expose float Weight as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(ComputeProperty->SignatureFunction, TEXT("Label")), TEXT("typed single-cast delegate signature should expose FString Label")));
		ASSERT_THAT(IsNotNull(FindFProperty<FNameProperty>(ComputeProperty->SignatureFunction, TEXT("Tag")), TEXT("typed single-cast delegate signature should expose FName Tag")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(ComputeProperty->SignatureFunction, TEXT("Location")), TEXT("typed single-cast delegate signature should expose FVector Location")));
		ASSERT_THAT(IsNotNull(FindFProperty<FObjectPropertyBase>(ComputeProperty->SignatureFunction, TEXT("ActorValue")), TEXT("typed single-cast delegate signature should expose AActor ActorValue")));
		ASSERT_THAT(IsNotNull(FindFProperty<FEnumProperty>(ComputeProperty->SignatureFunction, TEXT("State")), TEXT("typed single-cast delegate signature should expose UENUM State")));
		ASSERT_THAT(IsNotNull(FindFProperty<FBoolProperty>(SignalProperty->SignatureFunction, TEXT("bEnabled")), TEXT("typed multicast event signature should expose bool bEnabled")));
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(SignalProperty->SignatureFunction, TEXT("Weight")), TEXT("typed multicast event signature should expose float Weight as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(SignalProperty->SignatureFunction, TEXT("Label")), TEXT("typed multicast event signature should expose FString Label")));
		ASSERT_THAT(IsNotNull(FindFProperty<FNameProperty>(SignalProperty->SignatureFunction, TEXT("Tag")), TEXT("typed multicast event signature should expose FName Tag")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(SignalProperty->SignatureFunction, TEXT("Location")), TEXT("typed multicast event signature should expose FVector Location")));
		ASSERT_THAT(IsNotNull(FindFProperty<FObjectPropertyBase>(SignalProperty->SignatureFunction, TEXT("ActorValue")), TEXT("typed multicast event signature should expose AActor ActorValue")));
		ASSERT_THAT(IsNotNull(FindFProperty<FEnumProperty>(SignalProperty->SignatureFunction, TEXT("State")), TEXT("typed multicast event signature should expose UENUM State")));
		FProperty* ComputeReturnProperty = ComputeProperty->SignatureFunction->GetReturnProperty();
		ASSERT_THAT(IsNotNull(ComputeReturnProperty, TEXT("typed single-cast delegate signature should expose ReturnValue")));
		if (ComputeReturnProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ComputeReturnProperty->IsA<FIntProperty>(), TEXT("typed single-cast delegate return should reflect as FIntProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate typed-payload actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComputeBound"), true, TEXT("typed single-cast delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComputeEnabled"), true, TEXT("typed single-cast delegate should pass bool parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("ComputeWeight"), 2.5, TEXT("typed single-cast delegate should pass float parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ComputeLabel"), FString(TEXT("ComputeLabel")), TEXT("typed single-cast delegate should pass FString parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("ComputeTag"), FName(TEXT("ComputeTag")), TEXT("typed single-cast delegate should pass FName parameters"))));
		FVector ComputeLocation;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("ComputeLocation"), ComputeLocation), TEXT("typed single-cast delegate FVector parameter should be readable")));
		ASSERT_THAT(IsTrue(ComputeLocation.Equals(FVector(1, 2, 3), 0.001), TEXT("typed single-cast delegate should pass FVector parameters")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComputeActorWasSelf"), true, TEXT("typed single-cast delegate should pass AActor handle parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeState"), 1, TEXT("typed single-cast delegate should pass UENUM parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeResult"), 26, TEXT("typed single-cast delegate should return handler result"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSignalBound"), true, TEXT("typed multicast event should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSignalEnabled"), false, TEXT("typed multicast event should pass bool parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("SignalWeight"), 4.25, TEXT("typed multicast event should pass float parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("SignalLabel"), FString(TEXT("SignalLabel")), TEXT("typed multicast event should pass FString parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("SignalTag"), FName(TEXT("SignalTag")), TEXT("typed multicast event should pass FName parameters"))));
		FVector SignalLocation;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("SignalLocation"), SignalLocation), TEXT("typed multicast event FVector parameter should be readable")));
		ASSERT_THAT(IsTrue(SignalLocation.Equals(FVector(4, 5, 6), 0.001), TEXT("typed multicast event should pass FVector parameters")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSignalActorWasSelf"), true, TEXT("typed multicast event should pass AActor handle parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalState"), 2, TEXT("typed multicast event should pass UENUM parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalResult"), 44, TEXT("typed multicast event should execute handler path"))));
	}

	TEST_METHOD(UClassDelegateStructPayloadMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DelegateStructPayloadMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUClassPropertyDelegatePayload
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				int Bonus = 0;

				UPROPERTY()
				FName Tag;
			}

			delegate int FUClassPropertyPayloadComputeDelegate(FUClassPropertyDelegatePayload Payload);
			event void FUClassPropertyPayloadEvent(FUClassPropertyDelegatePayload Payload);

			UCLASS()
			class ACoverageUClassDelegateStructPayloadActor : AActor
			{
				UPROPERTY()
				FUClassPropertyPayloadComputeDelegate OnPayloadCompute;

				UPROPERTY(BlueprintAssignable)
				FUClassPropertyPayloadEvent OnPayloadSignal;

				UPROPERTY()
				bool bPayloadComputeBound = false;

				UPROPERTY()
				bool bPayloadSignalBound = false;

				UPROPERTY()
				int ComputePayloadValue = 0;

				UPROPERTY()
				int ComputePayloadBonus = 0;

				UPROPERTY()
				FName ComputePayloadTag;

				UPROPERTY()
				int ComputePayloadResult = 0;

				UPROPERTY()
				int SignalPayloadValue = 0;

				UPROPERTY()
				int SignalPayloadBonus = 0;

				UPROPERTY()
				FName SignalPayloadTag;

				UPROPERTY()
				int SignalPayloadResult = 0;

				FUClassPropertyDelegatePayload MakePayload(int Value, int Bonus, FName Tag)
				{
					FUClassPropertyDelegatePayload Payload;
					Payload.Value = Value;
					Payload.Bonus = Bonus;
					Payload.Tag = Tag;
					return Payload;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					OnPayloadCompute.BindUFunction(this, n"HandlePayloadCompute");
					bPayloadComputeBound = OnPayloadCompute.IsBound();
					ComputePayloadResult = OnPayloadCompute.Execute(MakePayload(19, 23, n"Compute"));

					OnPayloadSignal.AddUFunction(this, n"HandlePayloadSignal");
					bPayloadSignalBound = OnPayloadSignal.IsBound();
					OnPayloadSignal.Broadcast(MakePayload(29, 31, n"Signal"));
				}

				UFUNCTION()
				int HandlePayloadCompute(FUClassPropertyDelegatePayload Payload)
				{
					ComputePayloadValue = Payload.Value;
					ComputePayloadBonus = Payload.Bonus;
					ComputePayloadTag = Payload.Tag;
					return Payload.Value + Payload.Bonus;
				}

				UFUNCTION()
				void HandlePayloadSignal(FUClassPropertyDelegatePayload Payload)
				{
					SignalPayloadValue = Payload.Value;
					SignalPayloadBonus = Payload.Bonus;
					SignalPayloadTag = Payload.Tag;
					SignalPayloadResult = Payload.Value + Payload.Bonus;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDelegateStructPayloadMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDelegateStructPayloadActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate struct-payload actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* ComputeProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnPayloadCompute"));
		FMulticastDelegateProperty* SignalProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnPayloadSignal"));
		ASSERT_THAT(IsNotNull(ComputeProperty, TEXT("single-cast struct-payload delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("multicast struct-payload event member should reflect as FMulticastDelegateProperty")));
		if (ComputeProperty == nullptr || SignalProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(ComputeProperty->SignatureFunction, TEXT("single-cast struct-payload delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(SignalProperty->SignatureFunction, TEXT("multicast struct-payload event should keep a signature function")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("struct-payload event should carry CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("struct-payload event should be Blueprint-visible")));
		if (ComputeProperty->SignatureFunction == nullptr || SignalProperty->SignatureFunction == nullptr)
		{
			return;
		}

		FStructProperty* ComputePayloadProperty = FindFProperty<FStructProperty>(ComputeProperty->SignatureFunction, TEXT("Payload"));
		FStructProperty* SignalPayloadProperty = FindFProperty<FStructProperty>(SignalProperty->SignatureFunction, TEXT("Payload"));
		FProperty* ComputeReturnProperty = ComputeProperty->SignatureFunction->GetReturnProperty();
		ASSERT_THAT(IsNotNull(ComputePayloadProperty, TEXT("single-cast struct-payload delegate signature should expose Payload")));
		ASSERT_THAT(IsNotNull(SignalPayloadProperty, TEXT("multicast struct-payload event signature should expose Payload")));
		ASSERT_THAT(IsNotNull(ComputeReturnProperty, TEXT("single-cast struct-payload delegate signature should expose ReturnValue")));
		if (ComputePayloadProperty == nullptr || SignalPayloadProperty == nullptr || ComputeReturnProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ComputeReturnProperty->IsA<FIntProperty>(), TEXT("single-cast struct-payload delegate return should reflect as FIntProperty")));
		ASSERT_THAT(AreEqual(ComputePayloadProperty->Struct, SignalPayloadProperty->Struct, TEXT("single-cast and multicast struct-payload delegates should share payload type identity")));
		ASSERT_THAT(IsNotNull(ComputePayloadProperty->Struct, TEXT("struct-payload delegate signature should keep payload UScriptStruct")));
		if (ComputePayloadProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ComputePayloadProperty->Struct, TEXT("Value")), TEXT("struct-payload delegate payload should expose Value")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ComputePayloadProperty->Struct, TEXT("Bonus")), TEXT("struct-payload delegate payload should expose Bonus")));
		ASSERT_THAT(IsNotNull(FindFProperty<FNameProperty>(ComputePayloadProperty->Struct, TEXT("Tag")), TEXT("struct-payload delegate payload should expose Tag")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate struct-payload actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPayloadComputeBound"), true, TEXT("single-cast struct-payload delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputePayloadValue"), 19, TEXT("single-cast struct-payload delegate should pass Value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputePayloadBonus"), 23, TEXT("single-cast struct-payload delegate should pass Bonus"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("ComputePayloadTag"), FName(TEXT("Compute")), TEXT("single-cast struct-payload delegate should pass Tag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputePayloadResult"), 42, TEXT("single-cast struct-payload delegate should return handler result"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bPayloadSignalBound"), true, TEXT("multicast struct-payload event should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalPayloadValue"), 29, TEXT("multicast struct-payload event should pass Value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalPayloadBonus"), 31, TEXT("multicast struct-payload event should pass Bonus"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("SignalPayloadTag"), FName(TEXT("Signal")), TEXT("multicast struct-payload event should pass Tag"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalPayloadResult"), 60, TEXT("multicast struct-payload event should execute handler path"))));
	}

	TEST_METHOD(UClassDelegateContainerPayloadMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DelegateContainerPayloadMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			delegate int FUClassPropertyContainerComputeDelegate(TArray<int> Values, TArray<FVector> Vectors, TMap<FString, int> Scores, TSet<FName> Tags);
			event void FUClassPropertyContainerEvent(TArray<int> Values, TMap<FName, int> Scores);

			UCLASS()
			class ACoverageUClassDelegateContainerPayloadActor : AActor
			{
				UPROPERTY()
				FUClassPropertyContainerComputeDelegate OnContainerCompute;

				UPROPERTY(BlueprintAssignable)
				FUClassPropertyContainerEvent OnContainerSignal;

				UPROPERTY()
				bool bContainerComputeBound = false;

				UPROPERTY()
				bool bContainerSignalBound = false;

				UPROPERTY()
				int ComputeValueCount = 0;

				UPROPERTY()
				int ComputeVectorCount = 0;

				UPROPERTY()
				int ComputeScoreCount = 0;

				UPROPERTY()
				int ComputeTagCount = 0;

				UPROPERTY()
				bool bComputeHasReadyTag = false;

				UPROPERTY()
				int ComputeResult = 0;

				UPROPERTY()
				int SignalValueCount = 0;

				UPROPERTY()
				int SignalScoreCount = 0;

				UPROPERTY()
				int SignalScoreValue = 0;

				UPROPERTY()
				int SignalTotal = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TArray<int> Values;
					Values.Add(5);
					Values.Add(6);

					TArray<FVector> Vectors;
					Vectors.Add(FVector(1, 0, 0));
					Vectors.Add(FVector(0, 2, 0));

					TMap<FString, int> Scores;
					Scores.Add("Alpha", 7);
					Scores.Add("Beta", 8);

					TSet<FName> Tags;
					Tags.Add(n"Ready");
					Tags.Add(n"Live");

					OnContainerCompute.BindUFunction(this, n"HandleContainerCompute");
					bContainerComputeBound = OnContainerCompute.IsBound();
					ComputeResult = OnContainerCompute.Execute(Values, Vectors, Scores, Tags);

					TMap<FName, int> SignalScores;
					SignalScores.Add(n"First", 13);
					SignalScores.Add(n"Second", 17);

					OnContainerSignal.AddUFunction(this, n"HandleContainerSignal");
					bContainerSignalBound = OnContainerSignal.IsBound();
					OnContainerSignal.Broadcast(Values, SignalScores);
				}

				UFUNCTION()
				int HandleContainerCompute(TArray<int> Values, TArray<FVector> Vectors, TMap<FString, int> Scores, TSet<FName> Tags)
				{
					ComputeValueCount = Values.Num();
					ComputeVectorCount = Vectors.Num();
					ComputeScoreCount = Scores.Num();
					ComputeTagCount = Tags.Num();
					bComputeHasReadyTag = Tags.Contains(n"Ready");

					int AlphaScore = 0;
					int BetaScore = 0;
					Scores.Find("Alpha", AlphaScore);
					Scores.Find("Beta", BetaScore);

					return Values[0] + Values[1] + int(Vectors[0].X) + int(Vectors[1].Y) + AlphaScore + BetaScore + (bComputeHasReadyTag ? 100 : 0);
				}

				UFUNCTION()
				void HandleContainerSignal(TArray<int> Values, TMap<FName, int> Scores)
				{
					SignalValueCount = Values.Num();
					SignalScoreCount = Scores.Num();
					Scores.Find(n"Second", SignalScoreValue);
					SignalTotal = Values[0] + Values[1] + SignalScoreValue;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDelegateContainerPayloadMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDelegateContainerPayloadActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("delegate container-payload actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* ComputeProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OnContainerCompute"));
		FMulticastDelegateProperty* SignalProperty = FindFProperty<FMulticastDelegateProperty>(ScriptClass, TEXT("OnContainerSignal"));
		ASSERT_THAT(IsNotNull(ComputeProperty, TEXT("single-cast container delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("multicast container event member should reflect as FMulticastDelegateProperty")));
		if (ComputeProperty == nullptr || SignalProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(ComputeProperty->SignatureFunction, TEXT("single-cast container delegate should keep a signature function")));
		ASSERT_THAT(IsNotNull(SignalProperty->SignatureFunction, TEXT("multicast container event should keep a signature function")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable), TEXT("container event should carry CPF_BlueprintAssignable")));
		ASSERT_THAT(IsTrue(SignalProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("container event should be Blueprint-visible")));
		if (ComputeProperty->SignatureFunction == nullptr || SignalProperty->SignatureFunction == nullptr)
		{
			return;
		}

		FArrayProperty* ComputeValuesProperty = FindFProperty<FArrayProperty>(ComputeProperty->SignatureFunction, TEXT("Values"));
		FArrayProperty* ComputeVectorsProperty = FindFProperty<FArrayProperty>(ComputeProperty->SignatureFunction, TEXT("Vectors"));
		FMapProperty* ComputeScoresProperty = FindFProperty<FMapProperty>(ComputeProperty->SignatureFunction, TEXT("Scores"));
		FSetProperty* ComputeTagsProperty = FindFProperty<FSetProperty>(ComputeProperty->SignatureFunction, TEXT("Tags"));
		FProperty* ComputeReturnProperty = ComputeProperty->SignatureFunction->GetReturnProperty();
		FArrayProperty* SignalValuesProperty = FindFProperty<FArrayProperty>(SignalProperty->SignatureFunction, TEXT("Values"));
		FMapProperty* SignalScoresProperty = FindFProperty<FMapProperty>(SignalProperty->SignatureFunction, TEXT("Scores"));
		ASSERT_THAT(IsNotNull(ComputeValuesProperty, TEXT("container delegate should expose TArray<int> Values")));
		ASSERT_THAT(IsNotNull(ComputeVectorsProperty, TEXT("container delegate should expose TArray<FVector> Vectors")));
		ASSERT_THAT(IsNotNull(ComputeScoresProperty, TEXT("container delegate should expose TMap<FString,int> Scores")));
		ASSERT_THAT(IsNotNull(ComputeTagsProperty, TEXT("container delegate should expose TSet<FName> Tags")));
		ASSERT_THAT(IsNotNull(ComputeReturnProperty, TEXT("container delegate should expose ReturnValue")));
		ASSERT_THAT(IsNotNull(SignalValuesProperty, TEXT("container event should expose TArray<int> Values")));
		ASSERT_THAT(IsNotNull(SignalScoresProperty, TEXT("container event should expose TMap<FName,int> Scores")));
		if (ComputeValuesProperty == nullptr || ComputeVectorsProperty == nullptr || ComputeScoresProperty == nullptr
			|| ComputeTagsProperty == nullptr || ComputeReturnProperty == nullptr || SignalValuesProperty == nullptr || SignalScoresProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(ComputeValuesProperty->Inner), TEXT("delegate TArray<int> inner should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(ComputeVectorsProperty->Inner), TEXT("delegate TArray<FVector> inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(ComputeScoresProperty->KeyProp), TEXT("delegate TMap<FString,int> key should be FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(ComputeScoresProperty->ValueProp), TEXT("delegate TMap<FString,int> value should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(ComputeTagsProperty->ElementProp), TEXT("delegate TSet<FName> element should be FNameProperty")));
		ASSERT_THAT(IsTrue(ComputeReturnProperty->IsA<FIntProperty>(), TEXT("single-cast container delegate return should reflect as FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(SignalValuesProperty->Inner), TEXT("event TArray<int> inner should be FIntProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(SignalScoresProperty->KeyProp), TEXT("event TMap<FName,int> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(SignalScoresProperty->ValueProp), TEXT("event TMap<FName,int> value should be FIntProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("delegate container-payload actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainerComputeBound"), true, TEXT("single-cast container delegate should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeValueCount"), 2, TEXT("container delegate should pass array values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeVectorCount"), 2, TEXT("container delegate should pass vector arrays"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeScoreCount"), 2, TEXT("container delegate should pass string-keyed maps"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeTagCount"), 2, TEXT("container delegate should pass name sets"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bComputeHasReadyTag"), true, TEXT("container delegate should preserve set membership"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ComputeResult"), 129, TEXT("container delegate should return the computed handler result"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bContainerSignalBound"), true, TEXT("multicast container event should bind to the AS receiver"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalValueCount"), 2, TEXT("container event should pass array values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalScoreCount"), 2, TEXT("container event should pass name-keyed maps"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalScoreValue"), 17, TEXT("container event should preserve map lookup values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SignalTotal"), 28, TEXT("container event should execute the handler path"))));
	}

	TEST_METHOD(UClassDefaultValueAndCDOMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_DefaultValueAndCDOMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassPropertyDefaultBaseActor : AActor
			{
				default Tags.Add(n"BaseDefaultTag");

				UPROPERTY()
				int Health = 100;

				UPROPERTY()
				FString Label = "BaseLabel";

				UPROPERTY()
				FName StateName = n"BaseState";

				UPROPERTY()
				FVector SpawnOffset = FVector(1, 2, 3);

				UPROPERTY()
				TArray<FName> DefaultNames;

				UPROPERTY()
				TSet<FName> DefaultFlags;

				UPROPERTY()
				TMap<FName, int> DefaultScores;

				default DefaultNames.Add(n"BaseName");
				default DefaultFlags.Add(n"BaseFlag");
				default DefaultScores.Add(n"BaseScore", 101);
			}

			UCLASS()
			class ACoverageUClassPropertyDefaultLeafActor : ACoverageUClassPropertyDefaultBaseActor
			{
				default Health = 250;
				default Label = "LeafLabel";
				default SpawnOffset = FVector(4, 5, 6);
				default Tags.Add(n"LeafDefaultTag");
				default DefaultNames.Add(n"LeafName");
				default DefaultFlags.Add(n"LeafFlag");
				default DefaultScores.Add(n"LeafScore", 202);
				default SetReplicates(true);

				UPROPERTY()
				int DefaultIntZero;

				UPROPERTY()
				bool bDefaultBoolFalse;

				UPROPERTY()
				FString DefaultStringEmpty;

				UPROPERTY()
				FName DefaultNameNone;

				UPROPERTY()
				FVector DefaultVectorZero;

				UPROPERTY()
				UObject DefaultObjectNull;

				UPROPERTY()
				TArray<int> EmptyDefaultNumbers;

				UPROPERTY()
				TSet<FName> EmptyDefaultNames;

				UPROPERTY()
				TMap<FName, int> EmptyDefaultScores;

				UPROPERTY()
				TSubclassOf<AActor> NativeActorClass = AActor::StaticClass();

				UPROPERTY()
				TSubclassOf<ACoverageUClassPropertyDefaultBaseActor> ScriptActorClass = ACoverageUClassPropertyDefaultBaseActor::StaticClass();

				UPROPERTY()
				int RuntimeHealth = 0;

				UPROPERTY()
				FString RuntimeLabel;

				UPROPERTY()
				FName RuntimeStateName;

				UPROPERTY()
				FVector RuntimeSpawnOffset;

				UPROPERTY()
				int RuntimeNameCount = 0;

				UPROPERTY()
				int RuntimeFlagCount = 0;

				UPROPERTY()
				int RuntimeBaseScore = 0;

				UPROPERTY()
				int RuntimeLeafScore = 0;

				UPROPERTY()
				bool bRuntimeHasBaseName = false;

				UPROPERTY()
				bool bRuntimeHasLeafName = false;

				UPROPERTY()
				bool bRuntimeHasBaseFlag = false;

				UPROPERTY()
				bool bRuntimeHasLeafFlag = false;

				UPROPERTY()
				bool bRuntimeReplicates = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RuntimeHealth = Health;
					RuntimeLabel = Label;
					RuntimeStateName = StateName;
					RuntimeSpawnOffset = SpawnOffset;
					RuntimeNameCount = DefaultNames.Num();
					RuntimeFlagCount = DefaultFlags.Num();
					DefaultScores.Find(n"BaseScore", RuntimeBaseScore);
					DefaultScores.Find(n"LeafScore", RuntimeLeafScore);
					bRuntimeHasBaseName = DefaultNames.Contains(n"BaseName");
					bRuntimeHasLeafName = DefaultNames.Contains(n"LeafName");
					bRuntimeHasBaseFlag = DefaultFlags.Contains(n"BaseFlag");
					bRuntimeHasLeafFlag = DefaultFlags.Contains(n"LeafFlag");
					bRuntimeReplicates = GetIsReplicated();
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyDefaultValueAndCDOMatrix.as"), ScriptSource)));

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPropertyDefaultBaseActor"));
		UClass* LeafClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPropertyDefaultLeafActor"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("default base actor should be generated")));
		ASSERT_THAT(IsNotNull(LeafClass, TEXT("default leaf actor should be generated")));
		if (BaseClass == nullptr || LeafClass == nullptr)
		{
			return;
		}

		AActor* BaseCDO = Cast<AActor>(BaseClass->GetDefaultObject());
		AActor* LeafCDO = Cast<AActor>(LeafClass->GetDefaultObject());
		ASSERT_THAT(IsNotNull(BaseCDO, TEXT("default base actor should expose a CDO")));
		ASSERT_THAT(IsNotNull(LeafCDO, TEXT("default leaf actor should expose a CDO")));
		if (BaseCDO == nullptr || LeafCDO == nullptr)
		{
			return;
		}

		FIntProperty* HealthProperty = FindFProperty<FIntProperty>(LeafClass, TEXT("Health"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(LeafClass, TEXT("Label"));
		FNameProperty* StateNameProperty = FindFProperty<FNameProperty>(LeafClass, TEXT("StateName"));
		FStructProperty* SpawnOffsetProperty = FindFProperty<FStructProperty>(LeafClass, TEXT("SpawnOffset"));
		FArrayProperty* DefaultNamesProperty = FindFProperty<FArrayProperty>(LeafClass, TEXT("DefaultNames"));
		FSetProperty* DefaultFlagsProperty = FindFProperty<FSetProperty>(LeafClass, TEXT("DefaultFlags"));
		FMapProperty* DefaultScoresProperty = FindFProperty<FMapProperty>(LeafClass, TEXT("DefaultScores"));
		FIntProperty* DefaultIntZeroProperty = FindFProperty<FIntProperty>(LeafClass, TEXT("DefaultIntZero"));
		FBoolProperty* DefaultBoolFalseProperty = FindFProperty<FBoolProperty>(LeafClass, TEXT("bDefaultBoolFalse"));
		FStrProperty* DefaultStringEmptyProperty = FindFProperty<FStrProperty>(LeafClass, TEXT("DefaultStringEmpty"));
		FNameProperty* DefaultNameNoneProperty = FindFProperty<FNameProperty>(LeafClass, TEXT("DefaultNameNone"));
		FStructProperty* DefaultVectorZeroProperty = FindFProperty<FStructProperty>(LeafClass, TEXT("DefaultVectorZero"));
		FObjectPropertyBase* DefaultObjectNullProperty = FindFProperty<FObjectPropertyBase>(LeafClass, TEXT("DefaultObjectNull"));
		FArrayProperty* EmptyDefaultNumbersProperty = FindFProperty<FArrayProperty>(LeafClass, TEXT("EmptyDefaultNumbers"));
		FSetProperty* EmptyDefaultNamesProperty = FindFProperty<FSetProperty>(LeafClass, TEXT("EmptyDefaultNames"));
		FMapProperty* EmptyDefaultScoresProperty = FindFProperty<FMapProperty>(LeafClass, TEXT("EmptyDefaultScores"));
		FClassProperty* NativeActorClassProperty = FindFProperty<FClassProperty>(LeafClass, TEXT("NativeActorClass"));
		FClassProperty* ScriptActorClassProperty = FindFProperty<FClassProperty>(LeafClass, TEXT("ScriptActorClass"));
		ASSERT_THAT(IsNotNull(HealthProperty, TEXT("default Health member should reflect")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("default Label member should reflect")));
		ASSERT_THAT(IsNotNull(StateNameProperty, TEXT("default StateName member should reflect")));
		ASSERT_THAT(IsNotNull(SpawnOffsetProperty, TEXT("default SpawnOffset member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultNamesProperty, TEXT("default DefaultNames member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultFlagsProperty, TEXT("default DefaultFlags member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultScoresProperty, TEXT("default DefaultScores member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultIntZeroProperty, TEXT("uninitialized int member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultBoolFalseProperty, TEXT("uninitialized bool member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultStringEmptyProperty, TEXT("uninitialized FString member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultNameNoneProperty, TEXT("uninitialized FName member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultVectorZeroProperty, TEXT("uninitialized FVector member should reflect")));
		ASSERT_THAT(IsNotNull(DefaultObjectNullProperty, TEXT("uninitialized UObject member should reflect")));
		ASSERT_THAT(IsNotNull(EmptyDefaultNumbersProperty, TEXT("uninitialized TArray member should reflect")));
		ASSERT_THAT(IsNotNull(EmptyDefaultNamesProperty, TEXT("uninitialized TSet member should reflect")));
		ASSERT_THAT(IsNotNull(EmptyDefaultScoresProperty, TEXT("uninitialized TMap member should reflect")));
		ASSERT_THAT(IsNotNull(NativeActorClassProperty, TEXT("native class default member should reflect")));
		ASSERT_THAT(IsNotNull(ScriptActorClassProperty, TEXT("script class default member should reflect")));
		if (HealthProperty == nullptr || LabelProperty == nullptr || StateNameProperty == nullptr || SpawnOffsetProperty == nullptr
			|| DefaultNamesProperty == nullptr || DefaultFlagsProperty == nullptr || DefaultScoresProperty == nullptr
			|| DefaultIntZeroProperty == nullptr || DefaultBoolFalseProperty == nullptr || DefaultStringEmptyProperty == nullptr
			|| DefaultNameNoneProperty == nullptr || DefaultVectorZeroProperty == nullptr || DefaultObjectNullProperty == nullptr
			|| EmptyDefaultNumbersProperty == nullptr || EmptyDefaultNamesProperty == nullptr || EmptyDefaultScoresProperty == nullptr
			|| NativeActorClassProperty == nullptr || ScriptActorClassProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(250, HealthProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("default Health override should land on the leaf CDO")));
		ASSERT_THAT(AreEqual(FString(TEXT("LeafLabel")), LabelProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("default FString override should land on the leaf CDO")));
		ASSERT_THAT(AreEqual(FName(TEXT("BaseState")), StateNameProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("inherited FName initializer should land on the leaf CDO")));
		ASSERT_THAT(IsTrue(SpawnOffsetProperty->Struct != nullptr && SpawnOffsetProperty->Struct->IsChildOf(TBaseStructure<FVector>::Get()), TEXT("SpawnOffset should reflect as FVector")));
		const FVector CDOOffset = *SpawnOffsetProperty->ContainerPtrToValuePtr<FVector>(LeafCDO);
		ASSERT_THAT(IsTrue(CDOOffset.Equals(FVector(4, 5, 6), 0.001), TEXT("default FVector override should land on the leaf CDO")));
		ASSERT_THAT(AreEqual(AActor::StaticClass(), NativeActorClassProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("native TSubclassOf default should store AActor")));
		ASSERT_THAT(AreEqual(BaseClass, ScriptActorClassProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("script TSubclassOf default should store the generated base class")));
		ASSERT_THAT(IsTrue(BaseCDO->Tags.Contains(FName(TEXT("BaseDefaultTag"))), TEXT("base default Tags.Add should affect the base CDO")));
		ASSERT_THAT(IsTrue(LeafCDO->Tags.Contains(FName(TEXT("BaseDefaultTag"))) && LeafCDO->Tags.Contains(FName(TEXT("LeafDefaultTag"))), TEXT("leaf CDO should accumulate inherited and leaf default tags")));
		ASSERT_THAT(IsTrue(LeafCDO->GetIsReplicated(), TEXT("default SetReplicates(true) should affect the leaf CDO")));
		ASSERT_THAT(AreEqual(0, DefaultIntZeroProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("uninitialized int UPROPERTY should default to zero on the CDO")));
		ASSERT_THAT(IsFalse(DefaultBoolFalseProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("uninitialized bool UPROPERTY should default to false on the CDO")));
		ASSERT_THAT(IsTrue(DefaultStringEmptyProperty->GetPropertyValue_InContainer(LeafCDO).IsEmpty(), TEXT("uninitialized FString UPROPERTY should default to empty on the CDO")));
		ASSERT_THAT(AreEqual(FName(), DefaultNameNoneProperty->GetPropertyValue_InContainer(LeafCDO), TEXT("uninitialized FName UPROPERTY should default to NAME_None on the CDO")));
		const FVector CDODefaultVector = *DefaultVectorZeroProperty->ContainerPtrToValuePtr<FVector>(LeafCDO);
		ASSERT_THAT(IsTrue(CDODefaultVector.Equals(FVector::ZeroVector, 0.001), TEXT("uninitialized FVector UPROPERTY should default to zero on the CDO")));
		ASSERT_THAT(IsNull(DefaultObjectNullProperty->GetObjectPropertyValue_InContainer(LeafCDO), TEXT("uninitialized UObject UPROPERTY should default to null on the CDO")));
		ASSERT_THAT(AreEqual(0, FScriptArrayHelper(EmptyDefaultNumbersProperty, EmptyDefaultNumbersProperty->ContainerPtrToValuePtr<void>(LeafCDO)).Num(), TEXT("uninitialized TArray UPROPERTY should default empty on the CDO")));
		ASSERT_THAT(AreEqual(0, FScriptSetHelper(EmptyDefaultNamesProperty, EmptyDefaultNamesProperty->ContainerPtrToValuePtr<void>(LeafCDO)).Num(), TEXT("uninitialized TSet UPROPERTY should default empty on the CDO")));
		ASSERT_THAT(AreEqual(0, FScriptMapHelper(EmptyDefaultScoresProperty, EmptyDefaultScoresProperty->ContainerPtrToValuePtr<void>(LeafCDO)).Num(), TEXT("uninitialized TMap UPROPERTY should default empty on the CDO")));

		FScriptArrayHelper DefaultNamesHelper(DefaultNamesProperty, DefaultNamesProperty->ContainerPtrToValuePtr<void>(LeafCDO));
		ASSERT_THAT(AreEqual(2, DefaultNamesHelper.Num(), TEXT("default array operations should accumulate on the CDO")));
		if (DefaultNamesHelper.Num() < 2)
		{
			return;
		}
		const FNameProperty* DefaultNamesInnerProperty = CastField<const FNameProperty>(DefaultNamesProperty->Inner);
		ASSERT_THAT(IsNotNull(DefaultNamesInnerProperty, TEXT("DefaultNames inner should reflect as FNameProperty")));
		if (DefaultNamesInnerProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FName(TEXT("BaseName")), DefaultNamesInnerProperty->GetPropertyValue(DefaultNamesHelper.GetRawPtr(0)), TEXT("base default array value should remain first")));
		ASSERT_THAT(AreEqual(FName(TEXT("LeafName")), DefaultNamesInnerProperty->GetPropertyValue(DefaultNamesHelper.GetRawPtr(1)), TEXT("leaf default array value should append second")));

		FScriptSetHelper DefaultFlagsHelper(DefaultFlagsProperty, DefaultFlagsProperty->ContainerPtrToValuePtr<void>(LeafCDO));
		ASSERT_THAT(AreEqual(2, DefaultFlagsHelper.Num(), TEXT("default set operations should accumulate on the CDO")));
		ASSERT_THAT(IsTrue(DefaultFlagsProperty->ElementProp->IsA<FNameProperty>(), TEXT("DefaultFlags element should reflect as FNameProperty")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FName>(*TestRunner, LeafCDO, TEXT("DefaultFlags"), FName(TEXT("BaseFlag"))), TEXT("default set should keep inherited CDO entry")));
		ASSERT_THAT(IsTrue(SetContainsByPath<FName>(*TestRunner, LeafCDO, TEXT("DefaultFlags"), FName(TEXT("LeafFlag"))), TEXT("default set should keep leaf CDO entry")));

		FScriptMapHelper DefaultScoresHelper(DefaultScoresProperty, DefaultScoresProperty->ContainerPtrToValuePtr<void>(LeafCDO));
		ASSERT_THAT(AreEqual(2, DefaultScoresHelper.Num(), TEXT("default map operations should accumulate on the CDO")));
		ASSERT_THAT(IsTrue(DefaultScoresProperty->KeyProp->IsA<FNameProperty>(), TEXT("DefaultScores key should reflect as FNameProperty")));
		ASSERT_THAT(IsTrue(DefaultScoresProperty->ValueProp->IsA<FIntProperty>(), TEXT("DefaultScores value should reflect as FIntProperty")));
		int32 DefaultScore = 0;
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, LeafCDO, TEXT("DefaultScores"), FName(TEXT("BaseScore")), DefaultScore), TEXT("default map should keep inherited CDO key")));
		ASSERT_THAT(AreEqual(101, DefaultScore, TEXT("default map inherited CDO value should round-trip")));
		ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, LeafCDO, TEXT("DefaultScores"), FName(TEXT("LeafScore")), DefaultScore), TEXT("default map should keep leaf CDO key")));
		ASSERT_THAT(AreEqual(202, DefaultScore, TEXT("default map leaf CDO value should round-trip")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, LeafClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("default leaf actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RuntimeHealth"), 250, TEXT("runtime Health should copy the CDO default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("RuntimeLabel"), FString(TEXT("LeafLabel")), TEXT("runtime Label should copy the CDO default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("RuntimeStateName"), FName(TEXT("BaseState")), TEXT("runtime StateName should copy the inherited default"))));

		FVector RuntimeSpawnOffset;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("RuntimeSpawnOffset"), RuntimeSpawnOffset), TEXT("runtime SpawnOffset should be readable")));
		ASSERT_THAT(IsTrue(RuntimeSpawnOffset.Equals(FVector(4, 5, 6), 0.001), TEXT("runtime SpawnOffset should copy the CDO default")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RuntimeNameCount"), 2, TEXT("runtime array default should have two entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRuntimeHasBaseName"), true, TEXT("runtime array default should include inherited entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRuntimeHasLeafName"), true, TEXT("runtime array default should include leaf entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RuntimeFlagCount"), 2, TEXT("runtime set default should have two entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRuntimeHasBaseFlag"), true, TEXT("runtime set default should include inherited entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRuntimeHasLeafFlag"), true, TEXT("runtime set default should include leaf entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RuntimeBaseScore"), 101, TEXT("runtime map default should include inherited entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RuntimeLeafScore"), 202, TEXT("runtime map default should include leaf entry"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bRuntimeReplicates"), true, TEXT("runtime actor should inherit CDO replication default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DefaultIntZero"), 0, TEXT("runtime uninitialized int UPROPERTY should remain zero"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bDefaultBoolFalse"), false, TEXT("runtime uninitialized bool UPROPERTY should remain false"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("DefaultStringEmpty"), FString(), TEXT("runtime uninitialized FString UPROPERTY should remain empty"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("DefaultNameNone"), FName(), TEXT("runtime uninitialized FName UPROPERTY should remain NAME_None"))));
		FVector RuntimeDefaultVector;
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("DefaultVectorZero"), RuntimeDefaultVector), TEXT("runtime uninitialized FVector UPROPERTY should be readable")));
		ASSERT_THAT(IsTrue(RuntimeDefaultVector.Equals(FVector::ZeroVector, 0.001), TEXT("runtime uninitialized FVector UPROPERTY should remain zero")));
		UObject* RuntimeDefaultObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("DefaultObjectNull"), RuntimeDefaultObject), TEXT("runtime uninitialized UObject UPROPERTY should be readable")));
		ASSERT_THAT(IsNull(RuntimeDefaultObject, TEXT("runtime uninitialized UObject UPROPERTY should remain null")));
		int32 EmptyDefaultCount = INDEX_NONE;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("EmptyDefaultNumbers"), EmptyDefaultCount), TEXT("runtime uninitialized TArray UPROPERTY count should be readable")));
		ASSERT_THAT(AreEqual(0, EmptyDefaultCount, TEXT("runtime uninitialized TArray UPROPERTY should remain empty")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("EmptyDefaultNames"), EmptyDefaultCount), TEXT("runtime uninitialized TSet UPROPERTY count should be readable")));
		ASSERT_THAT(AreEqual(0, EmptyDefaultCount, TEXT("runtime uninitialized TSet UPROPERTY should remain empty")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("EmptyDefaultScores"), EmptyDefaultCount), TEXT("runtime uninitialized TMap UPROPERTY count should be readable")));
		ASSERT_THAT(AreEqual(0, EmptyDefaultCount, TEXT("runtime uninitialized TMap UPROPERTY should remain empty")));
	}

	TEST_METHOD(UClassAccessAndBlueprintVisibilityMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_AccessAndBlueprintVisibilityMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassPropertyAccessBaseObject : UObject
			{
				UPROPERTY(BlueprintReadWrite)
				int PublicReadWriteValue = 3;

				UPROPERTY(BlueprintReadOnly)
				int PublicReadOnlyValue = 5;

				UPROPERTY(BlueprintHidden)
				int PublicHiddenValue = 7;

				UPROPERTY(BlueprintReadWrite)
				protected int ProtectedReadWriteValue = 11;

				UPROPERTY(BlueprintReadOnly)
				protected int ProtectedReadOnlyValue = 13;

				UPROPERTY(BlueprintHidden)
				protected int ProtectedHiddenValue = 17;

				UPROPERTY(BlueprintReadWrite, meta=(AllowPrivateAccess))
				private int PrivateAllowedReadWriteValue = 19;

				UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess))
				private int PrivateAllowedReadOnlyValue = 23;

				UPROPERTY(BlueprintReadWrite)
				private int PrivateHiddenReadWriteValue = 29;

				UPROPERTY(BlueprintHidden)
				private int PrivateExplicitHiddenValue = 31;

				UFUNCTION(BlueprintCallable)
				int ReadBaseAccessSum()
				{
					return PublicReadWriteValue
						+ PublicReadOnlyValue
						+ PublicHiddenValue
						+ ProtectedReadWriteValue
						+ ProtectedReadOnlyValue
						+ ProtectedHiddenValue
						+ PrivateAllowedReadWriteValue
						+ PrivateAllowedReadOnlyValue
						+ PrivateHiddenReadWriteValue
						+ PrivateExplicitHiddenValue;
				}
			}

			UCLASS()
			class UCoverageUClassPropertyAccessLeafObject : UCoverageUClassPropertyAccessBaseObject
			{
				UPROPERTY()
				int ObservedProtectedSum = 0;

				UPROPERTY()
				int ObservedPublicSum = 0;

				UFUNCTION(BlueprintCallable)
				int MutateInheritedVisibleMembers()
				{
					PublicReadWriteValue += 100;
					ProtectedReadWriteValue += 200;
					ObservedPublicSum = PublicReadWriteValue + PublicReadOnlyValue + PublicHiddenValue;
					ObservedProtectedSum = ProtectedReadWriteValue + ProtectedReadOnlyValue + ProtectedHiddenValue;
					return ObservedPublicSum + ObservedProtectedSum;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyAccessAndBlueprintVisibilityMatrix.as"), ScriptSource)));

		UClass* BaseClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPropertyAccessBaseObject"));
		UClass* LeafClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassPropertyAccessLeafObject"));
		ASSERT_THAT(IsNotNull(BaseClass, TEXT("access matrix base class should be generated")));
		ASSERT_THAT(IsNotNull(LeafClass, TEXT("access matrix leaf class should be generated")));
		if (BaseClass == nullptr || LeafClass == nullptr)
		{
			return;
		}

		FProperty* PublicReadWriteValue = BaseClass->FindPropertyByName(TEXT("PublicReadWriteValue"));
		FProperty* PublicReadOnlyValue = BaseClass->FindPropertyByName(TEXT("PublicReadOnlyValue"));
		FProperty* PublicHiddenValue = BaseClass->FindPropertyByName(TEXT("PublicHiddenValue"));
		FProperty* ProtectedReadWriteValue = BaseClass->FindPropertyByName(TEXT("ProtectedReadWriteValue"));
		FProperty* ProtectedReadOnlyValue = BaseClass->FindPropertyByName(TEXT("ProtectedReadOnlyValue"));
		FProperty* ProtectedHiddenValue = BaseClass->FindPropertyByName(TEXT("ProtectedHiddenValue"));
		FProperty* PrivateAllowedReadWriteValue = BaseClass->FindPropertyByName(TEXT("PrivateAllowedReadWriteValue"));
		FProperty* PrivateAllowedReadOnlyValue = BaseClass->FindPropertyByName(TEXT("PrivateAllowedReadOnlyValue"));
		FProperty* PrivateHiddenReadWriteValue = BaseClass->FindPropertyByName(TEXT("PrivateHiddenReadWriteValue"));
		FProperty* PrivateExplicitHiddenValue = BaseClass->FindPropertyByName(TEXT("PrivateExplicitHiddenValue"));
		ASSERT_THAT(IsNotNull(PublicReadWriteValue, TEXT("public BlueprintReadWrite member should reflect")));
		ASSERT_THAT(IsNotNull(PublicReadOnlyValue, TEXT("public BlueprintReadOnly member should reflect")));
		ASSERT_THAT(IsNotNull(PublicHiddenValue, TEXT("public BlueprintHidden member should reflect")));
		ASSERT_THAT(IsNotNull(ProtectedReadWriteValue, TEXT("protected BlueprintReadWrite member should reflect")));
		ASSERT_THAT(IsNotNull(ProtectedReadOnlyValue, TEXT("protected BlueprintReadOnly member should reflect")));
		ASSERT_THAT(IsNotNull(ProtectedHiddenValue, TEXT("protected BlueprintHidden member should reflect")));
		ASSERT_THAT(IsNotNull(PrivateAllowedReadWriteValue, TEXT("private BlueprintReadWrite AllowPrivateAccess member should reflect")));
		ASSERT_THAT(IsNotNull(PrivateAllowedReadOnlyValue, TEXT("private BlueprintReadOnly AllowPrivateAccess member should reflect")));
		ASSERT_THAT(IsNotNull(PrivateHiddenReadWriteValue, TEXT("private BlueprintReadWrite member without AllowPrivateAccess should reflect")));
		ASSERT_THAT(IsNotNull(PrivateExplicitHiddenValue, TEXT("private BlueprintHidden member should reflect")));
		if (PublicReadWriteValue == nullptr || PublicReadOnlyValue == nullptr || PublicHiddenValue == nullptr
			|| ProtectedReadWriteValue == nullptr || ProtectedReadOnlyValue == nullptr || ProtectedHiddenValue == nullptr
			|| PrivateAllowedReadWriteValue == nullptr || PrivateAllowedReadOnlyValue == nullptr
			|| PrivateHiddenReadWriteValue == nullptr || PrivateExplicitHiddenValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(PublicReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("public BlueprintReadWrite should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(PublicReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("public BlueprintReadWrite should remain writable")));
		ASSERT_THAT(IsTrue(PublicReadOnlyValue->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("public BlueprintReadOnly should be visible and read-only")));
		ASSERT_THAT(IsFalse(PublicHiddenValue->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("public BlueprintHidden should clear Blueprint visibility flags")));

		ASSERT_THAT(IsTrue(ProtectedReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("protected BlueprintReadWrite should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(ProtectedReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("protected BlueprintReadWrite should remain writable")));
		ASSERT_THAT(IsTrue(ProtectedReadOnlyValue->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("protected BlueprintReadOnly should be visible and read-only")));
		ASSERT_THAT(IsFalse(ProtectedHiddenValue->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("protected BlueprintHidden should clear Blueprint visibility flags")));

		ASSERT_THAT(IsTrue(PrivateAllowedReadWriteValue->HasMetaData(TEXT("AllowPrivateAccess")), TEXT("private BlueprintReadWrite should keep AllowPrivateAccess metadata")));
		ASSERT_THAT(IsTrue(PrivateAllowedReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("private BlueprintReadWrite with AllowPrivateAccess should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(PrivateAllowedReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("private BlueprintReadWrite with AllowPrivateAccess should remain writable")));
		ASSERT_THAT(IsTrue(PrivateAllowedReadOnlyValue->HasMetaData(TEXT("AllowPrivateAccess")), TEXT("private BlueprintReadOnly should keep AllowPrivateAccess metadata")));
		ASSERT_THAT(IsTrue(PrivateAllowedReadOnlyValue->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("private BlueprintReadOnly with AllowPrivateAccess should be visible and read-only")));
		ASSERT_THAT(IsFalse(PrivateHiddenReadWriteValue->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("private BlueprintReadWrite without AllowPrivateAccess should remain Blueprint-hidden")));
		ASSERT_THAT(IsFalse(PrivateExplicitHiddenValue->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly), TEXT("private BlueprintHidden should remain Blueprint-hidden")));

		UObject* BaseInstance = NewObject<UObject>(GetTransientPackage(), BaseClass, TEXT("CoverageUClassPropertyAccessBaseObject"), RF_Transient);
		UObject* LeafInstance = NewObject<UObject>(GetTransientPackage(), LeafClass, TEXT("CoverageUClassPropertyAccessLeafObject"), RF_Transient);
		ASSERT_THAT(IsNotNull(BaseInstance, TEXT("access matrix base object should instantiate")));
		ASSERT_THAT(IsNotNull(LeafInstance, TEXT("access matrix leaf object should instantiate")));
		if (BaseInstance == nullptr || LeafInstance == nullptr)
		{
			return;
		}

		FFunctionInvoker ReadBaseAccessSumInvoker(*TestRunner, BaseInstance, TEXT("ReadBaseAccessSum"));
		ASSERT_THAT(IsTrue(ReadBaseAccessSumInvoker.IsValid(), TEXT("ReadBaseAccessSum invoker should be valid")));
		if (!ReadBaseAccessSumInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(158, ReadBaseAccessSumInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("declaring class should read all public/protected/private members")));

		FFunctionInvoker MutateInheritedVisibleMembersInvoker(*TestRunner, LeafInstance, TEXT("MutateInheritedVisibleMembers"));
		ASSERT_THAT(IsTrue(MutateInheritedVisibleMembersInvoker.IsValid(), TEXT("MutateInheritedVisibleMembers invoker should be valid")));
		if (!MutateInheritedVisibleMembersInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(356, MutateInheritedVisibleMembersInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("derived class should mutate inherited public/protected writable members")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LeafInstance, TEXT("ObservedPublicSum"), 115, TEXT("derived object should observe public inherited values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, LeafInstance, TEXT("ObservedProtectedSum"), 241, TEXT("derived object should observe protected inherited values"))));
	}

	TEST_METHOD(UClassNonUPropertyMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_NonUPropertyMemberMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassNonUPropertyMemberActor : AActor
			{
				int RuntimeCounter = 7;
				FString RuntimeLabel = "Seed";
				TArray<int> RuntimeValues;

				UPROPERTY()
				int ReflectedCounter = 0;

				UPROPERTY()
				FString ReflectedLabel;

				UPROPERTY()
				int ReflectedArraySum = 0;

				UPROPERTY()
				int ReflectedFunctionResult = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RuntimeCounter += 35;
					RuntimeLabel += "_Runtime";
					RuntimeValues.Add(5);
					RuntimeValues.Add(6);
					ReflectedCounter = RuntimeCounter;
					ReflectedLabel = RuntimeLabel;
					ReflectedArraySum = RuntimeValues[0] + RuntimeValues[1];
					ReflectedFunctionResult = ReadRuntimeCounter();
				}

				UFUNCTION()
				int ReadRuntimeCounter()
				{
					return RuntimeCounter;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertyNonUPropertyMemberMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassNonUPropertyMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("non-UPROPERTY actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FProperty* RuntimeCounterProperty = ScriptClass->FindPropertyByName(TEXT("RuntimeCounter"));
		FProperty* RuntimeLabelProperty = ScriptClass->FindPropertyByName(TEXT("RuntimeLabel"));
		FProperty* RuntimeValuesProperty = ScriptClass->FindPropertyByName(TEXT("RuntimeValues"));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ScriptClass, TEXT("ReflectedCounter")), TEXT("observable counter should reflect")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStrProperty>(ScriptClass, TEXT("ReflectedLabel")), TEXT("observable label should reflect")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ScriptClass, TEXT("ReflectedArraySum")), TEXT("observable array sum should reflect")));
		ASSERT_THAT(IsNotNull(ScriptClass->FindFunctionByName(TEXT("ReadRuntimeCounter")), TEXT("UFUNCTION should expose access to non-reflected state")));
		if (RuntimeCounterProperty != nullptr)
		{
			ASSERT_THAT(IsFalse(RuntimeCounterProperty->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame | CPF_Net), TEXT("plain int member should not inherit explicit UPROPERTY flags")));
		}
		if (RuntimeLabelProperty != nullptr)
		{
			ASSERT_THAT(IsFalse(RuntimeLabelProperty->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame | CPF_Net), TEXT("plain FString member should not inherit explicit UPROPERTY flags")));
		}
		if (RuntimeValuesProperty != nullptr)
		{
			ASSERT_THAT(IsFalse(RuntimeValuesProperty->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame | CPF_Net), TEXT("plain TArray member should not inherit explicit UPROPERTY flags")));
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("non-UPROPERTY actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReflectedCounter"), 42, TEXT("plain int member should be usable at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReflectedLabel"), FString(TEXT("Seed_Runtime")), TEXT("plain FString member should be usable at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReflectedArraySum"), 11, TEXT("plain TArray member should be usable at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReflectedFunctionResult"), 42, TEXT("UFUNCTION should read non-reflected state"))));

		FFunctionInvoker ReadCounterInvoker(*TestRunner, Actor, TEXT("ReadRuntimeCounter"));
		ASSERT_THAT(IsTrue(ReadCounterInvoker.IsValid(), TEXT("ReadRuntimeCounter invoker should be valid")));
		ASSERT_THAT(AreEqual(42, ReadCounterInvoker.CallAndReturn<int32>(), TEXT("reflected UFUNCTION should return the current non-UPROPERTY value")));
	}

	TEST_METHOD(UClassPropertySpecifierAndMetadataMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassProperty_SpecifierAndMetadataMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Config=Game)
			class ACoverageUClassPropertySpecifierActor : AActor
			{
				UPROPERTY(VisibleAnywhere)
				int VisibleValue = 1;

				UPROPERTY(VisibleDefaultsOnly)
				int VisibleDefaultValue = 2;

				UPROPERTY(VisibleInstanceOnly)
				int VisibleInstanceValue = 3;

				UPROPERTY(EditAnywhere)
				int EditAnywhereValue = 4;

				UPROPERTY(EditDefaultsOnly)
				int EditDefaultValue = 5;

				UPROPERTY(EditInstanceOnly)
				int EditInstanceValue = 6;

				UPROPERTY(NotVisible)
				int NotVisibleValue = 21;

				UPROPERTY(NotEditable)
				int NotEditableValue = 7;

				UPROPERTY(EditConst)
				int EditConstValue = 8;

				UPROPERTY(AdvancedDisplay)
				int AdvancedValue = 9;

				UPROPERTY(Interp)
				float InterpValue = 9.5f;

				UPROPERTY(Config)
				int ConfigValue = 10;

				UPROPERTY(AssetRegistrySearchable)
				int SearchableValue = 11;

				UPROPERTY(SkipSerialization)
				int SkipSerializedValue = 12;

				UPROPERTY(NoClear)
				AActor NoClearActor;

				UPROPERTY(Transient)
				int TransientValue = 13;

				UPROPERTY(SaveGame)
				int SaveGameValue = 14;

				UPROPERTY(EditFixedSize)
				TArray<int> FixedArray;

				UPROPERTY(EditInline)
				UObject EditInlineObject;

				UPROPERTY(BindWidget)
				UObject BoundWidget;

				UPROPERTY(ExposeOnSpawn)
				int SpawnExposedValue = 15;

				UPROPERTY(meta=(EditorOnly))
				int EditorOnlyValue = 16;

				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coverage|Member", meta=(DisplayName="Editable Count", ToolTip="Editable count tooltip", ClampMin="0", ClampMax="100"))
				int EditableCount = 19;

				UPROPERTY(EditAnywhere, meta=(ScriptName="AliasCount"))
				int NativeCount = 27;

				UPROPERTY(EditAnywhere, meta=(ScriptName="AliasLabel", DeprecatedProperty, DeprecationMessage="Use AliasLabel instead"))
				FString NativeLabel = "AliasDefault";

				UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle))
				bool bSpecifierToggle = true;

				UPROPERTY(EditAnywhere, meta=(EditCondition="bSpecifierToggle"))
				int EditConditionValue = 24;

				UPROPERTY(EditAnywhere, meta=(EditCondition="bSpecifierToggle", EditConditionHides))
				int EditConditionHiddenValue = 28;

				UPROPERTY(EditAnywhere, meta=(UIMin="0.0", UIMax="100.0", Units="Seconds"))
				float TimedValue = 1.5f;

				UPROPERTY(EditAnywhere, meta=(MakeEditWidget))
				FVector WidgetLocation = FVector(1, 2, 3);

				UPROPERTY(EditAnywhere, meta=(NoResetToDefault))
				int NoResetValue = 25;

				UPROPERTY(EditAnywhere, meta=(DeprecatedProperty, DeprecationMessage="Use EditableCount instead"))
				int DeprecatedValue = 26;

				UPROPERTY(EditAnywhere, meta=(DisplayAfter="EditableCount", DisplayPriority="2", ShortToolTip="Ordered short tooltip", CoverageAdvancedKey="OrderedValue", ConfigRestartRequired="true"))
				int OrderedMetadataValue = 29;

				UPROPERTY(BlueprintHidden)
				int BlueprintHiddenValue = 22;

				UPROPERTY(BlueprintProtected)
				int BlueprintProtectedValue = 23;

				UPROPERTY(BlueprintReadWrite, BlueprintGetter=GetAccessorValue, BlueprintSetter=SetAccessorValue)
				int AccessorValue = 20;

				UFUNCTION(BlueprintPure)
				int GetAccessorValue() const
				{
					return AccessorValue;
				}

				UFUNCTION(BlueprintCallable)
				void SetAccessorValue(int NewValue)
				{
					AccessorValue = NewValue;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileUClassPropertyFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassPropertySpecifierAndMetadataMatrix.as"), ScriptSource)));

		UClass* ScriptClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassPropertySpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("property specifier actor should be generated")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ScriptClass->HasAnyClassFlags(CLASS_Config), TEXT("specifier actor should keep Config=Game")));
		ASSERT_THAT(AreEqual(FName(TEXT("Game")), ScriptClass->ClassConfigName, TEXT("specifier actor should use Game config")));

		FProperty* VisibleValue = ScriptClass->FindPropertyByName(TEXT("VisibleValue"));
		FProperty* VisibleDefaultValue = ScriptClass->FindPropertyByName(TEXT("VisibleDefaultValue"));
		FProperty* VisibleInstanceValue = ScriptClass->FindPropertyByName(TEXT("VisibleInstanceValue"));
		FProperty* EditAnywhereValue = ScriptClass->FindPropertyByName(TEXT("EditAnywhereValue"));
		FProperty* EditDefaultValue = ScriptClass->FindPropertyByName(TEXT("EditDefaultValue"));
		FProperty* EditInstanceValue = ScriptClass->FindPropertyByName(TEXT("EditInstanceValue"));
		FProperty* NotVisibleValue = ScriptClass->FindPropertyByName(TEXT("NotVisibleValue"));
		FProperty* NotEditableValue = ScriptClass->FindPropertyByName(TEXT("NotEditableValue"));
		FProperty* EditConstValue = ScriptClass->FindPropertyByName(TEXT("EditConstValue"));
		FProperty* AdvancedValue = ScriptClass->FindPropertyByName(TEXT("AdvancedValue"));
		FProperty* InterpValue = ScriptClass->FindPropertyByName(TEXT("InterpValue"));
		FProperty* ConfigValue = ScriptClass->FindPropertyByName(TEXT("ConfigValue"));
		FProperty* SearchableValue = ScriptClass->FindPropertyByName(TEXT("SearchableValue"));
		FProperty* SkipSerializedValue = ScriptClass->FindPropertyByName(TEXT("SkipSerializedValue"));
		FProperty* NoClearActor = ScriptClass->FindPropertyByName(TEXT("NoClearActor"));
		FProperty* TransientValue = ScriptClass->FindPropertyByName(TEXT("TransientValue"));
		FProperty* SaveGameValue = ScriptClass->FindPropertyByName(TEXT("SaveGameValue"));
		FProperty* FixedArray = ScriptClass->FindPropertyByName(TEXT("FixedArray"));
		FProperty* EditInlineObject = ScriptClass->FindPropertyByName(TEXT("EditInlineObject"));
		FProperty* BoundWidget = ScriptClass->FindPropertyByName(TEXT("BoundWidget"));
		FProperty* SpawnExposedValue = ScriptClass->FindPropertyByName(TEXT("SpawnExposedValue"));
		FProperty* EditorOnlyValue = ScriptClass->FindPropertyByName(TEXT("EditorOnlyValue"));
		FProperty* EditableCount = ScriptClass->FindPropertyByName(TEXT("EditableCount"));
		FProperty* NativeCount = ScriptClass->FindPropertyByName(TEXT("NativeCount"));
		FProperty* NativeLabel = ScriptClass->FindPropertyByName(TEXT("NativeLabel"));
		FProperty* SpecifierToggle = ScriptClass->FindPropertyByName(TEXT("bSpecifierToggle"));
		FProperty* EditConditionValue = ScriptClass->FindPropertyByName(TEXT("EditConditionValue"));
		FProperty* EditConditionHiddenValue = ScriptClass->FindPropertyByName(TEXT("EditConditionHiddenValue"));
		FProperty* TimedValue = ScriptClass->FindPropertyByName(TEXT("TimedValue"));
		FProperty* WidgetLocation = ScriptClass->FindPropertyByName(TEXT("WidgetLocation"));
		FProperty* NoResetValue = ScriptClass->FindPropertyByName(TEXT("NoResetValue"));
		FProperty* DeprecatedValue = ScriptClass->FindPropertyByName(TEXT("DeprecatedValue"));
		FProperty* OrderedMetadataValue = ScriptClass->FindPropertyByName(TEXT("OrderedMetadataValue"));
		FProperty* BlueprintHiddenValue = ScriptClass->FindPropertyByName(TEXT("BlueprintHiddenValue"));
		FProperty* BlueprintProtectedValue = ScriptClass->FindPropertyByName(TEXT("BlueprintProtectedValue"));
		FProperty* AccessorValue = ScriptClass->FindPropertyByName(TEXT("AccessorValue"));

		ASSERT_THAT(IsNotNull(VisibleValue, TEXT("VisibleAnywhere member should reflect")));
		ASSERT_THAT(IsNotNull(VisibleDefaultValue, TEXT("VisibleDefaultsOnly member should reflect")));
		ASSERT_THAT(IsNotNull(VisibleInstanceValue, TEXT("VisibleInstanceOnly member should reflect")));
		ASSERT_THAT(IsNotNull(EditAnywhereValue, TEXT("EditAnywhere member should reflect")));
		ASSERT_THAT(IsNotNull(EditDefaultValue, TEXT("EditDefaultsOnly member should reflect")));
		ASSERT_THAT(IsNotNull(EditInstanceValue, TEXT("EditInstanceOnly member should reflect")));
		ASSERT_THAT(IsNotNull(NotVisibleValue, TEXT("NotVisible member should reflect")));
		ASSERT_THAT(IsNotNull(NotEditableValue, TEXT("NotEditable member should reflect")));
		ASSERT_THAT(IsNotNull(EditConstValue, TEXT("EditConst member should reflect")));
		ASSERT_THAT(IsNotNull(AdvancedValue, TEXT("AdvancedDisplay member should reflect")));
		ASSERT_THAT(IsNotNull(InterpValue, TEXT("Interp member should reflect")));
		ASSERT_THAT(IsNotNull(ConfigValue, TEXT("Config member should reflect")));
		ASSERT_THAT(IsNotNull(SearchableValue, TEXT("AssetRegistrySearchable member should reflect")));
		ASSERT_THAT(IsNotNull(SkipSerializedValue, TEXT("SkipSerialization member should reflect")));
		ASSERT_THAT(IsNotNull(NoClearActor, TEXT("NoClear object member should reflect")));
		ASSERT_THAT(IsNotNull(TransientValue, TEXT("Transient member should reflect")));
		ASSERT_THAT(IsNotNull(SaveGameValue, TEXT("SaveGame member should reflect")));
		ASSERT_THAT(IsNotNull(FixedArray, TEXT("EditFixedSize array member should reflect")));
		ASSERT_THAT(IsNotNull(EditInlineObject, TEXT("EditInline object member should reflect")));
		ASSERT_THAT(IsNotNull(BoundWidget, TEXT("BindWidget object member should reflect")));
		ASSERT_THAT(IsNotNull(SpawnExposedValue, TEXT("ExposeOnSpawn member should reflect")));
		ASSERT_THAT(IsNotNull(EditorOnlyValue, TEXT("EditorOnly metadata member should reflect")));
		ASSERT_THAT(IsNotNull(EditableCount, TEXT("EditAnywhere BlueprintReadOnly metadata member should reflect")));
		ASSERT_THAT(IsNotNull(NativeCount, TEXT("ScriptName metadata int member should keep its native property name")));
		ASSERT_THAT(IsNotNull(NativeLabel, TEXT("ScriptName/deprecation metadata FString member should keep its native property name")));
		ASSERT_THAT(IsNotNull(SpecifierToggle, TEXT("InlineEditConditionToggle metadata member should reflect")));
		ASSERT_THAT(IsNotNull(EditConditionValue, TEXT("EditCondition metadata member should reflect")));
		ASSERT_THAT(IsNotNull(EditConditionHiddenValue, TEXT("EditConditionHides metadata member should reflect")));
		ASSERT_THAT(IsNotNull(TimedValue, TEXT("UIMin/UIMax/Units metadata member should reflect")));
		ASSERT_THAT(IsNotNull(WidgetLocation, TEXT("MakeEditWidget metadata member should reflect")));
		ASSERT_THAT(IsNotNull(NoResetValue, TEXT("NoResetToDefault metadata member should reflect")));
		ASSERT_THAT(IsNotNull(DeprecatedValue, TEXT("DeprecatedProperty metadata member should reflect")));
		ASSERT_THAT(IsNotNull(OrderedMetadataValue, TEXT("ordering/custom metadata member should reflect")));
		ASSERT_THAT(IsNotNull(BlueprintHiddenValue, TEXT("BlueprintHidden member should reflect")));
		ASSERT_THAT(IsNotNull(BlueprintProtectedValue, TEXT("BlueprintProtected metadata member should reflect")));
		ASSERT_THAT(IsNotNull(AccessorValue, TEXT("BlueprintGetter/Setter member should reflect")));
		if (VisibleValue == nullptr || VisibleDefaultValue == nullptr || VisibleInstanceValue == nullptr || EditAnywhereValue == nullptr
			|| EditDefaultValue == nullptr || EditInstanceValue == nullptr || NotVisibleValue == nullptr || NotEditableValue == nullptr || EditConstValue == nullptr
			|| AdvancedValue == nullptr || InterpValue == nullptr || ConfigValue == nullptr || SearchableValue == nullptr || SkipSerializedValue == nullptr
			|| NoClearActor == nullptr || TransientValue == nullptr || SaveGameValue == nullptr || FixedArray == nullptr || EditInlineObject == nullptr
			|| BoundWidget == nullptr || SpawnExposedValue == nullptr || EditorOnlyValue == nullptr || EditableCount == nullptr || NativeCount == nullptr
			|| NativeLabel == nullptr || SpecifierToggle == nullptr || EditConditionValue == nullptr || EditConditionHiddenValue == nullptr || TimedValue == nullptr
			|| WidgetLocation == nullptr || NoResetValue == nullptr || DeprecatedValue == nullptr || OrderedMetadataValue == nullptr || BlueprintHiddenValue == nullptr
			|| BlueprintProtectedValue == nullptr || AccessorValue == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(HasAllFlags(VisibleValue, CPF_Edit | CPF_EditConst), TEXT("VisibleAnywhere should set edit visibility plus EditConst")));
		ASSERT_THAT(IsTrue(HasAllFlags(VisibleDefaultValue, CPF_Edit | CPF_EditConst | CPF_DisableEditOnInstance), TEXT("VisibleDefaultsOnly should disable instance editing")));
		ASSERT_THAT(IsTrue(HasAllFlags(VisibleInstanceValue, CPF_Edit | CPF_EditConst | CPF_DisableEditOnTemplate), TEXT("VisibleInstanceOnly should disable template editing")));
		ASSERT_THAT(IsTrue(EditAnywhereValue->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit")));
		ASSERT_THAT(IsFalse(EditAnywhereValue->HasAnyPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate), TEXT("EditAnywhere should not disable instance or template editing")));
		ASSERT_THAT(IsTrue(HasAllFlags(EditDefaultValue, CPF_Edit | CPF_DisableEditOnInstance), TEXT("EditDefaultsOnly should disable instance editing")));
		ASSERT_THAT(IsTrue(HasAllFlags(EditInstanceValue, CPF_Edit | CPF_DisableEditOnTemplate), TEXT("EditInstanceOnly should disable template editing")));
		ASSERT_THAT(IsFalse(NotVisibleValue->HasAnyPropertyFlags(CPF_Edit), TEXT("NotVisible should suppress edit visibility")));
		ASSERT_THAT(IsFalse(NotEditableValue->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable should suppress CPF_Edit")));
		ASSERT_THAT(IsFalse(EditConstValue->HasAnyPropertyFlags(CPF_Edit), TEXT("EditConst without an edit-visible specifier should not force CPF_Edit")));
		ASSERT_THAT(IsTrue(AdvancedValue->HasAnyPropertyFlags(CPF_AdvancedDisplay), TEXT("AdvancedDisplay should set CPF_AdvancedDisplay")));
		ASSERT_THAT(IsTrue(InterpValue->HasAnyPropertyFlags(CPF_Interp), TEXT("Interp should set CPF_Interp")));
		ASSERT_THAT(IsTrue(ConfigValue->HasAnyPropertyFlags(CPF_Config), TEXT("Config should set CPF_Config")));
		ASSERT_THAT(IsTrue(SearchableValue->HasAnyPropertyFlags(CPF_AssetRegistrySearchable), TEXT("AssetRegistrySearchable should set CPF_AssetRegistrySearchable")));
		ASSERT_THAT(IsTrue(SkipSerializedValue->HasAnyPropertyFlags(CPF_SkipSerialization), TEXT("SkipSerialization should set CPF_SkipSerialization")));
		ASSERT_THAT(IsTrue(NoClearActor->HasAnyPropertyFlags(CPF_NoClear), TEXT("NoClear should set CPF_NoClear")));
		ASSERT_THAT(IsTrue(TransientValue->HasAnyPropertyFlags(CPF_Transient), TEXT("Transient should set CPF_Transient")));
		ASSERT_THAT(IsTrue(SaveGameValue->HasAnyPropertyFlags(CPF_SaveGame), TEXT("SaveGame should set CPF_SaveGame")));
		ASSERT_THAT(IsTrue(FixedArray->HasAnyPropertyFlags(CPF_EditFixedSize), TEXT("EditFixedSize should set CPF_EditFixedSize")));
		ASSERT_THAT(IsTrue(EditInlineObject->HasMetaData(TEXT("EditInline")), TEXT("EditInline metadata should round-trip on UClass members")));
		ASSERT_THAT(IsFalse(BoundWidget->HasAnyPropertyFlags(CPF_Edit), TEXT("BindWidget should suppress edit visibility")));
		ASSERT_THAT(IsTrue(BoundWidget->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BindWidget should remain Blueprint-visible")));
		ASSERT_THAT(IsTrue(BoundWidget->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BindWidget should be Blueprint read-only")));
		ASSERT_THAT(IsTrue(BoundWidget->HasMetaData(TEXT("BindWidget")), TEXT("BindWidget metadata should round-trip on UClass members")));
		ASSERT_THAT(IsTrue(SpawnExposedValue->HasAnyPropertyFlags(CPF_ExposeOnSpawn), TEXT("ExposeOnSpawn should set CPF_ExposeOnSpawn")));
		ASSERT_THAT(IsTrue(EditorOnlyValue->HasAnyPropertyFlags(CPF_EditorOnly), TEXT("meta EditorOnly should set CPF_EditorOnly")));
		ASSERT_THAT(IsTrue(EditableCount->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere metadata member should carry CPF_Edit")));
		ASSERT_THAT(IsTrue(EditableCount->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly metadata member should be Blueprint-visible")));
		ASSERT_THAT(IsTrue(EditableCount->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly metadata member should carry CPF_BlueprintReadOnly")));
		ASSERT_THAT(AreEqual(FString(TEXT("AliasCount")), NativeCount->GetMetaData(TEXT("ScriptName")), TEXT("ScriptName metadata should round-trip on UClass int members")));
		ASSERT_THAT(IsFalse(NativeCount->HasMetaData(TEXT("DeprecatedProperty")), TEXT("ScriptName-only UClass property should not gain deprecation metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("AliasLabel")), NativeLabel->GetMetaData(TEXT("ScriptName")), TEXT("ScriptName metadata should round-trip on UClass FString members")));
		ASSERT_THAT(IsTrue(NativeLabel->HasMetaData(TEXT("DeprecatedProperty")), TEXT("ScriptName and DeprecatedProperty metadata should combine on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use AliasLabel instead")), NativeLabel->GetMetaData(TEXT("DeprecationMessage")), TEXT("combined UClass property deprecation message should round-trip")));
		ASSERT_THAT(IsTrue(SpecifierToggle->HasMetaData(TEXT("InlineEditConditionToggle")), TEXT("InlineEditConditionToggle metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("bSpecifierToggle")), EditConditionValue->GetMetaData(TEXT("EditCondition")), TEXT("EditCondition metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("bSpecifierToggle")), EditConditionHiddenValue->GetMetaData(TEXT("EditCondition")), TEXT("EditCondition metadata should combine with EditConditionHides on UClass members")));
		ASSERT_THAT(IsTrue(EditConditionHiddenValue->HasMetaData(TEXT("EditConditionHides")), TEXT("EditConditionHides metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("0.0")), TimedValue->GetMetaData(TEXT("UIMin")), TEXT("UIMin metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("100.0")), TimedValue->GetMetaData(TEXT("UIMax")), TEXT("UIMax metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Seconds")), TimedValue->GetMetaData(TEXT("Units")), TEXT("Units metadata should round-trip on UClass members")));
		ASSERT_THAT(IsTrue(WidgetLocation->HasMetaData(TEXT("MakeEditWidget")), TEXT("MakeEditWidget metadata should round-trip on UClass FVector members")));
		ASSERT_THAT(IsTrue(NoResetValue->HasMetaData(TEXT("NoResetToDefault")), TEXT("NoResetToDefault metadata should round-trip on UClass members")));
		ASSERT_THAT(IsTrue(DeprecatedValue->HasMetaData(TEXT("DeprecatedProperty")), TEXT("DeprecatedProperty metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use EditableCount instead")), DeprecatedValue->GetMetaData(TEXT("DeprecationMessage")), TEXT("DeprecationMessage metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("EditableCount")), OrderedMetadataValue->GetMetaData(TEXT("DisplayAfter")), TEXT("DisplayAfter metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("2")), OrderedMetadataValue->GetMetaData(TEXT("DisplayPriority")), TEXT("DisplayPriority metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Ordered short tooltip")), OrderedMetadataValue->GetMetaData(TEXT("ShortToolTip")), TEXT("ShortToolTip metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("OrderedValue")), OrderedMetadataValue->GetMetaData(TEXT("CoverageAdvancedKey")), TEXT("custom advanced metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("true")), OrderedMetadataValue->GetMetaData(TEXT("ConfigRestartRequired")), TEXT("boolean metadata values should round-trip on UClass members")));
		ASSERT_THAT(IsFalse(BlueprintHiddenValue->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintHidden should suppress Blueprint visibility")));
		ASSERT_THAT(IsTrue(BlueprintProtectedValue->HasMetaData(TEXT("BlueprintProtected")), TEXT("BlueprintProtected metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Member")), EditableCount->GetMetaData(TEXT("Category")), TEXT("Category metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Editable Count")), EditableCount->GetMetaData(TEXT("DisplayName")), TEXT("DisplayName metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("Editable count tooltip")), EditableCount->GetMetaData(TEXT("ToolTip")), TEXT("ToolTip metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("0")), EditableCount->GetMetaData(TEXT("ClampMin")), TEXT("ClampMin metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("100")), EditableCount->GetMetaData(TEXT("ClampMax")), TEXT("ClampMax metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("GetAccessorValue")), AccessorValue->GetMetaData(TEXT("BlueprintGetter")), TEXT("BlueprintGetter metadata should round-trip on UClass members")));
		ASSERT_THAT(AreEqual(FString(TEXT("SetAccessorValue")), AccessorValue->GetMetaData(TEXT("BlueprintSetter")), TEXT("BlueprintSetter metadata should round-trip on UClass members")));
		ASSERT_THAT(IsTrue(AccessorValue->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintGetter/Setter member should be Blueprint-visible")));
		ASSERT_THAT(IsNotNull(ScriptClass->FindFunctionByName(TEXT("GetAccessorValue")), TEXT("BlueprintGetter callback should generate a reflected UFUNCTION")));
		ASSERT_THAT(IsNotNull(ScriptClass->FindFunctionByName(TEXT("SetAccessorValue")), TEXT("BlueprintSetter callback should generate a reflected UFUNCTION")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("property specifier actor should spawn for accessor validation")));
		if (Actor == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NativeCount"), 27,
			TEXT("ScriptName metadata should not disturb UClass int property defaults"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NativeLabel"), FString(TEXT("AliasDefault")),
			TEXT("ScriptName/deprecation metadata should not disturb UClass FString property defaults"))));

		FFunctionInvoker InitialGetterInvoker(*TestRunner, Actor, TEXT("GetAccessorValue"));
		ASSERT_THAT(IsTrue(InitialGetterInvoker.IsValid(), TEXT("BlueprintGetter callback invoker should be valid")));
		ASSERT_THAT(AreEqual(20, InitialGetterInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("BlueprintGetter callback should read the initial property value")));

		FFunctionInvoker SetterInvoker(*TestRunner, Actor, TEXT("SetAccessorValue"));
		ASSERT_THAT(IsTrue(SetterInvoker.IsValid(), TEXT("BlueprintSetter callback invoker should be valid")));
		SetterInvoker.AddParam<int32>(37);
		ASSERT_THAT(IsTrue(SetterInvoker.Call(), TEXT("BlueprintSetter callback should accept an int parameter")));

		FFunctionInvoker UpdatedGetterInvoker(*TestRunner, Actor, TEXT("GetAccessorValue"));
		ASSERT_THAT(IsTrue(UpdatedGetterInvoker.IsValid(), TEXT("BlueprintGetter callback invoker should stay valid after setter call")));
		ASSERT_THAT(AreEqual(37, UpdatedGetterInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("BlueprintSetter callback should update the value read by BlueprintGetter")));
	}
};

#endif
