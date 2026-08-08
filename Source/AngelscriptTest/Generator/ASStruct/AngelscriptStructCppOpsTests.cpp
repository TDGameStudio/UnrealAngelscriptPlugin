// Generated AS-struct C++ operation coverage.
#include "AngelscriptTestMacros.h"
#include "../../AngelscriptRuntime/Core/AngelscriptBinds.h"
#include "AngelscriptStructCppOpsTestTypes.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FStructCppOpsTests,
	"Angelscript.TestModule.Generator.ASStruct.CppOps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static UScriptStruct* BuildScriptStruct(
		FAutomationTestBase& Test,
		FNoDiscardAsserter& Assert,
		FAngelscriptEngine& Engine,
		const char* ModuleName,
		const FString& Source,
		const char* TypeName)
	{
		asIScriptModule* Module = BuildModule(Test, Engine, ModuleName, Source);
		if (Module == nullptr)
		{
			return nullptr;
		}

		FString UnrealName = UTF8_TO_TCHAR(TypeName);
		if (UnrealName.Len() >= 2 && UnrealName[0] == 'F' && FChar::IsUpper(UnrealName[1]))
		{
			UnrealName.RightChopInline(1, EAllowShrinking::No);
		}

		UScriptStruct* Struct = FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *UnrealName);
		if (!Assert.IsNotNull(Struct, TEXT("Compiled script struct should have a backing UScriptStruct")))
		{
			return nullptr;
		}
		return Struct;
	}

public:
	TEST_METHOD(NotBlueprintTypeByDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		UScriptStruct* Struct = BuildScriptStruct(
			*TestRunner,
			this->Assert,
			Engine,
			"StructCppOpsScopeModule",
			TEXT(R"ANGELSCRIPT(
struct FScopeConstructStruct
{
	int Value = 7;
}
)ANGELSCRIPT"),
			"FScopeConstructStruct");
		if (Struct == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsFalse(Struct->GetBoolMetaData(TEXT("BlueprintType")),
			TEXT("Script structs should not be BlueprintType by default")));
		}
	}

	TEST_METHOD(ValueClassUsesCppStructOps)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptStructCppOpsLifecycleFixture::ResetCounters();
		UScriptStruct* Struct = FAngelscriptStructCppOpsLifecycleFixture::StaticStruct();
		ASSERT_THAT(IsNotNull(Struct,
			TEXT("StructCppOps fixture should expose a native UScriptStruct")));

		UScriptStruct::ICppStructOps* Ops = Struct->GetCppStructOps();
		ASSERT_THAT(IsNotNull(Ops,
			TEXT("StructCppOps fixture should expose cpp struct ops")));

		ASSERT_THAT(AreEqual(static_cast<int32>(alignof(FAngelscriptStructCppOpsLifecycleFixture)), Struct->GetMinAlignment(),
			TEXT("StructCppOps fixture should keep the expected native alignment")));
		ASSERT_THAT(AreEqual(static_cast<int32>(sizeof(FAngelscriptStructCppOpsLifecycleFixture)), Ops->GetSize(),
			TEXT("StructCppOps fixture should report cpp ops size")));
		ASSERT_THAT(IsTrue(Ops->HasCopy(),
			TEXT("StructCppOps fixture should expose copy support through cpp ops")));
		ASSERT_THAT(IsTrue(Ops->HasDestructor(),
			TEXT("StructCppOps fixture should expose destructor support through cpp ops")));

		FAngelscriptBinds Binds(Engine);
		FAngelscriptBinds BoundType =
			Binds.ValueClassForTarget("FStructCppOpsLifecycleFixtureNative", Struct, FBindFlags());
		asITypeInfo* TypeInfo = BoundType.GetTypeInfo();
		ASSERT_THAT(IsNotNull(TypeInfo,
			TEXT("ValueClass should register a script type for the native struct")));

		ASSERT_THAT(AreEqual(Ops->GetSize(), static_cast<int32>(TypeInfo->GetSize()),
			TEXT("ValueClass should use cpp ops size for the bound type")));
		ASSERT_THAT(AreEqual(Struct->GetMinAlignment(), TypeInfo->alignment,
			TEXT("ValueClass should preserve the struct alignment")));

		void* SourceMemory = FMemory::Malloc(Ops->GetSize(), Struct->GetMinAlignment());
		void* DestinationMemory = FMemory::Malloc(Ops->GetSize(), Struct->GetMinAlignment());
		bool bSourceInitialized = false;
		bool bDestinationInitialized = false;
		ON_SCOPE_EXIT
		{
			if (bDestinationInitialized)
			{
				Struct->DestroyStruct(DestinationMemory, 1);
			}

			if (bSourceInitialized)
			{
				Struct->DestroyStruct(SourceMemory, 1);
			}

			FMemory::Free(DestinationMemory);
			FMemory::Free(SourceMemory);
		};

		ASSERT_THAT(IsNotNull(SourceMemory,
			TEXT("StructCppOps fixture should allocate source memory")));
		ASSERT_THAT(IsNotNull(DestinationMemory,
			TEXT("StructCppOps fixture should allocate destination memory")));

		Struct->InitializeStruct(SourceMemory, 1);
		bSourceInitialized = true;
		auto* SourceValue = static_cast<FAngelscriptStructCppOpsLifecycleFixture*>(SourceMemory);
		ASSERT_THAT(AreEqual(1, FAngelscriptStructCppOpsLifecycleFixture::DefaultConstructorCount,
			TEXT("InitializeStruct should run the default constructor once")));
		ASSERT_THAT(AreEqual(FAngelscriptStructCppOpsLifecycleFixture::DefaultSentinelValue, SourceValue->SentinelValue,
			TEXT("InitializeStruct should write the sentinel value")));
		ASSERT_THAT(AreEqual(FAngelscriptStructCppOpsLifecycleFixture::DefaultPayloadValue, SourceValue->PayloadValue,
			TEXT("InitializeStruct should write the payload value")));

		SourceValue->PayloadValue = 9001;

		Struct->InitializeStruct(DestinationMemory, 1);
		bDestinationInitialized = true;
		ASSERT_THAT(AreEqual(2, FAngelscriptStructCppOpsLifecycleFixture::DefaultConstructorCount,
			TEXT("Destination InitializeStruct should run the default constructor again")));

		Struct->CopyScriptStruct(DestinationMemory, SourceMemory, 1);
		auto* DestinationValue = static_cast<FAngelscriptStructCppOpsLifecycleFixture*>(DestinationMemory);
		ASSERT_THAT(AreEqual(1, FAngelscriptStructCppOpsLifecycleFixture::CopyCount,
			TEXT("CopyScriptStruct should route through the native copy op")));
		ASSERT_THAT(AreEqual(SourceValue->SentinelValue, DestinationValue->SentinelValue,
			TEXT("CopyScriptStruct should copy the sentinel value")));
		ASSERT_THAT(AreEqual(SourceValue->PayloadValue, DestinationValue->PayloadValue,
			TEXT("CopyScriptStruct should copy the payload value")));

		Struct->DestroyStruct(DestinationMemory, 1);
		bDestinationInitialized = false;
		Struct->DestroyStruct(SourceMemory, 1);
		bSourceInitialized = false;
		ASSERT_THAT(AreEqual(2, FAngelscriptStructCppOpsLifecycleFixture::DestructorCount,
			TEXT("DestroyStruct should run both native destructors")));

		}
	}
};

#endif
