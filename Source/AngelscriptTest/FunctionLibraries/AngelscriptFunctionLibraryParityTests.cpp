#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptFunctionLibraryParityTests,
	"Angelscript.TestModule.FunctionLibraries.Parity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CompileSnippet(FAutomationTestBase& Test, FAngelscriptEngine& Engine,
		const char* ModuleName, const char* Source)
	{
		FNoDiscardAsserter LocalAssert(Test);
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (!LocalAssert.IsNotNull(
				Module,
				*FString::Printf(TEXT("%hs should create a script module"), ModuleName)))
		{
			return false;
		}

		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction(ModuleName, Source, 0, 0, &Function);
		const bool bOk = LocalAssert.AreEqual(
			asSUCCESS,
			CompileResult,
			*FString::Printf(TEXT("%hs should compile successfully"), ModuleName));
		if (Function != nullptr)
		{
			Function->Release();
		}
		return bOk;
	}

	static bool CompileAndExecuteInt(FAutomationTestBase& Test, FAngelscriptEngine& Engine,
		const char* ModuleName, const char* Source, int32& OutResult)
	{
		FNoDiscardAsserter LocalAssert(Test);
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (!LocalAssert.IsNotNull(
				Module,
				*FString::Printf(TEXT("%hs should create a script module"), ModuleName)))
		{
			return false;
		}

		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction(ModuleName, Source, 0, 0, &Function);
		if (!LocalAssert.AreEqual(
				asSUCCESS,
				CompileResult,
				*FString::Printf(TEXT("%hs should compile successfully"), ModuleName))
			|| !LocalAssert.IsNotNull(
				Function,
				*FString::Printf(TEXT("%hs should produce a function"), ModuleName)))
		{
			if (Function != nullptr)
			{
				Function->Release();
			}
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!LocalAssert.IsNotNull(
				Context,
				*FString::Printf(TEXT("%hs should create a script context"), ModuleName)))
		{
			Function->Release();
			return false;
		}

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		bool bPassed = true;
		bPassed &= LocalAssert.AreEqual(
			asSUCCESS,
			PrepareResult,
			*FString::Printf(TEXT("%hs should prepare successfully"), ModuleName));
		bPassed &= LocalAssert.AreEqual(
			asEXECUTION_FINISHED,
			ExecuteResult,
			*FString::Printf(TEXT("%hs should finish successfully"), ModuleName));

		OutResult = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		Function->Release();
		return bPassed;
	}

