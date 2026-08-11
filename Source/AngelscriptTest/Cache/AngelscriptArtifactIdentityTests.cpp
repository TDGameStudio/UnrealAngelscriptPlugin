#include "Artifacts/AngelscriptArtifactIdentity.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptArtifactIdentityTests,
	"Angelscript.TestModule.Cache.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptModuleIdentityDescriptor MakeModuleDescriptor(
		const TCHAR* LogicalMount = TEXT("Game"),
		const TCHAR* VirtualPath = TEXT("Gameplay/Hero.as"),
		const TCHAR* ModuleName = TEXT("Gameplay.Hero"))
	{
		TOptional<FAngelscriptLogicalVirtualPath> LogicalPath =
			FAngelscriptArtifactIdentityBuilder::TryCreateLogicalVirtualPath(VirtualPath);
		checkf(LogicalPath.IsSet(), TEXT("Test fixture virtual path must be valid: %s"), VirtualPath);
		return FAngelscriptModuleIdentityDescriptor(LogicalMount, LogicalPath.GetValue(), ModuleName);
	}

	static FAngelscriptTypeIdentityDescriptor MakeTypeDescriptor(
		const FAngelscriptStableModuleKey& ModuleKey,
		const TCHAR* Declaration = TEXT("class AHero : AActor"))
	{
		FAngelscriptTypeIdentityDescriptor Descriptor;
		Descriptor.ModuleKey = ModuleKey;
		Descriptor.Namespace = TEXT("Gameplay");
		Descriptor.Kind = EAngelscriptArtifactEntityKind::Class;
		Descriptor.CanonicalDeclaration = Declaration;
		Descriptor.CanonicalTraits = {TEXT("Blueprintable"), TEXT("Transient=false")};
		return Descriptor;
	}

	static FAngelscriptFunctionIdentityDescriptor MakeFunctionDescriptor(
		const FAngelscriptHash256& OwnerKey,
		const TCHAR* Declaration = TEXT("int Compute(int Value) const"),
		EAngelscriptArtifactEntityKind Kind = EAngelscriptArtifactEntityKind::Method,
		EAngelscriptFunctionOwnerKind OwnerKind = EAngelscriptFunctionOwnerKind::Type)
	{
		FAngelscriptFunctionIdentityDescriptor Descriptor;
		Descriptor.OwnerKind = OwnerKind;
		Descriptor.OwnerKey = OwnerKey;
		Descriptor.Namespace = TEXT("Gameplay");
		Descriptor.Kind = Kind;
		Descriptor.CanonicalDeclaration = Declaration;
		Descriptor.CanonicalTraits = {TEXT("const"), TEXT("final=false")};
		return Descriptor;
	}

	static FAngelscriptHash256 MakeHashWithByte(const uint8 TailByte)
	{
		FBlake3Hash::ByteArray Bytes{};
		for (uint8 Index = 0; Index < 16; ++Index)
		{
			Bytes[Index] = static_cast<uint8>(0xa0 + Index);
		}
		Bytes[31] = TailByte;
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static TArray<FAngelscriptHash256> SortFingerprints(TArray<FAngelscriptHash256> Fingerprints)
	{
		Fingerprints.Sort([](const FAngelscriptHash256& A, const FAngelscriptHash256& B)
		{
			return A < B;
		});
		return Fingerprints;
	}

	template <typename T>
	static constexpr bool HasForbiddenIdentityCoordinate =
		requires(T Value) { Value.FunctionId; }
		|| requires(T Value) { Value.Pointer; }
		|| requires(T Value) { Value.Address; }
		|| requires(T Value) { Value.ProcessAddress; }
		|| requires(T Value) { Value.FunctionAddress; }
		|| requires(T Value) { Value.Line; }
		|| requires(T Value) { Value.SourceLine; }
		|| requires(T Value) { Value.Column; }
		|| requires(T Value) { Value.SourceColumn; }
		|| requires(T Value) { Value.FNameIndex; }
		|| requires(T Value) { Value.FNameComparisonIndex; }
		|| requires(T Value) { Value.ComparisonIndex; }
		|| requires(T Value) { Value.RandomValue; }
		|| requires(T Value) { Value.RandomId; }
		|| requires(T Value) { Value.AllocationOrdinal; }
		|| requires(T Value) { Value.RegistrationOrder; };

	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptModuleIdentityDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptTypeIdentityDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptFunctionIdentityDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptGlobalIdentityDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptPropertyIdentityDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptFunctionSourceDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptFunctionInputDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptCompatibilityDescriptor>);
	static_assert(!HasForbiddenIdentityCoordinate<FAngelscriptContextDescriptor>);

