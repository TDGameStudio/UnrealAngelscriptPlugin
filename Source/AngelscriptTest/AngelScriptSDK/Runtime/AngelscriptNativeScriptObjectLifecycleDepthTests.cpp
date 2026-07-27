#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "ClassGenerator/ASClass.h"
#include "CQTest.h"
#include "HAL/UnrealMemory.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeScriptObjectLifecycleDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ScriptObjectLifecycleDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FDestructionObservation
	{
		int32 DestructionCount = 0;
		int32 RetainRequestCount = 0;
		asITypeInfo* ScriptType = nullptr;
		void* RetainedObject = nullptr;
		bool bRetainDuringDestruction = false;

		void Reset()
		{
			DestructionCount = 0;
			RetainRequestCount = 0;
			RetainedObject = nullptr;
			bRetainDuringDestruction = false;
		}
	};

	static constexpr asPWORD OwnershipProbeDestructionObservationSlot = 0x4F424A44;

	static FDestructionObservation* GetActiveOwnershipProbeDestructionObservation()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FDestructionObservation*>(
				Context->GetEngine()->GetUserData(
					OwnershipProbeDestructionObservationSlot))
			: nullptr;
	}

	static void RecordOwnershipProbeDestruction()
	{
		asIScriptContext* const Context = asGetActiveContext();
		FDestructionObservation* const Observation =
			GetActiveOwnershipProbeDestructionObservation();
		if (Observation != nullptr)
		{
			++Observation->DestructionCount;
			if (Observation->bRetainDuringDestruction
				&& Observation->ScriptType != nullptr
				&& Context != nullptr
				&& Context->GetEngine() != nullptr)
			{
				void* const Object = Context->GetThisPointer();
				if (Object != nullptr)
				{
					Context->GetEngine()->AddRefScriptObject(
						Object,
						Observation->ScriptType);
					Observation->RetainedObject = Object;
					++Observation->RetainRequestCount;
				}
			}
		}
	}

	static FString BuildSource(const bool bRecordDestruction = false)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class OwnershipProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tOwnershipProbe()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = 7;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tOwnershipProbe(const OwnershipProbe& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value + 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (bRecordDestruction)
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~OwnershipProbe()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRecordOwnershipProbeDestruction();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class OtherProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint OtherValue;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildReviewSource(
		const FString& ScriptSource,
		const TCHAR* Dimension,
		const TCHAR* Value)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// %s: %s"), Dimension, Value));
		Source += ScriptSource;
		return Source;
	}

	static asITypeInfo* FindOwnershipType(asIScriptModule* Module)
	{
		if (Module == nullptr)
		{
			return nullptr;
		}

		for (asUINT TypeIndex = 0; TypeIndex < Module->GetObjectTypeCount(); ++TypeIndex)
		{
			asITypeInfo* const Candidate = Module->GetObjectTypeByIndex(TypeIndex);
			if (Candidate != nullptr && FCStringAnsi::Strcmp(Candidate->GetName(), "OwnershipProbe") == 0)
			{
				return Candidate;
			}
		}

		return nullptr;
	}

	static int32 ReadValue(asIScriptObject* Object)
	{
		return Object != nullptr && Object->GetAddressOfProperty(0) != nullptr
			? *static_cast<int32*>(Object->GetAddressOfProperty(0))
			: INDEX_NONE;
	}