public:
	FAngelscriptEngine* Engine = nullptr;

	BEFORE_EACH()
	{
		Engine = RequireRunningProductionEngine(
			*TestRunner, TEXT("Function library parity tests require a running production engine"));
	}

	TEST_METHOD(WorldCollisionCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		CompileSnippet(*TestRunner, *Engine, "WorldCollisionParity",
			"void CheckWorldCollision(UPrimitiveComponent PrimitiveComponent)\n"
			"{\n"
			"    FCollisionQueryParams QueryParams;\n"
			"    FCollisionResponseParams ResponseParams;\n"
			"    FCollisionObjectQueryParams ObjectQueryParams;\n"
			"    FComponentQueryParams ComponentQueryParams;\n"
			"    FCollisionShape Shape = FCollisionShape::MakeSphere(10.0f);\n"
			"    FHitResult Hit;\n"
			"    TArray<FHitResult> Hits;\n"
			"    TArray<FOverlapResult> Overlaps;\n"
			"    System::LineTraceTestByChannel(FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), ECollisionChannel::ECC_Visibility, QueryParams, ResponseParams);\n"
			"    System::SweepSingleByObjectType(Hit, FVector::ZeroVector, FVector(100.0f, 0.0f, 0.0f), FQuat::Identity, ObjectQueryParams, Shape, QueryParams);\n"
			"    System::OverlapMultiByProfile(Overlaps, FVector::ZeroVector, FQuat::Identity, CollisionProfile::BlockAllDynamic, Shape, QueryParams);\n"
			"    System::ComponentSweepMulti(Hits, PrimitiveComponent, FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f), FQuat::Identity, ComponentQueryParams);\n"
			"    System::ComponentOverlapMulti(Overlaps, PrimitiveComponent, FVector::ZeroVector, FQuat::Identity, ComponentQueryParams, ObjectQueryParams);\n"
			"    System::AsyncOverlapByProfile(FVector::ZeroVector, FQuat::Identity, CollisionProfile::BlockAllDynamic, Shape, QueryParams);\n"
			"}");
	}

	TEST_METHOD(SoftReferenceCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		const FString Source =
			TEXT("UObject CheckSoftObjectGet(TSoftObjectPtr<UObject> Ptr)\n")
			TEXT("{\n")
			TEXT("    return Ptr.Get();\n")
			TEXT("}\n")
			TEXT("UTexture2D CheckSoftObjectEditorLoad(TSoftObjectPtr<UTexture2D> Ptr)\n")
			TEXT("{\n")
			TEXT("    return Ptr.EditorOnlyLoadSynchronous();\n")
			TEXT("}\n")
			TEXT("TSubclassOf<AActor> CheckSoftClassGet(TSoftClassPtr<AActor> Ptr)\n")
			TEXT("{\n")
			TEXT("    return Ptr.Get();\n")
			TEXT("}\n");

		asIScriptModule* Module = BuildModule(*TestRunner, *Engine, "Editor.SoftReferenceParity", Source);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(IsNotNull(
			GetFunctionByDecl(*TestRunner, *Module, TEXT("UObject CheckSoftObjectGet(TSoftObjectPtr<UObject> Ptr)")),
			TEXT("TSoftObjectPtr Get() smoke")));
		ASSERT_THAT(IsNotNull(
			GetFunctionByDecl(*TestRunner, *Module, TEXT("UTexture2D CheckSoftObjectEditorLoad(TSoftObjectPtr<UTexture2D> Ptr)")),
			TEXT("TSoftObjectPtr editor-only soft load smoke")));
		ASSERT_THAT(IsNotNull(
			GetFunctionByDecl(*TestRunner, *Module, TEXT("TSubclassOf<AActor> CheckSoftClassGet(TSoftClassPtr<AActor> Ptr)")),
			TEXT("TSoftClassPtr Get() smoke")));
	}

	TEST_METHOD(LevelStreamingCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		asITypeInfo* TypeInfo = Engine->GetScriptEngine()->GetTypeInfoByName("ULevelStreaming");
		ASSERT_THAT(IsNotNull(TypeInfo));
		ASSERT_THAT(IsNotNull(
			Engine->GetScriptEngine()->GetTypeInfoByName("UAngelscriptLevelStreamingLibrary"),
			TEXT("UAngelscriptLevelStreamingLibrary should be visible")));
		ASSERT_THAT(IsNotNull(
			TypeInfo->GetMethodByDecl("bool GetShouldBeVisibleInEditor() const"),
			TEXT("ULevelStreaming should expose GetShouldBeVisibleInEditor()")));
	}

	TEST_METHOD(RuntimeCurveLinearColorCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		asITypeInfo* TypeInfo = Engine->GetScriptEngine()->GetTypeInfoByName("FRuntimeCurveLinearColor");
		ASSERT_THAT(IsNotNull(TypeInfo));
		ASSERT_THAT(IsNotNull(TypeInfo->GetMethodByName("AddDefaultKey")));
		CompileSnippet(*TestRunner, *Engine, "RuntimeCurveLinearColorParity",
			"void CheckRuntimeCurve() { FRuntimeCurveLinearColor Curve; URuntimeCurveLinearColorMixinLibrary::AddDefaultKey(Curve, 0.0f, FLinearColor::Red); Curve.AddDefaultKey(0.0f, FLinearColor::Red); }");
	}

	TEST_METHOD(HitResultCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		int32 Result = 0;
		if (CompileAndExecuteInt(*TestRunner, *Engine, "HitResultParity",
				"int CheckHitResult() {\n"
				"    FHitResult Hit(FVector::ZeroVector, FVector::ForwardVector);\n"
				"    Hit.FaceIndex = 1;\n"
				"    Hit.ElementIndex = 2;\n"
				"    Hit.Item = 3;\n"
				"    Hit.MyItem = 4;\n"
				"    Hit.BoneName = FName(\"Bone\");\n"
				"    Hit.MyBoneName = FName(\"MyBone\");\n"
				"    return Hit.FaceIndex + Hit.ElementIndex + Hit.Item + Hit.MyItem;\n"
				"}", Result))
		{
			ASSERT_THAT(AreEqual(10, Result, TEXT("FHitResult parity should read/write the restored fields")));
		}
	}
};

#endif
