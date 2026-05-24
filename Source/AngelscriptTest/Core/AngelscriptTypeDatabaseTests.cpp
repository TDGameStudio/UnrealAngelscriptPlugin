#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptUhtCoverageTestTypes.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Core_AngelscriptTypeDatabaseTests_Private
{
	struct FCoreTestContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FCoreTestContextStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FCoreTestContextStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

		void DiscardSavedStack()
		{
			SavedStack.Reset();
		}
	};

	class FAutomationRegisteredType final : public FAngelscriptType
	{
	public:
		explicit FAutomationRegisteredType(FString InTypeName)
			: TypeName(MoveTemp(InTypeName))
		{
		}

		virtual FString GetAngelscriptTypeName() const override
		{
			return TypeName;
		}

	private:
		FString TypeName;
	};

	class FAutomationPropertyMatchedType final : public FAngelscriptType
	{
	public:
		FAutomationPropertyMatchedType(FString InTypeName, const FProperty* InExpectedProperty)
			: TypeName(MoveTemp(InTypeName))
			, ExpectedProperty(InExpectedProperty)
		{
		}

		virtual FString GetAngelscriptTypeName() const override
		{
			return TypeName;
		}

		virtual bool MatchesProperty(
			const FAngelscriptTypeUsage& Usage,
			const FProperty* Property,
			EPropertyMatchType MatchType) const override
		{
			return MatchType == EPropertyMatchType::TypeFinder && Property == ExpectedProperty;
		}

	private:
		FString TypeName;
		const FProperty* ExpectedProperty = nullptr;
	};

	bool ExpectUsageMatches(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const FAngelscriptTypeUsage& Usage,
		const TSharedRef<FAngelscriptType>& ExpectedType,
		const FString& ExpectedDeclaration)
	{
		const FAngelscriptType* ExpectedTypePtr = &ExpectedType.Get();
		bool bOk = true;
		bOk &= Test.TestTrue(
			*FString::Printf(TEXT("%s should resolve to a valid usage"), Context),
			Usage.IsValid());
		bOk &= Test.TestTrue(
			*FString::Printf(TEXT("%s should resolve to the expected registered type"), Context),
			Usage.Type.Get() == ExpectedTypePtr);
		bOk &= Test.TestFalse(
			*FString::Printf(TEXT("%s should not carry const qualifiers for a plain reflected property"), Context),
			Usage.bIsConst);
		bOk &= Test.TestFalse(
			*FString::Printf(TEXT("%s should not carry reference qualifiers for a plain reflected property"), Context),
			Usage.bIsReference);
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s should preserve the fake declaration"), Context),
			Usage.GetAngelscriptDeclaration(),
			ExpectedDeclaration);
		bOk &= Test.TestEqual(
			*FString::Printf(TEXT("%s should not create synthetic template subtypes"), Context),
			Usage.SubTypes.Num(),
			0);
		return bOk;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptTypeDatabaseTests,
	"Angelscript.TestModule.Engine.TypeDatabase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AliasAndTypeFindersResetCleanly)
	{
		using namespace AngelscriptTest_Core_AngelscriptTypeDatabaseTests_Private;
		FCoreTestContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();

		ON_SCOPE_EXIT
		{
			FAngelscriptType::ResetTypeDatabase();
			FAngelscriptEngineContextStack::SnapshotAndClear();
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		if (!TestRunner->TestNull(
				TEXT("Type database lifecycle test should start without an ambient engine so it uses the legacy database"),
				FAngelscriptEngine::TryGetCurrentEngine()))
		{
			return;
		}

		FAngelscriptType::ResetTypeDatabase();
		if (!TestRunner->TestEqual(
				TEXT("Type database lifecycle test should start from an empty legacy database"),
				FAngelscriptType::GetTypes().Num(),
				0))
		{
			return;
		}

		FProperty* StoredValueProperty = UAngelscriptUhtCoverageTestObject::StaticClass()->FindPropertyByName(TEXT("StoredValue"));
		if (!TestRunner->TestNotNull(
				TEXT("Type database lifecycle test should find UAngelscriptUhtCoverageTestObject::StoredValue"),
				StoredValueProperty))
		{
			return;
		}

		const FString PreferredTypeName = TEXT("AutomationMappedType");
		const FString FallbackTypeName = TEXT("AutomationFallbackType");
		const FString AliasName = TEXT("AutomationAlias");

		const TSharedRef<FAngelscriptType> PreferredType = MakeShared<FAutomationRegisteredType>(PreferredTypeName);
		const TSharedRef<FAngelscriptType> FallbackType = MakeShared<FAutomationPropertyMatchedType>(FallbackTypeName, StoredValueProperty);
		const FAngelscriptType* PreferredTypePtr = &PreferredType.Get();
		const FAngelscriptType* FallbackTypePtr = &FallbackType.Get();

		FAngelscriptType::Register(PreferredType);
		FAngelscriptType::Register(FallbackType);
		FAngelscriptType::RegisterAlias(AliasName, PreferredType);
		FAngelscriptType::RegisterTypeFinder([StoredValueProperty, PreferredType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
		{
			if (Property != StoredValueProperty)
			{
				return false;
			}

			Usage.Type = PreferredType;
			return true;
		});

		TestRunner->TestEqual(
			TEXT("Type database lifecycle test should register exactly two concrete types before reset"),
			FAngelscriptType::GetTypes().Num(),
			2);
		TestRunner->TestTrue(
			TEXT("Type database lifecycle test should resolve the base type by its registered name"),
			FAngelscriptType::GetByAngelscriptTypeName(PreferredTypeName).Get() == PreferredTypePtr);
		TestRunner->TestTrue(
			TEXT("Type database lifecycle test should resolve aliases to the same fake type"),
			FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get() == PreferredTypePtr);
		TestRunner->TestTrue(
			TEXT("Type database lifecycle test should still expose the fallback property matcher when type finders are disabled"),
			FAngelscriptType::GetByProperty(StoredValueProperty, false).Get() == FallbackTypePtr);
		TestRunner->TestTrue(
			TEXT("Type database lifecycle test should prefer the registered type finder over the fallback property matcher"),
			FAngelscriptType::GetByProperty(StoredValueProperty).Get() == PreferredTypePtr);

		const FAngelscriptTypeUsage UsageBeforeReset = FAngelscriptTypeUsage::FromProperty(StoredValueProperty);
		ExpectUsageMatches(
			*TestRunner,
			TEXT("Type database lifecycle test before reset"),
			UsageBeforeReset,
			PreferredType,
			PreferredTypeName);

		FAngelscriptType::ResetTypeDatabase();

		const FAngelscriptTypeUsage UsageAfterReset = FAngelscriptTypeUsage::FromProperty(StoredValueProperty);
		TestRunner->TestEqual(
			TEXT("Type database lifecycle test should clear all registered types after reset"),
			FAngelscriptType::GetTypes().Num(),
			0);
		TestRunner->TestNull(
			TEXT("Type database lifecycle test should clear the registered base type after reset"),
			FAngelscriptType::GetByAngelscriptTypeName(PreferredTypeName).Get());
		TestRunner->TestNull(
			TEXT("Type database lifecycle test should clear the registered fallback type after reset"),
			FAngelscriptType::GetByAngelscriptTypeName(FallbackTypeName).Get());
		TestRunner->TestNull(
			TEXT("Type database lifecycle test should clear the registered alias after reset"),
			FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get());
		TestRunner->TestNull(
			TEXT("Type database lifecycle test should remove fallback property resolution after reset"),
			FAngelscriptType::GetByProperty(StoredValueProperty, false).Get());
		TestRunner->TestNull(
			TEXT("Type database lifecycle test should remove finder-based property resolution after reset"),
			FAngelscriptType::GetByProperty(StoredValueProperty).Get());
		TestRunner->TestFalse(
			TEXT("Type database lifecycle test should leave FromProperty invalid after reset"),
			UsageAfterReset.IsValid());
	}
};

#endif