public:

	TEST_METHOD(ConstructCopyAssignPropertyAndReferenceOwnership)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-OBJ-CONSTRUCT-COPY-ASSIGN-PROPERTY",
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
		AS_NATIVE_PRODUCT("RT-OBJ-REFCOUNT-WEAKFLAG-FORK",
			ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Metadata);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Script object lifecycle products should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FDestructionObservation DestructionObservation;
		ASSERT_THAT(IsNull(
			ScriptEngine->SetUserData(
				&DestructionObservation,
				OwnershipProbeDestructionObservationSlot),
			TEXT("Script object lifecycle products should begin with an empty destruction-observation slot")));
		bool bDestructionObservationSlotCleared = false;
		ON_SCOPE_EXIT
		{
			if (!bDestructionObservationSlotCleared)
			{
				ScriptEngine->SetUserData(
					nullptr,
					OwnershipProbeDestructionObservationSlot);
			}
		};

		const ASAutoCaller::FunctionCaller DestructorCaller =
			ASAutoCaller::MakeFunctionCaller(RecordOwnershipProbeDestruction);
		const int32 DestructorCallbackRegistrationResult =
			ScriptEngine->RegisterGlobalFunction(
				"void RecordOwnershipProbeDestruction()",
				asFUNCTION(RecordOwnershipProbeDestruction),
				asCALL_CDECL,
				*(asFunctionCaller*)&DestructorCaller);
		ASSERT_THAT(IsTrue(
			DestructorCallbackRegistrationResult >= 0,
			*FString::Printf(
				TEXT("Script object lifecycle products should register their destruction callback before compilation. Result=%d Messages={%s}"),
				DestructorCallbackRegistrationResult,
				*Engine.GetMessagesText())));
		asIScriptFunction* const DestructorCallback =
			ScriptEngine->GetGlobalFunctionByDecl(
				"void RecordOwnershipProbeDestruction()");
		ASSERT_THAT(IsNotNull(
			DestructorCallback,
			TEXT("Script object lifecycle products should publish the exact destruction callback declaration")));
		if (DestructorCallbackRegistrationResult < 0 || DestructorCallback == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeScriptObjectLifecycleDepth");
		const FString Source = BuildSource(true);
		PrintGeneratedAsSource(*TestRunner, TEXT("RT-OBJ-CONSTRUCT-COPY-ASSIGN-PROPERTY"), ModuleName, Source);
		PrintGeneratedAsSource(*TestRunner, TEXT("RT-OBJ-REFCOUNT-WEAKFLAG-FORK"), ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("Script object lifecycle source should compile. Build=%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(Module, TEXT("Script object lifecycle source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		bool bModuleDiscarded = false;
		ON_SCOPE_EXIT
		{
			if (!bModuleDiscarded
				&& ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) != nullptr)
			{
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			}
		};
		TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] Reference-returning opAssign on a reference script class is rejected as Not a valid reference; local reference-class value construction reports Null pointer access, so this lifecycle probe uses raw object APIs and the default assignment path"));
		TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] CreateScriptObjectCopy on a reference script class falls back to default construction plus property copy when the value-style copy constructor is not published"));

		asITypeInfo* const OwnershipType = FindOwnershipType(Module);
		ASSERT_THAT(IsNotNull(OwnershipType, TEXT("Script object lifecycle product should resolve the script class type exactly by its published object table")));
		if (OwnershipType == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue((OwnershipType->GetFlags() & asOBJ_SCRIPT_OBJECT) != 0,
			TEXT("Script object lifecycle product should classify OwnershipProbe as a raw script object type")));
		asITypeInfo* const OtherType = Module->GetTypeInfoByName("OtherProbe");
		ASSERT_THAT(IsNotNull(
			OtherType,
			TEXT("Script object lifecycle product should resolve its wrong-TypeInfo control")));
		ASSERT_THAT(IsTrue(
			OtherType != OwnershipType,
			TEXT("Wrong-TypeInfo control should not alias the ownership type")));
		if (OtherType == nullptr || OtherType == OwnershipType)
		{
			return;
		}
		DestructionObservation.ScriptType = OwnershipType;

		asCScriptEngine* const RawEngine = static_cast<asCScriptEngine*>(ScriptEngine);
		DestructionObservation.Reset();
		ASSERT_THAT(AreEqual(0, DestructionObservation.DestructionCount,
			TEXT("Construct/copy/assign product should begin from an exact destruction baseline")));
		void* OriginalPointer = RawEngine->CreateScriptObject(OwnershipType);
		void* CopyPointer = nullptr;
		void* AssignedPointer = nullptr;
		ON_SCOPE_EXIT
		{
			if (AssignedPointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(AssignedPointer, OwnershipType);
			}
			if (CopyPointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(CopyPointer, OwnershipType);
			}
			if (OriginalPointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(OriginalPointer, OwnershipType);
			}
		};
		ASSERT_THAT(IsNotNull(OriginalPointer, TEXT("Script object lifecycle product should create a constructed script object")));
		if (OriginalPointer == nullptr)
		{
			return;
		}
		asIScriptObject* const Original = static_cast<asIScriptObject*>(OriginalPointer);

		ASSERT_THAT(AreEqual(1, static_cast<int32>(Original->GetPropertyCount()), TEXT("Script object lifecycle product should expose exactly one reflected property")));
		ASSERT_THAT(AreEqual(FString(TEXT("Value")), FString(UTF8_TO_TCHAR(Original->GetPropertyName(0) != nullptr ? Original->GetPropertyName(0) : "")), TEXT("Script object lifecycle product should expose the Value property by name")));
		ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl("int"), Original->GetPropertyTypeId(0), TEXT("Script object lifecycle product should expose the exact int property type id")));
		ASSERT_THAT(IsNotNull(Original->GetAddressOfProperty(0), TEXT("Script object lifecycle product should expose a writable property address")));
		ASSERT_THAT(AreEqual(7, ReadValue(Original), TEXT("Script object lifecycle product should run the default constructor through CreateScriptObject")));
		ASSERT_THAT(AreEqual(asINVALID_ARG, Original->GetPropertyTypeId(1), TEXT("Script object lifecycle product should reject an out-of-range property type query")));
		ASSERT_THAT(IsNull(Original->GetPropertyName(1), TEXT("Script object lifecycle product should return null for an out-of-range property name query")));
		ASSERT_THAT(IsNull(Original->GetAddressOfProperty(1), TEXT("Script object lifecycle product should return null for an out-of-range property address query")));

		CopyPointer = RawEngine->CreateScriptObjectCopy(OriginalPointer, OwnershipType);
		ASSERT_THAT(IsNotNull(CopyPointer, TEXT("Script object lifecycle product should create a copy through the script copy constructor")));
		if (CopyPointer == nullptr)
		{
			return;
		}
		asIScriptObject* const Copy = static_cast<asIScriptObject*>(CopyPointer);
		ASSERT_THAT(AreEqual(7, ReadValue(Copy), TEXT("Script object lifecycle product should preserve the reference-class default-copy observable value")));
		int32* const CopyValue = static_cast<int32*>(Copy->GetAddressOfProperty(0));
		ASSERT_THAT(IsNotNull(CopyValue, TEXT("Script object lifecycle product should expose the copied object's writable property address")));
		if (CopyValue == nullptr)
		{
			return;
		}
		*CopyValue = 31;
		ASSERT_THAT(AreEqual(31, ReadValue(Copy), TEXT("Script object lifecycle product should retain the copied object's independent mutation")));
		ASSERT_THAT(AreEqual(7, ReadValue(Original), TEXT("Script object lifecycle product should isolate the constructed object's property from copy mutation")));

		AssignedPointer = RawEngine->CreateUninitializedScriptObject(OwnershipType);
		ASSERT_THAT(IsNotNull(AssignedPointer, TEXT("Script object lifecycle product should allocate an uninitialized script object")));
		if (AssignedPointer == nullptr)
		{
			return;
		}
		asIScriptObject* const Assigned = static_cast<asIScriptObject*>(AssignedPointer);
		const asPWORD PreviousDisallowValueAssign = ScriptEngine->GetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE);
		ASSERT_THAT(AreEqual(asPWORD(1), PreviousDisallowValueAssign,
			TEXT("Script object lifecycle product should observe the native harness's explicit ref-value-assignment restriction")));
		const int RestrictedAssignResult = RawEngine->AssignScriptObject(AssignedPointer, CopyPointer, OwnershipType);
		ASSERT_THAT(AreEqual(asNOT_SUPPORTED, RestrictedAssignResult,
			TEXT("Script object lifecycle product should reject value assignment for a reference script class while the fork restriction is enabled")));
		TestRunner->AddInfo(TEXT("[AS-FORK-LIMITATION] AssignScriptObject returns asNOT_SUPPORTED for reference script classes when asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE is enabled; the test then exercises the opt-in API path with the property temporarily disabled"));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, 0),
			TEXT("Script object lifecycle product should be able to opt into reference value assignment for the positive API path")));
		ASSERT_THAT(AreEqual(asSUCCESS, RawEngine->AssignScriptObject(AssignedPointer, CopyPointer, OwnershipType),
			TEXT("Script object lifecycle product should assign into the uninitialized object after the explicit restriction is disabled")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE, PreviousDisallowValueAssign),
			TEXT("Script object lifecycle product should restore the ref-value-assignment restriction after the positive API path")));
		ASSERT_THAT(AreEqual(31, ReadValue(Assigned), TEXT("Script object lifecycle product should preserve the copied value through default assignment")));
		int32* const AssignedValue = static_cast<int32*>(Assigned->GetAddressOfProperty(0));
		ASSERT_THAT(IsNotNull(AssignedValue, TEXT("Script object lifecycle product should expose the assigned object's writable property address")));
		if (AssignedValue == nullptr)
		{
			return;
		}
		*AssignedValue = 41;
		ASSERT_THAT(AreEqual(41, ReadValue(Assigned), TEXT("Script object lifecycle product should retain the assigned object's independent mutation")));
		ASSERT_THAT(AreEqual(31, ReadValue(Copy), TEXT("Script object lifecycle product should isolate the copied object's property from assigned-object mutation")));
		ASSERT_THAT(AreEqual(7, ReadValue(Original), TEXT("Script object lifecycle product should isolate the constructed object's property from assigned-object mutation")));
		ASSERT_THAT(AreEqual(asSUCCESS, Assigned->CopyFrom(Original), TEXT("Script object lifecycle product should expose CopyFrom as an equivalent assignment entry")));
		ASSERT_THAT(AreEqual(7, ReadValue(Assigned), TEXT("Script object lifecycle product should update the assigned property after CopyFrom")));
		ASSERT_THAT(AreEqual(31, ReadValue(Copy), TEXT("Script object lifecycle product should preserve the independently mutated copy after CopyFrom")));

		RawEngine->ReleaseScriptObject(AssignedPointer, OwnershipType);
		AssignedPointer = nullptr;
		ASSERT_THAT(AreEqual(1, DestructionObservation.DestructionCount,
			TEXT("Construct/copy/assign product should destroy the assigned origin exactly once at its final release")));
		RawEngine->ReleaseScriptObject(CopyPointer, OwnershipType);
		CopyPointer = nullptr;
		ASSERT_THAT(AreEqual(2, DestructionObservation.DestructionCount,
			TEXT("Construct/copy/assign product should destroy the copied origin exactly once after the assigned origin")));
		RawEngine->ReleaseScriptObject(OriginalPointer, OwnershipType);
		OriginalPointer = nullptr;
		ASSERT_THAT(AreEqual(3, DestructionObservation.DestructionCount,
			TEXT("Construct/copy/assign product should invoke exactly one script destructor for each explicitly released object")));

		DestructionObservation.Reset();
		ASSERT_THAT(AreEqual(0, DestructionObservation.DestructionCount,
			TEXT("Refcount/weak-flag product should reset to an exact destruction baseline")));
		void* ReferenceProbePointer = RawEngine->CreateScriptObject(OwnershipType);
		ON_SCOPE_EXIT
		{
			if (ReferenceProbePointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(ReferenceProbePointer, OwnershipType);
			}
		};
		ASSERT_THAT(IsNotNull(ReferenceProbePointer, TEXT("Refcount/weak-flag product should create its independent reference probe")));
		if (ReferenceProbePointer == nullptr)
		{
			return;
		}
		asIScriptObject* const ReferenceProbe = static_cast<asIScriptObject*>(ReferenceProbePointer);

		asILockableSharedBool* const WeakFlag = RawEngine->GetWeakRefFlagOfScriptObject(ReferenceProbePointer, OwnershipType);
		// The fork does not register a script-object get-weak-ref behaviour. The
		// null result is an explicit current-fork contract, not a skipped check.
		ASSERT_THAT(IsNull(WeakFlag, TEXT("Refcount/weak-flag product should report the current fork's absent script-object weak-reference flag")));
		RawEngine->AddRefScriptObject(ReferenceProbePointer, OtherType);
		RawEngine->ReleaseScriptObject(ReferenceProbePointer, OtherType);
		ASSERT_THAT(AreEqual(
			0,
			DestructionObservation.DestructionCount,
			TEXT("Wrong-TypeInfo public ownership calls should not destroy the registered raw object")));
		ASSERT_THAT(AreEqual(
			7,
			ReadValue(ReferenceProbe),
			TEXT("Wrong-TypeInfo public ownership calls should leave the registered raw object unchanged")));
		RawEngine->AddRefScriptObject(ReferenceProbePointer, OwnershipType);
		RawEngine->ReleaseScriptObject(ReferenceProbePointer, OwnershipType);
		ASSERT_THAT(AreEqual(0, DestructionObservation.DestructionCount,
			TEXT("Refcount/weak-flag product should preserve its exact destruction baseline after a balanced raw AddRef/Release pair")));
		ASSERT_THAT(AreEqual(7, ReadValue(ReferenceProbe), TEXT("Refcount/weak-flag product should preserve the object after a balanced raw AddRef/Release pair")));
		RawEngine->ReleaseScriptObject(ReferenceProbePointer, OwnershipType);
		ReferenceProbePointer = nullptr;
		ASSERT_THAT(AreEqual(1, DestructionObservation.DestructionCount,
			TEXT("Refcount/weak-flag product should destroy exactly once after its final explicit release")));

		DestructionObservation.Reset();
		DestructionObservation.bRetainDuringDestruction = true;
		void* ResurrectionProbePointer = RawEngine->CreateScriptObject(OwnershipType);
		ON_SCOPE_EXIT
		{
			if (ResurrectionProbePointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(
					ResurrectionProbePointer,
					OwnershipType);
			}
		};
		ASSERT_THAT(IsNotNull(
			ResurrectionProbePointer,
			TEXT("Refcount/weak-flag product should create its destructor-retain probe")));
		if (ResurrectionProbePointer == nullptr)
		{
			return;
		}
		RawEngine->ReleaseScriptObject(ResurrectionProbePointer, OwnershipType);
		ASSERT_THAT(AreEqual(
			1,
			DestructionObservation.DestructionCount,
			TEXT("Destructor-retain probe should run its script destructor exactly once")));
		ASSERT_THAT(AreEqual(
			1,
			DestructionObservation.RetainRequestCount,
			TEXT("Destructor-retain probe should issue exactly one public AddRef request during destruction")));
		ASSERT_THAT(AreEqual(
			ResurrectionProbePointer,
			DestructionObservation.RetainedObject,
			TEXT("Destructor-retain probe should retain the exact object being destroyed")));
		ASSERT_THAT(AreEqual(
			7,
			ReadValue(static_cast<asIScriptObject*>(ResurrectionProbePointer)),
			TEXT("Destructor-retain probe should remain alive after its reentrant public AddRef")));
		DestructionObservation.bRetainDuringDestruction = false;
		RawEngine->ReleaseScriptObject(ResurrectionProbePointer, OwnershipType);
		ResurrectionProbePointer = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			DestructionObservation.DestructionCount,
			TEXT("Releasing a destructor-retained object should free it without repeating its script destructor")));

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
			TEXT("Script object lifecycle products should explicitly discard their module after all object releases")));
		bModuleDiscarded = true;
		ASSERT_THAT(AreEqual(1, DestructionObservation.DestructionCount,
			TEXT("Module discard should not repeat the already completed reference-probe destruction")));
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Script object lifecycle products should leave no name-visible module after explicit discard")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&DestructionObservation),
			ScriptEngine->SetUserData(
				nullptr,
				OwnershipProbeDestructionObservationSlot),
			TEXT("Script object lifecycle products should explicitly clear and return their destruction-observation slot after module cleanup")));
		bDestructionObservationSlotCleared = true;
		ASSERT_THAT(IsNull(
			ScriptEngine->GetUserData(OwnershipProbeDestructionObservationSlot),
			TEXT("Script object lifecycle products should leave the destruction-observation slot empty after explicit cleanup")));
	}

	TEST_METHOD(TypeObjectAndEngineIdentityByCreationOrigin)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-OBJ-TYPE-ENGINE-IDENTITY",
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

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Script object identity product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FDestructionObservation DestructionObservation;
		ASSERT_THAT(IsNull(
			ScriptEngine->SetUserData(
				&DestructionObservation,
				OwnershipProbeDestructionObservationSlot),
			TEXT("Script object identity product should begin with an empty destruction-observation slot")));
		bool bDestructionObservationSlotCleared = false;
		ON_SCOPE_EXIT
		{
			if (!bDestructionObservationSlotCleared)
			{
				ScriptEngine->SetUserData(
					nullptr,
					OwnershipProbeDestructionObservationSlot);
			}
		};

		const ASAutoCaller::FunctionCaller DestructorCaller =
			ASAutoCaller::MakeFunctionCaller(RecordOwnershipProbeDestruction);
		const int32 DestructorCallbackRegistrationResult =
			ScriptEngine->RegisterGlobalFunction(
				"void RecordOwnershipProbeDestruction()",
				asFUNCTION(RecordOwnershipProbeDestruction),
				asCALL_CDECL,
				*(asFunctionCaller*)&DestructorCaller);
		ASSERT_THAT(IsTrue(
			DestructorCallbackRegistrationResult >= 0,
			*FString::Printf(
				TEXT("Script object identity product should register its destruction callback before compilation. Result=%d Messages={%s}"),
				DestructorCallbackRegistrationResult,
				*Engine.GetMessagesText())));
		asIScriptFunction* const DestructorCallback =
			ScriptEngine->GetGlobalFunctionByDecl(
				"void RecordOwnershipProbeDestruction()");
		ASSERT_THAT(IsNotNull(
			DestructorCallback,
			TEXT("Script object identity product should publish the exact destruction callback declaration")));
		if (DestructorCallbackRegistrationResult < 0 || DestructorCallback == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeScriptObjectIdentityDepth");
		const FString ScriptSource = BuildSource(true);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*ScriptSource);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(
			ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			Module);
		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*FString::Printf(
				TEXT("Script object identity source should compile. Build=%d Messages={%s}"),
				BuildResult,
				*Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Script object identity source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		bool bModuleDiscarded = false;
		ON_SCOPE_EXIT
		{
			if (!bModuleDiscarded
				&& ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) != nullptr)
			{
				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			}
		};

		asITypeInfo* const OwnershipType = FindOwnershipType(Module);
		ASSERT_THAT(IsNotNull(
			OwnershipType,
			TEXT("Script object identity product should resolve OwnershipProbe exactly")));
		if (OwnershipType == nullptr)
		{
			return;
		}
		DestructionObservation.ScriptType = OwnershipType;

		asCScriptEngine* const RawEngine = static_cast<asCScriptEngine*>(ScriptEngine);
		DestructionObservation.Reset();
		ASSERT_THAT(AreEqual(0, DestructionObservation.DestructionCount,
			TEXT("Script object identity product should begin its creation-origin coverage from an exact destruction baseline")));
		void* ConstructedPointer = RawEngine->CreateScriptObject(OwnershipType);
		void* CopyPointer = nullptr;
		void* UninitializedPointer = nullptr;
		ON_SCOPE_EXIT
		{
			if (UninitializedPointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(UninitializedPointer, OwnershipType);
			}
			if (CopyPointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(CopyPointer, OwnershipType);
			}
			if (ConstructedPointer != nullptr)
			{
				RawEngine->ReleaseScriptObject(ConstructedPointer, OwnershipType);
			}
		};
		ASSERT_THAT(IsNotNull(
			ConstructedPointer,
			TEXT("Script object identity product should construct its origin object")));
		if (ConstructedPointer == nullptr)
		{
			return;
		}

		CopyPointer = RawEngine->CreateScriptObjectCopy(ConstructedPointer, OwnershipType);
		ASSERT_THAT(IsNotNull(
			CopyPointer,
			TEXT("Script object identity product should create its copy origin")));
		if (CopyPointer == nullptr)
		{
			return;
		}

		UninitializedPointer = RawEngine->CreateUninitializedScriptObject(OwnershipType);
		ASSERT_THAT(IsNotNull(
			UninitializedPointer,
			TEXT("Script object identity product should create its uninitialized origin")));
		if (UninitializedPointer == nullptr)
		{
			return;
		}

		struct FOrigin
		{
			const TCHAR* Name;
			asIScriptObject* Object;
		};
		const FOrigin Origins[] =
		{
			{ TEXT("constructed"), static_cast<asIScriptObject*>(ConstructedPointer) },
			{ TEXT("copy"), static_cast<asIScriptObject*>(CopyPointer) },
			{ TEXT("uninitialized"), static_cast<asIScriptObject*>(UninitializedPointer) },
		};
		const TCHAR* Queries[] =
		{
			TEXT("object_type"),
			TEXT("type_id"),
			TEXT("engine"),
		};

		for (const FOrigin& Origin : Origins)
		{
			for (const TCHAR* Query : Queries)
			{
				const FString CaseId = MakeNativeCaseId(
					"RT-OBJ-TYPE-ENGINE-IDENTITY",
					{ Origin.Name, Query });
				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					ModuleName,
					BuildReviewSource(
						ScriptSource,
						TEXT("Identity query"),
						*FString::Printf(
							TEXT("%s / %s"),
							Origin.Name,
							Query)));
			}

			ASSERT_THAT(AreEqual(
				OwnershipType,
				Origin.Object->GetObjectType(),
				*FString::Printf(
					TEXT("%s script object should retain exact TypeInfo identity"),
					Origin.Name)));
			ASSERT_THAT(AreEqual(
				OwnershipType->GetTypeId(),
				Origin.Object->GetTypeId(),
				*FString::Printf(
					TEXT("%s script object should retain exact type ID"),
					Origin.Name)));
			ASSERT_THAT(AreEqual(
				OwnershipType,
				ScriptEngine->GetTypeInfoById(Origin.Object->GetTypeId()),
				*FString::Printf(
					TEXT("%s type ID should resolve to the exact TypeInfo"),
					Origin.Name)));
			ASSERT_THAT(AreEqual(
				ScriptEngine,
				Origin.Object->GetEngine(),
				*FString::Printf(
					TEXT("%s script object should retain exact engine identity"),
					Origin.Name)));

			RawEngine->AddRefScriptObject(Origin.Object, OwnershipType);
			RawEngine->ReleaseScriptObject(Origin.Object, OwnershipType);
			ASSERT_THAT(AreEqual(
				0,
				DestructionObservation.DestructionCount,
				*FString::Printf(
					TEXT("%s identity should preserve the destruction baseline after a balanced AddRef/Release pair"),
					Origin.Name)));
			ASSERT_THAT(AreEqual(
				OwnershipType,
				Origin.Object->GetObjectType(),
				*FString::Printf(
					TEXT("%s identity should survive a balanced AddRef/Release pair"),
					Origin.Name)));
		}

		RawEngine->ReleaseScriptObject(UninitializedPointer, OwnershipType);
		UninitializedPointer = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			DestructionObservation.DestructionCount,
			TEXT("Script object identity product should destroy the uninitialized origin exactly once at its final release")));
		RawEngine->ReleaseScriptObject(CopyPointer, OwnershipType);
		CopyPointer = nullptr;
		ASSERT_THAT(AreEqual(
			2,
			DestructionObservation.DestructionCount,
			TEXT("Script object identity product should destroy the copied origin exactly once after the uninitialized origin")));
		RawEngine->ReleaseScriptObject(ConstructedPointer, OwnershipType);
		ConstructedPointer = nullptr;
		ASSERT_THAT(AreEqual(
			3,
			DestructionObservation.DestructionCount,
			TEXT("Script object identity product should destroy exactly its constructed, copied, and uninitialized origins before module discard")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get()),
			TEXT("Script object identity product should explicitly discard its module after every creation-origin object is released")));
		bModuleDiscarded = true;
		ASSERT_THAT(AreEqual(
			3,
			DestructionObservation.DestructionCount,
			TEXT("Module discard should not repeat any completed creation-origin destruction")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			TEXT("Script object identity product should leave no name-visible module after explicit discard")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&DestructionObservation),
			ScriptEngine->SetUserData(
				nullptr,
				OwnershipProbeDestructionObservationSlot),
			TEXT("Script object identity product should explicitly clear and return its destruction-observation slot after module cleanup")));
		bDestructionObservationSlotCleared = true;
		ASSERT_THAT(IsNull(
			ScriptEngine->GetUserData(OwnershipProbeDestructionObservationSlot),
			TEXT("Script object identity product should leave the destruction-observation slot empty after explicit cleanup")));
	}

	TEST_METHOD(OutstandingRegistrationIsRemovedBeforeIndependentEngine)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-OBJ-ENGINE-TEARDOWN-REGISTRY-ISOLATION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		const FString ScriptSource = BuildSource();
		const FString FirstModuleName =
			TEXT("NativeScriptObjectOutstandingRegistry");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-OBJ-ENGINE-TEARDOWN-REGISTRY-ISOLATION"),
			FirstModuleName,
			BuildReviewSource(
				ScriptSource,
				TEXT("Engine generation"),
				TEXT("outstanding owner")));

		FNativeTestEngine FirstEngine;
		FirstEngine.Create(*TestRunner);
		asITypeInfo* FirstOwnershipType = nullptr;
		void* OutstandingPointer = nullptr;
		ON_SCOPE_EXIT
		{
			if (OutstandingPointer != nullptr)
			{
				if (FirstEngine.Get() != nullptr
					&& FirstOwnershipType != nullptr)
				{
					static_cast<asCScriptEngine*>(FirstEngine.Get())
						->ReleaseScriptObject(
							OutstandingPointer,
							FirstOwnershipType);
				}
				else
				{
					FMemory::Free(OutstandingPointer);
				}
				OutstandingPointer = nullptr;
			}
			FirstEngine.Destroy();
		};

		asIScriptEngine* const FirstScriptEngine = FirstEngine.Get();
		ASSERT_THAT(IsNotNull(
			FirstScriptEngine,
			TEXT("Outstanding registry product should create its first raw SDK engine")));
		if (FirstScriptEngine == nullptr)
		{
			return;
		}

		const FTCHARToUTF8 FirstModuleNameUtf8(*FirstModuleName);
		const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);
		asIScriptModule* FirstModule = nullptr;
		const int FirstBuildResult = CompileNativeModule(
			FirstScriptEngine,
			FirstModuleNameUtf8.Get(),
			ScriptSourceUtf8.Get(),
			FirstModule);
		ASSERT_THAT(IsTrue(
			FirstBuildResult >= 0,
			*FString::Printf(
				TEXT("Outstanding registry source should compile in the first engine. Build=%d Messages={%s}"),
				FirstBuildResult,
				*FirstEngine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(
			FirstModule,
			TEXT("Outstanding registry source should publish its first module")));
		if (FirstBuildResult < 0 || FirstModule == nullptr)
		{
			return;
		}

		FirstOwnershipType = FindOwnershipType(FirstModule);
		ASSERT_THAT(IsNotNull(
			FirstOwnershipType,
			TEXT("Outstanding registry product should resolve its first OwnershipProbe type")));
		if (FirstOwnershipType == nullptr)
		{
			return;
		}

		OutstandingPointer =
			static_cast<asCScriptEngine*>(FirstScriptEngine)
				->CreateUninitializedScriptObject(FirstOwnershipType);
		ASSERT_THAT(IsNotNull(
			OutstandingPointer,
			TEXT("Outstanding registry product should allocate an application-held raw object")));
		if (OutstandingPointer == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			FirstOwnershipType,
			UASClass::GetRawScriptObjectType(OutstandingPointer),
			TEXT("Application-held raw object should have an exact registry entry before engine teardown")));

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] Source-backed current-fork contract: engine teardown unregisters the raw object but does not call CallFree for the application-held allocation, so this owner manually frees it and makes no shutdown-free claim. The declared global memory setter/reset APIs have no definitions and do not cover raw CallFree; if shutdown later acquires allocation ownership, replace this manual-free strategy before enabling that semantic to avoid double free"));
		FirstEngine.Destroy();
		FirstOwnershipType = nullptr;
		ASSERT_THAT(IsNull(
			UASClass::GetRawScriptObjectType(OutstandingPointer),
			TEXT("Engine teardown should remove every raw-object registry entry owned by the retired engine")));
		FMemory::Free(OutstandingPointer);
		OutstandingPointer = nullptr;

		const FString SuccessorModuleName =
			TEXT("NativeScriptObjectOutstandingRegistrySuccessor");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("RT-OBJ-ENGINE-TEARDOWN-REGISTRY-ISOLATION"),
			SuccessorModuleName,
			BuildReviewSource(
				ScriptSource,
				TEXT("Engine generation"),
				TEXT("independent successor")));

		FNativeTestEngine SuccessorEngine;
		SuccessorEngine.Create(*TestRunner);
		asITypeInfo* SuccessorOwnershipType = nullptr;
		void* SuccessorPointer = nullptr;
		ON_SCOPE_EXIT
		{
			if (SuccessorPointer != nullptr
				&& SuccessorEngine.Get() != nullptr
				&& SuccessorOwnershipType != nullptr)
			{
				static_cast<asCScriptEngine*>(SuccessorEngine.Get())
					->ReleaseScriptObject(
						SuccessorPointer,
						SuccessorOwnershipType);
				SuccessorPointer = nullptr;
			}
			SuccessorEngine.Destroy();
		};

		asIScriptEngine* const SuccessorScriptEngine =
			SuccessorEngine.Get();
		ASSERT_THAT(IsNotNull(
			SuccessorScriptEngine,
			TEXT("Outstanding registry product should create an independent successor engine")));
		if (SuccessorScriptEngine == nullptr)
		{
			return;
		}

		const FTCHARToUTF8 SuccessorModuleNameUtf8(*SuccessorModuleName);
		asIScriptModule* SuccessorModule = nullptr;
		const int SuccessorBuildResult = CompileNativeModule(
			SuccessorScriptEngine,
			SuccessorModuleNameUtf8.Get(),
			ScriptSourceUtf8.Get(),
			SuccessorModule);
		ASSERT_THAT(IsTrue(
			SuccessorBuildResult >= 0,
			*FString::Printf(
				TEXT("Outstanding registry source should compile in the independent successor. Build=%d Messages={%s}"),
				SuccessorBuildResult,
				*SuccessorEngine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(
			SuccessorModule,
			TEXT("Outstanding registry source should publish its successor module")));
		if (SuccessorBuildResult < 0 || SuccessorModule == nullptr)
		{
			return;
		}

		SuccessorOwnershipType = FindOwnershipType(SuccessorModule);
		ASSERT_THAT(IsNotNull(
			SuccessorOwnershipType,
			TEXT("Independent successor should resolve its own OwnershipProbe type")));
		if (SuccessorOwnershipType == nullptr)
		{
			return;
		}

		asCScriptEngine* const RawSuccessorEngine =
			static_cast<asCScriptEngine*>(SuccessorScriptEngine);
		SuccessorPointer =
			RawSuccessorEngine->CreateUninitializedScriptObject(
				SuccessorOwnershipType);
		ASSERT_THAT(IsNotNull(
			SuccessorPointer,
			TEXT("Independent successor should allocate its own raw object")));
		if (SuccessorPointer == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			SuccessorOwnershipType,
			UASClass::GetRawScriptObjectType(SuccessorPointer),
			TEXT("Independent successor object should map only to its own TypeInfo")));
		void* const ReleasedSuccessorPointer = SuccessorPointer;
		RawSuccessorEngine->ReleaseScriptObject(
			SuccessorPointer,
			SuccessorOwnershipType);
		SuccessorPointer = nullptr;
		ASSERT_THAT(IsNull(
			UASClass::GetRawScriptObjectType(
				ReleasedSuccessorPointer),
			TEXT("Independent successor final release should leave no raw-object registry entry")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			SuccessorScriptEngine->DiscardModule(
				SuccessorModuleNameUtf8.Get()),
			TEXT("Independent successor should discard its module after final release")));
		ASSERT_THAT(IsNull(
			SuccessorScriptEngine->GetModule(
				SuccessorModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			TEXT("Independent successor should leave no name-visible module")));
	}

	TEST_METHOD(UserDataSlotsRetainCurrentForkStubBehavior)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-OBJ-USERDATA-FORK-STUB",
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

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Script object user-data product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeScriptObjectUserDataDepth");
		const FString ScriptSource = BuildSource();
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*ScriptSource);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(
			ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			Module);
		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*FString::Printf(
				TEXT("Script object user-data source should compile. Build=%d Messages={%s}"),
				BuildResult,
				*Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Script object user-data source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleNameUtf8.Get()); };

		asITypeInfo* const OwnershipType = FindOwnershipType(Module);
		ASSERT_THAT(IsNotNull(
			OwnershipType,
			TEXT("Script object user-data product should resolve OwnershipProbe exactly")));
		if (OwnershipType == nullptr)
		{
			return;
		}

		asCScriptEngine* const RawEngine = static_cast<asCScriptEngine*>(ScriptEngine);
		void* const ObjectPointer = RawEngine->CreateScriptObject(OwnershipType);
		ASSERT_THAT(IsNotNull(
			ObjectPointer,
			TEXT("Script object user-data product should construct a live object")));
		if (ObjectPointer == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			RawEngine->ReleaseScriptObject(ObjectPointer, OwnershipType);
		};
		asIScriptObject* const Object =
			static_cast<asIScriptObject*>(ObjectPointer);

		struct FSlot
		{
			const TCHAR* Name;
			asPWORD Value;
		};
		const FSlot Slots[] =
		{
			{ TEXT("default"), 0 },
			{ TEXT("custom"), 73 },
		};
		const TCHAR* States[] =
		{
			TEXT("initial"),
			TEXT("install"),
			TEXT("replace"),
			TEXT("clear"),
		};
		int32 UserDataA = 17;
		int32 UserDataB = 29;

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] asIScriptObject user-data accessors are storage stubs in this fork; every slot and transition remains null until keyed storage and destruction cleanup are implemented"));
		for (const FSlot& Slot : Slots)
		{
			for (const TCHAR* State : States)
			{
				const FString CaseId = MakeNativeCaseId(
					"RT-OBJ-USERDATA-FORK-STUB",
					{ Slot.Name, State });
				PrintGeneratedAsSource(
					*TestRunner,
					CaseId,
					ModuleName,
					BuildReviewSource(
						ScriptSource,
						TEXT("User-data transition"),
						*FString::Printf(
							TEXT("%s / %s"),
							Slot.Name,
							State)));
			}

			ASSERT_THAT(IsNull(
				Object->GetUserData(Slot.Value),
				*FString::Printf(
					TEXT("%s user-data slot should begin null"),
					Slot.Name)));
			ASSERT_THAT(IsNull(
				Object->SetUserData(&UserDataA, Slot.Value),
				*FString::Printf(
					TEXT("%s user-data stub should return null for install"),
					Slot.Name)));
			ASSERT_THAT(IsNull(
				Object->GetUserData(Slot.Value),
				*FString::Printf(
					TEXT("%s user-data stub should not expose installed data"),
					Slot.Name)));
			ASSERT_THAT(IsNull(
				Object->SetUserData(&UserDataB, Slot.Value),
				*FString::Printf(
					TEXT("%s user-data stub should return null for replacement"),
					Slot.Name)));
			ASSERT_THAT(IsNull(
				Object->GetUserData(Slot.Value),
				*FString::Printf(
					TEXT("%s user-data stub should remain null after replacement"),
					Slot.Name)));
			ASSERT_THAT(IsNull(
				Object->SetUserData(nullptr, Slot.Value),
				*FString::Printf(
					TEXT("%s user-data stub should return null for clear"),
					Slot.Name)));
			ASSERT_THAT(IsNull(
				Object->GetUserData(Slot.Value),
				*FString::Printf(
					TEXT("%s user-data stub should remain null after clear"),
					Slot.Name)));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
