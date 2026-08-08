#include "CQTest.h"

#include "AngelscriptTestEngine.h"
#include "Binds/Helper_ToString.h"
#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	static void AppendAuxiliaryMarker(void*, FString& OutString)
	{
		OutString += TEXT("Auxiliary");
	}

	static FString MakeAuxiliaryName()
	{
		return FString::Printf(
			TEXT("FAuxiliary_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}

	static bool ContainsToStringContribution(
		const TArray<FToStringType>& Contributions,
		const FString& TypeName)
	{
		return Contributions.ContainsByPredicate([&TypeName](const FToStringType& Contribution)
		{
			return Contribution.TypeName == TypeName;
		});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptAuxiliaryBindingArchitectureTests,
	"Angelscript.TestModule.Engine.BindingArchitecture.Auxiliary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ToStringContributionRequiresAndUsesAnExplicitTarget)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(
			EngineA.IsValid() && EngineB.IsValid(),
			TEXT("Both auxiliary-store engines should be created")));

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		const FString TypeName = MakeAuxiliaryName();
		const int32 CountA = BindsA.GetTargetToStringList().Num();
		const int32 CountB = BindsB.GetTargetToStringList().Num();

		FToStringHelper::Register(BindsA, TypeName, &AppendAuxiliaryMarker);

		ASSERT_THAT(AreEqual(
			CountA + 1,
			BindsA.GetTargetToStringList().Num(),
			TEXT("The explicit formatter contribution should be stored in engine A")));
		ASSERT_THAT(AreEqual(
			CountB,
			BindsB.GetTargetToStringList().Num(),
			TEXT("The explicit formatter contribution should not mutate engine B")));
		ASSERT_THAT(IsTrue(
			ContainsToStringContribution(BindsA.GetTargetToStringList(), TypeName),
			TEXT("Engine A should retain the exact explicit formatter contribution")));
		ASSERT_THAT(IsFalse(
			ContainsToStringContribution(BindsB.GetTargetToStringList(), TypeName),
			TEXT("Engine B should not observe engine A's formatter contribution")));
	}

	TEST_METHOD(AuxiliaryStoresSurviveOtherEngineTeardownAndReplayFreshState)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineB =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(
			EngineB.IsValid(),
			TEXT("The surviving auxiliary-store engine should be created")));

		FAngelscriptBinds BindsB(*EngineB);
		FAngelscriptTypeDatabase* TypeDatabaseB = &BindsB.GetTargetTypeDatabase();
		FAngelscriptBindDatabase* BindDatabaseB = &BindsB.GetTargetBindDatabase();
		TArray<FToStringType>* ToStringListB = &BindsB.GetTargetToStringList();
		FBlueprintEventSignatureRegistry* InterfaceRegistryB =
			&BindsB.GetTargetBlueprintEventSignatureRegistry();
		asITypeInfo* ArrayTypeB = BindsB.GetTargetTypeDatabase().ArrayTemplateTypeInfo;
		const void* CallbackCollection =
			FAngelscriptBind::GetRegisteredCollectionIdentityForTesting();

		{
			TUniquePtr<FAngelscriptEngine> EngineA =
				FAngelscriptTestEngine::Create(Config, Dependencies);
			ASSERT_THAT(IsTrue(
				EngineA.IsValid(),
				TEXT("The temporary auxiliary-store engine should be created")));
			FAngelscriptBinds BindsA(*EngineA);
			ASSERT_THAT(IsTrue(
				&BindsA.GetTargetTypeDatabase() != TypeDatabaseB
					&& &BindsA.GetTargetBindDatabase() != BindDatabaseB
					&& &BindsA.GetTargetToStringList() != ToStringListB
					&& &BindsA.GetTargetBlueprintEventSignatureRegistry() != InterfaceRegistryB,
				TEXT("Concurrent engines should own distinct auxiliary stores")));
		}

		ASSERT_THAT(IsTrue(
			&BindsB.GetTargetTypeDatabase() == TypeDatabaseB
				&& &BindsB.GetTargetBindDatabase() == BindDatabaseB
				&& &BindsB.GetTargetToStringList() == ToStringListB
				&& &BindsB.GetTargetBlueprintEventSignatureRegistry() == InterfaceRegistryB,
			TEXT("Destroying engine A should not invalidate engine B's auxiliary stores")));

		TUniquePtr<FAngelscriptEngine> EngineC =
			FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(
			EngineC.IsValid(),
			TEXT("The recreated auxiliary-store engine should be created")));
		FAngelscriptBinds BindsC(*EngineC);
		ASSERT_THAT(IsTrue(
			&BindsC.GetTargetTypeDatabase() != TypeDatabaseB
				&& &BindsC.GetTargetBindDatabase() != BindDatabaseB
				&& &BindsC.GetTargetToStringList() != ToStringListB
				&& &BindsC.GetTargetBlueprintEventSignatureRegistry() != InterfaceRegistryB,
			TEXT("A recreated engine should replay into fresh auxiliary stores")));
		ASSERT_THAT(IsTrue(
			ArrayTypeB != nullptr
				&& BindsC.GetTargetTypeDatabase().ArrayTemplateTypeInfo != nullptr
				&& BindsC.GetTargetTypeDatabase().ArrayTemplateTypeInfo != ArrayTypeB,
			TEXT("Well-known array type information should remain engine-owned")));
		ASSERT_THAT(IsTrue(
			FAngelscriptBind::GetRegisteredCollectionIdentityForTesting()
				== CallbackCollection,
			TEXT("All engines should replay the same sealed callback collection")));
	}

	TEST_METHOD(ToStringHelperHasNoAmbientMutationOrFallbackStore)
	{
		const FString RuntimeDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript/Source/AngelscriptRuntime"));
		FString Header;
		FString Implementation;
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(
				Header,
				*FPaths::Combine(RuntimeDirectory, TEXT("Binds/Helper_ToString.h")))
				&& FFileHelper::LoadFileToString(
					Implementation,
					*FPaths::Combine(RuntimeDirectory, TEXT("Binds/Bind_FString.cpp"))),
			TEXT("The ToString helper sources should be readable")));

		ASSERT_THAT(IsFalse(
			Header.Contains(TEXT("Register(const FString& TypeName"))
				|| Header.Contains(TEXT("GetRegisteredTypeCountForTesting"))
				|| Implementation.Contains(TEXT("LegacyToStringList"))
				|| Implementation.Contains(TEXT("TryGetCurrentEngine()")),
			TEXT("ToString binding mutations should require an explicit Binds target")));
	}
};

#endif
