#include "CQTest.h"
#include "Shared/AngelscriptBindingsAssertions.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

namespace
{
	static constexpr TCHAR FixtureModuleName[] = TEXT("ASUserWidget_Fixture");
	static constexpr TCHAR FixtureWidgetName[] = TEXT("BindingUserWidget");
	static constexpr TCHAR MissingTreeWidgetName[] = TEXT("BindingUserWidgetMissingTree");
	static constexpr TCHAR RuntimeRootWidgetName[] = TEXT("RuntimeText");
	static constexpr TCHAR RuntimeWidgetClassName[] = TEXT("UBindingUserWidgetCompat");
	static constexpr TCHAR DetachedTextBlockName[] = TEXT("DetachedTextBlock");


	struct FUserWidgetFixture
	{
		UUserWidget* Widget = nullptr;
		UWidgetTree* WidgetTree = nullptr;
		FString WidgetPath;
	};

	struct FScopedRootedObject
	{
		explicit FScopedRootedObject(UObject* InObject)
			: Object(InObject)
		{
			if (Object != nullptr)
			{
				Object->AddToRoot();
			}
		}

		~FScopedRootedObject()
		{
			if (Object != nullptr)
			{
				if (Object->IsRooted())
				{
					Object->RemoveFromRoot();
				}
				Object->MarkAsGarbage();
			}
		}

		FScopedRootedObject(const FScopedRootedObject&) = delete;
		FScopedRootedObject& operator=(const FScopedRootedObject&) = delete;

	private:
		UObject* Object = nullptr;
	};

	FString EscapeScriptString(const FString& Value)
	{
		return Value.ReplaceCharWithEscapedChar();
	}

	void ReplaceToken(FString& Source, const TCHAR* Token, const FString& Replacement)
	{
		Source.ReplaceInline(Token, *Replacement, ESearchCase::CaseSensitive);
	}

	UClass* CreateFixtureWidgetClass(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		const bool bCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			FName(FixtureModuleName),
			FString(FixtureModuleName) + TEXT(".as"),
			FString::Printf(TEXT(R"AS(
UCLASS()
class %s : UUserWidget
{
}
)AS"), RuntimeWidgetClassName));
		if (!Test.TestTrue(TEXT("UserWidget fixture class should compile"), bCompiled))
		{
			return nullptr;
		}

		UClass* WidgetClass = FindGeneratedClass(&Engine, FName(RuntimeWidgetClassName));
		Test.TestNotNull(TEXT("UserWidget fixture class should be published"), WidgetClass);
		return WidgetClass;
	}

	FUserWidgetFixture CreateWidgetFixture(FAutomationTestBase& Test, UClass* WidgetClass, const TCHAR* WidgetName, bool bCreateTree)
	{
		FUserWidgetFixture Fixture;
		Fixture.Widget = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass, WidgetName, RF_Transient);
		if (!Test.TestNotNull(TEXT("UserWidget fixture should be created"), Fixture.Widget))
		{
			return Fixture;
		}

		Fixture.WidgetPath = Fixture.Widget->GetPathName();
		if (!bCreateTree)
		{
			return Fixture;
		}

		Fixture.WidgetTree = NewObject<UWidgetTree>(Fixture.Widget, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transient);
		if (!Test.TestNotNull(TEXT("UserWidget fixture should create a WidgetTree"), Fixture.WidgetTree))
		{
			return Fixture;
		}

		Fixture.Widget->WidgetTree = Fixture.WidgetTree;
		return Fixture;
	}

