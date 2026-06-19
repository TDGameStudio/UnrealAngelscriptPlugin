#include "AngelscriptBinds.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptBindModuleCacheTests_Private
{
	FString MakeBindModuleCacheAutomationDirectory()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("BindModulesCache"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	TArray<FString> MakeExpectedBindModules()
	{
		return {
			TEXT("ASRuntimeBind_Alpha"),
			TEXT("ASEditorBind_Beta"),
			TEXT("ASRuntimeBind_Gamma"),
		};
	}

	bool ExpectBindModuleSequence(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const TArray<FString>& Actual,
		const TArray<FString>& Expected)
	{
		FNoDiscardAsserter Assert(Test);
		bool bOk = true;
		bOk &= Assert.AreEqual(
			Expected.Num(),
			Actual.Num(),
			*FString::Printf(TEXT("%s should preserve the bind module count"), Context));

		for (int32 Index = 0; Index < FMath::Min(Actual.Num(), Expected.Num()); ++Index)
		{
			bOk &= Assert.AreEqual(
				Expected[Index],
				Actual[Index],
				*FString::Printf(TEXT("%s should preserve the bind module at index %d"), Context, Index));
		}

		return bOk;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBindModuleCacheTests,
	"Angelscript.TestModule.Engine.BindConfig.BindModuleCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RoundTripsOrderAndClearsOnMissingFile)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindModuleCacheTests_Private;
		const TArray<FString> ExpectedBindModules = MakeExpectedBindModules();
		const FString CacheDirectory = MakeBindModuleCacheAutomationDirectory();
		const FString CachePath = FPaths::Combine(CacheDirectory, TEXT("BindModules.Cache"));
		const FString MissingCachePath = FPaths::Combine(CacheDirectory, TEXT("MissingBindModules.Cache"));

		IFileManager::Get().MakeDirectory(*CacheDirectory, true);
		FAngelscriptBinds::ResetBindState();
		ON_SCOPE_EXIT
		{
			FAngelscriptBinds::ResetBindState();
			IFileManager::Get().DeleteDirectory(*CacheDirectory, false, true);
		};

		TArray<FString>& BindModuleNames = FAngelscriptBinds::GetBindModuleNames();
		BindModuleNames = ExpectedBindModules;
		FAngelscriptBinds::SaveBindModules(CachePath);

		if (!this->Assert.IsTrue(
				IFileManager::Get().FileExists(*CachePath),
				TEXT("BindModuleCache should write BindModules.Cache to the automation directory")))
		{
			return;
		}

		TArray<FString> SavedLines;
		if (!this->Assert.IsTrue(
				FFileHelper::LoadFileToStringArray(SavedLines, *CachePath),
				TEXT("BindModuleCache should persist BindModules.Cache as readable string lines")))
		{
			return;
		}

		if (!ExpectBindModuleSequence(
				*TestRunner,
				TEXT("BindModuleCache save path"),
				SavedLines,
				ExpectedBindModules))
		{
			return;
		}

		FAngelscriptBinds::ResetBindState();
		if (!this->Assert.AreEqual(
				0,
				FAngelscriptBinds::GetBindModuleNames().Num(),
				TEXT("BindModuleCache reset should clear the in-memory bind module list before reload")))
		{
			return;
		}

		FAngelscriptBinds::LoadBindModules(CachePath);
		if (!ExpectBindModuleSequence(
				*TestRunner,
				TEXT("BindModuleCache round-trip"),
				FAngelscriptBinds::GetBindModuleNames(),
				ExpectedBindModules))
		{
			return;
		}

		BindModuleNames = {
			TEXT("ASRuntimeBind_Stale"),
			TEXT("ASEditorBind_Stale"),
		};

		if (!this->Assert.IsFalse(
				IFileManager::Get().FileExists(*MissingCachePath),
				TEXT("BindModuleCache missing-file coverage should use a path that does not exist")))
		{
			return;
		}

		FAngelscriptBinds::LoadBindModules(MissingCachePath);
		ASSERT_THAT(AreEqual(
			0,
			FAngelscriptBinds::GetBindModuleNames().Num(),
			TEXT("BindModuleCache missing-file load should clear stale in-memory bind module names")));
	}
};

#endif
