// =============================================================================
// AngelscriptEditorCodeGenTests.cpp
//
// Tests for AngelscriptEditorCodeGen.cpp — include paths, build files, and
// generated binding module output.
//
// Automation IDs:
//   Angelscript.Editor.CodeGen.*
// =============================================================================

#include "CQTest.h"
#include "Core/AngelscriptEditorModule.h"
#include "Core/AngelscriptBinds.h"

#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)

// ---------------------------------------------------------------------------
// GetInclude.PublicPath
//   Header path containing "Public/" should strip everything up to and
//   including "Public/" and produce a proper #include directive.
// ---------------------------------------------------------------------------

static bool RunGetIncludePublicPath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("C:/UnrealEngine/Engine/Source/Runtime/Engine/Public/GameFramework/Actor.h");
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should start with #include"),
		Result.StartsWith(TEXT("#include")));
	TestTrue(TEXT("Result should contain the relative path after Public/"),
		Result.Contains(TEXT("GameFramework/Actor.h")));
	TestFalse(TEXT("Result should not contain the full absolute path prefix"),
		Result.Contains(TEXT("C:/UnrealEngine")));

	return true;
}

// ---------------------------------------------------------------------------
// GetInclude.PrivatePath
//   Header path containing "Private/" should strip the Private/ prefix.
// ---------------------------------------------------------------------------

static bool RunGetIncludePrivatePath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("C:/Project/Plugins/MyPlugin/Source/MyPlugin/Private/Internal/Helper.h");
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should contain #include"), Result.Contains(TEXT("#include")));
	TestTrue(TEXT("Result should contain relative path after Private/"),
		Result.Contains(TEXT("Internal/Helper.h")));

	return true;
}

// ---------------------------------------------------------------------------
// GetInclude.ClassesPath
//   Header path containing "Classes/" should strip the Classes/ prefix.
// ---------------------------------------------------------------------------

static bool RunGetIncludeClassesPath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("C:/Engine/Source/Runtime/CoreUObject/Classes/Object.h");
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should contain #include"), Result.Contains(TEXT("#include")));
	TestTrue(TEXT("Result should contain relative path after Classes/"),
		Result.Contains(TEXT("Object.h")));

	return true;
}

// ---------------------------------------------------------------------------
// GetInclude.EmptyPath
//   Empty header path should not crash and should produce a minimal include.
// ---------------------------------------------------------------------------

static bool RunGetIncludeEmptyPath(FAutomationTestBase& Test)
{
	FString HeaderPath = TEXT("");
	// GetIncludeForModule with null UField and empty path — should not crash.
	const FString Result = FAngelscriptEditorModule::GetIncludeForModule(nullptr, HeaderPath);

	TestTrue(TEXT("Result should still contain #include directive"),
		Result.Contains(TEXT("#include")));

	return true;
}

// ---------------------------------------------------------------------------
// GenerateBuildFile.OutputStructure
//   Generated build file lines should contain expected C# structure.
// ---------------------------------------------------------------------------

static bool RunGenerateBuildFileOutputStructure(FAutomationTestBase& Test)
{
	TArray<FString> PublicDeps = { TEXT("Core"), TEXT("CoreUObject"), TEXT("Engine") };
	TArray<FString> PrivateDeps = { TEXT("AngelscriptRuntime") };
	TArray<FString> OutBuildFile;

	FAngelscriptEditorModule::GenerateBuildFile(
		TEXT("TestModule"), PublicDeps, PrivateDeps, OutBuildFile, false);

	// Join all lines for easier search
	FString Joined;
	for (const FString& Line : OutBuildFile)
	{
		Joined += Line + TEXT("\n");
	}

	TestTrue(TEXT("Build file should contain 'using UnrealBuildTool'"),
		Joined.Contains(TEXT("using UnrealBuildTool")));
	TestTrue(TEXT("Build file should contain the module class name"),
		Joined.Contains(TEXT("TestModule")));
	TestTrue(TEXT("Build file should contain 'ModuleRules'"),
		Joined.Contains(TEXT("ModuleRules")));
	TestTrue(TEXT("Build file should contain 'Core' in public dependencies"),
		Joined.Contains(TEXT("\"Core\"")));
	TestTrue(TEXT("Build file should contain 'Engine' in public dependencies"),
		Joined.Contains(TEXT("\"Engine\"")));
	TestTrue(TEXT("Build file should contain 'AngelscriptRuntime' in private dependencies"),
		Joined.Contains(TEXT("AngelscriptRuntime")));
	TestTrue(TEXT("Build file should contain PCHUsage"),
		Joined.Contains(TEXT("PCHUsage")));

	return true;
}

