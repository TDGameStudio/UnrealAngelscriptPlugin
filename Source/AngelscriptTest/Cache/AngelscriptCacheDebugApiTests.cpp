#include "Cache/AngelscriptCacheDiagnostics.h"
#include "Cache/AngelscriptCacheDiagnosticsLibrary.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/StringOutputDevice.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheDebugApiTests,
	"Angelscript.TestModule.Cache.DebugApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitEngineAndBlueprintFacadeShareOneStatusJson)
	{
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptEngineScope Scope(Fixture.GetEngine());

		const FAngelscriptCacheDiagnosticJsonResult CppResult =
			CaptureAngelscriptCacheDiagnosticJson(&Fixture.GetEngine());
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::None, CppResult.Error));
		ASSERT_THAT(IsTrue(CppResult.Detail.IsEmpty()));
		ASSERT_THAT(IsTrue(CppResult.Json.Contains(
			TEXT("\"schemaVersion\":1"))));

		FString BlueprintJson;
		FString BlueprintError;
		ASSERT_THAT(IsTrue(
			UAngelscriptCacheDiagnosticsLibrary::GetCacheStatusJson(
				BlueprintJson, BlueprintError)));
		ASSERT_THAT(IsTrue(BlueprintError.IsEmpty()));
		ASSERT_THAT(AreEqual(CppResult.Json, BlueprintJson));
		ASSERT_THAT(IsFalse(BlueprintJson.Contains(TEXT("functionId"))));
		ASSERT_THAT(IsFalse(BlueprintJson.Contains(TEXT("serviceIdentity"))));
	}

	TEST_METHOD(MissingEngineIsTypedAndStatusConsoleCommandIsRegistered)
	{
		const FAngelscriptCacheDiagnosticJsonResult Result =
			CaptureAngelscriptCacheDiagnosticJson(nullptr);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable,
			Result.Error));
		ASSERT_THAT(IsTrue(Result.Json.IsEmpty()));
		ASSERT_THAT(IsFalse(Result.Detail.IsEmpty()));

		IConsoleObject* StatusCommand =
			IConsoleManager::Get().FindConsoleObject(TEXT("as.Cache.Status"));
		ASSERT_THAT(IsNotNull(StatusCommand));
		ASSERT_THAT(IsNotNull(StatusCommand->AsCommand()));
	}

	TEST_METHOD(StatusConsoleCommandWritesPythonConsumableSessionJson)
	{
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptEngineScope Scope(Fixture.GetEngine());
		const FAngelscriptCacheDiagnosticJsonResult Expected =
			CaptureAngelscriptCacheDiagnosticJson(&Fixture.GetEngine());
		ASSERT_THAT(IsTrue(Expected.IsSuccess()));

		const FString OutputPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("CacheV2"),
			FString::Printf(TEXT("Status-%s.json"),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().Delete(*OutputPath, false, true, true);
		};

		FStringOutputDevice ConsoleOutput;
		const FString Command = FString::Printf(
			TEXT("as.Cache.Status Json=%s"), *OutputPath);
		ASSERT_THAT(IsTrue(
			IConsoleManager::Get().ProcessUserConsoleInput(
				*Command, ConsoleOutput, nullptr)));

		FString PersistedJson;
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(PersistedJson, *OutputPath)));
		ASSERT_THAT(AreEqual(Expected.Json, PersistedJson));
		ASSERT_THAT(IsTrue(ConsoleOutput.Contains(TEXT("wrote"))));
	}
};

#endif
