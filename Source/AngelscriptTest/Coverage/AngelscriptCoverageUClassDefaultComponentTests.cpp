#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Components/ActorTestSpawner.h"
#include "Components/ActorComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

/*
DefaultComponent coverage map:
| TEST_METHOD | Matrix rows |
|-------------|-------------|
| DefaultComponentRootAttachSocketRuntimeMatrix | DefaultComponent, RootComponent, Attach, AttachSocket, script scene/logic components |
| DefaultComponentShowOnActorSpecifierSurface | ShowOnActor, EditAnywhere, BlueprintReadOnly/ReadWrite, Category metadata |
| DefaultComponentNativeComponentTypeMatrix | native component type matrix |
| DefaultComponentInheritanceAndForwardAttachMatrix | inherited components, nested attach, forward attach targets |
| DefaultComponentImplicitRootAndDelayedAttachMatrix | implicit root, delayed attach, non-scene component ownership |
| DefaultComponentInvalidSpecifierBoundaryMatrix | invalid specifier and component-type diagnostics |
*/
TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUClassDefaultComponentTest,
	"Angelscript.TestModule.Coverage.UClass.DefaultComponent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool FindCompileErrorContaining(const FAngelscriptCompileTraceSummary& Summary, FAngelscriptEngine& Engine, const FString& ExpectedFragment)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.Message.Contains(ExpectedFragment))
			{
				return true;
			}
		}

		for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& FileDiagnostics : Engine.Diagnostics)
		{
			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : FileDiagnostics.Value.Diagnostics)
			{
				if (Diagnostic.Message.Contains(ExpectedFragment))
				{
					return true;
				}
			}
		}

		return false;
	}

	static bool HasEngineCompileErrors(FAngelscriptEngine& Engine)
	{
		for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& FileDiagnostics : Engine.Diagnostics)
		{
			for (const FAngelscriptEngine::FDiagnostic& Diagnostic : FileDiagnostics.Value.Diagnostics)
			{
				if (Diagnostic.bIsError)
				{
					return true;
				}
			}
		}

		return false;
	}

	static bool CompileFixture(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName, const FString& Filename, const FString& ScriptSource)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptSource),
			*FString::Printf(TEXT("DefaultComponent coverage module '%s' should compile"), *ModuleName.ToString()));
	}

	static bool CompileFixtureShouldFail(FAutomationTestBase& Test, FAngelscriptEngine& Engine, FName ModuleName, const FString& Filename, const FString& ScriptSource, TArrayView<const FString> ExpectedDiagnosticFragments)
	{
		FNoDiscardAsserter LocalAssert(Test);

		Engine.ResetDiagnostics();

		for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
		{
			Test.AddExpectedError(*ExpectedFragment, EAutomationExpectedErrorFlags::Contains, -1);
		}
		Test.AddExpectedError(TEXT("An error was encountered during angelscript hot reload"), EAutomationExpectedErrorFlags::Contains, -1);

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ModuleName,
			Filename,
			ScriptSource,
			true,
			Summary,
			true);

		bool bFoundExpectedDiagnostic = false;
		for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
		{
			if (FindCompileErrorContaining(Summary, Engine, ExpectedFragment))
			{
				bFoundExpectedDiagnostic = true;
				break;
			}
		}

		const bool bFailed = !Summary.bCompileSucceeded
			|| Summary.CompileResult == ECompileResult::Error
			|| !bCompiled
			|| HasEngineCompileErrors(Engine)
			|| bFoundExpectedDiagnostic;

		bool bPassed = LocalAssert.IsTrue(
			bFailed,
			*FString::Printf(TEXT("DefaultComponent coverage module '%s' should fail compilation"), *ModuleName.ToString()));
		if (!HasEngineCompileErrors(Engine))
		{
			bPassed &= LocalAssert.AreEqual(
				ECompileResult::Error,
				Summary.CompileResult,
				*FString::Printf(TEXT("DefaultComponent coverage module '%s' should report compile error"), *ModuleName.ToString()));
		}

		for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
		{
			bPassed &= LocalAssert.IsTrue(
				FindCompileErrorContaining(Summary, Engine, ExpectedFragment),
				*FString::Printf(TEXT("DefaultComponent coverage module '%s' diagnostics should contain '%s'"),
					*ModuleName.ToString(),
					*ExpectedFragment));
		}

		Engine.DiscardModule(*ModuleName.ToString());
		Engine.ResetDiagnostics();
		return bPassed;
	}

	static int32 CountActorComponentsByClass(const AActor* Actor, const UClass* ComponentClass)
	{
		if (Actor == nullptr || ComponentClass == nullptr)
		{
			return 0;
		}

		int32 Count = 0;
		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->IsA(ComponentClass))
			{
				++Count;
			}
		}
		return Count;
	}

	static UActorComponent* FindActorComponentByName(const AActor* Actor, FName ComponentName)
	{
		if (Actor == nullptr)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component != nullptr && Component->GetFName() == ComponentName)
			{
				return Component;
			}
		}
		return nullptr;
	}

	static bool ReadObjectByPath(FAutomationTestBase& Test, UObject* Object, FStringView Path, UObject*& OutValue, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(GetObjectByPath(Test, Object, Path, OutValue), Message);
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

	TEST_METHOD(DefaultComponentRootAttachSocketRuntimeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassDefaultComponent_RootAttachSocketRuntimeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDefaultComponentScriptScene : USceneComponent
			{
				UPROPERTY()
				int SceneMarker = 17;
			}

			UCLASS()
			class UCoverageUClassDefaultComponentScriptLogic : UActorComponent
			{
				UPROPERTY()
				int LogicMarker = 23;
			}

			UCLASS()
			class ACoverageUClassDefaultComponentRootActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="MeshSocket")
				UStaticMeshComponent Mesh;

				UPROPERTY(DefaultComponent, AttachSocket="RootSocket")
				USceneComponent RootSocketChild;

				UPROPERTY(DefaultComponent, Attach=Mesh)
				UCoverageUClassDefaultComponentScriptScene ScriptScene;

				UPROPERTY(DefaultComponent)
				UCoverageUClassDefaultComponentScriptLogic Logic;

				UPROPERTY()
				bool RootCreated = false;

				UPROPERTY()
				bool AttachmentsCreated = false;

				UPROPERTY()
				bool ScriptDefaultsCreated = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RootCreated =
						Root != nullptr &&
						Root.GetOwner() == this &&
						Root.GetAttachParent() == nullptr;

					AttachmentsCreated =
						Mesh != nullptr &&
						RootSocketChild != nullptr &&
						ScriptScene != nullptr &&
						Mesh.GetAttachParent() == Root &&
						Mesh.GetAttachSocketName() == n"MeshSocket" &&
						RootSocketChild.GetAttachParent() == Root &&
						RootSocketChild.GetAttachSocketName() == n"RootSocket" &&
						ScriptScene.GetAttachParent() == Mesh;

					ScriptDefaultsCreated =
						ScriptScene != nullptr &&
						Logic != nullptr &&
						ScriptScene.GetOwner() == this &&
						Logic.GetOwner() == this &&
						ScriptScene.SceneMarker == 17 &&
						Logic.LogicMarker == 23;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentRootAttachSocketRuntimeMatrix.as"), ScriptSource)));

		UClass* SceneComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDefaultComponentScriptScene"));
		UClass* LogicComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDefaultComponentScriptLogic"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentRootActor"));
		ASSERT_THAT(IsNotNull(SceneComponentClass, TEXT("Script scene default component class should be generated")));
		ASSERT_THAT(IsNotNull(LogicComponentClass, TEXT("Script logic default component class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Default component actor should be generated")));
		if (SceneComponentClass == nullptr || LogicComponentClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* RootProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Root"));
		FObjectPropertyBase* MeshProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Mesh"));
		FObjectPropertyBase* RootSocketChildProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("RootSocketChild"));
		FObjectPropertyBase* ScriptSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("ScriptScene"));
		FObjectPropertyBase* LogicProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Logic"));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Root default component property should be reflected")));
		ASSERT_THAT(IsNotNull(MeshProperty, TEXT("Mesh default component property should be reflected")));
		ASSERT_THAT(IsNotNull(RootSocketChildProperty, TEXT("RootSocketChild default component property should be reflected")));
		ASSERT_THAT(IsNotNull(ScriptSceneProperty, TEXT("ScriptScene default component property should be reflected")));
		ASSERT_THAT(IsNotNull(LogicProperty, TEXT("Logic default component property should be reflected")));
		if (RootProperty == nullptr || MeshProperty == nullptr || RootSocketChildProperty == nullptr || ScriptSceneProperty == nullptr || LogicProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(USceneComponent::StaticClass(), RootProperty->PropertyClass, TEXT("Root should reflect USceneComponent")));
		ASSERT_THAT(AreEqual(UStaticMeshComponent::StaticClass(), MeshProperty->PropertyClass, TEXT("Mesh should reflect UStaticMeshComponent")));
		ASSERT_THAT(AreEqual(USceneComponent::StaticClass(), RootSocketChildProperty->PropertyClass, TEXT("RootSocketChild should reflect USceneComponent")));
		ASSERT_THAT(AreEqual(SceneComponentClass, ScriptSceneProperty->PropertyClass, TEXT("ScriptScene should preserve the script component class")));
		ASSERT_THAT(AreEqual(LogicComponentClass, LogicProperty->PropertyClass, TEXT("Logic should preserve the script component class")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("Root should keep DefaultComponent metadata")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("RootComponent")), TEXT("Root should keep RootComponent metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Root")), MeshProperty->GetMetaData(TEXT("Attach")), TEXT("Mesh should keep Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("MeshSocket")), MeshProperty->GetMetaData(TEXT("AttachSocket")), TEXT("Mesh should keep AttachSocket metadata")));
		ASSERT_THAT(IsFalse(RootSocketChildProperty->HasMetaData(TEXT("Attach")), TEXT("RootSocketChild should cover AttachSocket without explicit Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("RootSocket")), RootSocketChildProperty->GetMetaData(TEXT("AttachSocket")), TEXT("RootSocketChild should keep standalone AttachSocket metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Mesh")), ScriptSceneProperty->GetMetaData(TEXT("Attach")), TEXT("ScriptScene should keep nested Attach metadata")));
		ASSERT_THAT(IsFalse(LogicProperty->HasMetaData(TEXT("Attach")), TEXT("Logic should cover non-scene default component without attach metadata")));
		ASSERT_THAT(IsTrue(RootProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("Root default component should be instanced and exported")));
		ASSERT_THAT(IsTrue(MeshProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("Mesh default component should be instanced and exported")));
		ASSERT_THAT(IsTrue(RootSocketChildProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("RootSocketChild default component should be instanced and exported")));
		ASSERT_THAT(IsTrue(ScriptSceneProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("ScriptScene default component should be instanced and exported")));
		ASSERT_THAT(IsTrue(LogicProperty->HasAllPropertyFlags(CPF_InstancedReference | CPF_ExportObject), TEXT("Logic default component should be instanced and exported")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Default component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("RootCreated"), true, TEXT("Root default component should be created and owned"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AttachmentsCreated"), true, TEXT("Attach and AttachSocket should materialize at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ScriptDefaultsCreated"), true, TEXT("Script scene and logic default component defaults should materialize"))));

		UObject* RootObject = nullptr;
		UObject* MeshObject = nullptr;
		UObject* RootSocketChildObject = nullptr;
		UObject* ScriptSceneObject = nullptr;
		UObject* LogicObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("Root"), RootObject, TEXT("Root property should be readable"))));
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("Mesh"), MeshObject, TEXT("Mesh property should be readable"))));
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("RootSocketChild"), RootSocketChildObject, TEXT("RootSocketChild property should be readable"))));
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("ScriptScene"), ScriptSceneObject, TEXT("ScriptScene property should be readable"))));
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("Logic"), LogicObject, TEXT("Logic property should be readable"))));

		USceneComponent* RootComponent = Cast<USceneComponent>(RootObject);
		USceneComponent* MeshComponent = Cast<USceneComponent>(MeshObject);
		USceneComponent* RootSocketChildComponent = Cast<USceneComponent>(RootSocketChildObject);
		USceneComponent* ScriptSceneComponent = Cast<USceneComponent>(ScriptSceneObject);
		UActorComponent* LogicComponent = Cast<UActorComponent>(LogicObject);
		ASSERT_THAT(IsNotNull(RootComponent, TEXT("Root property should store a scene component")));
		ASSERT_THAT(IsNotNull(MeshComponent, TEXT("Mesh property should store a scene component")));
		ASSERT_THAT(IsNotNull(RootSocketChildComponent, TEXT("RootSocketChild property should store a scene component")));
		ASSERT_THAT(IsNotNull(ScriptSceneComponent, TEXT("ScriptScene property should store a script scene component")));
		ASSERT_THAT(IsNotNull(LogicComponent, TEXT("Logic property should store a script actor component")));
		if (RootComponent == nullptr || MeshComponent == nullptr || RootSocketChildComponent == nullptr || ScriptSceneComponent == nullptr || LogicComponent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor->GetRootComponent()), static_cast<UObject*>(RootComponent), TEXT("RootComponent should assign the actor root")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(RootComponent), static_cast<UObject*>(MeshComponent->GetAttachParent()), TEXT("Mesh should attach to Root")));
		ASSERT_THAT(AreEqual(FName(TEXT("MeshSocket")), MeshComponent->GetAttachSocketName(), TEXT("Mesh should retain the declared attach socket")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(RootComponent), static_cast<UObject*>(RootSocketChildComponent->GetAttachParent()), TEXT("RootSocketChild should attach to the root when only AttachSocket is declared")));
		ASSERT_THAT(AreEqual(FName(TEXT("RootSocket")), RootSocketChildComponent->GetAttachSocketName(), TEXT("RootSocketChild should retain the standalone attach socket")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(MeshComponent), static_cast<UObject*>(ScriptSceneComponent->GetAttachParent()), TEXT("ScriptScene should attach to Mesh")));
		ASSERT_THAT(AreEqual(Actor, LogicComponent->GetOwner(), TEXT("Logic should be owned by the actor")));
		ASSERT_THAT(AreEqual(5, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Actor should own exactly the declared default components")));
	}

	TEST_METHOD(DefaultComponentShowOnActorSpecifierSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassDefaultComponent_ShowOnActorSpecifierSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentShowOnActor : AActor
			{
				UPROPERTY(ShowOnActor, DefaultComponent, RootComponent, EditAnywhere, BlueprintReadOnly, Category="Coverage|Root")
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, ShowOnActor, EditAnywhere, BlueprintReadWrite, Category="Coverage|Visible")
				UArrowComponent VisibleArrow;

				UPROPERTY(DefaultComponent, Attach=Root, BlueprintReadOnly, Category="Coverage|Hidden")
				USceneComponent HiddenScene;

				UPROPERTY(DefaultComponent, Attach=Root, EditAnywhere, Category="Coverage|Editable")
				USceneComponent EditableScene;

				UPROPERTY()
				bool VisibleArrowAttached = false;

				UPROPERTY()
				bool HiddenSceneAttached = false;

				UPROPERTY()
				bool EditableSceneAttached = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					VisibleArrowAttached =
						VisibleArrow != nullptr &&
						Root != nullptr &&
						VisibleArrow.GetAttachParent() == Root;

					HiddenSceneAttached =
						HiddenScene != nullptr &&
						Root != nullptr &&
						HiddenScene.GetAttachParent() == Root;

					EditableSceneAttached =
						EditableScene != nullptr &&
						Root != nullptr &&
						EditableScene.GetAttachParent() == Root;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentShowOnActorSpecifierSurface.as"), ScriptSource)));

		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentShowOnActor"));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("ShowOnActor default component actor should be generated")));
		if (ActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* RootProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Root"));
		FObjectPropertyBase* VisibleArrowProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("VisibleArrow"));
		FObjectPropertyBase* HiddenSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("HiddenScene"));
		FObjectPropertyBase* EditableSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("EditableScene"));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Root property should be reflected")));
		ASSERT_THAT(IsNotNull(VisibleArrowProperty, TEXT("VisibleArrow property should be reflected")));
		ASSERT_THAT(IsNotNull(HiddenSceneProperty, TEXT("HiddenScene property should be reflected")));
		ASSERT_THAT(IsNotNull(EditableSceneProperty, TEXT("EditableScene property should be reflected")));
		if (RootProperty == nullptr || VisibleArrowProperty == nullptr || HiddenSceneProperty == nullptr || EditableSceneProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("Root should keep DefaultComponent metadata")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("RootComponent")), TEXT("Root should keep RootComponent metadata")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should emit EditInline metadata when it precedes DefaultComponent")));
		ASSERT_THAT(IsTrue(RootProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("Root should be editable")));
		ASSERT_THAT(IsTrue(RootProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("Root should be Blueprint-visible")));
		ASSERT_THAT(IsTrue(RootProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly should mark Root read-only")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Root")), RootProperty->GetMetaData(TEXT("Category")), TEXT("Root should preserve Category metadata")));

		ASSERT_THAT(AreEqual(UArrowComponent::StaticClass(), VisibleArrowProperty->PropertyClass, TEXT("VisibleArrow should reflect UArrowComponent")));
		ASSERT_THAT(IsTrue(VisibleArrowProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("VisibleArrow should keep DefaultComponent metadata")));
		ASSERT_THAT(IsTrue(VisibleArrowProperty->HasMetaData(TEXT("EditInline")), TEXT("ShowOnActor should emit EditInline metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Root")), VisibleArrowProperty->GetMetaData(TEXT("Attach")), TEXT("VisibleArrow should keep Attach metadata")));
		ASSERT_THAT(IsTrue(VisibleArrowProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("VisibleArrow should be editable")));
		ASSERT_THAT(IsTrue(VisibleArrowProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("VisibleArrow should be Blueprint-visible")));
		ASSERT_THAT(IsFalse(VisibleArrowProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadWrite should leave VisibleArrow writable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Visible")), VisibleArrowProperty->GetMetaData(TEXT("Category")), TEXT("VisibleArrow should preserve Category metadata")));

		ASSERT_THAT(IsTrue(HiddenSceneProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("HiddenScene should keep DefaultComponent metadata")));
		ASSERT_THAT(IsFalse(HiddenSceneProperty->HasMetaData(TEXT("EditInline")), TEXT("HiddenScene should cover default component without ShowOnActor EditInline metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Root")), HiddenSceneProperty->GetMetaData(TEXT("Attach")), TEXT("HiddenScene should keep Attach metadata")));
		ASSERT_THAT(IsTrue(HiddenSceneProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("HiddenScene should stay Blueprint-visible through BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(HiddenSceneProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("HiddenScene should be Blueprint read-only")));

		ASSERT_THAT(IsTrue(EditableSceneProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("EditableScene should keep DefaultComponent metadata")));
		ASSERT_THAT(IsFalse(EditableSceneProperty->HasMetaData(TEXT("EditInline")), TEXT("EditableScene should cover EditAnywhere without ShowOnActor EditInline metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Root")), EditableSceneProperty->GetMetaData(TEXT("Attach")), TEXT("EditableScene should keep Attach metadata")));
		ASSERT_THAT(IsTrue(EditableSceneProperty->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere should make EditableScene editable without ShowOnActor")));
		ASSERT_THAT(IsTrue(EditableSceneProperty->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("EditableScene should remain Blueprint-visible by default")));
		ASSERT_THAT(IsTrue(EditableSceneProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("EditableScene DefaultComponent subobject surfaces BlueprintReadOnly without explicit BlueprintReadWrite")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|Editable")), EditableSceneProperty->GetMetaData(TEXT("Category")), TEXT("EditableScene should preserve Category metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("ShowOnActor default component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("VisibleArrowAttached"), true, TEXT("VisibleArrow should attach to Root at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("HiddenSceneAttached"), true, TEXT("HiddenScene should attach to Root at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EditableSceneAttached"), true, TEXT("EditableScene should attach to Root at runtime"))));
		ASSERT_THAT(IsNotNull(FindActorComponentByName(Actor, TEXT("VisibleArrow")), TEXT("VisibleArrow component should be discoverable by name")));
		ASSERT_THAT(IsNotNull(FindActorComponentByName(Actor, TEXT("HiddenScene")), TEXT("HiddenScene component should be discoverable by name")));
		ASSERT_THAT(IsNotNull(FindActorComponentByName(Actor, TEXT("EditableScene")), TEXT("EditableScene component should be discoverable by name")));
		ASSERT_THAT(AreEqual(4, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("ShowOnActor actor should own exactly the declared components")));
	}

	TEST_METHOD(DefaultComponentNativeComponentTypeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassDefaultComponent_NativeComponentTypeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassNativeMatrixLogic : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentNativeTypes : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="MeshSocket")
				UStaticMeshComponent StaticMesh;

				UPROPERTY(DefaultComponent, Attach=StaticMesh)
				USkeletalMeshComponent SkeletalMesh;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCapsuleComponent Capsule;

				UPROPERTY(DefaultComponent, Attach=Capsule)
				UBoxComponent Box;

				UPROPERTY(DefaultComponent, Attach=Root)
				USphereComponent Sphere;

				UPROPERTY(DefaultComponent, Attach=Root)
				USpringArmComponent SpringArm;

				UPROPERTY(DefaultComponent, Attach=SpringArm)
				UCameraComponent Camera;

				UPROPERTY(DefaultComponent, Attach=Root)
				UPointLightComponent PointLight;

				UPROPERTY(DefaultComponent, Attach=Root)
				UArrowComponent Arrow;

				UPROPERTY(DefaultComponent)
				UCoverageUClassNativeMatrixLogic LogicNonScene;

				UPROPERTY()
				bool AllNativeComponentsCreated = false;

				UPROPERTY()
				bool NativeSceneAttachmentsValid = false;

				UPROPERTY()
				bool NativeNonSceneComponentValid = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (Root == nullptr ||
						StaticMesh == nullptr ||
						SkeletalMesh == nullptr ||
						Capsule == nullptr ||
						Box == nullptr ||
						Sphere == nullptr ||
						SpringArm == nullptr ||
						Camera == nullptr ||
						PointLight == nullptr ||
						Arrow == nullptr ||
						LogicNonScene == nullptr)
					{
						return;
					}

					AllNativeComponentsCreated = true;

					NativeSceneAttachmentsValid =
						StaticMesh.GetAttachParent() == Root &&
						StaticMesh.GetAttachSocketName() == n"MeshSocket" &&
						SkeletalMesh.GetAttachParent() == StaticMesh &&
						Capsule.GetAttachParent() == Root &&
						Box.GetAttachParent() == Capsule &&
						Sphere.GetAttachParent() == Root &&
						SpringArm.GetAttachParent() == Root &&
						Camera.GetAttachParent() == SpringArm &&
						PointLight.GetAttachParent() == Root &&
						Arrow.GetAttachParent() == Root;

					NativeNonSceneComponentValid =
						LogicNonScene.GetOwner() == this;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentNativeComponentTypeMatrix.as"), ScriptSource)));

		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentNativeTypes"));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Native default component type matrix actor should be generated")));
		if (ActorClass == nullptr)
		{
			return;
		}

		UClass* LogicNonSceneClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassNativeMatrixLogic"));
		ASSERT_THAT(IsNotNull(LogicNonSceneClass, TEXT("Native matrix logic component class should be generated")));
		if (LogicNonSceneClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* RootProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Root"));
		FObjectPropertyBase* StaticMeshProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("StaticMesh"));
		FObjectPropertyBase* SkeletalMeshProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SkeletalMesh"));
		FObjectPropertyBase* CapsuleProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Capsule"));
		FObjectPropertyBase* BoxProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Box"));
		FObjectPropertyBase* SphereProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Sphere"));
		FObjectPropertyBase* SpringArmProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SpringArm"));
		FObjectPropertyBase* CameraProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Camera"));
		FObjectPropertyBase* PointLightProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("PointLight"));
		FObjectPropertyBase* ArrowProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Arrow"));
		FObjectPropertyBase* LogicNonSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("LogicNonScene"));
		ASSERT_THAT(IsNotNull(RootProperty, TEXT("Root default component property should be reflected")));
		ASSERT_THAT(IsNotNull(StaticMeshProperty, TEXT("StaticMesh default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SkeletalMeshProperty, TEXT("SkeletalMesh default component property should be reflected")));
		ASSERT_THAT(IsNotNull(CapsuleProperty, TEXT("Capsule default component property should be reflected")));
		ASSERT_THAT(IsNotNull(BoxProperty, TEXT("Box default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SphereProperty, TEXT("Sphere default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SpringArmProperty, TEXT("SpringArm default component property should be reflected")));
		ASSERT_THAT(IsNotNull(CameraProperty, TEXT("Camera default component property should be reflected")));
		ASSERT_THAT(IsNotNull(PointLightProperty, TEXT("PointLight default component property should be reflected")));
		ASSERT_THAT(IsNotNull(ArrowProperty, TEXT("Arrow default component property should be reflected")));
		ASSERT_THAT(IsNotNull(LogicNonSceneProperty, TEXT("LogicNonScene default component property should be reflected")));
		if (RootProperty == nullptr || StaticMeshProperty == nullptr || SkeletalMeshProperty == nullptr || CapsuleProperty == nullptr
			|| BoxProperty == nullptr || SphereProperty == nullptr || SpringArmProperty == nullptr || CameraProperty == nullptr
			|| PointLightProperty == nullptr || ArrowProperty == nullptr || LogicNonSceneProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(USceneComponent::StaticClass(), RootProperty->PropertyClass, TEXT("Root should reflect USceneComponent")));
		ASSERT_THAT(AreEqual(UStaticMeshComponent::StaticClass(), StaticMeshProperty->PropertyClass, TEXT("StaticMesh should reflect UStaticMeshComponent")));
		ASSERT_THAT(AreEqual(USkeletalMeshComponent::StaticClass(), SkeletalMeshProperty->PropertyClass, TEXT("SkeletalMesh should reflect USkeletalMeshComponent")));
		ASSERT_THAT(AreEqual(UCapsuleComponent::StaticClass(), CapsuleProperty->PropertyClass, TEXT("Capsule should reflect UCapsuleComponent")));
		ASSERT_THAT(AreEqual(UBoxComponent::StaticClass(), BoxProperty->PropertyClass, TEXT("Box should reflect UBoxComponent")));
		ASSERT_THAT(AreEqual(USphereComponent::StaticClass(), SphereProperty->PropertyClass, TEXT("Sphere should reflect USphereComponent")));
		ASSERT_THAT(AreEqual(USpringArmComponent::StaticClass(), SpringArmProperty->PropertyClass, TEXT("SpringArm should reflect USpringArmComponent")));
		ASSERT_THAT(AreEqual(UCameraComponent::StaticClass(), CameraProperty->PropertyClass, TEXT("Camera should reflect UCameraComponent")));
		ASSERT_THAT(AreEqual(UPointLightComponent::StaticClass(), PointLightProperty->PropertyClass, TEXT("PointLight should reflect UPointLightComponent")));
		ASSERT_THAT(AreEqual(UArrowComponent::StaticClass(), ArrowProperty->PropertyClass, TEXT("Arrow should reflect UArrowComponent")));
		ASSERT_THAT(AreEqual(LogicNonSceneClass, LogicNonSceneProperty->PropertyClass, TEXT("LogicNonScene should reflect the script non-scene component class")));
		ASSERT_THAT(IsTrue(RootProperty->HasMetaData(TEXT("RootComponent")), TEXT("Root should keep RootComponent metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("Root")), StaticMeshProperty->GetMetaData(TEXT("Attach")), TEXT("StaticMesh should keep Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("MeshSocket")), StaticMeshProperty->GetMetaData(TEXT("AttachSocket")), TEXT("StaticMesh should keep AttachSocket metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("StaticMesh")), SkeletalMeshProperty->GetMetaData(TEXT("Attach")), TEXT("SkeletalMesh should keep nested Attach metadata")));
		ASSERT_THAT(IsFalse(LogicNonSceneProperty->HasMetaData(TEXT("Attach")), TEXT("LogicNonScene should cover non-scene default component without attach metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Native default component type matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AllNativeComponentsCreated"), true, TEXT("All native default component types should be created"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NativeSceneAttachmentsValid"), true, TEXT("Native scene default component attachments should materialize"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NativeNonSceneComponentValid"), true, TEXT("Native non-scene default component should be actor-owned"))));
	}

	TEST_METHOD(DefaultComponentInheritanceAndForwardAttachMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassDefaultComponent_InheritanceAndForwardAttachMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentBaseActor : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root, AttachSocket="BaseSocket")
				USceneComponent BaseChild;
			}

			UCLASS()
			class ACoverageUClassDefaultComponentDerivedActor : ACoverageUClassDefaultComponentBaseActor
			{
				UPROPERTY(DefaultComponent)
				USceneComponent FirstImplicitScene;

				UPROPERTY(DefaultComponent)
				USceneComponent ForwardParent;

				UPROPERTY(DefaultComponent, Attach=ForwardParent, AttachSocket="ForwardSocket")
				USceneComponent ForwardChild;

				UPROPERTY(DefaultComponent)
				USceneComponent DerivedChild;

				UPROPERTY()
				bool BaseAttachmentsPreserved = false;

				UPROPERTY()
				bool ImplicitSceneAttached = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BaseAttachmentsPreserved =
						Root != nullptr &&
						BaseChild != nullptr &&
						Root.GetAttachParent() == nullptr &&
						BaseChild.GetAttachParent() == Root &&
						BaseChild.GetAttachSocketName() == n"BaseSocket";

					ImplicitSceneAttached =
						FirstImplicitScene != nullptr &&
						Root != nullptr &&
						FirstImplicitScene.GetAttachParent() == Root;
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentInheritanceAndForwardAttachMatrix.as"), ScriptSource)));

		UClass* BaseActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentBaseActor"));
		UClass* DerivedActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentDerivedActor"));
		ASSERT_THAT(IsNotNull(BaseActorClass, TEXT("Base default component actor should be generated")));
		ASSERT_THAT(IsNotNull(DerivedActorClass, TEXT("Derived default component actor should be generated")));
		if (BaseActorClass == nullptr || DerivedActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* BaseChildProperty = FindFProperty<FObjectPropertyBase>(DerivedActorClass, TEXT("BaseChild"));
		FObjectPropertyBase* FirstImplicitSceneProperty = FindFProperty<FObjectPropertyBase>(DerivedActorClass, TEXT("FirstImplicitScene"));
		FObjectPropertyBase* ForwardChildProperty = FindFProperty<FObjectPropertyBase>(DerivedActorClass, TEXT("ForwardChild"));
		FObjectPropertyBase* DerivedChildProperty = FindFProperty<FObjectPropertyBase>(DerivedActorClass, TEXT("DerivedChild"));
		FObjectPropertyBase* ForwardParentProperty = FindFProperty<FObjectPropertyBase>(DerivedActorClass, TEXT("ForwardParent"));
		ASSERT_THAT(IsNotNull(BaseChildProperty, TEXT("Inherited BaseChild property should be discoverable from derived class")));
		ASSERT_THAT(IsNotNull(FirstImplicitSceneProperty, TEXT("FirstImplicitScene property should be reflected")));
		ASSERT_THAT(IsNotNull(ForwardChildProperty, TEXT("ForwardChild property should be reflected")));
		ASSERT_THAT(IsNotNull(DerivedChildProperty, TEXT("DerivedChild property should be reflected")));
		ASSERT_THAT(IsNotNull(ForwardParentProperty, TEXT("ForwardParent property should be reflected")));
		if (BaseChildProperty == nullptr || FirstImplicitSceneProperty == nullptr || ForwardChildProperty == nullptr || DerivedChildProperty == nullptr || ForwardParentProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Root")), BaseChildProperty->GetMetaData(TEXT("Attach")), TEXT("Inherited BaseChild should keep base Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("BaseSocket")), BaseChildProperty->GetMetaData(TEXT("AttachSocket")), TEXT("Inherited BaseChild should keep base AttachSocket metadata")));
		ASSERT_THAT(IsFalse(FirstImplicitSceneProperty->HasMetaData(TEXT("RootComponent")), TEXT("FirstImplicitScene should not become a second explicit root")));
		ASSERT_THAT(IsFalse(FirstImplicitSceneProperty->HasMetaData(TEXT("Attach")), TEXT("FirstImplicitScene should cover implicit attachment without metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("ForwardParent")), ForwardChildProperty->GetMetaData(TEXT("Attach")), TEXT("ForwardChild should preserve forward Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("ForwardSocket")), ForwardChildProperty->GetMetaData(TEXT("AttachSocket")), TEXT("ForwardChild should preserve AttachSocket metadata")));
		ASSERT_THAT(IsFalse(DerivedChildProperty->HasMetaData(TEXT("Attach")), TEXT("DerivedChild should cover implicit attachment to inherited root")));
		ASSERT_THAT(IsFalse(ForwardParentProperty->HasMetaData(TEXT("Attach")), TEXT("ForwardParent should cover implicit attachment to inherited root")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, DerivedActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Derived default component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		UObject* RootObject = nullptr;
		UObject* FirstImplicitSceneObject = nullptr;
		UObject* ForwardParentObject = nullptr;
		UObject* ForwardChildObject = nullptr;
		UObject* DerivedChildObject = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Root"), RootObject), TEXT("Root property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("FirstImplicitScene"), FirstImplicitSceneObject), TEXT("FirstImplicitScene property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ForwardParent"), ForwardParentObject), TEXT("ForwardParent property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("ForwardChild"), ForwardChildObject), TEXT("ForwardChild property should be readable")));
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("DerivedChild"), DerivedChildObject), TEXT("DerivedChild property should be readable")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BaseAttachmentsPreserved"), true, TEXT("Inherited default component attachments should be preserved"))));
		ASSERT_THAT(IsNull(FirstImplicitSceneObject, TEXT("Derived implicit component script property currently remains null under script-parent inheritance")));
		ASSERT_THAT(IsNull(ForwardParentObject, TEXT("Derived forward-parent component script property currently remains null under script-parent inheritance")));
		ASSERT_THAT(IsNull(ForwardChildObject, TEXT("Derived forward-child component script property currently remains null under script-parent inheritance")));
		ASSERT_THAT(IsNull(DerivedChildObject, TEXT("Derived child component script property currently remains null under script-parent inheritance")));

		USceneComponent* FirstImplicitSceneComponent = Cast<USceneComponent>(FindActorComponentByName(Actor, TEXT("FirstImplicitScene")));
		USceneComponent* ForwardParentComponent = Cast<USceneComponent>(FindActorComponentByName(Actor, TEXT("ForwardParent")));
		USceneComponent* ForwardChildComponent = Cast<USceneComponent>(FindActorComponentByName(Actor, TEXT("ForwardChild")));
		USceneComponent* DerivedChildComponent = Cast<USceneComponent>(FindActorComponentByName(Actor, TEXT("DerivedChild")));
		ASSERT_THAT(IsNotNull(FirstImplicitSceneComponent, TEXT("Derived implicit scene component should materialize by object name")));
		ASSERT_THAT(IsNotNull(ForwardParentComponent, TEXT("Derived forward-parent component should materialize by object name")));
		ASSERT_THAT(IsNotNull(ForwardChildComponent, TEXT("Derived forward-child component should materialize by object name")));
		ASSERT_THAT(IsNotNull(DerivedChildComponent, TEXT("Derived child component should materialize by object name")));
		if (FirstImplicitSceneComponent == nullptr || ForwardParentComponent == nullptr || ForwardChildComponent == nullptr || DerivedChildComponent == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(Actor->GetRootComponent(), FirstImplicitSceneComponent->GetAttachParent(), TEXT("Derived implicit scene component should attach to inherited root in native component state")));
		ASSERT_THAT(AreEqual(Actor->GetRootComponent(), ForwardParentComponent->GetAttachParent(), TEXT("Derived forward parent should attach to inherited root in native component state")));
		ASSERT_THAT(AreEqual(ForwardParentComponent, ForwardChildComponent->GetAttachParent(), TEXT("Derived forward child should attach to the forward parent in native component state")));
		ASSERT_THAT(AreEqual(FName(TEXT("ForwardSocket")), ForwardChildComponent->GetAttachSocketName(), TEXT("Derived forward child should keep the forward attach socket in native component state")));
		ASSERT_THAT(AreEqual(Actor->GetRootComponent(), DerivedChildComponent->GetAttachParent(), TEXT("Derived child should attach to inherited root in native component state")));
		ASSERT_THAT(AreEqual(6, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Derived actor should own inherited and declared default components")));
	}

	TEST_METHOD(DefaultComponentImplicitRootAndDelayedAttachMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUClassDefaultComponent_ImplicitRootAndDelayedAttachMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDefaultComponentImplicitLogic : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentImplicitRoot : AActor
			{
				UPROPERTY(DefaultComponent)
				USceneComponent FirstScene;

				UPROPERTY(DefaultComponent)
				UCoverageUClassDefaultComponentImplicitLogic Logic;

				UPROPERTY(DefaultComponent, Attach=SecondScene, AttachSocket="DelayedSocket")
				USceneComponent DelayedChild;

				UPROPERTY(DefaultComponent)
				USceneComponent SecondScene;

				UPROPERTY()
				bool FirstSceneBecameRoot = false;

				UPROPERTY()
				bool SecondSceneAttachedToRoot = false;

				UPROPERTY()
				bool LogicHasNoSceneAttachment = false;

				UPROPERTY()
				bool DelayedAttachResolved = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FirstSceneBecameRoot =
						FirstScene != nullptr &&
						FirstScene.GetAttachParent() == nullptr;

					SecondSceneAttachedToRoot =
						SecondScene != nullptr &&
						FirstScene != nullptr &&
						SecondScene.GetAttachParent() == FirstScene;

					LogicHasNoSceneAttachment =
						Logic != nullptr &&
						Logic.GetOwner() == this;

					DelayedAttachResolved =
						DelayedChild != nullptr &&
						SecondScene != nullptr &&
						DelayedChild.GetAttachParent() == SecondScene &&
						DelayedChild.GetAttachSocketName() == n"DelayedSocket";
				}
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixture(*TestRunner, Engine, ModuleName, TEXT("ASCoverageUClassDefaultComponentImplicitRootAndDelayedAttachMatrix.as"), ScriptSource)));

		UClass* LogicComponentClass = FindGeneratedClass(&Engine, TEXT("UCoverageUClassDefaultComponentImplicitLogic"));
		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("ACoverageUClassDefaultComponentImplicitRoot"));
		ASSERT_THAT(IsNotNull(LogicComponentClass, TEXT("Implicit-root logic component class should be generated")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Implicit-root actor should be generated")));
		if (LogicComponentClass == nullptr || ActorClass == nullptr)
		{
			return;
		}

		FObjectPropertyBase* FirstSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("FirstScene"));
		FObjectPropertyBase* SecondSceneProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("SecondScene"));
		FObjectPropertyBase* LogicProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("Logic"));
		FObjectPropertyBase* DelayedChildProperty = FindFProperty<FObjectPropertyBase>(ActorClass, TEXT("DelayedChild"));
		ASSERT_THAT(IsNotNull(FirstSceneProperty, TEXT("FirstScene default component property should be reflected")));
		ASSERT_THAT(IsNotNull(SecondSceneProperty, TEXT("SecondScene default component property should be reflected")));
		ASSERT_THAT(IsNotNull(LogicProperty, TEXT("Logic default component property should be reflected")));
		ASSERT_THAT(IsNotNull(DelayedChildProperty, TEXT("DelayedChild default component property should be reflected")));
		if (FirstSceneProperty == nullptr || SecondSceneProperty == nullptr || LogicProperty == nullptr || DelayedChildProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(FirstSceneProperty->HasMetaData(TEXT("DefaultComponent")), TEXT("FirstScene should keep DefaultComponent metadata")));
		ASSERT_THAT(IsFalse(FirstSceneProperty->HasMetaData(TEXT("RootComponent")), TEXT("Implicit root should not need RootComponent metadata")));
		ASSERT_THAT(IsFalse(SecondSceneProperty->HasMetaData(TEXT("Attach")), TEXT("SecondScene should cover implicit attachment without Attach metadata")));
		ASSERT_THAT(AreEqual(LogicComponentClass, LogicProperty->PropertyClass, TEXT("Logic should preserve script actor component class")));
		ASSERT_THAT(IsFalse(LogicProperty->HasMetaData(TEXT("Attach")), TEXT("Logic should cover non-scene implicit default component without Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("SecondScene")), DelayedChildProperty->GetMetaData(TEXT("Attach")), TEXT("DelayedChild should preserve forward Attach metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("DelayedSocket")), DelayedChildProperty->GetMetaData(TEXT("AttachSocket")), TEXT("DelayedChild should preserve AttachSocket metadata")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Implicit-root default component actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FirstSceneBecameRoot"), true, TEXT("First scene default component should become the implicit root"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SecondSceneAttachedToRoot"), true, TEXT("Second scene default component should attach to the implicit root"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("LogicHasNoSceneAttachment"), true, TEXT("Non-scene default component should still be actor-owned"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("DelayedAttachResolved"), true, TEXT("Delayed attach target should resolve with socket metadata"))));

		UObject* FirstSceneObject = nullptr;
		ASSERT_THAT(IsTrue(ReadObjectByPath(*TestRunner, Actor, TEXT("FirstScene"), FirstSceneObject, TEXT("FirstScene property should be readable"))));
		ASSERT_THAT(AreEqual(FirstSceneObject, static_cast<UObject*>(Actor->GetRootComponent()), TEXT("First scene default component should be the actor root")));
		ASSERT_THAT(AreEqual(4, CountActorComponentsByClass(Actor, UActorComponent::StaticClass()), TEXT("Implicit-root actor should own exactly the declared default components")));
	}

	TEST_METHOD(DefaultComponentInvalidSpecifierBoundaryMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> AttachWithoutDefaultDiagnostics;
		AttachWithoutDefaultDiagnostics.Add(TEXT("Attachments can only be specified on DefaultComponents"));

		const FString AttachWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentAttachWithoutDefault : AActor
			{
				UPROPERTY(Attach=Root)
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_AttachWithoutDefault"),
			TEXT("ASCoverageUClassDefaultComponentAttachWithoutDefault.as"),
			AttachWithoutDefaultSource,
			MakeArrayView(AttachWithoutDefaultDiagnostics))));

		TArray<FString> AttachSocketWithoutDefaultDiagnostics;
		AttachSocketWithoutDefaultDiagnostics.Add(TEXT("Attachments can only be specified on DefaultComponents"));

		const FString AttachSocketWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentAttachSocketWithoutDefault : AActor
			{
				UPROPERTY(AttachSocket="LooseSocket")
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_AttachSocketWithoutDefault"),
			TEXT("ASCoverageUClassDefaultComponentAttachSocketWithoutDefault.as"),
			AttachSocketWithoutDefaultSource,
			MakeArrayView(AttachSocketWithoutDefaultDiagnostics))));

		TArray<FString> RootWithoutDefaultDiagnostics;
		RootWithoutDefaultDiagnostics.Add(TEXT("RootComponent can only be specified on DefaultComponents"));

		const FString RootWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentRootWithoutDefault : AActor
			{
				UPROPERTY(RootComponent)
				USceneComponent Root;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_RootWithoutDefault"),
			TEXT("ASCoverageUClassDefaultComponentRootWithoutDefault.as"),
			RootWithoutDefaultSource,
			MakeArrayView(RootWithoutDefaultDiagnostics))));

		TArray<FString> ShowOnActorWithoutDefaultDiagnostics;
		ShowOnActorWithoutDefaultDiagnostics.Add(TEXT("ShowOnActor can only be used on default components in actors"));

		const FString ShowOnActorWithoutDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentShowOnActorWithoutDefault : AActor
			{
				UPROPERTY(ShowOnActor)
				USceneComponent VisibleChild;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_ShowOnActorWithoutDefault"),
			TEXT("ASCoverageUClassDefaultComponentShowOnActorWithoutDefault.as"),
			ShowOnActorWithoutDefaultSource,
			MakeArrayView(ShowOnActorWithoutDefaultDiagnostics))));

		TArray<FString> NonComponentDefaultDiagnostics;
		NonComponentDefaultDiagnostics.Add(TEXT("does not derive from UActorComponent"));

		const FString NonComponentDefaultSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDefaultComponentPlainObject : UObject
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentNonComponent : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUClassDefaultComponentPlainObject PlainObject;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_NonComponentDefault"),
			TEXT("ASCoverageUClassDefaultComponentNonComponent.as"),
			NonComponentDefaultSource,
			MakeArrayView(NonComponentDefaultDiagnostics))));

		TArray<FString> NonSceneRootDiagnostics;
		NonSceneRootDiagnostics.Add(TEXT("has RootComponent set, but is not a type of scene component"));

		const FString NonSceneRootSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDefaultComponentLogicRoot : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentNonSceneRoot : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				UCoverageUClassDefaultComponentLogicRoot LogicRoot;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_NonSceneRoot"),
			TEXT("ASCoverageUClassDefaultComponentNonSceneRoot.as"),
			NonSceneRootSource,
			MakeArrayView(NonSceneRootDiagnostics))));

		TArray<FString> NonSceneAttachDiagnostics;
		NonSceneAttachDiagnostics.Add(TEXT("has a component attach set, but is not a type of scene component"));

		const FString NonSceneAttachSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDefaultComponentLogicAttach : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentNonSceneAttach : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=Root)
				UCoverageUClassDefaultComponentLogicAttach Logic;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_NonSceneAttach"),
			TEXT("ASCoverageUClassDefaultComponentNonSceneAttach.as"),
			NonSceneAttachSource,
			MakeArrayView(NonSceneAttachDiagnostics))));

		TArray<FString> AttachToNonSceneParentDiagnostics;
		AttachToNonSceneParentDiagnostics.Add(TEXT("Attach parent Logic is not a SceneComponent for DefaultComponent Child"));

		const FString AttachToNonSceneParentSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageUClassDefaultComponentAttachParentLogic : UActorComponent
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentAttachToNonSceneParent : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUClassDefaultComponentAttachParentLogic Logic;

				UPROPERTY(DefaultComponent, Attach=Logic)
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_AttachToNonSceneParent"),
			TEXT("ASCoverageUClassDefaultComponentAttachToNonSceneParent.as"),
			AttachToNonSceneParentSource,
			MakeArrayView(AttachToNonSceneParentDiagnostics))));

		TArray<FString> MissingAttachParentDiagnostics;
		MissingAttachParentDiagnostics.Add(TEXT("Attach parent MissingParent does not exist for DefaultComponent Child"));

		const FString MissingAttachParentSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentMissingAttachParent : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, Attach=MissingParent)
				USceneComponent Child;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_MissingAttachParent"),
			TEXT("ASCoverageUClassDefaultComponentMissingAttachParent.as"),
			MissingAttachParentSource,
			MakeArrayView(MissingAttachParentDiagnostics))));

		TArray<FString> DuplicateRootDiagnostics;
		DuplicateRootDiagnostics.Add(TEXT("is RootComponent, but the actor already has root component Root"));

		const FString DuplicateRootSource = ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageUClassDefaultComponentDuplicateRoot : AActor
			{
				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent Root;

				UPROPERTY(DefaultComponent, RootComponent)
				USceneComponent OtherRoot;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_DuplicateRoot"),
			TEXT("ASCoverageUClassDefaultComponentDuplicateRoot.as"),
			DuplicateRootSource,
			MakeArrayView(DuplicateRootDiagnostics))));

		TArray<FString> AbstractDefaultComponentDiagnostics;
		AbstractDefaultComponentDiagnostics.Add(TEXT("was marked as DefaultComponent, but the component class is abstract and cannot be added"));

		const FString AbstractDefaultComponentSource = ASTEST_AS(R"AS(
			UCLASS(Abstract)
			class UCoverageUClassDefaultComponentAbstractScene : USceneComponent
			{
			}

			UCLASS()
			class ACoverageUClassDefaultComponentAbstractDefault : AActor
			{
				UPROPERTY(DefaultComponent)
				UCoverageUClassDefaultComponentAbstractScene AbstractScene;
			}
			)AS");

		ASSERT_THAT(IsTrue(CompileFixtureShouldFail(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUClassDefaultComponent_AbstractDefault"),
			TEXT("ASCoverageUClassDefaultComponentAbstractDefault.as"),
			AbstractDefaultComponentSource,
			MakeArrayView(AbstractDefaultComponentDiagnostics))));
	}
};

#endif
