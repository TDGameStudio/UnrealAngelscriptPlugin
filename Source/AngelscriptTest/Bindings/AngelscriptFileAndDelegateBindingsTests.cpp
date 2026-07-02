// ============================================================================
// AngelscriptFileAndDelegateBindingsTests.cpp
//
// File helper and delegate binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.FileAndDelegate.FAngelscriptFileAndDelegateBindingsTest.*
//
// Sections:
//   ScriptDelegateBinding       — delegate bind/unbind/clear operations
//   ScriptDelegateExecution     — delegate execute and broadcast
//   SoftPathValueOperations     — FSoftObjectPath/FSoftClassPath value operations
//   SoftPathResolveAndLoad      — soft path resolve/load with token substitution
//   SourceMetadataAccessors     — UClass/UFunction source metadata accessors
//   FileHelperSaveAndLoad       — FFileHelper save/load string
//   DelegateWithPayloadPaths    — FAngelscriptDelegateWithPayload happy + error paths
//
// CQTest adaptation notes:
//   - Entry() functions split into individual 1/0-returning functions where feasible.
//   - SoftPathResolveCompat uses $TOKEN$ → ReplaceInline for runtime paths.
//   - SourceMetadataCompat uses $TOKEN$ → ReplaceInline for script file path.
//   - DelegateWithPayloadCompat retains exception-testing helper with AddExpectedError.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptNativeScriptTestObject.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/SoftObjectPath.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptFileAndDelegateBindingsTest,
	"Angelscript.TestModule.Bindings.FileAndDelegate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: ScriptDelegateCompat
	// ====================================================================

	TEST_METHOD(ScriptDelegateBinding)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFileDelegate_DelegateBind"), ASTEST_AS(R"AS(
			delegate int FNativeCallback(int Value, const FString& Label);
			event void FNativeEvent(int Value, const FString& Label);

			int DelegateBind_EmptyNotBound()
			{
				FNativeCallback Single;
				return Single.IsBound() ? 0 : 1;
			}

			int DelegateBind_BindUFunction()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FNativeCallback Single;
				Single.BindUFunction(TestObject, n"NativeIntStringEvent");
				return Single.IsBound() ? 1 : 0;
			}

			int DelegateBind_GetFunctionName()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FNativeCallback Single;
				Single.BindUFunction(TestObject, n"NativeIntStringEvent");
				return Single.GetFunctionName().IsEqual(n"NativeIntStringEvent") ? 1 : 0;
			}

			int DelegateBind_MulticastAddUnbind()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FNativeEvent Multi;
				if (Multi.IsBound())
				{
					return 0;
				}
				Multi.AddUFunction(TestObject, n"SetIntStringFromDelegate");
				if (!Multi.IsBound())
				{
					return 0;
				}
				Multi.Unbind(TestObject, n"SetIntStringFromDelegate");
				return Multi.IsBound() ? 0 : 1;
			}

			int DelegateBind_ClearMakesUnbound()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FNativeCallback Single;
				Single.BindUFunction(TestObject, n"NativeIntStringEvent");
				Single.Clear();
				return Single.IsBound() ? 0 : 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateBind_EmptyNotBound()"), TEXT("Empty delegate should not be bound"), 1), TEXT("Empty delegate should not be bound")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateBind_BindUFunction()"), TEXT("BindUFunction should make delegate bound"), 1), TEXT("BindUFunction should make delegate bound")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateBind_GetFunctionName()"), TEXT("GetFunctionName should return bound function name"), 1), TEXT("GetFunctionName should return bound function name")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateBind_MulticastAddUnbind()"), TEXT("Multicast Add then Unbind should leave unbound"), 1), TEXT("Multicast Add then Unbind should leave unbound")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateBind_ClearMakesUnbound()"), TEXT("Clear should make delegate unbound"), 1), TEXT("Clear should make delegate unbound")));
	}

	// ====================================================================
	// Section: ScriptDelegateExecuteCompat
	// ====================================================================

	TEST_METHOD(ScriptDelegateExecution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UAngelscriptNativeScriptTestObject* NativeTestObject = GetMutableDefault<UAngelscriptNativeScriptTestObject>();
		ASSERT_THAT(IsNotNull(NativeTestObject, TEXT("Script delegate execute compat test should resolve the native test object")));
		NativeTestObject->NameCounts.Reset();
		ON_SCOPE_EXIT { NativeTestObject->NameCounts.Reset(); };

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFileDelegate_DelegateExec"), ASTEST_AS(R"AS(
			delegate int FNativeCallback(int Value, const FString& Label);
			event void FNativeEvent(int Value, const FString& Label);

			int DelegateExec_SingleExecute()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FNativeCallback Single;
				Single.BindUFunction(TestObject, n"NativeIntStringEvent");
				return (Single.Execute(7, "Alpha") == 12) ? 1 : 0;
			}

			int DelegateExec_MulticastBroadcast()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FNativeEvent Multi;
				Multi.AddUFunction(TestObject, n"SetIntStringFromDelegate");
				Multi.Broadcast(7, "Alpha");
				Multi.Unbind(TestObject, n"SetIntStringFromDelegate");
				Multi.Broadcast(11, "Beta");
				return Multi.IsBound() ? 0 : 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateExec_SingleExecute()"), TEXT("Single delegate Execute should return expected value"), 1), TEXT("Single delegate Execute should return expected value")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegateExec_MulticastBroadcast()"), TEXT("Multicast broadcast then unbind should leave unbound"), 1), TEXT("Multicast broadcast then unbind should leave unbound")));

		const int32* AlphaCount = NativeTestObject->NameCounts.Find(TEXT("Alpha"));
		ASSERT_THAT(IsNotNull(AlphaCount, TEXT("Multicast delegate broadcast should write the expected label key")));
		ASSERT_THAT(AreEqual(7, *AlphaCount, TEXT("Multicast delegate broadcast should forward the expected value")));
		ASSERT_THAT(IsFalse(NativeTestObject->NameCounts.Contains(TEXT("Beta")), TEXT("Unbound multicast delegate should not write additional label entries")));
	}

	// ====================================================================
	// Section: SoftPathCompat
	// ====================================================================

	TEST_METHOD(SoftPathValueOperations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFileDelegate_SoftPath"), ASTEST_AS(R"AS(
			int SoftPath_EmptyIsNull()
			{
				FSoftObjectPath EmptyPath;
				return EmptyPath.IsNull() ? 1 : 0;
			}

			int SoftPath_ObjectPathValid()
			{
				FSoftObjectPath ObjectPath(AActor::StaticClass().GetPathName());
				return ObjectPath.IsValid() ? 1 : 0;
			}

			int SoftPath_ObjectPathAssetName()
			{
				FSoftObjectPath ObjectPath(AActor::StaticClass().GetPathName());
				return ObjectPath.GetAssetName().IsEmpty() ? 0 : 1;
			}

			int SoftPath_ObjectPathPackageName()
			{
				FSoftObjectPath ObjectPath(AActor::StaticClass().GetPathName());
				return ObjectPath.GetLongPackageName().IsEmpty() ? 0 : 1;
			}

			int SoftPath_ObjectPathNotSubobject()
			{
				FSoftObjectPath ObjectPath(AActor::StaticClass().GetPathName());
				return ObjectPath.IsSubobject() ? 0 : 1;
			}

			int SoftPath_ObjectPathEquality()
			{
				FSoftObjectPath ObjectPath(AActor::StaticClass().GetPathName());
				return (ObjectPath == FSoftObjectPath(AActor::StaticClass().GetPathName())) ? 1 : 0;
			}

			int SoftPath_ClassPathValid()
			{
				FSoftClassPath ClassPath(AActor::StaticClass().GetPathName());
				return ClassPath.IsValid() ? 1 : 0;
			}

			int SoftPath_ClassPathAssetName()
			{
				FSoftClassPath ClassPath(AActor::StaticClass().GetPathName());
				return ClassPath.GetAssetName().IsEmpty() ? 0 : 1;
			}

			int SoftPath_ClassPathPackageName()
			{
				FSoftClassPath ClassPath(AActor::StaticClass().GetPathName());
				return ClassPath.GetLongPackageName().IsEmpty() ? 0 : 1;
			}

			int SoftPath_ClassPathCopyEquality()
			{
				FSoftClassPath ClassPath(AActor::StaticClass().GetPathName());
				FSoftClassPath Copy = ClassPath;
				if (Copy.ToString().IsEmpty())
				{
					return 0;
				}
				return (Copy.ToString() == ClassPath.ToString()) ? 1 : 0;
			}

			int SoftPath_ClassPathFromString()
			{
				FSoftClassPath ClassPath(AActor::StaticClass().GetPathName());
				FSoftClassPath FromString(ClassPath.ToString());
				return (FromString.ToString() == ClassPath.ToString()) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_EmptyIsNull()"), TEXT("Empty FSoftObjectPath should be null"), 1), TEXT("Empty FSoftObjectPath should be null")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ObjectPathValid()"), TEXT("FSoftObjectPath from class path should be valid"), 1), TEXT("FSoftObjectPath from class path should be valid")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ObjectPathAssetName()"), TEXT("FSoftObjectPath should have non-empty asset name"), 1), TEXT("FSoftObjectPath should have non-empty asset name")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ObjectPathPackageName()"), TEXT("FSoftObjectPath should have non-empty package name"), 1), TEXT("FSoftObjectPath should have non-empty package name")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ObjectPathNotSubobject()"), TEXT("FSoftObjectPath from class should not be subobject"), 1), TEXT("FSoftObjectPath from class should not be subobject")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ObjectPathEquality()"), TEXT("FSoftObjectPath equality from same source should hold"), 1), TEXT("FSoftObjectPath equality from same source should hold")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ClassPathValid()"), TEXT("FSoftClassPath from class path should be valid"), 1), TEXT("FSoftClassPath from class path should be valid")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ClassPathAssetName()"), TEXT("FSoftClassPath should have non-empty asset name"), 1), TEXT("FSoftClassPath should have non-empty asset name")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ClassPathPackageName()"), TEXT("FSoftClassPath should have non-empty package name"), 1), TEXT("FSoftClassPath should have non-empty package name")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ClassPathCopyEquality()"), TEXT("FSoftClassPath copy should equal original"), 1), TEXT("FSoftClassPath copy should equal original")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPath_ClassPathFromString()"), TEXT("FSoftClassPath from string roundtrip should match"), 1), TEXT("FSoftClassPath from string roundtrip should match")));
	}

	// ====================================================================
	// Section: SoftPathResolveCompat
	// ====================================================================

	TEST_METHOD(SoftPathResolveAndLoad)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UClass* NativeActorClass = AActor::StaticClass();
		ASSERT_THAT(IsNotNull(NativeActorClass, TEXT("SoftPath resolve compat test should resolve the native actor class")));

		const FSoftObjectPath NativeObjectPath(NativeActorClass);
		const FSoftClassPath NativeClassPath(NativeActorClass);
		const FString ExpectedObjectPathString = NativeObjectPath.ToString();
		const FString ExpectedClassPathString = NativeClassPath.ToString();
		const FString ExpectedAssetName = NativeObjectPath.GetAssetName();
		const FString ExpectedLongPackageName = NativeObjectPath.GetLongPackageName();
		const FString ExpectedObjectAssetPathString = NativeObjectPath.GetAssetPath().ToString();
		const FString ExpectedClassAssetPathString = NativeClassPath.GetAssetPath().ToString();

		ASSERT_THAT(IsTrue(NativeObjectPath.IsValid(), TEXT("Native soft object path baseline should be valid")));
		ASSERT_THAT(IsTrue(NativeClassPath.IsValid(), TEXT("Native soft class path baseline should be valid")));

		FString SoftResolveSource = ASTEST_AS(R"AS(
			int SoftResolve_ObjectPathValid()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return ObjectPath.IsValid() ? 1 : 0;
			}

			int SoftResolve_ObjectPathToString()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return (ObjectPath.ToString() == "__OBJECT_PATH__") ? 1 : 0;
			}

			int SoftResolve_ObjectPathAssetName()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return (ObjectPath.GetAssetName() == "__ASSET_NAME__") ? 1 : 0;
			}

			int SoftResolve_ObjectPathPackageName()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return (ObjectPath.GetLongPackageName() == "__PACKAGE_NAME__") ? 1 : 0;
			}

			int SoftResolve_ObjectPathAssetPath()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return (ObjectPath.GetAssetPath() == FTopLevelAssetPath("__OBJECT_ASSET_PATH__")) ? 1 : 0;
			}

			int SoftResolve_ObjectPathResolve()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return (ObjectPath.ResolveObject() == AActor::StaticClass()) ? 1 : 0;
			}

			int SoftResolve_ObjectPathTryLoad()
			{
				FSoftObjectPath ObjectPath("__OBJECT_PATH__");
				return (ObjectPath.TryLoad() == AActor::StaticClass()) ? 1 : 0;
			}

			int SoftResolve_ClassPathValid()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return ClassPath.IsValid() ? 1 : 0;
			}

			int SoftResolve_ClassPathToString()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return (ClassPath.ToString() == "__CLASS_PATH__") ? 1 : 0;
			}

			int SoftResolve_ClassPathAssetName()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return (ClassPath.GetAssetName() == "__ASSET_NAME__") ? 1 : 0;
			}

			int SoftResolve_ClassPathPackageName()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return (ClassPath.GetLongPackageName() == "__PACKAGE_NAME__") ? 1 : 0;
			}

			int SoftResolve_ClassPathAssetPath()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return (ClassPath.GetAssetPath() == FTopLevelAssetPath("__CLASS_ASSET_PATH__")) ? 1 : 0;
			}

			int SoftResolve_ClassPathResolve()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return (ClassPath.ResolveClass() == AActor::StaticClass()) ? 1 : 0;
			}

			int SoftResolve_ClassPathTryLoad()
			{
				FSoftClassPath ClassPath("__CLASS_PATH__");
				return (ClassPath.TryLoadClass() == AActor::StaticClass()) ? 1 : 0;
			}
			)AS");
		SoftResolveSource.ReplaceInline(TEXT("__OBJECT_PATH__"), *ExpectedObjectPathString.ReplaceCharWithEscapedChar());
		SoftResolveSource.ReplaceInline(TEXT("__CLASS_PATH__"), *ExpectedClassPathString.ReplaceCharWithEscapedChar());
		SoftResolveSource.ReplaceInline(TEXT("__ASSET_NAME__"), *ExpectedAssetName.ReplaceCharWithEscapedChar());
		SoftResolveSource.ReplaceInline(TEXT("__PACKAGE_NAME__"), *ExpectedLongPackageName.ReplaceCharWithEscapedChar());
		SoftResolveSource.ReplaceInline(TEXT("__OBJECT_ASSET_PATH__"), *ExpectedObjectAssetPathString.ReplaceCharWithEscapedChar());
		SoftResolveSource.ReplaceInline(TEXT("__CLASS_ASSET_PATH__"), *ExpectedClassAssetPathString.ReplaceCharWithEscapedChar());

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFileDelegate_SoftResolve"), SoftResolveSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathValid()"), TEXT("FSoftObjectPath from string should be valid"), 1), TEXT("FSoftObjectPath from string should be valid")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathToString()"), TEXT("FSoftObjectPath ToString should roundtrip"), 1), TEXT("FSoftObjectPath ToString should roundtrip")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathAssetName()"), TEXT("FSoftObjectPath GetAssetName should match"), 1), TEXT("FSoftObjectPath GetAssetName should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathPackageName()"), TEXT("FSoftObjectPath GetLongPackageName should match"), 1), TEXT("FSoftObjectPath GetLongPackageName should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathAssetPath()"), TEXT("FSoftObjectPath GetAssetPath should match"), 1), TEXT("FSoftObjectPath GetAssetPath should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathResolve()"), TEXT("FSoftObjectPath ResolveObject should find AActor class"), 1), TEXT("FSoftObjectPath ResolveObject should find AActor class")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ObjectPathTryLoad()"), TEXT("FSoftObjectPath TryLoad should find AActor class"), 1), TEXT("FSoftObjectPath TryLoad should find AActor class")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathValid()"), TEXT("FSoftClassPath from string should be valid"), 1), TEXT("FSoftClassPath from string should be valid")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathToString()"), TEXT("FSoftClassPath ToString should roundtrip"), 1), TEXT("FSoftClassPath ToString should roundtrip")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathAssetName()"), TEXT("FSoftClassPath GetAssetName should match"), 1), TEXT("FSoftClassPath GetAssetName should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathPackageName()"), TEXT("FSoftClassPath GetLongPackageName should match"), 1), TEXT("FSoftClassPath GetLongPackageName should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathAssetPath()"), TEXT("FSoftClassPath GetAssetPath should match"), 1), TEXT("FSoftClassPath GetAssetPath should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathResolve()"), TEXT("FSoftClassPath ResolveClass should find AActor class"), 1), TEXT("FSoftClassPath ResolveClass should find AActor class")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftResolve_ClassPathTryLoad()"), TEXT("FSoftClassPath TryLoadClass should find AActor class"), 1), TEXT("FSoftClassPath TryLoadClass should find AActor class")));
	}

	// ====================================================================
	// Section: SourceMetadataCompat
	// ====================================================================

	TEST_METHOD(SourceMetadataAccessors)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString SourceMetadataFixtureSource = ASTEST_AS(R"AS(
			UCLASS()
			class UBindingSourceMetadataCarrier : UObject
			{
				UFUNCTION()
				int ComputeValue()
				{
					return 7;
				}
			}
			)AS");
		const FString ScriptDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Script/Automation"));
		IFileManager::Get().MakeDirectory(*ScriptDirectory, true);
		const FString ScriptPath = ScriptDirectory / TEXT("ASFileDelegateSourceMetadataProbe.as");
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASFileDelegateSourceMeta"));
			Engine.DiscardModule(TEXT("ASFileDelegateSourceMetadataProbe"));
			IFileManager::Get().Delete(*ScriptPath, false, true, true);
		};

		ASSERT_THAT(IsTrue(
			FFileHelper::SaveStringToFile(SourceMetadataFixtureSource, *ScriptPath),
			TEXT("Write source metadata script file should succeed")));

		const bool bAnnotatedCompiled = CompileAnnotatedModuleFromMemory(&Engine, TEXT("ASFileDelegateSourceMetadataProbe"), ScriptPath, SourceMetadataFixtureSource);
		ASSERT_THAT(IsTrue(bAnnotatedCompiled, TEXT("Compile annotated source metadata module should succeed")));

		FString SourceMetadataAccessSource = ASTEST_AS(R"AS(
			int SourceMeta_ClassFilePath()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}
				return (Type.GetSourceFilePath() == "__SCRIPT_PATH__") ? 1 : 0;
			}

			int SourceMeta_ClassModuleName()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}
				return Type.GetScriptModuleName().Contains("ASFileDelegateSourceMetadataProbe") ? 1 : 0;
			}

			int SourceMeta_ClassTypeDeclaration()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}
				return Type.GetScriptTypeDeclaration().IsEmpty() ? 0 : 1;
			}

			int SourceMeta_FunctionImplementedInScript()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}
				return Type.IsFunctionImplementedInScript(n"ComputeValue") ? 1 : 0;
			}

			int SourceMeta_FunctionFilePath()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}

				UFunction Func = Type.FindFunctionByName(n"ComputeValue");
				if (Func == null)
				{
					return 0;
				}
				return (Func.GetSourceFilePath() == "__SCRIPT_PATH__") ? 1 : 0;
			}

			int SourceMeta_FunctionLineNumber()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}

				UFunction Func = Type.FindFunctionByName(n"ComputeValue");
				if (Func == null)
				{
					return 0;
				}
				return (Func.GetSourceLineNumber() == 5) ? 1 : 0;
			}

			int SourceMeta_FunctionDeclaration()
			{
				UClass Type = FindClass("UBindingSourceMetadataCarrier");
				if (Type == null)
				{
					return 0;
				}

				UFunction Func = Type.FindFunctionByName(n"ComputeValue");
				if (Func == null)
				{
					return 0;
				}
				FString ExpectedDeclaration = "int ComputeValue(";
				ExpectedDeclaration += ")";
				return (Func.GetScriptFunctionDeclaration() == ExpectedDeclaration) ? 1 : 0;
			}
			)AS");
		SourceMetadataAccessSource.ReplaceInline(TEXT("__SCRIPT_PATH__"), *ScriptPath.ReplaceCharWithEscapedChar());

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASFileDelegateSourceMeta", SourceMetadataAccessSource);
		if (Module == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_ClassFilePath()"), TEXT("UClass GetSourceFilePath should match written file"), 1), TEXT("UClass GetSourceFilePath should match written file")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_ClassModuleName()"), TEXT("UClass GetScriptModuleName should contain module name"), 1), TEXT("UClass GetScriptModuleName should contain module name")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_ClassTypeDeclaration()"), TEXT("UClass GetScriptTypeDeclaration should be non-empty"), 1), TEXT("UClass GetScriptTypeDeclaration should be non-empty")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_FunctionImplementedInScript()"), TEXT("IsFunctionImplementedInScript should be true"), 1), TEXT("IsFunctionImplementedInScript should be true")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_FunctionFilePath()"), TEXT("UFunction GetSourceFilePath should match written file"), 1), TEXT("UFunction GetSourceFilePath should match written file")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_FunctionLineNumber()"), TEXT("UFunction GetSourceLineNumber should be 5"), 1), TEXT("UFunction GetSourceLineNumber should be 5")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, *Module,  TEXT("int SourceMeta_FunctionDeclaration()"), TEXT("UFunction GetScriptFunctionDeclaration should match"), 1), TEXT("UFunction GetScriptFunctionDeclaration should match")));
	}

	// ====================================================================
	// Section: FileHelperCompat
	// ====================================================================

	TEST_METHOD(FileHelperSaveAndLoad)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFileDelegate_FileHelper"), ASTEST_AS(R"AS(
			int FileHelper_SaveAndLoad()
			{
				FString Filename = FPaths::CombinePaths(FPaths::ProjectSavedDir(), "AngelscriptFileHelperCompat.txt");
				if (!FFileHelper::SaveStringToFile("HelloFileHelper", Filename))
				{
					return 0;
				}
				FString Loaded;
				if (!FFileHelper::LoadFileToString(Loaded, Filename))
				{
					return 0;
				}
				return (Loaded == "HelloFileHelper") ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FileHelper_SaveAndLoad()"), TEXT("FFileHelper save then load should roundtrip string content"), 1), TEXT("FFileHelper save then load should roundtrip string content")));
	}

	// ====================================================================
	// Section: DelegateWithPayloadCompat
	// ====================================================================

	TEST_METHOD(DelegateWithPayloadPaths)
	{
		TestRunner->AddExpectedError(TEXT("Invalid payload type"), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Invalid object passed to BindUFunction."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("Specified function is not compatible with delegate function."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(TEXT("ASFileDelegate_DelegatePayload"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerInvalidPayloadType()"), EAutomationExpectedErrorFlags::Contains, 0, false);
		TestRunner->AddExpectedError(TEXT("void TriggerInvalidObject()"), EAutomationExpectedErrorFlags::Contains, 0, false);
		TestRunner->AddExpectedError(TEXT("void TriggerSignatureMismatch()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		UAngelscriptNativeScriptTestObject* NativeTestObject = GetMutableDefault<UAngelscriptNativeScriptTestObject>();
		ASSERT_THAT(IsNotNull(NativeTestObject, TEXT("DelegateWithPayload compat test should resolve the native test object")));
		NativeTestObject->bNativeFlag = false;
		NativeTestObject->LargeCount = 0;

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASFileDelegate_DelegatePayload"), ASTEST_AS(R"AS(
			int DelegatePayload_HappyPath()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				if (TestObject == nullptr)
				{
					return 0;
				}

				FAngelscriptDelegateWithPayload NoPayloadDelegate;
				if (NoPayloadDelegate.IsBound())
				{
					return 0;
				}
				NoPayloadDelegate.ExecuteIfBound();
				NoPayloadDelegate.BindUFunction(TestObject, n"MarkNativeFlagFromDelegate");
				if (!NoPayloadDelegate.IsBound())
				{
					return 0;
				}
				NoPayloadDelegate.ExecuteIfBound();

				int PayloadValue = 123;
				FAngelscriptDelegateWithPayload PayloadDelegate;
				PayloadDelegate.BindWithPayload(TestObject, n"SetLargeCountFromDelegate", PayloadValue);
				if (!PayloadDelegate.IsBound())
				{
					return 0;
				}
				PayloadDelegate.ExecuteIfBound();

				return 1;
			}

			void TriggerInvalidPayloadType()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				UObject PayloadObject = TestObject;
				FAngelscriptDelegateWithPayload Delegate;
				Delegate.BindWithPayload(TestObject, n"SetLargeCountFromDelegate", PayloadObject);
			}

			void TriggerInvalidObject()
			{
				FAngelscriptDelegateWithPayload Delegate;
				Delegate.BindUFunction(nullptr, n"MarkNativeFlagFromDelegate");
			}

			void TriggerSignatureMismatch()
			{
				UObject TestObject = FindClass("UAngelscriptNativeScriptTestObject").GetDefaultObject();
				FAngelscriptDelegateWithPayload Delegate;
				Delegate.BindWithPayload(TestObject, n"MarkNativeFlagFromDelegate", 7);
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int DelegatePayload_HappyPath()"), TEXT("DelegateWithPayload happy path should execute both bindings"), 1), TEXT("DelegateWithPayload happy path should execute both bindings")));
		ASSERT_THAT(IsTrue(NativeTestObject->bNativeFlag, TEXT("DelegateWithPayload should execute the bound no-payload native function")));
		ASSERT_THAT(AreEqual(static_cast<int64>(123), NativeTestObject->LargeCount, TEXT("DelegateWithPayload should forward the boxed int payload")));

		ASSERT_THAT(IsTrue(
			ExecuteFunctionExpectingScriptException(
				*TestRunner,
				Engine,
				M,
				TEXT("void TriggerInvalidPayloadType()"),
				TEXT("invalid payload type should raise exception"),
				TEXT("Invalid payload type"))));

		ASSERT_THAT(IsTrue(
			ExecuteFunctionExpectingScriptException(
				*TestRunner,
				Engine,
				M,
				TEXT("void TriggerInvalidObject()"),
				TEXT("invalid object should raise exception"),
				TEXT("Invalid object passed to BindUFunction."))));

		ASSERT_THAT(IsTrue(
			ExecuteFunctionExpectingScriptException(
				*TestRunner,
				Engine,
				M,
				TEXT("void TriggerSignatureMismatch()"),
				TEXT("signature mismatch should raise exception"),
				TEXT("Specified function is not compatible with delegate function."))));
	}
};

#endif