	int32 CountWidgets(const FUserWidgetFixture& Fixture, bool& bContainsRuntimeRoot)
	{
		bContainsRuntimeRoot = false;
		if (Fixture.WidgetTree == nullptr)
		{
			return 0;
		}

		TArray<UWidget*> Widgets;
		Fixture.WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			bContainsRuntimeRoot |= Widget != nullptr && Widget->GetFName() == FName(RuntimeRootWidgetName);
		}
		return Widgets.Num();
	}

	bool VerifyEmptyTreeState(FAutomationTestBase& Test, const FUserWidgetFixture& Fixture, const TCHAR* Context)
	{
		bool bContainsRuntimeRoot = false;
		const int32 WidgetCount = CountWidgets(Fixture, bContainsRuntimeRoot);
		bool bPassed = true;
		bPassed &= Test.TestNull(*FString::Printf(TEXT("%s should have no root widget"), Context), Fixture.Widget ? Fixture.Widget->GetRootWidget() : nullptr);
		bPassed &= Test.TestNull(*FString::Printf(TEXT("%s should have no WidgetTree root"), Context), Fixture.WidgetTree ? Fixture.WidgetTree->RootWidget : nullptr);
		bPassed &= Test.TestEqual(*FString::Printf(TEXT("%s should have no tree widgets"), Context), WidgetCount, 0);
		bPassed &= Test.TestFalse(*FString::Printf(TEXT("%s should not contain the runtime root name"), Context), bContainsRuntimeRoot);
		return bPassed;
	}

	FString BuildBasicScript(const FUserWidgetFixture& Fixture)
	{
		FString Script = TEXT(R"AS(
UUserWidget GetFixture() { return Cast<UUserWidget>(FindObject("__WIDGET_PATH__")); }
UWidget MakeRoot(UUserWidget Widget) { return Widget.ConstructWidget(UTextBlock::StaticClass(), n"__ROOT_NAME__"); }
int FixtureResolves() { return GetFixture() != null ? 1 : 0; }
int InitialRootIsNull() { UUserWidget Widget = GetFixture(); return Widget != null && Widget.GetRootWidget() == null ? 1 : 0; }
int ConstructTextBlockSucceeds() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); if (Root == null) return 0; Widget.SetRootWidget(Root); Widget.RemoveWidget(Root); return 1; }
int ConstructedWidgetCastsToTextBlock() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); bool bOk = Cast<UTextBlock>(Root) != null; Widget.SetRootWidget(Root); Widget.RemoveWidget(Root); return bOk ? 1 : 0; }
int ConstructedWidgetKeepsName() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); bool bOk = Root != null && Root.GetName() == n"__ROOT_NAME__"; Widget.SetRootWidget(Root); Widget.RemoveWidget(Root); return bOk ? 1 : 0; }
int SetRootRoundTrips() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); Widget.SetRootWidget(Root); bool bOk = Widget.GetRootWidget() == Root; Widget.RemoveWidget(Root); return bOk ? 1 : 0; }
int GetAllWidgetsCountAfterSet() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); Widget.SetRootWidget(Root); TArray<UWidget> Widgets; Widget.GetAllWidgets(Widgets); bool bOk = Widgets.Num() == 1; Widget.RemoveWidget(Root); return bOk ? 1 : 0; }
int GetAllWidgetsIdentityAfterSet() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); Widget.SetRootWidget(Root); TArray<UWidget> Widgets; Widget.GetAllWidgets(Widgets); bool bOk = Widgets.Num() == 1 && Widgets[0] == Root; Widget.RemoveWidget(Root); return bOk ? 1 : 0; }
int RemoveRootReturnsTrue() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); Widget.SetRootWidget(Root); return Widget.RemoveWidget(Root) ? 1 : 0; }
int RootClearsAfterRemove() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); Widget.SetRootWidget(Root); Widget.RemoveWidget(Root); return Widget.GetRootWidget() == null ? 1 : 0; }
int WidgetsClearAfterRemove() { UUserWidget Widget = GetFixture(); UWidget Root = MakeRoot(Widget); Widget.SetRootWidget(Root); Widget.RemoveWidget(Root); TArray<UWidget> Widgets; Widget.GetAllWidgets(Widgets); return Widgets.Num() == 0 ? 1 : 0; }
)AS");
		ReplaceToken(Script, TEXT("__WIDGET_PATH__"), EscapeScriptString(Fixture.WidgetPath));
		ReplaceToken(Script, TEXT("__ROOT_NAME__"), FString(RuntimeRootWidgetName));
		return Script;
	}

	FString BuildErrorScript(const FUserWidgetFixture& WithTree, const FUserWidgetFixture& WithoutTree, const UTextBlock& DetachedTextBlock)
	{
		FString Script = TEXT(R"AS(
UUserWidget WithTreeWidget() { return Cast<UUserWidget>(FindObject("__WITH_TREE_PATH__")); }
UUserWidget WithoutTreeWidget() { return Cast<UUserWidget>(FindObject("__WITHOUT_TREE_PATH__")); }
UTextBlock DetachedTextBlock() { return Cast<UTextBlock>(FindObject("__TEXT_BLOCK_PATH__")); }
int FixturesResolve() { return WithTreeWidget() != null && WithoutTreeWidget() != null && DetachedTextBlock() != null ? 1 : 0; }
int WithTreeRootStartsNull() { UUserWidget Widget = WithTreeWidget(); return Widget != null && Widget.GetRootWidget() == null ? 1 : 0; }
int InvalidClassConstructReturnsNullAndLeavesRootNull() { UUserWidget Widget = WithTreeWidget(); UWidget InvalidWidget = Widget.ConstructWidget(AActor::StaticClass(), n"BadWidget"); return InvalidWidget == null && Widget.GetRootWidget() == null ? 1 : 0; }
int MissingTreeSetRootNoops() { UUserWidget Widget = WithoutTreeWidget(); UTextBlock TextBlock = DetachedTextBlock(); Widget.SetRootWidget(TextBlock); return Widget.GetRootWidget() == null ? 1 : 0; }
int MissingTreeRemoveReturnsFalse() { UUserWidget Widget = WithoutTreeWidget(); return Widget.RemoveWidget(DetachedTextBlock()) ? 0 : 1; }
)AS");
		ReplaceToken(Script, TEXT("__WITH_TREE_PATH__"), EscapeScriptString(WithTree.WidgetPath));
		ReplaceToken(Script, TEXT("__WITHOUT_TREE_PATH__"), EscapeScriptString(WithoutTree.WidgetPath));
		ReplaceToken(Script, TEXT("__TEXT_BLOCK_PATH__"), EscapeScriptString(DetachedTextBlock.GetPathName()));
		return Script;
	}

	bool RunWidgetTreeBasicSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		UClass* WidgetClass = CreateFixtureWidgetClass(Test, Engine);
		if (WidgetClass == nullptr)
		{
			return false;
		}

		FUserWidgetFixture Fixture = CreateWidgetFixture(Test, WidgetClass, FixtureWidgetName, true);
		if (Fixture.Widget == nullptr || Fixture.WidgetTree == nullptr)
		{
			return false;
		}
		FScopedRootedObject FixtureRoot(Fixture.Widget);

		bool bPassed = VerifyEmptyTreeState(Test, Fixture, TEXT("UserWidgetTreeCompat native baseline"));
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUserWidget_Basic"), BuildBasicScript(Fixture));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FixtureResolves()"), TEXT("FindObject should resolve transient UUserWidget fixture"), 1 },
			{ TEXT("int InitialRootIsNull()"), TEXT("Initial root widget should be null"), 1 },
			{ TEXT("int ConstructTextBlockSucceeds()"), TEXT("ConstructWidget should create a UTextBlock widget"), 1 },
			{ TEXT("int ConstructedWidgetCastsToTextBlock()"), TEXT("Constructed widget should cast to UTextBlock"), 1 },
			{ TEXT("int ConstructedWidgetKeepsName()"), TEXT("Constructed widget should keep requested FName"), 1 },
			{ TEXT("int SetRootRoundTrips()"), TEXT("SetRootWidget should round-trip through GetRootWidget"), 1 },
			{ TEXT("int GetAllWidgetsCountAfterSet()"), TEXT("GetAllWidgets should report one widget after root set"), 1 },
			{ TEXT("int GetAllWidgetsIdentityAfterSet()"), TEXT("GetAllWidgets should return the root widget identity"), 1 },
			{ TEXT("int RemoveRootReturnsTrue()"), TEXT("RemoveWidget should remove the root widget"), 1 },
			{ TEXT("int RootClearsAfterRemove()"), TEXT("GetRootWidget should be null after removal"), 1 },
			{ TEXT("int WidgetsClearAfterRemove()"), TEXT("GetAllWidgets should be empty after removal"), 1 },
		};
		bPassed &= AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
		bPassed &= VerifyEmptyTreeState(Test, Fixture, TEXT("UserWidgetTreeCompat native postcondition"));
		return bPassed;
	}

	bool RunWidgetTreeErrorSection(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		UClass* WidgetClass = CreateFixtureWidgetClass(Test, Engine);
		if (WidgetClass == nullptr)
		{
			return false;
		}

		FUserWidgetFixture WithTree = CreateWidgetFixture(Test, WidgetClass, FixtureWidgetName, true);
		FUserWidgetFixture WithoutTree = CreateWidgetFixture(Test, WidgetClass, MissingTreeWidgetName, false);
		UTextBlock* DetachedTextBlock = NewObject<UTextBlock>(GetTransientPackage(), UTextBlock::StaticClass(), DetachedTextBlockName, RF_Transient);
		if (WithTree.Widget == nullptr || WithTree.WidgetTree == nullptr || WithoutTree.Widget == nullptr || DetachedTextBlock == nullptr)
		{
			return false;
		}
		FScopedRootedObject WithTreeRoot(WithTree.Widget);
		FScopedRootedObject WithoutTreeRoot(WithoutTree.Widget);
		FScopedRootedObject DetachedRoot(DetachedTextBlock);

		bool bPassed = true;
		bPassed &= Test.TestFalse(TEXT("UserWidgetTreeErrorPaths tree-backed fixture path should be non-empty"), WithTree.WidgetPath.IsEmpty());
		bPassed &= Test.TestFalse(TEXT("UserWidgetTreeErrorPaths missing-tree fixture path should be non-empty"), WithoutTree.WidgetPath.IsEmpty());
		bPassed &= Test.TestNull(TEXT("UserWidgetTreeErrorPaths missing-tree fixture should start without WidgetTree"), WithoutTree.Widget->WidgetTree);
		bPassed &= Test.TestNull(TEXT("UserWidgetTreeErrorPaths missing-tree fixture should start without root"), WithoutTree.Widget->GetRootWidget());
		bPassed &= Test.TestNull(TEXT("UserWidgetTreeErrorPaths detached text block should start parentless"), DetachedTextBlock->GetParent());

		Test.AddExpectedErrorPlain(TEXT("Ensure condition failed: WidgetClass && WidgetClass->IsChildOf(UWidget::StaticClass())"), EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("LogOutputDevice:"), EAutomationExpectedErrorFlags::Contains, 0);

		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUserWidget_Error"), BuildErrorScript(WithTree, WithoutTree, *DetachedTextBlock));
		if (!ModuleScope.IsValid())
		{
			return false;
		}

		asIScriptModule& Module = ModuleScope.GetModule();
		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FixturesResolve()"), TEXT("Script should resolve tree, missing-tree and detached fixtures"), 1 },
			{ TEXT("int WithTreeRootStartsNull()"), TEXT("Tree-backed fixture root should start null"), 1 },
			{ TEXT("int InvalidClassConstructReturnsNullAndLeavesRootNull()"), TEXT("Invalid widget class should return null and leave root unset"), 1 },
			{ TEXT("int MissingTreeSetRootNoops()"), TEXT("SetRootWidget without WidgetTree should be a no-op"), 1 },
			{ TEXT("int MissingTreeRemoveReturnsFalse()"), TEXT("RemoveWidget without WidgetTree should return false"), 1 },
		};
		bPassed &= AngelscriptTestBindings::ExpectGlobalInts(Test, Engine, Module,  Cases);
		bPassed &= VerifyEmptyTreeState(Test, WithTree, TEXT("UserWidgetTreeErrorPaths tree-backed postcondition"));
		bPassed &= Test.TestNull(TEXT("UserWidgetTreeErrorPaths missing-tree fixture should keep WidgetTree null"), WithoutTree.Widget->WidgetTree);
		bPassed &= Test.TestNull(TEXT("UserWidgetTreeErrorPaths missing-tree fixture should keep root null"), WithoutTree.Widget->GetRootWidget());
		bPassed &= Test.TestNull(TEXT("UserWidgetTreeErrorPaths detached text block should remain parentless"), DetachedTextBlock->GetParent());
		return bPassed;
	}
}

// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptUserWidgetBindingsTest,
	"Angelscript.TestModule.Bindings.UserWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(UserWidgetTreeCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunWidgetTreeBasicSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(UserWidgetTreeErrorPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunWidgetTreeErrorSection(*TestRunner, Engine);
		}
	}
};

#endif
