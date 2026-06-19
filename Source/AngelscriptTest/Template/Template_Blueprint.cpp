#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace TemplateBlueprintTest
{
	UBlueprint* CreateTransientBlueprintChild(
		FAutomationTestBase& Test,
		UClass* ParentClass,
		FStringView Suffix,
		const TCHAR* CallingContext = TEXT("Template_Blueprint"))
	{
		if (!Test.TestNotNull(TEXT("Blueprint parent class should be valid"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptTemplate_%.*s_%s"),
			Suffix.Len(),
			Suffix.GetData(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("Transient blueprint package should be created"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			CallingContext);
		if (!Test.TestNotNull(TEXT("Transient blueprint asset should be created"), Blueprint))
		{
			return nullptr;
		}

		return Blueprint;
	}

	bool CompileAndValidateBlueprint(FAutomationTestBase& Test, UBlueprint& Blueprint)
	{
		FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		return Test.TestNotNull(TEXT("Blueprint should compile to a generated class"), Blueprint.GeneratedClass.Get());
	}

	void CleanupBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;

		~FScopedTransientBlueprint()
		{
			CleanupBlueprint(Blueprint);
		}

		bool CreateAndCompile(
			FAutomationTestBase& Test,
			UClass* ParentClass,
			FStringView Suffix,
			const TCHAR* CallingContext = TEXT("Template_Blueprint"))
		{
			Blueprint = CreateTransientBlueprintChild(Test, ParentClass, Suffix, CallingContext);
			return Blueprint != nullptr && CompileAndValidateBlueprint(Test, *Blueprint);
		}

		UClass* GetGeneratedClass() const
		{
			return Blueprint != nullptr ? Blueprint->GeneratedClass.Get() : nullptr;
		}
	};
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTemplateBlueprintTest,
	"Angelscript.Template.Blueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptParentChild)
	{
		FAngelscriptEngine& Engine = AcquireCleanSharedCloneEngine();
		static const FName ModuleName(TEXT("TemplateBlueprintScriptParent"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("TemplateBlueprintScriptParent.as"),
			TEXT(R"AS(
UCLASS()
class ATemplateBlueprintScriptParent : AActor
{
	UPROPERTY()
	int BlueprintTemplateMarker = 7;
}
)AS"),
			TEXT("ATemplateBlueprintScriptParent"));
		ASSERT_THAT(IsNotNull(ScriptClass));

		TemplateBlueprintTest::FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ScriptClass, TEXT("ScriptParentChild"))));

		UClass* GeneratedBlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(GeneratedBlueprintClass, TEXT("Blueprint template should expose a generated class")));

		ASSERT_THAT(IsTrue(GeneratedBlueprintClass->IsChildOf(ScriptClass),
			TEXT("Blueprint template should create a child blueprint from the script class")));
	}
};

#endif
