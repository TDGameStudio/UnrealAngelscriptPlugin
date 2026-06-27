#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadSoftReloadPropertyTests,
	"Angelscript.TestModule.HotReload.SoftReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName BasicModuleName = FName(TEXT("HotReloadPropertySoftBasic"));
	inline static const FString BasicFilename = FString(TEXT("HotReloadPropertySoftBasic.as"));
	inline static const FName BasicClassName = FName(TEXT("USoftReloadTarget"));

	inline static const FName PreserveModuleAName = FName(TEXT("HotReloadPropertySoftPreserveA"));
	inline static const FString PreserveModuleAFilename = FString(TEXT("HotReloadPropertySoftPreserveA.as"));
	inline static const FName PreserveModuleBName = FName(TEXT("HotReloadPropertySoftPreserveB"));
	inline static const FString PreserveModuleBFilename = FString(TEXT("HotReloadPropertySoftPreserveB.as"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

public:
	TEST_METHOD(SoftReloadUpdatesMemberFunctionBodyWithoutReplacingClass)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*BasicModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class USoftReloadTarget : UObject
			{
				UPROPERTY()
				int Version;

				default Version = 1;

				UFUNCTION()
				int GetVersion()
				{
					return Version;
				}
			}

			int GetSoftReloadVersion()
			{
				return 1;
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class USoftReloadTarget : UObject
			{
				UPROPERTY()
				int Version;

				default Version = 1;

				UFUNCTION()
				int GetVersion()
				{
					return Version + 1;
				}
			}

			int GetSoftReloadVersion()
			{
				return 2;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, BasicModuleName, BasicFilename, ScriptV1),
			TEXT("Initial soft-reload module compile should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, BasicClassName);
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Soft reload target class should exist before reload")));

		UFunction* GetVersionV1 = FindGeneratedFunction(ClassV1, TEXT("GetVersion"));
		ASSERT_THAT(IsNotNull(GetVersionV1, TEXT("GetVersion should exist before soft reload")));

		UObject* ObjectBeforeReload = NewObject<UObject>(GetTransientPackageAsObject(), ClassV1);
		ASSERT_THAT(IsNotNull(ObjectBeforeReload, TEXT("Soft reload target object should instantiate before reload")));

		int32 MemberResultBeforeReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, ObjectBeforeReload, GetVersionV1, MemberResultBeforeReload),
			TEXT("Member GetVersion should execute before soft reload")));
		ASSERT_THAT(AreEqual(1, MemberResultBeforeReload, TEXT("Member GetVersion should return v1 before soft reload")));

		int32 BeforeReloadResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, BasicModuleName, TEXT("int GetSoftReloadVersion()"), BeforeReloadResult),
			TEXT("Global version function should execute before soft reload")));
		ASSERT_THAT(AreEqual(1, BeforeReloadResult, TEXT("Global version should return v1 before soft reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, BasicModuleName, BasicFilename, ScriptV2, ReloadResult),
			TEXT("SoftReload compile wrapper should succeed")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("SoftReload compile should remain on the soft reload path")));

		UClass* ClassAfterReload = FindGeneratedClass(&Engine, BasicClassName);
		ASSERT_THAT(IsNotNull(ClassAfterReload, TEXT("Soft reload target class should exist after reload")));
		ASSERT_THAT(AreEqual(ClassV1, ClassAfterReload, TEXT("Soft reload should preserve the generated UClass instance")));

		UFunction* GetVersionAfterReload = FindGeneratedFunction(ClassAfterReload, TEXT("GetVersion"));
		ASSERT_THAT(IsNotNull(GetVersionAfterReload, TEXT("GetVersion should exist after soft reload")));

		int32 ExistingObjectMemberResultAfterReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, ObjectBeforeReload, GetVersionAfterReload, ExistingObjectMemberResultAfterReload),
			TEXT("Existing object should execute the reloaded member function body")));
		ASSERT_THAT(AreEqual(2, ExistingObjectMemberResultAfterReload, TEXT("Existing object member function should return v2 after soft reload")));

		UObject* ObjectAfterReload = NewObject<UObject>(GetTransientPackageAsObject(), ClassAfterReload);
		ASSERT_THAT(IsNotNull(ObjectAfterReload, TEXT("Soft reload target object should instantiate after reload")));

		int32 NewObjectMemberResultAfterReload = 0;
		ASSERT_THAT(IsTrue(
			ExecuteGeneratedIntEventOnGameThread(&Engine, ObjectAfterReload, GetVersionAfterReload, NewObjectMemberResultAfterReload),
			TEXT("New object should execute the reloaded member function body")));
		ASSERT_THAT(AreEqual(2, NewObjectMemberResultAfterReload, TEXT("New object member function should return v2 after soft reload")));

		int32 AfterReloadResult = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, BasicModuleName, TEXT("int GetSoftReloadVersion()"), AfterReloadResult),
			TEXT("Global version function should execute after soft reload")));
		ASSERT_THAT(AreEqual(2, AfterReloadResult, TEXT("Global version should return v2 after soft reload")));
	}

	TEST_METHOD(SoftReloadPreservesUnrelatedModuleExecution)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*PreserveModuleAName.ToString());
			Engine.DiscardModule(*PreserveModuleBName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptA = ASTEST_AS(R"AS(
			int GetValueA()
			{
				return 10;
			}
			)AS");

		const FString ScriptB = ASTEST_AS(R"AS(
			int GetValueB()
			{
				return 20;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(&Engine, PreserveModuleAName, PreserveModuleAFilename, ScriptA),
			TEXT("Module A should compile before soft reload preservation test")));
		ASSERT_THAT(IsTrue(
			CompileModuleFromMemory(&Engine, PreserveModuleBName, PreserveModuleBFilename, ScriptB),
			TEXT("Module B should compile before soft reload preservation test")));

		int32 ResultA = 0;
		int32 ResultB = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, PreserveModuleAName, TEXT("int GetValueA()"), ResultA),
			TEXT("Module A should execute before reload")));
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, PreserveModuleBName, TEXT("int GetValueB()"), ResultB),
			TEXT("Module B should execute before reload")));
		ASSERT_THAT(AreEqual(10, ResultA, TEXT("Module A should return its initial value before reload")));
		ASSERT_THAT(AreEqual(20, ResultB, TEXT("Module B should return its initial value before reload")));

		const FString ScriptAV2 = ASTEST_AS(R"AS(
			int GetValueA()
			{
				return 11;
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, PreserveModuleAName, PreserveModuleAFilename, ScriptAV2, ReloadResult),
			TEXT("Soft reload of module A should succeed")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Soft reload of module A should remain in soft reload path")));

		ResultA = 0;
		ResultB = 0;
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, PreserveModuleAName, TEXT("int GetValueA()"), ResultA),
			TEXT("Module A should execute after reload")));
		ASSERT_THAT(IsTrue(
			ExecuteIntFunction(&Engine, PreserveModuleBName, TEXT("int GetValueB()"), ResultB),
			TEXT("Module B should still execute after module A reload")));

		ASSERT_THAT(AreEqual(11, ResultA, TEXT("Module A should reflect its reloaded implementation")));
		ASSERT_THAT(AreEqual(20, ResultB, TEXT("Module B should preserve its original implementation")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadFullReloadPropertyTests,
	"Angelscript.TestModule.HotReload.FullReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName BasicModuleName = FName(TEXT("HotReloadPropertyFullBasic"));
	inline static const FString BasicFilename = FString(TEXT("HotReloadPropertyFullBasic.as"));
	inline static const FName BasicClassName = FName(TEXT("UFullReloadTarget"));

	inline static const FName EnumModuleName = FName(TEXT("HotReloadPropertyFullEnum"));
	inline static const FString EnumFilename = FString(TEXT("HotReloadPropertyFullEnum.as"));
	inline static const FName EnumClassName = FName(TEXT("UFullReloadEnumTarget"));
	inline static const FString EnumName = FString(TEXT("EFullReloadEnumState"));

	inline static const FName FailureModuleName = FName(TEXT("HotReloadPropertyFullFailure"));
	inline static const FString FailureFilename = FString(TEXT("HotReloadPropertyFullFailure.as"));
	inline static const FName FailureClassName = FName(TEXT("UFullReloadFailureTarget"));

	inline static const FName RemovalModuleName = FName(TEXT("HotReloadPropertyFullRemoval"));
	inline static const FString RemovalFilename = FString(TEXT("HotReloadPropertyFullRemoval.as"));
	inline static const FName RemovalClassName = FName(TEXT("UFullReloadRemovalTarget"));

	inline static const FName TypeChangeModuleName = FName(TEXT("HotReloadPropertyFullTypeChange"));
	inline static const FString TypeChangeFilename = FString(TEXT("HotReloadPropertyFullTypeChange.as"));
	inline static const FName TypeChangeClassName = FName(TEXT("UFullReloadTypeChangeTarget"));

	inline static const FName SpecifierModuleName = FName(TEXT("HotReloadPropertyFullSpecifier"));
	inline static const FString SpecifierFilename = FString(TEXT("HotReloadPropertyFullSpecifier.as"));
	inline static const FName SpecifierClassName = FName(TEXT("UFullReloadSpecifierTarget"));

	inline static const FName MultiClassModuleName = FName(TEXT("HotReloadPropertyFullMultiClass"));
	inline static const FString MultiClassFilename = FString(TEXT("HotReloadPropertyFullMultiClass.as"));
	inline static const FName MultiClassAName = FName(TEXT("UFullReloadMultiClassA"));
	inline static const FName MultiClassBName = FName(TEXT("UFullReloadMultiClassB"));

	inline static const FName InheritanceModuleName = FName(TEXT("HotReloadPropertyFullInheritance"));
	inline static const FString InheritanceFilename = FString(TEXT("HotReloadPropertyFullInheritance.as"));
	inline static const FName InheritanceBaseClassName = FName(TEXT("UFullReloadBaseTarget"));
	inline static const FName InheritanceChildClassName = FName(TEXT("UFullReloadChildTarget"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static bool IsErrorReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::Error || ReloadResult == ECompileResult::ErrorNeedFullReload;
	}

	static bool TryFindEnumValueBySuffix(const UEnum& Enum, const TCHAR* Suffix, int64& OutValue)
	{
		const FString ExpectedSuffix(Suffix);
		const int32 EnumeratorsToCheck = Enum.NumEnums();
		for (int32 Index = 0; Index < EnumeratorsToCheck; ++Index)
		{
			const FString EnumEntryName = Enum.GetNameByIndex(Index).ToString();
			if (EnumEntryName.EndsWith(TEXT("_MAX")))
			{
				continue;
			}

			if (EnumEntryName.EndsWith(ExpectedSuffix))
			{
				OutValue = Enum.GetValueByIndex(Index);
				return true;
			}
		}

		return false;
	}

	static int64 ReadEnumPropertyValue(const FEnumProperty& Property, const UObject& Object)
	{
		const uint8* ValuePtr = Property.ContainerPtrToValuePtr<uint8>(&Object);
		return Property.GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
	}

public:
	TEST_METHOD(FullReloadAddsPropertyAndUpdatesDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*BasicModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadTarget : UObject
			{
				UPROPERTY()
				int Version;

				default Version = 1;

				UFUNCTION()
				int GetVersion()
				{
					return Version;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadTarget : UObject
			{
				UPROPERTY()
				int Version;

				UPROPERTY()
				int Mana;

				default Version = 2;
				default Mana = 5;

				UFUNCTION()
				int GetVersion()
				{
					return Version;
				}

				UFUNCTION()
				int GetMana()
				{
					return Mana;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, BasicModuleName, BasicFilename, ScriptV1),
			TEXT("Initial full-reload module compile should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, BasicClassName);
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Full reload target class should exist before reload")));

		FIntProperty* OldVersionProperty = FindFProperty<FIntProperty>(ClassV1, TEXT("Version"));
		ASSERT_THAT(IsNotNull(OldVersionProperty, TEXT("Version property should exist before full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassV1, TEXT("Mana")), TEXT("Old class should not expose Mana before full reload")));

		UObject* ObjV1 = NewObject<UObject>(GetTransientPackageAsObject(), ClassV1);
		ASSERT_THAT(IsNotNull(ObjV1, TEXT("Full reload target object should instantiate before reload")));
		const int32 PreReloadOldVersion = OldVersionProperty->GetPropertyValue_InContainer(ObjV1);
		ASSERT_THAT(AreEqual(1, PreReloadOldVersion, TEXT("Old class default should be v1 before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, BasicModuleName, BasicFilename, ScriptV2, ReloadResult),
			TEXT("Full reload compile wrapper should succeed")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Full reload compile should be handled without fatal compile error")));

		UClass* ClassV2 = FindGeneratedClass(&Engine, BasicClassName);
		ASSERT_THAT(IsNotNull(ClassV2, TEXT("Full reload target class should exist after reload")));
		ASSERT_THAT(AreNotEqual(ClassV1, ClassV2, TEXT("Full reload should replace the generated UClass instance")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassV1, TEXT("Mana")), TEXT("Old class should keep its original reflected layout")));

		FIntProperty* VersionProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Version"));
		FIntProperty* ManaProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Mana"));
		UFunction* GetVersionFunction = FindGeneratedFunction(ClassV2, TEXT("GetVersion"));
		UFunction* GetManaFunction = FindGeneratedFunction(ClassV2, TEXT("GetMana"));
		ASSERT_THAT(IsNotNull(VersionProperty, TEXT("Version property should exist after full reload")));
		ASSERT_THAT(IsNotNull(ManaProperty, TEXT("Mana property should exist after full reload")));
		ASSERT_THAT(IsNotNull(GetVersionFunction, TEXT("GetVersion should exist after full reload")));
		ASSERT_THAT(IsNotNull(GetManaFunction, TEXT("GetMana should exist after full reload")));

		UObject* ObjV2 = NewObject<UObject>(GetTransientPackageAsObject(), ClassV2);
		ASSERT_THAT(IsNotNull(ObjV2, TEXT("Full reload target object should instantiate after reload")));

		ASSERT_THAT(AreEqual(2, VersionProperty->GetPropertyValue_InContainer(ObjV2), TEXT("Version default should update after full reload")));
		ASSERT_THAT(AreEqual(5, ManaProperty->GetPropertyValue_InContainer(ObjV2), TEXT("Mana default should be introduced after full reload")));
	}

	TEST_METHOD(FullReloadUpdatesEnumValueAndDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*EnumModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EFullReloadEnumState : uint16
			{
				Alpha,
				Beta = 4
			}

			UCLASS()
			class UFullReloadEnumTarget : UObject
			{
				UPROPERTY()
				EFullReloadEnumState State;

				default State = EFullReloadEnumState::Alpha;
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum class EFullReloadEnumState : uint16
			{
				Alpha,
				Beta = 4,
				Gamma = 9
			}

			UCLASS()
			class UFullReloadEnumTarget : UObject
			{
				UPROPERTY()
				EFullReloadEnumState State;

				default State = EFullReloadEnumState::Gamma;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, EnumModuleName, EnumFilename, ScriptV1),
			TEXT("Initial enum full-reload module compile should succeed")));

		const TSharedPtr<FAngelscriptEnumDesc> EnumBeforeReload = Engine.GetEnum(EnumName);
		ASSERT_THAT(IsTrue(EnumBeforeReload.IsValid(), TEXT("Enum metadata should exist before full reload")));
		ASSERT_THAT(IsNotNull(EnumBeforeReload->Enum, TEXT("Enum UObject should exist before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, EnumModuleName, EnumFilename, ScriptV2, ReloadResult),
			TEXT("Full reload for enum-bearing module should succeed")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Enum full reload should complete without fatal compile error")));

		const TSharedPtr<FAngelscriptEnumDesc> EnumAfterReload = Engine.GetEnum(EnumName);
		ASSERT_THAT(IsTrue(EnumAfterReload.IsValid(), TEXT("Enum metadata should still exist after full reload")));
		ASSERT_THAT(IsNotNull(EnumAfterReload->Enum, TEXT("Enum UObject should still exist after full reload")));
		ASSERT_THAT(IsTrue(EnumAfterReload->EnumValues.Contains(9), TEXT("Enum metadata should include the new Gamma value")));

		int64 GammaValue = INDEX_NONE;
		ASSERT_THAT(IsTrue(
			TryFindEnumValueBySuffix(*EnumAfterReload->Enum, TEXT("Gamma"), GammaValue),
			TEXT("Reloaded enum UObject should expose the new Gamma enumerator")));
		ASSERT_THAT(AreEqual(9LL, GammaValue, TEXT("Reloaded enum Gamma enumerator should keep its explicit value")));

		UClass* ReloadedClass = FindGeneratedClass(&Engine, EnumClassName);
		ASSERT_THAT(IsNotNull(ReloadedClass, TEXT("Enum carrier class should still exist after full reload")));

		FEnumProperty* StateProperty = FindFProperty<FEnumProperty>(ReloadedClass, TEXT("State"));
		ASSERT_THAT(IsNotNull(StateProperty, TEXT("Enum-backed property should still exist after full reload")));

		UObject* ReloadedObject = NewObject<UObject>(GetTransientPackageAsObject(), ReloadedClass);
		ASSERT_THAT(IsNotNull(ReloadedObject, TEXT("Enum carrier object should instantiate after full reload")));
		ASSERT_THAT(AreEqual(
			9LL,
			ReadEnumPropertyValue(*StateProperty, *ReloadedObject),
			TEXT("Enum-backed property default should update to Gamma after full reload")));
	}

	TEST_METHOD(FailedReloadKeepsOldClassAndProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FailureModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadFailureTarget : UObject
			{
				UPROPERTY()
				int Value;

				default Value = 7;
			}
			)AS");

		const FString BrokenReloadSource = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadFailureTarget : UObject
			{
				UPROPERTY()
				int Value;

				UPROPERTY()
				int PollutedValue;

				default Value = 99;
				default PollutedValue = 13;

				UFUNCTION()
				MissingType GetMissingValue()
				{
					MissingType Value;
					return Value;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, FailureModuleName, FailureFilename, ReloadV1Source),
			TEXT("Initial failure-isolation module compile should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, FailureClassName);
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Failure-isolation target class should exist before broken reload")));

		FIntProperty* OldValueProperty = FindFProperty<FIntProperty>(ClassV1, TEXT("Value"));
		ASSERT_THAT(IsNotNull(OldValueProperty, TEXT("Failure-isolation target should expose Value before broken reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassV1, TEXT("PollutedValue")), TEXT("Old class should not expose future PollutedValue before broken reload")));

		UObject* ObjectBeforeFailure = NewObject<UObject>(GetTransientPackageAsObject(), ClassV1);
		ASSERT_THAT(IsNotNull(ObjectBeforeFailure, TEXT("Failure-isolation target should instantiate before broken reload")));
		ASSERT_THAT(AreEqual(7, OldValueProperty->GetPropertyValue_InContainer(ObjectBeforeFailure), TEXT("Old class default should be visible before broken reload")));

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FailureModuleName,
			FailureFilename,
			BrokenReloadSource,
			/*bUsePreprocessor=*/ true,
			Summary,
			/*bSuppressCompileErrorLogs=*/ true);
		ASSERT_THAT(IsFalse(bCompiled, TEXT("Broken full reload should fail to compile")));
		ASSERT_THAT(IsTrue(IsErrorReloadResult(Summary.CompileResult), TEXT("Broken full reload should report an error reload state")));

		UClass* ClassAfterFailure = FindGeneratedClass(&Engine, FailureClassName);
		ASSERT_THAT(AreEqual(ClassV1, ClassAfterFailure, TEXT("Failed full reload should leave the old generated class active")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassAfterFailure, TEXT("PollutedValue")), TEXT("Failed full reload should not publish rejected properties")));

		UObject* ObjectAfterFailure = NewObject<UObject>(GetTransientPackageAsObject(), ClassAfterFailure);
		ASSERT_THAT(IsNotNull(ObjectAfterFailure, TEXT("Failure-isolation target should instantiate after broken reload")));
		FIntProperty* ValueAfterFailureProperty = FindFProperty<FIntProperty>(ClassAfterFailure, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueAfterFailureProperty, TEXT("Old Value property should remain active after broken reload")));
		ASSERT_THAT(AreEqual(7, ValueAfterFailureProperty->GetPropertyValue_InContainer(ObjectAfterFailure), TEXT("Failed full reload should keep old property defaults active")));
	}

	TEST_METHOD(PropertyRemovalDropsFieldFromReplacementClass)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*RemovalModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadRemovalTarget : UObject
			{
				UPROPERTY()
				int Value;

				UPROPERTY()
				int RemovedValue;

				default Value = 3;
				default RemovedValue = 4;
			}
			)AS");

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadRemovalTarget : UObject
			{
				UPROPERTY()
				int Value;

				default Value = 5;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, RemovalModuleName, RemovalFilename, ReloadV1Source),
			TEXT("Initial removal module compile should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, RemovalClassName);
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Removal target class should exist before full reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassV1, TEXT("RemovedValue")), TEXT("Old class should expose RemovedValue before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RemovalModuleName, RemovalFilename, ReloadV2Source, ReloadResult),
			TEXT("Removal full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Removal full reload should finish on a handled path")));

		UClass* ClassV2 = FindGeneratedClass(&Engine, RemovalClassName);
		ASSERT_THAT(IsNotNull(ClassV2, TEXT("Removal target class should exist after full reload")));
		ASSERT_THAT(AreNotEqual(ClassV1, ClassV2, TEXT("Removal full reload should replace the generated UClass instance")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassV2, TEXT("RemovedValue")), TEXT("Replacement class should drop removed property")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassV1, TEXT("RemovedValue")), TEXT("Old class should keep its original removed-property layout")));

		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(ClassV2, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueProperty, TEXT("Replacement class should keep Value")));

		UObject* ObjectV2 = NewObject<UObject>(GetTransientPackageAsObject(), ClassV2);
		ASSERT_THAT(IsNotNull(ObjectV2, TEXT("Removal target should instantiate after full reload")));
		ASSERT_THAT(AreEqual(5, ValueProperty->GetPropertyValue_InContainer(ObjectV2), TEXT("Replacement class should use updated surviving property default")));
	}

	TEST_METHOD(PropertyTypeChangeReplacesFieldType)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*TypeChangeModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadTypeChangeTarget : UObject
			{
				UPROPERTY()
				int Value;

				default Value = 8;
			}
			)AS");

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadTypeChangeTarget : UObject
			{
				UPROPERTY()
				FString Value;

				default Value = "Reloaded";
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, TypeChangeModuleName, TypeChangeFilename, ReloadV1Source),
			TEXT("Initial type-change module compile should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, TypeChangeClassName);
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Type-change target class should exist before full reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassV1, TEXT("Value")), TEXT("Old class should expose Value as int before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, TypeChangeModuleName, TypeChangeFilename, ReloadV2Source, ReloadResult),
			TEXT("Type-change full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Type-change full reload should finish on a handled path")));

		UClass* ClassV2 = FindGeneratedClass(&Engine, TypeChangeClassName);
		ASSERT_THAT(IsNotNull(ClassV2, TEXT("Type-change target class should exist after full reload")));
		ASSERT_THAT(AreNotEqual(ClassV1, ClassV2, TEXT("Type-change full reload should replace the generated UClass instance")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassV1, TEXT("Value")), TEXT("Old class should keep Value as int after full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassV2, TEXT("Value")), TEXT("Replacement class should no longer expose Value as int")));

		FStrProperty* ValueProperty = FindFProperty<FStrProperty>(ClassV2, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueProperty, TEXT("Replacement class should expose Value as FString")));

		UObject* ObjectV2 = NewObject<UObject>(GetTransientPackageAsObject(), ClassV2);
		ASSERT_THAT(IsNotNull(ObjectV2, TEXT("Type-change target should instantiate after full reload")));
		ASSERT_THAT(AreEqual(FString(TEXT("Reloaded")), ValueProperty->GetPropertyValue_InContainer(ObjectV2), TEXT("Replacement class should use the updated FString default")));
	}

	TEST_METHOD(PropertySpecifierReloadUpdatesFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*SpecifierModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadSpecifierTarget : UObject
			{
				UPROPERTY(NotEditable)
				int Value;

				default Value = 1;
			}
			)AS");

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadSpecifierTarget : UObject
			{
				UPROPERTY(EditAnywhere)
				int Value;

				default Value = 2;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, SpecifierModuleName, SpecifierFilename, ReloadV1Source),
			TEXT("Initial specifier module compile should succeed")));

		UClass* ClassV1 = FindGeneratedClass(&Engine, SpecifierClassName);
		ASSERT_THAT(IsNotNull(ClassV1, TEXT("Specifier target class should exist before full reload")));

		FIntProperty* ValueBeforeReload = FindFProperty<FIntProperty>(ClassV1, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueBeforeReload, TEXT("Specifier target should expose Value before full reload")));
		ASSERT_THAT(IsFalse(ValueBeforeReload->HasAnyPropertyFlags(CPF_Edit), TEXT("NotEditable should keep CPF_Edit unset before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, SpecifierModuleName, SpecifierFilename, ReloadV2Source, ReloadResult),
			TEXT("Specifier full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Specifier full reload should finish on a handled path")));

		UClass* ClassV2 = FindGeneratedClass(&Engine, SpecifierClassName);
		ASSERT_THAT(IsNotNull(ClassV2, TEXT("Specifier target class should exist after full reload")));
		ASSERT_THAT(AreNotEqual(ClassV1, ClassV2, TEXT("Specifier full reload should replace the generated UClass instance")));

		FIntProperty* ValueAfterReload = FindFProperty<FIntProperty>(ClassV2, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ValueAfterReload, TEXT("Specifier target should expose Value after full reload")));
		ASSERT_THAT(IsTrue(ValueAfterReload->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should set CPF_Edit after full reload")));
		ASSERT_THAT(IsFalse(ValueBeforeReload->HasAnyPropertyFlags(CPF_Edit), TEXT("Old class should keep the original NotEditable flag")));

		UObject* ObjectV2 = NewObject<UObject>(GetTransientPackageAsObject(), ClassV2);
		ASSERT_THAT(IsNotNull(ObjectV2, TEXT("Specifier target should instantiate after full reload")));
		ASSERT_THAT(AreEqual(2, ValueAfterReload->GetPropertyValue_InContainer(ObjectV2), TEXT("Specifier reload should update property default with flags")));
	}

	TEST_METHOD(MultiClassModuleReloadUpdatesChangedClassAndKeepsSiblingQueryable)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*MultiClassModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadMultiClassA : UObject
			{
				UPROPERTY()
				int ValueA;

				default ValueA = 1;
			}

			UCLASS()
			class UFullReloadMultiClassB : UObject
			{
				UPROPERTY()
				int ValueB;

				default ValueB = 10;
			}
			)AS");

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadMultiClassA : UObject
			{
				UPROPERTY()
				int ValueA;

				UPROPERTY()
				int AddedA;

				default ValueA = 2;
				default AddedA = 3;
			}

			UCLASS()
			class UFullReloadMultiClassB : UObject
			{
				UPROPERTY()
				int ValueB;

				default ValueB = 10;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, MultiClassModuleName, MultiClassFilename, ReloadV1Source),
			TEXT("Initial multi-class module compile should succeed")));

		UClass* ClassAV1 = FindGeneratedClass(&Engine, MultiClassAName);
		UClass* ClassBV1 = FindGeneratedClass(&Engine, MultiClassBName);
		ASSERT_THAT(IsNotNull(ClassAV1, TEXT("Multi-class A should exist before full reload")));
		ASSERT_THAT(IsNotNull(ClassBV1, TEXT("Multi-class B should exist before full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassAV1, TEXT("AddedA")), TEXT("Multi-class A should not expose AddedA before full reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassBV1, TEXT("ValueB")), TEXT("Multi-class B should expose ValueB before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, MultiClassModuleName, MultiClassFilename, ReloadV2Source, ReloadResult),
			TEXT("Multi-class full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Multi-class full reload should finish on a handled path")));

		UClass* ClassAV2 = FindGeneratedClass(&Engine, MultiClassAName);
		UClass* ClassBV2 = FindGeneratedClass(&Engine, MultiClassBName);
		ASSERT_THAT(IsNotNull(ClassAV2, TEXT("Multi-class A should exist after full reload")));
		ASSERT_THAT(IsNotNull(ClassBV2, TEXT("Multi-class B should remain queryable after sibling reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassAV2, TEXT("AddedA")), TEXT("Multi-class A should expose the added property after full reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(ClassBV2, TEXT("ValueB")), TEXT("Multi-class B should preserve its original property after full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ClassBV2, TEXT("AddedA")), TEXT("Multi-class B should not receive sibling properties after full reload")));

		UObject* ObjectA = NewObject<UObject>(GetTransientPackageAsObject(), ClassAV2);
		UObject* ObjectB = NewObject<UObject>(GetTransientPackageAsObject(), ClassBV2);
		ASSERT_THAT(IsNotNull(ObjectA, TEXT("Multi-class A should instantiate after full reload")));
		ASSERT_THAT(IsNotNull(ObjectB, TEXT("Multi-class B should instantiate after full reload")));

		FIntProperty* ValueA = FindFProperty<FIntProperty>(ClassAV2, TEXT("ValueA"));
		FIntProperty* AddedA = FindFProperty<FIntProperty>(ClassAV2, TEXT("AddedA"));
		FIntProperty* ValueB = FindFProperty<FIntProperty>(ClassBV2, TEXT("ValueB"));
		ASSERT_THAT(IsNotNull(ValueA, TEXT("Multi-class A should keep ValueA after full reload")));
		ASSERT_THAT(IsNotNull(AddedA, TEXT("Multi-class A should expose AddedA after full reload")));
		ASSERT_THAT(IsNotNull(ValueB, TEXT("Multi-class B should keep ValueB after full reload")));
		ASSERT_THAT(AreEqual(2, ValueA->GetPropertyValue_InContainer(ObjectA), TEXT("Multi-class A should use its updated default after full reload")));
		ASSERT_THAT(AreEqual(3, AddedA->GetPropertyValue_InContainer(ObjectA), TEXT("Multi-class A should use its added-property default after full reload")));
		ASSERT_THAT(AreEqual(10, ValueB->GetPropertyValue_InContainer(ObjectB), TEXT("Multi-class B should preserve its default after sibling reload")));
	}

	TEST_METHOD(InheritanceChainPropertyReloadPropagatesToChild)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*InheritanceModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadBaseTarget : UObject
			{
				UPROPERTY()
				int BaseValue;

				default BaseValue = 4;
			}

			UCLASS()
			class UFullReloadChildTarget : UFullReloadBaseTarget
			{
				UPROPERTY()
				int ChildValue;

				default ChildValue = 6;
			}
			)AS");

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			UCLASS()
			class UFullReloadBaseTarget : UObject
			{
				UPROPERTY()
				int BaseValue;

				UPROPERTY()
				int AddedBaseValue;

				default BaseValue = 5;
				default AddedBaseValue = 9;
			}

			UCLASS()
			class UFullReloadChildTarget : UFullReloadBaseTarget
			{
				UPROPERTY()
				int ChildValue;

				default ChildValue = 6;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, InheritanceModuleName, InheritanceFilename, ReloadV1Source),
			TEXT("Initial inheritance module compile should succeed")));

		UClass* BaseClassV1 = FindGeneratedClass(&Engine, InheritanceBaseClassName);
		UClass* ChildClassV1 = FindGeneratedClass(&Engine, InheritanceChildClassName);
		ASSERT_THAT(IsNotNull(BaseClassV1, TEXT("Inheritance base class should exist before full reload")));
		ASSERT_THAT(IsNotNull(ChildClassV1, TEXT("Inheritance child class should exist before full reload")));
		ASSERT_THAT(IsTrue(ChildClassV1->IsChildOf(BaseClassV1), TEXT("Inheritance child should derive from base before full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(ChildClassV1, TEXT("AddedBaseValue")), TEXT("Inheritance child should not expose future base property before full reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, InheritanceModuleName, InheritanceFilename, ReloadV2Source, ReloadResult),
			TEXT("Inheritance full reload should compile")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Inheritance full reload should finish on a handled path")));

		UClass* BaseClassV2 = FindGeneratedClass(&Engine, InheritanceBaseClassName);
		UClass* ChildClassV2 = FindGeneratedClass(&Engine, InheritanceChildClassName);
		ASSERT_THAT(IsNotNull(BaseClassV2, TEXT("Inheritance base class should exist after full reload")));
		ASSERT_THAT(IsNotNull(ChildClassV2, TEXT("Inheritance child class should exist after full reload")));
		ASSERT_THAT(AreNotEqual(BaseClassV1, BaseClassV2, TEXT("Inheritance base full reload should replace the base UClass instance")));
		ASSERT_THAT(AreNotEqual(ChildClassV1, ChildClassV2, TEXT("Inheritance full reload should replace the child UClass instance")));
		ASSERT_THAT(IsTrue(ChildClassV2->IsChildOf(BaseClassV2), TEXT("Inheritance child should derive from reloaded base after full reload")));

		FIntProperty* BaseValue = FindFProperty<FIntProperty>(ChildClassV2, TEXT("BaseValue"));
		FIntProperty* AddedBaseValue = FindFProperty<FIntProperty>(ChildClassV2, TEXT("AddedBaseValue"));
		FIntProperty* ChildValue = FindFProperty<FIntProperty>(ChildClassV2, TEXT("ChildValue"));
		ASSERT_THAT(IsNotNull(BaseValue, TEXT("Reloaded child should inherit existing base property")));
		ASSERT_THAT(IsNotNull(AddedBaseValue, TEXT("Reloaded child should inherit added base property")));
		ASSERT_THAT(IsNotNull(ChildValue, TEXT("Reloaded child should keep child property")));

		UObject* ChildCDO = ChildClassV2->GetDefaultObject(false);
		ASSERT_THAT(IsNotNull(ChildCDO, TEXT("Reloaded child should have an initialized CDO after full reload")));
		const int32 BaseCDOValue = BaseValue->GetPropertyValue_InContainer(ChildCDO);
		const int32 AddedBaseCDOValue = AddedBaseValue->GetPropertyValue_InContainer(ChildCDO);
		const int32 ChildCDOValue = ChildValue->GetPropertyValue_InContainer(ChildCDO);
		ASSERT_THAT(AreEqual(
			6,
			ChildCDOValue,
			*FString::Printf(
				TEXT("Reloaded child CDO should keep child default, BaseValue=%d AddedBaseValue=%d ChildValue=%d"),
				BaseCDOValue,
				AddedBaseCDOValue,
				ChildCDOValue)));

		UObject* ChildObject = NewObject<UObject>(GetTransientPackageAsObject(), ChildClassV2);
		ASSERT_THAT(IsNotNull(ChildObject, TEXT("Reloaded child should instantiate after inherited property reload")));
		const int32 BaseObjectValue = BaseValue->GetPropertyValue_InContainer(ChildObject);
		const int32 AddedBaseObjectValue = AddedBaseValue->GetPropertyValue_InContainer(ChildObject);
		const int32 ChildObjectValue = ChildValue->GetPropertyValue_InContainer(ChildObject);
		ASSERT_THAT(AreEqual(5, BaseObjectValue, *FString::Printf(TEXT("Reloaded child should read updated inherited base default, actual=%d"), BaseObjectValue)));
		ASSERT_THAT(AreEqual(9, AddedBaseObjectValue, *FString::Printf(TEXT("Reloaded child should read added inherited base default, actual=%d"), AddedBaseObjectValue)));
		ASSERT_THAT(AreEqual(6, ChildObjectValue, *FString::Printf(TEXT("Reloaded child should keep child default, actual=%d"), ChildObjectValue)));
	}
};

#endif
