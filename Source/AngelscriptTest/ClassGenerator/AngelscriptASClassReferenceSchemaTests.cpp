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
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_ClassGenerator_AngelscriptASClassReferenceSchemaTests_Private
{
	static const FName ReferenceSchemaModuleName(TEXT("ASClassReferenceSchema"));
	static const FString ReferenceSchemaFilename(TEXT("ASClassReferenceSchema.as"));
	static const FName ReferenceSchemaClassName(TEXT("UReferenceSchemaHolder"));
	static const FName ReferenceSchemaSoftReloadModuleName(TEXT("ASClassReferenceSchemaSoftReload"));
	static const FString ReferenceSchemaSoftReloadFilename(TEXT("ASClassReferenceSchemaSoftReload.as"));
	static const FName ReferenceSchemaSoftReloadClassName(TEXT("UReferenceSchemaReloadHolder"));

	struct FStoreParams { UObject* InValue = nullptr; };
	struct FGetStoredParams { UObject* ReturnValue = nullptr; };
	struct FGetVersionParams { int32 ReturnValue = 0; };

	UFunction* RequireGeneratedFunction(FAutomationTestBase& Test, UClass* OwnerClass, FName FunctionName, const TCHAR* Context)
	{
		UFunction* Function = FindGeneratedFunction(OwnerClass, FunctionName);
		Test.TestNotNull(*FString::Printf(TEXT("%s should expose generated function '%s'"), Context, *FunctionName.ToString()), Function);
		return Function;
	}

	bool InvokeGeneratedFunction(FAngelscriptEngine& Engine, UObject* Object, UFunction* Function, void* Params)
	{
		if (!::IsValid(Object) || Function == nullptr) { return false; }
		FAngelscriptEngineScope FunctionScope(Engine, Object);
		if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
		{ ScriptFunction->RuntimeCallEvent(Object, Params); }
		else
		{ Object->ProcessEvent(Function, Params); }
		return true;
	}

	int32 CountSchemaMembers(UE::GC::FSchemaView Schema)
	{
		if (Schema.IsEmpty()) { return 0; }
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASClassReferenceSchemaTests,
	"Angelscript.TestModule.ClassGenerator.ASClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RuntimeAddReferencedObjectsKeepsScriptOnlyObjectReferenceAlive)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASClassReferenceSchemaTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ReferenceSchemaModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptSource = TEXT(R"AS(
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

		UClass* ScriptClass = CompileScriptModule(*TestRunner, Engine, ReferenceSchemaModuleName, ReferenceSchemaFilename, ScriptSource, ReferenceSchemaClassName);
		if (ScriptClass == nullptr) { return; }

		UASClass* ScriptASClass = Cast<UASClass>(ScriptClass);
		if (!TestRunner->TestNotNull(TEXT("Reference-schema GC test case should compile to a UASClass"), ScriptASClass)) { return; }

		TestRunner->TestNull(TEXT("Reference-schema GC test case should keep HiddenRef out of reflected UPROPERTY storage"), FindFProperty<FProperty>(ScriptClass, TEXT("HiddenRef")));
		TestRunner->TestTrue(TEXT("Reference-schema GC test case should build a non-empty GC schema"), !ScriptASClass->ReferenceSchema.Get().IsEmpty());

		UFunction* StoreFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("Store"), TEXT("Reference-schema GC test case"));
		UFunction* GetStoredFunction = RequireGeneratedFunction(*TestRunner, ScriptClass, TEXT("GetStored"), TEXT("Reference-schema GC test case"));
		if (StoreFunction == nullptr || GetStoredFunction == nullptr) { return; }

		UObject* Holder = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("ReferenceSchemaHolder"));
		if (!TestRunner->TestNotNull(TEXT("Reference-schema GC test case should instantiate the generated holder"), Holder)) { return; }

		Holder->AddToRoot();
		ON_SCOPE_EXIT
		{
			if (Holder != nullptr) { Holder->RemoveFromRoot(); Holder->MarkAsGarbage(); }
			CollectGarbage(RF_NoFlags, true);
		};

		UAngelscriptNativeScriptTestObject* StrongTarget = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("ReferenceSchemaTarget"));
		if (!TestRunner->TestNotNull(TEXT("Reference-schema GC test case should create a transient target UObject"), StrongTarget)) { return; }
		TWeakObjectPtr<UAngelscriptNativeScriptTestObject> WeakTarget = StrongTarget;

		FStoreParams StoreParams;
		StoreParams.InValue = StrongTarget;
		if (!TestRunner->TestTrue(TEXT("Reference-schema GC test case should store the transient target"), InvokeGeneratedFunction(Engine, Holder, StoreFunction, &StoreParams))) { return; }

		FGetStoredParams GetStoredBeforeGC;
		if (!TestRunner->TestTrue(TEXT("Reference-schema GC test case should read back stored object before GC"), InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredBeforeGC))) { return; }
		TestRunner->TestTrue(TEXT("Reference-schema GC test case should return the same target before GC"), GetStoredBeforeGC.ReturnValue == StrongTarget);

		StrongTarget = nullptr;
		CollectGarbage(RF_NoFlags, true);
		TestRunner->TestTrue(TEXT("Reference-schema GC test case should keep target alive while rooted holder has script-only reference"), WeakTarget.IsValid());

		FGetStoredParams GetStoredAfterGC;
		if (!TestRunner->TestTrue(TEXT("Reference-schema GC test case should still expose stored object after GC"), InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredAfterGC))) { return; }
		TestRunner->TestTrue(TEXT("Reference-schema GC test case should preserve same object identity after GC"), GetStoredAfterGC.ReturnValue == WeakTarget.Get());

		FStoreParams ClearParams;
		if (!TestRunner->TestTrue(TEXT("Reference-schema GC test case should clear the script-only reference"), InvokeGeneratedFunction(Engine, Holder, StoreFunction, &ClearParams))) { return; }

		FGetStoredParams GetStoredAfterClear;
		if (!TestRunner->TestTrue(TEXT("Reference-schema GC test case should execute GetStored after clearing"), InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredAfterClear))) { return; }
		TestRunner->TestNull(TEXT("Reference-schema GC test case should report null after clearing"), GetStoredAfterClear.ReturnValue);

		CollectGarbage(RF_NoFlags, true);
		TestRunner->TestFalse(TEXT("Reference-schema GC test case should release target after clearing last reference"), WeakTarget.IsValid());
		}
	}

	TEST_METHOD(ReferenceSchemaDoesNotDuplicateAcrossRepeatedSoftReload)
	{
		using namespace AngelscriptTest_ClassGenerator_AngelscriptASClassReferenceSchemaTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ReferenceSchemaSoftReloadModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		auto MakeScript = [](int32 Version) -> FString
		{
			return FString::Printf(TEXT(R"AS(
UCLASS()
class UReferenceSchemaReloadHolder : UObject
{
	UObject HiddenRef = nullptr;
	UFUNCTION() void Store(UObject InValue) { HiddenRef = InValue; }
	UFUNCTION() UObject GetStored() const { return HiddenRef; }
	UFUNCTION() int GetVersion() const { return %d; }
}
)AS"), Version);
		};

		UClass* InitialClass = CompileScriptModule(*TestRunner, Engine, ReferenceSchemaSoftReloadModuleName, ReferenceSchemaSoftReloadFilename, MakeScript(1), ReferenceSchemaSoftReloadClassName);
		if (InitialClass == nullptr) { return; }
		UASClass* InitialASClass = Cast<UASClass>(InitialClass);
		if (!TestRunner->TestNotNull(TEXT("Reference-schema soft-reload should compile as UASClass"), InitialASClass)) { return; }

		const int32 InitialMemberCount = CountSchemaMembers(InitialASClass->ReferenceSchema.Get());
		if (!TestRunner->TestTrue(TEXT("Reference-schema soft-reload should start with non-empty GC schema"), InitialMemberCount > 0)) { return; }

		UFunction* StoreFunction = RequireGeneratedFunction(*TestRunner, InitialClass, TEXT("Store"), TEXT("Reference-schema soft-reload"));
		UFunction* GetStoredFunction = RequireGeneratedFunction(*TestRunner, InitialClass, TEXT("GetStored"), TEXT("Reference-schema soft-reload"));
		UFunction* GetVersionFunction = RequireGeneratedFunction(*TestRunner, InitialClass, TEXT("GetVersion"), TEXT("Reference-schema soft-reload"));
		if (StoreFunction == nullptr || GetStoredFunction == nullptr || GetVersionFunction == nullptr) { return; }

		FGetVersionParams GetVersionBeforeReload;
		if (!TestRunner->TestTrue(TEXT("Should execute GetVersion before reload"), InvokeGeneratedFunction(Engine, InitialASClass->GetDefaultObject(), GetVersionFunction, &GetVersionBeforeReload))) { return; }
		TestRunner->TestEqual(TEXT("Should start at version 1"), GetVersionBeforeReload.ReturnValue, 1);

		// First soft reload
		ECompileResult FirstReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("First soft reload should compile"),
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ReferenceSchemaSoftReloadModuleName, ReferenceSchemaSoftReloadFilename, MakeScript(2), FirstReloadResult)))
		{ return; }
		if (!TestRunner->TestTrue(TEXT("First reload should be handled"), FirstReloadResult == ECompileResult::FullyHandled || FirstReloadResult == ECompileResult::PartiallyHandled))
		{ return; }

		UASClass* FirstReloadClass = Cast<UASClass>(FindGeneratedClass(&Engine, ReferenceSchemaSoftReloadClassName));
		if (!TestRunner->TestNotNull(TEXT("Should still expose holder after first reload"), FirstReloadClass)) { return; }
		TestRunner->TestTrue(TEXT("Should preserve UASClass instance after first reload"), FirstReloadClass == InitialASClass);
		TestRunner->TestEqual(TEXT("Schema member count should be stable after first reload"), CountSchemaMembers(FirstReloadClass->ReferenceSchema.Get()), InitialMemberCount);

		UFunction* GetVersionAfterFirstReload = FindGeneratedFunction(FirstReloadClass, TEXT("GetVersion"));
		FGetVersionParams GetVersionAfterReloadOne;
		if (!TestRunner->TestNotNull(TEXT("Should still expose GetVersion after first reload"), GetVersionAfterFirstReload)) { return; }
		if (!TestRunner->TestTrue(TEXT("Should execute GetVersion after first reload"), InvokeGeneratedFunction(Engine, FirstReloadClass->GetDefaultObject(), GetVersionAfterFirstReload, &GetVersionAfterReloadOne))) { return; }
		TestRunner->TestEqual(TEXT("Should advance to version 2"), GetVersionAfterReloadOne.ReturnValue, 2);

		// Second soft reload
		ECompileResult SecondReloadResult = ECompileResult::Error;
		if (!TestRunner->TestTrue(TEXT("Second soft reload should compile"),
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ReferenceSchemaSoftReloadModuleName, ReferenceSchemaSoftReloadFilename, MakeScript(3), SecondReloadResult)))
		{ return; }
		if (!TestRunner->TestTrue(TEXT("Second reload should be handled"), SecondReloadResult == ECompileResult::FullyHandled || SecondReloadResult == ECompileResult::PartiallyHandled))
		{ return; }

		UASClass* SecondReloadClass = Cast<UASClass>(FindGeneratedClass(&Engine, ReferenceSchemaSoftReloadClassName));
		if (!TestRunner->TestNotNull(TEXT("Should still expose holder after second reload"), SecondReloadClass)) { return; }
		TestRunner->TestTrue(TEXT("Should preserve UASClass instance after second reload"), SecondReloadClass == InitialASClass);
		TestRunner->TestEqual(TEXT("Schema member count should be stable after second reload"), CountSchemaMembers(SecondReloadClass->ReferenceSchema.Get()), InitialMemberCount);

		UFunction* GetVersionAfterSecondReload = FindGeneratedFunction(SecondReloadClass, TEXT("GetVersion"));
		FGetVersionParams GetVersionAfterReloadTwo;
		if (!TestRunner->TestNotNull(TEXT("Should still expose GetVersion after second reload"), GetVersionAfterSecondReload)) { return; }
		if (!TestRunner->TestTrue(TEXT("Should execute GetVersion after second reload"), InvokeGeneratedFunction(Engine, SecondReloadClass->GetDefaultObject(), GetVersionAfterSecondReload, &GetVersionAfterReloadTwo))) { return; }
		TestRunner->TestEqual(TEXT("Should advance to version 3"), GetVersionAfterReloadTwo.ReturnValue, 3);

		// Verify GC still works after repeated reloads
		UObject* Holder = NewObject<UObject>(GetTransientPackage(), SecondReloadClass, TEXT("ReferenceSchemaSoftReloadHolder"));
		if (!TestRunner->TestNotNull(TEXT("Should instantiate reloaded holder"), Holder)) { return; }
		Holder->AddToRoot();
		ON_SCOPE_EXIT
		{
			if (Holder != nullptr) { Holder->RemoveFromRoot(); Holder->MarkAsGarbage(); }
			CollectGarbage(RF_NoFlags, true);
		};

		UAngelscriptNativeScriptTestObject* StrongTarget = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage(), TEXT("ReferenceSchemaSoftReloadTarget"));
		if (!TestRunner->TestNotNull(TEXT("Should create transient target"), StrongTarget)) { return; }
		TWeakObjectPtr<UAngelscriptNativeScriptTestObject> WeakTarget = StrongTarget;

		FStoreParams StoreParams;
		StoreParams.InValue = StrongTarget;
		if (!TestRunner->TestTrue(TEXT("Should store target after repeated reloads"), InvokeGeneratedFunction(Engine, Holder, StoreFunction, &StoreParams))) { return; }

		StrongTarget = nullptr;
		CollectGarbage(RF_NoFlags, true);
		TestRunner->TestTrue(TEXT("Should keep target alive after repeated reloads"), WeakTarget.IsValid());

		FGetStoredParams GetStoredAfterGC;
		if (!TestRunner->TestTrue(TEXT("Should expose stored object after repeated reloads and GC"), InvokeGeneratedFunction(Engine, Holder, GetStoredFunction, &GetStoredAfterGC))) { return; }
		TestRunner->TestTrue(TEXT("Should preserve same stored object identity after repeated reloads"), GetStoredAfterGC.ReturnValue == WeakTarget.Get());

		}
	}
};

#endif
