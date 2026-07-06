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


TEST_CLASS_WITH_FLAGS(FAngelscriptASGeneratedTypeIdentityTests,
	"Angelscript.TestModule.Generator.ASStruct.GeneratedTypeIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName StructModuleName = FName(TEXT("ASGeneratedStructIdentity"));
	inline static const FName StructName = FName(TEXT("StructIdentityTarget"));
	inline static const FString StructScriptFilename = FString(TEXT("ASGeneratedStructIdentity.as"));

	static FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), StructScriptFilename);
	}

	static UScriptStruct* FindStructObjectByName(const FName InStructName)
	{
		return FindObject<UScriptStruct>(FAngelscriptEngine::GetPackage(), *InStructName.ToString());
	}

	static UASStruct* FindCurrentStruct()
	{
		return Cast<UASStruct>(FindStructObjectByName(StructName));
	}

	static bool VerifyHandledReloadResult(FAutomationTestBase& Test, const TCHAR* Context, const ECompileResult ReloadResult)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled,
			Context);
	}

	static bool VerifyLiveStructIdentity(
		FAutomationTestBase& Test,
		UASStruct* Struct,
		const TCHAR* StageLabel)
	{
		const FString StructMessage = FString::Printf(TEXT("%s should publish a generated UASStruct"), StageLabel);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Struct, *StructMessage))
		{
			return false;
		}

		const bool bIsScriptStructMatches = LocalAssert.IsTrue(
			Struct->bIsScriptStruct,
			*FString::Printf(TEXT("%s should mark the struct as script-generated"), StageLabel));
		const bool bScriptTypeMatches = LocalAssert.IsNotNull(
			Struct->ScriptType,
			*FString::Printf(TEXT("%s should publish a live script type pointer"), StageLabel));
		const bool bNewestVersionMatches = LocalAssert.AreEqual(
			static_cast<UScriptStruct*>(Struct),
			Struct->GetNewestVersion(),
			*FString::Printf(TEXT("%s should resolve GetNewestVersion to itself while canonical"), StageLabel));
		return bIsScriptStructMatches && bScriptTypeMatches && bNewestVersionMatches;
	}

	static bool VerifyReplacedStructIdentity(
		FAutomationTestBase& Test,
		UASStruct* Struct,
		UASStruct* ExpectedNewestVersion,
		const TCHAR* StageLabel)
	{
		const FString StructMessage = FString::Printf(TEXT("%s should keep the replaced struct alive for version-chain lookups"), StageLabel);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Struct, *StructMessage))
		{
			return false;
		}

		const bool bIsScriptStructMatches = LocalAssert.IsTrue(
			Struct->bIsScriptStruct,
			*FString::Printf(TEXT("%s should keep the replaced struct tagged as a script struct"), StageLabel));
		const bool bNewestVersionMatches = LocalAssert.AreEqual(
			static_cast<UScriptStruct*>(ExpectedNewestVersion),
			Struct->GetNewestVersion(),
			*FString::Printf(TEXT("%s should point GetNewestVersion at the replacement struct"), StageLabel));
		const bool bDirectVersionLinkMatches = LocalAssert.AreEqual(
			ExpectedNewestVersion,
			Struct->NewerVersion,
			*FString::Printf(TEXT("%s should wire NewerVersion directly to the replacement struct"), StageLabel));
		const bool bClearedScriptTypeMatches = LocalAssert.IsNull(
			Struct->ScriptType,
			*FString::Printf(TEXT("%s should clear the stale script type pointer after full reload"), StageLabel));
		return bIsScriptStructMatches
			&& bNewestVersionMatches
			&& bDirectVersionLinkMatches
			&& bClearedScriptTypeMatches;
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

	TEST_METHOD(ScriptIdentityFieldsTrackFullReloadLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FAngelscriptASGeneratedTypeIdentityTests::StructModuleName.ToString());
			IFileManager::Get().Delete(*FAngelscriptASGeneratedTypeIdentityTests::GetScriptAbsoluteFilename(), false, true, true);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FStructIdentityTarget
			{
				UPROPERTY()
				int Value = 1;
			};
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FStructIdentityTarget
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			};
			)AS");

		if (!this->Assert.IsTrue(
				CompileAnnotatedModuleFromMemory(&Engine, FAngelscriptASGeneratedTypeIdentityTests::StructModuleName, FAngelscriptASGeneratedTypeIdentityTests::StructScriptFilename, ScriptV1),
				TEXT("Struct identity baseline compile should succeed")))
		{
			return;
		}

		UASStruct* StructV1 = FAngelscriptASGeneratedTypeIdentityTests::FindCurrentStruct();
		if (!FAngelscriptASGeneratedTypeIdentityTests::VerifyLiveStructIdentity(*TestRunner, StructV1, TEXT("Struct identity baseline")))
		{
			return;
		}

		ASSERT_THAT(IsNull(
			StructV1->NewerVersion,
			TEXT("Struct identity baseline should not publish a newer version link before reload")));
		ASSERT_THAT(IsNull(
			StructV1->FindPropertyByName(TEXT("AddedValue")),
			TEXT("Struct identity baseline should not expose the structural-change property before reload")));

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!this->Assert.IsTrue(
				CompileModuleWithResult(
					&Engine,
					ECompileType::FullReload,
					FAngelscriptASGeneratedTypeIdentityTests::StructModuleName,
					FAngelscriptASGeneratedTypeIdentityTests::StructScriptFilename,
					ScriptV2,
					ReloadResult),
				TEXT("Struct identity full reload should compile successfully")))
		{
			return;
		}

		if (!FAngelscriptASGeneratedTypeIdentityTests::VerifyHandledReloadResult(
				*TestRunner,
				TEXT("Struct identity full reload should be handled by the full reload pipeline"),
				ReloadResult))
		{
			return;
		}

		UASStruct* StructV2 = FAngelscriptASGeneratedTypeIdentityTests::FindCurrentStruct();
		if (!FAngelscriptASGeneratedTypeIdentityTests::VerifyLiveStructIdentity(*TestRunner, StructV2, TEXT("Struct identity replacement")))
		{
			return;
		}

		ASSERT_THAT(AreNotEqual(
			static_cast<UScriptStruct*>(StructV1),
			static_cast<UScriptStruct*>(StructV2),
			TEXT("Struct identity full reload should replace the canonical struct object")));
		ASSERT_THAT(IsNotNull(
			StructV2->FindPropertyByName(TEXT("AddedValue")),
			TEXT("Struct identity replacement should expose the newly added reflected property")));
		ASSERT_THAT(IsNull(
			StructV2->NewerVersion,
			TEXT("Struct identity replacement should become the leaf of the version chain")));
		ASSERT_THAT(IsNull(
			StructV1->FindPropertyByName(TEXT("AddedValue")),
			TEXT("Struct identity replaced struct should keep its original reflected layout")));
		FAngelscriptASGeneratedTypeIdentityTests::VerifyReplacedStructIdentity(
			*TestRunner,
			StructV1,
			StructV2,
			TEXT("Struct identity replaced struct"));
	}
};

#endif
