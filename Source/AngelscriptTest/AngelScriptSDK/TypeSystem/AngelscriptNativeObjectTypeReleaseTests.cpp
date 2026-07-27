#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FObjectTypeReleaseTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.ObjectTypeRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 ObserveInternalReferenceCount(asCTypeInfo& Type)
	{
		const int32 CountAfterAdd = Type.AddRefInternal();
		const int32 CountAfterRestore = Type.ReleaseInternal();
		return CountAfterAdd == CountAfterRestore + 1
			? CountAfterRestore
			: INDEX_NONE;
	}

	enum class EFunctionSlot : uint8
	{
		Factory,
		Constructor,
		Method,
		Virtual,
		GarbageCollection,
	};

	struct FFunctionSlotCase
	{
		const TCHAR* Id;
		EFunctionSlot Slot;
	};

	struct FPropertyCase
	{
		const TCHAR* OwnerId;
		const TCHAR* PropertyId;
		bool bScriptObject;
		bool bObjectProperty;
	};

	inline static constexpr FFunctionSlotCase FunctionSlotCases[] =
	{
		{ TEXT("factory"), EFunctionSlot::Factory },
		{ TEXT("constructor"), EFunctionSlot::Constructor },
		{ TEXT("method"), EFunctionSlot::Method },
		{ TEXT("virtual"), EFunctionSlot::Virtual },
		{ TEXT("gc"), EFunctionSlot::GarbageCollection },
	};

	inline static constexpr FPropertyCase PropertyCases[] =
	{
		{ TEXT("application"), TEXT("primitive"), false, false },
		{ TEXT("application"), TEXT("object"), false, true },
		{ TEXT("script"), TEXT("primitive"), true, false },
		{ TEXT("script"), TEXT("object"), true, true },
	};

	static int32 AppendFunctionSlot(asCScriptEngine& Engine, asCScriptFunction& Function)
	{
		if (Engine.scriptFunctions.GetLength() == 0)
		{
			Engine.scriptFunctions.SetLength(1);
			Engine.scriptFunctions[0] = nullptr;
		}
		const int32 FunctionId = static_cast<int32>(Engine.scriptFunctions.GetLength());
		Engine.scriptFunctions.SetLength(FunctionId + 1);
		Engine.scriptFunctions[FunctionId] = &Function;
		return FunctionId;
	}

	static void RemoveFunctionSlots(asCScriptEngine& Engine, const asUINT OriginalLength)
	{
		for (asUINT Index = OriginalLength; Index < Engine.scriptFunctions.GetLength(); ++Index)
		{
			Engine.scriptFunctions[Index] = nullptr;
		}
		Engine.scriptFunctions.SetLength(OriginalLength);
	}

