#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptUhtCoverageTestTypes.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptTypeDatabaseTests,
	"Angelscript.TestModule.Engine.TypeDatabase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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

static bool ExpectUsageMatches(
	FAutomationTestBase& Test,
	const TCHAR* Context,
	const FAngelscriptTypeUsage& Usage,
	const TSharedRef<FAngelscriptType>& ExpectedType,
	const FString& ExpectedDeclaration)
{
	FNoDiscardAsserter LocalAssert(Test);
	const FAngelscriptType* ExpectedTypePtr = &ExpectedType.Get();
	bool bOk = true;
	bOk &= LocalAssert.IsTrue(
		Usage.IsValid(),
		*FString::Printf(TEXT("%s should resolve to a valid usage"), Context));
	bOk &= LocalAssert.IsTrue(
		Usage.Type.Get() == ExpectedTypePtr,
		*FString::Printf(TEXT("%s should resolve to the expected registered type"), Context));
	bOk &= LocalAssert.IsFalse(
		Usage.bIsConst,
		*FString::Printf(TEXT("%s should not carry const qualifiers for a plain reflected property"), Context));
	bOk &= LocalAssert.IsFalse(
		Usage.bIsReference,
		*FString::Printf(TEXT("%s should not carry reference qualifiers for a plain reflected property"), Context));
	bOk &= LocalAssert.AreEqual(
		ExpectedDeclaration,
		Usage.GetAngelscriptDeclaration(),
		*FString::Printf(TEXT("%s should preserve the fake declaration"), Context));
	bOk &= LocalAssert.AreEqual(
		0,
		Usage.SubTypes.Num(),
		*FString::Printf(TEXT("%s should not create synthetic template subtypes"), Context));
	return bOk;
}

public:
	TEST_METHOD(AliasAndTypeFindersResetCleanly)
	{
FCoreTestContextStackGuard ContextGuard;
		FScopedSuppressProductionAngelscriptSubsystem SuppressProductionSubsystem;
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

		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("Type database lifecycle test should start without an ambient engine so it uses the legacy database")));

		FAngelscriptType::ResetTypeDatabase();
		ASSERT_THAT(AreEqual(
			0,
			FAngelscriptType::GetTypes().Num(),
			TEXT("Type database lifecycle test should start from an empty legacy database")));

		FProperty* StoredValueProperty = UAngelscriptUhtCoverageTestObject::StaticClass()->FindPropertyByName(TEXT("StoredValue"));
		ASSERT_THAT(IsNotNull(
			StoredValueProperty,
			TEXT("Type database lifecycle test should find UAngelscriptUhtCoverageTestObject::StoredValue")));

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

		ASSERT_THAT(AreEqual(
			2,
			FAngelscriptType::GetTypes().Num(),
			TEXT("Type database lifecycle test should register exactly two concrete types before reset")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByAngelscriptTypeName(PreferredTypeName).Get() == PreferredTypePtr,
			TEXT("Type database lifecycle test should resolve the base type by its registered name")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get() == PreferredTypePtr,
			TEXT("Type database lifecycle test should resolve aliases to the same fake type")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByProperty(StoredValueProperty, false).Get() == FallbackTypePtr,
			TEXT("Type database lifecycle test should still expose the fallback property matcher when type finders are disabled")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByProperty(StoredValueProperty).Get() == PreferredTypePtr,
			TEXT("Type database lifecycle test should prefer the registered type finder over the fallback property matcher")));

		const FAngelscriptTypeUsage UsageBeforeReset = FAngelscriptTypeUsage::FromProperty(StoredValueProperty);
		ExpectUsageMatches(
			*TestRunner,
			TEXT("Type database lifecycle test before reset"),
			UsageBeforeReset,
			PreferredType,
			PreferredTypeName);

		FAngelscriptType::ResetTypeDatabase();

		const FAngelscriptTypeUsage UsageAfterReset = FAngelscriptTypeUsage::FromProperty(StoredValueProperty);
		ASSERT_THAT(AreEqual(
			0,
			FAngelscriptType::GetTypes().Num(),
			TEXT("Type database lifecycle test should clear all registered types after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByAngelscriptTypeName(PreferredTypeName).Get(),
			TEXT("Type database lifecycle test should clear the registered base type after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByAngelscriptTypeName(FallbackTypeName).Get(),
			TEXT("Type database lifecycle test should clear the registered fallback type after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByAngelscriptTypeName(AliasName).Get(),
			TEXT("Type database lifecycle test should clear the registered alias after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByProperty(StoredValueProperty, false).Get(),
			TEXT("Type database lifecycle test should remove fallback property resolution after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByProperty(StoredValueProperty).Get(),
			TEXT("Type database lifecycle test should remove finder-based property resolution after reset")));
		ASSERT_THAT(IsFalse(
			UsageAfterReset.IsValid(),
			TEXT("Type database lifecycle test should leave FromProperty invalid after reset")));
	}
};

#endif
