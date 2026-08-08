// ============================================================================
// AngelscriptJsonBindingsTests.cpp
//
// JSON binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.Json.*
//
// Sections:
//   ObjectRoundTrip — Full JSON object create/serialize/parse round-trip
//   ValueContainers  — Value extraction and object/value lifetime constructors
//   Serialization    — Load/save entry points and malformed-input rejection
//   IteratorBoundary — Single-element traversal and out-of-range Proceed
//   ErrorPaths      — Type mismatch, out-of-bounds, iterator mutation exceptions
//
// CQTest adaptation notes:
//   Two original legacy automation classes merged into one
//   TEST_CLASS with two TEST_METHODs. The custom exception execution helper
//   is retained for the error-path tests. The round-trip test uses
//   ExpectGlobalInt via standard `int Entry()` → `int RoundTrip()` rename.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptJsonBindingsTest,
	"Angelscript.TestModule.Bindings.Json",
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
	// Section: ObjectRoundTrip
	// ====================================================================

	TEST_METHOD(ObjectRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJson_ObjectRoundTrip"), ASTEST_AS(R"AS(
			int RoundTrip()
			{
				FJsonObject Root;
				Root.SetStringField("Name", "Alice");
				Root.SetNumberField("Score", 1337.0);
				Root.SetBoolField("Enabled", true);

				FJsonObject Child = Root.CreateObjectField("Child");
				Child.SetStringField("Label", "Nested");
				Child.SetNumberField("Count", 2.0);

				FJsonArray Values;
				Values.AddString("First");
				Values.AddNumber(42);
				Root.SetArrayField("Values", Values);

				FString Serialized = Root.SaveToString(false);
				if (Serialized.IsEmpty())
				{
					return 10;
				}

				FJsonObject Parsed = Json::ParseString(Serialized);
				if (!Parsed.IsValid())
				{
					return 20;
				}

				if (Parsed.GetStringField("Name") != "Alice")
				{
					return 30;
				}
				if (Parsed.GetNumberField("Score") != 1337.0)
				{
					return 40;
				}
				if (!Parsed.GetBoolField("Enabled"))
				{
					return 50;
				}

				FJsonArray ParsedValues = Parsed.GetArrayField("Values");
				if (ParsedValues.Num() != 2)
				{
					return 60;
				}

				FJsonObjectFieldIterator Iterator = Parsed.Iterator();
				bool bSawEnabled = false;
				bool bIteratorBoolValue = false;
				while (Iterator.CanProceed)
				{
					Iterator.Proceed();
					if (Iterator.GetFieldName() == "Enabled")
					{
						bSawEnabled = Iterator.GetValue().TryGetBool(bIteratorBoolValue);
						break;
					}
				}
				if (!bSawEnabled || !bIteratorBoolValue)
				{
					return 65;
				}

				FJsonValue FirstValue = ParsedValues.GetValueAt(0);
				FString FirstString;
				if (!FirstValue.TryGetString(FirstString))
				{
					return 70;
				}
				if (FirstString != "First")
				{
					return 80;
				}

				FJsonValue SecondValue = ParsedValues.GetValueAt(1);
				int32 SecondNumber = 0;
				if (!SecondValue.TryGetNumber(SecondNumber))
				{
					return 90;
				}
				if (SecondNumber != 42)
				{
					return 100;
				}

				FJsonObject ParsedChild;
				if (!Parsed.TryGetObjectField("Child", ParsedChild))
				{
					return 110;
				}
				if (ParsedChild.GetStringField("Label") != "Nested")
				{
					return 120;
				}
				if (ParsedChild.GetNumberField("Count") != 2.0)
				{
					return 130;
				}

				FJsonArray ParsedValuesAgain;
				if (!Parsed.TryGetArrayField("Values", ParsedValuesAgain))
				{
					return 140;
				}
				if (ParsedValuesAgain.Num() != 2)
				{
					return 150;
				}

				if (Json::ValueTypeToString(EJsonType::Array) != "Array")
				{
					return 160;
				}

				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M, TEXT("int RoundTrip()"), TEXT("Json object round-trip operations should preserve field values and JSON type strings"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: ValueContainers
	// ====================================================================

	TEST_METHOD(ValueContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int ValueContainers()
			{
				FJsonValue DefaultValue;
				FString UntouchedString = "Sentinel";
				if (DefaultValue.GetType() != EJsonType::None || DefaultValue.TryGetString(UntouchedString) || UntouchedString != "Sentinel")
				{
					return 0;
				}

				FJsonObject Parsed = Json::ParseString("{\"Child\":{\"Label\":\"Nested\"},\"Values\":[\"First\",7]}");
				FJsonObject Copy(Parsed);
				if (!Copy.IsValid())
				{
					return 0;
				}

				bool bReadChild = false;
				bool bReadArray = false;
				FJsonObjectFieldIterator Iterator = Copy.Iterator();
				while (Iterator.CanProceed)
				{
					Iterator.Proceed();
					FJsonValue Value = Iterator.GetValue();
					if (Iterator.GetFieldName() == "Child")
					{
						FJsonObject Child;
						FJsonArray WrongArray;
						if (!Value.TryGetObject(Child) || Value.TryGetArray(WrongArray) || Child.GetStringField("Label") != "Nested")
						{
							return 0;
						}
						bReadChild = true;
					}
					else if (Iterator.GetFieldName() == "Values")
					{
						FJsonArray Values;
						FJsonObject WrongObject;
						if (!Value.TryGetArray(Values) || Value.TryGetObject(WrongObject) || Values.Num() != 2)
						{
							return 0;
						}
						bReadArray = true;
					}
				}

				return bReadChild && bReadArray ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJson_ValueContainers"), ScriptSource);
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("Json value container module should compile")));
		if (!Mod.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int ValueContainers()"),
				TEXT("Json value and object constructors should support nested object and array extraction"), 1),
			TEXT("Json value containers should preserve nested extraction behavior")));
	}

	// ====================================================================
	// Section: Serialization
	// ====================================================================

	TEST_METHOD(Serialization)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int LoadSaveAndRejectMalformedInput()
			{
				FJsonObject Loaded;
				if (!Loaded.LoadFromString("{\"Name\":\"Loaded\",\"Count\":2}"))
				{
					return 0;
				}
				if (Loaded.GetStringField("Name") != "Loaded" || Loaded.GetNumberField("Count") != 2.0)
				{
					return 0;
				}

				FString Pretty = Loaded.SaveToString();
				if (Pretty.IsEmpty() || !Pretty.Contains("\n") || !Pretty.Contains("\"Name\""))
				{
					return 0;
				}

				if (Loaded.LoadFromString("{"))
				{
					return 0;
				}

				FJsonObject ParseFailure = Json::ParseString("{");
				if (ParseFailure.IsValid())
				{
					return 0;
				}

				if (Json::ValueTypeToString(EJsonType::None) != "None"
					|| Json::ValueTypeToString(EJsonType::Object) != "Object"
					|| Json::ValueTypeToString(EJsonType(255)) != "<Invalid Type>")
				{
					return 0;
				}

				return 1;
			}
			)AS");

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJson_Serialization"), ScriptSource);
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("Json serialization module should compile")));
		if (!Mod.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int LoadSaveAndRejectMalformedInput()"),
				TEXT("Json load/save entry points should serialize pretty output and reject malformed input"), 1),
			TEXT("Json serialization entry points should handle valid and malformed input")));
	}

	// ====================================================================
	// Section: IteratorBoundary
	// ====================================================================

	TEST_METHOD(IteratorBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int IteratesSingleValue()
			{
				FJsonObject Root;
				Root.SetBoolField("Enabled", true);

				FJsonObjectFieldIterator Iterator = Root.Iterator();
				if (!Iterator.CanProceed)
				{
					return 0;
				}

				Iterator.Proceed();
				if (Iterator.CanProceed || Iterator.GetFieldName() != "Enabled" || Iterator.GetType() != EJsonType::Boolean)
				{
					return 0;
				}

				bool Enabled = false;
				return Iterator.GetValue().TryGetBool(Enabled) && Enabled ? 1 : 0;
			}

			void TriggerIteratorOutOfBounds()
			{
				FJsonObject EmptyObject;
				FJsonObjectFieldIterator Iterator = EmptyObject.Iterator();
				Iterator.Proceed();
			}
			)AS");

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJson_IteratorBoundary"), ScriptSource);
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("Json iterator boundary module should compile")));
		if (!Mod.IsValid())
		{
			return;
		}

		auto& M = Mod.GetModule();
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M, TEXT("int IteratesSingleValue()"),
				TEXT("Json iterator should expose the current field name, type, and value after Proceed"), 1),
			TEXT("Json iterator should traverse a single field")));

		TestRunner->AddExpectedError(TEXT("Iterator out of bounds."), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_IteratorBoundary"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerIteratorOutOfBounds()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		ASSERT_THAT(IsTrue(
			ExecuteAndExpectException(*TestRunner, Engine, M, TEXT("void TriggerIteratorOutOfBounds()"),
				TEXT("Json iterator out-of-range Proceed path"), TEXT("Iterator out of bounds.")),
			TEXT("Json iterator should report a deterministic out-of-range Proceed exception")));
	}

	// ====================================================================
	// Section: ErrorPaths
	// ====================================================================

	TEST_METHOD(ErrorPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJson_ErrorPaths"), ASTEST_AS(R"AS(
			void TriggerTypeError()
			{
				FJsonObject Root;
				FJsonObject Child = Root.CreateObjectField("Child");
				Child.SetNumberField("Score", 3.5);

				FJsonArray Values;
				Values.AddNumber(1);
				Root.SetArrayField("Values", Values);

				FString WrongTypeValue = Root.GetStringField("Child");
				if (WrongTypeValue == "NeverReached")
				{
					Root.SetStringField("Unreachable", WrongTypeValue);
				}
			}

			void TriggerOutOfBounds()
			{
				FJsonArray Values;
				Values.AddNumber(1);

				FJsonValue MissingValue = Values.GetValueAt(1);
				if (MissingValue.IsNull())
				{
					Values.AddString("NeverReached");
				}
			}

			void TriggerIteratorMutation()
			{
				FJsonObject Root;
				Root.SetNumberField("Score", 3.5);

				FJsonArray Values;
				Values.AddNumber(1);
				Root.SetArrayField("Values", Values);

				auto Iterator = Root.Iterator();
				while (Iterator.CanProceed)
				{
					Iterator.Proceed();
					Root.SetStringField("Injected", "bad");
				}
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Type error path
		TestRunner->AddExpectedError(TEXT("Json Value of type 'Object' used as a 'String'."), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_ErrorPaths"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerTypeError()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		if (!ExecuteAndExpectException(
				*TestRunner,
				Engine,
				M,
				TEXT("void TriggerTypeError()"),
				TEXT("Json type-error path"),
				TEXT("Json Value of type 'Object' used as a 'String'.")))
		{
			return;
		}

		// Out-of-bounds path
		TestRunner->AddExpectedError(TEXT("Array index is out of bounds"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_ErrorPaths"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerOutOfBounds()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		if (!ExecuteAndExpectException(
				*TestRunner,
				Engine,
				M,
				TEXT("void TriggerOutOfBounds()"),
				TEXT("Json out-of-bounds path"),
				TEXT("Array index is out of bounds")))
		{
			return;
		}

		// Iterator mutation path
		TestRunner->AddExpectedError(TEXT("FJsonObject is being modified during for loop iteration"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASJson_ErrorPaths"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerIteratorMutation()"), EAutomationExpectedErrorFlags::Contains, 0, false);

		if (!ExecuteAndExpectException(
				*TestRunner,
				Engine,
				M,
				TEXT("void TriggerIteratorMutation()"),
				TEXT("Json iterator-mutation path"),
				TEXT("FJsonObject is being modified during for loop iteration")))
		{
			return;
		}
	}
};

#endif
