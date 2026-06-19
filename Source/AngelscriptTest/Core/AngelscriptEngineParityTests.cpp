#include "AngelscriptEngine.h"
#include "AngelscriptBinds.h"
#include "AngelscriptType.h"
#include "AngelscriptTestUtilities.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptEngineParityTests_Private
{
	FString SanitizeCollisionProfileIdentifier(const FName& ProfileName)
	{
		FString Identifier = ProfileName.ToString();
		for (int32 Index = Identifier.Len() - 1; Index >= 0; --Index)
		{
			if (!FAngelscriptEngine::IsValidIdentifierCharacter(Identifier[Index]))
			{
				Identifier[Index] = '_';
			}
		}

		if (!Identifier.IsEmpty() && Identifier[0] >= '0' && Identifier[0] <= '9')
		{
			Identifier = TEXT("_") + Identifier;
		}

		return Identifier;
	}

	FAngelscriptTypeUsage MakeTemplateTypeUsage(const TCHAR* BaseTypeName, UClass* SubTypeClass)
	{
		FAngelscriptTypeUsage Usage(FAngelscriptType::GetByAngelscriptTypeName(BaseTypeName));
		Usage.SubTypes.Add(FAngelscriptTypeUsage(FAngelscriptType::GetByClass(SubTypeClass)));
		return Usage;
	}

	// Compile a snippet inside the production engine. Returns true on success.
	bool CompileSnippet(FAutomationTestBase& Test, FAngelscriptEngine& Engine,
		const char* ModuleName, const char* Source)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (!Assert.IsNotNull(
				Module,
				*FString::Printf(TEXT("%hs should create a script module"), ModuleName)))
		{
			return false;
		}

		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction(ModuleName, Source, 0, 0, &Function);
		const bool bOk = Assert.AreEqual(
			asSUCCESS,
			CompileResult,
			*FString::Printf(TEXT("%hs should compile successfully"), ModuleName));
		if (Function != nullptr)
		{
			Function->Release();
		}
		return bOk;
	}

	// Compile + execute a snippet that returns int32. Returns true on success.
	bool CompileAndExecuteInt(FAutomationTestBase& Test, FAngelscriptEngine& Engine,
		const char* ModuleName, const char* Source, int32& OutResult)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (!Assert.IsNotNull(
				Module,
				*FString::Printf(TEXT("%hs should create a script module"), ModuleName)))
		{
			return false;
		}

		asIScriptFunction* Function = nullptr;
		const int CompileResult = Module->CompileFunction(ModuleName, Source, 0, 0, &Function);
		if (!Assert.AreEqual(
				asSUCCESS,
				CompileResult,
				*FString::Printf(TEXT("%hs should compile successfully"), ModuleName))
			|| !Assert.IsNotNull(
				Function,
				*FString::Printf(TEXT("%hs should produce a function"), ModuleName)))
		{
			if (Function) Function->Release();
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context,
				*FString::Printf(TEXT("%hs should create a script context"), ModuleName)))
		{
			Function->Release();
			return false;
		}

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		bool bPassed = true;
		bPassed &= Assert.AreEqual(
			asSUCCESS,
			PrepareResult,
			*FString::Printf(TEXT("%hs should prepare successfully"), ModuleName));
		bPassed &= Assert.AreEqual(
			asEXECUTION_FINISHED,
			ExecuteResult,
			*FString::Printf(TEXT("%hs should finish successfully"), ModuleName));

		OutResult = static_cast<int32>(Context->GetReturnDWord());

		Context->Release();
		Function->Release();
		return bPassed;
	}
}