#undef TestTrue
#undef TestFalse
#undef TestEqual

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorCodeGenGetIncludeTests,
	"Angelscript.Editor.CodeGen.GetInclude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PublicPath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludePublicPath(*TestRunner)));
	}

	TEST_METHOD(PrivatePath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludePrivatePath(*TestRunner)));
	}

	TEST_METHOD(ClassesPath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludeClassesPath(*TestRunner)));
	}

	TEST_METHOD(EmptyPath)
	{
		ASSERT_THAT(IsTrue(RunGetIncludeEmptyPath(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorCodeGenGenerateBuildFileTests,
	"Angelscript.Editor.CodeGen.GenerateBuildFile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(OutputStructure)
	{
		ASSERT_THAT(IsTrue(RunGenerateBuildFileOutputStructure(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptEditorCodeGenGeneratedBindingsTests,
	"Angelscript.Editor.CodeGen.GeneratedBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 CountOccurrences(const FString& Source, const FString& Needle)
	{
		int32 Count = 0;
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 FoundAt = Source.Find(
				Needle,
				ESearchCase::CaseSensitive,
				ESearchDir::FromStart,
				SearchFrom);
			if (FoundAt == INDEX_NONE)
			{
				return Count;
			}

			++Count;
			SearchFrom = FoundAt + Needle.Len();
		}
	}

	static FString JoinLines(const TArray<FString>& Lines)
	{
		FString Joined;
		for (const FString& Line : Lines)
		{
			Joined += Line;
			Joined += TEXT("\n");
		}
		return Joined;
	}

	static FString GetPluginDirectory()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Angelscript")));
	}

public:
	TEST_METHOD(GeneratedModuleUsesOneExplicitGeneratedCallback)
	{
		const FString FixtureId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString FixtureModuleKey = FString::Printf(TEXT("ASCodeGenFixture_%s"), *FixtureId);
		const FString OutputDirectory = FPaths::Combine(
			FPaths::ProjectIntermediateDir(),
			TEXT("AngelscriptEditorCodeGenTests"),
			FixtureId);
		ASSERT_THAT(IsTrue(
			IFileManager::Get().MakeDirectory(*OutputDirectory, true),
			TEXT("CodeGen fixture output directory should be created")));
		ON_SCOPE_EXIT
		{
			FAngelscriptBinds::GetRuntimeClassDB().Remove(FixtureModuleKey);
			IFileManager::Get().DeleteDirectory(*OutputDirectory, false, true);
		};

		TArray<TObjectPtr<UClass>> FixtureClasses;
		FixtureClasses.Add(AActor::StaticClass());
		FAngelscriptBinds::GetRuntimeClassDB().Add(FixtureModuleKey, MoveTemp(FixtureClasses));

		TArray<FString> SourceModules{FixtureModuleKey};
		TArray<FString> GeneratedHeaderLines;
		FString CPPDirectory = OutputDirectory;
		CPPDirectory.AppendChar(TEXT('/'));
		FAngelscriptEditorModule::GenerateSourceFilesV2(
			TEXT("ASCodeGenFixture"),
			SourceModules,
			false,
			GeneratedHeaderLines,
			CPPDirectory);

		const FString GeneratedHeader = JoinLines(GeneratedHeaderLines);
		FString GeneratedModuleCPP;
		FString GeneratedClassCPP;
		const FString GeneratedModulePath = FPaths::Combine(
			OutputDirectory,
			TEXT("ASCodeGenFixtureModule.cpp"));
		const FString GeneratedClassPath = FPaths::Combine(OutputDirectory, TEXT("Bind_Actor.cpp"));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(GeneratedModuleCPP, *GeneratedModulePath),
			*FString::Printf(TEXT("Generated module source should be readable: %s"), *GeneratedModulePath)));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(GeneratedClassCPP, *GeneratedClassPath),
			*FString::Printf(TEXT("Generated class binding source should be readable: %s"), *GeneratedClassPath)));

		ASSERT_THAT(IsTrue(
			GeneratedHeader.Contains(TEXT("class FAngelscriptBinds;"))
				&& GeneratedHeader.Contains(TEXT("static void Bind_Actor(FAngelscriptBinds& Binds);")),
			TEXT("Generated header should declare explicit bind-context class callbacks")));
		ASSERT_THAT(IsTrue(
			GeneratedClassCPP.Contains(TEXT(
				"void FASCodeGenFixtureModule::Bind_Actor(FAngelscriptBinds& Binds)")),
			TEXT("Generated class source should define the explicit bind-context callback")));
		ASSERT_THAT(IsTrue(
			GeneratedClassCPP.Contains(TEXT(
				"Binds.RegisterGeneratedFunctionBindingForTarget(AActor::StaticClass(), \"TearOff\", "
				"{ ERASE_METHOD_PTR(AActor, TearOff")),
			TEXT("Generated entries should preserve native caller output through the explicit generated facade")));
		ASSERT_THAT(IsFalse(
			GeneratedClassCPP.Contains(TEXT("FAngelscriptBinds::RegisterFunctionBinding(")),
			TEXT("Generated class source should not use ambient registration")));

		ASSERT_THAT(AreEqual(
			1,
			CountOccurrences(
				GeneratedModuleCPP,
				TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_AS_EditorCodeGen_ASCodeGenFixture")),
			TEXT("Generated module should own exactly one file-static direct bind record")));
		ASSERT_THAT(IsTrue(
			GeneratedModuleCPP.Contains(TEXT(
				"static void BindGeneratedFunctionBindings_ASCodeGenFixture(FAngelscriptBinds& Binds)"))
				&& GeneratedModuleCPP.Contains(TEXT("FASCodeGenFixtureModule::Bind_Actor(Binds);"))
				&& GeneratedModuleCPP.Contains(TEXT("TEXT(\"EditorCodeGen.FunctionBinding.ASCodeGenFixture\")"))
				&& GeneratedModuleCPP.Contains(TEXT("EAngelscriptBindPhase::GeneratedBindings"))
				&& GeneratedModuleCPP.Contains(TEXT("&BindGeneratedFunctionBindings_ASCodeGenFixture")),
			TEXT("Generated module should expose one named GeneratedBindings callback")));
		ASSERT_THAT(IsTrue(
			GeneratedModuleCPP.Contains(TEXT("void FASCodeGenFixtureModule::StartupModule()\n{\n}")),
			TEXT("Generated module StartupModule should remain empty")));
		ASSERT_THAT(IsFalse(
			GeneratedModuleCPP.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| GeneratedModuleCPP.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| GeneratedModuleCPP.Contains(TEXT("[]()")),
			TEXT("Generated module should contain no dynamic legacy binding submission")));
	}

	TEST_METHOD(LegacyCodeGeneratorsAreRemoved)
	{
		FString CodeGenSource;
		FString EditorModuleHeader;
		const FString CodeGenSourcePath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptEditor/CodeGen/AngelscriptEditorCodeGen.cpp"));
		const FString EditorModuleHeaderPath = FPaths::Combine(
			GetPluginDirectory(),
			TEXT("Source/AngelscriptEditor/Core/AngelscriptEditorModule.h"));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(CodeGenSource, *CodeGenSourcePath),
			*FString::Printf(TEXT("Editor CodeGen source should be readable: %s"), *CodeGenSourcePath)));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(EditorModuleHeader, *EditorModuleHeaderPath),
			*FString::Printf(TEXT("Editor module header should be readable: %s"), *EditorModuleHeaderPath)));

		ASSERT_THAT(IsFalse(
			CodeGenSource.Contains(TEXT("FAngelscriptEditorModule::GenerateSourceFiles("))
				|| CodeGenSource.Contains(TEXT("FAngelscriptEditorModule::GenerateFunctionEntriesOld"))
				|| CodeGenSource.Contains(TEXT("void GenerateSourceFilesOG("))
				|| EditorModuleHeader.Contains(TEXT("static void GenerateSourceFiles("))
				|| EditorModuleHeader.Contains(TEXT("GenerateFunctionEntriesOld")),
			TEXT("Unused legacy editor generators should not remain as code or commented source")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
