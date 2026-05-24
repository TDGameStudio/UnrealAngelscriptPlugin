#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "../Core/AngelscriptUhtCoverageTestTypes.h"
#include "../../AngelscriptRuntime/Core/AngelscriptType.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypeRegistryTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AliasAndPropertyFinder)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};

		const FString AliasName = TEXT("ScoreAlias");
		FProperty* StoredValueProperty = UAngelscriptUhtCoverageTestObject::StaticClass()->FindPropertyByName(TEXT("StoredValue"));
		if (!TestRunner->TestNotNull(
				TEXT("Type registry alias/property-finder test should find UAngelscriptUhtCoverageTestObject::StoredValue"),
				StoredValueProperty))
		{
			return;
		}

		const TSharedPtr<FAngelscriptType> IntType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int"));
		if (!TestRunner->TestNotNull(TEXT("Type registry alias/property-finder test should resolve the baseline int type"), IntType.Get()))
		{
			return;
		}

		FAngelscriptType::RegisterAlias(AliasName, IntType.ToSharedRef());

		int32 FinderCallCount = 0;
		FAngelscriptType::RegisterTypeFinder(
			[StoredValueProperty, AliasName, &FinderCallCount](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
			{
				if (Property != StoredValueProperty)
				{
					return false;
				}

				++FinderCallCount;
				Usage.Type = FAngelscriptType::GetByAngelscriptTypeName(AliasName);
				return Usage.Type.IsValid();
			});

		const TSharedPtr<FAngelscriptType> AliasType = FAngelscriptType::GetByAngelscriptTypeName(AliasName);
		const TSharedPtr<FAngelscriptType> FallbackPropertyType = FAngelscriptType::GetByProperty(StoredValueProperty, false);
		const int32 FinderCallsAfterFallbackLookup = FinderCallCount;

		FinderCallCount = 0;
		const TSharedPtr<FAngelscriptType> FinderPropertyType = FAngelscriptType::GetByProperty(StoredValueProperty, true);
		const int32 FinderCallsAfterFinderLookup = FinderCallCount;

		FinderCallCount = 0;
		const FAngelscriptTypeUsage UsageFromProperty = FAngelscriptTypeUsage::FromProperty(StoredValueProperty);
		const int32 FinderCallsAfterUsageLookup = FinderCallCount;

		TestRunner->TestTrue(
			TEXT("RegisterAlias should resolve the alias name to the baseline int type"),
			AliasType.Get() == IntType.Get());
		TestRunner->TestTrue(
			TEXT("GetByProperty(..., false) should keep using the built-in reflected-property fallback"),
			FallbackPropertyType.Get() == IntType.Get());
		TestRunner->TestEqual(
			TEXT("GetByProperty(..., false) should not invoke registered type finders"),
			FinderCallsAfterFallbackLookup,
			0);
		TestRunner->TestTrue(
			TEXT("GetByProperty(..., true) should resolve the alias-backed finder result"),
			FinderPropertyType.Get() == IntType.Get());
		TestRunner->TestEqual(
			TEXT("GetByProperty(..., true) should invoke the registered type finder once"),
			FinderCallsAfterFinderLookup,
			1);
		TestRunner->TestTrue(
			TEXT("FromProperty should preserve the alias-backed finder result as a valid usage"),
			UsageFromProperty.IsValid() && UsageFromProperty.Type.Get() == IntType.Get());
		TestRunner->TestEqual(
			TEXT("FromProperty should query the registered type finder exactly once"),
			FinderCallsAfterUsageLookup,
			1);
		TestRunner->TestFalse(
			TEXT("FromProperty should not add const qualifiers for a plain reflected int property"),
			UsageFromProperty.bIsConst);
		TestRunner->TestFalse(
			TEXT("FromProperty should not add reference qualifiers for a plain reflected int property"),
			UsageFromProperty.bIsReference);
		TestRunner->TestEqual(
			TEXT("FromProperty should still render the baseline int declaration"),
			UsageFromProperty.GetAngelscriptDeclaration(),
			TEXT("int"));

		}
	}
};

#endif