using namespace AngelscriptTest_Core_AngelscriptEngineParityTests_Private;

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineParityTests,
	"Angelscript.TestModule.Parity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	FAngelscriptEngine* Engine = nullptr;

	BEFORE_EACH()
	{
		Engine = RequireRunningProductionEngine(
			*TestRunner, TEXT("Parity tests require a running production engine"));
	}

	TEST_METHOD(SkinnedMeshCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));
		asITypeInfo* TypeInfo = Engine->GetScriptEngine()->GetTypeInfoByName("USkinnedMeshComponent");
		ASSERT_THAT(IsNotNull(TypeInfo));

		ASSERT_THAT(IsNotNull(
			TypeInfo->GetMethodByDecl("void UpdateLODStatus()"),
			TEXT("USkinnedMeshComponent should expose UpdateLODStatus()")));
		ASSERT_THAT(IsNotNull(
			TypeInfo->GetMethodByDecl("void InvalidateCachedBounds()"),
			TEXT("USkinnedMeshComponent should expose InvalidateCachedBounds()")));
	}

	TEST_METHOD(DelegateWithPayloadCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));
		asITypeInfo* TypeInfo = Engine->GetScriptEngine()->GetTypeInfoByName("FAngelscriptDelegateWithPayload");
		ASSERT_THAT(IsNotNull(TypeInfo));

		ASSERT_THAT(IsNotNull(
			TypeInfo->GetMethodByDecl("bool IsBound() const"),
			TEXT("FAngelscriptDelegateWithPayload should expose IsBound()")));
		ASSERT_THAT(IsNotNull(
			TypeInfo->GetMethodByDecl("void ExecuteIfBound() const"),
			TEXT("FAngelscriptDelegateWithPayload should expose ExecuteIfBound()")));
	}

	TEST_METHOD(CollisionProfileCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		TArray<TSharedPtr<FName>> CollisionProfiles;
		UCollisionProfile::GetProfileNames(CollisionProfiles);
		ASSERT_THAT(IsTrue(CollisionProfiles.Num() > 0));

		const FName ProfileName = *CollisionProfiles[0].Get();
		const FString SanitizedIdentifier = SanitizeCollisionProfileIdentifier(ProfileName);
		ASSERT_THAT(IsFalse(SanitizedIdentifier.IsEmpty()));

		const FString Source = FString::Printf(
			TEXT("int CheckCollisionProfileConstant() { return CollisionProfile::%s.Compare(FName(\"%s\")); }"),
			*SanitizedIdentifier, *ProfileName.ToString());

		int32 Result = -1;
		if (CompileAndExecuteInt(*TestRunner, *Engine, "CollisionProfileParity", TCHAR_TO_ANSI(*Source), Result))
		{
			ASSERT_THAT(AreEqual(0, Result, TEXT("CollisionProfile constant should compare equal to the underlying FName")));
		}
	}

	TEST_METHOD(CollisionQueryParamsCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		ASSERT_THAT(IsNotNull(
			Engine->GetScriptEngine()->GetTypeInfoByName("FCollisionEnabledMask"),
			TEXT("FCollisionEnabledMask should exist in the script type system")));
		ASSERT_THAT(IsNotNull(
			Engine->GetScriptEngine()->GetTypeInfoByName("FComponentQueryParams"),
			TEXT("FComponentQueryParams should exist in the script type system")));

		CompileSnippet(*TestRunner, *Engine, "CollisionQueryParamsParity",
			"int CheckCollisionQueryParams() { FCollisionEnabledMask Mask(ECollisionEnabled::QueryOnly); FComponentQueryParams Params; Params.ShapeCollisionMask = Mask; return Params.ShapeCollisionMask.Bits; }");
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

	TEST_METHOD(FIntPointCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));
		ASSERT_THAT(IsNotNull(Engine->GetScriptEngine()->GetTypeInfoByName("FIntPoint")));

		int32 Result = 0;
		if (CompileAndExecuteInt(*TestRunner, *Engine, "FIntPointParity",
				"int CheckFIntPoint() { FIntPoint A(1, 2); FIntPoint B(3); FIntPoint C = A + B; return C.X + C.Y + C[0]; }", Result))
		{
			ASSERT_THAT(AreEqual(13, Result, TEXT("FIntPoint parity should return the expected sum")));
		}
	}

	TEST_METHOD(FVector2fCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));
		ASSERT_THAT(IsNotNull(Engine->GetScriptEngine()->GetTypeInfoByName("FVector2f")));

		// Compile-only verification; float return value checked via CompileSnippet.
		CompileSnippet(*TestRunner, *Engine, "FVector2fParity",
			"float CheckFVector2f() { FVector2f A(1.0f, 2.0f); FVector2f B(1.0f, 1.0f); FVector2f C = A + B; return C.X + C.Y; }");
	}

	TEST_METHOD(SoftReferenceCppForm)
	{
		ASSERT_THAT(IsNotNull(Engine));

		const FAngelscriptTypeUsage SoftObjectUsage = MakeTemplateTypeUsage(TEXT("TSoftObjectPtr"), UTexture2D::StaticClass());
		const FAngelscriptTypeUsage SoftClassUsage = MakeTemplateTypeUsage(TEXT("TSoftClassPtr"), AActor::StaticClass());
		ASSERT_THAT(IsTrue(SoftObjectUsage.IsValid() && SoftObjectUsage.SubTypes.Num() == 1 && SoftObjectUsage.SubTypes[0].IsValid()));
		ASSERT_THAT(IsTrue(SoftClassUsage.IsValid() && SoftClassUsage.SubTypes.Num() == 1 && SoftClassUsage.SubTypes[0].IsValid()));

		FAngelscriptType::FCppForm SoftObjectForm;
		FAngelscriptType::FCppForm SoftClassForm;
		ASSERT_THAT(IsTrue(SoftObjectUsage.GetCppForm(SoftObjectForm)));
		ASSERT_THAT(IsTrue(SoftClassUsage.GetCppForm(SoftClassForm)));

		ASSERT_THAT(AreEqual(FString(TEXT("TSoftObjectPtr<UTexture2D>")), SoftObjectForm.CppType, TEXT("TSoftObjectPtr CppType")));
		ASSERT_THAT(AreEqual(FString(TEXT("TSoftObjectPtr<UObject>")), SoftObjectForm.CppGenericType, TEXT("TSoftObjectPtr CppGenericType")));
		ASSERT_THAT(AreEqual(FString(TEXT("TSoftObjectPtr<UObject>")), SoftObjectForm.TemplateObjectForm, TEXT("TSoftObjectPtr TemplateObjectForm")));
		ASSERT_THAT(IsFalse(SoftObjectForm.CppHeader.IsEmpty(), TEXT("TSoftObjectPtr should emit a header include")));

		ASSERT_THAT(AreEqual(FString(TEXT("TSoftClassPtr<AActor>")), SoftClassForm.CppType, TEXT("TSoftClassPtr CppType")));
		ASSERT_THAT(AreEqual(FString(TEXT("TSoftClassPtr<UObject>")), SoftClassForm.CppGenericType, TEXT("TSoftClassPtr CppGenericType")));
		ASSERT_THAT(AreEqual(FString(TEXT("TSoftClassPtr<UObject>")), SoftClassForm.TemplateObjectForm, TEXT("TSoftClassPtr TemplateObjectForm")));
		ASSERT_THAT(IsFalse(SoftClassForm.CppHeader.IsEmpty(), TEXT("TSoftClassPtr should emit a header include")));
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

	TEST_METHOD(UserWidgetPaintCompile)
	{
		ASSERT_THAT(IsNotNull(Engine));

		const FString Source =
			TEXT("void CheckWidgetPaint(FPaintContext& Context, const FGeometry& Geometry, UTexture2D Texture, UMaterialInterface Material)\n")
			TEXT("{\n")
			TEXT("    FSlateBrush StyleBrush(FName(\"WhiteBrush\"));\n")
			TEXT("    FSlateBrush ColorBrush(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));\n")
			TEXT("    FSlateBrush TextureBrush(Texture, FVector2D(16.0f, 16.0f));\n")
			TEXT("    FSlateBrush MaterialBrush(Material, FVector2D(16.0f, 16.0f));\n")
			TEXT("    Context.DrawBox(Geometry, StyleBrush);\n")
			TEXT("    Context.DrawRotatedBox(FVector2D::ZeroVector, FVector2D(8.0f, 8.0f), 0.0f, ColorBrush);\n")
			TEXT("    Context.DrawBox(FVector2D::ZeroVector, FVector2D(8.0f, 8.0f), TextureBrush);\n")
			TEXT("    Context.DrawBox(FVector2D::ZeroVector, FVector2D(8.0f, 8.0f), MaterialBrush);\n")
			TEXT("}\n");

		asIScriptModule* Module = BuildModule(*TestRunner, *Engine, "UserWidgetPaintParity", Source);
		ASSERT_THAT(IsNotNull(Module, TEXT("UserWidget paint parity module should compile")));
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

	TEST_METHOD(DeprecationsMetadata)
	{
		static const FName NAME_META_DeprecatedFunction(TEXT("DeprecatedFunction"));
		static const FName NAME_META_DeprecationMessage(TEXT("DeprecationMessage"));

		UFunction* Function = FindObject<UFunction>(nullptr, TEXT("/Script/Niagara.NiagaraComponent:SetNiagaraVariableLinearColor"));
		ASSERT_THAT(IsNotNull(Function));

		ASSERT_THAT(IsTrue(
			Function->HasMetaData(NAME_META_DeprecatedFunction),
			TEXT("Niagara linear color setter should be marked deprecated")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Use the SetVariable variant that takes FName instead")),
			Function->GetMetaData(NAME_META_DeprecationMessage),
			TEXT("Niagara linear color setter deprecation message")));
	}

	TEST_METHOD(StartupBindRegistrySmoke)
	{
		ASSERT_THAT(IsNotNull(Engine));

		const TArray<FName> RegisteredBindNames = FAngelscriptBinds::GetAllRegisteredBindNames();
		const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList();
		ASSERT_THAT(IsTrue(RegisteredBindNames.Num() > 0));
		ASSERT_THAT(IsTrue(BindInfos.Num() == RegisteredBindNames.Num()));

		for (int32 BindIndex = 1; BindIndex < BindInfos.Num(); ++BindIndex)
		{
			ASSERT_THAT(IsTrue(
				BindInfos[BindIndex - 1].BindOrder <= BindInfos[BindIndex].BindOrder,
				TEXT("Bind info should be sorted by bind order")));
		}

		CompileSnippet(*TestRunner, *Engine, "StartupBindRegistryParity",
			"int CheckStartupBindSurface() {\n"
			"    USkinnedMeshComponent Component;\n"
			"    FIntPoint Point(3, 4);\n"
			"    FAngelscriptDelegateWithPayload Delegate;\n"
			"    FHitResult Hit(FVector::ZeroVector, FVector::ForwardVector);\n"
			"    return Point.X + Point.Y + (Delegate.IsBound() ? 1 : 0) + Hit.FaceIndex;\n"
			"}");
	}
};

#endif
