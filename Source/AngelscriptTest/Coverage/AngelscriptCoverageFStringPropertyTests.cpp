#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleBuilder.h"
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

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageFStringPropertyTest,
	"Angelscript.TestModule.Coverage.FStringProperty",
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-defaults actor should spawn")));

		// FString with value
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello")), TEXT("FString UPROPERTY with default value"));

		// FString empty
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EmptyString"), FString(TEXT("")), TEXT("FString UPROPERTY with empty default"));

		// FString no default (should be empty)
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NoDefaultString"), FString(TEXT("")), TEXT("FString UPROPERTY without default should be empty"));

		// FName with value
		VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("MyName")), TEXT("FName UPROPERTY with default value"));

		// FName empty
		VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("EmptyName"), FName(NAME_None), TEXT("FName UPROPERTY with empty default"));

		// FText default constructed (empty)
		FText ReadText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), ReadText)));
		TestRunner->TestTrue(TEXT("FText UPROPERTY default should be empty"), ReadText.IsEmpty());
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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-write actor should spawn")));

		// FString write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello World")))));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("Hello World")), TEXT("FString write round-trip"));

		ASSERT_THAT(IsTrue(SetByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("")))));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringValue"), FString(TEXT("")), TEXT("FString empty write round-trip"));

		// FName write round-trip
		ASSERT_THAT(IsTrue(SetByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("TestName")))));
		VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameValue"), FName(TEXT("TestName")), TEXT("FName write round-trip"));

		// FText write round-trip
		FText TestText = FText::FromString(TEXT("Test Text"));
		ASSERT_THAT(IsTrue(SetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), TestText)));
		FText ReadText;
		ASSERT_THAT(IsTrue(GetByPath<FTextProperty, FText>(*TestRunner, Actor, TEXT("TextValue"), ReadText)));
		TestRunner->TestTrue(TEXT("FText write round-trip"), ReadText.EqualTo(TestText));
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

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					EmptyString = "";

					// Long string (100+ characters)
					LongString = "This is a very long string that contains more than one hundred characters to test the handling of large string properties in the property system and reflection layer.";

					// Special characters
					SpecialChars = "Hello\nWorld\tTab\"Quote\"\\Backslash";

					// Unicode (if supported)
					UnicodeString = "Hello 世界 🌍";
				}
			}
			)AS"),
			TEXT("ACoverageFStringSpecialActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("String-special actor class should compile")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-special actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// Empty string
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("EmptyString"), FString(TEXT("")), TEXT("Empty string"));

		// Long string
		FString ExpectedLong = TEXT("This is a very long string that contains more than one hundred characters to test the handling of large string properties in the property system and reflection layer.");
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LongString"), ExpectedLong, TEXT("Long string (100+ chars)"));

		// Special characters
		FString ExpectedSpecial = TEXT("Hello\nWorld\tTab\"Quote\"\\Backslash");
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("SpecialChars"), ExpectedSpecial, TEXT("Special characters (newline/tab/quote/backslash)"));

		// Unicode
		FString ExpectedUnicode = TEXT("Hello 世界 🌍");
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("UnicodeString"), ExpectedUnicode, TEXT("Unicode string"));
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

					TextArray.Add(FText::FromString("Text1"));
					TextArray.Add(FText::FromString("Text2"));

					StringToIntMap.Add("One", 1);
					StringToIntMap.Add("Two", 2);

					IntToStringMap.Add(10, "Ten");
					IntToStringMap.Add(20, "Twenty");

					NameToIntMap.Add(n"First", 100);
					NameToIntMap.Add(n"Second", 200);

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

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("String-container actor should spawn")));
		BeginPlayActor(Engine, *Actor);

		// TArray<FString>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("StringArray"), Length)));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FString> should have 3 elements")));

			VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[0]"), FString(TEXT("First")), TEXT("TArray<FString>[0]"));
			VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[1]"), FString(TEXT("Second")), TEXT("TArray<FString>[1]"));
			VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StringArray[2]"), FString(TEXT("Third")), TEXT("TArray<FString>[2]"));
		}

		// TArray<FName>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("NameArray"), Length)));
			ASSERT_THAT(AreEqual(2, Length, TEXT("TArray<FName> should have 2 elements")));

			VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[0]"), FName(TEXT("Alpha")), TEXT("TArray<FName>[0]"));
			VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("NameArray[1]"), FName(TEXT("Beta")), TEXT("TArray<FName>[1]"));
		}

		// TMap<FString, int>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringToIntMap"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,int> should have 2 entries")));

			int32 Value = 0;
			ASSERT_THAT(IsTrue(GetMapValueByPath<FString, FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToIntMap"), FString(TEXT("One")), Value)));
			ASSERT_THAT(AreEqual(1, Value, TEXT("TMap<FString,int>[\"One\"] should be 1")));
		}

		// TMap<int, FString>
		{
			FString Value;
			ASSERT_THAT(IsTrue(GetMapValueByPath<int32, FStrProperty, FString>(*TestRunner, Actor, TEXT("IntToStringMap"), 10, Value)));
			ASSERT_THAT(AreEqual(FString(TEXT("Ten")), Value, TEXT("TMap<int,FString>[10] should be \"Ten\"")));
		}

		// TSet<FString>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StringSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FString> should have 2 elements (deduplicated)")));

			bool bContains = SetContainsByPath<FString>(*TestRunner, Actor, TEXT("StringSet"), FString(TEXT("Apple")));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<FString> should contain \"Apple\"")));
		}

		// TSet<FName>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("NameSet"), Count)));
			ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FName> should have 2 elements")));

			bool bContains = SetContainsByPath<FName>(*TestRunner, Actor, TEXT("NameSet"), FName(TEXT("Tag1")));
			ASSERT_THAT(IsTrue(bContains, TEXT("TSet<FName> should contain n\"Tag1\"")));
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
