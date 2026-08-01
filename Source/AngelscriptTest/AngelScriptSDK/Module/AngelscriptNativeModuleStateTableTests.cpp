#include "Support/AngelscriptNativeCaseTestSupport.h"
#include "Support/AngelscriptNativeCoreTestSupport.h"
#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleStateTableTests, "Angelscript.TestModule.AngelScriptSDK.Module.StateTables", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asIScriptModule* CreateScriptModule(asIScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return ScriptEngine != nullptr
			? ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE)
			: nullptr;
	}

	static FString FormatPointer(const void* Pointer)
	{
		return FString::Printf(TEXT("%p"), Pointer);
	}

	static FString DescribeObjectTypes(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT TypeCount = Module->GetObjectTypeCount();
		for (asUINT TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = Module->GetObjectTypeByIndex(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(TypeInfo->GetName());
		}

		return Result.IsEmpty() ? TEXT("<no object types>") : Result;
	}

	static FString DescribeGlobals(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* Declaration = Module->GetGlobalVarDeclaration(GlobalIndex, true);
			if (Declaration == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			Result += UTF8_TO_TCHAR(Declaration);
		}

		return Result.IsEmpty() ? TEXT("<no globals>") : Result;
	}

	static FString DescribeTypeInfoList(asIScriptModule* Module, asUINT Count, asITypeInfo* (asIScriptModule::*Getter)(asUINT) const, const TCHAR* EmptyText)
	{
		if (Module == nullptr)
		{
			return TEXT("<null module>");
		}

		FString Result;
		for (asUINT TypeIndex = 0; TypeIndex < Count; ++TypeIndex)
		{
			asITypeInfo* TypeInfo = (Module->*Getter)(TypeIndex);
			if (TypeInfo == nullptr)
			{
				continue;
			}

			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}

			const char* Namespace = TypeInfo->GetNamespace();
			if (Namespace != nullptr && Namespace[0] != '\0')
			{
				Result += UTF8_TO_TCHAR(Namespace);
				Result += TEXT("::");
			}
			Result += UTF8_TO_TCHAR(TypeInfo->GetName());
		}

		return Result.IsEmpty() ? EmptyText : Result;
	}

	static int32 FindGlobalVarIndexByName(asIScriptModule* Module, const char* Name)
	{
		if (Module == nullptr || Name == nullptr)
		{
			return INDEX_NONE;
		}

		const asUINT GlobalCount = Module->GetGlobalVarCount();
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* GlobalName = nullptr;
			if (Module->GetGlobalVar(GlobalIndex, &GlobalName) >= 0 &&
				GlobalName != nullptr &&
				FCStringAnsi::Strcmp(GlobalName, Name) == 0)
			{
				return static_cast<int32>(GlobalIndex);
			}
		}

		return INDEX_NONE;
	}

	static asIScriptFunction* FindFunctionByNameAndNamespace(asIScriptModule* Module, const char* Name, const char* Namespace)
	{
		if (Module == nullptr || Name == nullptr || Namespace == nullptr)
		{
			return nullptr;
		}

		const asUINT FunctionCount = Module->GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(FunctionIndex);
			if (Function != nullptr &&
				FCStringAnsi::Strcmp(Function->GetName(), Name) == 0 &&
				FCStringAnsi::Strcmp(Function->GetNamespace(), Namespace) == 0)
			{
				return Function;
			}
		}

		return nullptr;
	}

	static void LogModuleState(FAutomationTestBase& Test, asIScriptEngine* ScriptEngine, asIScriptModule* Module, const TCHAR* Stage)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		Test.AddInfo(FString::Printf(
			TEXT("ScriptModule state [%s]: engineModuleCount=%u module=%s name=%s defaultNamespace=%s functions={%s} globals={%s} objectTypes={%s} enums={%s} typedefs={%s} imports=%u"),
			Stage != nullptr ? Stage : TEXT("<unknown>"),
			ScriptEngine != nullptr ? ScriptEngine->GetModuleCount() : 0,
			*FormatPointer(Module),
			Module != nullptr ? UTF8_TO_TCHAR(Module->GetName()) : TEXT("<null>"),
			Module != nullptr ? UTF8_TO_TCHAR(Module->GetDefaultNamespace()) : TEXT("<null>"),
			*CollectFunctionDeclarations(Module),
			*DescribeGlobals(Module),
			*DescribeObjectTypes(Module),
			*DescribeTypeInfoList(Module, Module != nullptr ? Module->GetEnumCount() : 0, &asIScriptModule::GetEnumByIndex, TEXT("<no enums>")),
			*DescribeTypeInfoList(Module, Module != nullptr ? Module->GetTypedefCount() : 0, &asIScriptModule::GetTypedefByIndex, TEXT("<no typedefs>")),
			Module != nullptr ? Module->GetImportedFunctionCount() : 0));
	}

