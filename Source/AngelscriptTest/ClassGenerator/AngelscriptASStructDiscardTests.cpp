#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASStruct.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


namespace ASStructDiscardTest
{
	static const FName ModuleName(TEXT("ASStructDiscardModule"));
	static const FName StructName(TEXT("DiscardableStruct"));
	static const FString ScriptFilename(TEXT("ASStructDiscardModule.as"));

	FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), ScriptFilename);
	}

	UASStruct* FindStruct()
	{
		return FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *StructName.ToString());
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptASStructDiscardTests,
	"Angelscript.TestModule.ClassGenerator.ASStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DiscardModuleClearsScriptTypeAndNativeOps)
	{
		using namespace ASStructDiscardTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ASStructDiscardTest::ModuleName.ToString());
			IFileManager::Get().Delete(*ASStructDiscardTest::GetScriptAbsoluteFilename(), false, true, true);
			ASTEST_RESET_ENGINE(Engine);
		};

		const FString ScriptSource = TEXT(R"AS(
USTRUCT()
struct FDiscardableStruct
{
	UPROPERTY()
	int Value = 7;

	bool opEquals(const FDiscardableStruct& Other) const
	{
		return Value == Other.Value;
	}

	uint32 Hash() const
	{
		return uint32(Value + 11);
	}

	FString ToString() const
	{
		return "Discardable";
	}
};
)AS");

		if (!TestRunner->TestTrue(
				TEXT("ASStruct discard test should compile the struct module"),
				CompileAnnotatedModuleFromMemory(&Engine, ASStructDiscardTest::ModuleName, ASStructDiscardTest::ScriptFilename, ScriptSource)))
		{
			return;
		}

		UASStruct* Struct = ASStructDiscardTest::FindStruct();
		if (!TestRunner->TestNotNull(TEXT("ASStruct discard test should register the generated struct in the Angelscript package"), Struct))
		{
			return;
		}

		Struct->PrepareCppStructOps();

		if (!TestRunner->TestNotNull(TEXT("ASStruct discard test should publish a script type before discard"), Struct->ScriptType)
			|| !TestRunner->TestNotNull(TEXT("ASStruct discard test should create cpp struct ops before discard"), Struct->GetCppStructOps())
			|| !TestRunner->TestNotNull(TEXT("ASStruct discard test should keep the script ToString binding before discard"), Struct->GetToStringFunction()))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("ASStruct discard test should advertise identical-native support before discard"),
			EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative));

		const bool bDiscarded = Engine.DiscardModule(*ASStructDiscardTest::ModuleName.ToString());
		TestRunner->TestTrue(TEXT("ASStruct discard test should discard the owning module successfully"), bDiscarded);
		TestRunner->TestFalse(
			TEXT("ASStruct discard test should remove the module record after discard"),
			Engine.GetModuleByModuleName(ASStructDiscardTest::ModuleName.ToString()).IsValid());
		TestRunner->TestNull(TEXT("ASStruct discard test should clear the struct script type after discard"), Struct->ScriptType);
		TestRunner->TestNotNull(
			TEXT("ASStruct discard test should keep the cached cpp struct ops object alive after discard"),
			Struct->GetCppStructOps());
		TestRunner->TestNull(
			TEXT("ASStruct discard test should clear the cached ToString function after discard"),
			Struct->GetToStringFunction());
		TestRunner->TestFalse(
			TEXT("ASStruct discard test should clear STRUCT_IdenticalNative after discard"),
			EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative));

		}
	}
};

#endif
