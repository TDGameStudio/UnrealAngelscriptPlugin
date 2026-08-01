#include "CQTest.h"
#include "Dump/AngelscriptOfflineContractIdentity.h"
#include "Dump/AngelscriptOfflineContractJson.h"
#include "Dump/AngelscriptOfflineAdapterExporter.h"
#include "Dump/AngelscriptOfflineBundleFixtureReader.h"
#include "Dump/AngelscriptOfflineBundleWriter.h"
#include "Dump/AngelscriptOfflineContractSerializer.h"
#include "Dump/AngelscriptOfflineExportService.h"
#include "Dump/AngelscriptOfflineSymbolExporter.h"
#include "Dump/AngelscriptOfflineContractTypes.h"

#include "AngelscriptEngine.h"
#include "AngelscriptInclude.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Shared/AngelscriptTestEngineHelper.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptOfflineContractCoreTests,
	"Angelscript.TestModule.CppTests.OfflineContract.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString Utf8BytesToString(const TArray<uint8>& Bytes)
	{
		if (Bytes.IsEmpty())
		{
			return FString();
		}

		const FUTF8ToTCHAR Converted(
			reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),
			Bytes.Num());
		return FString(Converted.Length(), Converted.Get());
	}

	static void GenericCompileOnlyTrap(asIScriptGeneric* Generic)
	{
		check(Generic != nullptr);
	}

	static const AngelscriptOfflineContract::FSymbolRecord* FindSymbol(
		const TArray<AngelscriptOfflineContract::FSymbolRecord>& Symbols,
		const AngelscriptOfflineContract::ESymbolKind Kind,
		const FStringView Name)
	{
		using namespace AngelscriptOfflineContract;

		return Symbols.FindByPredicate([Kind, Name](const FSymbolRecord& Symbol)
		{
			if (Symbol.Kind != Kind)
			{
				return false;
			}
			switch (Kind)
			{
			case ESymbolKind::Type:
			case ESymbolKind::Typedef:
			case ESymbolKind::Funcdef:
			case ESymbolKind::Delegate:
				return Symbol.Type.Name == Name;
			case ESymbolKind::Callable:
				return Symbol.Callable.Name == Name;
			case ESymbolKind::Property:
			case ESymbolKind::Global:
				return Symbol.Property.Name == Name;
			default:
				return false;
			}
		});
	}

	static const AngelscriptOfflineContract::FSymbolRecord* FindOwnedCallable(
		const TArray<AngelscriptOfflineContract::FSymbolRecord>& Symbols,
		const FStringView OwnerStableId,
		const FStringView Name)
	{
		using namespace AngelscriptOfflineContract;

		return Symbols.FindByPredicate(
			[OwnerStableId, Name](const FSymbolRecord& Symbol)
		{
			return Symbol.Kind == ESymbolKind::Callable
				&& Symbol.Callable.OwnerStableId == OwnerStableId
				&& Symbol.Callable.Name == Name;
		});
	}

