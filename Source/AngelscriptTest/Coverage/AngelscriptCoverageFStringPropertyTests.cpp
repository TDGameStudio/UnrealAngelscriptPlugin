#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestUtilities.h"

#include "Components/ActorTestSpawner.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageFStringPropertyTests
// -----------------------------------------------------------------------------
// Coverage for AngelScript string-family *UPROPERTY usage* -- the FProperty
// reflection half of the string matrix. This file covers:
//
//   * UPROPERTY declarations (FString / FName / FText)
//   * Read-back via FProperty reflection
//   * Write round-trip (C++ → FProperty → C++)
//   * Special values (empty string, long string, special characters)
//   * Container properties (TArray, TMap, TSet)
//   * Property specifiers (Edit/Visible/Blueprint + meta)
//
// Test pattern: Pattern D (Actor + FProperty reflection)
//
// String family under test:
//   FString / FName / FText (3 types with different semantics)
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFStringPropertyTest,
	"Angelscript.TestModule.Coverage.FStringProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FProperty* RequireProperty(UClass* ScriptClass, const TCHAR* PropertyName)
	{
		return ScriptClass != nullptr ? ScriptClass->FindPropertyByName(FName(PropertyName)) : nullptr;
	}

	static bool EnsureModuleBuilt(FAutomationTestBase& Test, asIScriptModule* Module, const TCHAR* Message)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsNotNull(Module, Message);
	}

	static bool EnsureGlobalInvokerValid(FAutomationTestBase& Test, FASGlobalFunctionInvoker& Invoker, const TCHAR* Message)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.IsTrue(Invoker.IsValid(), Message);
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

	TEST_METHOD(StringDeclarationContexts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageFStringProperty_DeclarationContexts", ASTEST_AS(R"AS(
		const FString GlobalString = "Global FString";
		const FName GlobalName = n"GlobalName";
		const FText GlobalText;

		int ValidateStringDeclarations()
		{
			FString DeferredString;
			if (DeferredString != "")
				return 10;

			FString DefaultString = "Local FString";
			if (DefaultString != "Local FString")
				return 20;

			const FString ConstString = "Const FString";
			if (ConstString != "Const FString")
				return 30;

			if (GlobalString != "Global FString")
				return 40;

			auto AutoString = "Auto FString";
			if (AutoString != "Auto FString")
				return 50;

			return 0;
		}

		int ValidateNameDeclarations()
		{
			FName DeferredName;
			if (DeferredName != NAME_None)
				return 10;

			FName DefaultName = n"LocalName";
			if (DefaultName != n"LocalName")
				return 20;

			const FName ConstName = n"ConstName";
			if (ConstName != n"ConstName")
				return 30;

			if (GlobalName != n"GlobalName")
				return 40;

			return 0;
		}

		int ValidateTextDeclarations()
		{
			FText DeferredText;
			if (!DeferredText.IsEmpty())
				return 10;

			FText DefaultText = FText::FromString("Local Text");
			if (DefaultText.ToString() != "Local Text")
				return 20;

			const FText ConstText = FText::FromString("Const Text");
			if (ConstText.ToString() != "Const Text")
				return 30;

			if (!GlobalText.IsEmpty())
				return 40;

			return 0;
		}
		)AS"));
		ON_SCOPE_EXIT
		{
			if (Module != nullptr)
			{
				Engine.DiscardModule(UTF8_TO_TCHAR(Module->GetName()));
			}
		};
		if (!EnsureModuleBuilt(*TestRunner, Module, TEXT("String declaration-context module should compile")))
		{
			return;
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateStringDeclarations()"));
			if (!EnsureGlobalInvokerValid(*TestRunner, Invoker, TEXT("ValidateStringDeclarations should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(0, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("local/default/const/global/auto FString declarations should execute")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateNameDeclarations()"));
			if (!EnsureGlobalInvokerValid(*TestRunner, Invoker, TEXT("ValidateNameDeclarations should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(0, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("local/default/const/global FName declarations should execute")));
		}
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ValidateTextDeclarations()"));
			if (!EnsureGlobalInvokerValid(*TestRunner, Invoker, TEXT("ValidateTextDeclarations should resolve and prepare")))
			{
				return;
			}
			ASSERT_THAT(AreEqual(0, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("local/default/const/default-global FText declarations should execute")));
		}
	}

	// -------------------------------------------------------------------------
	// String family declaration defaults: ensure default-initialized string
	// properties are empty and readable via FProperty.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringFamilyDeclarationDefaults)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_Defaults"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyDefaults.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringDefaultsActor : AActor
			{
				UPROPERTY()
				FString StringValue = "Hello";

				UPROPERTY()
				FString EmptyString = "";

				UPROPERTY()
				FString NoDefaultString;

				UPROPERTY()
				FName NameValue = n"MyName";

				UPROPERTY()
				FName EmptyName = n"";

				UPROPERTY()
				FText TextValue;  // Default constructed
			}
			)AS"),
			TEXT("ACoverageFStringDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-defaults actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-defaults actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		// FString with value
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello")), TEXT("FString UPROPERTY with default value"))));

		// FString empty
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EmptyString"), FString(TEXT("")), TEXT("FString UPROPERTY with empty default"))));

		// FString no default (should be empty)
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NoDefaultString"), FString(TEXT("")), TEXT("FString UPROPERTY without default should be empty"))));

		// FName with value
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("MyName")), TEXT("FName UPROPERTY with default value"))));

		// FName empty
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("EmptyName"), FName(NAME_None), TEXT("FName UPROPERTY with empty default"))));

		// FText default constructed (empty)
		FText ReadText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), ReadText)));
		ASSERT_THAT(IsTrue(ReadText.IsEmpty(), TEXT("FText UPROPERTY default should be empty")));
	}

	// -------------------------------------------------------------------------
	// String family write round-trip: SetByPath → read back.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringFamilyWriteRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_WriteRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyWriteRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringWriteActor : AActor
			{
				UPROPERTY()
				FString StringValue;

				UPROPERTY()
				FName NameValue;

				UPROPERTY()
				FText TextValue;
			}
			)AS"),
			TEXT("ACoverageFStringWriteActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-write actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-write actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		// FString write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello World")))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello World")), TEXT("FString write round-trip"))));

		ASSERT_THAT(IsTrue(SetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("")))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("")), TEXT("FString empty write round-trip"))));

		// FName write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("TestName")))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("TestName")), TEXT("FName write round-trip"))));

		// FText write round-trip
		FText TestText = FText::FromString(TEXT("Test Text"));
		ASSERT_THAT(IsTrue(SetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), TestText)));
		FText ReadText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), ReadText)));
		ASSERT_THAT(IsTrue(ReadText.EqualTo(TestText), TEXT("FText write round-trip")));

		ASSERT_THAT(IsTrue(SetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), FText::GetEmpty())));
		FText EmptyText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), EmptyText)));
		ASSERT_THAT(IsTrue(EmptyText.IsEmpty(), TEXT("FText empty write round-trip")));

		const FString EscapedStringValue = FString(TEXT("LineOne\nLineTwo\t\"Quote\"\\Slash"));
		ASSERT_THAT(IsTrue(SetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), EscapedStringValue)));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), EscapedStringValue, TEXT("FString escaped C++ write round-trip"))));

		FString LongStringValue;
		LongStringValue.Reserve(1100);
		for (int32 Index = 0; Index < 1100; ++Index)
		{
			LongStringValue.AppendChar(TCHAR('L'));
		}
		ASSERT_THAT(IsTrue(SetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), LongStringValue)));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), LongStringValue, TEXT("FString long C++ write round-trip"))));

		const FName SpecialNameValue(TEXT("Cpp.Name-With_Punctuation_123"));
		ASSERT_THAT(IsTrue(SetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), SpecialNameValue)));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), SpecialNameValue, TEXT("FName special C++ write round-trip"))));

		ASSERT_THAT(IsTrue(SetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), NAME_None)));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), NAME_None, TEXT("FName NAME_None write round-trip"))));

		const FText SpecialTextValue = FText::FromString(EscapedStringValue);
		ASSERT_THAT(IsTrue(SetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), SpecialTextValue)));
		FText SpecialReadText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), SpecialReadText)));
		ASSERT_THAT(AreEqual(EscapedStringValue, SpecialReadText.ToString(), TEXT("FText escaped C++ write round-trip")));
	}

	TEST_METHOD(StringUFunctionPropertyRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_UFunctionRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyUFunctionRoundTrip.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringUFunctionActor : AActor
			{
				UPROPERTY()
				FString StoredString = "Initial";

				UPROPERTY()
				FName StoredName = n"InitialName";

				UPROPERTY()
				FText StoredText;

				UFUNCTION()
				FString StoreAndReturnString(FString Input)
				{
					StoredString = Input + "_Stored";
					return StoredString;
				}

				UFUNCTION()
				FName StoreAndReturnName(FName Input)
				{
					StoredName = Input;
					return StoredName;
				}

				UFUNCTION()
				FText StoreAndReturnText(FText Input)
				{
					StoredText = Input;
					return StoredText;
				}

				UFUNCTION()
				FString UseDefaultString(FString Input = "DefaultString")
				{
					StoredString = Input;
					return StoredString + "_Returned";
				}

				UFUNCTION()
				FName UseDefaultName(FName Input = n"DefaultName")
				{
					StoredName = Input;
					return StoredName;
				}

				UFUNCTION()
				FText UseExplicitText(FText Input)
				{
					StoredText = Input;
					return StoredText;
				}

				UFUNCTION()
				FString CallDefaultString()
				{
					return UseDefaultString();
				}

				UFUNCTION()
				FName CallDefaultName()
				{
					return UseDefaultName();
				}

				UFUNCTION()
				FText CallDefaultText()
				{
					return UseExplicitText(FText::FromString("Default Text"));
				}
			}
			)AS"),
			TEXT("ACoverageFStringUFunctionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FString UFUNCTION property actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FString UFUNCTION property actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("StoreAndReturnString"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("StoreAndReturnString should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<FString>(FString(TEXT("RuntimeString")));
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("RuntimeString_Stored")), Result, TEXT("UFUNCTION FString return should include AS property write")));
			ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StoredString"), FString(TEXT("RuntimeString_Stored")), TEXT("StoredString should reflect UFUNCTION write"))));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("StoreAndReturnName"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("StoreAndReturnName should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<FName>(FName(TEXT("RuntimeName")));
			const FName Result = Invoker.CallAndReturn<FName>();
			ASSERT_THAT(AreEqual(FName(TEXT("RuntimeName")), Result, TEXT("UFUNCTION FName return should include AS property write")));
			ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("StoredName"), FName(TEXT("RuntimeName")), TEXT("StoredName should reflect UFUNCTION write"))));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("StoreAndReturnText"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("StoreAndReturnText should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			Invoker.AddParam<FText>(FText::FromString(TEXT("Runtime Text")));
			const FText Result = Invoker.CallAndReturn<FText>(FText::GetEmpty());
			ASSERT_THAT(AreEqual(FString(TEXT("Runtime Text")), Result.ToString(), TEXT("UFUNCTION FText return should include AS property write")));

			FText StoredText;
			ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("StoredText"), StoredText)));
			ASSERT_THAT(AreEqual(FString(TEXT("Runtime Text")), StoredText.ToString(), TEXT("StoredText should reflect UFUNCTION write")));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("CallDefaultString"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallDefaultString should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const FString Result = Invoker.CallAndReturn<FString>();
			ASSERT_THAT(AreEqual(FString(TEXT("DefaultString_Returned")), Result, TEXT("UFUNCTION FString default parameter should execute")));
			ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StoredString"), FString(TEXT("DefaultString")), TEXT("StoredString should reflect default parameter write"))));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("CallDefaultName"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallDefaultName should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const FName Result = Invoker.CallAndReturn<FName>();
			ASSERT_THAT(AreEqual(FName(TEXT("DefaultName")), Result, TEXT("UFUNCTION FName default parameter should execute")));
			ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("StoredName"), FName(TEXT("DefaultName")), TEXT("StoredName should reflect default parameter write"))));
		}
		{
			FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("CallDefaultText"));
			ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("CallDefaultText should be invokable")));
			if (!Invoker.IsValid())
			{
				return;
			}
			const FText Result = Invoker.CallAndReturn<FText>(FText::GetEmpty());
			ASSERT_THAT(AreEqual(FString(TEXT("Default Text")), Result.ToString(), TEXT("UFUNCTION FText explicit wrapper should execute")));

			FText StoredText;
			ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("StoredText"), StoredText)));
			ASSERT_THAT(AreEqual(FString(TEXT("Default Text")), StoredText.ToString(), TEXT("StoredText should reflect explicit FText wrapper write")));
		}
	}

	TEST_METHOD(StringPropertyScriptReadWriteApiSurface)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_ScriptApiSurface"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyScriptApiSurface.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringScriptApiSurfaceActor : AActor
			{
				UPROPERTY()
				FString StoredString = "InitialString";

				UPROPERTY()
				FName StoredName = n"InitialName";

				UPROPERTY()
				FText StoredText;

				UFUNCTION()
				int ReadInitialState()
				{
					int Mask = 0;
					if (StoredString == "InitialString")
						Mask |= 1;
					if (StoredName == n"InitialName")
						Mask |= 2;
					if (StoredText.IsEmpty())
						Mask |= 4;
					return Mask;
				}

				UFUNCTION()
				int RewriteAndReadState()
				{
					StoredString = "ScriptString";
					StoredName = n"ScriptName";
					StoredText = FText::FromString("Script Text");

					int Mask = 0;
					if (StoredString == "ScriptString")
						Mask |= 1;
					if (StoredName == n"ScriptName")
						Mask |= 2;
					if (StoredText.ToString() == "Script Text")
						Mask |= 4;
					return Mask;
				}
			}
			)AS"),
			TEXT("ACoverageFStringScriptApiSurfaceActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("FString script API surface actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(ScriptClass->FindPropertyByName(FName(TEXT("StoredString")))), TEXT("script FString UPROPERTY should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(ScriptClass->FindPropertyByName(FName(TEXT("StoredName")))), TEXT("script FName UPROPERTY should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(ScriptClass->FindPropertyByName(FName(TEXT("StoredText")))), TEXT("script FText UPROPERTY should reflect as FTextProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("FString script API surface actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ReadInvoker(*TestRunner, Actor, TEXT("ReadInitialState"));
		ASSERT_THAT(IsTrue(ReadInvoker.IsValid(), TEXT("ReadInitialState should be invokable through reflection")));
		if (!ReadInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(7, ReadInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("AS should read initial string-family UPROPERTY values")));

		FFunctionInvoker RewriteInvoker(*TestRunner, Actor, TEXT("RewriteAndReadState"));
		ASSERT_THAT(IsTrue(RewriteInvoker.IsValid(), TEXT("RewriteAndReadState should be invokable through reflection")));
		if (!RewriteInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(AreEqual(7, RewriteInvoker.CallAndReturn<int32>(INDEX_NONE), TEXT("AS should write and reread string-family UPROPERTY values")));

		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StoredString"), FString(TEXT("ScriptString")), TEXT("script-written FString UPROPERTY should read back in C++"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("StoredName"), FName(TEXT("ScriptName")), TEXT("script-written FName UPROPERTY should read back in C++"))));

		FText StoredText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("StoredText"), StoredText)));
		ASSERT_THAT(AreEqual(FString(TEXT("Script Text")), StoredText.ToString(), TEXT("script-written FText UPROPERTY should read back in C++")));
	}

	TEST_METHOD(StringPropertySpecifierFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertySpecifiers.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringSpecifierActor : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coverage|String")
				FString EditableString;

				UPROPERTY(BlueprintReadOnly)
				FName ReadOnlyName;

				UPROPERTY(AdvancedDisplay)
				FText AdvancedText;

				UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (DisplayName = "Visible String", ToolTip = "Visible string tooltip"))
				FString VisibleString;

				UPROPERTY(Transient)
				FName TransientName;

				UPROPERTY(SaveGame)
				FText SaveGameText;
			}
			)AS"),
			TEXT("ACoverageFStringSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-specifier actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FProperty* EditableString = FindFProperty<FProperty>(ScriptClass, TEXT("EditableString"));
		ASSERT_THAT(IsNotNull(EditableString, TEXT("EditableString property should exist")));
		if (EditableString == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EditableString->HasAnyPropertyFlags(CPF_Edit), TEXT("EditAnywhere FString should set CPF_Edit")));
		ASSERT_THAT(IsTrue(EditableString->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadWrite FString should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsFalse(EditableString->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadWrite FString should not set CPF_BlueprintReadOnly")));
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|String")), EditableString->GetMetaData(TEXT("Category")), TEXT("FString Category metadata should round-trip")));
#endif

		const FProperty* ReadOnlyName = FindFProperty<FProperty>(ScriptClass, TEXT("ReadOnlyName"));
		ASSERT_THAT(IsNotNull(ReadOnlyName, TEXT("ReadOnlyName property should exist")));
		if (ReadOnlyName == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReadOnlyName->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly FName should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(ReadOnlyName->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly FName should set CPF_BlueprintReadOnly")));

		const FProperty* AdvancedText = FindFProperty<FProperty>(ScriptClass, TEXT("AdvancedText"));
		ASSERT_THAT(IsNotNull(AdvancedText, TEXT("AdvancedText property should exist")));
		if (AdvancedText == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AdvancedText->HasAnyPropertyFlags(CPF_AdvancedDisplay), TEXT("AdvancedDisplay FText should set CPF_AdvancedDisplay")));

		const FProperty* VisibleString = RequireProperty(ScriptClass, TEXT("VisibleString"));
		ASSERT_THAT(IsNotNull(VisibleString, TEXT("VisibleString property should exist")));
		if (VisibleString == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(VisibleString->HasAnyPropertyFlags(CPF_Edit), TEXT("VisibleAnywhere FString should set CPF_Edit")));
		ASSERT_THAT(IsTrue(VisibleString->HasAnyPropertyFlags(CPF_EditConst), TEXT("VisibleAnywhere FString should set CPF_EditConst")));
		ASSERT_THAT(IsTrue(VisibleString->HasAnyPropertyFlags(CPF_BlueprintVisible), TEXT("BlueprintReadOnly visible FString should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(VisibleString->HasAnyPropertyFlags(CPF_BlueprintReadOnly), TEXT("BlueprintReadOnly visible FString should set CPF_BlueprintReadOnly")));
#if WITH_EDITOR
		ASSERT_THAT(AreEqual(FString(TEXT("Visible String")), VisibleString->GetMetaData(TEXT("DisplayName")), TEXT("FString DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Visible string tooltip")), VisibleString->GetMetaData(TEXT("ToolTip")), TEXT("FString ToolTip metadata should round-trip")));
#endif

		const FProperty* TransientName = RequireProperty(ScriptClass, TEXT("TransientName"));
		ASSERT_THAT(IsNotNull(TransientName, TEXT("TransientName property should exist")));
		if (TransientName == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(TransientName->HasAnyPropertyFlags(CPF_Transient), TEXT("Transient FName should set CPF_Transient")));

		const FProperty* SaveGameText = RequireProperty(ScriptClass, TEXT("SaveGameText"));
		ASSERT_THAT(IsNotNull(SaveGameText, TEXT("SaveGameText property should exist")));
		if (SaveGameText == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SaveGameText->HasAnyPropertyFlags(CPF_SaveGame), TEXT("SaveGame FText should set CPF_SaveGame")));
	}

	TEST_METHOD(StringFamilyReplicatedProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_Replication"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyReplication.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringReplicationActor : AActor
			{
				default SetReplicates(true);

				UPROPERTY(Replicated)
				FString ReplicatedString = "Initial";

				UPROPERTY(Replicated)
				FName ReplicatedName = n"InitialName";

				UPROPERTY(ReplicatedUsing=OnRep_DisplayText)
				FText DisplayText;

				UFUNCTION()
				void OnRep_DisplayText()
				{
				}
			}
			)AS"),
			TEXT("ACoverageFStringReplicationActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-replication actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FStrProperty* ReplicatedString = CastField<FStrProperty>(RequireProperty(ScriptClass, TEXT("ReplicatedString")));
		const FNameProperty* ReplicatedName = CastField<FNameProperty>(RequireProperty(ScriptClass, TEXT("ReplicatedName")));
		const FTextProperty* DisplayText = CastField<FTextProperty>(RequireProperty(ScriptClass, TEXT("DisplayText")));
		ASSERT_THAT(IsNotNull(ReplicatedString, TEXT("ReplicatedString should be generated as FStrProperty")));
		ASSERT_THAT(IsNotNull(ReplicatedName, TEXT("ReplicatedName should be generated as FNameProperty")));
		ASSERT_THAT(IsNotNull(DisplayText, TEXT("DisplayText should be generated as FTextProperty")));
		if (ReplicatedString == nullptr || ReplicatedName == nullptr || DisplayText == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReplicatedString->HasAnyPropertyFlags(CPF_Net), TEXT("Replicated FString should carry CPF_Net")));
		ASSERT_THAT(IsTrue(ReplicatedName->HasAnyPropertyFlags(CPF_Net), TEXT("Replicated FName should carry CPF_Net")));
		ASSERT_THAT(IsTrue(DisplayText->HasAnyPropertyFlags(CPF_Net), TEXT("ReplicatedUsing FText should carry CPF_Net")));
		ASSERT_THAT(IsTrue(DisplayText->HasAnyPropertyFlags(CPF_RepNotify), TEXT("ReplicatedUsing FText should carry CPF_RepNotify")));
		ASSERT_THAT(AreEqual(FName(TEXT("OnRep_DisplayText")), DisplayText->RepNotifyFunc, TEXT("ReplicatedUsing FText should preserve RepNotify function")));
		ASSERT_THAT(IsNotNull(FindGeneratedFunction(ScriptClass, TEXT("OnRep_DisplayText")), TEXT("FText RepNotify callback should be generated")));
	}

	// -------------------------------------------------------------------------
	// String special values: empty, long, special characters, Unicode.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringSpecialValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_Special"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertySpecial.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringSpecialActor : AActor
			{
				UPROPERTY()
				FString EmptyString;

				UPROPERTY()
				FString LongString;

				UPROPERTY()
				FString SpecialChars;

				UPROPERTY()
				FString UnicodeString;

				UPROPERTY()
				FName EmptyName;

				UPROPERTY()
				FName SpecialName;

				UPROPERTY()
				FText EmptyText;

				UPROPERTY()
				FText UnicodeText;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					EmptyString = "";

					for (int i = 0; i < 1024; ++i)
					{
						LongString += "x";
					}

					SpecialChars = "Hello\nWorld\tTab\"Quote\"\\Backslash";

					UnicodeString = "Hello 世界 🌍";
					EmptyName = NAME_None;
					SpecialName = FName("Name.With.Dots-123");
					EmptyText = FText::FromString("");
					UnicodeText = FText::FromString("Text 世界");
				}
			}
			)AS"),
			TEXT("ACoverageFStringSpecialActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-special actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-special actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Empty string
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EmptyString"), FString(TEXT("")), TEXT("Empty string"))));

		// Long string
		FString ExpectedLong;
		ExpectedLong.Reserve(1024);
		for (int32 Index = 0; Index < 1024; ++Index)
		{
			ExpectedLong.AppendChar(TCHAR('x'));
		}
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LongString"), ExpectedLong, TEXT("Long string (1000+ chars)"))));

		// Special characters
		FString ExpectedSpecial = TEXT("Hello\nWorld\tTab\"Quote\"\\Backslash");
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("SpecialChars"), ExpectedSpecial, TEXT("Special characters (newline/tab/quote/backslash)"))));

		// Unicode
		FString ExpectedUnicode = TEXT("Hello 世界 🌍");
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("UnicodeString"), ExpectedUnicode, TEXT("Unicode string"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("EmptyName"), NAME_None, TEXT("FName empty/None value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("SpecialName"), FName(TEXT("Name.With.Dots-123")), TEXT("FName special identifier characters"))));

		FText EmptyText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("EmptyText"), EmptyText)));
		ASSERT_THAT(IsTrue(EmptyText.IsEmpty(), TEXT("FText empty value should round-trip")));

		FText UnicodeText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("UnicodeText"), UnicodeText)));
		ASSERT_THAT(AreEqual(FString(TEXT("Text 世界")), UnicodeText.ToString(), TEXT("FText Unicode value should round-trip through ToString")));
	}

	TEST_METHOD(StringFamilyScriptSpecialTextValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_SpecialText"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertySpecialText.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringSpecialTextActor : AActor
			{
				UPROPERTY()
				FText EscapedText;

				UPROPERTY()
				FText LongText;

				UPROPERTY()
				FString LongTextSource;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					EscapedText = FText::FromString("LineOne\nLineTwo\t\"Quote\"\\Slash");

					for (int i = 0; i < 1050; ++i)
					{
						LongTextSource += "t";
					}

					LongText = FText::FromString(LongTextSource);
				}
			}
			)AS"),
			TEXT("ACoverageFStringSpecialTextActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-family special text actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-family special text actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		const FString ExpectedEscapedText = FString(TEXT("LineOne\nLineTwo\t\"Quote\"\\Slash"));
		FText EscapedText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("EscapedText"), EscapedText)));
		ASSERT_THAT(AreEqual(ExpectedEscapedText, EscapedText.ToString(), TEXT("AS-written FText should preserve escaped characters")));

		FString ExpectedLongText;
		ExpectedLongText.Reserve(1050);
		for (int32 Index = 0; Index < 1050; ++Index)
		{
			ExpectedLongText.AppendChar(TCHAR('t'));
		}
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LongTextSource"), ExpectedLongText, TEXT("AS long FString source should be observable"))));

		FText LongText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("LongText"), LongText)));
		ASSERT_THAT(AreEqual(ExpectedLongText, LongText.ToString(), TEXT("AS-written long FText should preserve the full string")));
	}

	// -------------------------------------------------------------------------
	// String containers: TArray, TMap, TSet.
	// -------------------------------------------------------------------------
	TEST_METHOD(StringContainerProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_Container"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyContainer.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringContainerActor : AActor
			{
				UPROPERTY()
				TArray<FString> StringArray;

				UPROPERTY()
				TArray<FName> NameArray;

				UPROPERTY()
				TArray<FText> TextArray;

				UPROPERTY()
				TMap<FString, int> StringToIntMap;

				UPROPERTY()
				TMap<int, FString> IntToStringMap;

				UPROPERTY()
				TMap<int, FName> IntToNameMap;

				UPROPERTY()
				TMap<int, FText> IntToTextMap;

				UPROPERTY()
				TMap<FName, int> NameToIntMap;

				UPROPERTY()
				TSet<FString> StringSet;

				UPROPERTY()
				TSet<FName> NameSet;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StringArray.Add("First");
					StringArray.Add("Second");
					StringArray.Add("Third");

					NameArray.Add(n"Alpha");
					NameArray.Add(n"Beta");
					NameArray.Add(NAME_None);

					TextArray.Add(FText::FromString("Text1"));
					TextArray.Add(FText::FromString("Text2"));
					TextArray.Add(FText::FromString(""));

					StringToIntMap.Add("One", 1);
					StringToIntMap.Add("Two", 2);

					IntToStringMap.Add(10, "Ten");
					IntToStringMap.Add(20, "Twenty");

					IntToNameMap.Add(10, n"TenName");
					IntToNameMap.Add(20, n"TwentyName");

					IntToTextMap.Add(10, FText::FromString("TenText"));
					IntToTextMap.Add(20, FText::FromString("TwentyText"));

					NameToIntMap.Add(n"First", 100);
					NameToIntMap.Add(n"Second", 200);
					NameToIntMap.Add(NAME_None, 300);

					StringSet.Add("Apple");
					StringSet.Add("Banana");
					StringSet.Add("Apple");  // Duplicate

					NameSet.Add(n"Tag1");
					NameSet.Add(n"Tag2");
				}
			}
			)AS"),
			TEXT("ACoverageFStringContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-container actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FArrayProperty* StringArrayProperty = CastField<FArrayProperty>(RequireProperty(ScriptClass, TEXT("StringArray")));
		const FArrayProperty* NameArrayProperty = CastField<FArrayProperty>(RequireProperty(ScriptClass, TEXT("NameArray")));
		const FArrayProperty* TextArrayProperty = CastField<FArrayProperty>(RequireProperty(ScriptClass, TEXT("TextArray")));
		const FMapProperty* StringToIntMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("StringToIntMap")));
		const FMapProperty* IntToStringMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("IntToStringMap")));
		const FMapProperty* IntToNameMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("IntToNameMap")));
		const FMapProperty* IntToTextMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("IntToTextMap")));
		const FMapProperty* NameToIntMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("NameToIntMap")));
		const FSetProperty* StringSetProperty = CastField<FSetProperty>(RequireProperty(ScriptClass, TEXT("StringSet")));
		const FSetProperty* NameSetProperty = CastField<FSetProperty>(RequireProperty(ScriptClass, TEXT("NameSet")));
		ASSERT_THAT(IsNotNull(StringArrayProperty, TEXT("StringArray should be generated as FArrayProperty")));
		ASSERT_THAT(IsNotNull(NameArrayProperty, TEXT("NameArray should be generated as FArrayProperty")));
		ASSERT_THAT(IsNotNull(TextArrayProperty, TEXT("TextArray should be generated as FArrayProperty")));
		ASSERT_THAT(IsNotNull(StringToIntMapProperty, TEXT("StringToIntMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(IntToStringMapProperty, TEXT("IntToStringMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(IntToNameMapProperty, TEXT("IntToNameMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(IntToTextMapProperty, TEXT("IntToTextMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(NameToIntMapProperty, TEXT("NameToIntMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(StringSetProperty, TEXT("StringSet should be generated as FSetProperty")));
		ASSERT_THAT(IsNotNull(NameSetProperty, TEXT("NameSet should be generated as FSetProperty")));
		if (StringArrayProperty == nullptr || NameArrayProperty == nullptr || TextArrayProperty == nullptr
			|| StringToIntMapProperty == nullptr || IntToStringMapProperty == nullptr || IntToNameMapProperty == nullptr
			|| IntToTextMapProperty == nullptr || NameToIntMapProperty == nullptr
			|| StringSetProperty == nullptr || NameSetProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringArrayProperty->Inner), TEXT("TArray<FString> should use FStrProperty inner")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameArrayProperty->Inner), TEXT("TArray<FName> should use FNameProperty inner")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(TextArrayProperty->Inner), TEXT("TArray<FText> should use FTextProperty inner")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringToIntMapProperty->KeyProp), TEXT("TMap<FString,int> should use FStrProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(StringToIntMapProperty->ValueProp), TEXT("TMap<FString,int> should use FIntProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(IntToStringMapProperty->KeyProp), TEXT("TMap<int,FString> should use FIntProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(IntToStringMapProperty->ValueProp), TEXT("TMap<int,FString> should use FStrProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(IntToNameMapProperty->KeyProp), TEXT("TMap<int,FName> should use FIntProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(IntToNameMapProperty->ValueProp), TEXT("TMap<int,FName> should use FNameProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(IntToTextMapProperty->KeyProp), TEXT("TMap<int,FText> should use FIntProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(IntToTextMapProperty->ValueProp), TEXT("TMap<int,FText> should use FTextProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToIntMapProperty->KeyProp), TEXT("TMap<FName,int> should use FNameProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FIntProperty>(NameToIntMapProperty->ValueProp), TEXT("TMap<FName,int> should use FIntProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringSetProperty->ElementProp), TEXT("TSet<FString> should use FStrProperty element")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameSetProperty->ElementProp), TEXT("TSet<FName> should use FNameProperty element")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// TArray<FString>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StringArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FString> should have 3 elements")));

			ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[0]"), FString(TEXT("First")), TEXT("TArray<FString>[0]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[1]"), FString(TEXT("Second")), TEXT("TArray<FString>[1]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[2]"), FString(TEXT("Third")), TEXT("TArray<FString>[2]"))));
		}

		// TArray<FName>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("NameArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FName> should have 3 elements")));

			ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[0]"), FName(TEXT("Alpha")), TEXT("TArray<FName>[0]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[1]"), FName(TEXT("Beta")), TEXT("TArray<FName>[1]"))));
			ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[2]"), NAME_None, TEXT("TArray<FName>[2] should allow NAME_None"))));
		}

		// TArray<FText>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("TextArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FText> should have 3 elements")));

			FText TextValue;
			ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextArray[0]"), TextValue)));
			ASSERT_THAT(IsTrue(TextValue.EqualTo(FText::FromString(TEXT("Text1"))), TEXT("TArray<FText>[0] should round-trip")));

			FText SecondTextValue;
			ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextArray[1]"), SecondTextValue)));
			ASSERT_THAT(IsTrue(SecondTextValue.EqualTo(FText::FromString(TEXT("Text2"))), TEXT("TArray<FText>[1] should round-trip")));

			FText EmptyTextValue;
			ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextArray[2]"), EmptyTextValue)));
			ASSERT_THAT(IsTrue(EmptyTextValue.IsEmpty(), TEXT("TArray<FText>[2] should allow empty text")));
		}

		// TMap<FString, int>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToIntMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,int> should have 2 entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToIntMap"), FString(TEXT("One")), Value)));
			ASSERT_THAT(AreEqual(1, Value, TEXT("TMap<FString,int>[\"One\"] should be 1")));

			int32 SecondValue = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToIntMap"), FString(TEXT("Two")), SecondValue)));
			ASSERT_THAT(AreEqual(2, SecondValue, TEXT("TMap<FString,int>[\"Two\"] should be 2")));
		}

		// TMap<int, FString>
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("IntToStringMap"), 10, Value)));
			ASSERT_THAT(AreEqual(FString(TEXT("Ten")), Value, TEXT("TMap<int,FString>[10] should be \"Ten\"")));

			FString SecondValue;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("IntToStringMap"), 20, SecondValue)));
			ASSERT_THAT(AreEqual(FString(TEXT("Twenty")), SecondValue, TEXT("TMap<int,FString>[20] should be \"Twenty\"")));
		}

		// TMap<int, FName>
		{
			FName Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FNameProperty, FName>(*TestRunner, Actor, TEXT("IntToNameMap"), 20, Value)));
			ASSERT_THAT(AreEqual(FName(TEXT("TwentyName")), Value, TEXT("TMap<int,FName>[20] should be n\"TwentyName\"")));

			FName FirstValue;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FNameProperty, FName>(*TestRunner, Actor, TEXT("IntToNameMap"), 10, FirstValue)));
			ASSERT_THAT(AreEqual(FName(TEXT("TenName")), FirstValue, TEXT("TMap<int,FName>[10] should be n\"TenName\"")));
		}

		// TMap<int, FText>
		{
			FText Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FTextProperty, FText>(*TestRunner, Actor, TEXT("IntToTextMap"), 10, Value)));
			ASSERT_THAT(AreEqual(FString(TEXT("TenText")), Value.ToString(), TEXT("TMap<int,FText>[10] should be \"TenText\"")));

			FText SecondValue;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FTextProperty, FText>(*TestRunner, Actor, TEXT("IntToTextMap"), 20, SecondValue)));
			ASSERT_THAT(AreEqual(FString(TEXT("TwentyText")), SecondValue.ToString(), TEXT("TMap<int,FText>[20] should be \"TwentyText\"")));
		}

		// TMap<FName, int>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToIntMap"), Count)));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<FName,int> should have 3 entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, Actor, TEXT("NameToIntMap"), FName(TEXT("Second")), Value)));
			ASSERT_THAT(AreEqual(200, Value, TEXT("TMap<FName,int>[n\"Second\"] should be 200")));

			int32 FirstValue = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, Actor, TEXT("NameToIntMap"), FName(TEXT("First")), FirstValue)));
			ASSERT_THAT(AreEqual(100, FirstValue, TEXT("TMap<FName,int>[n\"First\"] should be 100")));

			int32 NoneValue = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FIntProperty, int32>(*TestRunner, Actor, TEXT("NameToIntMap"), NAME_None, NoneValue)));
			ASSERT_THAT(AreEqual(300, NoneValue, TEXT("TMap<FName,int>[NAME_None] should be 300")));
		}

		// TSet<FString>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StringSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FString> should have 2 elements (deduplicated)")));

			bool bContains = SetContainsByPath<FString>(*TestRunner, Actor, TEXT("StringSet"), FString(TEXT("Apple")));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<FString> should contain \"Apple\"")));

			bool bContainsSecond = SetContainsByPath<FString>(*TestRunner, Actor, TEXT("StringSet"), FString(TEXT("Banana")));
			ASSERT_THAT(IsTrue(bContainsSecond, TEXT("TSet<FString> should contain \"Banana\"")));
		}

		// TSet<FName>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("NameSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FName> should have 2 elements")));

			bool bContains = SetContainsByPath<FName>(*TestRunner, Actor, TEXT("NameSet"), FName(TEXT("Tag1")));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<FName> should contain n\"Tag1\"")));

			bool bContainsSecond = SetContainsByPath<FName>(*TestRunner, Actor, TEXT("NameSet"), FName(TEXT("Tag2")));
			ASSERT_THAT(IsTrue(bContainsSecond, TEXT("TSet<FName> should contain n\"Tag2\"")));
		}
	}

	TEST_METHOD(StringFamilyMapKeyValueCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageFStringProperty_MapCombinations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageFStringPropertyMapCombinations.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageFStringMapCombinationsActor : AActor
			{
				UPROPERTY()
				TMap<FString, FName> StringToNameMap;

				UPROPERTY()
				TMap<FString, FText> StringToTextMap;

				UPROPERTY()
				TMap<FName, FString> NameToStringMap;

				UPROPERTY()
				TMap<FName, FText> NameToTextMap;

				UPROPERTY()
				TMap<FName, FName> NameToNameMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					StringToNameMap.Add("StringKey", n"StringName");
					StringToTextMap.Add("TextKey", FText::FromString("String Text Value"));

					NameToStringMap.Add(n"NameKey", "Name String Value");
					NameToTextMap.Add(n"NameTextKey", FText::FromString("Name Text Value"));
					NameToNameMap.Add(n"OuterName", n"InnerName");
				}
			}
			)AS"),
			TEXT("ACoverageFStringMapCombinationsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-family map-combinations actor class should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		const FMapProperty* StringToNameMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("StringToNameMap")));
		const FMapProperty* StringToTextMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("StringToTextMap")));
		const FMapProperty* NameToStringMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("NameToStringMap")));
		const FMapProperty* NameToTextMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("NameToTextMap")));
		const FMapProperty* NameToNameMapProperty = CastField<FMapProperty>(RequireProperty(ScriptClass, TEXT("NameToNameMap")));
		ASSERT_THAT(IsNotNull(StringToNameMapProperty, TEXT("StringToNameMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(StringToTextMapProperty, TEXT("StringToTextMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(NameToStringMapProperty, TEXT("NameToStringMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(NameToTextMapProperty, TEXT("NameToTextMap should be generated as FMapProperty")));
		ASSERT_THAT(IsNotNull(NameToNameMapProperty, TEXT("NameToNameMap should be generated as FMapProperty")));
		if (StringToNameMapProperty == nullptr || StringToTextMapProperty == nullptr || NameToStringMapProperty == nullptr
			|| NameToTextMapProperty == nullptr || NameToNameMapProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringToNameMapProperty->KeyProp), TEXT("TMap<FString,FName> should use FStrProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(StringToNameMapProperty->ValueProp), TEXT("TMap<FString,FName> should use FNameProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringToTextMapProperty->KeyProp), TEXT("TMap<FString,FText> should use FStrProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(StringToTextMapProperty->ValueProp), TEXT("TMap<FString,FText> should use FTextProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToStringMapProperty->KeyProp), TEXT("TMap<FName,FString> should use FNameProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(NameToStringMapProperty->ValueProp), TEXT("TMap<FName,FString> should use FStrProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToTextMapProperty->KeyProp), TEXT("TMap<FName,FText> should use FNameProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FTextProperty>(NameToTextMapProperty->ValueProp), TEXT("TMap<FName,FText> should use FTextProperty value")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToNameMapProperty->KeyProp), TEXT("TMap<FName,FName> should use FNameProperty key")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToNameMapProperty->ValueProp), TEXT("TMap<FName,FName> should use FNameProperty value")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-family map-combinations actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToNameMap"), Count)));
			ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FString,FName> should have 1 entry")));

			FName Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FNameProperty, FName>(*TestRunner, Actor, TEXT("StringToNameMap"), FString(TEXT("StringKey")), Value)));
			ASSERT_THAT(AreEqual(FName(TEXT("StringName")), Value, TEXT("TMap<FString,FName> should preserve FName values")));
		}
		{
			FText Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FTextProperty, FText>(*TestRunner, Actor, TEXT("StringToTextMap"), FString(TEXT("TextKey")), Value)));
			ASSERT_THAT(AreEqual(FString(TEXT("String Text Value")), Value.ToString(), TEXT("TMap<FString,FText> should preserve FText values")));
		}
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FStrProperty, FString>(*TestRunner, Actor, TEXT("NameToStringMap"), FName(TEXT("NameKey")), Value)));
			ASSERT_THAT(AreEqual(FString(TEXT("Name String Value")), Value, TEXT("TMap<FName,FString> should preserve FString values")));
		}
		{
			FText Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FTextProperty, FText>(*TestRunner, Actor, TEXT("NameToTextMap"), FName(TEXT("NameTextKey")), Value)));
			ASSERT_THAT(AreEqual(FString(TEXT("Name Text Value")), Value.ToString(), TEXT("TMap<FName,FText> should preserve FText values")));
		}
		{
			FName Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FName, FNameProperty, FName>(*TestRunner, Actor, TEXT("NameToNameMap"), FName(TEXT("OuterName")), Value)));
			ASSERT_THAT(AreEqual(FName(TEXT("InnerName")), Value, TEXT("TMap<FName,FName> should preserve FName values")));
		}
	}

	TEST_METHOD(FTextContainerHashBoundariesRemainUnsupported)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const TArray<FString> ExpectedDiagnostics = { TEXT("Key type does not have a hash function defined") };

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageFStringProperty_FTextMapKeyUnsupported"),
			ASTEST_AS(R"AS(
			int UseTextMapKey()
			{
				TMap<FText, int> Values;
				Values.Add(FText::FromString("Key"), 1);
				return Values.Num();
			}
			)AS"),
			TEXT("FText should remain unsupported as a TMap key"),
			MakeArrayView(ExpectedDiagnostics))));

		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageFStringProperty_FTextSetUnsupported"),
			ASTEST_AS(R"AS(
			int UseTextSetElement()
			{
				TSet<FText> Values;
				Values.Add(FText::FromString("Value"));
				return Values.Num();
			}
			)AS"),
			TEXT("FText should remain unsupported as a TSet element"),
			MakeArrayView(ExpectedDiagnostics))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
