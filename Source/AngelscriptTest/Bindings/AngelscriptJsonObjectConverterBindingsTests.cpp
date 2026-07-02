// ============================================================================
// AngelscriptJsonObjectConverterBindingsTests.cpp
//
// FJsonObjectConverter binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.JsonObjectConverter.FAngelscriptJsonObjectConverterBindingsTest.*
//
// Sections:
//   RoundTrip  — UStructToJsonObjectString, JsonObjectStringToUStruct,
//                AppendUStructToJsonObjectString, Json::ParseString field parity
//   ErrorPaths — non-USTRUCT input rejection, malformed JSON rejection
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptJsonObjectConverterBindingsTest,
	"Angelscript.TestModule.Bindings.JsonObjectConverter",
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
	// Section: RoundTrip
	// ====================================================================

	TEST_METHOD(RoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJsonObjectConverter_RoundTrip"), ASTEST_AS(R"AS(
			int RoundTrip_VectorToJson()
			{
				const FVector Original = FVector(1.0, 2.0, 3.0);
				FString Json;
				if (!FJsonObjectConverter::UStructToJsonObjectString(Original, Json, 0, 0, 0, false))
				{
					return 0;
				}
				if (Json.IsEmpty())
				{
					return 0;
				}
				return 1;
			}

			int RoundTrip_JsonToVector()
			{
				const FVector Original = FVector(1.0, 2.0, 3.0);
				FString Json;
				FJsonObjectConverter::UStructToJsonObjectString(Original, Json, 0, 0, 0, false);

				FVector Parsed;
				if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, Parsed, 0, 0))
				{
					return 0;
				}
				if (!Parsed.Equals(Original))
				{
					return 0;
				}
				return 1;
			}

			int RoundTrip_AppendRotator()
			{
				const FVector Original = FVector(1.0, 2.0, 3.0);
				FString Json;
				FJsonObjectConverter::UStructToJsonObjectString(Original, Json, 0, 0, 0, false);

				const FRotator ExtraRotation = FRotator(10.0, 20.0, 30.0);
				if (!FJsonObjectConverter::AppendUStructToJsonObjectString(ExtraRotation, Json, 0, 0, 0, false))
				{
					return 0;
				}
				if (!Json.Contains("\"X\"") || !Json.Contains("\"Y\"") || !Json.Contains("\"Z\""))
				{
					return 0;
				}
				if (!Json.Contains("\"Pitch\"") || !Json.Contains("\"Yaw\"") || !Json.Contains("\"Roll\""))
				{
					return 0;
				}
				return 1;
			}

			int RoundTrip_AppendedFieldParity()
			{
				const FVector Original = FVector(1.0, 2.0, 3.0);
				FString Json;
				FJsonObjectConverter::UStructToJsonObjectString(Original, Json, 0, 0, 0, false);

				const FRotator ExtraRotation = FRotator(10.0, 20.0, 30.0);
				FJsonObjectConverter::AppendUStructToJsonObjectString(ExtraRotation, Json, 0, 0, 0, false);

				FJsonObject Appended = Json::ParseString(Json);
				if (!Appended.IsValid())
				{
					return 0;
				}
				if (Appended.GetNumberField("X") != 1.0 || Appended.GetNumberField("Y") != 2.0 || Appended.GetNumberField("Z") != 3.0)
				{
					return 0;
				}
				if (Appended.GetNumberField("Pitch") != 10.0 || Appended.GetNumberField("Yaw") != 20.0 || Appended.GetNumberField("Roll") != 30.0)
				{
					return 0;
				}
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RoundTrip_VectorToJson()"), TEXT("UStructToJsonObjectString produces non-empty JSON"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RoundTrip_JsonToVector()"), TEXT("JsonObjectStringToUStruct round-trips FVector"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RoundTrip_AppendRotator()"), TEXT("AppendUStructToJsonObjectString merges FRotator fields"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int RoundTrip_AppendedFieldParity()"), TEXT("appended JSON preserves numeric field values"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	// ====================================================================
	// Section: ErrorPaths
	// ====================================================================

	TEST_METHOD(ErrorPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASJsonObjectConverter_ErrorPaths"), ASTEST_AS(R"AS(
			int Error_NonUStructRejected()
			{
				int PlainValue = 7;
				FString Json = "Seed";
				if (FJsonObjectConverter::UStructToJsonObjectString(PlainValue, Json, 0, 0, 0, false))
				{
					return 0;
				}
				if (Json != "Seed")
				{
					return 0;
				}
				return 1;
			}

			int Error_MalformedJsonRejected()
			{
				FVector Parsed = FVector(9.0, 9.0, 9.0);
				if (FJsonObjectConverter::JsonObjectStringToUStruct("{", Parsed, 0, 0))
				{
					return 0;
				}
				if (!Parsed.Equals(FVector(9.0, 9.0, 9.0)))
				{
					return 0;
				}
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Error_NonUStructRejected()"), TEXT("non-USTRUCT input fails without mutating sentinel"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Error_MalformedJsonRejected()"), TEXT("malformed JSON fails without mutating output"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