public:
	TEST_METHOD(DestroyInternalClearsEmptyListAndOwnedState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-OBJECTTYPE-DESTRUCTION-OWNERSHIP",
			ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* const InternalEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(
			InternalEngine,
			TEXT("Object-type destruction should create a case-owned raw SDK engine")));
		if (InternalEngine == nullptr)
		{
			return;
		}

		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"TYPE-OBJECTTYPE-DESTRUCTION-OWNERSHIP",
				{ TEXT("empty") }));
			asCObjectType Owner(InternalEngine);

			Owner.DestroyInternal();

			ASSERT_THAT(IsNull(
				Owner.engine,
				*Case.Describe(TEXT("empty object type should detach from the engine"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.properties.GetLength(),
				*Case.Describe(TEXT("empty object type should retain no property state"))));
			Owner.DestroyInternal();
			ASSERT_THAT(IsNull(
				Owner.engine,
				*Case.Describe(TEXT("repeated destruction should remain inert"))));
		}

		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"TYPE-OBJECTTYPE-DESTRUCTION-OWNERSHIP",
				{ TEXT("list_pattern") }));
			asCObjectType Owner(InternalEngine);
			Owner.flags = asOBJ_LIST_PATTERN;

			Owner.DestroyInternal();

			ASSERT_THAT(IsNull(
				Owner.engine,
				*Case.Describe(TEXT("list-pattern destruction should take the documented detach-only path"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.templateSubTypes.GetLength(),
				*Case.Describe(TEXT("empty list-pattern fixture should not acquire subtype ownership"))));
		}

		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"TYPE-OBJECTTYPE-DESTRUCTION-OWNERSHIP",
				{ TEXT("owned_graph") }));
			const asUINT OriginalFunctionCount = InternalEngine->scriptFunctions.GetLength();
			asCObjectType BaseType(InternalEngine);
			asCObjectType SubType(InternalEngine);
			asCObjectType PropertyType(InternalEngine);
			asCScriptFunction OwnedFunction(InternalEngine, nullptr, asFUNC_DUMMY);
			asCObjectType Owner(InternalEngine);
			ON_SCOPE_EXIT
			{
				RemoveFunctionSlots(*InternalEngine, OriginalFunctionCount);
			};

			BaseType.AddRefInternal();
			SubType.AddRefInternal();
			Owner.derivedFrom = &BaseType;
			Owner.templateSubTypes.PushLast(asCDataType::CreateType(&SubType, false));
			Owner.flags = asOBJ_SCRIPT_OBJECT;
			asCObjectProperty* const Property = Owner.AddPropertyToClass(
				"OwnedProperty",
				asCDataType::CreateType(&PropertyType, false),
				false,
				false);
			ASSERT_THAT(IsNotNull(
				Property,
				*Case.Describe(TEXT("owned graph should create an object property"))));
			if (Property == nullptr)
			{
				Owner.derivedFrom = nullptr;
				Owner.templateSubTypes.SetLength(0);
				BaseType.ReleaseInternal();
				SubType.ReleaseInternal();
				return;
			}

			const int32 FunctionId = AppendFunctionSlot(*InternalEngine, OwnedFunction);
			OwnedFunction.AddRefInternal();
			Owner.beh.addref = FunctionId;

			ASSERT_THAT(AreEqual(
				2,
				ObserveInternalReferenceCount(BaseType),
				*Case.Describe(TEXT("owned graph should retain its base before destruction"))));
			ASSERT_THAT(AreEqual(
				2,
				ObserveInternalReferenceCount(SubType),
				*Case.Describe(TEXT("owned graph should retain its template subtype before destruction"))));
			ASSERT_THAT(AreEqual(
				2,
				ObserveInternalReferenceCount(PropertyType),
				*Case.Describe(TEXT("owned graph should retain its property type before destruction"))));
			ASSERT_THAT(AreEqual(
				2,
				static_cast<int32>(OwnedFunction.internalRefCount.get()),
				*Case.Describe(TEXT("owned graph should retain its behavior function before destruction"))));

			Owner.DestroyInternal();

			ASSERT_THAT(IsNull(
				Owner.engine,
				*Case.Describe(TEXT("owned graph should detach from the engine after destruction"))));
			ASSERT_THAT(IsNull(
				Owner.derivedFrom,
				*Case.Describe(TEXT("owned graph should clear its base pointer"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.templateSubTypes.GetLength(),
				*Case.Describe(TEXT("owned graph should clear template subtypes"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.properties.GetLength(),
				*Case.Describe(TEXT("owned graph should clear properties"))));
			ASSERT_THAT(AreEqual(
				0,
				Owner.beh.addref,
				*Case.Describe(TEXT("owned graph should clear behavior function IDs"))));
			ASSERT_THAT(AreEqual(
				1,
				ObserveInternalReferenceCount(BaseType),
				*Case.Describe(TEXT("owned graph should release its base exactly once"))));
			ASSERT_THAT(AreEqual(
				1,
				ObserveInternalReferenceCount(SubType),
				*Case.Describe(TEXT("owned graph should release its subtype exactly once"))));
			ASSERT_THAT(AreEqual(
				1,
				ObserveInternalReferenceCount(PropertyType),
				*Case.Describe(TEXT("owned graph should release its property type exactly once"))));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(OwnedFunction.internalRefCount.get()),
				*Case.Describe(TEXT("owned graph should release its behavior function exactly once"))));
		}
	}

	TEST_METHOD(ReleaseAllFunctionsClearsEveryOwnedSlotFamily)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-OBJECTTYPE-FUNCTION-RELEASE",
			ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* const InternalEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(
			InternalEngine,
			TEXT("Object-type function release should create a case-owned raw SDK engine")));
		if (InternalEngine == nullptr)
		{
			return;
		}

		for (const FFunctionSlotCase& SlotCase : FunctionSlotCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"TYPE-OBJECTTYPE-FUNCTION-RELEASE",
				{ SlotCase.Id }));
			const asUINT OriginalFunctionCount = InternalEngine->scriptFunctions.GetLength();
			asCScriptFunction OwnedFunction(InternalEngine, nullptr, asFUNC_DUMMY);
			asCScriptFunction UnrelatedFunction(InternalEngine, nullptr, asFUNC_DUMMY);
			asCObjectType Owner(InternalEngine);
			ON_SCOPE_EXIT
			{
				RemoveFunctionSlots(*InternalEngine, OriginalFunctionCount);
			};

			const int32 OwnedFunctionId = AppendFunctionSlot(*InternalEngine, OwnedFunction);
			(void)AppendFunctionSlot(*InternalEngine, UnrelatedFunction);
			OwnedFunction.AddRefInternal();

			switch (SlotCase.Slot)
			{
			case EFunctionSlot::Factory:
				Owner.beh.factory = OwnedFunctionId;
				Owner.beh.factories.PushLast(OwnedFunctionId);
				break;
			case EFunctionSlot::Constructor:
				Owner.beh.construct = OwnedFunctionId;
				Owner.beh.constructors.PushLast(OwnedFunctionId);
				break;
			case EFunctionSlot::Method:
				OwnedFunction.name = "OwnedMethod";
				Owner.methods.PushLast(OwnedFunctionId);
				Owner.methodTable.Add(&OwnedFunction);
				break;
			case EFunctionSlot::Virtual:
				Owner.virtualFunctionTable.PushLast(&OwnedFunction);
				break;
			case EFunctionSlot::GarbageCollection:
				Owner.beh.addref = OwnedFunctionId;
				break;
			}

			Owner.ReleaseAllFunctions();

			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(OwnedFunction.internalRefCount.get()),
				*Case.Describe(TEXT("owned function slot should release exactly one internal reference"))));
			ASSERT_THAT(AreEqual(
				1,
				static_cast<int32>(UnrelatedFunction.internalRefCount.get()),
				*Case.Describe(TEXT("unrelated engine function should retain its reference count"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.beh.factories.GetLength(),
				*Case.Describe(TEXT("factory collection should be empty after release"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.beh.constructors.GetLength(),
				*Case.Describe(TEXT("constructor collection should be empty after release"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.methods.GetLength(),
				*Case.Describe(TEXT("method ID collection should be empty after release"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.virtualFunctionTable.GetLength(),
				*Case.Describe(TEXT("virtual function collection should be empty after release"))));
			ASSERT_THAT(AreEqual(
				0,
				Owner.beh.factory + Owner.beh.construct + Owner.beh.addref,
				*Case.Describe(TEXT("selected behavior IDs should be cleared after release"))));
		}
	}

	TEST_METHOD(ReleaseAllPropertiesBalancesOwnerAndPropertyKinds)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-OBJECTTYPE-PROPERTY-RELEASE",
			ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* const InternalEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(
			InternalEngine,
			TEXT("Object-type property release should create a case-owned raw SDK engine")));
		if (InternalEngine == nullptr)
		{
			return;
		}

		for (const FPropertyCase& PropertyCase : PropertyCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"TYPE-OBJECTTYPE-PROPERTY-RELEASE",
				{ PropertyCase.OwnerId, PropertyCase.PropertyId }));
			asCObjectType PropertyType(InternalEngine);
			asCObjectType Owner(InternalEngine);
			Owner.flags = PropertyCase.bScriptObject ? asOBJ_SCRIPT_OBJECT : asOBJ_REF;
			const asCDataType PropertyDataType = PropertyCase.bObjectProperty
				? asCDataType::CreateType(&PropertyType, false)
				: asCDataType::CreatePrimitive(ttInt, false);

			asCObjectProperty* const Property = Owner.AddPropertyToClass(
				"OwnedProperty",
				PropertyDataType,
				false,
				false);
			ASSERT_THAT(IsNotNull(
				Property,
				*Case.Describe(TEXT("property fixture should create the selected property kind"))));
			if (Property == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				PropertyCase.bObjectProperty ? 2 : 1,
				ObserveInternalReferenceCount(PropertyType),
				*Case.Describe(TEXT("object properties should retain their type exactly once before release"))));

			Owner.ReleaseAllProperties();

			ASSERT_THAT(AreEqual(
				1,
				ObserveInternalReferenceCount(PropertyType),
				*Case.Describe(TEXT("property release should restore the independent type baseline"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.properties.GetLength(),
				*Case.Describe(TEXT("property view should be empty after release"))));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Owner.localProperties.GetLength(),
				*Case.Describe(TEXT("local property ownership should be empty after release"))));
			ASSERT_THAT(IsNull(
				Owner.propertyTable.FindFirst("OwnedProperty"),
				*Case.Describe(TEXT("property name index should be empty after release"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
