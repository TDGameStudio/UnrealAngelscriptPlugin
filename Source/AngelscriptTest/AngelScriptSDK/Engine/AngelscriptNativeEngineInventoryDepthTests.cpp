#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEngineInventoryDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.InventoryDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FRegisteredTypeCase
	{
		const TCHAR* Id;
		const char* Name;
		const char* Namespace;
		int32 TypeId;
		bool bEnum;
	};

	struct FPrimitiveSizeCase
	{
		const TCHAR* Id;
		int32 TypeId;
		int32 ExpectedSize;
	};

	static FString BuildReviewSource(
		const TCHAR* Operation,
		const FString& Input,
		const FString& Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Operation: %s"), Operation));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Input: %s"), *Input));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Expected: %s"), *Expected));
		return Source;
	}

	static void PrintReviewSource(
		const TCHAR* Id,
		const TCHAR* Operation,
		const FString& Input,
		const FString& Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			*TestRunner,
			Id,
			TEXT("EngineInventoryDepth"),
			BuildReviewSource(Operation, Input, Expected));
	}

	static bool RegisterInventoryContracts(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine)
	{
		FNoDiscardAsserter Assert(Test);
		bool bSuccess = true;

		const char* const InitialNamespace = ScriptEngine.GetDefaultNamespace();
		const std::string SavedNamespace =
			InitialNamespace != nullptr ? InitialNamespace : "";

		BaselineObjectTypeCount = ScriptEngine.GetObjectTypeCount();
		BaselineEnumCount = ScriptEngine.GetEnumCount();

		bSuccess &= Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine.SetDefaultNamespace(""),
			TEXT("Engine inventory should select the root namespace before root registrations"));
		RootObjectTypeId = ScriptEngine.RegisterObjectType(
			"InventoryRootObject",
			0,
			asOBJ_REF | asOBJ_NOCOUNT);
		bSuccess &= Assert.IsTrue(
			RootObjectTypeId >= 0,
			TEXT("Engine inventory should register its root object type"));
		RootEnumTypeId = ScriptEngine.RegisterEnum("InventoryRootEnum");
		bSuccess &= Assert.IsTrue(
			RootEnumTypeId >= 0,
			TEXT("Engine inventory should register its root enum type"));
		bSuccess &= Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine.RegisterEnumValue("InventoryRootEnum", "RootValue", 11),
			TEXT("Engine inventory should register its root enum value"));

		bSuccess &= Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine.SetDefaultNamespace("InventoryDepth"),
			TEXT("Engine inventory should select its nested registration namespace"));
		NestedObjectTypeId = ScriptEngine.RegisterObjectType(
			"InventoryNestedObject",
			0,
			asOBJ_REF | asOBJ_NOCOUNT);
		bSuccess &= Assert.IsTrue(
			NestedObjectTypeId >= 0,
			TEXT("Engine inventory should register its nested object type"));
		NestedEnumTypeId = ScriptEngine.RegisterEnum("InventoryNestedEnum");
		bSuccess &= Assert.IsTrue(
			NestedEnumTypeId >= 0,
			TEXT("Engine inventory should register its nested enum type"));
		bSuccess &= Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine.RegisterEnumValue("InventoryNestedEnum", "NestedValue", 22),
			TEXT("Engine inventory should register its nested enum value"));

		bSuccess &= Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			ScriptEngine.SetDefaultNamespace(SavedNamespace.c_str()),
			TEXT("Engine inventory should restore the pre-registration namespace"));
		return bSuccess;
	}

	static asITypeInfo* FindRegisteredTypeByIndex(
		asIScriptEngine& ScriptEngine,
		const FRegisteredTypeCase& TypeCase,
		asUINT& OutIndex)
	{
		const asUINT Count = TypeCase.bEnum
			? ScriptEngine.GetEnumCount()
			: ScriptEngine.GetObjectTypeCount();
		for (asUINT Index = 0; Index < Count; ++Index)
		{
			asITypeInfo* const Type = TypeCase.bEnum
				? ScriptEngine.GetEnumByIndex(Index)
				: ScriptEngine.GetObjectTypeByIndex(Index);
			if (Type != nullptr
				&& FCStringAnsi::Strcmp(Type->GetName(), TypeCase.Name) == 0
				&& FCStringAnsi::Strcmp(Type->GetNamespace(), TypeCase.Namespace) == 0)
			{
				OutIndex = Index;
				return Type;
			}
		}

		OutIndex = MAX_uint32;
		return nullptr;
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static asUINT BaselineObjectTypeCount = 0;
	inline static asUINT BaselineEnumCount = 0;
	inline static int32 RootObjectTypeId = asINVALID_TYPE;
	inline static int32 NestedObjectTypeId = asINVALID_TYPE;
	inline static int32 RootEnumTypeId = asINVALID_TYPE;
	inline static int32 NestedEnumTypeId = asINVALID_TYPE;
	inline static bool bContractsRegistered = false;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine != nullptr)
		{
			bContractsRegistered =
				RegisterInventoryContracts(*TestRunner, *ScriptEngine);
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		BaselineObjectTypeCount = 0;
		BaselineEnumCount = 0;
		RootObjectTypeId = asINVALID_TYPE;
		NestedObjectTypeId = asINVALID_TYPE;
		RootEnumTypeId = asINVALID_TYPE;
		NestedEnumTypeId = asINVALID_TYPE;
		bContractsRegistered = false;
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
	}

	TEST_METHOD(ReferenceCountRemainsUsableAfterBalancedPair)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-ENGINE-REFCOUNT-LIFETIME",
			ENativeEvidence::Runtime
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Engine reference-count depth should have a raw engine")));
		ASSERT_THAT(IsTrue(
			bContractsRegistered,
			TEXT("Engine reference-count depth should have registered its shared contracts")));

		PrintReviewSource(
			TEXT("ENG-ENGINE-REFCOUNT-LIFETIME-BALANCED"),
			TEXT("asIScriptEngine AddRef and Release"),
			TEXT("one balanced pair"),
			TEXT("engine remains usable with the fixture-owned reference"));

		const int32 AddedCount = ScriptEngine->AddRef();
		const int32 ReleasedCount = ScriptEngine->Release();

		ASSERT_THAT(IsTrue(
			AddedCount >= 2,
			TEXT("Engine AddRef should retain at least the fixture and balanced temporary references")));
		ASSERT_THAT(AreEqual(
			AddedCount - 1,
			ReleasedCount,
			TEXT("Engine Release should remove exactly the temporary reference")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(BaselineObjectTypeCount + 2),
			static_cast<int32>(ScriptEngine->GetObjectTypeCount()),
			TEXT("Engine should remain usable after the balanced reference pair")));
	}

	TEST_METHOD(RegisteredTypesByCategoryAndNamespace)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-TYPE-INVENTORY-BY-CATEGORY",
			ENativeEvidence::Metadata
				| ENativeEvidence::Runtime
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Engine inventory depth should have a raw engine")));
		ASSERT_THAT(IsTrue(
			bContractsRegistered,
			TEXT("Engine inventory depth should have registered its shared contracts")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(BaselineObjectTypeCount + 2),
			static_cast<int32>(ScriptEngine->GetObjectTypeCount()),
			TEXT("Object inventory should add exactly the root and nested test types")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(BaselineEnumCount + 2),
			static_cast<int32>(ScriptEngine->GetEnumCount()),
			TEXT("Enum inventory should add exactly the root and nested test enums")));

		const FRegisteredTypeCase TypeCases[] =
		{
			{ TEXT("ENG-TYPE-INVENTORY-BY-CATEGORY-OBJECT-ROOT"), "InventoryRootObject", "", RootObjectTypeId, false },
			{ TEXT("ENG-TYPE-INVENTORY-BY-CATEGORY-OBJECT-NESTED"), "InventoryNestedObject", "InventoryDepth", NestedObjectTypeId, false },
			{ TEXT("ENG-TYPE-INVENTORY-BY-CATEGORY-ENUM-ROOT"), "InventoryRootEnum", "", RootEnumTypeId, true },
			{ TEXT("ENG-TYPE-INVENTORY-BY-CATEGORY-ENUM-NESTED"), "InventoryNestedEnum", "InventoryDepth", NestedEnumTypeId, true },
		};

		for (const FRegisteredTypeCase& TypeCase : TypeCases)
		{
			PrintReviewSource(
				TypeCase.Id,
				TEXT("registered type inventory"),
				FString::Printf(
					TEXT("category=%s; namespace=%hs; name=%hs"),
					TypeCase.bEnum ? TEXT("enum") : TEXT("object"),
					TypeCase.Namespace,
					TypeCase.Name),
				FString::Printf(TEXT("type_id=%d; exact indexed identity"), TypeCase.TypeId));

			asUINT TypeIndex = MAX_uint32;
			asITypeInfo* const Type =
				FindRegisteredTypeByIndex(*ScriptEngine, TypeCase, TypeIndex);
			ASSERT_THAT(IsNotNull(
				Type,
				FString::Printf(TEXT("%s should be discoverable through its category inventory"), TypeCase.Id)));
			ASSERT_THAT(AreNotEqual(
				static_cast<int32>(MAX_uint32),
				static_cast<int32>(TypeIndex),
				FString::Printf(TEXT("%s should expose an exact inventory index"), TypeCase.Id)));
			ASSERT_THAT(AreEqual(
				TypeCase.TypeId,
				Type->GetTypeId(),
				FString::Printf(TEXT("%s should preserve its registered type ID"), TypeCase.Id)));
		}

		ASSERT_THAT(IsNull(
			ScriptEngine->GetObjectTypeByIndex(ScriptEngine->GetObjectTypeCount()),
			TEXT("Object inventory should return null at its one-past-end boundary")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetEnumByIndex(ScriptEngine->GetEnumCount()),
			TEXT("Enum inventory should return null at its one-past-end boundary")));

		FNativeTestEngine DiagnosticEngine;
		DiagnosticEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			DiagnosticEngine.Destroy();
		};
		asIScriptEngine* const DiagnosticScriptEngine = DiagnosticEngine.Get();
		ASSERT_THAT(IsNotNull(
			DiagnosticScriptEngine,
			TEXT("Inventory diagnostic should use an independent raw engine")));
		if (DiagnosticScriptEngine == nullptr)
		{
			return;
		}

		const asUINT DiagnosticObjectBaseline =
			DiagnosticScriptEngine->GetObjectTypeCount();
		const asUINT DiagnosticEnumBaseline =
			DiagnosticScriptEngine->GetEnumCount();
		PrintReviewSource(
			TEXT("ENG-TYPE-INVENTORY-BY-CATEGORY-INVALID-REGISTRATION"),
			TEXT("rejected object registration preserves inventory"),
			TEXT("name=InventoryRejectedObject; flags=0"),
			TEXT("asINVALID_ARG; exact diagnostic; unchanged object and enum counts"));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asINVALID_ARG),
			DiagnosticScriptEngine->RegisterObjectType(
				"InventoryRejectedObject",
				0,
				0),
			TEXT("Inventory diagnostic should reject an object with no value/reference category")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(DiagnosticObjectBaseline),
			static_cast<int32>(DiagnosticScriptEngine->GetObjectTypeCount()),
			TEXT("Rejected object registration should not change the independent object inventory")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(DiagnosticEnumBaseline),
			static_cast<int32>(DiagnosticScriptEngine->GetEnumCount()),
			TEXT("Rejected object registration should not contaminate the independent enum inventory")));

		const FNativeMessageCollector& DiagnosticMessages =
			DiagnosticEngine.GetMessages();
		ASSERT_THAT(AreEqual(
			1,
			DiagnosticMessages.Entries.Num(),
			TEXT("Rejected object registration should emit exactly one diagnostic")));
		if (DiagnosticMessages.Entries.Num() == 1)
		{
			const FNativeMessageEntry& Diagnostic =
				DiagnosticMessages.Entries[0];
			ASSERT_THAT(AreEqual(
				asMSGTYPE_ERROR,
				Diagnostic.Type,
				TEXT("Rejected object registration diagnostic should be an error")));
			ASSERT_THAT(AreEqual(
				0,
				Diagnostic.Row,
				TEXT("Native registration diagnostic should use row zero")));
			ASSERT_THAT(AreEqual(
				0,
				Diagnostic.Column,
				TEXT("Native registration diagnostic should use column zero")));
			ASSERT_THAT(AreEqual(
				FString(TEXT(
					"Failed in call to function 'RegisterObjectType' with "
					"'InventoryRejectedObject' (Code: asINVALID_ARG, -5)")),
				Diagnostic.Message,
				TEXT("Rejected object registration should publish the exact fork diagnostic")));
		}
	}

	TEST_METHOD(DefaultNamespaceTransitionsAndRestores)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-DEFAULT-NAMESPACE-TRANSITIONS",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Engine namespace depth should have a raw engine")));

		const char* const InitialNamespace = ScriptEngine->GetDefaultNamespace();
		const std::string SavedNamespace =
			InitialNamespace != nullptr ? InitialNamespace : "";
		const char* const Namespaces[] =
		{
			"",
			"InventoryRuntime",
			SavedNamespace.c_str(),
		};
		const TCHAR* const Ids[] =
		{
			TEXT("ENG-DEFAULT-NAMESPACE-TRANSITIONS-ROOT"),
			TEXT("ENG-DEFAULT-NAMESPACE-TRANSITIONS-NESTED"),
			TEXT("ENG-DEFAULT-NAMESPACE-TRANSITIONS-RESTORED"),
		};

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Namespaces); ++Index)
		{
			PrintReviewSource(
				Ids[Index],
				TEXT("default namespace transition"),
				UTF8_TO_TCHAR(Namespaces[Index]),
				TEXT("exact namespace readback"));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				ScriptEngine->SetDefaultNamespace(Namespaces[Index]),
				FString::Printf(TEXT("%s should set its namespace state"), Ids[Index])));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(Namespaces[Index])),
				FString(UTF8_TO_TCHAR(ScriptEngine->GetDefaultNamespace())),
				FString::Printf(TEXT("%s should expose exact namespace readback"), Ids[Index])));
		}
	}

	TEST_METHOD(PrimitiveSizesByPublicTypeId)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-PRIMITIVE-SIZE-TYPEINFO",
			ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Engine primitive-size depth should have a raw engine")));

		const FPrimitiveSizeCase PrimitiveCases[] =
		{
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-BOOL"), asTYPEID_BOOL, 1 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-INT8"), asTYPEID_INT8, 1 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-UINT8"), asTYPEID_UINT8, 1 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-INT16"), asTYPEID_INT16, 2 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-UINT16"), asTYPEID_UINT16, 2 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-INT32"), asTYPEID_INT32, 4 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-UINT32"), asTYPEID_UINT32, 4 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-INT64"), asTYPEID_INT64, 8 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-UINT64"), asTYPEID_UINT64, 8 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-FLOAT32"), asTYPEID_FLOAT32, 4 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-FLOAT64"), asTYPEID_FLOAT64, 8 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-VOID"), asTYPEID_VOID, 0 },
			{ TEXT("ENG-PRIMITIVE-SIZE-TYPEINFO-OBJECT"), RootObjectTypeId, 0 },
		};

		for (const FPrimitiveSizeCase& PrimitiveCase : PrimitiveCases)
		{
			PrintReviewSource(
				PrimitiveCase.Id,
				TEXT("primitive type size"),
				FString::Printf(TEXT("type_id=%d"), PrimitiveCase.TypeId),
				FString::Printf(TEXT("size=%d"), PrimitiveCase.ExpectedSize));
			ASSERT_THAT(AreEqual(
				PrimitiveCase.ExpectedSize,
				ScriptEngine->GetSizeOfPrimitiveType(PrimitiveCase.TypeId),
				FString::Printf(TEXT("%s should expose the exact primitive byte size"), PrimitiveCase.Id)));
		}
	}

	TEST_METHOD(TypeInfoLookupByIdKindAndFlags)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-TYPEINFO-ID-LOOKUP",
			ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Engine type-ID lookup depth should have a raw engine")));
		ASSERT_THAT(IsTrue(
			bContractsRegistered,
			TEXT("Engine type-ID lookup depth should have registered its shared contracts")));

		const FRegisteredTypeCase TypeCases[] =
		{
			{ TEXT("ENG-TYPEINFO-ID-LOOKUP-OBJECT-ROOT"), "InventoryRootObject", "", RootObjectTypeId, false },
			{ TEXT("ENG-TYPEINFO-ID-LOOKUP-OBJECT-NESTED"), "InventoryNestedObject", "InventoryDepth", NestedObjectTypeId, false },
			{ TEXT("ENG-TYPEINFO-ID-LOOKUP-ENUM-ROOT"), "InventoryRootEnum", "", RootEnumTypeId, true },
			{ TEXT("ENG-TYPEINFO-ID-LOOKUP-ENUM-NESTED"), "InventoryNestedEnum", "InventoryDepth", NestedEnumTypeId, true },
		};

		for (const FRegisteredTypeCase& TypeCase : TypeCases)
		{
			PrintReviewSource(
				TypeCase.Id,
				TEXT("type info lookup by ID"),
				FString::Printf(TEXT("type_id=%d"), TypeCase.TypeId),
				FString::Printf(
					TEXT("namespace=%hs; name=%hs"),
					TypeCase.Namespace,
					TypeCase.Name));
			asITypeInfo* const Type = ScriptEngine->GetTypeInfoById(TypeCase.TypeId);
			ASSERT_THAT(IsNotNull(
				Type,
				FString::Printf(TEXT("%s should resolve its registered type ID"), TypeCase.Id)));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(TypeCase.Name)),
				FString(UTF8_TO_TCHAR(Type->GetName())),
				FString::Printf(TEXT("%s should preserve the exact type name"), TypeCase.Id)));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(TypeCase.Namespace)),
				FString(UTF8_TO_TCHAR(Type->GetNamespace())),
				FString::Printf(TEXT("%s should preserve the exact type namespace"), TypeCase.Id)));
		}

		PrintReviewSource(
			TEXT("ENG-TYPEINFO-ID-LOOKUP-OBJECT-HANDLE"),
			TEXT("type info lookup by handle-qualified ID"),
			FString::Printf(TEXT("type_id=%d"), RootObjectTypeId | asTYPEID_OBJHANDLE),
			TEXT("same root object type"));
		ASSERT_THAT(AreEqual(
			ScriptEngine->GetTypeInfoById(RootObjectTypeId),
			ScriptEngine->GetTypeInfoById(RootObjectTypeId | asTYPEID_OBJHANDLE),
			TEXT("Handle qualification should resolve to the same registered object TypeInfo")));

		PrintReviewSource(
			TEXT("ENG-TYPEINFO-ID-LOOKUP-PRIMITIVE"),
			TEXT("type info lookup by primitive ID"),
			TEXT("asTYPEID_INT32"),
			TEXT("null because primitives have no TypeInfo object"));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetTypeInfoById(asTYPEID_INT32),
			TEXT("Primitive type IDs should not resolve to object TypeInfo")));

		PrintReviewSource(
			TEXT("ENG-TYPEINFO-ID-LOOKUP-INVALID"),
			TEXT("type info lookup by invalid ID"),
			TEXT("asINVALID_TYPE"),
			TEXT("null"));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetTypeInfoById(asINVALID_TYPE),
			TEXT("Invalid type IDs should not resolve to TypeInfo")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