public:

	TEST_METHOD(RichModuleStoresTopLevelTablesAndExecutesEntry)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("MOD-STATE-TABLE-REBUILD",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule rich storage test should create a standalone SDK engine")));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			namespace Inventory
			{
				const int BaseScore = 40;
				const int SlotCount = 3;

				enum ESlot
				{
					Head = 1,
					Body = 2,
					Weapon = 4
				}

				class ItemState
				{
					int Id;
					int Quantity;

					int GetScore()
					{
						return Id + Quantity;
					}
				}

				int Add(int A, int B)
				{
					return A + B;
				}

				int Entry()
				{
					return Add(BaseScore, SlotCount) + int(ESlot::Weapon);
				}
			}
			)AS");
		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "ScriptModuleRichStorage", ScriptSource.c_str());
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}
		LogModuleState(*TestRunner, ScriptEngine, Module, TEXT("rich-storage-after-build"));

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetFunctionCount()), TEXT("ScriptModule rich storage test should expose two global functions")));
		ASSERT_THAT(IsNotNull(FindFunctionByNameAndNamespace(Module, "Add", "Inventory"), TEXT("ScriptModule rich storage test should store Add in the Inventory namespace")));
		ASSERT_THAT(IsNotNull(Module->GetFunctionByDecl("int Inventory::Entry()"), TEXT("ScriptModule rich storage test should resolve the namespaced Entry function")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("ScriptModule rich storage test should expose two const globals")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetObjectTypeCount()), TEXT("ScriptModule rich storage test should expose one object type")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetEnumCount()), TEXT("ScriptModule rich storage test should expose one enum")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(Module->GetTypedefCount()), TEXT("ScriptModule rich storage test should expose no typedefs")));

		const int32 BaseScoreIndex = FindGlobalVarIndexByName(Module, "BaseScore");
		ASSERT_THAT(IsTrue(BaseScoreIndex >= 0, TEXT("ScriptModule rich storage test should find BaseScore by name")));
		const char* GlobalName = nullptr;
		const char* GlobalNamespace = nullptr;
		int GlobalTypeId = 0;
		bool bIsConst = false;
		ASSERT_THAT(IsTrue(
			Module->GetGlobalVar(static_cast<asUINT>(BaseScoreIndex), &GlobalName, &GlobalNamespace, &GlobalTypeId, &bIsConst) >= 0,
			TEXT("ScriptModule rich storage test should read BaseScore metadata")));
		ASSERT_THAT(AreEqual(FString(TEXT("BaseScore")), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")), TEXT("ScriptModule rich storage test should keep BaseScore name")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(GlobalNamespace != nullptr ? GlobalNamespace : "")), TEXT("ScriptModule rich storage test should keep BaseScore namespace")));
		ASSERT_THAT(IsTrue(bIsConst, TEXT("ScriptModule rich storage test should keep BaseScore const flag")));
		ASSERT_THAT(IsNotNull(Module->GetAddressOfGlobalVar(static_cast<asUINT>(BaseScoreIndex)), TEXT("ScriptModule rich storage test should expose BaseScore storage")));
		ASSERT_THAT(IsTrue(GlobalTypeId > 0, TEXT("ScriptModule rich storage test should expose a valid BaseScore type id")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("const int Inventory::BaseScore")),
			FString(UTF8_TO_TCHAR(Module->GetGlobalVarDeclaration(static_cast<asUINT>(BaseScoreIndex), true))),
			TEXT("ScriptModule rich storage test should include namespaces in global declarations")));

		asITypeInfo* ItemStateType = Module->GetTypeInfoByDecl("Inventory::ItemState");
		asITypeInfo* SlotEnumType = Module->GetEnumByIndex(0);
		ASSERT_THAT(IsNotNull(ItemStateType, TEXT("ScriptModule rich storage test should expose ItemState type")));
		ASSERT_THAT(IsNotNull(SlotEnumType, TEXT("ScriptModule rich storage test should expose ESlot enum")));
		ASSERT_THAT(AreEqual(FString(TEXT("Inventory")), FString(UTF8_TO_TCHAR(ItemStateType->GetNamespace())), TEXT("ScriptModule rich storage test should keep ItemState namespace")));
		ASSERT_THAT(AreEqual(FString(TEXT("ESlot")), FString(UTF8_TO_TCHAR(SlotEnumType->GetName())), TEXT("ScriptModule rich storage test should keep ESlot enum name")));

		int32 EntryResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Inventory::Entry()", EntryResult))
		{
			return;
		}
		TestRunner->AddInfo(FString::Printf(TEXT("ScriptModule rich storage execution: Entry()=%d"), EntryResult));
		ASSERT_THAT(AreEqual(47, EntryResult, TEXT("ScriptModule rich storage test should execute namespaced Entry through stored declarations")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(Module.Discard()),
			TEXT("ScriptModule rich storage test should explicitly discard the owning module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule("ScriptModuleRichStorage", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule rich storage test should remove all top-level tables with the discarded module")));

		AngelscriptNativeTestSupport::FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("ScriptModule rich storage test should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != ScriptEngine, TEXT("ScriptModule rich storage test should isolate top-level tables by engine")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule("ScriptModuleRichStorage", asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule rich storage test should not publish tables into an independent engine")));
	}
	TEST_METHOD(RebuildClearsPreviousTopLevelTables)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-STATE-TABLE-REBUILD", "rebuild_clears_previous_tables");

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ScriptModule table rebuild test should create a standalone SDK engine")));

		AngelscriptNativeTestSupport::FScopedNativeModuleName ModuleScope(Engine, "ScriptModuleTableRebuild");
		const std::string ModuleV1Source = ASTEST_AS_ANSI(R"AS(
			const int OldValue = 100;

			enum EOld
			{
				One = 1
			}

			class OldState
			{
				int Value;
			}

			int OldEntry()
			{
				return OldValue + int(EOld::One);
			}
			)AS");
		asIScriptModule* ModuleV1 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV1Source.c_str());
		if (ModuleV1 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV1, TEXT("ScriptModule table rebuild test should build version one")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV1, TEXT("table-rebuild-v1-after-build"));
		if (ModuleV1 == nullptr)
		{
			return;
		}

		asIScriptFunction* const FunctionV1 = ModuleV1->GetFunctionByDecl("int OldEntry()");
		asITypeInfo* const ObjectTypeV1 = ModuleV1->GetTypeInfoByDecl("OldState");
		asITypeInfo* const EnumTypeV1 = ModuleV1->GetTypeInfoByDecl("EOld");
		const int32 OldGlobalIndex = FindGlobalVarIndexByName(ModuleV1, "OldValue");
		ASSERT_THAT(IsNotNull(FunctionV1, TEXT("ScriptModule table rebuild test should publish the version-one function")));
		ASSERT_THAT(IsNotNull(ObjectTypeV1, TEXT("ScriptModule table rebuild test should publish the version-one object type")));
		ASSERT_THAT(IsNotNull(EnumTypeV1, TEXT("ScriptModule table rebuild test should publish the version-one enum")));
		ASSERT_THAT(AreEqual(0, OldGlobalIndex, TEXT("ScriptModule table rebuild test should publish OldValue at index zero")));
		if (OldGlobalIndex >= 0)
		{
			const int32* const OldGlobalValue = static_cast<const int32*>(ModuleV1->GetAddressOfGlobalVar(static_cast<asUINT>(OldGlobalIndex)));
			ASSERT_THAT(IsNotNull(OldGlobalValue, TEXT("ScriptModule table rebuild test should expose OldValue storage")));
			if (OldGlobalValue != nullptr)
			{
				ASSERT_THAT(AreEqual(100, *OldGlobalValue, TEXT("ScriptModule table rebuild test should initialize OldValue before replacement")));
			}
		}

		int32 V1Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV1, "int OldEntry()", V1Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(101, V1Result, TEXT("ScriptModule table rebuild test should execute version one")));

		const std::string ModuleV2Source = ASTEST_AS_ANSI(R"AS(
			const int NewValue = 200;

			enum ENew
			{
				Two = 2
			}

			class NewState
			{
				uint Value;
			}

			int NewEntry()
			{
				return NewValue + int(ENew::Two);
			}
			)AS");
		asIScriptModule* ModuleV2 = BuildNativeModule(ScriptEngine, ModuleScope.Get(), ModuleV2Source.c_str());
		if (ModuleV2 == nullptr)
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
		}
		ASSERT_THAT(IsNotNull(ModuleV2, TEXT("ScriptModule table rebuild test should build version two")));
		LogModuleState(*TestRunner, ScriptEngine, ModuleV2, TEXT("table-rebuild-v2-after-build"));
		if (ModuleV2 == nullptr)
		{
			return;
		}

		asIScriptFunction* const FunctionV2 = ModuleV2->GetFunctionByDecl("int NewEntry()");
		asITypeInfo* const ObjectTypeV2 = ModuleV2->GetTypeInfoByDecl("NewState");
		asITypeInfo* const EnumTypeV2 = ModuleV2->GetTypeInfoByDecl("ENew");
		ASSERT_THAT(AreNotEqual(ModuleV1, ModuleV2, TEXT("ScriptModule table rebuild test should allocate a replacement module")));
		ASSERT_THAT(AreNotEqual(FunctionV1, FunctionV2, TEXT("ScriptModule table rebuild test should replace the function table identity")));
		ASSERT_THAT(AreNotEqual(ObjectTypeV1, ObjectTypeV2, TEXT("ScriptModule table rebuild test should replace the object-type table identity")));
		ASSERT_THAT(AreNotEqual(EnumTypeV1, EnumTypeV2, TEXT("ScriptModule table rebuild test should replace the enum table identity")));

		int32 V2Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, ModuleV2, "int NewEntry()", V2Result))
		{
			return;
		}
		ASSERT_THAT(AreEqual(202, V2Result, TEXT("ScriptModule table rebuild test should execute version two")));

		ASSERT_THAT(IsNull(ModuleV2->GetFunctionByDecl("int OldEntry()"), TEXT("ScriptModule table rebuild test should remove the old function declaration")));
		ASSERT_THAT(IsNull(ModuleV2->GetTypeInfoByDecl("OldState"), TEXT("ScriptModule table rebuild test should remove the old class declaration")));
		ASSERT_THAT(IsNull(ModuleV2->GetTypeInfoByDecl("EOld"), TEXT("ScriptModule table rebuild test should remove the old enum declaration")));
		ASSERT_THAT(AreEqual(-1, FindGlobalVarIndexByName(ModuleV2, "OldValue"), TEXT("ScriptModule table rebuild test should remove the old global declaration")));
		ASSERT_THAT(IsNotNull(ModuleV2->GetFunctionByDecl("int NewEntry()"), TEXT("ScriptModule table rebuild test should expose the new function declaration")));
		ASSERT_THAT(IsNotNull(ModuleV2->GetTypeInfoByDecl("NewState"), TEXT("ScriptModule table rebuild test should expose the new class declaration")));
		ASSERT_THAT(IsNotNull(ModuleV2->GetTypeInfoByDecl("ENew"), TEXT("ScriptModule table rebuild test should expose the new enum declaration")));
		const int32 NewGlobalIndex = FindGlobalVarIndexByName(ModuleV2, "NewValue");
		ASSERT_THAT(AreEqual(0, NewGlobalIndex, TEXT("ScriptModule table rebuild test should publish NewValue at index zero")));
		if (NewGlobalIndex >= 0)
		{
			const int32* const NewGlobalValue = static_cast<const int32*>(ModuleV2->GetAddressOfGlobalVar(static_cast<asUINT>(NewGlobalIndex)));
			ASSERT_THAT(IsNotNull(NewGlobalValue, TEXT("ScriptModule table rebuild test should expose NewValue storage")));
			if (NewGlobalValue != nullptr)
			{
				ASSERT_THAT(AreEqual(200, *NewGlobalValue, TEXT("ScriptModule table rebuild test should initialize NewValue after replacement")));
			}
		}
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetFunctionCount()), TEXT("ScriptModule table rebuild test should keep one function after rebuild")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetGlobalVarCount()), TEXT("ScriptModule table rebuild test should keep one global after rebuild")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetObjectTypeCount()), TEXT("ScriptModule table rebuild test should keep one object type after rebuild")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(ModuleV2->GetEnumCount()), TEXT("ScriptModule table rebuild test should keep one enum after rebuild")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(ModuleV2->GetTypedefCount()), TEXT("ScriptModule table rebuild test should keep zero typedefs after rebuild")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule(ModuleScope.Get()),
			TEXT("ScriptModule table rebuild test should explicitly discard the replacement module")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleScope.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("ScriptModule table rebuild test should remove the replacement tables from name lookup")));
	}
};

#endif
