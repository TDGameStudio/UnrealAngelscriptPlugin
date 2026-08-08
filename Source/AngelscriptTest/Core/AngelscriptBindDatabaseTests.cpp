#include "AngelscriptBindDatabase.h"
#include "AngelscriptEngine.h"
#include "AngelscriptTestUtilities.h"

#include "CQTest.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
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

static bool LoadBindDatabaseRuntimeSource(const TCHAR* RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(
		OutSource,
		*FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript/Source/AngelscriptRuntime"),
			RelativePath));
}

static bool ContainsAmbientSourceHeaderLookup(const FString& Source)
{
	const FRegexPattern OneArgumentLookup(
		TEXT("FAngelscriptBindDatabase::GetSourceHeader\\([^,\\r\\n]*\\)"));
	FRegexMatcher Matcher(OneArgumentLookup, Source);
	return Matcher.FindNext();
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
	TEST_METHOD(TestingAccessorUsesOwnedDatabaseWithoutAmbientEngine)
	{
		FScopedAngelscriptEngineResolutionSuppressionForTesting NoCurrentEngineScope;
		FBindDatabaseContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized())
		{
			FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
		}
		ContextGuard.DiscardSavedStack();
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = CreateFullTestEngine();
		ASSERT_THAT(IsTrue(
			EngineA.IsValid() && EngineB.IsValid(),
			TEXT("BindDatabase explicit-access test should create two independent engines")));
		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("BindDatabase explicit-access test should not have an ambient engine")));
		ASSERT_THAT(IsTrue(
			&EngineA->GetBindDatabaseForTesting() == EngineA->GetBindDatabase(),
			TEXT("Engine A testing accessor should return its owned database without ambient resolution")));
		ASSERT_THAT(IsTrue(
			&EngineB->GetBindDatabaseForTesting() == EngineB->GetBindDatabase(),
			TEXT("Engine B testing accessor should return its owned database without ambient resolution")));
		ASSERT_THAT(IsTrue(
			&EngineA->GetBindDatabaseForTesting() != &EngineB->GetBindDatabaseForTesting(),
			TEXT("Independent engines should expose distinct owned bind databases")));
	}

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

	TEST_METHOD(GetRoutesOnlyThroughScopedEngineDatabases)
	{
		FScopedAngelscriptEngineResolutionSuppressionForTesting NoCurrentEngineScope;
		FBindDatabaseContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine(); }
		ContextGuard.DiscardSavedStack();

		static const FString EngineASentinelTypeName(TEXT("BindDatabaseEngineASentinel"));
		static const FString EngineBSentinelTypeName(TEXT("BindDatabaseEngineBSentinel"));
		ON_SCOPE_EXIT
		{
			if (FAngelscriptEngine::IsInitialized())
			{
				FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine();
			}
			DestroySharedTestEngine();
		};

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		TUniquePtr<FAngelscriptEngine> EngineB = CreateFullTestEngine();
		ASSERT_THAT(IsTrue(
			EngineA.IsValid() && EngineB.IsValid(),
			TEXT("Scoped BindDatabase routing should create two independent engines")));
		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("Scoped BindDatabase routing should begin without an ambient engine")));

		FAngelscriptBindDatabase* EngineADatabase = EngineA->GetBindDatabase();
		FAngelscriptBindDatabase* EngineBDatabase = EngineB->GetBindDatabase();
		ASSERT_THAT(IsNotNull(EngineADatabase, TEXT("Engine A should own a bind database")));
		ASSERT_THAT(IsNotNull(EngineBDatabase, TEXT("Engine B should own a bind database")));
		ASSERT_THAT(AreNotEqual(
			EngineADatabase,
			EngineBDatabase,
			TEXT("Independent engines should never share a bind database")));

		EngineADatabase->Clear();
		EngineBDatabase->Clear();
		EngineADatabase->Classes.Add(
			MakeNamedSampleClassBind(AActor::StaticClass(), EngineASentinelTypeName));
		EngineBDatabase->Classes.Add(
			MakeNamedSampleClassBind(AActor::StaticClass(), EngineBSentinelTypeName));

		{
			FAngelscriptEngineScope ScopeA(*EngineA);
			ASSERT_THAT(IsTrue(
				&FAngelscriptBindDatabase::Get() == EngineADatabase,
				TEXT("Get should resolve Engine A's owned database inside ScopeA")));
			ASSERT_THAT(IsTrue(
				DatabaseContainsClassBindNamed(*EngineADatabase, EngineASentinelTypeName)
					&& !DatabaseContainsClassBindNamed(*EngineADatabase, EngineBSentinelTypeName),
				TEXT("Engine A should expose only its own sentinel")));
		}

		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			ASSERT_THAT(IsTrue(
				&FAngelscriptBindDatabase::Get() == EngineBDatabase,
				TEXT("Get should resolve Engine B's owned database inside ScopeB")));
			ASSERT_THAT(IsTrue(
				DatabaseContainsClassBindNamed(*EngineBDatabase, EngineBSentinelTypeName)
					&& !DatabaseContainsClassBindNamed(*EngineBDatabase, EngineASentinelTypeName),
				TEXT("Engine B should expose only its own sentinel")));
		}

		ASSERT_THAT(IsNull(
			FAngelscriptEngine::TryGetCurrentEngine(),
			TEXT("Scoped BindDatabase routing should restore the no-current-engine baseline")));
	}

	TEST_METHOD(LegacySingletonIsAbsentAndSourceHeaderConsumersUseExplicitDatabases)
	{
		FString DatabaseHeader;
		FString DatabaseImplementation;
		FString OfflineMetadata;
		ASSERT_THAT(IsTrue(
			LoadBindDatabaseRuntimeSource(TEXT("Core/AngelscriptBindDatabase.h"), DatabaseHeader),
			TEXT("BindDatabase header should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindDatabaseRuntimeSource(TEXT("Core/AngelscriptBindDatabase.cpp"), DatabaseImplementation),
			TEXT("BindDatabase implementation should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindDatabaseRuntimeSource(TEXT("Dump/AngelscriptOfflineSymbolMetadata.cpp"), OfflineMetadata),
			TEXT("Offline symbol metadata implementation should be readable")));

		ASSERT_THAT(IsFalse(
			DatabaseImplementation.Contains(TEXT("LegacyBindDatabase")),
			TEXT("BindDatabase Get must not retain a process-wide fallback singleton")));
		ASSERT_THAT(IsTrue(
			DatabaseImplementation.Contains(TEXT("FAngelscriptEngine::TryGetCurrentEngine()"))
				&& DatabaseImplementation.Contains(TEXT("checkf(CurrentEngine != nullptr"))
				&& DatabaseImplementation.Contains(TEXT("checkf(BindDatabase != nullptr")),
			TEXT("BindDatabase Get should fail loudly when current-engine resolution is invalid")));
		ASSERT_THAT(IsTrue(
			DatabaseHeader.Contains(
				TEXT("GetSourceHeader(UField* Field, const FAngelscriptBindDatabase& Database)")),
			TEXT("GetSourceHeader should expose an explicit database overload")));
		ASSERT_THAT(IsTrue(
			OfflineMetadata.Contains(TEXT("FAngelscriptEngine::TryGetCurrentEngine()"))
				&& OfflineMetadata.Contains(TEXT("CurrentEngine->GetBindDatabase()"))
				&& !OfflineMetadata.Contains(TEXT("FAngelscriptBindDatabase::Get()")),
			TEXT("Offline symbol metadata should read the existing current engine database explicitly")));

		static const TCHAR* BindProviderFiles[] = {
			TEXT("Bind_BlueprintType.cpp"),
			TEXT("Bind_Delegates.cpp"),
			TEXT("Bind_TSoftObjectPtr.cpp"),
			TEXT("Bind_UEnum.cpp"),
			TEXT("Bind_UStruct.cpp"),
		};
		for (const TCHAR* BindProviderFile : BindProviderFiles)
		{
			FString ProviderSource;
			ASSERT_THAT(IsTrue(
				LoadBindDatabaseRuntimeSource(
					*FString::Printf(TEXT("Binds/%s"), BindProviderFile),
					ProviderSource),
				FString::Printf(TEXT("%s should be readable"), BindProviderFile)));
			ASSERT_THAT(IsFalse(
				ContainsAmbientSourceHeaderLookup(ProviderSource),
				FString::Printf(
					TEXT("%s should not resolve source headers through the ambient database"),
					BindProviderFile)));
			ASSERT_THAT(IsTrue(
				ProviderSource.Contains(TEXT("Binds.GetTargetBindDatabase()")),
				FString::Printf(
					TEXT("%s should capture its explicit target database for source-header lookup"),
					BindProviderFile)));
		}
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
