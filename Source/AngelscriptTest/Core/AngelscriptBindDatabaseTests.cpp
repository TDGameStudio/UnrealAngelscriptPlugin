#include "AngelscriptBindDatabase.h"
#include "AngelscriptEngine.h"
#include "Shared/AngelscriptTestUtilities.h"

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

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Core_AngelscriptBindDatabaseTests_Private
{
	struct FBindDatabaseContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;
		FBindDatabaseContextStackGuard() { SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear(); }
		~FBindDatabaseContextStackGuard() { FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack)); }
		void DiscardSavedStack() { SavedStack.Reset(); }
	};

	FString MakeBindDatabaseAutomationDirectory()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("BindDatabase"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	FAngelscriptPropertyBind MakeSamplePropertyBind(const FString& Declaration, const FString& UnrealPath)
	{
		FAngelscriptPropertyBind Bind;
		Bind.Declaration = Declaration; Bind.UnrealPath = UnrealPath;
		Bind.bCanWrite = true; Bind.bCanRead = true; Bind.bCanEdit = false;
		return Bind;
	}

	FAngelscriptMethodBind MakeSampleMethodBind()
	{
		FAngelscriptMethodBind Bind;
		Bind.Declaration = TEXT("void DestroyActor()"); Bind.UnrealPath = TEXT("/Script/Engine.Actor:K2_DestroyActor");
		Bind.ClassName = TEXT("AActor"); Bind.ScriptName = TEXT("DestroyActor");
		Bind.WorldContextArgument = 1; Bind.DeterminesOutputTypeArgument = -1;
		Bind.bStaticInUnreal = false; Bind.bStaticInScript = true; Bind.bGlobalScope = false;
		Bind.bNotAngelscriptProperty = true; Bind.bTrivial = true;
		return Bind;
	}

	FAngelscriptClassBind MakeSampleClassBind(UClass* Class)
	{
		FAngelscriptClassBind Bind;
		Bind.TypeName = TEXT("AActor"); Bind.UnrealPath = Class->GetPathName();
		Bind.Methods.Add(MakeSampleMethodBind());
		Bind.Properties.Add(MakeSamplePropertyBind(TEXT("float InitialLifeSpan"), TEXT("/Script/Engine.Actor:InitialLifeSpan")));
		return Bind;
	}

	FAngelscriptClassBind MakeNamedSampleClassBind(UClass* Class, const FString& TypeName)
	{
		FAngelscriptClassBind Bind = MakeSampleClassBind(Class); Bind.TypeName = TypeName; return Bind;
	}

	FAngelscriptStructBind MakeSampleStructBind(UScriptStruct* Struct)
	{
		FAngelscriptStructBind Bind;
		Bind.TypeName = TEXT("FHitResult"); Bind.UnrealPath = Struct->GetPathName();
		Bind.Properties.Add(MakeSamplePropertyBind(TEXT("bool bBlockingHit"), TEXT("/Script/Engine.HitResult:bBlockingHit")));
		return Bind;
	}

	void WriteLegacyUnversionedCache(const FString& CachePath, const FAngelscriptClassBind& ClassBind, const FAngelscriptStructBind& StructBind)
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

	bool ExpectPropertyBindEquals(FAutomationTestBase& Test, const TCHAR* Context, const FAngelscriptPropertyBind& Actual, const FAngelscriptPropertyBind& Expected)
	{
		bool bOk = true;
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s should round-trip property declaration"), Context), Actual.Declaration, Expected.Declaration);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s should round-trip property UnrealPath"), Context), Actual.UnrealPath, Expected.UnrealPath);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s should round-trip property bCanWrite"), Context), Actual.bCanWrite, Expected.bCanWrite);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s should round-trip property bCanRead"), Context), Actual.bCanRead, Expected.bCanRead);
		bOk &= Test.TestEqual(*FString::Printf(TEXT("%s should round-trip property bCanEdit"), Context), Actual.bCanEdit, Expected.bCanEdit);
		return bOk;
	}

	bool ExpectMethodBindEquals(FAutomationTestBase& Test, const FAngelscriptMethodBind& Actual, const FAngelscriptMethodBind& Expected)
	{
		bool bOk = true;
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method declaration"), Actual.Declaration, Expected.Declaration);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method UnrealPath"), Actual.UnrealPath, Expected.UnrealPath);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method ClassName"), Actual.ClassName, Expected.ClassName);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method ScriptName"), Actual.ScriptName, Expected.ScriptName);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method WorldContextArgument"), static_cast<int32>(Actual.WorldContextArgument), static_cast<int32>(Expected.WorldContextArgument));
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method DeterminesOutputTypeArgument"), static_cast<int32>(Actual.DeterminesOutputTypeArgument), static_cast<int32>(Expected.DeterminesOutputTypeArgument));
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method bStaticInUnreal"), Actual.bStaticInUnreal, Expected.bStaticInUnreal);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method bStaticInScript"), Actual.bStaticInScript, Expected.bStaticInScript);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method bGlobalScope"), Actual.bGlobalScope, Expected.bGlobalScope);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method bNotAngelscriptProperty"), Actual.bNotAngelscriptProperty, Expected.bNotAngelscriptProperty);
		bOk &= Test.TestEqual(TEXT("BindDatabase round-trip should preserve method bTrivial"), Actual.bTrivial, Expected.bTrivial);
		return bOk;
	}

	bool DatabaseContainsClassBindNamed(const FAngelscriptBindDatabase& Database, const FString& TypeName)
	{
		return Database.Classes.ContainsByPredicate([&TypeName](const FAngelscriptClassBind& Bind) { return Bind.TypeName == TypeName; });
	}

	UDelegateFunction* GetSampleDelegateFunction()
	{
		const FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(AActor::StaticClass(), GET_MEMBER_NAME_CHECKED(AActor, OnActorBeginOverlap));
		return DelegateProperty != nullptr ? Cast<UDelegateFunction>(DelegateProperty->SignatureFunction) : nullptr;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBindDatabaseTests, "Angelscript.TestModule.Engine.BindDatabase", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SaveLoadRoundTripsClassesAndHeaders)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindDatabaseTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!TestRunner->TestTrue(TEXT("BindDatabase.SaveLoadRoundTripsClassesAndHeaders should acquire an isolated full engine"), Fixture.IsValid())) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase* LocalDatabase = Engine.GetBindDatabase();
		if (!TestRunner->TestNotNull(TEXT("BindDatabase.SaveLoadRoundTripsClassesAndHeaders should expose an engine-local bind database"), LocalDatabase)) { return; }
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();
		if (!TestRunner->TestTrue(TEXT("BindDatabase.SaveLoadRoundTripsClassesAndHeaders should resolve GetBindDatabaseForTesting through the scoped engine"), &Database == LocalDatabase)) { return; }

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		if (!TestRunner->TestNotNull(TEXT("should resolve AActor"), ActorClass) || !TestRunner->TestNotNull(TEXT("should resolve FHitResult"), HitResultStruct)) { return; }

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
		if (!TestRunner->TestTrue(TEXT("should write Binds.Cache"), IFileManager::Get().FileExists(*CachePath)) || !TestRunner->TestTrue(TEXT("should write Binds.Cache.Headers"), IFileManager::Get().FileExists(*HeadersPath))) { return; }

		TArray<uint8> CacheBytes;
		if (!TestRunner->TestTrue(TEXT("should read Binds.Cache bytes"), FFileHelper::LoadFileToArray(CacheBytes, *CachePath))) { return; }
		FMemoryReader CacheReader(CacheBytes);
		uint32 CacheMagic = 0;
		int32 CacheVersion = 0;
		CacheReader << CacheMagic;
		CacheReader << CacheVersion;
		if (!TestRunner->TestEqual(TEXT("should write bind database magic header"), CacheMagic, FAngelscriptBindDatabase::CacheMagic)
			|| !TestRunner->TestEqual(TEXT("should write current bind database version"), CacheVersion, FAngelscriptBindDatabase::CacheVersion))
		{
			return;
		}

		Database.Clear();
		if (!TestRunner->TestEqual(TEXT("should clear class binds"), Database.Classes.Num(), 0) || !TestRunner->TestEqual(TEXT("should clear struct binds"), Database.Structs.Num(), 0) || !TestRunner->TestEqual(TEXT("should clear header links"), Database.HeaderLinks.Num(), 0)) { return; }

		Database.Load(CachePath, false);
		if (!TestRunner->TestEqual(TEXT("should restore one class bind"), Database.Classes.Num(), 1) || !TestRunner->TestEqual(TEXT("should restore one struct bind"), Database.Structs.Num(), 1) || !TestRunner->TestEqual(TEXT("should keep header links empty without precompiled"), Database.HeaderLinks.Num(), 0)) { return; }

		const FAngelscriptClassBind& LoadedClassBind = Database.Classes[0];
		const FAngelscriptStructBind& LoadedStructBind = Database.Structs[0];
		bool bOk = true;
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class TypeName"), LoadedClassBind.TypeName, ExpectedClassBind.TypeName);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class UnrealPath"), LoadedClassBind.UnrealPath, ExpectedClassBind.UnrealPath);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class method count"), LoadedClassBind.Methods.Num(), ExpectedClassBind.Methods.Num());
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class property count"), LoadedClassBind.Properties.Num(), ExpectedClassBind.Properties.Num());
		bOk &= TestRunner->TestEqual(TEXT("should round-trip struct TypeName"), LoadedStructBind.TypeName, ExpectedStructBind.TypeName);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip struct UnrealPath"), LoadedStructBind.UnrealPath, ExpectedStructBind.UnrealPath);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip struct property count"), LoadedStructBind.Properties.Num(), ExpectedStructBind.Properties.Num());
		if (!bOk) { return; }

		if (!ExpectMethodBindEquals(*TestRunner, LoadedClassBind.Methods[0], ExpectedClassBind.Methods[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase class bind"), LoadedClassBind.Properties[0], ExpectedClassBind.Properties[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase struct bind"), LoadedStructBind.Properties[0], ExpectedStructBind.Properties[0]))
		{ return; }

		Database.Clear();
		Database.Load(CachePath, true);
		if (!TestRunner->TestTrue(TEXT("should populate class header links with precompiled"), Database.HeaderLinks.Contains(ActorClass)) ||
			!TestRunner->TestTrue(TEXT("should populate struct header links with precompiled"), Database.HeaderLinks.Contains(HitResultStruct)))
		{ return; }

		const FString ActorHeader = Database.HeaderLinks.FindRef(ActorClass);
		const FString StructHeader = Database.HeaderLinks.FindRef(HitResultStruct);
		TestRunner->TestFalse(TEXT("should load a non-empty header for AActor"), ActorHeader.IsEmpty());
		TestRunner->TestFalse(TEXT("should load a non-empty header for FHitResult"), StructHeader.IsEmpty());
		TestRunner->TestTrue(TEXT("should load an existing header path for AActor"), IFileManager::Get().FileExists(*ActorHeader));
		TestRunner->TestTrue(TEXT("should load an existing header path for FHitResult"), IFileManager::Get().FileExists(*StructHeader));
	}

	TEST_METHOD(RejectsLegacyUnversionedCache)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindDatabaseTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!TestRunner->TestTrue(TEXT("should acquire an isolated full engine"), Fixture.IsValid())) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		if (!TestRunner->TestNotNull(TEXT("should resolve AActor"), ActorClass) || !TestRunner->TestNotNull(TEXT("should resolve FHitResult"), HitResultStruct)) { return; }

		const FString CacheDirectory = MakeBindDatabaseAutomationDirectory();
		const FString CachePath = FPaths::Combine(CacheDirectory, TEXT("Binds.Cache"));
		IFileManager::Get().MakeDirectory(*CacheDirectory, true);
		ON_SCOPE_EXIT { Database.Clear(); IFileManager::Get().DeleteDirectory(*CacheDirectory, false, true); };

		Database.Clear();
		WriteLegacyUnversionedCache(CachePath, MakeSampleClassBind(ActorClass), MakeSampleStructBind(HitResultStruct));
		Database.Classes.Add(MakeNamedSampleClassBind(ActorClass, TEXT("BindDatabaseSentinelBeforeRejectedLoad")));

		FString LoadError;
		const bool bLoaded = Database.TryLoad(CachePath, false, &LoadError);
		TestRunner->TestFalse(TEXT("legacy unversioned Binds.Cache should be rejected"), bLoaded);
		TestRunner->TestTrue(TEXT("legacy rejection should explain cache regeneration"), LoadError.Contains(TEXT("regenerate Script/Binds.Cache")));
		TestRunner->TestEqual(TEXT("failed legacy load should preserve existing class binds"), Database.Classes.Num(), 1);
		TestRunner->TestTrue(TEXT("failed legacy load should preserve existing sentinel bind"), DatabaseContainsClassBindNamed(Database, TEXT("BindDatabaseSentinelBeforeRejectedLoad")));
	}

	TEST_METHOD(GetPrefersCurrentEngineSharedDatabaseAndFallsBackToLegacySingleton)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindDatabaseTests_Private;
		FBindDatabaseContextStackGuard ContextGuard;
		DestroySharedTestEngine();
		if (FAngelscriptEngine::IsInitialized()) { FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine(); }
		ContextGuard.DiscardSavedStack();

		static const FString LegacySentinelTypeName(TEXT("BindDatabaseLegacySentinel"));
		static const FString EngineASentinelTypeName(TEXT("BindDatabaseEngineASentinel"));
		FAngelscriptBindDatabase* LegacyDatabase = &FAngelscriptBindDatabase::Get();
		if (!TestRunner->TestNotNull(TEXT("should expose a legacy database without a current engine"), LegacyDatabase)) { return; }

		ON_SCOPE_EXIT { LegacyDatabase->Clear(); if (FAngelscriptEngine::IsInitialized()) { FAngelscriptTestEngineScopeAccess::DestroyGlobalEngine(); } DestroySharedTestEngine(); };

		LegacyDatabase->Clear();
		LegacyDatabase->Classes.Add(MakeNamedSampleClassBind(AActor::StaticClass(), LegacySentinelTypeName));
		FAngelscriptBindDatabase* LegacyDatabaseSecondRead = &FAngelscriptBindDatabase::Get();

		bool bOk = true;
		bOk &= TestRunner->TestNull(TEXT("should start without a current engine"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine());
		bOk &= TestRunner->TestTrue(TEXT("should reuse the same legacy singleton"), LegacyDatabaseSecondRead == LegacyDatabase);
		bOk &= TestRunner->TestTrue(TEXT("should preserve legacy sentinel data"), DatabaseContainsClassBindNamed(*LegacyDatabase, LegacySentinelTypeName));

		TUniquePtr<FAngelscriptEngine> EngineA = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("should create engine A"), EngineA.Get())) { return; }
		FAngelscriptBindDatabase* EngineADatabaseFromGet = nullptr;
		FAngelscriptBindDatabase* EngineADirectDatabase = nullptr;

		{
			FAngelscriptEngineScope ScopeA(*EngineA);
			EngineADirectDatabase = EngineA->GetBindDatabase();
			EngineADatabaseFromGet = &FAngelscriptBindDatabase::Get();
			FAngelscriptBindDatabase& EngineADatabaseFromTesting = EngineA->GetBindDatabaseForTesting();
			bOk &= TestRunner->TestNotNull(TEXT("should expose an engine-owned bind database"), EngineADirectDatabase);
			bOk &= TestRunner->TestTrue(TEXT("should prefer current engine bind database"), EngineADatabaseFromGet == EngineADirectDatabase);
			bOk &= TestRunner->TestTrue(TEXT("should align GetBindDatabaseForTesting"), &EngineADatabaseFromTesting == EngineADirectDatabase);
			bOk &= TestRunner->TestTrue(TEXT("should not alias legacy singleton"), EngineADirectDatabase != LegacyDatabase);

			EngineADirectDatabase->Clear();
			EngineADirectDatabase->Classes.Add(MakeNamedSampleClassBind(AActor::StaticClass(), EngineASentinelTypeName));
			bOk &= TestRunner->TestTrue(TEXT("should keep engine A sentinel in engine-owned database"), DatabaseContainsClassBindNamed(*EngineADirectDatabase, EngineASentinelTypeName));
			bOk &= TestRunner->TestFalse(TEXT("should keep engine A sentinel out of legacy singleton"), DatabaseContainsClassBindNamed(*LegacyDatabase, EngineASentinelTypeName));

			// Clone-shares-database sub-test removed alongside the Clone
			// engine mechanism; independent Full engines have independent
			// bind databases, which is covered by the engine A vs C
			// recreation block below.
		}

		EngineA.Reset();
		bOk &= TestRunner->TestNull(TEXT("should restore no-current-engine baseline after destroying engine A"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine());

		TUniquePtr<FAngelscriptEngine> EngineC = CreateFullTestEngine();
		if (!TestRunner->TestNotNull(TEXT("should create engine C"), EngineC.Get())) { return; }
		{
			FAngelscriptEngineScope ScopeC(*EngineC);
			FAngelscriptBindDatabase* EngineCDirectDatabase = EngineC->GetBindDatabase();
			FAngelscriptBindDatabase* EngineCDatabaseFromGet = &FAngelscriptBindDatabase::Get();
			bOk &= TestRunner->TestNotNull(TEXT("should expose bind database for engine C"), EngineCDirectDatabase);
			bOk &= TestRunner->TestTrue(TEXT("Get() should route through engine C database"), EngineCDatabaseFromGet == EngineCDirectDatabase);
			bOk &= TestRunner->TestTrue(TEXT("should allocate fresh bind database for recreated full engine"), EngineCDirectDatabase != EngineADatabaseFromGet);
			bOk &= TestRunner->TestFalse(TEXT("should not leak engine A sentinel into recreated engine C"), DatabaseContainsClassBindNamed(*EngineCDirectDatabase, EngineASentinelTypeName));
		}
		EngineC.Reset();

		bOk &= TestRunner->TestNull(TEXT("should end without a current engine"), FAngelscriptTestEngineScopeAccess::GetCurrentEngine());
		bOk &= TestRunner->TestTrue(TEXT("should fall back to legacy singleton after all engines gone"), &FAngelscriptBindDatabase::Get() == LegacyDatabase);
		bOk &= TestRunner->TestTrue(TEXT("should preserve legacy sentinel after scoped engine lifetimes end"), DatabaseContainsClassBindNamed(*LegacyDatabase, LegacySentinelTypeName));
	}

	TEST_METHOD(LoadWithoutHeadersSidecarLeavesHeaderLinksEmptyButRestoresBinds)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindDatabaseTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!TestRunner->TestTrue(TEXT("should acquire an isolated full engine"), Fixture.IsValid())) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		if (!TestRunner->TestNotNull(TEXT("should resolve AActor"), ActorClass) || !TestRunner->TestNotNull(TEXT("should resolve FHitResult"), HitResultStruct)) { return; }

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
		if (!TestRunner->TestTrue(TEXT("should write Binds.Cache"), IFileManager::Get().FileExists(*CachePath)) || !TestRunner->TestTrue(TEXT("should write Binds.Cache.Headers"), IFileManager::Get().FileExists(*HeadersPath))) { return; }

		Database.Clear();
		Database.HeaderLinks.Add(ActorClass, SentinelHeaderPath);
		if (!TestRunner->TestEqual(TEXT("should stage one sentinel header link"), Database.HeaderLinks.Num(), 1) || !TestRunner->TestEqual(TEXT("should preserve staged sentinel header path"), Database.HeaderLinks.FindRef(ActorClass), SentinelHeaderPath)) { return; }
		if (!TestRunner->TestTrue(TEXT("should delete .Headers sidecar"), IFileManager::Get().Delete(*HeadersPath, false, true)) || !TestRunner->TestFalse(TEXT("should confirm .Headers sidecar missing"), IFileManager::Get().FileExists(*HeadersPath))) { return; }

		Database.Load(CachePath, true);
		if (!TestRunner->TestEqual(TEXT("should restore one class bind"), Database.Classes.Num(), 1) || !TestRunner->TestEqual(TEXT("should restore one struct bind"), Database.Structs.Num(), 1)) { return; }

		const FAngelscriptClassBind& LoadedClassBind = Database.Classes[0];
		const FAngelscriptStructBind& LoadedStructBind = Database.Structs[0];
		bool bOk = true;
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class TypeName"), LoadedClassBind.TypeName, ExpectedClassBind.TypeName);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class UnrealPath"), LoadedClassBind.UnrealPath, ExpectedClassBind.UnrealPath);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class method count"), LoadedClassBind.Methods.Num(), ExpectedClassBind.Methods.Num());
		bOk &= TestRunner->TestEqual(TEXT("should round-trip class property count"), LoadedClassBind.Properties.Num(), ExpectedClassBind.Properties.Num());
		bOk &= TestRunner->TestEqual(TEXT("should round-trip struct TypeName"), LoadedStructBind.TypeName, ExpectedStructBind.TypeName);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip struct UnrealPath"), LoadedStructBind.UnrealPath, ExpectedStructBind.UnrealPath);
		bOk &= TestRunner->TestEqual(TEXT("should round-trip struct property count"), LoadedStructBind.Properties.Num(), ExpectedStructBind.Properties.Num());
		bOk &= TestRunner->TestEqual(TEXT("should clear stale header links when sidecar missing"), Database.HeaderLinks.Num(), 0);
		bOk &= TestRunner->TestFalse(TEXT("should remove sentinel actor header link when sidecar missing"), Database.HeaderLinks.Contains(ActorClass));
		if (!bOk) { return; }
		if (!ExpectMethodBindEquals(*TestRunner, LoadedClassBind.Methods[0], ExpectedClassBind.Methods[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase missing-sidecar class bind"), LoadedClassBind.Properties[0], ExpectedClassBind.Properties[0]) ||
			!ExpectPropertyBindEquals(*TestRunner, TEXT("BindDatabase missing-sidecar struct bind"), LoadedStructBind.Properties[0], ExpectedStructBind.Properties[0]))
		{ return; }
	}

	TEST_METHOD(ClearPurgesEnumsDelegatesAndHeaderLinks)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindDatabaseTests_Private;
		FAngelscriptTestFixture Fixture(*TestRunner, ETestEngineMode::IsolatedFull);
		if (!TestRunner->TestTrue(TEXT("should acquire an isolated full engine"), Fixture.IsValid())) { return; }
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		FAngelscriptBindDatabase& Database = Engine.GetBindDatabaseForTesting();

		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* HitResultStruct = TBaseStructure<FHitResult>::Get();
		UEnum* CollisionEnum = StaticEnum<ECollisionChannel>();
		UDelegateFunction* DelegateFunction = GetSampleDelegateFunction();
		if (!TestRunner->TestNotNull(TEXT("should resolve AActor"), ActorClass) || !TestRunner->TestNotNull(TEXT("should resolve FHitResult"), HitResultStruct)
			|| !TestRunner->TestNotNull(TEXT("should resolve ECollisionChannel"), CollisionEnum) || !TestRunner->TestNotNull(TEXT("should resolve sample delegate"), DelegateFunction))
		{ return; }

		ON_SCOPE_EXIT { Database.Clear(); };
		Database.Clear();
		Database.Classes.Add(MakeSampleClassBind(ActorClass)); Database.Structs.Add(MakeSampleStructBind(HitResultStruct));
		Database.HeaderLinks.Add(ActorClass, TEXT("Dummy/ActorHeader.h")); Database.BoundEnums.Add(CollisionEnum); Database.BoundDelegateFunctions.Add(DelegateFunction);
		if (!TestRunner->TestEqual(TEXT("should stage one class bind"), Database.Classes.Num(), 1) || !TestRunner->TestEqual(TEXT("should stage one struct bind"), Database.Structs.Num(), 1)
			|| !TestRunner->TestEqual(TEXT("should stage one header link"), Database.HeaderLinks.Num(), 1) || !TestRunner->TestEqual(TEXT("should stage one enum"), Database.BoundEnums.Num(), 1)
			|| !TestRunner->TestEqual(TEXT("should stage one delegate"), Database.BoundDelegateFunctions.Num(), 1))
		{ return; }

		Database.Clear();
		TestRunner->TestEqual(TEXT("should clear class binds"), Database.Classes.Num(), 0);
		TestRunner->TestEqual(TEXT("should clear struct binds"), Database.Structs.Num(), 0);
		TestRunner->TestEqual(TEXT("should clear header links"), Database.HeaderLinks.Num(), 0);
		TestRunner->TestEqual(TEXT("should clear bound enums"), Database.BoundEnums.Num(), 0);
		TestRunner->TestEqual(TEXT("should clear bound delegate functions"), Database.BoundDelegateFunctions.Num(), 0);
	}
};

#endif
