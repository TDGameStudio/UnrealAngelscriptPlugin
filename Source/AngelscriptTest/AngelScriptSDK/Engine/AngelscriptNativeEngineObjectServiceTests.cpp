#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeEngineObjectServiceTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.ObjectServices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FServiceObservation
	{
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		int32 DestroyedObjects = 0;
		int32 DelegateCalls = 0;
		int32 ImplicitCastCalls = 0;
		int32 ExplicitCastCalls = 0;
	};

	struct FServiceObject
	{
		FServiceObject(
			FServiceObservation& InObservation,
			const int32 InValue)
			: Observation(&InObservation)
			, Value(InValue)
		{
		}

		int32 AddRef()
		{
			++Observation->AddRefCalls;
			return ++ReferenceCount;
		}

		int32 Release()
		{
			++Observation->ReleaseCalls;
			const int32 RemainingReferences = --ReferenceCount;
			if (RemainingReferences == 0)
			{
				++Observation->DestroyedObjects;
				delete this;
			}
			return RemainingReferences;
		}

		FServiceObservation* Observation = nullptr;
		int32 Value = 0;
		int32 ReferenceCount = 1;
	};

	static void GenericAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FServiceObject* const Object =
				static_cast<FServiceObject*>(Generic->GetObject()))
			{
				Object->AddRef();
			}
		}
	}

	static void GenericRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FServiceObject* const Object =
				static_cast<FServiceObject*>(Generic->GetObject()))
			{
				Object->Release();
			}
		}
	}

	static void GenericInvoke(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FServiceObject* const Object =
			static_cast<FServiceObject*>(Generic->GetObject());
		if (Object == nullptr)
		{
			Generic->SetReturnDWord(0);
			return;
		}

		++Object->Observation->DelegateCalls;
		Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
	}

	static void GenericGlobal(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(Generic->GetArgDWord(0));
		}
	}

	static void GenericImplicitCast(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FServiceObject* const Object =
			static_cast<FServiceObject*>(Generic->GetObject());
		if (Object != nullptr)
		{
			++Object->Observation->ImplicitCastCalls;
			Object->AddRef();
		}
		Generic->SetReturnAddress(Object);
	}

	static void GenericExplicitCast(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FServiceObject* const Object =
			static_cast<FServiceObject*>(Generic->GetObject());
		if (Object != nullptr)
		{
			++Object->Observation->ExplicitCastCalls;
			Object->AddRef();
		}
		Generic->SetReturnAddress(Object);
	}

	static bool RegisterReferenceType(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const ANSICHAR* TypeName)
	{
		FNoDiscardAsserter Assert(Test);
		bool bRegistered = true;
		bRegistered &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				TypeName,
				0,
				asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0,
			*FString::Printf(
				TEXT("Object service fixture should register reference type %hs"),
				TypeName));
		bRegistered &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(GenericAddRef),
				asCALL_GENERIC) >= 0,
			*FString::Printf(
				TEXT("Object service fixture should register addref for %hs"),
				TypeName));
		bRegistered &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(GenericRelease),
				asCALL_GENERIC) >= 0,
			*FString::Printf(
				TEXT("Object service fixture should register release for %hs"),
				TypeName));
		return bRegistered;
	}

	static bool RegisterObjectServices(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine)
	{
		FNoDiscardAsserter Assert(Test);
		bool bRegistered =
			RegisterReferenceType(
				Test,
				ScriptEngine,
				"FDelegateServiceReceiver")
			&& RegisterReferenceType(
				Test,
				ScriptEngine,
				"FCastServiceSource")
			&& RegisterReferenceType(
				Test,
				ScriptEngine,
				"FCastServiceTarget")
			&& RegisterReferenceType(
				Test,
				ScriptEngine,
				"FCastServiceUnrelated")
			&& RegisterReferenceType(
				Test,
				ScriptEngine,
				"FCastServiceMissing");
		bRegistered &= Assert.IsTrue(
			ScriptEngine.RegisterObjectMethod(
				"FDelegateServiceReceiver",
				"int Invoke() const",
				asFUNCTION(GenericInvoke),
				asCALL_GENERIC) >= 0,
			TEXT("Object service fixture should register its delegate receiver method"));
		GlobalFunctionId = ScriptEngine.RegisterGlobalFunction(
				"int ObjectServiceGlobal(int Value)",
				asFUNCTION(GenericGlobal),
				asCALL_GENERIC);
		bRegistered &= Assert.IsTrue(
			GlobalFunctionId >= 0,
			TEXT("Object service fixture should register its non-method rejection target"));
		bRegistered &= Assert.IsTrue(
			ScriptEngine.RegisterObjectMethod(
				"FCastServiceSource",
				"FCastServiceTarget opImplCast()",
				asFUNCTION(GenericImplicitCast),
				asCALL_GENERIC) >= 0,
			TEXT("Object service fixture should register its implicit cast method"));
		bRegistered &= Assert.IsTrue(
			ScriptEngine.RegisterObjectMethod(
				"FCastServiceSource",
				"FCastServiceUnrelated opCast()",
				asFUNCTION(GenericExplicitCast),
				asCALL_GENERIC) >= 0,
			TEXT("Object service fixture should register its explicit cast method"));
		return bRegistered;
	}

	static void PrintReviewSource(
		FAutomationTestBase& Test,
		const TCHAR* CaseId,
		const TCHAR* Operation,
		const TCHAR* Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Case: %s"), CaseId));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Native operation: %s"), Operation));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Expected: %s"), Expected));
		PrintGeneratedAsSource(
			Test,
			CaseId,
			TEXT("NativeEngineObjectServiceReview"),
			Source);
	}

	static asITypeInfo* FindType(
		asIScriptEngine& ScriptEngine,
		const ANSICHAR* TypeName)
	{
		return ScriptEngine.GetTypeInfoByName(TypeName);
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static FServiceObservation Observation;
	inline static bool bServicesRegistered = false;
	inline static int32 GlobalFunctionId = asNO_FUNCTION;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine != nullptr)
		{
			bServicesRegistered =
				RegisterObjectServices(*TestRunner, *ScriptEngine);
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		bServicesRegistered = false;
		GlobalFunctionId = asNO_FUNCTION;
		Observation = FServiceObservation();
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
		Observation = FServiceObservation();
	}

	TEST_METHOD(DelegateCreationRejectsInvalidInputsAndRecordsUnsafePositivePath)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Delegate object service should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bServicesRegistered)
		{
			return;
		}

		asITypeInfo* const ReceiverType =
			FindType(*ScriptEngine, "FDelegateServiceReceiver");
		asIScriptFunction* const ReceiverMethod =
			ReceiverType != nullptr && ReceiverType->GetMethodCount() == 1
				? ReceiverType->GetMethodByIndex(0)
				: nullptr;
		asIScriptFunction* const GlobalFunction =
			GlobalFunctionId >= 0
				? ScriptEngine->GetFunctionById(GlobalFunctionId)
				: nullptr;
		ASSERT_THAT(IsNotNull(
			ReceiverType,
			TEXT("Delegate object service should resolve its exact receiver type")));
		ASSERT_THAT(IsNotNull(
			ReceiverMethod,
			TEXT("Delegate object service should resolve its sole registered receiver method by exact index")));
		ASSERT_THAT(IsNotNull(
			GlobalFunction,
			TEXT("Delegate object service should resolve its exact global rejection target")));
		if (ReceiverType == nullptr
			|| ReceiverMethod == nullptr
			|| GlobalFunction == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			FString(TEXT("Invoke")),
			FString(UTF8_TO_TCHAR(ReceiverMethod->GetName())),
			TEXT("Indexed receiver method should retain the exact registered name")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("ObjectServiceGlobal")),
			FString(UTF8_TO_TCHAR(GlobalFunction->GetName())),
			TEXT("Registration-ID global function should retain the exact registered name")));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] TypeInfo GetMethodByDecl rejects the registered const declaration and GetGlobalFunctionByDecl rejects the registered named-parameter declaration in this fork; the fixture proves exact registration inventory and resolves the method by unique index plus the global function by registration ID"));

		FServiceObject* Receiver =
			new FServiceObject(Observation, 40);
		ON_SCOPE_EXIT
		{
			if (Receiver != nullptr)
			{
				Receiver->Release();
			}
		};

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-NULL-FUNCTION"),
			TEXT("CreateDelegate(nullptr, Receiver)"),
			TEXT("null delegate and unchanged receiver ownership"));
		ASSERT_THAT(IsNull(
			ScriptEngine->CreateDelegate(nullptr, Receiver),
			TEXT("CreateDelegate should reject a null function")));
		ASSERT_THAT(AreEqual(
			1,
			Receiver->ReferenceCount,
			TEXT("Null-function rejection should not retain the receiver")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-NULL-OBJECT"),
			TEXT("CreateDelegate(ReceiverMethod, nullptr)"),
			TEXT("null delegate and no method retention"));
		ASSERT_THAT(IsNull(
			ScriptEngine->CreateDelegate(ReceiverMethod, nullptr),
			TEXT("CreateDelegate should reject a null object")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-NON-METHOD"),
			TEXT("CreateDelegate(GlobalFunction, Receiver)"),
			TEXT("null delegate because a global function has no receiver type"));
		ASSERT_THAT(IsNull(
			ScriptEngine->CreateDelegate(GlobalFunction, Receiver),
			TEXT("CreateDelegate should reject a global function")));
		ASSERT_THAT(AreEqual(
			1,
			Receiver->ReferenceCount,
			TEXT("Non-method rejection should not retain the receiver")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-POSITIVE-DEFERRED"),
			TEXT("CreateDelegate(ReceiverMethod, Receiver)"),
			TEXT("not executed until delegate state, argument metadata, dispatch, and GC ownership are repaired"));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-PARAMETER-DEFERRED"),
			TEXT("Context.SetArgDWord(...) on a prepared parameterized delegate"),
			TEXT("not executed because MakeDelegate does not copy parameter offsets or argument-space metadata"));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-EXECUTION-DEFERRED"),
			TEXT("Context.Execute() on a prepared delegate"),
			TEXT("not executed because Execute has no top-level asFUNC_DELEGATE dispatch"));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-DELEGATE-LIFECYCLE-GC-DEFERRED"),
			TEXT("Release the application delegate reference and shut down the engine"),
			TEXT("not executed because the current GC/delegate ownership path can retain an unsafe shutdown entry"));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] Valid CreateDelegate is intentionally deferred: asCScriptFunction stores delegate state in static fields, MakeDelegate omits parameter offsets and argument-space metadata, Execute lacks a top-level asFUNC_DELEGATE branch, and the retained focused runs prove access violations in SetArgDWord, ExecuteNext, and ReportAndReleaseUndestroyedObjects during shutdown"));
		ASSERT_THAT(AreEqual(
			1,
			Receiver->ReferenceCount,
			TEXT("Deferred positive delegate path should leave receiver ownership unchanged")));
		ASSERT_THAT(AreEqual(
			0,
			Observation.AddRefCalls,
			TEXT("Deferred positive delegate path should not invoke receiver addref")));
		ASSERT_THAT(AreEqual(
			0,
			Observation.ReleaseCalls,
			TEXT("Deferred positive delegate path should not invoke receiver release before scope cleanup")));
	}

	TEST_METHOD(ReferenceCastGuardsConversionsAndOwnership)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("RefCast object service should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bServicesRegistered)
		{
			return;
		}

		asITypeInfo* const SourceType =
			FindType(*ScriptEngine, "FCastServiceSource");
		asITypeInfo* const TargetType =
			FindType(*ScriptEngine, "FCastServiceTarget");
		asITypeInfo* const UnrelatedType =
			FindType(*ScriptEngine, "FCastServiceUnrelated");
		asITypeInfo* const MissingType =
			FindType(*ScriptEngine, "FCastServiceMissing");
		ASSERT_THAT(IsNotNull(
			SourceType,
			TEXT("RefCast object service should resolve its source type")));
		ASSERT_THAT(IsNotNull(
			TargetType,
			TEXT("RefCast object service should resolve its implicit target type")));
		ASSERT_THAT(IsNotNull(
			UnrelatedType,
			TEXT("RefCast object service should resolve its explicit target type")));
		ASSERT_THAT(IsNotNull(
			MissingType,
			TEXT("RefCast object service should resolve its no-conversion target type")));
		if (SourceType == nullptr
			|| TargetType == nullptr
			|| UnrelatedType == nullptr
			|| MissingType == nullptr)
		{
			return;
		}

		FServiceObject* Source =
			new FServiceObject(Observation, 17);
		ON_SCOPE_EXIT
		{
			if (Source != nullptr)
			{
				Source->Release();
			}
		};

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-NULL-OUTPUT"),
			TEXT("RefCastObject(Source, SourceType, TargetType, nullptr)"),
			TEXT("asINVALID_ARG without touching object ownership"));
		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				TargetType,
				nullptr),
			TEXT("RefCastObject should reject a null output pointer")));
		ASSERT_THAT(AreEqual(
			1,
			Source->ReferenceCount,
			TEXT("Null-output rejection should not change ownership")));

		void* CastResult = reinterpret_cast<void*>(UPTRINT(1));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-NULL-FROM"),
			TEXT("RefCastObject(Source, nullptr, TargetType, &Result)"),
			TEXT("asINVALID_ARG and Result reset to null"));
		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			ScriptEngine->RefCastObject(
				Source,
				nullptr,
				TargetType,
				&CastResult),
			TEXT("RefCastObject should reject a null source type")));
		ASSERT_THAT(IsNull(
			CastResult,
			TEXT("Null-source-type rejection should clear the output")));

		CastResult = reinterpret_cast<void*>(UPTRINT(1));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-NULL-TO"),
			TEXT("RefCastObject(Source, SourceType, nullptr, &Result)"),
			TEXT("asINVALID_ARG and Result reset to null"));
		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				nullptr,
				&CastResult),
			TEXT("RefCastObject should reject a null target type")));
		ASSERT_THAT(IsNull(
			CastResult,
			TEXT("Null-target-type rejection should clear the output")));

		CastResult = reinterpret_cast<void*>(UPTRINT(1));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-NULL-OBJECT"),
			TEXT("RefCastObject(nullptr, SourceType, TargetType, &Result)"),
			TEXT("asSUCCESS and Result remains null"));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RefCastObject(
				nullptr,
				SourceType,
				TargetType,
				&CastResult),
			TEXT("RefCastObject should accept a null object for valid types")));
		ASSERT_THAT(IsNull(
			CastResult,
			TEXT("Null-object cast should return a null result")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-IDENTITY"),
			TEXT("RefCastObject(Source, SourceType, SourceType, &Result)"),
			TEXT("same pointer with one additional reference"));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				SourceType,
				&CastResult),
			TEXT("RefCastObject should accept an identity cast")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(Source),
			CastResult,
			TEXT("Identity cast should preserve exact pointer identity")));
		ASSERT_THAT(AreEqual(
			2,
			Source->ReferenceCount,
			TEXT("Identity cast should add one result reference")));
		ScriptEngine->ReleaseScriptObject(CastResult, SourceType);
		CastResult = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			Source->ReferenceCount,
			TEXT("Releasing the identity result should restore source ownership")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-NO-CONVERSION"),
			TEXT("RefCastObject(Source, SourceType, MissingType, &Result)"),
			TEXT("asSUCCESS with null result and no ownership change"));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				MissingType,
				&CastResult),
			TEXT("Unavailable RefCastObject conversion should remain a successful null result")));
		ASSERT_THAT(IsNull(
			CastResult,
			TEXT("Unavailable conversion should return null")));
		ASSERT_THAT(AreEqual(
			1,
			Source->ReferenceCount,
			TEXT("Unavailable conversion should not change ownership")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-IMPLICIT"),
			TEXT("RefCastObject(Source, SourceType, TargetType, &Result, true)"),
			TEXT("implicit cast callback, same pointer, and one owned result reference"));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				TargetType,
				&CastResult,
				true),
			TEXT("RefCastObject should execute the registered implicit cast")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(Source),
			CastResult,
			TEXT("Implicit conversion should preserve the fixture pointer")));
		ASSERT_THAT(AreEqual(
			1,
			Observation.ImplicitCastCalls,
			TEXT("Implicit conversion should invoke its cast callback once")));
		ASSERT_THAT(AreEqual(
			2,
			Source->ReferenceCount,
			TEXT("Implicit conversion callback should own one result reference")));
		ScriptEngine->ReleaseScriptObject(CastResult, TargetType);
		CastResult = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			Source->ReferenceCount,
			TEXT("Releasing the implicit result should restore source ownership")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-EXPLICIT-SUPPRESSED"),
			TEXT("RefCastObject(Source, SourceType, UnrelatedType, &Result, true)"),
			TEXT("asSUCCESS with null result because explicit casts are disabled"));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				UnrelatedType,
				&CastResult,
				true),
			TEXT("Implicit-only RefCastObject should suppress an explicit cast")));
		ASSERT_THAT(IsNull(
			CastResult,
			TEXT("Suppressed explicit cast should return null")));
		ASSERT_THAT(AreEqual(
			0,
			Observation.ExplicitCastCalls,
			TEXT("Suppressed explicit cast should not invoke its callback")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-EXPLICIT"),
			TEXT("RefCastObject(Source, SourceType, UnrelatedType, &Result, false)"),
			TEXT("explicit cast callback, same pointer, and one owned result reference"));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->RefCastObject(
				Source,
				SourceType,
				UnrelatedType,
				&CastResult,
				false),
			TEXT("RefCastObject should execute the registered explicit cast when allowed")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(Source),
			CastResult,
			TEXT("Explicit conversion should preserve the fixture pointer")));
		ASSERT_THAT(AreEqual(
			1,
			Observation.ExplicitCastCalls,
			TEXT("Explicit conversion should invoke its callback once")));
		ASSERT_THAT(AreEqual(
			2,
			Source->ReferenceCount,
			TEXT("Explicit conversion callback should own one result reference")));
		ScriptEngine->ReleaseScriptObject(CastResult, UnrelatedType);
		CastResult = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			Source->ReferenceCount,
			TEXT("Releasing the explicit result should restore source ownership")));

		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-UNSAFE-MISMATCH-DEFERRED"),
			TEXT("RefCastObject(ObjectWhoseRuntimeTypeDiffersFromFromType, ...)"),
			TEXT("not executed because the current fork trusts the caller-provided concrete type"));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] RefCastObject validates null pointers but does not verify that obj belongs to fromType or that TypeInfo values belong to this engine; mismatched-object and cross-engine probes are intentionally not executed because the cast path can dispatch an unrelated method table or reinterpret object layout"));
		PrintReviewSource(
			*TestRunner,
			TEXT("ENG-OBJECT-SERVICE-REFCAST-CONTRACT-CROSS-ENGINE-TYPE-DEFERRED"),
			TEXT("RefCastObject(Source, TypeInfoFromAnotherEngine, TargetType, &Result)"),
			TEXT("not executed because the current fork does not validate TypeInfo engine ownership"));

		ASSERT_THAT(AreEqual(
			0,
			Source->Release(),
			TEXT("Releasing the source owner should destroy it after every cast result is balanced")));
		Source = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			Observation.DestroyedObjects,
			TEXT("RefCast object service should destroy exactly one source object")));
		ASSERT_THAT(AreEqual(
			Observation.AddRefCalls + 1,
			Observation.ReleaseCalls,
			TEXT("Each cast-owned reference plus the original owner should be released exactly once")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
