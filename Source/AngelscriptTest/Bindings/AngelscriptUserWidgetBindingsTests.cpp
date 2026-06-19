#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


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
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsTrue(bCompiled, TEXT("UserWidget fixture class should compile")))
		{
			return nullptr;
		}

		UClass* WidgetClass = FindGeneratedClass(&Engine, FName(RuntimeWidgetClassName));
		if (!Assert.IsNotNull(WidgetClass, TEXT("UserWidget fixture class should be published")))
		{
			return nullptr;
		}
		return WidgetClass;
	}

	FUserWidgetFixture CreateWidgetFixture(FAutomationTestBase& Test, UClass* WidgetClass, const TCHAR* WidgetName, bool bCreateTree)
	{
		FUserWidgetFixture Fixture;
		Fixture.Widget = NewObject<UUserWidget>(GetTransientPackage(), WidgetClass, WidgetName, RF_Transient);
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsNotNull(Fixture.Widget, TEXT("UserWidget fixture should be created")))
		{
			return Fixture;
		}

		Fixture.WidgetPath = Fixture.Widget->GetPathName();
		if (!bCreateTree)
		{
			return Fixture;
		}

		Fixture.WidgetTree = NewObject<UWidgetTree>(Fixture.Widget, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transient);
		if (!Assert.IsNotNull(Fixture.WidgetTree, TEXT("UserWidget fixture should create a WidgetTree")))
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
		FNoDiscardAsserter Assert(Test);
		bool bPassed = true;
		bPassed &= Assert.IsNull(Fixture.Widget ? Fixture.Widget->GetRootWidget() : nullptr, *FString::Printf(TEXT("%s should have no root widget"), Context));
		bPassed &= Assert.IsNull(Fixture.WidgetTree ? Fixture.WidgetTree->RootWidget : nullptr, *FString::Printf(TEXT("%s should have no WidgetTree root"), Context));
		bPassed &= Assert.AreEqual(0, WidgetCount, *FString::Printf(TEXT("%s should have no tree widgets"), Context));
		bPassed &= Assert.IsFalse(bContainsRuntimeRoot, *FString::Printf(TEXT("%s should not contain the runtime root name"), Context));
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
		bPassed &= ExpectGlobalInts(Test, Engine, Module,  Cases);
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
		FNoDiscardAsserter Assert(Test);
		bPassed &= Assert.IsFalse(WithTree.WidgetPath.IsEmpty(), TEXT("UserWidgetTreeErrorPaths tree-backed fixture path should be non-empty"));
		bPassed &= Assert.IsFalse(WithoutTree.WidgetPath.IsEmpty(), TEXT("UserWidgetTreeErrorPaths missing-tree fixture path should be non-empty"));
		bPassed &= Assert.IsNull(WithoutTree.Widget->WidgetTree, TEXT("UserWidgetTreeErrorPaths missing-tree fixture should start without WidgetTree"));
		bPassed &= Assert.IsNull(WithoutTree.Widget->GetRootWidget(), TEXT("UserWidgetTreeErrorPaths missing-tree fixture should start without root"));
		bPassed &= Assert.IsNull(DetachedTextBlock->GetParent(), TEXT("UserWidgetTreeErrorPaths detached text block should start parentless"));

		Test.AddExpectedErrorPlain(TEXT("Ensure condition failed: WidgetClass && WidgetClass->IsChildOf(UWidget::StaticClass())"), EAutomationExpectedErrorFlags::Contains, 2);
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
		bPassed &= ExpectGlobalInts(Test, Engine, Module,  Cases);
		bPassed &= VerifyEmptyTreeState(Test, WithTree, TEXT("UserWidgetTreeErrorPaths tree-backed postcondition"));
		bPassed &= Assert.IsNull(WithoutTree.Widget->WidgetTree, TEXT("UserWidgetTreeErrorPaths missing-tree fixture should keep WidgetTree null"));
		bPassed &= Assert.IsNull(WithoutTree.Widget->GetRootWidget(), TEXT("UserWidgetTreeErrorPaths missing-tree fixture should keep root null"));
		bPassed &= Assert.IsNull(DetachedTextBlock->GetParent(), TEXT("UserWidgetTreeErrorPaths detached text block should remain parentless"));
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
