#include "AngelscriptBindDatabase.h"
#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptBindDatabaseTests, "Angelscript.TestModule.Engine.BindDatabase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
struct FBindDatabaseContextStackGuard
{
	TArray<FAngelscriptEngine*> SavedStack;
	FBindDatabaseContextStackGuard() { SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear(); }
	~FBindDatabaseContextStackGuard() { FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack)); }
	void DiscardSavedStack() { SavedStack.Reset(); }
};

static FString MakeBindDatabaseAutomationDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("BindDatabase"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static FAngelscriptPropertyBind MakeSamplePropertyBind(const FString& Declaration, const FString& UnrealPath)
{
	FAngelscriptPropertyBind Bind;
	Bind.Declaration = Declaration; Bind.UnrealPath = UnrealPath;
	Bind.bCanWrite = true; Bind.bCanRead = true; Bind.bCanEdit = false;
	return Bind;
}

static FAngelscriptMethodBind MakeSampleMethodBind()
{
	FAngelscriptMethodBind Bind;
	Bind.Declaration = TEXT("void DestroyActor()"); Bind.UnrealPath = TEXT("/Script/Engine.Actor:K2_DestroyActor");
	Bind.ClassName = TEXT("AActor"); Bind.ScriptName = TEXT("DestroyActor");
	Bind.WorldContextArgument = 1; Bind.DeterminesOutputTypeArgument = -1;
	Bind.bStaticInUnreal = false; Bind.bStaticInScript = true; Bind.bGlobalScope = false;
	Bind.bNotAngelscriptProperty = true; Bind.bTrivial = true;
	return Bind;
}

static FAngelscriptClassBind MakeSampleClassBind(UClass* Class)
{
	FAngelscriptClassBind Bind;
	Bind.TypeName = TEXT("AActor"); Bind.UnrealPath = Class->GetPathName();
	Bind.Methods.Add(MakeSampleMethodBind());
	Bind.Properties.Add(MakeSamplePropertyBind(TEXT("float InitialLifeSpan"), TEXT("/Script/Engine.Actor:InitialLifeSpan")));
	return Bind;
}

static FAngelscriptClassBind MakeNamedSampleClassBind(UClass* Class, const FString& TypeName)
{
	FAngelscriptClassBind Bind = MakeSampleClassBind(Class); Bind.TypeName = TypeName; return Bind;
}

static FAngelscriptStructBind MakeSampleStructBind(UScriptStruct* Struct)
{
	FAngelscriptStructBind Bind;
	Bind.TypeName = TEXT("FHitResult"); Bind.UnrealPath = Struct->GetPathName();
	Bind.Properties.Add(MakeSamplePropertyBind(TEXT("bool bBlockingHit"), TEXT("/Script/Engine.HitResult:bBlockingHit")));
	return Bind;
}

static void WriteLegacyUnversionedCache(const FString& CachePath, const FAngelscriptClassBind& ClassBind, const FAngelscriptStructBind& StructBind)
{
	TArray<FAngelscriptStructBind> Structs;
	Structs.Add(StructBind);
	TArray<FAngelscriptClassBind> Classes;
	Classes.Add(ClassBind);

	TArray<uint8> Data;
	FMemoryWriter Writer(Data);
	Writer << Structs;
	Writer << Classes;
	FFileHelper::SaveArrayToFile(Data, *CachePath);
}

static bool ExpectPropertyBindEquals(FAutomationTestBase& Test, const TCHAR* Context, const FAngelscriptPropertyBind& Actual, const FAngelscriptPropertyBind& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	bool bOk = true;
	bOk &= LocalAssert.AreEqual(Expected.Declaration, Actual.Declaration, *FString::Printf(TEXT("%s should round-trip property declaration"), Context));
	bOk &= LocalAssert.AreEqual(Expected.UnrealPath, Actual.UnrealPath, *FString::Printf(TEXT("%s should round-trip property UnrealPath"), Context));
	bOk &= LocalAssert.AreEqual(Expected.bCanWrite, Actual.bCanWrite, *FString::Printf(TEXT("%s should round-trip property bCanWrite"), Context));
	bOk &= LocalAssert.AreEqual(Expected.bCanRead, Actual.bCanRead, *FString::Printf(TEXT("%s should round-trip property bCanRead"), Context));
	bOk &= LocalAssert.AreEqual(Expected.bCanEdit, Actual.bCanEdit, *FString::Printf(TEXT("%s should round-trip property bCanEdit"), Context));
	return bOk;
}

static bool ExpectMethodBindEquals(FAutomationTestBase& Test, const FAngelscriptMethodBind& Actual, const FAngelscriptMethodBind& Expected)
{
	FNoDiscardAsserter LocalAssert(Test);
	bool bOk = true;
	bOk &= LocalAssert.AreEqual(Expected.Declaration, Actual.Declaration, TEXT("BindDatabase round-trip should preserve method declaration"));
	bOk &= LocalAssert.AreEqual(Expected.UnrealPath, Actual.UnrealPath, TEXT("BindDatabase round-trip should preserve method UnrealPath"));
	bOk &= LocalAssert.AreEqual(Expected.ClassName, Actual.ClassName, TEXT("BindDatabase round-trip should preserve method ClassName"));
	bOk &= LocalAssert.AreEqual(Expected.ScriptName, Actual.ScriptName, TEXT("BindDatabase round-trip should preserve method ScriptName"));
	bOk &= LocalAssert.AreEqual(static_cast<int32>(Expected.WorldContextArgument), static_cast<int32>(Actual.WorldContextArgument), TEXT("BindDatabase round-trip should preserve method WorldContextArgument"));
	bOk &= LocalAssert.AreEqual(static_cast<int32>(Expected.DeterminesOutputTypeArgument), static_cast<int32>(Actual.DeterminesOutputTypeArgument), TEXT("BindDatabase round-trip should preserve method DeterminesOutputTypeArgument"));
	bOk &= LocalAssert.AreEqual(Expected.bStaticInUnreal, Actual.bStaticInUnreal, TEXT("BindDatabase round-trip should preserve method bStaticInUnreal"));
	bOk &= LocalAssert.AreEqual(Expected.bStaticInScript, Actual.bStaticInScript, TEXT("BindDatabase round-trip should preserve method bStaticInScript"));
	bOk &= LocalAssert.AreEqual(Expected.bGlobalScope, Actual.bGlobalScope, TEXT("BindDatabase round-trip should preserve method bGlobalScope"));
	bOk &= LocalAssert.AreEqual(Expected.bNotAngelscriptProperty, Actual.bNotAngelscriptProperty, TEXT("BindDatabase round-trip should preserve method bNotAngelscriptProperty"));
	bOk &= LocalAssert.AreEqual(Expected.bTrivial, Actual.bTrivial, TEXT("BindDatabase round-trip should preserve method bTrivial"));
	return bOk;
}

static bool DatabaseContainsClassBindNamed(const FAngelscriptBindDatabase& Database, const FString& TypeName)
{
	return Database.Classes.ContainsByPredicate([&TypeName](const FAngelscriptClassBind& Bind) { return Bind.TypeName == TypeName; });
}

static UDelegateFunction* GetSampleDelegateFunction()
{
	const FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(AActor::StaticClass(), GET_MEMBER_NAME_CHECKED(AActor, OnActorBeginOverlap));
	return DelegateProperty != nullptr ? Cast<UDelegateFunction>(DelegateProperty->SignatureFunction) : nullptr;
}

public:
	TEST_METHOD(SaveLoadRoundTripsClassesAndHeaders)
	{
FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!this->Assert.IsTrue(Fixture.IsValid(), TEXT("BindDatabase.SaveLoadRoundTripsClassesAndHeaders should acquire an isolated full engine"))) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase* LocalDatabase = Engine.GetBindDatabase();
		if (!this->Assert.IsNotNull(LocalDatabase, TEXT("BindDatabase.SaveLoadRoundTripsClassesAndHeaders should expose an engine-local bind database"))) { return; }
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();
		if (!this->Assert.IsTrue(&Database == LocalDatabase, TEXT("BindDatabase.SaveLoadRoundTripsClassesAndHeaders should resolve GetBindDatabaseForTesting through the scoped engine"))) { return; }

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		if (!this->Assert.IsNotNull(ActorClass, TEXT("should resolve AActor")) || !this->Assert.IsNotNull(HitResultStruct, TEXT("should resolve FHitResult"))) { return; }

		const FAngelscriptClassBind ExpectedClassBind = MakeSampleClassBind(ActorClass);
		const FAngelscriptStructBind ExpectedStructBind = MakeSampleStructBind(HitResultStruct);
		const FString CacheDirectory = MakeBindDatabaseAutomationDirectory();
		const FString CachePath = FPaths::Combine(CacheDirectory, TEXT("Binds.Cache"));
		const FString HeadersPath = CachePath + TEXT(".Headers");
		IFileManager::Get().MakeDirectory(*CacheDirectory, true);
		ON_SCOPE_EXIT { Database.Clear(); IFileManager::Get().DeleteDirectory(*CacheDirectory, false, true); };

		Database.Clear();
		Database.Classes.Add(ExpectedClassBind); Database.Structs.Add(ExpectedStructBind);
		Database.HeaderLinks.Add(ActorClass, TEXT("Dummy/ActorHeader.h")); Database.HeaderLinks.Add(HitResultStruct, TEXT("Dummy/HitResultHeader.h"));
		Database.Save(CachePath);
		if (!this->Assert.IsTrue(IFileManager::Get().FileExists(*CachePath), TEXT("should write Binds.Cache")) || !this->Assert.IsTrue(IFileManager::Get().FileExists(*HeadersPath), TEXT("should write Binds.Cache.Headers"))) { return; }

		TArray<uint8> CacheBytes;
		if (!this->Assert.IsTrue(FFileHelper::LoadFileToArray(CacheBytes, *CachePath), TEXT("should read Binds.Cache bytes"))) { return; }
		FMemoryReader CacheReader(CacheBytes);
		uint32 CacheMagic = 0;
		int32 CacheVersion = 0;
		CacheReader << CacheMagic;
		CacheReader << CacheVersion;
		if (!this->Assert.AreEqual(FAngelscriptBindDatabase::CacheMagic, CacheMagic, TEXT("should write bind database magic header"))
			|| !this->Assert.AreEqual(FAngelscriptBindDatabase::CacheVersion, CacheVersion, TEXT("should write current bind database version")))
		{
			return;
		}

		Database.Clear();
		if (!this->Assert.AreEqual(0, Database.Classes.Num(), TEXT("should clear class binds")) || !this->Assert.AreEqual(0, Database.Structs.Num(), TEXT("should clear struct binds")) || !this->Assert.AreEqual(0, Database.HeaderLinks.Num(), TEXT("should clear header links"))) { return; }

		Database.Load(CachePath, false);
		if (!this->Assert.AreEqual(1, Database.Classes.Num(), TEXT("should restore one class bind")) || !this->Assert.AreEqual(1, Database.Structs.Num(), TEXT("should restore one struct bind")) || !this->Assert.AreEqual(0, Database.HeaderLinks.Num(), TEXT("should keep header links empty without precompiled"))) { return; }

		const FAngelscriptClassBind& LoadedClassBind = Database.Classes[0];
		const FAngelscriptStructBind& LoadedStructBind = Database.Structs[0];
		bool bOk = true;
		bOk &= this->Assert.AreEqual(ExpectedClassBind.TypeName, LoadedClassBind.TypeName, TEXT("should round-trip class TypeName"));
		bOk &= this->Assert.AreEqual(ExpectedClassBind.UnrealPath, LoadedClassBind.UnrealPath, TEXT("should round-trip class UnrealPath"));
		bOk &= this->Assert.AreEqual(ExpectedClassBind.Methods.Num(), LoadedClassBind.Methods.Num(), TEXT("should round-trip class method count"));
		bOk &= this->Assert.AreEqual(ExpectedClassBind.Properties.Num(), LoadedClassBind.Properties.Num(), TEXT("should round-trip class property count"));
		bOk &= this->Assert.AreEqual(ExpectedStructBind.TypeName, LoadedStructBind.TypeName, TEXT("should round-trip struct TypeName"));
		bOk &= this->Assert.AreEqual(ExpectedStructBind.UnrealPath, LoadedStructBind.UnrealPath, TEXT("should round-trip struct UnrealPath"));
		bOk &= this->Assert.AreEqual(ExpectedStructBind.Properties.Num(), LoadedStructBind.Properties.Num(), TEXT("should round-trip struct property count"));
		if (!bOk) { return; }

		if (!ExpectMethodBindEquals(*TestRunner, LoadedClassBind.Methods[0], ExpectedClassBind.Methods[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase class bind"), LoadedClassBind.Properties[0], ExpectedClassBind.Properties[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase struct bind"), LoadedStructBind.Properties[0], ExpectedStructBind.Properties[0]))
		{ return; }

		Database.Clear();
		Database.Load(CachePath, true);
		if (!this->Assert.IsTrue(Database.HeaderLinks.Contains(ActorClass), TEXT("should populate class header links with precompiled")) ||
			!this->Assert.IsTrue(Database.HeaderLinks.Contains(HitResultStruct), TEXT("should populate struct header links with precompiled")))
		{ return; }

		const FString ActorHeader = Database.HeaderLinks.FindRef(ActorClass);
		const FString StructHeader = Database.HeaderLinks.FindRef(HitResultStruct);
		bOk &= this->Assert.IsFalse(ActorHeader.IsEmpty(), TEXT("should load a non-empty header for AActor"));
		bOk &= this->Assert.IsFalse(StructHeader.IsEmpty(), TEXT("should load a non-empty header for FHitResult"));
		bOk &= this->Assert.IsTrue(IFileManager::Get().FileExists(*ActorHeader), TEXT("should load an existing header path for AActor"));
		bOk &= this->Assert.IsTrue(IFileManager::Get().FileExists(*StructHeader), TEXT("should load an existing header path for FHitResult"));
		(void)bOk;
	}

	TEST_METHOD(RejectsLegacyUnversionedCache)
	{
FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!this->Assert.IsTrue(Fixture.IsValid(), TEXT("should acquire an isolated full engine"))) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		if (!this->Assert.IsNotNull(ActorClass, TEXT("should resolve AActor")) || !this->Assert.IsNotNull(HitResultStruct, TEXT("should resolve FHitResult"))) { return; }

		const FString CacheDirectory = MakeBindDatabaseAutomationDirectory();
		const FString CachePath = FPaths::Combine(CacheDirectory, TEXT("Binds.Cache"));
		IFileManager::Get().MakeDirectory(*CacheDirectory, true);
		ON_SCOPE_EXIT { Database.Clear(); IFileManager::Get().DeleteDirectory(*CacheDirectory, false, true); };

		Database.Clear();
		WriteLegacyUnversionedCache(CachePath, MakeSampleClassBind(ActorClass), MakeSampleStructBind(HitResultStruct));
		Database.Classes.Add(MakeNamedSampleClassBind(ActorClass, TEXT("BindDatabaseSentinelBeforeRejectedLoad")));

		FString LoadError;
		const bool bLoaded = Database.TryLoad(CachePath, false, &LoadError);
		bool bOk = true;
		bOk &= this->Assert.IsFalse(bLoaded, TEXT("legacy unversioned Binds.Cache should be rejected"));
		bOk &= this->Assert.IsTrue(LoadError.Contains(TEXT("regenerate Script/Binds.Cache")), TEXT("legacy rejection should explain cache regeneration"));
		bOk &= this->Assert.AreEqual(1, Database.Classes.Num(), TEXT("failed legacy load should preserve existing class binds"));
		bOk &= this->Assert.IsTrue(DatabaseContainsClassBindNamed(Database, TEXT("BindDatabaseSentinelBeforeRejectedLoad")), TEXT("failed legacy load should preserve existing sentinel bind"));
		(void)bOk;
	}

	TEST_METHOD(GetPrefersCurrentEngineSharedDatabaseAndFallsBackToLegacySingleton)
	{
FBindDatabaseContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine(); }
		ContextGuard.DiscardSavedStack();

		static const FString LegacySentinelTypeName(TEXT("BindDatabaseLegacySentinel"));
		static const FString EngineASentinelTypeName(TEXT("BindDatabaseEngineASentinel"));
		FAngelscriptBindDatabase* LegacyDatabase = &FAngelscriptBindDatabase::Get();
		if (!this->Assert.IsNotNull(LegacyDatabase, TEXT("should expose a legacy database without a current engine"))) { return; }

		ON_SCOPE_EXIT { LegacyDatabase->Clear(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine(); } DestroySharedTestEngine(); };

		LegacyDatabase->Clear();
		LegacyDatabase->Classes.Add(MakeNamedSampleClassBind(AActor::StaticClass(), LegacySentinelTypeName));
		FAngelscriptBindDatabase* LegacyDatabaseSecondRead = &FAngelscriptBindDatabase::Get();

		bool bOk = true;
		bOk &= this->Assert.IsNull(FAngelscriptTestEngineScopeAccess::GetCurrentEngine(), TEXT("should start without a current engine"));
		bOk &= this->Assert.IsTrue(LegacyDatabaseSecondRead == LegacyDatabase, TEXT("should reuse the same legacy singleton"));
		bOk &= this->Assert.IsTrue(DatabaseContainsClassBindNamed(*LegacyDatabase, LegacySentinelTypeName), TEXT("should preserve legacy sentinel data"));

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(EngineA.Get(), TEXT("should create engine A"))) { return; }
		FAngelscriptBindDatabase* EngineADatabaseFromGet = nullptr;
		FAngelscriptBindDatabase* EngineADirectDatabase = nullptr;

		{
			FAngelscriptEngineScope ScopeA(*EngineA);
			EngineADirectDatabase = EngineA->GetBindDatabase();
			EngineADatabaseFromGet = &FAngelscriptBindDatabase::Get();
			FAngelscriptBindDatabase& EngineADatabaseFromTesting = EngineA->GetBindDatabaseForTesting();
			bOk &= this->Assert.IsNotNull(EngineADirectDatabase, TEXT("should expose an engine-owned bind database"));
			bOk &= this->Assert.IsTrue(EngineADatabaseFromGet == EngineADirectDatabase, TEXT("should prefer current engine bind database"));
			bOk &= this->Assert.IsTrue(&EngineADatabaseFromTesting == EngineADirectDatabase, TEXT("should align GetBindDatabaseForTesting"));
			bOk &= this->Assert.IsTrue(EngineADirectDatabase != LegacyDatabase, TEXT("should not alias legacy singleton"));

			EngineADirectDatabase->Clear();
			EngineADirectDatabase->Classes.Add(MakeNamedSampleClassBind(AActor::StaticClass(), EngineASentinelTypeName));
			bOk &= this->Assert.IsTrue(DatabaseContainsClassBindNamed(*EngineADirectDatabase, EngineASentinelTypeName), TEXT("should keep engine A sentinel in engine-owned database"));
			bOk &= this->Assert.IsFalse(DatabaseContainsClassBindNamed(*LegacyDatabase, EngineASentinelTypeName), TEXT("should keep engine A sentinel out of legacy singleton"));

			// Clone-shares-database sub-test removed alongside the Clone
			// engine mechanism; independent Full engines have independent
			// bind databases, which is covered by the engine A vs C
			// recreation block below.
		}

		EngineA.Reset();
		bOk &= this->Assert.IsNull(FAngelscriptTestEngineScopeAccess::GetCurrentEngine(), TEXT("should restore no-current-engine baseline after destroying engine A"));

		TUniquePtr<FAngelscriptEngine> EngineC = CreateFullTestEngine();
		if (!this->Assert.IsNotNull(EngineC.Get(), TEXT("should create engine C"))) { return; }
		{
			FAngelscriptEngineScope ScopeC(*EngineC);
			FAngelscriptBindDatabase* EngineCDirectDatabase = EngineC->GetBindDatabase();
			FAngelscriptBindDatabase* EngineCDatabaseFromGet = &FAngelscriptBindDatabase::Get();
			bOk &= this->Assert.IsNotNull(EngineCDirectDatabase, TEXT("should expose bind database for engine C"));
			bOk &= this->Assert.IsTrue(EngineCDatabaseFromGet == EngineCDirectDatabase, TEXT("Get() should route through engine C database"));
			bOk &= this->Assert.IsTrue(EngineCDirectDatabase != EngineADatabaseFromGet, TEXT("should allocate fresh bind database for recreated full engine"));
			bOk &= this->Assert.IsFalse(DatabaseContainsClassBindNamed(*EngineCDirectDatabase, EngineASentinelTypeName), TEXT("should not leak engine A sentinel into recreated engine C"));
		}
		EngineC.Reset();

		bOk &= this->Assert.IsNull(FAngelscriptTestEngineScopeAccess::GetCurrentEngine(), TEXT("should end without a current engine"));
		bOk &= this->Assert.IsTrue(&FAngelscriptBindDatabase::Get() == LegacyDatabase, TEXT("should fall back to legacy singleton after all engines gone"));
		bOk &= this->Assert.IsTrue(DatabaseContainsClassBindNamed(*LegacyDatabase, LegacySentinelTypeName), TEXT("should preserve legacy sentinel after scoped engine lifetimes end"));
		(void)bOk;
	}

	TEST_METHOD(LoadWithoutHeadersSidecarLeavesHeaderLinksEmptyButRestoresBinds)
	{
FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!this->Assert.IsTrue(Fixture.IsValid(), TEXT("should acquire an isolated full engine"))) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		if (!this->Assert.IsNotNull(ActorClass, TEXT("should resolve AActor")) || !this->Assert.IsNotNull(HitResultStruct, TEXT("should resolve FHitResult"))) { return; }

		const FAngelscriptClassBind ExpectedClassBind = MakeSampleClassBind(ActorClass);
		const FAngelscriptStructBind ExpectedStructBind = MakeSampleStructBind(HitResultStruct);
		const FString CacheDirectory = MakeBindDatabaseAutomationDirectory();
		const FString CachePath = FPaths::Combine(CacheDirectory, TEXT("Binds.Cache"));
		const FString HeadersPath = CachePath + TEXT(".Headers");
		const FString SentinelHeaderPath = TEXT("Sentinel/ShouldBeCleared.h");
		IFileManager::Get().MakeDirectory(*CacheDirectory, true);
		ON_SCOPE_EXIT { Database.Clear(); IFileManager::Get().DeleteDirectory(*CacheDirectory, false, true); };

		Database.Clear();
		Database.Classes.Add(ExpectedClassBind); Database.Structs.Add(ExpectedStructBind);
		Database.HeaderLinks.Add(ActorClass, TEXT("Dummy/ActorHeader.h")); Database.HeaderLinks.Add(HitResultStruct, TEXT("Dummy/HitResultHeader.h"));
		Database.Save(CachePath);
		if (!this->Assert.IsTrue(IFileManager::Get().FileExists(*CachePath), TEXT("should write Binds.Cache")) || !this->Assert.IsTrue(IFileManager::Get().FileExists(*HeadersPath), TEXT("should write Binds.Cache.Headers"))) { return; }

		Database.Clear();
		Database.HeaderLinks.Add(ActorClass, SentinelHeaderPath);
		if (!this->Assert.AreEqual(1, Database.HeaderLinks.Num(), TEXT("should stage one sentinel header link")) || !this->Assert.AreEqual(SentinelHeaderPath, Database.HeaderLinks.FindRef(ActorClass), TEXT("should preserve staged sentinel header path"))) { return; }
		if (!this->Assert.IsTrue(IFileManager::Get().Delete(*HeadersPath, false, true), TEXT("should delete .Headers sidecar")) || !this->Assert.IsFalse(IFileManager::Get().FileExists(*HeadersPath), TEXT("should confirm .Headers sidecar missing"))) { return; }

		Database.Load(CachePath, true);
		if (!this->Assert.AreEqual(1, Database.Classes.Num(), TEXT("should restore one class bind")) || !this->Assert.AreEqual(1, Database.Structs.Num(), TEXT("should restore one struct bind"))) { return; }

		const FAngelscriptClassBind& LoadedClassBind = Database.Classes[0];
		const FAngelscriptStructBind& LoadedStructBind = Database.Structs[0];
		bool bOk = true;
		bOk &= this->Assert.AreEqual(ExpectedClassBind.TypeName, LoadedClassBind.TypeName, TEXT("should round-trip class TypeName"));
		bOk &= this->Assert.AreEqual(ExpectedClassBind.UnrealPath, LoadedClassBind.UnrealPath, TEXT("should round-trip class UnrealPath"));
		bOk &= this->Assert.AreEqual(ExpectedClassBind.Methods.Num(), LoadedClassBind.Methods.Num(), TEXT("should round-trip class method count"));
		bOk &= this->Assert.AreEqual(ExpectedClassBind.Properties.Num(), LoadedClassBind.Properties.Num(), TEXT("should round-trip class property count"));
		bOk &= this->Assert.AreEqual(ExpectedStructBind.TypeName, LoadedStructBind.TypeName, TEXT("should round-trip struct TypeName"));
		bOk &= this->Assert.AreEqual(ExpectedStructBind.UnrealPath, LoadedStructBind.UnrealPath, TEXT("should round-trip struct UnrealPath"));
		bOk &= this->Assert.AreEqual(ExpectedStructBind.Properties.Num(), LoadedStructBind.Properties.Num(), TEXT("should round-trip struct property count"));
		bOk &= this->Assert.AreEqual(0, Database.HeaderLinks.Num(), TEXT("should clear stale header links when sidecar missing"));
		bOk &= this->Assert.IsFalse(Database.HeaderLinks.Contains(ActorClass), TEXT("should remove sentinel actor header link when sidecar missing"));
		if (!bOk) { return; }
		if (!ExpectMethodBindEquals(*TestRunner, LoadedClassBind.Methods[0], ExpectedClassBind.Methods[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase missing-sidecar class bind"), LoadedClassBind.Properties[0], ExpectedClassBind.Properties[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase missing-sidecar struct bind"), LoadedStructBind.Properties[0], ExpectedStructBind.Properties[0]))
		{ return; }
	}

	TEST_METHOD(ClearPurgesEnumsDelegatesAndHeaderLinks)
	{
FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!this->Assert.IsTrue(Fixture.IsValid(), TEXT("should acquire an isolated full engine"))) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		UEnum* CollisionEnum = StaticEnum<ECollisionChannel>();
		UDelegateFunction* DelegateFunction = GetSampleDelegateFunction();
		if (!this->Assert.IsNotNull(ActorClass, TEXT("should resolve AActor")) || !this->Assert.IsNotNull(HitResultStruct, TEXT("should resolve FHitResult"))
			|| !this->Assert.IsNotNull(CollisionEnum, TEXT("should resolve ECollisionChannel")) || !this->Assert.IsNotNull(DelegateFunction, TEXT("should resolve sample delegate")))
		{ return; }

		ON_SCOPE_EXIT { Database.Clear(); };
		Database.Clear();
		Database.Classes.Add(MakeSampleClassBind(ActorClass)); Database.Structs.Add(MakeSampleStructBind(HitResultStruct));
		Database.HeaderLinks.Add(ActorClass, TEXT("Dummy/ActorHeader.h")); Database.BoundEnums.Add(CollisionEnum); Database.BoundDelegateFunctions.Add(DelegateFunction);
		if (!this->Assert.AreEqual(1, Database.Classes.Num(), TEXT("should stage one class bind")) || !this->Assert.AreEqual(1, Database.Structs.Num(), TEXT("should stage one struct bind"))
			|| !this->Assert.AreEqual(1, Database.HeaderLinks.Num(), TEXT("should stage one header link")) || !this->Assert.AreEqual(1, Database.BoundEnums.Num(), TEXT("should stage one enum"))
			|| !this->Assert.AreEqual(1, Database.BoundDelegateFunctions.Num(), TEXT("should stage one delegate")))
		{ return; }

		Database.Clear();
		bool bOk = true;
		bOk &= this->Assert.AreEqual(0, Database.Classes.Num(), TEXT("should clear class binds"));
		bOk &= this->Assert.AreEqual(0, Database.Structs.Num(), TEXT("should clear struct binds"));
		bOk &= this->Assert.AreEqual(0, Database.HeaderLinks.Num(), TEXT("should clear header links"));
		bOk &= this->Assert.AreEqual(0, Database.BoundEnums.Num(), TEXT("should clear bound enums"));
		bOk &= this->Assert.AreEqual(0, Database.BoundDelegateFunctions.Num(), TEXT("should clear bound delegate functions"));
		(void)bOk;
	}
};

#endif
