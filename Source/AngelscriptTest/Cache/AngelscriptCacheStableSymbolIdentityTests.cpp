#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_objecttype.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheStableSymbolIdentityTests_Private
{
	static constexpr const char* ModuleName =
		"ASCacheV2StableSymbolIdentity";
	static constexpr const char* Source = R"AS(
struct FStableKeyStruct
{
	int Value;

	int Read() const
	{
		return Value;
	}
}

class FStableKeyClass
{
	int Read() const
	{
		return 7;
	}
}

int GlobalRead()
{
	return 9;
}
)AS";

	static FAngelscriptStableModuleKey BuildModuleKey()
	{
		const TOptional<FAngelscriptStableModuleKey> Key =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("/Angelscript/Game"),
				TEXT("StableSymbolIdentity.as"),
				ANSI_TO_TCHAR(ModuleName));
		check(Key.IsSet());
		return Key.GetValue();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStableSymbolIdentityTests,
	"Angelscript.TestModule.Cache.StableSymbolIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CanonicalKindsAndMalformedFunctionOwnersFailClosed)
	{
		using namespace AngelscriptCacheStableSymbolIdentityTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asIScriptModule* Module = Fixture.BuildModule(
			ModuleName, FString(ANSI_TO_TCHAR(Source)));
		ASSERT_THAT(IsNotNull(Module));

		asCTypeInfo* StructType = static_cast<asCTypeInfo*>(
			Module->GetTypeInfoByDecl("FStableKeyStruct"));
		asCTypeInfo* ClassType = static_cast<asCTypeInfo*>(
			Module->GetTypeInfoByDecl("FStableKeyClass"));
		ASSERT_THAT(IsNotNull(StructType));
		ASSERT_THAT(IsNotNull(ClassType));

		const FAngelscriptStableModuleKey ModuleKey = BuildModuleKey();
		FAngelscriptStableTypeKey StructKey;
		FAngelscriptStableTypeKey ClassKey;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				ModuleKey, *StructType, StructKey)));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				ModuleKey, *ClassType, ClassKey)));

		FAngelscriptTypeIdentityDescriptor ExpectedStruct;
		ExpectedStruct.ModuleKey = ModuleKey;
		ExpectedStruct.Kind = EAngelscriptArtifactEntityKind::Struct;
		ExpectedStruct.Namespace = UTF8_TO_TCHAR(StructType->GetNamespace());
		ExpectedStruct.CanonicalDeclaration = TEXT("struct FStableKeyStruct");
		FAngelscriptTypeIdentityDescriptor ExpectedClass;
		ExpectedClass.ModuleKey = ModuleKey;
		ExpectedClass.Kind = EAngelscriptArtifactEntityKind::Class;
		ExpectedClass.Namespace = UTF8_TO_TCHAR(ClassType->GetNamespace());
		ExpectedClass.CanonicalDeclaration = TEXT("class FStableKeyClass");

		TestRunner->AddInfo(FString::Printf(
			TEXT("Stable type keys: Struct=%s Class=%s StructFlags=0x%x ClassFlags=0x%x"),
			*StructKey.Hash.ToHexString(),
			*ClassKey.Hash.ToHexString(),
			StructType->flags,
			ClassType->flags));
		ASSERT_THAT(IsTrue(StructKey ==
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(ExpectedStruct)));
		ASSERT_THAT(IsTrue(ClassKey ==
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(ExpectedClass)));
		ASSERT_THAT(IsTrue(StructKey != ClassKey));

		asCScriptFunction* ClassMethod = static_cast<asCScriptFunction*>(
			ClassType->GetMethodByDecl("int Read() const"));
		asCScriptFunction* Global = static_cast<asCScriptFunction*>(
			Module->GetFunctionByDecl("int GlobalRead()"));
		asCScriptFunction* Factory = ClassType->GetFactoryCount() != 0
			? static_cast<asCScriptFunction*>(ClassType->GetFactoryByIndex(0))
			: nullptr;
		ASSERT_THAT(IsNotNull(ClassMethod));
		ASSERT_THAT(IsNotNull(Global));
		ASSERT_THAT(IsNotNull(Factory));

		FAngelscriptStableFunctionKey Key;
		FString Failure;
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *ClassMethod, Key, &Failure)));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Global, Key, &Failure)));
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Factory, Key, &Failure)));

		const asEBuildArtifactInvocationKind OriginalKind =
			ClassMethod->artifactInvocationKind;
		asCTypeInfo* const OriginalOwner = ClassMethod->artifactOwnerType;
		ClassMethod->artifactInvocationKind =
			asBUILD_ARTIFACT_INVOCATION_PUBLIC_SINGLE_FUNCTION;
		ASSERT_THAT(IsFalse(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *ClassMethod, Key, &Failure)));
		ASSERT_THAT(IsTrue(Failure.Contains(TEXT("invocation kind"))));

		ClassMethod->artifactInvocationKind = OriginalKind;
		ClassMethod->artifactOwnerType = nullptr;
		ASSERT_THAT(IsFalse(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *ClassMethod, Key, &Failure)));
		ASSERT_THAT(IsTrue(Failure.Contains(TEXT("owner"))));

		ClassMethod->artifactOwnerType = StructType;
		const bool bMismatchedOwnerAccepted =
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *ClassMethod, Key, &Failure);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Stable function owner mismatch: Accepted=%d MethodObject=%s SuppliedOwner=%s Failure=%s"),
			bMismatchedOwnerAccepted ? 1 : 0,
			UTF8_TO_TCHAR(ClassMethod->objectType->GetName()),
			UTF8_TO_TCHAR(StructType->GetName()),
			*Failure));
		ASSERT_THAT(IsFalse(bMismatchedOwnerAccepted));
		ASSERT_THAT(IsTrue(Failure.Contains(TEXT("owner"))));

		ClassMethod->artifactInvocationKind = OriginalKind;
		ClassMethod->artifactOwnerType = OriginalOwner;
		asCTypeInfo* const OriginalFactoryOwner = Factory->artifactOwnerType;
		Factory->artifactOwnerType = StructType;
		ASSERT_THAT(IsFalse(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Factory, Key, &Failure)));
		ASSERT_THAT(IsTrue(Failure.Contains(TEXT("owner"))));
		Factory->artifactOwnerType = OriginalFactoryOwner;

		asCTypeInfo* const OriginalGlobalOwner = Global->artifactOwnerType;
		Global->artifactOwnerType = ClassType;
		ASSERT_THAT(IsFalse(
			FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				ModuleKey, *Global, Key, &Failure)));
		ASSERT_THAT(IsTrue(Failure.Contains(TEXT("type owner"))));
		Global->artifactOwnerType = OriginalGlobalOwner;
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
