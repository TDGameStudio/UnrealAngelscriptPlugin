#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptUhtCoverageTestTypes.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptTypeDatabaseTests,
	"Angelscript.TestModule.Engine.TypeDatabase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
class FAutomationRegisteredType final : public FAngelscriptType
{
public:
	explicit FAutomationRegisteredType(
		FString InTypeName,
		UClass* InClass = nullptr,
		void* InData = nullptr)
		: TypeName(MoveTemp(InTypeName))
		, Class(InClass)
		, Data(InData)
	{
	}

	virtual FString GetAngelscriptTypeName() const override
	{
		return TypeName;
	}

	virtual UClass* GetClass(const FAngelscriptTypeUsage&) const override
	{
		return Class;
	}

	virtual void* GetData() const override
	{
		return Data;
	}

private:
	FString TypeName;
	UClass* Class = nullptr;
	void* Data = nullptr;
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
	TEST_METHOD(ExplicitDatabaseLookupsAndAliasesNeverUseAmbientState)
	{
		FScopedAngelscriptEngineResolutionSuppressionForTesting NoCurrentEngineScope;
		FAngelscriptTypeDatabase DatabaseA;
		FAngelscriptTypeDatabase DatabaseB;
		int32 SharedDataIdentity = 0;
		UClass* SharedClassIdentity = UAngelscriptUhtCoverageTestObject::StaticClass();
		const FString SharedTypeName = TEXT("FExplicitDatabaseType");
		const FString SharedAliasName = TEXT("FExplicitDatabaseAlias");

		const TSharedRef<FAngelscriptType> TypeA = MakeShared<FAutomationRegisteredType>(
			SharedTypeName,
			SharedClassIdentity,
			&SharedDataIdentity);
		const TSharedRef<FAngelscriptType> TypeB = MakeShared<FAutomationRegisteredType>(
			SharedTypeName,
			SharedClassIdentity,
			&SharedDataIdentity);
		FAngelscriptType::Register(DatabaseA, TypeA);
		FAngelscriptType::Register(DatabaseB, TypeB);
		FAngelscriptType::RegisterAlias(DatabaseA, SharedAliasName, TypeA);
		FAngelscriptType::RegisterAlias(DatabaseB, SharedAliasName, TypeB);

		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByAngelscriptTypeName(DatabaseA, SharedTypeName).Get() == &TypeA.Get()
				&& FAngelscriptType::GetByAngelscriptTypeName(DatabaseB, SharedTypeName).Get() == &TypeB.Get(),
			TEXT("Explicit name lookups should resolve independently in the selected databases")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByAngelscriptTypeName(DatabaseA, SharedAliasName).Get() == &TypeA.Get()
				&& FAngelscriptType::GetByAngelscriptTypeName(DatabaseB, SharedAliasName).Get() == &TypeB.Get(),
			TEXT("Explicit alias registration should remain isolated to the selected databases")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByClass(DatabaseA, SharedClassIdentity).Get() == &TypeA.Get()
				&& FAngelscriptType::GetByClass(DatabaseB, SharedClassIdentity).Get() == &TypeB.Get(),
			TEXT("Explicit class lookups should resolve independently in the selected databases")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByData(DatabaseA, &SharedDataIdentity).Get() == &TypeA.Get()
				&& FAngelscriptType::GetByData(DatabaseB, &SharedDataIdentity).Get() == &TypeB.Get(),
			TEXT("Explicit data lookups should resolve independently in the selected databases")));
	}

	TEST_METHOD(AliasAndTypeFindersResetCleanly)
	{
		FScopedAngelscriptEngineResolutionSuppressionForTesting NoCurrentEngineScope;
		FAngelscriptTypeDatabase Database;
		ASSERT_THAT(AreEqual(
			0,
			Database.RegisteredTypes.Num(),
			TEXT("Type database lifecycle test should start from an empty local database")));

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

		FAngelscriptType::Register(Database, PreferredType);
		FAngelscriptType::Register(Database, FallbackType);
		FAngelscriptType::RegisterAlias(Database, AliasName, PreferredType);
		FAngelscriptType::RegisterTypeFinder(
			Database,
			[StoredValueProperty, PreferredType](FProperty* Property, FAngelscriptTypeUsage& Usage) -> bool
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
			Database.RegisteredTypes.Num(),
			TEXT("Type database lifecycle test should register exactly two concrete types before reset")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByAngelscriptTypeName(Database, PreferredTypeName).Get() == PreferredTypePtr,
			TEXT("Type database lifecycle test should resolve the base type by its registered name")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByAngelscriptTypeName(Database, AliasName).Get() == PreferredTypePtr,
			TEXT("Type database lifecycle test should resolve aliases to the same fake type")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByProperty(Database, StoredValueProperty, false).Get() == FallbackTypePtr,
			TEXT("Type database lifecycle test should still expose the fallback property matcher when type finders are disabled")));
		ASSERT_THAT(IsTrue(
			FAngelscriptType::GetByProperty(Database, StoredValueProperty).Get() == PreferredTypePtr,
			TEXT("Type database lifecycle test should prefer the registered type finder over the fallback property matcher")));

		const FAngelscriptTypeUsage UsageBeforeReset =
			FAngelscriptTypeUsage::FromProperty(Database, StoredValueProperty);
		ExpectUsageMatches(
			*TestRunner,
			TEXT("Type database lifecycle test before reset"),
			UsageBeforeReset,
			PreferredType,
			PreferredTypeName);

		Database = FAngelscriptTypeDatabase();

		const FAngelscriptTypeUsage UsageAfterReset =
			FAngelscriptTypeUsage::FromProperty(Database, StoredValueProperty);
		ASSERT_THAT(AreEqual(
			0,
			Database.RegisteredTypes.Num(),
			TEXT("Type database lifecycle test should clear all registered types after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByAngelscriptTypeName(Database, PreferredTypeName).Get(),
			TEXT("Type database lifecycle test should clear the registered base type after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByAngelscriptTypeName(Database, FallbackTypeName).Get(),
			TEXT("Type database lifecycle test should clear the registered fallback type after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByAngelscriptTypeName(Database, AliasName).Get(),
			TEXT("Type database lifecycle test should clear the registered alias after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByProperty(Database, StoredValueProperty, false).Get(),
			TEXT("Type database lifecycle test should remove fallback property resolution after reset")));
		ASSERT_THAT(IsNull(
			FAngelscriptType::GetByProperty(Database, StoredValueProperty).Get(),
			TEXT("Type database lifecycle test should remove finder-based property resolution after reset")));
		ASSERT_THAT(IsFalse(
			UsageAfterReset.IsValid(),
			TEXT("Type database lifecycle test should leave FromProperty invalid after reset")));
	}

	TEST_METHOD(CompatibilityAPIsRequireCheckedCurrentEngine)
	{
		FString Source;
		const FString SourcePath = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript/Source/AngelscriptRuntime/Core/AngelscriptType.cpp"));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(Source, *SourcePath),
			TEXT("Type database architecture guard should load AngelscriptType.cpp")));
		ASSERT_THAT(IsFalse(
			Source.Contains(TEXT("static FAngelscriptTypeDatabase LegacyDatabase")),
			TEXT("Type compatibility APIs should not retain a no-engine legacy database")));
		ASSERT_THAT(IsTrue(
			Source.Contains(TEXT("FAngelscriptEngine& Engine = FAngelscriptEngine::Get();"))
				&& Source.Contains(TEXT("Database != nullptr")),
			TEXT("Type compatibility APIs should resolve a checked current engine and its owned database")));
	}
};

#endif
