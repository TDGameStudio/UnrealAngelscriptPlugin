#include "CQTest.h"

#include "Cache/AngelscriptCacheCompilerBridge.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionInputAuthorityTests_Private
{
	static FAngelscriptHash256 MakeHash(const uint8 Seed)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (int32 Index = 0; Index < static_cast<int32>(sizeof(Bytes)); ++Index)
		{
			Bytes[Index] = static_cast<uint8>(Seed + Index);
		}
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheSemanticDependency MakeContentDependency(
		const EAngelscriptCacheSemanticDependencyKind Kind,
		const EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey,
		const FAngelscriptHash256& DeclarationAbi,
		const FAngelscriptHash256& Content)
	{
		return {
			Kind,
			{ReferenceKind, StableKey, DeclarationAbi},
			Content,
		};
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionInputAuthorityTests,
	"Angelscript.TestModule.Cache.FunctionInputAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(LayoutAndStorageDependenciesUseDeclarationAbiPlusContentWitness)
	{
		using namespace AngelscriptCacheFunctionInputAuthorityTests_Private;
		const FAngelscriptHash256 TypeKey = MakeHash(0x10);
		const FAngelscriptHash256 TypeAbi = MakeHash(0x20);
		const FAngelscriptHash256 TypeLayout = MakeHash(0x30);
		const FAngelscriptHash256 PropertyKey = MakeHash(0x40);
		const FAngelscriptHash256 PropertyAbi = MakeHash(0x50);
		const FAngelscriptHash256 PropertyLayout = MakeHash(0x60);
		const FAngelscriptHash256 GlobalKey = MakeHash(0x70);
		const FAngelscriptHash256 GlobalAbi = MakeHash(0x80);
		const FAngelscriptHash256 GlobalStorage = MakeHash(0x90);

		FAngelscriptCachedModuleInterface Interface;
		Interface.ModuleKey.Hash = MakeHash(0xa0);
		Interface.Declarations.Reserve(3);
		FAngelscriptCachedDeclaration& TypeDeclaration =
			Interface.Declarations.AddDefaulted_GetRef();
		TypeDeclaration.DeclarationKind =
			EAngelscriptCacheDeclarationKind::Type;
		TypeDeclaration.StableKey = TypeKey;
		TypeDeclaration.SignatureHash = TypeAbi;
		FAngelscriptCachedDeclaration& PropertyDeclaration =
			Interface.Declarations.AddDefaulted_GetRef();
		PropertyDeclaration.DeclarationKind =
			EAngelscriptCacheDeclarationKind::Property;
		PropertyDeclaration.StableKey = PropertyKey;
		PropertyDeclaration.SignatureHash = PropertyAbi;
		FAngelscriptCachedDeclaration& GlobalDeclaration =
			Interface.Declarations.AddDefaulted_GetRef();
		GlobalDeclaration.DeclarationKind =
			EAngelscriptCacheDeclarationKind::Global;
		GlobalDeclaration.StableKey = GlobalKey;
		GlobalDeclaration.SignatureHash = GlobalAbi;

		FAngelscriptCachedTypeSchema TypeSchema;
		TypeSchema.TypeKey.Hash = TypeKey;
		TypeSchema.Layout.TypeLayoutHash = TypeLayout;
		FAngelscriptCachedPropertySchema& Property =
			TypeSchema.OrderedProperties.AddDefaulted_GetRef();
		Property.PropertyKey.Hash = PropertyKey;
		Property.PropertyLayoutFingerprint = PropertyLayout;

		FAngelscriptCachedModuleState State;
		State.ModuleKey = Interface.ModuleKey;
		FAngelscriptCachedGlobalSchema& Global =
			State.OrderedGlobals.AddDefaulted_GetRef();
		Global.GlobalKey.Hash = GlobalKey;
		Global.StorageLayoutFingerprint = GlobalStorage;

		FAngelscriptCachedFunctionBody Body;
		Body.FunctionSourceDigest.Hash = MakeHash(0xb0);
		TArray<FAngelscriptCacheSemanticDependency> Observed = {
			MakeContentDependency(
				EAngelscriptCacheSemanticDependencyKind::GlobalStorage,
				EAngelscriptCacheReferenceKind::ScriptGlobal,
				GlobalKey, GlobalAbi, GlobalStorage),
			MakeContentDependency(
				EAngelscriptCacheSemanticDependencyKind::PropertyLayout,
				EAngelscriptCacheReferenceKind::ScriptProperty,
				PropertyKey, PropertyAbi, PropertyLayout),
			MakeContentDependency(
				EAngelscriptCacheSemanticDependencyKind::ValueLayout,
				EAngelscriptCacheReferenceKind::ScriptType,
				TypeKey, TypeAbi, TypeLayout),
		};
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheCompilerBridge::CanonicalizeActualDependencies(
				Observed, Body.ActualDependencies).IsSuccess()));

		FAngelscriptCacheFunctionInputAuthorities Authorities;
		Authorities.ModuleInterface = &Interface;
		Authorities.TypeSchemas =
			TConstArrayView<FAngelscriptCachedTypeSchema>(&TypeSchema, 1);
		Authorities.ModuleState = &State;
		const FAngelscriptCacheFunctionInputResolution Initial =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			Initial.Status));
		ASSERT_THAT(IsFalse(Initial.CurrentInputDigest.Hash.IsZero()));
		Body.FunctionInputDigest = Initial.CurrentInputDigest;

		const FAngelscriptCacheFunctionInputResolution Match =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMatch,
			Match.Status));

		TypeSchema.Layout.TypeLayoutHash = MakeHash(0x31);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities).Status));
		TypeSchema.Layout.TypeLayoutHash = TypeLayout;

		Property.PropertyLayoutFingerprint = MakeHash(0x61);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities).Status));
		Property.PropertyLayoutFingerprint = PropertyLayout;

		Global.StorageLayoutFingerprint = MakeHash(0x91);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities).Status));
		Global.StorageLayoutFingerprint = GlobalStorage;

		PropertyDeclaration.SignatureHash = MakeHash(0x51);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities).Status));

		TestRunner->AddInfo(FString::Printf(
			TEXT("Function input authority witnesses verified: Type=%s Property=%s Global=%s Input=%s"),
			*TypeLayout.ToHexString(),
			*PropertyLayout.ToHexString(),
			*GlobalStorage.ToHexString(),
			*Body.FunctionInputDigest.Hash.ToHexString()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
