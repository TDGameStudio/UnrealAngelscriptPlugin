#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageContainerNestedTests
// -----------------------------------------------------------------------------
// Coverage for nested container declarations in AngelScript.
//
// The current fork rejects containers nested inside other containers at compile
// time. These tests keep the coverage matrix honest by proving each documented
// nested-container shape fails with the explicit compiler diagnostic.
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageContainerNestedTest,
	"Angelscript.TestModule.Coverage.ContainerNested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExpectNestedContainerRejected(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const FString& Source,
		const TCHAR* Label)
	{
		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Containers cannot be nested in other containers"));

		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			Source,
			Label,
			MakeArrayView(ExpectedDiagnostics));
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

	TEST_METHOD(NestedArrays_TwoDimensionalMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageNestedArrayUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageNestedArrayActor : AActor
			{
				UPROPERTY()
				TArray<TArray<int>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<int>> should remain an explicit unsupported boundary"))));
	}

	TEST_METHOD(NestedArrays_DeepMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageNestedArrayDeepUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageDeepNestedArrayActor : AActor
			{
				UPROPERTY()
				TArray<TArray<TArray<int>>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<TArray<int>>> should remain an explicit unsupported boundary"))));
	}

	TEST_METHOD(NestedArrays_LocalDeepMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageNestedArrayLocalDeepUnsupported"), ASTEST_AS(R"AS(
			int BuildLocalDeepMatrix()
			{
				TArray<TArray<TArray<int>>> Matrix;
				return Matrix.Num();
			}
			)AS"),
			TEXT("local TArray<TArray<TArray<int>>> should remain an explicit unsupported boundary"))));
	}

	TEST_METHOD(ArrayOfMaps)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageArrayOfMapsUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageArrayOfMapsActor : AActor
			{
				UPROPERTY()
				TArray<TMap<int, FString>> Dictionaries;
			}
			)AS"),
			TEXT("TArray<TMap<int,FString>> should remain an explicit unsupported boundary"))));
	}

	TEST_METHOD(MapOfArrays_OneToMany)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageMapOfArraysUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMapOfArraysActor : AActor
			{
				UPROPERTY()
				TMap<int, TArray<int>> GroupedData;
			}
			)AS"),
			TEXT("TMap<int,TArray<int>> should remain an explicit unsupported boundary"))));
	}

	TEST_METHOD(ArrayOfSets)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageArrayOfSetsUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageArrayOfSetsActor : AActor
			{
				UPROPERTY()
				TArray<TSet<int>> SetCollection;
			}
			)AS"),
			TEXT("TArray<TSet<int>> should remain an explicit unsupported boundary"))));
	}

	TEST_METHOD(MapOfMaps_TwoDimensionalMapping)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectNestedContainerRejected(*TestRunner, Engine, TEXT("ASCoverageMapOfMapsUnsupported"), ASTEST_AS(R"AS(
			UCLASS()
			class ACoverageMapOfMapsActor : AActor
			{
				UPROPERTY()
				TMap<int, TMap<FString, float>> NestedMap;
			}
			)AS"),
			TEXT("TMap<int,TMap<FString,float>> should remain an explicit unsupported boundary"))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
