#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptNativeScriptTestObject.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASClass.h"
#include "Misc/ScopeExit.h"
#include "UObject/FastReferenceCollector.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassReferenceSchemaTests,
	"Angelscript.TestModule.Generator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ReferenceSchemaModuleName = FName(TEXT("ASClassReferenceSchema"));
	inline static const FString ReferenceSchemaFilename = FString(TEXT("ASClassReferenceSchema.as"));
	inline static const FName ReferenceSchemaClassName = FName(TEXT("UReferenceSchemaHolder"));
	inline static const FName ReferenceSchemaSoftReloadModuleName = FName(TEXT("ASClassReferenceSchemaSoftReload"));
	inline static const FString ReferenceSchemaSoftReloadFilename = FString(TEXT("ASClassReferenceSchemaSoftReload.as"));
	inline static const FName ReferenceSchemaSoftReloadClassName = FName(TEXT("UReferenceSchemaReloadHolder"));

	struct FStoreParams
	{
		UObject* InValue = nullptr;
	};

	struct FGetStoredParams
	{
		UObject* ReturnValue = nullptr;
	};

	struct FGetVersionParams
	{
		int32 ReturnValue = 0;
	};

	static UFunction* RequireGeneratedFunction(FAutomationTestBase& Test, UClass* OwnerClass, FName FunctionName, const TCHAR* Context)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		FNoDiscardAsserter LocalAssert(Test);
		(void)LocalAssert.IsNotNull(Function, *FString::Printf(TEXT("%s should expose generated function '%s'"), Context, *FunctionName.ToString()));
		return Function;
	}

	static bool InvokeGeneratedFunction(FAngelscriptEngine& Engine, UObject* Object, UFunction* Function, void* Params)
	{
		if (!::IsValid(Object) || Function == nullptr)
		{
			return false;
		}

		FAngelscriptEngineScope FunctionScope(Engine, Object);
		if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{
			ScriptFunction->RuntimeCallEvent(Object, Params);
		}
		else
		{
			Object->ProcessEvent(Function, Params);
		}
		return true;
	}

	static int32 CountSchemaMembers(UE::GC::FSchemaView Schema)
	{
		if (Schema.IsEmpty())
		{
			return 0;
		}

		int32 Count = 0;
		for (const UE::GC::FMemberWord* WordIt = Schema.GetWords(); true; ++WordIt)
		{
			const UE::GC::Private::FMemberWordUnpacked Quad(WordIt->Members);
			for (UE::GC::Private::FMemberUnpacked Member : Quad.Members)
			{
				switch (Member.Type)
				{
				case UE::GC::EMemberType::StridedArray:
				case UE::GC::EMemberType::StructArray:
				case UE::GC::EMemberType::StructSet:
				case UE::GC::EMemberType::FreezableStructArray:
				case UE::GC::EMemberType::Optional:
				case UE::GC::EMemberType::MemberARO:
					++WordIt;
					break;
				case UE::GC::EMemberType::ARO:
				case UE::GC::EMemberType::SlowARO:
				case UE::GC::EMemberType::Stop:
					return Count;
				default:
					break;
				}
				++Count;
			}
		}
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

	TEST_METHOD(RuntimeAddReferencedObjectsKeepsScriptOnlyObjectReferenceAlive)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ReferenceSchemaModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UReferenceSchemaHolder : UObject
			{
				UObject HiddenRef = nullptr;

				UFUNCTION()
				void Store(UObject InValue)
				{
					HiddenRef = InValue;
				}

				UFUNCTION()
				UObject GetStored() const
				{
					return HiddenRef;
				}
			}
			)AS");

		UClass* ScriptClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ReferenceSchemaModuleName, ReferenceSchemaFilename, ScriptSource, ReferenceSchemaClassName);
		if (ScriptClass == nullptr) { return; }

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		if (!this->Assert.IsNotNull(ScriptASClass, TEXT("Reference-schema GC test case should compile to a UASClass"))) { return; }

		ASSERT_THAT(IsNull(FindFProperty<FProperty>(ScriptClass, TEXT("HiddenRef")), TEXT("Reference-schema GC test case should keep HiddenRef out of reflected UPROPERTY storage")));
		ASSERT_THAT(IsTrue(!ScriptASClass->ReferenceSchema.Get().IsEmpty(), TEXT("Reference-schema GC test case should build a non-empty GC schema")));

		UFunction* StoreFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("Store"), TEXT("Reference-schema GC test case"));
		UFunction* GetStoredFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("GetStored"), TEXT("Reference-schema GC test case"));
		if (StoreFunction == nullptr || GetStoredFunction == nullptr) { return; }

		UObject* Holder = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("ReferenceSchemaHolder"));
		if (!this->Assert.IsNotNull(Holder, TEXT("Reference-schema GC test case should instantiate the generated holder"))) { return; }

		Holder->AddToRoot();
		ON_SCOPE_EXIT
		{
			if (Holder != nullptr) { Holder->RemoveFromRoot(); Holder->MarkAsGarbage(); }
			CollectGarbage(RF_NoFlags, true);
		};

		UAngelscriptNativeScriptTestObject* StrongTarget = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("ReferenceSchemaTarget"));
		if (!this->Assert.IsNotNull(StrongTarget, TEXT("Reference-schema GC test case should create a transient target UObject"))) { return; }
		TWeakObjectPtr<UAngelscriptNativeScriptTestObject> WeakTarget = StrongTarget;

		FStoreParams StoreParams;
		StoreParams.InValue = StrongTarget;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, StoreFunction, &StoreParams), TEXT("Reference-schema GC test case should store the transient target"))) { return; }

		FGetStoredParams GetStoredBeforeGC;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredBeforeGC), TEXT("Reference-schema GC test case should read back stored object before GC"))) { return; }
		ASSERT_THAT(IsTrue(GetStoredBeforeGC.ReturnValue == StrongTarget, TEXT("Reference-schema GC test case should return the same target before GC")));

		StrongTarget = nullptr;
		CollectGarbage(RF_NoFlags, true);
		ASSERT_THAT(IsTrue(WeakTarget.IsValid(), TEXT("Reference-schema GC test case should keep target alive while rooted holder has script-only reference")));

		FGetStoredParams GetStoredAfterGC;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredAfterGC), TEXT("Reference-schema GC test case should still expose stored object after GC"))) { return; }
		ASSERT_THAT(IsTrue(GetStoredAfterGC.ReturnValue == WeakTarget.Get(), TEXT("Reference-schema GC test case should preserve same object identity after GC")));

		FStoreParams ClearParams;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, StoreFunction, &ClearParams), TEXT("Reference-schema GC test case should clear the script-only reference"))) { return; }

		FGetStoredParams GetStoredAfterClear;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredAfterClear), TEXT("Reference-schema GC test case should execute GetStored after clearing"))) { return; }
		ASSERT_THAT(IsNull(GetStoredAfterClear.ReturnValue, TEXT("Reference-schema GC test case should report null after clearing")));

		CollectGarbage(RF_NoFlags, true);
		ASSERT_THAT(IsFalse(WeakTarget.IsValid(), TEXT("Reference-schema GC test case should release target after clearing last reference")));
	}

	TEST_METHOD(ReferenceSchemaDoesNotDuplicateAcrossRepeatedSoftReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ReferenceSchemaSoftReloadModuleName.ToString());
		};

		auto MakeScript = [](int32 Version) -> FString
		{
			FString ScriptSource = ASTEST_AS(R"AS(
				UCLASS()
				class UReferenceSchemaReloadHolder : UObject
				{
					UObject HiddenRef = nullptr;

					UFUNCTION()
					void Store(UObject InValue)
					{
						HiddenRef = InValue;
					}

					UFUNCTION()
					UObject GetStored() const
					{
						return HiddenRef;
					}

					UFUNCTION()
					int GetVersion() const
					{
						return __Version__;
					}
				}
				)AS");
			ScriptSource.ReplaceInline(TEXT("__Version__"), *FString::FromInt(Version));
			return ScriptSource;
		};

		UClass* InitialClass = AngelscriptFunctionalTestUtils::CompileScriptModule(*TestRunner, Engine, ReferenceSchemaSoftReloadModuleName, ReferenceSchemaSoftReloadFilename, MakeScript(1), ReferenceSchemaSoftReloadClassName);
		if (InitialClass == nullptr) { return; }
		UASClass* InitialASClass = Cast<UASClass>(InitialClass);
		if (!this->Assert.IsNotNull(InitialASClass, TEXT("Reference-schema soft-reload should compile as UASClass"))) { return; }

		const int32 InitialMemberCount = CountSchemaMembers(InitialASClass->ReferenceSchema.Get());
		if (!this->Assert.IsTrue(InitialMemberCount > 0, TEXT("Reference-schema soft-reload should start with non-empty GC schema"))) { return; }

		UFunction* StoreFunction = RequireGeneratedFunction(*TestRunner, InitialClass, TEXT("Store"), TEXT("Reference-schema soft-reload"));
		UFunction* GetStoredFunction = RequireGeneratedFunction(*TestRunner, InitialClass, TEXT("GetStored"), TEXT("Reference-schema soft-reload"));
		UFunction* GetVersionFunction = RequireGeneratedFunction(*TestRunner, InitialClass, TEXT("GetVersion"), TEXT("Reference-schema soft-reload"));
		if (StoreFunction == nullptr || GetStoredFunction == nullptr || GetVersionFunction == nullptr) { return; }

		FGetVersionParams GetVersionBeforeReload;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, InitialASClass->GetDefaultObject(), GetVersionFunction, &GetVersionBeforeReload), TEXT("Should execute GetVersion before reload"))) { return; }
		ASSERT_THAT(AreEqual(1, GetVersionBeforeReload.ReturnValue, TEXT("Should start at version 1")));

		// First soft reload
		ECompileResult FirstReloadResult = ECompileResult::Error;
		if (!this->Assert.IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ReferenceSchemaSoftReloadModuleName, ReferenceSchemaSoftReloadFilename, MakeScript(2), FirstReloadResult),
				TEXT("First soft reload should compile")))
		{ return; }
		if (!this->Assert.IsTrue(FirstReloadResult == ECompileResult::FullyHandled || FirstReloadResult == ECompileResult::PartiallyHandled, TEXT("First reload should be handled")))
		{ return; }

		UASClass* FirstReloadClass = Cast<UASClass>(FindGeneratedClass(&Engine, ReferenceSchemaSoftReloadClassName));
		if (!this->Assert.IsNotNull(FirstReloadClass, TEXT("Should still expose holder after first reload"))) { return; }
		ASSERT_THAT(IsTrue(FirstReloadClass == InitialASClass, TEXT("Should preserve UASClass instance after first reload")));
		ASSERT_THAT(AreEqual(InitialMemberCount, CountSchemaMembers(FirstReloadClass->ReferenceSchema.Get()), TEXT("Schema member count should be stable after first reload")));

		UFunction* GetVersionAfterFirstReload = FindGeneratedFunction(FirstReloadClass, TEXT("GetVersion"));
		FGetVersionParams GetVersionAfterReloadOne;
		if (!this->Assert.IsNotNull(GetVersionAfterFirstReload, TEXT("Should still expose GetVersion after first reload"))) { return; }
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, FirstReloadClass->GetDefaultObject(), GetVersionAfterFirstReload, &GetVersionAfterReloadOne), TEXT("Should execute GetVersion after first reload"))) { return; }
		ASSERT_THAT(AreEqual(2, GetVersionAfterReloadOne.ReturnValue, TEXT("Should advance to version 2")));

		// Second soft reload
		ECompileResult SecondReloadResult = ECompileResult::Error;
		if (!this->Assert.IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ReferenceSchemaSoftReloadModuleName, ReferenceSchemaSoftReloadFilename, MakeScript(3), SecondReloadResult),
				TEXT("Second soft reload should compile")))
		{ return; }
		if (!this->Assert.IsTrue(SecondReloadResult == ECompileResult::FullyHandled || SecondReloadResult == ECompileResult::PartiallyHandled, TEXT("Second reload should be handled")))
		{ return; }

		UASClass* SecondReloadClass = Cast<UASClass>(FindGeneratedClass(&Engine, ReferenceSchemaSoftReloadClassName));
		if (!this->Assert.IsNotNull(SecondReloadClass, TEXT("Should still expose holder after second reload"))) { return; }
		ASSERT_THAT(IsTrue(SecondReloadClass == InitialASClass, TEXT("Should preserve UASClass instance after second reload")));
		ASSERT_THAT(AreEqual(InitialMemberCount, CountSchemaMembers(SecondReloadClass->ReferenceSchema.Get()), TEXT("Schema member count should be stable after second reload")));

		UFunction* GetVersionAfterSecondReload = FindGeneratedFunction(SecondReloadClass, TEXT("GetVersion"));
		FGetVersionParams GetVersionAfterReloadTwo;
		if (!this->Assert.IsNotNull(GetVersionAfterSecondReload, TEXT("Should still expose GetVersion after second reload"))) { return; }
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, SecondReloadClass->GetDefaultObject(), GetVersionAfterSecondReload, &GetVersionAfterReloadTwo), TEXT("Should execute GetVersion after second reload"))) { return; }
		ASSERT_THAT(AreEqual(3, GetVersionAfterReloadTwo.ReturnValue, TEXT("Should advance to version 3")));

		// Verify GC still works after repeated reloads
		UObject* Holder = NewObject<UObject>(GetTransientPackage(), SecondReloadClass, TEXT("ReferenceSchemaSoftReloadHolder"));
		if (!this->Assert.IsNotNull(Holder, TEXT("Should instantiate reloaded holder"))) { return; }
		Holder->AddToRoot();
		ON_SCOPE_EXIT
		{
			if (Holder != nullptr) { Holder->RemoveFromRoot(); Holder->MarkAsGarbage(); }
			CollectGarbage(RF_NoFlags, true);
		};

		UAngelscriptNativeScriptTestObject* StrongTarget = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("ReferenceSchemaSoftReloadTarget"));
		if (!this->Assert.IsNotNull(StrongTarget, TEXT("Should create transient target"))) { return; }
		TWeakObjectPtr<UAngelscriptNativeScriptTestObject> WeakTarget = StrongTarget;

		FStoreParams StoreParams;
		StoreParams.InValue = StrongTarget;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, StoreFunction, &StoreParams), TEXT("Should store target after repeated reloads"))) { return; }

		StrongTarget = nullptr;
		CollectGarbage(RF_NoFlags, true);
		ASSERT_THAT(IsTrue(WeakTarget.IsValid(), TEXT("Should keep target alive after repeated reloads")));

		FGetStoredParams GetStoredAfterGC;
		if (!this->Assert.IsTrue(InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredAfterGC), TEXT("Should expose stored object after repeated reloads and GC"))) { return; }
		ASSERT_THAT(IsTrue(GetStoredAfterGC.ReturnValue == WeakTarget.Get(), TEXT("Should preserve same stored object identity after repeated reloads")));
	}
};

#endif