public:
	TEST_METHOD(TypeEntityKindExtensionsDoNotRenumberExistingWireValues)
	{
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptFunctionOwnerKind::Module)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptFunctionOwnerKind::Type)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptFunctionOwnerKind::Global)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptFunctionOwnerKind::Property)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptArtifactEntityKind::Class)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptArtifactEntityKind::Struct)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptArtifactEntityKind::Interface)));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptArtifactEntityKind::Enum)));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptArtifactEntityKind::Delegate)));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(EAngelscriptArtifactEntityKind::Typedef)));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(EAngelscriptArtifactEntityKind::Funcdef)));
		ASSERT_THAT(AreEqual(16, static_cast<int32>(EAngelscriptArtifactEntityKind::GlobalVariable)));
		ASSERT_THAT(AreEqual(17, static_cast<int32>(EAngelscriptArtifactEntityKind::Property)));
		ASSERT_THAT(AreEqual(32, static_cast<int32>(EAngelscriptArtifactEntityKind::GlobalFunction)));
		ASSERT_THAT(AreEqual(33, static_cast<int32>(EAngelscriptArtifactEntityKind::Method)));
		ASSERT_THAT(AreEqual(34, static_cast<int32>(EAngelscriptArtifactEntityKind::Constructor)));
		ASSERT_THAT(AreEqual(35, static_cast<int32>(EAngelscriptArtifactEntityKind::Destructor)));
		ASSERT_THAT(AreEqual(36, static_cast<int32>(EAngelscriptArtifactEntityKind::Factory)));
		ASSERT_THAT(AreEqual(37, static_cast<int32>(EAngelscriptArtifactEntityKind::DelegateSignature)));
		ASSERT_THAT(AreEqual(38, static_cast<int32>(EAngelscriptArtifactEntityKind::ModuleInitializer)));
		ASSERT_THAT(AreEqual(39, static_cast<int32>(EAngelscriptArtifactEntityKind::GlobalInitializer)));
		ASSERT_THAT(AreEqual(40, static_cast<int32>(EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor)));
		ASSERT_THAT(AreEqual(41, static_cast<int32>(EAngelscriptArtifactEntityKind::InitDefaults)));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor)));
	}

	TEST_METHOD(IdentitySetOrderingUsesCanonicalUtf8BytesAcrossNonBmpText)
	{
		const FString BmpPrivateUse(TEXT("\ue000"));
		const FString Supplementary(TEXT("\U00010000"));
		ASSERT_THAT(IsTrue(FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			BmpPrivateUse, Supplementary) < 0,
			TEXT("UTF-8 bytes order U+E000 before U+10000 independently of TCHAR width")));

		const FAngelscriptStableModuleKey ModuleKey =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(MakeModuleDescriptor());
		FAngelscriptTypeIdentityDescriptor Forward = MakeTypeDescriptor(ModuleKey);
		Forward.CanonicalTraits = {BmpPrivateUse, Supplementary};
		FAngelscriptTypeIdentityDescriptor Reverse = Forward;
		Algo::Reverse(Reverse.CanonicalTraits);
		ASSERT_THAT(AreEqual(
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Forward).Hash.ToHexString(),
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(Reverse).Hash.ToHexString(),
			TEXT("Non-BMP insertion order must not change a stable entity key")));
	}

	TEST_METHOD(EntityKeysIgnoreProcessAndEnumerationState)
	{
		const FAngelscriptStableModuleKey FirstModule =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(
				MakeModuleDescriptor(TEXT("Game"), TEXT("Gameplay\\.\\Hero.as"), TEXT("Gameplay.Hero")));
		const FAngelscriptStableModuleKey SecondModule =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(
				MakeModuleDescriptor(TEXT("Game"), TEXT("Gameplay/Hero.as"), TEXT("Gameplay.Hero")));
		FAngelscriptTypeIdentityDescriptor FirstTypeDescriptor = MakeTypeDescriptor(FirstModule);
		FirstTypeDescriptor.CanonicalTraits = {TEXT("Transient=false"), TEXT("Blueprintable")};
		FAngelscriptTypeIdentityDescriptor SecondTypeDescriptor = MakeTypeDescriptor(SecondModule);
		SecondTypeDescriptor.CanonicalTraits = {TEXT("Blueprintable"), TEXT("Transient=false")};
		const FAngelscriptStableTypeKey FirstType =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(FirstTypeDescriptor);
		const FAngelscriptStableTypeKey SecondType =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(SecondTypeDescriptor);
		FAngelscriptFunctionIdentityDescriptor FirstFunctionDescriptor = MakeFunctionDescriptor(FirstType.Hash);
		FirstFunctionDescriptor.CanonicalTraits = {TEXT("final=false"), TEXT("const")};
		FAngelscriptFunctionIdentityDescriptor SecondFunctionDescriptor = MakeFunctionDescriptor(SecondType.Hash);
		SecondFunctionDescriptor.CanonicalTraits = {TEXT("const"), TEXT("final=false")};
		const FAngelscriptStableFunctionKey FirstFunction =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(FirstFunctionDescriptor);
		const FAngelscriptStableFunctionKey SecondFunction =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(SecondFunctionDescriptor);
		FAngelscriptGlobalIdentityDescriptor FirstGlobalDescriptor;
		FirstGlobalDescriptor.ModuleKey = FirstModule;
		FirstGlobalDescriptor.Namespace = TEXT("Gameplay");
		FirstGlobalDescriptor.Kind = EAngelscriptArtifactEntityKind::GlobalVariable;
		FirstGlobalDescriptor.Name = TEXT("Difficulty");
		FirstGlobalDescriptor.CanonicalType = TEXT("const int");
		FirstGlobalDescriptor.CanonicalTraits = {TEXT("readonly"), TEXT("config")};
		FAngelscriptGlobalIdentityDescriptor SecondGlobalDescriptor = FirstGlobalDescriptor;
		SecondGlobalDescriptor.ModuleKey = SecondModule;
		SecondGlobalDescriptor.CanonicalTraits = {TEXT("config"), TEXT("readonly")};
		const FAngelscriptStableGlobalKey FirstGlobal =
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(FirstGlobalDescriptor);
		const FAngelscriptStableGlobalKey SecondGlobal =
			FAngelscriptArtifactIdentityBuilder::BuildGlobalKey(SecondGlobalDescriptor);
		FAngelscriptPropertyIdentityDescriptor FirstPropertyDescriptor;
		FirstPropertyDescriptor.OwnerTypeKey = FirstType;
		FirstPropertyDescriptor.Kind = EAngelscriptArtifactEntityKind::Property;
		FirstPropertyDescriptor.Name = TEXT("Health");
		FirstPropertyDescriptor.CanonicalType = TEXT("float");
		FirstPropertyDescriptor.CanonicalTraits = {TEXT("EditAnywhere"), TEXT("Replicated")};
		FAngelscriptPropertyIdentityDescriptor SecondPropertyDescriptor = FirstPropertyDescriptor;
		SecondPropertyDescriptor.OwnerTypeKey = SecondType;
		SecondPropertyDescriptor.CanonicalTraits = {TEXT("Replicated"), TEXT("EditAnywhere")};
		const FAngelscriptStablePropertyKey FirstProperty =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(FirstPropertyDescriptor);
		const FAngelscriptStablePropertyKey SecondProperty =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(SecondPropertyDescriptor);
		const FString ExpectedModuleHash = TEXT("b6318b710ea6fa80cab1300a994f94f7528177ed6340c8c5fd388bb9a4608901");
		const FString ExpectedTypeHash = TEXT("9f40c2dae35bde5e33cee3cb5643bdd4d9eff0f5ad8d9d494746c8bab0f2cbac");
		const FString ExpectedFunctionHash = TEXT("acc715df1e889d80cd194751b559ba819f3c71d178907aee3ff7473817dc666b");
		const FString ExpectedGlobalHash = TEXT("21ab3180e18fee31c37451969b8c8a36e548c31b1ac85efa3ebe9a704be20065");
		const FString ExpectedPropertyHash = TEXT("ca384a931e9e740a5ec5c31971aacb51e2400ce2b015cae214db6b452fe19f4e");
		ASSERT_THAT(AreEqual(ExpectedModuleHash, FirstModule.Hash.ToHexString(),
			*FString::Printf(TEXT("Module identity must match its frozen encoding; actual=%s"), *FirstModule.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedModuleHash, SecondModule.Hash.ToHexString(),
			TEXT("Equivalent logical module paths must match the same frozen module encoding")));
		ASSERT_THAT(AreEqual(ExpectedTypeHash, FirstType.Hash.ToHexString(),
			*FString::Printf(TEXT("Type identity must match its frozen encoding; actual=%s"), *FirstType.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedTypeHash, SecondType.Hash.ToHexString(),
			TEXT("Trait insertion order must match the same frozen type encoding")));
		ASSERT_THAT(AreEqual(ExpectedFunctionHash, FirstFunction.Hash.ToHexString(),
			*FString::Printf(TEXT("Function identity must match its frozen encoding; actual=%s"), *FirstFunction.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedFunctionHash, SecondFunction.Hash.ToHexString(),
			TEXT("Trait insertion order must match the same frozen function encoding")));
		ASSERT_THAT(AreEqual(ExpectedGlobalHash, FirstGlobal.Hash.ToHexString(),
			*FString::Printf(TEXT("Global identity must match its frozen encoding; actual=%s"), *FirstGlobal.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedGlobalHash, SecondGlobal.Hash.ToHexString(),
			TEXT("Trait insertion order must match the same frozen global encoding")));
		ASSERT_THAT(AreEqual(ExpectedPropertyHash, FirstProperty.Hash.ToHexString(),
			*FString::Printf(TEXT("Property identity must match its frozen encoding; actual=%s"), *FirstProperty.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedPropertyHash, SecondProperty.Hash.ToHexString(),
			TEXT("Trait insertion order must match the same frozen property encoding")));
	}

	TEST_METHOD(ProjectRelocationPreservesLogicalKeys)
	{
		const TOptional<FAngelscriptStableModuleKey> DriveAbsolute =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("D:/FirstCheckout/Script/Gameplay/Hero.as"), TEXT("Gameplay.Hero"));
		const TOptional<FAngelscriptStableModuleKey> SlashRooted =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("/Game/Gameplay/Hero.as"), TEXT("Gameplay.Hero"));
		const TOptional<FAngelscriptStableModuleKey> UncAbsolute =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("\\\\server\\share\\Gameplay\\Hero.as"), TEXT("Gameplay.Hero"));
		const TOptional<FAngelscriptStableModuleKey> EscapesLogicalRoot =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				TEXT("Game"), TEXT("Scripts/../../HostSecret.as"), TEXT("Gameplay.Hero"));
		ASSERT_THAT(IsFalse(DriveAbsolute.IsSet(),
			TEXT("A Windows drive-qualified host path must fail closed before ModuleKey creation")));
		ASSERT_THAT(IsFalse(SlashRooted.IsSet(),
			TEXT("A slash-rooted path must fail closed before ModuleKey creation")));
		ASSERT_THAT(IsFalse(UncAbsolute.IsSet(),
			TEXT("A UNC host path must fail closed before ModuleKey creation")));
		ASSERT_THAT(IsFalse(EscapesLogicalRoot.IsSet(),
			TEXT("A relative path that escapes its logical root must fail closed before ModuleKey creation")));

		const FAngelscriptStableModuleKey FirstModule =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(
				MakeModuleDescriptor(TEXT("Game"), TEXT("Actors/../Gameplay//Hero.as"), TEXT("Gameplay.Hero")));
		const FAngelscriptStableModuleKey RelocatedModule =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(
				MakeModuleDescriptor(TEXT("Game"), TEXT("Gameplay/Hero.as"), TEXT("Gameplay.Hero")));
		const FString ExpectedModuleHash = TEXT("b6318b710ea6fa80cab1300a994f94f7528177ed6340c8c5fd388bb9a4608901");
		ASSERT_THAT(AreEqual(ExpectedModuleHash, FirstModule.Hash.ToHexString(),
			TEXT("A normalized relative path must match the frozen logical module encoding")));
		ASSERT_THAT(AreEqual(ExpectedModuleHash, RelocatedModule.Hash.ToHexString(),
			TEXT("Equivalent relative coordinates must match after checkout relocation")));

		const FAngelscriptStableTypeKey FirstType =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(MakeTypeDescriptor(FirstModule));
		const FAngelscriptStableTypeKey RelocatedType =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(MakeTypeDescriptor(RelocatedModule));
		ASSERT_THAT(IsTrue(FirstType.Hash == RelocatedType.Hash,
			TEXT("Descendant type identity must survive project relocation")));

		const FAngelscriptStableFunctionKey FirstFunction =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(MakeFunctionDescriptor(FirstType.Hash));
		const FAngelscriptStableFunctionKey RelocatedFunction =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(MakeFunctionDescriptor(RelocatedType.Hash));
		ASSERT_THAT(IsTrue(FirstFunction.Hash == RelocatedFunction.Hash,
			TEXT("Descendant function identity must survive project relocation")));
	}

	TEST_METHOD(OverloadsAndSyntheticOwnersAreDistinct)
	{
		const FAngelscriptStableModuleKey Module =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(MakeModuleDescriptor());
		const FAngelscriptStableTypeKey HeroType =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(MakeTypeDescriptor(Module));
		const FAngelscriptStableTypeKey EnemyType =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(
				MakeTypeDescriptor(Module, TEXT("class AEnemy : AActor")));

		const FAngelscriptStableFunctionKey IntOverload =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(
				MakeFunctionDescriptor(HeroType.Hash, TEXT("int Compute(int Value) const")));
		const FAngelscriptStableFunctionKey FloatOverload =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(
				MakeFunctionDescriptor(HeroType.Hash, TEXT("int Compute(float Value) const")));
		ASSERT_THAT(IsFalse(IntOverload.Hash == FloatOverload.Hash,
			TEXT("Canonical declarations must distinguish overloads")));

		const FAngelscriptStableFunctionKey OtherOwner =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(
				MakeFunctionDescriptor(EnemyType.Hash, TEXT("int Compute(int Value) const")));
		ASSERT_THAT(IsFalse(IntOverload.Hash == OtherOwner.Hash,
			TEXT("The stable owner must distinguish otherwise identical methods")));

		FAngelscriptFunctionIdentityDescriptor ModuleInitializerDescriptor = MakeFunctionDescriptor(
			Module.Hash,
			TEXT("void $module_init()"),
			EAngelscriptArtifactEntityKind::ModuleInitializer,
			EAngelscriptFunctionOwnerKind::Module);
		ModuleInitializerDescriptor.CanonicalTraits = {TEXT("synthetic"), TEXT("startup")};
		FAngelscriptFunctionIdentityDescriptor ModuleInitializerVariantDescriptor = ModuleInitializerDescriptor;
		ModuleInitializerVariantDescriptor.CanonicalTraits = {TEXT("startup"), TEXT("synthetic")};
		const FAngelscriptStableFunctionKey ModuleInitializer =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(ModuleInitializerDescriptor);
		const FAngelscriptStableFunctionKey ModuleInitializerVariant =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(ModuleInitializerVariantDescriptor);
		const FString ExpectedSyntheticFunctionHash = TEXT("95c046ac732d0ec0339a22f001836ed105962195aac3991b37c0fa39c425ec85");
		ASSERT_THAT(AreEqual(ExpectedSyntheticFunctionHash, ModuleInitializer.Hash.ToHexString(),
			*FString::Printf(TEXT("Synthetic function owner/kind must match its frozen encoding; actual=%s"),
				*ModuleInitializer.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedSyntheticFunctionHash, ModuleInitializerVariant.Hash.ToHexString(),
			TEXT("Synthetic function trait insertion order must match the same frozen encoding")));
		const FAngelscriptStableFunctionKey GlobalInitializer =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(MakeFunctionDescriptor(
				Module.Hash,
				TEXT("void $module_init()"),
				EAngelscriptArtifactEntityKind::GlobalInitializer,
				EAngelscriptFunctionOwnerKind::Module));
		ASSERT_THAT(IsFalse(ModuleInitializer.Hash == GlobalInitializer.Hash,
			TEXT("Synthetic invocation kind must participate in function identity")));

		const FAngelscriptStableFunctionKey HeroFactory =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(MakeFunctionDescriptor(
				HeroType.Hash,
				TEXT("AHero@ $factory()"),
				EAngelscriptArtifactEntityKind::Factory));
		const FAngelscriptStableFunctionKey EnemyFactory =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(MakeFunctionDescriptor(
				EnemyType.Hash,
				TEXT("AHero@ $factory()"),
				EAngelscriptArtifactEntityKind::Factory));
		ASSERT_THAT(IsFalse(HeroFactory.Hash == EnemyFactory.Hash,
			TEXT("Synthetic factories must derive identity from their semantic owner")));
	}

	TEST_METHOD(BodyChangesInputAndContentButNotFunctionKey)
	{
		const FAngelscriptStableModuleKey Module =
			FAngelscriptArtifactIdentityBuilder::BuildModuleKey(MakeModuleDescriptor());
		const FAngelscriptStableTypeKey Type =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(MakeTypeDescriptor(Module));
		const FAngelscriptFunctionIdentityDescriptor FunctionDescriptor = MakeFunctionDescriptor(Type.Hash);
		const FAngelscriptStableFunctionKey FirstFunctionKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(FunctionDescriptor);
		const FAngelscriptStableFunctionKey ChangedBodyFunctionKey =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(FunctionDescriptor);
		ASSERT_THAT(IsTrue(FirstFunctionKey.Hash == ChangedBodyFunctionKey.Hash,
			TEXT("A body-only edit must preserve StableFunctionKey")));

		FAngelscriptFunctionSourceDescriptor FirstSourceDescriptor;
		FirstSourceDescriptor.Kind = EAngelscriptArtifactEntityKind::Method;
		FirstSourceDescriptor.CanonicalSource = TEXT("return Value + 1;");
		FirstSourceDescriptor.CanonicalOptions = {TEXT("optimize=true"), TEXT("preprocessor=v1")};
		FAngelscriptFunctionSourceDescriptor FirstSourceVariantDescriptor = FirstSourceDescriptor;
		FirstSourceVariantDescriptor.CanonicalOptions = {TEXT("preprocessor=v1"), TEXT("optimize=true")};
		FAngelscriptFunctionSourceDescriptor ChangedSourceDescriptor = FirstSourceDescriptor;
		ChangedSourceDescriptor.CanonicalSource = TEXT("return Value + 2;");
		const FAngelscriptFunctionSourceDigest FirstSource =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(FirstSourceDescriptor);
		const FAngelscriptFunctionSourceDigest FirstSourceVariant =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(FirstSourceVariantDescriptor);
		const FAngelscriptFunctionSourceDigest ChangedSource =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(ChangedSourceDescriptor);
		ASSERT_THAT(IsFalse(FirstSource.Hash == ChangedSource.Hash,
			TEXT("A body edit must change FunctionSourceDigest")));

		FAngelscriptCompatibilityDescriptor FirstDependencyDescriptor;
		FirstDependencyDescriptor.CanonicalInputs = {TEXT("dependency:Gameplay::SharedType=v3")};
		FAngelscriptCompatibilityDescriptor SecondDependencyDescriptor;
		SecondDependencyDescriptor.CanonicalInputs = {TEXT("dependency:Gameplay::SharedGlobal=v5")};
		const FAngelscriptHash256 FirstDependencyFingerprint =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(FirstDependencyDescriptor).Hash;
		const FAngelscriptHash256 SecondDependencyFingerprint =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(SecondDependencyDescriptor).Hash;
		FAngelscriptFunctionInputDescriptor FirstInputDescriptor;
		FirstInputDescriptor.SourceDigest = FirstSource;
		FirstInputDescriptor.OrderedDependencyFingerprints =
			SortFingerprints({FirstDependencyFingerprint, SecondDependencyFingerprint});
		FAngelscriptFunctionInputDescriptor FirstInputVariantDescriptor = FirstInputDescriptor;
		FirstInputVariantDescriptor.OrderedDependencyFingerprints =
			SortFingerprints({SecondDependencyFingerprint, FirstDependencyFingerprint});
		FAngelscriptFunctionInputDescriptor ChangedInputDescriptor = FirstInputDescriptor;
		ChangedInputDescriptor.SourceDigest = ChangedSource;
		const FAngelscriptFunctionInputDigest FirstInput =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionInputDigest(FirstInputDescriptor);
		const FAngelscriptFunctionInputDigest FirstInputVariant =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionInputDigest(FirstInputVariantDescriptor);
		const FAngelscriptFunctionInputDigest ChangedInput =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionInputDigest(ChangedInputDescriptor);
		const FString ExpectedFunctionSourceHash = TEXT("f5f0713214fdc38f629425c0bdecfcc4e6e57a25030ad314aa3031e6898c46cd");
		const FString ExpectedFunctionInputHash = TEXT("79f055c2756c00ed6684b3964938c0af12e38d7b032a53674c2112163a9f2f4b");
		ASSERT_THAT(AreEqual(ExpectedFunctionSourceHash, FirstSource.Hash.ToHexString(),
			*FString::Printf(TEXT("Function source must match its frozen encoding; actual=%s"),
				*FirstSource.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedFunctionSourceHash, FirstSourceVariant.Hash.ToHexString(),
			TEXT("Function option insertion order must match the same frozen source encoding")));
		ASSERT_THAT(AreEqual(ExpectedFunctionInputHash, FirstInput.Hash.ToHexString(),
			*FString::Printf(TEXT("Function input must match its frozen encoding; actual=%s"),
				*FirstInput.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedFunctionInputHash, FirstInputVariant.Hash.ToHexString(),
			TEXT("Dependency collection insertion order must match after canonical ordering")));
		ASSERT_THAT(IsFalse(FirstInput.Hash == ChangedInput.Hash,
			TEXT("A body edit must change FunctionInputDigest before compilation")));

		const TArray<uint8> FirstExecution = {0x10, 0x01, 0x20};
		const TArray<uint8> ChangedExecution = {0x10, 0x02, 0x20};
		const TArray<uint8> DebugPayload = {0x2a, 0x09};
		const FAngelscriptFunctionContentHash FirstContent =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(FirstExecution, DebugPayload);
		const FAngelscriptFunctionContentHash ChangedContent =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(ChangedExecution, DebugPayload);
		ASSERT_THAT(IsFalse(FirstContent.Execution == ChangedContent.Execution,
			TEXT("Changed canonical execution payload must change execution content")));
	}

	TEST_METHOD(DebugAndExecutionContentAreIndependent)
	{
		const TArray<uint8> ExecutionV1 = {0x01, 0x02, 0x03};
		const TArray<uint8> ExecutionV2 = {0x01, 0x02, 0x04};
		const TArray<uint8> DebugV1 = {0x10, 0x20};
		const TArray<uint8> DebugV2 = {0x10, 0x21};

		const FAngelscriptFunctionContentHash Baseline =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(ExecutionV1, DebugV1);
		const FAngelscriptFunctionContentHash DebugOnlyChange =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(ExecutionV1, DebugV2);
		const FString ExpectedExecutionHash = TEXT("aa8ac2c22a3a1a67937bcaaa9aa2e641d65d759cbd181ad4283b8e6edb779246");
		const FString ExpectedDebugHash = TEXT("c4afa75e722008d03691b8bb4f8f1759fdd02cd1032142cf01b00e2f89ff4d2d");
		ASSERT_THAT(AreEqual(ExpectedExecutionHash, Baseline.Execution.ToHexString(),
			*FString::Printf(TEXT("Execution content must match its frozen encoding; actual=%s"),
				*Baseline.Execution.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedDebugHash, Baseline.Debug.ToHexString(),
			*FString::Printf(TEXT("Debug content must match its frozen encoding; actual=%s"),
				*Baseline.Debug.ToHexString())));
		ASSERT_THAT(IsTrue(Baseline.Execution == DebugOnlyChange.Execution,
			TEXT("A debug-only change must preserve execution content")));
		ASSERT_THAT(IsFalse(Baseline.Debug == DebugOnlyChange.Debug,
			TEXT("A debug-only change must change debug content")));

		const FAngelscriptFunctionContentHash ExecutionOnlyChange =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(ExecutionV2, DebugV1);
		ASSERT_THAT(IsFalse(Baseline.Execution == ExecutionOnlyChange.Execution,
			TEXT("An execution change must change execution content")));
		ASSERT_THAT(IsTrue(Baseline.Debug == ExecutionOnlyChange.Debug,
			TEXT("An execution-only change must preserve debug content")));
	}

	TEST_METHOD(DebugAbsenceIsAProfileSpecificFrozenIdentity)
	{
		FAngelscriptCompatibilityDescriptor CompatibilityDescriptor;
		CompatibilityDescriptor.CanonicalInputs = {
			TEXT("cache-schema=2"), TEXT("bytecode-abi=7"), TEXT("platform=Win64")};
		FAngelscriptContextDescriptor EditorContext;
		EditorContext.CanonicalInputs = {
			TEXT("target=Editor"), TEXT("configuration=Development"), TEXT("debug=true")};
		FAngelscriptContextDescriptor ShippingContext;
		ShippingContext.CanonicalInputs = {
			TEXT("target=Game"), TEXT("configuration=Shipping"), TEXT("debug=false")};
		const FAngelscriptCacheCompatibilityKey Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				CompatibilityDescriptor);
		const FAngelscriptArtifactProfileKey EditorProfile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Compatibility,
				FAngelscriptArtifactIdentityBuilder::BuildContextKey(EditorContext));
		const FAngelscriptArtifactProfileKey ShippingProfile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Compatibility,
				FAngelscriptArtifactIdentityBuilder::BuildContextKey(ShippingContext));
		const FAngelscriptHash256 EditorAbsent =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(EditorProfile);
		const FAngelscriptHash256 ShippingAbsent =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionDebugAbsentHash(ShippingProfile);
		UE_LOG(LogTemp, Display,
			TEXT("[CacheV2][Identity] debug-absent editor=%s shipping=%s"),
			*EditorAbsent.ToHexString(), *ShippingAbsent.ToHexString());
		const FString ExpectedEditor = TEXT(
			"99bb2d1ff43783d9e797ad35d5607db87ca8e13a09446a0c03f725adcdc7ccc0");
		const FString ExpectedShipping = TEXT(
			"24c6faa88ee73a59682e8e4513c425b97c33c887c639fd4d90f9861cb91707e9");
		ASSERT_THAT(AreEqual(ExpectedEditor, EditorAbsent.ToHexString(),
			*FString::Printf(TEXT("Frozen debug-absence vectors: editor=%s shipping=%s"),
				*EditorAbsent.ToHexString(), *ShippingAbsent.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedShipping, ShippingAbsent.ToHexString()));
		ASSERT_THAT(IsFalse(EditorAbsent == ShippingAbsent));
		ASSERT_THAT(IsFalse(EditorAbsent
			== FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash({}, {}).Debug,
			TEXT("absence is not the empty present-debug payload hash")));
	}

	TEST_METHOD(ProfileInputsRemainSeparated)
	{
		FAngelscriptCompatibilityDescriptor CompatibilityDescriptor;
		CompatibilityDescriptor.CanonicalInputs = {
			TEXT("cache-schema=2"), TEXT("bytecode-abi=7"), TEXT("platform=Win64")};
		FAngelscriptCompatibilityDescriptor CompatibilityVariantDescriptor;
		CompatibilityVariantDescriptor.CanonicalInputs = {
			TEXT("platform=Win64"), TEXT("cache-schema=2"), TEXT("bytecode-abi=7")};
		FAngelscriptCompatibilityDescriptor ChangedCompatibilityDescriptor = CompatibilityDescriptor;
		ChangedCompatibilityDescriptor.CanonicalInputs = {
			TEXT("cache-schema=2"), TEXT("bytecode-abi=8"), TEXT("platform=Win64")};

		FAngelscriptContextDescriptor EditorContextDescriptor;
		EditorContextDescriptor.CanonicalInputs = {
			TEXT("target=Editor"), TEXT("configuration=Development"), TEXT("debug=true")};
		FAngelscriptContextDescriptor EditorContextVariantDescriptor;
		EditorContextVariantDescriptor.CanonicalInputs = {
			TEXT("debug=true"), TEXT("target=Editor"), TEXT("configuration=Development")};
		FAngelscriptContextDescriptor ShippingContextDescriptor;
		ShippingContextDescriptor.CanonicalInputs = {
			TEXT("target=Game"), TEXT("configuration=Shipping"), TEXT("debug=false")};

		const FAngelscriptCacheCompatibilityKey Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(CompatibilityDescriptor);
		const FAngelscriptCacheCompatibilityKey CompatibilityVariant =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(CompatibilityVariantDescriptor);
		const FAngelscriptCacheCompatibilityKey ChangedCompatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(ChangedCompatibilityDescriptor);
		const FAngelscriptCacheContextKey EditorContext =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(EditorContextDescriptor);
		const FAngelscriptCacheContextKey EditorContextVariant =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(EditorContextVariantDescriptor);
		const FAngelscriptCacheContextKey ShippingContext =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(ShippingContextDescriptor);

		const FAngelscriptArtifactProfileKey EditorProfile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(Compatibility, EditorContext);
		const FAngelscriptArtifactProfileKey EditorProfileVariant =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				CompatibilityVariant, EditorContextVariant);
		const FAngelscriptArtifactProfileKey ShippingProfile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(Compatibility, ShippingContext);
		const FAngelscriptArtifactProfileKey ChangedCompatibilityProfile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(ChangedCompatibility, EditorContext);
		const FString ExpectedCompatibilityHash = TEXT("c33a5dbefd943e446a91cb9e4f923a2d75ecf615c3f3f8c88ad89d9f8aaa33af");
		const FString ExpectedContextHash = TEXT("38aadabf648ee783d14a3bc32d76b786f2647ea2adccb62b3581ff9463768e6a");
		const FString ExpectedProfileHash = TEXT("85c25f64159a3ff0d393e11c8bf2f42579494faa0e8c8a7ab3250e0c5ce1f26b");
		ASSERT_THAT(AreEqual(ExpectedCompatibilityHash, Compatibility.Hash.ToHexString(),
			*FString::Printf(TEXT("Compatibility key must match its frozen encoding; actual=%s"),
				*Compatibility.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedCompatibilityHash, CompatibilityVariant.Hash.ToHexString(),
			TEXT("Compatibility input insertion order must match the same frozen encoding")));
		ASSERT_THAT(AreEqual(ExpectedContextHash, EditorContext.Hash.ToHexString(),
			*FString::Printf(TEXT("Context key must match its frozen encoding; actual=%s"),
				*EditorContext.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedContextHash, EditorContextVariant.Hash.ToHexString(),
			TEXT("Context input insertion order must match the same frozen encoding")));
		ASSERT_THAT(AreEqual(ExpectedProfileHash, EditorProfile.Hash.ToHexString(),
			*FString::Printf(TEXT("Artifact profile must match its frozen encoding; actual=%s"),
				*EditorProfile.Hash.ToHexString())));
		ASSERT_THAT(AreEqual(ExpectedProfileHash, EditorProfileVariant.Hash.ToHexString(),
			TEXT("Canonicalized profile inputs must match the same frozen profile encoding")));
		ASSERT_THAT(IsFalse(EditorProfile.Hash == ShippingProfile.Hash,
			TEXT("Editor and Shipping context inputs must select distinct profiles")));
		ASSERT_THAT(IsFalse(EditorProfile.Hash == ChangedCompatibilityProfile.Hash,
			TEXT("A compatibility ABI change must select a distinct profile")));

		FAngelscriptCompatibilityDescriptor SharedTextCompatibility;
		SharedTextCompatibility.CanonicalInputs = {TEXT("same-input")};
		FAngelscriptContextDescriptor SharedTextContext;
		SharedTextContext.CanonicalInputs = {TEXT("same-input")};
		const FAngelscriptCacheCompatibilityKey DomainSeparatedCompatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(SharedTextCompatibility);
		const FAngelscriptCacheContextKey DomainSeparatedContext =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(SharedTextContext);
		ASSERT_THAT(IsFalse(DomainSeparatedCompatibility.Hash == DomainSeparatedContext.Hash,
			TEXT("Compatibility and context domains must remain distinct for identical field text")));
	}

	TEST_METHOD(FullHashRejectsDisplayGuidCollision)
	{
		const FAngelscriptHash256 ZeroHash;
		const FAngelscriptHash256 FirstHash = MakeHashWithByte(0x01);
		const FAngelscriptHash256 SecondHash = MakeHashWithByte(0x02);
		ASSERT_THAT(IsTrue(ZeroHash.IsZero(),
			TEXT("A default full-width hash must report zero")));
		ASSERT_THAT(IsFalse(FirstHash.IsZero(),
			TEXT("A non-zero trailing byte must make the full-width hash non-zero")));
		ASSERT_THAT(AreEqual(FirstHash.ToDisplayGuid(), SecondHash.ToDisplayGuid(),
			TEXT("The fixture must collide in its derived display GUID")));
		ASSERT_THAT(IsFalse(FirstHash == SecondHash,
			TEXT("Hashes with equal display GUIDs but different trailing bytes must remain distinct")));
		ASSERT_THAT(IsTrue((FirstHash < SecondHash) != (SecondHash < FirstHash),
			TEXT("Full-width ordering must distinguish a display GUID collision")));
		ASSERT_THAT(AreEqual(64, FirstHash.ToHexString().Len(),
			TEXT("Authoritative hash text must expose all 256 bits")));
	}

	TEST_METHOD(CanonicalWriterHasByteExactGoldenVector)
	{
		FBlake3Hash::ByteArray HashBytes{};
		for (uint8 Index = 0; Index < 32; ++Index)
		{
			HashBytes[Index] = Index;
		}

		FAngelscriptArtifactCanonicalWriter Writer(TEXT("module"));
		Writer.WriteUInt8(0x7f);
		Writer.WriteUInt16(0x1234);
		Writer.WriteUInt32(0x78563412);
		Writer.WriteUInt64(UINT64_C(0x0102030405060708));
		Writer.WriteBool(true);
		Writer.WriteBool(false);
		Writer.WriteHash(FAngelscriptHash256{FBlake3Hash(HashBytes)});
		const TArray<uint8> BytePayload = {0xaa, 0xbb};
		Writer.WriteBytes(BytePayload);
		Writer.WriteString(TEXT("Aé"));

		const TConstArrayView<uint8> CanonicalBytes = Writer.GetBytes();
		const FString ActualBytesHex = BytesToHexLower(CanonicalBytes.GetData(), CanonicalBytes.Num());
		const FString ExpectedBytesHex = TEXT(
			"554541532d41525449464143540001000000060000006d6f64756c65"
			"7f34121234567808070605040302010100"
			"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
			"02000000aabb0300000041c3a9");
		ASSERT_THAT(AreEqual(ExpectedBytesHex, ActualBytesHex,
			TEXT("Canonical writer bytes must match the frozen little-endian UTF-8 vector")));

		const FString ActualHashHex = Writer.FinalizeHash().ToHexString();
		const FString ExpectedHashHex = TEXT("92d4b174180cfda1e596ed3b40717ed97e2e62cec65a95f07ab12b2ac490850d");
		ASSERT_THAT(AreEqual(ExpectedHashHex, ActualHashHex,
			*FString::Printf(TEXT("Canonical writer BLAKE3-256 must match the frozen full hash; actual=%s"), *ActualHashHex)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