public:
	TEST_METHOD(Sha256MatchesCanonicalKnownVectors)
	{
		using namespace AngelscriptOfflineContract;

		const TArray<uint8> EmptyBytes;
		ASSERT_THAT(AreEqual(
			FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")),
			Sha256Bytes(EmptyBytes),
			TEXT("Empty SHA-256 should match the canonical vector")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")),
			Sha256Utf8(TEXT("abc")),
			TEXT("ASCII UTF-8 SHA-256 should match the canonical vector")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("33650a369521ec29f2e26c43d25967535bcb26436755f536735d1ef6e84a1ec5")),
			Sha256Utf8(TEXT("世界")),
			TEXT("Non-ASCII UTF-8 SHA-256 should match the canonical vector")));
	}

	TEST_METHOD(ContractRecordsKeepSymbolAndAssetCompletenessIndependent)
	{
		using namespace AngelscriptOfflineContract;

		FManifestRecord Manifest;
		Manifest.BundleKind = EBundleKind::Project;
		Manifest.SymbolScope.bComplete = true;
		Manifest.SymbolScope.Included.Add(TEXT("AngelscriptRuntime"));
		Manifest.AssetScope.bComplete = false;
		Manifest.AssetScope.Included.Add(TEXT("/Game"));
		Manifest.AssetScope.Skipped.Add(TEXT("/Plugin/Unscanned"));

		FTypeRecord Type;
		Type.StableId = TEXT("type-id");
		Type.Kind = ETypeKind::Value;
		Type.Name = TEXT("FVector");
		Type.Namespace = TEXT("");
		Type.CompileSize = 24;
		Type.CompileAlignment = 8;
		Type.Origin.Layer = EOriginLayer::HostSurface;
		Type.Origin.Kind = EOriginKind::Manual;

		FCallableRecord Callable;
		Callable.StableId = TEXT("callable-id");
		Callable.OwnerStableId = Type.StableId;
		Callable.Kind = ECallableKind::Method;
		Callable.Name = TEXT("Size");
		Callable.Declaration = TEXT("double Size() const");
		Callable.ReturnType = TEXT("double");
		Callable.bConst = true;
		FParameterRecord ResourceParameter;
		ResourceParameter.Name = TEXT("PreviewPath");
		ResourceParameter.TypeDeclaration = TEXT("const FString&in");
		ResourceParameter.ResourceKind = TEXT("soft-object");
		ResourceParameter.ResourceTypeStableId = TEXT("texture-type-id");
		Callable.Parameters.Add(ResourceParameter);

		FPropertyRecord Property;
		Property.StableId = TEXT("property-id");
		Property.OwnerStableId = Type.StableId;
		Property.Name = TEXT("X");
		Property.TypeDeclaration = TEXT("double");

		FAdapterRecord Adapter;
		Adapter.StableId = TEXT("adapter-id");
		Adapter.Version = TEXT("1.0");
		Adapter.bDeclarativeOnly = false;
		Adapter.SurfaceHash = TEXT("surface-hash");

		FAssetRecord Asset;
		Asset.StableId = TEXT("asset-id");
		Asset.PackagePath = TEXT("/Game/Characters/BP_Hero");
		Asset.ObjectPath = TEXT("/Game/Characters/BP_Hero.BP_Hero");
		Asset.GeneratedClassPath = TEXT("/Game/Characters/BP_Hero.BP_Hero_C");

		ASSERT_THAT(IsTrue(Manifest.SymbolScope.bComplete,
			TEXT("Final symbol scope must be independently complete")));
		ASSERT_THAT(IsFalse(Manifest.AssetScope.bComplete,
			TEXT("Asset scope may remain explicitly incomplete")));
		ASSERT_THAT(AreEqual(ETypeKind::Value, Type.Kind,
			TEXT("Type records should retain compile-only type kind")));
		ASSERT_THAT(AreEqual(Type.StableId, Callable.OwnerStableId,
			TEXT("Callable records should refer to semantic owners by stable ID")));
		FSymbolRecord CallableSymbol;
		CallableSymbol.Kind = ESymbolKind::Callable;
		CallableSymbol.StableId = Callable.StableId;
		CallableSymbol.Callable = Callable;
		const FString CallableJson = Utf8BytesToString(
			SerializeCanonicalJsonDocument(
				ToCanonicalJson(CallableSymbol)));
		ASSERT_THAT(IsTrue(
			CallableJson.Contains(
				TEXT("\"resourceKind\":\"soft-object\""))
				&& CallableJson.Contains(
					TEXT("\"resourceTypeStableId\":\"texture-type-id\"")),
			TEXT("Callable parameters should serialize explicit resource semantics")));
		ASSERT_THAT(AreEqual(Type.StableId, Property.OwnerStableId,
			TEXT("Property records should refer to semantic owners by stable ID")));
		ASSERT_THAT(IsFalse(Adapter.bDeclarativeOnly,
			TEXT("Non-declarative adapters should be explicit")));
		ASSERT_THAT(IsTrue(Asset.GeneratedClassPath.EndsWith(TEXT("_C")),
			TEXT("Asset records should preserve normalized generated-class identity")));
	}

	TEST_METHOD(StableIdentityNormalizesDeclarationsWithoutUsingRuntimeOrder)
	{
		using namespace AngelscriptOfflineContract;

		FSymbolIdentityInput First;
		First.Kind = ESymbolKind::Callable;
		First.Namespace = TEXT(" Gameplay :: Math ");
		First.OwnerStableId = TEXT("owner-vector");
		First.CompleteDeclaration = TEXT(" double   Dot( const FVector & in Other ) const ");

		FSymbolIdentityInput SameSemanticIdentity = First;
		SameSemanticIdentity.Namespace = TEXT("Gameplay::Math");
		SameSemanticIdentity.CompleteDeclaration =
			TEXT("double Dot(const FVector& in Other) const");

		const FString FirstId = MakeStableSymbolId(First);
		const FString SecondId = MakeStableSymbolId(SameSemanticIdentity);
		ASSERT_THAT(AreEqual(FirstId, SecondId,
			TEXT("Whitespace and namespace spelling normalization should preserve symbol identity")));

		FSymbolIdentityInput Overload = SameSemanticIdentity;
		Overload.CompleteDeclaration = TEXT("double Dot(const FVector2D& in Other) const");
		ASSERT_THAT(AreNotEqual(FirstId, MakeStableSymbolId(Overload),
			TEXT("Overload declaration changes should change symbol identity")));

		FSymbolIdentityInput DifferentOwner = SameSemanticIdentity;
		DifferentOwner.OwnerStableId = TEXT("owner-vector2d");
		ASSERT_THAT(AreNotEqual(FirstId, MakeStableSymbolId(DifferentOwner),
			TEXT("Owner changes should change symbol identity")));

		FSymbolIdentityInput Unicode = SameSemanticIdentity;
		Unicode.Namespace = TEXT("游戏::数学");
		Unicode.CompleteDeclaration = TEXT("double 点积(const FVector& in 另一个) const");
		ASSERT_THAT(AreEqual(MakeStableSymbolId(Unicode), MakeStableSymbolId(Unicode),
			TEXT("Unicode semantic identities should be repeatable")));

		const FString ModuleIdBeforeSourceChange =
			MakeStableModuleId(TEXT("Gameplay/Player"), TEXT("/Script/Gameplay/Player.as"));
		const FString ModuleIdAfterSourceChange =
			MakeStableModuleId(TEXT("Gameplay/Player"), TEXT("/Script/Gameplay/Player.as"));
		ASSERT_THAT(AreEqual(ModuleIdBeforeSourceChange, ModuleIdAfterSourceChange,
			TEXT("Stable module identity must not include source contents")));
		ASSERT_THAT(AreNotEqual(
			ModuleIdBeforeSourceChange,
			MakeStableModuleId(TEXT("Gameplay/PlayerV2"), TEXT("/Script/Gameplay/PlayerV2.as")),
			TEXT("Logical module replacement identity changes should change the module ID")));
	}

	TEST_METHOD(CanonicalJsonUsesUtf8LfAndStableOrdering)
	{
		using namespace AngelscriptOfflineContract;

		FCanonicalJsonValue FirstPayload = FCanonicalJsonValue::Object();
		FirstPayload.Set(TEXT("zeta"), FCanonicalJsonValue::Integer(7));
		FirstPayload.Set(TEXT("alpha"), FCanonicalJsonValue::String(TEXT("世界\nline")));
		FirstPayload.Set(TEXT("enabled"), FCanonicalJsonValue::Boolean(true));
		FirstPayload.Set(TEXT("fraction"), FCanonicalJsonValue::Number(1.5));
		FirstPayload.Set(TEXT("negativeZero"), FCanonicalJsonValue::Number(-0.0));

		FCanonicalJsonValue SecondPayload = FCanonicalJsonValue::Object();
		SecondPayload.Set(TEXT("name"), FCanonicalJsonValue::String(TEXT("second")));

		TArray<FCanonicalJsonLine> Lines;
		Lines.Add({TEXT("symbol-z"), MoveTemp(SecondPayload)});
		Lines.Add({TEXT("symbol-a"), MoveTemp(FirstPayload)});

		const TArray<uint8> FirstBytes = SerializeCanonicalJsonLines(Lines);
		const TArray<uint8> SecondBytes = SerializeCanonicalJsonLines(Lines);
		ASSERT_THAT(AreEqual(FirstBytes, SecondBytes,
			TEXT("Canonical JSONL should be byte-for-byte repeatable")));
		ASSERT_THAT(IsTrue(FirstBytes.Num() > 3,
			TEXT("Canonical JSONL should contain serialized records")));
		ASSERT_THAT(IsFalse(
			FirstBytes.Num() >= 3
				&& FirstBytes[0] == 0xef
				&& FirstBytes[1] == 0xbb
				&& FirstBytes[2] == 0xbf,
			TEXT("Canonical JSONL must not include a UTF-8 BOM")));
		ASSERT_THAT(IsFalse(FirstBytes.Contains(static_cast<uint8>('\r')),
			TEXT("Canonical JSONL must use LF line endings")));
		ASSERT_THAT(AreEqual(static_cast<uint8>('\n'), FirstBytes.Last(),
			TEXT("Canonical JSONL should terminate every record with LF")));

		const FString Serialized = Utf8BytesToString(FirstBytes);
		const int32 FirstRecordPosition = Serialized.Find(TEXT("\"alpha\""));
		const int32 EnabledPosition = Serialized.Find(TEXT("\"enabled\""));
		const int32 FractionPosition = Serialized.Find(TEXT("\"fraction\""));
		const int32 NegativeZeroPosition = Serialized.Find(TEXT("\"negativeZero\""));
		const int32 ZetaPosition = Serialized.Find(TEXT("\"zeta\""));
		const int32 SecondRecordPosition = Serialized.Find(TEXT("\"second\""));

		ASSERT_THAT(IsTrue(
			FirstRecordPosition >= 0
				&& EnabledPosition > FirstRecordPosition
				&& FractionPosition > EnabledPosition
				&& NegativeZeroPosition > FractionPosition
				&& ZetaPosition > NegativeZeroPosition,
			TEXT("Canonical JSON object keys should use stable lexical ordering")));
		ASSERT_THAT(IsTrue(SecondRecordPosition > ZetaPosition,
			TEXT("Canonical JSONL records should use stable semantic-ID ordering")));
		ASSERT_THAT(IsTrue(Serialized.Contains(TEXT("\"enabled\":true")),
			TEXT("Canonical JSON should normalize booleans")));
		ASSERT_THAT(IsTrue(Serialized.Contains(TEXT("\"zeta\":7")),
			TEXT("Canonical JSON should normalize integers")));
		ASSERT_THAT(IsTrue(Serialized.Contains(TEXT("\"fraction\":1.5")),
			TEXT("Canonical JSON should normalize finite decimal numbers")));
		ASSERT_THAT(IsTrue(Serialized.Contains(TEXT("\"negativeZero\":0")),
			TEXT("Canonical JSON should normalize negative zero")));
		ASSERT_THAT(IsTrue(Serialized.Contains(TEXT("世界\\nline")),
			TEXT("Canonical JSON should preserve UTF-8 text and escape embedded newlines")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("{\"alpha\":\"世界\\nline\",\"enabled\":true,\"fraction\":1.5,\"negativeZero\":0,\"zeta\":7}\n{\"name\":\"second\"}\n")),
			Serialized,
			TEXT("Canonical JSONL should match the frozen byte-level golden document")));
	}

	TEST_METHOD(FinalEngineObserverExportsRegisteredDeclarationsWithoutAddresses)
	{
		using namespace AngelscriptOfflineContract;

		asIScriptEngine* ScriptEngine = asCreateScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Offline exporter fixture should create a script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->ShutDownAndRelease();
		};

		int32 ExportVersion = 1;
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterEnum("EExportState") >= 0
				&& ScriptEngine->RegisterEnumValue("EExportState", "Idle", 0) >= 0
				&& ScriptEngine->RegisterEnumValue("EExportState", "Ready", 7) >= 0,
			TEXT("Offline exporter fixture should register its enum surface")));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterObjectType(
				"FExportValue",
				8,
				asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE) >= 0
				&& ScriptEngine->RegisterObjectProperty(
					"FExportValue",
					"int Count",
					0) >= 0
				&& ScriptEngine->RegisterObjectMethod(
					"FExportValue",
					"int GetCount(int Bonus = 2) const",
					asFUNCTION(GenericCompileOnlyTrap),
					asCALL_GENERIC) >= 0,
			TEXT("Offline exporter fixture should register its value type surface")));
		const int GlobalPropertyResult = ScriptEngine->RegisterGlobalProperty(
			"const int ExportVersion",
			&ExportVersion);
		const int IntegerOverloadResult = ScriptEngine->RegisterGlobalFunction(
			"int ExportOverload(int Value)",
			asFUNCTION(GenericCompileOnlyTrap),
			asCALL_GENERIC);
		const int UnsignedOverloadResult = ScriptEngine->RegisterGlobalFunction(
			"int ExportOverload(uint Value)",
			asFUNCTION(GenericCompileOnlyTrap),
			asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			GlobalPropertyResult >= 0,
			*FString::Printf(
				TEXT("Offline exporter fixture should register the global property (result %d)"),
				GlobalPropertyResult)));
		ASSERT_THAT(IsTrue(
			IntegerOverloadResult >= 0,
			*FString::Printf(
				TEXT("Offline exporter fixture should register the integer overload (result %d)"),
				IntegerOverloadResult)));
		ASSERT_THAT(IsTrue(
			UnsignedOverloadResult >= 0,
			*FString::Printf(
				TEXT("Offline exporter fixture should register the unsigned overload (result %d)"),
				UnsignedOverloadResult)));
		ASSERT_THAT(IsTrue(
			ScriptEngine->RegisterTypedef("ExportCount", "int") >= 0
				&& ScriptEngine->RegisterFuncdef(
					"void FExportDelegate(int Value)") >= 0,
			TEXT("Offline exporter fixture should register typedef and funcdef declarations")));

		const FSymbolExportResult Export =
			FAngelscriptOfflineSymbolExporter::ExportHostSurface(*ScriptEngine);
		ASSERT_THAT(IsTrue(Export.bSuccess,
			TEXT("Final engine observer should export a complete host surface")));
		ASSERT_THAT(IsTrue(Export.SymbolScope.bComplete,
			TEXT("Successful final engine traversal should prove complete symbol scope")));
		ASSERT_THAT(IsTrue(Export.Symbols.Num() > 0,
			TEXT("Final engine traversal should produce symbol records")));

		const FSymbolRecord* EnumSymbol =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("EExportState"));
		const FSymbolRecord* ValueType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("FExportValue"));
		const FSymbolRecord* Method =
			FindSymbol(Export.Symbols, ESymbolKind::Callable, TEXT("GetCount"));
		const FSymbolRecord* Global =
			FindSymbol(Export.Symbols, ESymbolKind::Global, TEXT("ExportVersion"));
		const FSymbolRecord* Typedef =
			FindSymbol(Export.Symbols, ESymbolKind::Typedef, TEXT("ExportCount"));
		const FSymbolRecord* Funcdef =
			FindSymbol(Export.Symbols, ESymbolKind::Funcdef, TEXT("FExportDelegate"));

		ASSERT_THAT(IsNotNull(EnumSymbol,
			TEXT("Observer should export registered enum declarations")));
		ASSERT_THAT(IsNotNull(ValueType,
			TEXT("Observer should export registered value types")));
		ASSERT_THAT(IsNotNull(Method,
			TEXT("Observer should export registered methods")));
		ASSERT_THAT(IsNotNull(Global,
			TEXT("Observer should export registered global properties")));
		ASSERT_THAT(IsNotNull(Typedef,
			TEXT("Observer should export registered typedefs")));
		ASSERT_THAT(IsNotNull(Funcdef,
			TEXT("Observer should export registered funcdefs")));

		if (Typedef != nullptr)
		{
			ASSERT_THAT(IsTrue(
				Typedef->Type.CompleteDeclaration.Contains(TEXT("int")),
				TEXT("Typedef complete identity should include its underlying declaration")));
			FSymbolIdentityInput Identity;
			Identity.Kind = ESymbolKind::Typedef;
			Identity.Namespace = Typedef->Type.Namespace;
			Identity.CompleteDeclaration = Typedef->Type.CompleteDeclaration;
			ASSERT_THAT(AreEqual(
				MakeStableSymbolId(Identity),
				Typedef->StableId,
				TEXT("Typedef stable ID should derive from the exported complete declaration")));
		}
		if (Funcdef != nullptr)
		{
			ASSERT_THAT(IsTrue(
				Funcdef->Type.CompleteDeclaration.Contains(TEXT("(int"))
					&& Funcdef->Type.CompleteDeclaration.EndsWith(TEXT(")")),
				*FString::Printf(
					TEXT("Funcdef complete identity should include its full callable signature; got '%s'"),
					*Funcdef->Type.CompleteDeclaration)));
			FSymbolIdentityInput Identity;
			Identity.Kind = ESymbolKind::Funcdef;
			Identity.Namespace = Funcdef->Type.Namespace;
			Identity.CompleteDeclaration = Funcdef->Type.CompleteDeclaration;
			ASSERT_THAT(AreEqual(
				MakeStableSymbolId(Identity),
				Funcdef->StableId,
				TEXT("Funcdef stable ID should derive from the exported complete declaration")));
		}

		if (EnumSymbol != nullptr)
		{
			ASSERT_THAT(AreEqual(2, EnumSymbol->Type.EnumValues.Num(),
				TEXT("Observer should export every enum value")));
		}
		if (Method != nullptr)
		{
			ASSERT_THAT(IsTrue(Method->Callable.bConst,
				TEXT("Observer should preserve method const qualification")));
			ASSERT_THAT(AreEqual(1, Method->Callable.Parameters.Num(),
				TEXT("Observer should export method parameters")));
			if (Method->Callable.Parameters.Num() == 1)
			{
				ASSERT_THAT(IsTrue(Method->Callable.Parameters[0].bHasDefault,
					TEXT("Observer should preserve default-argument presence")));
				ASSERT_THAT(AreEqual(
					FString(TEXT("2")),
					Method->Callable.Parameters[0].DefaultExpression,
					TEXT("Observer should preserve the declarative default expression")));
			}
		}

		TSet<FString> StableIds;
		for (const FSymbolRecord& Symbol : Export.Symbols)
		{
			ASSERT_THAT(IsFalse(Symbol.StableId.IsEmpty(),
				TEXT("Every exported declaration should have a stable semantic ID")));
			ASSERT_THAT(IsFalse(StableIds.Contains(Symbol.StableId),
				TEXT("Final symbol output should not contain duplicate stable IDs")));
			StableIds.Add(Symbol.StableId);
		}
	}

	TEST_METHOD(InitializedUnrealEngineSurfaceIncludesRepresentativeManualBindings)
	{
		using namespace AngelscriptOfflineContract;

		ASSERT_THAT(IsTrue(FAngelscriptEngine::IsInitialized(),
			TEXT("Offline exporter integration test requires the initialized plugin engine")));
		if (!FAngelscriptEngine::IsInitialized())
		{
			return;
		}

		asIScriptEngine* ScriptEngine =
			FAngelscriptEngine::Get().GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Initialized plugin engine should expose its final script engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FSymbolExportResult Export =
			FAngelscriptOfflineSymbolExporter::ExportHostSurface(*ScriptEngine);
		ASSERT_THAT(IsTrue(Export.bSuccess,
			TEXT("Observer should traverse the initialized Unreal registration surface")));
		ASSERT_THAT(IsTrue(Export.SymbolScope.bComplete,
			TEXT("Initialized Unreal traversal should produce complete symbol scope")));

		const FSymbolRecord* VectorType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("FVector"));
		const FSymbolRecord* StringType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("FString"));
		const FSymbolRecord* ActorType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("AActor"));
		const FSymbolRecord* NameType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("FName"));
		const FSymbolRecord* KeyMappingRowType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("FKeyMappingRow"));
		const FSymbolRecord* TopLevelAssetPathType =
			FindSymbol(Export.Symbols, ESymbolKind::Type, TEXT("FTopLevelAssetPath"));

		ASSERT_THAT(IsNotNull(VectorType,
			TEXT("Final engine surface should contain FVector")));
		ASSERT_THAT(IsNotNull(StringType,
			TEXT("Final engine surface should contain FString")));
		ASSERT_THAT(IsNotNull(ActorType,
			TEXT("Final engine surface should contain AActor")));
		ASSERT_THAT(IsNotNull(NameType,
			TEXT("Final engine surface should contain FName")));
		ASSERT_THAT(IsNotNull(KeyMappingRowType,
			TEXT("Final engine surface should contain FKeyMappingRow")));
		ASSERT_THAT(IsNotNull(TopLevelAssetPathType,
			TEXT("Final engine surface should contain FTopLevelAssetPath")));

		if (VectorType != nullptr)
		{
			ASSERT_THAT(AreEqual(ETypeKind::Value, VectorType->Type.Kind,
				TEXT("FVector should export as a compile-time value type")));
			ASSERT_THAT(IsTrue(VectorType->Type.CompileSize > 0,
				TEXT("FVector should retain its compile-time size fact")));
			ASSERT_THAT(IsTrue(
				VectorType->Type.UETypePath.StartsWith(TEXT("/Script/")),
				TEXT("FVector should be supplemented with its reflected UE type path")));
		}
		if (StringType != nullptr)
		{
			ASSERT_THAT(IsTrue(
				StringType->Type.Flags.Contains(TEXT("string-factory")),
				TEXT("FString should record the final dynamic string-factory registration")));
			ASSERT_THAT(IsNotNull(
				FindOwnedCallable(
					Export.Symbols,
					StringType->StableId,
					TEXT("opAssign")),
				TEXT("FString should expose its manual conversion/assignment surface")));
		}
		if (ActorType != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("/Script/Engine.Actor")),
				ActorType->Type.UETypePath,
				TEXT("AActor should be supplemented with its reflected UE type path")));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Engine")),
				ActorType->Type.Origin.Module,
				TEXT("AActor should record its reflected owning module")));
			const FSymbolRecord* GetActorLocation = FindOwnedCallable(
				Export.Symbols,
				ActorType->StableId,
				TEXT("GetActorLocation"));
			ASSERT_THAT(IsNotNull(GetActorLocation,
				TEXT("AActor should expose the manually bound GetActorLocation declaration")));
			if (GetActorLocation != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("FVector")),
					GetActorLocation->Callable.ReturnType,
					TEXT("AActor manual binding should preserve its FVector return declaration")));
				ASSERT_THAT(IsTrue(GetActorLocation->Callable.bConst,
					TEXT("AActor manual binding should preserve const qualification")));
				ASSERT_THAT(IsTrue(
					GetActorLocation->Callable.UEFunctionPath
						== TEXT("/Script/Engine.Actor:K2_GetActorLocation"),
					TEXT("Manual bindings should retain the real reflected UFunction path while the callable name retains its ScriptName alias")));
				ASSERT_THAT(AreEqual(EOriginKind::Manual,
					GetActorLocation->Callable.Origin.Kind,
					TEXT("Central bind-provider metadata should prove the AActor binding is manual")));
				ASSERT_THAT(AreEqual(
					FString(TEXT("Engine")),
					GetActorLocation->Callable.Origin.Module,
					TEXT("Manual UFunction supplementation should record its owning module")));
			}
		}
		if (NameType != nullptr)
		{
			ASSERT_THAT(IsTrue(NameType->Type.Traits.bComparable,
				TEXT("Final FName opEquals/opCmp surface should infer comparable")));
			ASSERT_THAT(IsTrue(NameType->Type.Traits.bHashable,
				TEXT("Final FName GetHash surface should infer hashable")));
		}
		if (KeyMappingRowType != nullptr)
		{
			ASSERT_THAT(IsTrue(KeyMappingRowType->Type.Traits.bCopyConstructible,
				TEXT("Final FKeyMappingRow copy constructor surface should infer copy-constructible")));
			ASSERT_THAT(IsTrue(KeyMappingRowType->Type.Traits.bCopyAssignable,
				TEXT("Final FKeyMappingRow opAssign surface should infer copy-assignable")));
		}
		if (TopLevelAssetPathType != nullptr)
		{
			ASSERT_THAT(IsTrue(TopLevelAssetPathType->Type.Traits.bHashable,
				TEXT("Final FTopLevelAssetPath engine type usage should infer hashable even without a public GetHash method")));
		}

		const FSymbolRecord* ReflectiveCallable =
			Export.Symbols.FindByPredicate([](const FSymbolRecord& Symbol)
			{
				return Symbol.Kind == ESymbolKind::Callable
					&& Symbol.Callable.Origin.Kind == EOriginKind::Reflective;
			});
		ASSERT_THAT(IsNotNull(ReflectiveCallable,
			TEXT("Initialized UE surface should expose at least one proven reflective fallback")));
		if (ReflectiveCallable != nullptr)
		{
			ASSERT_THAT(IsFalse(ReflectiveCallable->Callable.UEFunctionPath.IsEmpty(),
				TEXT("Reflective provenance should carry a matching UFunction path")));
		}

		const FAdapterRecord* ArrayAdapter =
			Export.Adapters.FindByPredicate([](
				const FAdapterRecord& Adapter)
			{
				return Adapter.Name == TEXT("TArray");
			});
		ASSERT_THAT(IsNotNull(ArrayAdapter,
			TEXT("The final template surface should publish a TArray adapter handshake")));
		if (ArrayAdapter != nullptr)
		{
			ASSERT_THAT(IsFalse(ArrayAdapter->StableId.IsEmpty(),
				TEXT("Adapter handshakes require a stable adapter identity")));
			ASSERT_THAT(IsFalse(ArrayAdapter->SurfaceHash.IsEmpty(),
				TEXT("Adapter handshakes require a final registered surface hash")));
			ASSERT_THAT(IsFalse(ArrayAdapter->bDeclarativeOnly,
				TEXT("Standalone template adapters must remain compile-only")));
		}
	}

	TEST_METHOD(AdapterSurfaceHashChangesWithRegisteredDeclaration)
	{
		using namespace AngelscriptOfflineContract;

		FSymbolRecord ArrayType;
		ArrayType.Kind = ESymbolKind::Type;
		ArrayType.StableId = TEXT("type-array");
		ArrayType.Type.StableId = ArrayType.StableId;
		ArrayType.Type.Name = TEXT("TArray");
		ArrayType.Type.CompleteDeclaration = TEXT("TArray<T>");
		ArrayType.Type.Kind = ETypeKind::Template;

		FSymbolRecord ArrayMethod;
		ArrayMethod.Kind = ESymbolKind::Callable;
		ArrayMethod.StableId = TEXT("method-array-num");
		ArrayMethod.Callable.StableId = ArrayMethod.StableId;
		ArrayMethod.Callable.OwnerStableId = ArrayType.StableId;
		ArrayMethod.Callable.Name = TEXT("Num");
		ArrayMethod.Callable.Declaration = TEXT("int Num() const");

		FSymbolRecord ArrayIterator;
		ArrayIterator.Kind = ESymbolKind::Type;
		ArrayIterator.StableId = TEXT("type-array-iterator");
		ArrayIterator.Type.StableId = ArrayIterator.StableId;
		ArrayIterator.Type.Name = TEXT("TArrayIterator");
		ArrayIterator.Type.CompleteDeclaration =
			TEXT("TArrayIterator<T>");
		ArrayIterator.Type.Kind = ETypeKind::Template;

		TArray<FSymbolRecord> FirstSymbols = {
			ArrayType,
			ArrayMethod,
			ArrayIterator,
		};
		const TArray<FAdapterRecord> First =
			FAngelscriptOfflineAdapterExporter::ExportAndAssign(
				FirstSymbols);
		ASSERT_THAT(AreEqual(1, First.Num(),
			TEXT("The synthetic TArray surface should produce one adapter")));
		if (First.Num() != 1)
		{
			return;
		}

		ArrayMethod.StableId = TEXT("method-array-num-changed");
		ArrayMethod.Callable.StableId = ArrayMethod.StableId;
		ArrayMethod.Callable.Declaration =
			TEXT("int64 Num() const");
		TArray<FSymbolRecord> SecondSymbols = {
			ArrayType,
			ArrayMethod,
			ArrayIterator,
		};
		const TArray<FAdapterRecord> Second =
			FAngelscriptOfflineAdapterExporter::ExportAndAssign(
				SecondSymbols);
		ASSERT_THAT(AreEqual(1, Second.Num(),
			TEXT("The changed TArray surface should retain one adapter")));
		if (Second.Num() != 1)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			First[0].StableId,
			Second[0].StableId,
			TEXT("Adapter identity should depend on adapter name/version, not surface order")));
		ASSERT_THAT(IsTrue(
			First[0].SurfaceHash != Second[0].SurfaceHash,
			TEXT("Changing one registered method must invalidate the adapter surface hash")));
		ASSERT_THAT(AreEqual(
			First[0].StableId,
			FirstSymbols[0].Type.AdapterStableId,
			TEXT("The matching template type should reference its adapter")));
		ASSERT_THAT(AreEqual(
			First[0].StableId,
			FirstSymbols[1].Callable.AdapterStableId,
			TEXT("The matching template member should reference its adapter")));
		ASSERT_THAT(AreEqual(
			First[0].StableId,
			FirstSymbols[2].Type.AdapterStableId,
			TEXT("The related iterator template should reference the container adapter")));
	}

	TEST_METHOD(BundleWriterPublishesCanonicalCompleteSnapshot)
	{
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = FPaths::Combine(
			FPaths::ProjectIntermediateDir(),
			TEXT("OfflineBundleWriterTests"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("bundle"));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};

		FManifestRecord Manifest;
		Manifest.BundleKind = EBundleKind::Project;
		Manifest.ProducerName = TEXT("AngelscriptRuntime");
		Manifest.ProducerVersion = TEXT("test");
		Manifest.SymbolScope.bComplete = true;
		Manifest.SymbolScope.State = TEXT("final-engine-observed");
		Manifest.AssetScope.bComplete = false;
		Manifest.AssetScope.State = TEXT("fixture-incomplete");

		FSymbolRecord Later;
		Later.Kind = ESymbolKind::Type;
		Later.StableId = TEXT("symbol-z");
		Later.Type.StableId = Later.StableId;
		Later.Type.Name = TEXT("ZType");
		Later.Type.CompleteDeclaration = TEXT("ZType");

		FSymbolRecord Earlier = Later;
		Earlier.StableId = TEXT("symbol-a");
		Earlier.Type.StableId = Earlier.StableId;
		Earlier.Type.Name = TEXT("AType");
		Earlier.Type.CompleteDeclaration = TEXT("AType");

		FAssetRecord Asset;
		Asset.StableId = TEXT("asset-a");
		Asset.PackagePath = TEXT("/Game/Fixture");
		Asset.ObjectPath = TEXT("/Game/Fixture.Fixture");

		FBundleWriteRequest Request;
		Request.OutputDirectory = OutputDirectory;
		Request.Manifest = Manifest;
		Request.Symbols = {Later, Earlier};
		Request.Assets = {Asset};

		const FBundleWriteResult First =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsTrue(First.bSuccess,
			*FString::Printf(
				TEXT("Canonical bundle publication should succeed: %s"),
				*First.Error)));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(OutputDirectory, TEXT("manifest.json"))),
			TEXT("Successful publication should expose manifest.json")));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(OutputDirectory, TEXT("symbols.jsonl"))),
			TEXT("Successful publication should expose symbols.jsonl")));
		ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
			*FPaths::Combine(OutputDirectory, TEXT("assets.jsonl"))),
			TEXT("Successful publication should expose assets.jsonl")));

		TArray<uint8> FirstManifest;
		TArray<uint8> FirstSymbols;
		TArray<uint8> FirstAssets;
		FFileHelper::LoadFileToArray(
			FirstManifest,
			*FPaths::Combine(OutputDirectory, TEXT("manifest.json")));
		FFileHelper::LoadFileToArray(
			FirstSymbols,
			*FPaths::Combine(OutputDirectory, TEXT("symbols.jsonl")));
		FFileHelper::LoadFileToArray(
			FirstAssets,
			*FPaths::Combine(OutputDirectory, TEXT("assets.jsonl")));

		const FBundleWriteResult Second =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsTrue(Second.bSuccess,
			*FString::Printf(
				TEXT("Replacing a complete bundle should succeed: %s"),
				*Second.Error)));

		TArray<uint8> SecondManifest;
		TArray<uint8> SecondSymbols;
		TArray<uint8> SecondAssets;
		FFileHelper::LoadFileToArray(
			SecondManifest,
			*FPaths::Combine(OutputDirectory, TEXT("manifest.json")));
		FFileHelper::LoadFileToArray(
			SecondSymbols,
			*FPaths::Combine(OutputDirectory, TEXT("symbols.jsonl")));
		FFileHelper::LoadFileToArray(
			SecondAssets,
			*FPaths::Combine(OutputDirectory, TEXT("assets.jsonl")));

		ASSERT_THAT(AreEqual(FirstManifest, SecondManifest,
			TEXT("Repeated manifest publication should be byte-identical")));
		ASSERT_THAT(AreEqual(FirstSymbols, SecondSymbols,
			TEXT("Repeated symbol publication should be byte-identical")));
		ASSERT_THAT(AreEqual(FirstAssets, SecondAssets,
			TEXT("Repeated asset publication should be byte-identical")));
		ASSERT_THAT(IsFalse(
			First.SymbolFile.Sha256.IsEmpty(),
			TEXT("Successful publication should report a symbol file hash")));

		const FAngelscriptOfflineBundleFixtureReadResult ReadBack =
			FAngelscriptOfflineBundleFixtureReader::Read(OutputDirectory);
		ASSERT_THAT(IsTrue(ReadBack.bSuccess,
			*FString::Printf(
				TEXT("The strict producer fixture reader should accept the published bundle: %s"),
				*ReadBack.Error)));
		ASSERT_THAT(AreEqual(
			static_cast<int64>(2),
			ReadBack.SymbolCount,
			TEXT("The fixture reader should verify the symbol record count")));
		ASSERT_THAT(AreEqual(
			static_cast<int64>(1),
			ReadBack.AssetCount,
			TEXT("The fixture reader should verify the asset record count")));
	}

	TEST_METHOD(FrozenFixtureReaderRejectsIntegrityAndCompatibilityViolations)
	{
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = FPaths::Combine(
			FPaths::ProjectIntermediateDir(),
			TEXT("OfflineBundleFixtureReaderTests"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString ValidDirectory =
			FPaths::Combine(TestRoot, TEXT("valid"));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};

		FBundleWriteRequest Request;
		Request.OutputDirectory = ValidDirectory;
		Request.Manifest.BundleKind = EBundleKind::Project;
		Request.Manifest.SymbolScope.bComplete = true;
		FSymbolRecord Symbol;
		Symbol.Kind = ESymbolKind::Type;
		Symbol.StableId = TEXT("symbol-a");
		Symbol.Type.StableId = Symbol.StableId;
		Symbol.Type.Name = TEXT("FixtureType");
		Symbol.Type.CompleteDeclaration = TEXT("FixtureType");
		FAssetRecord Asset;
		Asset.StableId = TEXT("asset-a");
		Asset.ObjectPath = TEXT("/Game/Fixture.Fixture");
		FAssetRecord SecondAsset = Asset;
		SecondAsset.StableId = TEXT("asset-b");
		SecondAsset.ObjectPath = TEXT("/Game/Second.Second");
		Request.Symbols = {Symbol};
		Request.Assets = {Asset, SecondAsset};

		const FBundleWriteResult Written =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsTrue(Written.bSuccess,
			TEXT("The strict-reader source fixture must publish successfully")));
		if (!Written.bSuccess)
		{
			return;
		}

		auto CopyFixture = [&](
			const TCHAR* Name) -> FString
		{
			const FString Destination =
				FPaths::Combine(TestRoot, Name);
			const bool bCopied =
				FPlatformFileManager::Get()
					.GetPlatformFile()
					.CopyDirectoryTree(
					*Destination,
					*ValidDirectory,
					true);
			return bCopied ? Destination : FString();
		};

		const FString MissingDirectory = CopyFixture(TEXT("missing"));
		IFileManager::Get().Delete(
			*FPaths::Combine(MissingDirectory, TEXT("assets.jsonl")));
		ASSERT_THAT(IsFalse(
			FAngelscriptOfflineBundleFixtureReader::Read(
				MissingDirectory).bSuccess,
			TEXT("A missing required record file must invalidate the fixture")));

		const FString SchemaDirectory = CopyFixture(TEXT("schema"));
		const FString SchemaManifest = FPaths::Combine(
			SchemaDirectory,
			TEXT("manifest.json"));
		FString SchemaText;
		FFileHelper::LoadFileToString(SchemaText, *SchemaManifest);
		SchemaText.ReplaceInline(
			TEXT("\"major\":1"),
			TEXT("\"major\":999"),
			ESearchCase::CaseSensitive);
		FFileHelper::SaveStringToFile(
			SchemaText,
			*SchemaManifest,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		ASSERT_THAT(IsFalse(
			FAngelscriptOfflineBundleFixtureReader::Read(
				SchemaDirectory).bSuccess,
			TEXT("An unsupported major schema must invalidate the fixture")));

		const FString RequiredDirectory =
			CopyFixture(TEXT("required-field"));
		const FString RequiredManifest = FPaths::Combine(
			RequiredDirectory,
			TEXT("manifest.json"));
		FString RequiredText;
		FFileHelper::LoadFileToString(
			RequiredText,
			*RequiredManifest);
		RequiredText.ReplaceInline(
			TEXT("\"requiredFields\":[]"),
			TEXT("\"requiredFields\":[\"future.required\"]"),
			ESearchCase::CaseSensitive);
		FFileHelper::SaveStringToFile(
			RequiredText,
			*RequiredManifest,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		ASSERT_THAT(IsFalse(
			FAngelscriptOfflineBundleFixtureReader::Read(
				RequiredDirectory).bSuccess,
			TEXT("An unsupported required field must invalidate the fixture")));

		const FString Utf8Directory = CopyFixture(TEXT("utf8"));
		const TArray<uint8> InvalidUtf8 = {0xff, '\n'};
		FFileHelper::SaveArrayToFile(
			InvalidUtf8,
			*FPaths::Combine(Utf8Directory, TEXT("symbols.jsonl")));
		ASSERT_THAT(IsFalse(
			FAngelscriptOfflineBundleFixtureReader::Read(
				Utf8Directory).bSuccess,
			TEXT("Invalid UTF-8 must be rejected before record parsing")));

		const FString HashDirectory = CopyFixture(TEXT("hash"));
		const FString HashSymbols = FPaths::Combine(
			HashDirectory,
			TEXT("symbols.jsonl"));
		TArray<uint8> HashBytes;
		FFileHelper::LoadFileToArray(HashBytes, *HashSymbols);
		HashBytes.Add(static_cast<uint8>('\n'));
		FFileHelper::SaveArrayToFile(HashBytes, *HashSymbols);
		ASSERT_THAT(IsFalse(
			FAngelscriptOfflineBundleFixtureReader::Read(
				HashDirectory).bSuccess,
			TEXT("A record-file hash mismatch must invalidate the fixture")));

		const FString CountDirectory = CopyFixture(TEXT("count"));
		const FString CountManifest = FPaths::Combine(
			CountDirectory,
			TEXT("manifest.json"));
		FString CountText;
		FFileHelper::LoadFileToString(CountText, *CountManifest);
		CountText.ReplaceInline(
			TEXT("\"recordCount\":1"),
			TEXT("\"recordCount\":2"),
			ESearchCase::CaseSensitive);
		FFileHelper::SaveStringToFile(
			CountText,
			*CountManifest,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		ASSERT_THAT(IsFalse(
			FAngelscriptOfflineBundleFixtureReader::Read(
				CountDirectory).bSuccess,
			TEXT("A record-count mismatch must invalidate the fixture")));

		const FString DuplicateDirectory =
			CopyFixture(TEXT("duplicate"));
		const FString DuplicateSymbols = FPaths::Combine(
			DuplicateDirectory,
			TEXT("symbols.jsonl"));
		TArray<uint8> DuplicateBytes;
		FFileHelper::LoadFileToArray(
			DuplicateBytes,
			*DuplicateSymbols);
		const FString DuplicateLine =
			TEXT("{\"kind\":\"type\",\"stableId\":\"symbol-a\",\"type\":{\"name\":\"Changed\"}}\n");
		const FTCHARToUTF8 DuplicateUtf8(*DuplicateLine);
		DuplicateBytes.Append(
			reinterpret_cast<const uint8*>(DuplicateUtf8.Get()),
			DuplicateUtf8.Length());
		FFileHelper::SaveArrayToFile(
			DuplicateBytes,
			*DuplicateSymbols);

		const FString DuplicateManifest = FPaths::Combine(
			DuplicateDirectory,
			TEXT("manifest.json"));
		FString DuplicateManifestText;
		FFileHelper::LoadFileToString(
			DuplicateManifestText,
			*DuplicateManifest);
		DuplicateManifestText.ReplaceInline(
			*FString::Printf(
				TEXT("\"byteCount\":%lld"),
				Written.SymbolFile.ByteCount),
			*FString::Printf(
				TEXT("\"byteCount\":%d"),
				DuplicateBytes.Num()),
			ESearchCase::CaseSensitive);
		DuplicateManifestText.ReplaceInline(
			TEXT("\"recordCount\":1"),
			TEXT("\"recordCount\":2"),
			ESearchCase::CaseSensitive);
		DuplicateManifestText.ReplaceInline(
			*FString::Printf(
				TEXT("\"sha256\":\"%s\""),
				*Written.SymbolFile.Sha256),
			*FString::Printf(
				TEXT("\"sha256\":\"%s\""),
				*Sha256Bytes(DuplicateBytes)),
			ESearchCase::CaseSensitive);
		FFileHelper::SaveStringToFile(
			DuplicateManifestText,
			*DuplicateManifest,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const FAngelscriptOfflineBundleFixtureReadResult Duplicate =
			FAngelscriptOfflineBundleFixtureReader::Read(
				DuplicateDirectory);
		ASSERT_THAT(IsFalse(Duplicate.bSuccess,
			TEXT("Inconsistent duplicate stable IDs must invalidate the fixture")));
		ASSERT_THAT(IsTrue(
			Duplicate.Error.Contains(TEXT("Inconsistent duplicate")),
			TEXT("Duplicate rejection should identify the stable-ID conflict")));
	}

	TEST_METHOD(BundleWriterRejectsIncompleteOrAmbiguousSnapshots)
	{
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = FPaths::Combine(
			FPaths::ProjectIntermediateDir(),
			TEXT("OfflineBundleWriterTests"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};

		FBundleWriteRequest Request;
		Request.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("incomplete"));
		Request.Manifest.BundleKind = EBundleKind::Project;
		Request.Manifest.SymbolScope.bComplete = false;

		const FBundleWriteResult Incomplete =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsFalse(Incomplete.bSuccess,
			TEXT("An incomplete symbol snapshot must not be published")));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*Request.OutputDirectory),
			TEXT("Rejected incomplete snapshots must not create a destination")));

		Request.Manifest.SymbolScope.bComplete = true;
		Request.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("duplicate"));
		FSymbolRecord Symbol;
		Symbol.StableId = TEXT("duplicate-symbol");
		Symbol.Kind = ESymbolKind::Type;
		Symbol.Type.StableId = Symbol.StableId;
		Request.Symbols = {Symbol, Symbol};

		const FBundleWriteResult Duplicate =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsFalse(Duplicate.bSuccess,
			TEXT("Duplicate stable IDs must be rejected before publication")));
		ASSERT_THAT(IsTrue(
			Duplicate.Error.Contains(TEXT("Duplicate symbol stable ID")),
			TEXT("Duplicate rejection should identify the integrity violation")));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*Request.OutputDirectory),
			TEXT("Rejected ambiguous snapshots must not create a destination")));

		Request.OutputDirectory =
			FPaths::Combine(TestRoot, TEXT("empty"));
		Request.Symbols[1].StableId.Reset();
		const FBundleWriteResult Empty =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsFalse(Empty.bSuccess,
			TEXT("Empty stable IDs must be rejected before serialization")));
		ASSERT_THAT(IsFalse(IFileManager::Get().DirectoryExists(
			*Request.OutputDirectory),
			TEXT("Rejected empty identities must not create a destination")));
	}

	TEST_METHOD(BundleWriterDoesNotExposeManifestWhenPublicationFails)
	{
		using namespace AngelscriptOfflineContract;

		const FString TestRoot = FPaths::Combine(
			FPaths::ProjectIntermediateDir(),
			TEXT("OfflineBundleWriterTests"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits));
		const FString BlockedOutput =
			FPaths::Combine(TestRoot, TEXT("bundle"));
		ON_SCOPE_EXIT
		{
			IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
		};
		IFileManager::Get().MakeDirectory(*TestRoot, true);
		const bool bCreatedBlocker = FFileHelper::SaveStringToFile(
			TEXT("publication blocker"),
			*BlockedOutput);
		ASSERT_THAT(IsTrue(bCreatedBlocker,
			TEXT("The fixture must create a file at the destination path")));

		FBundleWriteRequest Request;
		Request.OutputDirectory = BlockedOutput;
		Request.Manifest.SymbolScope.bComplete = true;
		FSymbolRecord Symbol;
		Symbol.StableId = TEXT("symbol-a");
		Symbol.Kind = ESymbolKind::Type;
		Symbol.Type.StableId = Symbol.StableId;
		Request.Symbols = {Symbol};

		TestRunner->AddExpectedErrorPlain(
			TEXT("MoveFile was unable to move"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("Error moving file"),
			EAutomationExpectedErrorFlags::Contains,
			0);
		const FBundleWriteResult Result =
			FAngelscriptOfflineBundleWriter::Write(Request);
		ASSERT_THAT(IsFalse(Result.bSuccess,
			TEXT("A blocked atomic rename must report publication failure")));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(
			*FPaths::Combine(BlockedOutput, TEXT("manifest.json"))),
			TEXT("A failed publication must never expose a valid manifest")));

		TArray<FString> StagingArtifacts;
		IFileManager::Get().FindFiles(
			StagingArtifacts,
			*(BlockedOutput + TEXT(".staging-*")),
			false,
			true);
		ASSERT_THAT(AreEqual(0, StagingArtifacts.Num(),
			TEXT("A failed publication must clean its staging directory")));
	}

	TEST_METHOD(ScriptBaselineExportsActiveDeclarationsWithoutCode)
	{
		using namespace AngelscriptOfflineContract;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const FName ModuleName(TEXT("ASOfflineContractScriptBaseline"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString Source = TEXT(R"AS(
// BODY_SECRET_SENTINEL must never enter an offline bundle.
enum EBaselineFixture
{
	First = 1,
	Second = 2
}

class FBaselineFixture
{
	int Value;

	int Compute(int Input)
	{
		return Input + 37;
	}
}

const int BaselineGlobal = 9;

int BaselineFunction(int Input)
{
	return Input + 11;
}
)AS");
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			ModuleName,
			TEXT("Offline/ScriptBaseline.as"),
			Source);
		ASSERT_THAT(IsTrue(bCompiled,
			TEXT("The script-baseline fixture must compile successfully")));
		if (!bCompiled)
		{
			return;
		}

		const FSymbolExportResult Result =
			FAngelscriptOfflineSymbolExporter::ExportScriptBaseline(Engine);
		ASSERT_THAT(IsTrue(Result.bSuccess,
			*FString::Printf(
				TEXT("Active script-baseline export should succeed: %s"),
				*Result.Error)));
		ASSERT_THAT(IsTrue(Result.SymbolScope.bComplete,
			TEXT("Active-module observation should produce a complete baseline scope")));

		const FSymbolRecord* Type = FindSymbol(
			Result.Symbols,
			ESymbolKind::Type,
			TEXT("FBaselineFixture"));
		const FSymbolRecord* Function = FindSymbol(
			Result.Symbols,
			ESymbolKind::Callable,
			TEXT("BaselineFunction"));
		const FSymbolRecord* Global = FindSymbol(
			Result.Symbols,
			ESymbolKind::Global,
			TEXT("BaselineGlobal"));
		ASSERT_THAT(IsNotNull(Type,
			TEXT("The script class declaration should be exported")));
		ASSERT_THAT(IsNotNull(Function,
			TEXT("The global function declaration should be exported")));
		ASSERT_THAT(IsNotNull(Global,
			TEXT("The global declaration should be exported")));

		for (const FSymbolRecord& Symbol : Result.Symbols)
		{
			ASSERT_THAT(AreEqual(
				EOriginLayer::ScriptBaseline,
				Symbol.Origin.Layer,
				TEXT("Every active-module record must be marked script-baseline")));
			ASSERT_THAT(AreEqual(
				EOriginKind::Script,
				Symbol.Origin.Kind,
				TEXT("Every active-module record must retain script provenance")));
			ASSERT_THAT(IsFalse(
				Symbol.Origin.StableModuleId.IsEmpty(),
				TEXT("Every active-module record must name a stable logical module")));
		}

		const FString Serialized =
			Utf8BytesToString(SerializeSymbolRecords(Result.Symbols));
		ASSERT_THAT(IsFalse(
			Serialized.Contains(TEXT("BODY_SECRET_SENTINEL")),
			TEXT("The exported baseline must not contain source comments")));
		ASSERT_THAT(IsFalse(
			Serialized.Contains(TEXT("return Input +")),
			TEXT("The exported baseline must not contain function bodies")));

		FAngelscriptCompileTraceSummary FailedReload;
		const bool bFailedReloadCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			TEXT("Offline/ScriptBaseline.as"),
			TEXT("int BrokenReplacement( {"),
			false,
			FailedReload,
			true);
		ASSERT_THAT(IsFalse(bFailedReloadCompiled,
			TEXT("The invalid replacement fixture must fail compilation")));

		const FSymbolExportResult AfterFailedReload =
			FAngelscriptOfflineSymbolExporter::ExportScriptBaseline(Engine);
		ASSERT_THAT(IsTrue(AfterFailedReload.bSuccess,
			TEXT("A failed replacement must not invalidate the last successful baseline")));
		ASSERT_THAT(IsNotNull(FindSymbol(
			AfterFailedReload.Symbols,
			ESymbolKind::Callable,
			TEXT("BaselineFunction")),
			TEXT("The last successful active module must survive a failed replacement")));
		ASSERT_THAT(IsNull(FindSymbol(
			AfterFailedReload.Symbols,
			ESymbolKind::Callable,
			TEXT("BrokenReplacement")),
			TEXT("Failed replacement declarations must not enter the baseline")));

		FOfflineExportBuildRequest ProjectRequest;
		ProjectRequest.BundleKind = EBundleKind::Project;
		ProjectRequest.AssetScope.bComplete = false;
		ProjectRequest.AssetScope.State =
			TEXT("test-assets-not-scanned");
		const FOfflineExportBuildResult FirstBundle =
			FAngelscriptOfflineExportService::Build(
				Engine,
				ProjectRequest);
		const FOfflineExportBuildResult SecondBundle =
			FAngelscriptOfflineExportService::Build(
				Engine,
				ProjectRequest);
		ASSERT_THAT(IsTrue(FirstBundle.bSuccess,
			*FString::Printf(
				TEXT("The project bundle assembler should combine both symbol layers: %s"),
				*FirstBundle.Error)));
		ASSERT_THAT(IsTrue(SecondBundle.bSuccess,
			TEXT("A repeated final-state observation should also succeed")));
		if (!FirstBundle.bSuccess || !SecondBundle.bSuccess)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			SerializeSymbolRecords(FirstBundle.Bundle.Symbols),
			SerializeSymbolRecords(SecondBundle.Bundle.Symbols),
			TEXT("Two exports of the same final engine must have byte-identical symbols")));
		ASSERT_THAT(AreEqual(
			SerializeCanonicalJsonDocument(
				ToCanonicalJson(FirstBundle.Bundle.Manifest)),
			SerializeCanonicalJsonDocument(
				ToCanonicalJson(SecondBundle.Bundle.Manifest)),
			TEXT("Two exports of the same final engine must have byte-identical manifests before file hashes")));
		ASSERT_THAT(IsTrue(
			FirstBundle.Bundle.Manifest.SymbolScope.bComplete,
			TEXT("The assembled project bundle must preserve complete symbol scope")));
		ASSERT_THAT(IsFalse(
			FirstBundle.Bundle.Manifest.AssetScope.bComplete,
			TEXT("Asset completeness must remain independent from symbol completeness")));
		ASSERT_THAT(IsFalse(
			FirstBundle.Bundle.Manifest.Adapters.IsEmpty(),
			TEXT("The assembled manifest must carry adapter handshakes")));
		ASSERT_THAT(AreEqual(
			FString(FApp::GetProjectName()),
			FirstBundle.Bundle.Manifest.EngineProperties.FindRef(
				TEXT("unreal.project-name")),
			TEXT("The manifest must identify the Unreal project that produced the complete snapshot")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
