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
#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptASStructDiscardTests,
	"Angelscript.TestModule.Generator.ASStruct.Discard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName ModuleName = FName(TEXT("ASStructDiscardModule"));
	inline static const FName StructName = FName(TEXT("DiscardableStruct"));
	inline static const FString ScriptFilename = FString(TEXT("ASStructDiscardModule.as"));

	static FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), ScriptFilename);
	}

	static UASStruct* FindStruct()
	{
		return FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *StructName.ToString());
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

	TEST_METHOD(DiscardModuleClearsScriptTypeAndNativeOps)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FAngelscriptASStructDiscardTests::ModuleName.ToString());
			IFileManager::Get().Delete(*FAngelscriptASStructDiscardTests::GetScriptAbsoluteFilename(), false, true, true);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, FAngelscriptASStructDiscardTests::ModuleName, FAngelscriptASStructDiscardTests::ScriptFilename, ScriptSource),
			TEXT("ASStruct discard test should compile the struct module")));

		UASStruct* Struct = FAngelscriptASStructDiscardTests::FindStruct();
		ASSERT_THAT(IsNotNull(Struct, TEXT("ASStruct discard test should register the generated struct in the Angelscript package")));

		Struct->PrepareCppStructOps();

		ASSERT_THAT(IsNotNull(Struct->ScriptType, TEXT("ASStruct discard test should publish a script type before discard")));
		ASSERT_THAT(IsNotNull(Struct->GetCppStructOps(), TEXT("ASStruct discard test should create cpp struct ops before discard")));
		ASSERT_THAT(IsNotNull(Struct->GetToStringFunction(), TEXT("ASStruct discard test should keep the script ToString binding before discard")));

		ASSERT_THAT(IsTrue(
			EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative),
			TEXT("ASStruct discard test should advertise identical-native support before discard")));

		const bool bDiscarded = Engine.DiscardModule(*FAngelscriptASStructDiscardTests::ModuleName.ToString());
		ASSERT_THAT(IsTrue(bDiscarded, TEXT("ASStruct discard test should discard the owning module successfully")));
		ASSERT_THAT(IsFalse(
			Engine.GetModuleByModuleName(FAngelscriptASStructDiscardTests::ModuleName.ToString()).IsValid(),
			TEXT("ASStruct discard test should remove the module record after discard")));
		ASSERT_THAT(IsNull(Struct->ScriptType, TEXT("ASStruct discard test should clear the struct script type after discard")));
		ASSERT_THAT(IsNotNull(
			Struct->GetCppStructOps(),
			TEXT("ASStruct discard test should keep the cached cpp struct ops object alive after discard")));
		ASSERT_THAT(IsNull(
			Struct->GetToStringFunction(),
			TEXT("ASStruct discard test should clear the cached ToString function after discard")));
		ASSERT_THAT(IsFalse(
			EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative),
			TEXT("ASStruct discard test should clear STRUCT_IdenticalNative after discard")));
	}
};

#endif
