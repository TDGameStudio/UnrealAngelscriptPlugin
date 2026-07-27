#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeContextPublicApiDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ContextPublicApiDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr asPWORD ContextCleanupObservationSlot = 0x43545843;
	static constexpr asPWORD VarTypeObservationSlot = 0x43545856;

	struct FContextCleanupObservation
	{
		asIScriptContext* CleanedContext = nullptr;
		int32 CleanupCount = 0;
	};

	struct FContextPoolObservation
	{
		asIScriptEngine* RequestEngine = nullptr;
		asIScriptEngine* ReturnEngine = nullptr;
		asIScriptContext* LastRequestedContext = nullptr;
		asIScriptContext* LastReturnedContext = nullptr;
		void* RequestParameter = nullptr;
		void* ReturnParameter = nullptr;
		int32 RequestCount = 0;
		int32 ReturnCount = 0;
	};

	struct FVarTypeObservation
	{
		asIScriptEngine* Engine = nullptr;
		asIScriptFunction* Function = nullptr;
		void* Address = nullptr;
		int32 TypeId = asINVALID_TYPE;
		int32 CallbackCount = 0;
	};

	struct FVarTypeCase
	{
		const TCHAR* CaseId;
		void* Address;
		int32 TypeId;
		int32 ExpectedResult;
	};

	static FString BuildReviewSource(
		const TCHAR* Operation,
		const TCHAR* Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("// Raw AngelScript SDK review input"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Operation: %s"), Operation));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Expected: %s"), Expected));
		return Source;
	}

	static void PrintSource(
		FAutomationTestBase& Test,
		const TCHAR* SourceId,
		const FString& Source)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(
			Test,
			SourceId,
			TEXT("NativeContextPublicApi"),
			Source);
	}

	static void CleanupContextUserData(asIScriptContext* Context)
	{
		if (Context == nullptr)
		{
			return;
		}

		FContextCleanupObservation* const Observation =
			static_cast<FContextCleanupObservation*>(
				Context->GetUserData(ContextCleanupObservationSlot));
		if (Observation != nullptr)
		{
			Observation->CleanedContext = Context;
			++Observation->CleanupCount;
		}
	}

	static asIScriptContext* RequestObservedContext(
		asIScriptEngine* ScriptEngine,
		void* Parameter)
	{
		FContextPoolObservation* const Observation =
			static_cast<FContextPoolObservation*>(Parameter);
		if (ScriptEngine == nullptr || Observation == nullptr)
		{
			return nullptr;
		}

		Observation->RequestEngine = ScriptEngine;
		Observation->RequestParameter = Parameter;
		Observation->LastRequestedContext = ScriptEngine->CreateContext();
		++Observation->RequestCount;
		return Observation->LastRequestedContext;
	}

	static void ReturnObservedContext(
		asIScriptEngine* ScriptEngine,
		asIScriptContext* Context,
		void* Parameter)
	{
		FContextPoolObservation* const Observation =
			static_cast<FContextPoolObservation*>(Parameter);
		if (Observation != nullptr)
		{
			Observation->ReturnEngine = ScriptEngine;
			Observation->ReturnParameter = Parameter;
			Observation->LastReturnedContext = Context;
			++Observation->ReturnCount;
		}

		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	static void InspectVariableArgument(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || Generic->GetEngine() == nullptr)
		{
			return;
		}

		FVarTypeObservation* const Observation =
			static_cast<FVarTypeObservation*>(
				Generic->GetEngine()->GetUserData(VarTypeObservationSlot));
		if (Observation == nullptr)
		{
			return;
		}

		Observation->Engine = Generic->GetEngine();
		Observation->Function = Generic->GetFunction();
		Observation->Address = Generic->GetArgAddress(0);
		Observation->TypeId = Generic->GetArgTypeId(0);
		++Observation->CallbackCount;

		int32 Result = INDEX_NONE;
		if (Observation->TypeId == asTYPEID_INT32
			&& Observation->Address != nullptr)
		{
			Result = 1000 + *static_cast<int32*>(Observation->Address);
		}
		else if (Observation->TypeId == asTYPEID_FLOAT64
			&& Observation->Address != nullptr)
		{
			Result = 2000
				+ static_cast<int32>(
					*static_cast<double*>(Observation->Address) * 10.0);
		}
		else if (Observation->TypeId == asTYPEID_BOOL
			&& Observation->Address != nullptr)
		{
			Result = 3000
				+ (*static_cast<asBYTE*>(Observation->Address) != 0 ? 1 : 0);
		}

		Generic->SetReturnDWord(static_cast<asDWORD>(Result));
	}

	static void InspectOrdinaryArgument(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(Generic->GetArgDWord(0));
		}
	}

	static asIScriptFunction* GetMethodByExactDecl(
		asITypeInfo* Type,
		const char* Declaration)
	{
		if (Type == nullptr || Declaration == nullptr)
		{
			return nullptr;
		}

		if (asIScriptFunction* const Function =
			Type->GetMethodByDecl(Declaration))
		{
			return Function;
		}

		asIScriptFunction* ExactFunction = nullptr;
		for (asUINT MethodIndex = 0;
			MethodIndex < Type->GetMethodCount();
			++MethodIndex)
		{
			asIScriptFunction* const Candidate =
				Type->GetMethodByIndex(MethodIndex);
			if (Candidate == nullptr)
			{
				continue;
			}

			const bool bMatchesUnqualified =
				FCStringAnsi::Strcmp(
					Candidate->GetDeclaration(
						false,
						false,
						false),
					Declaration) == 0;
			const bool bMatchesQualified =
				FCStringAnsi::Strcmp(
					Candidate->GetDeclaration(
						true,
						true,
						false),
					Declaration) == 0;
			if (!bMatchesUnqualified && !bMatchesQualified)
			{
				continue;
			}

			if (ExactFunction != nullptr)
			{
				return nullptr;
			}
			ExactFunction = Candidate;
		}

		return ExactFunction;
	}

public:
	TEST_METHOD(FallbackRequestReturnAndBalancedReferenceLifetime)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-POOL-FALLBACK-LIFETIME",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Context fallback product should create a case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		PrintSource(
			*TestRunner,
			TEXT("RT-CTX-POOL-FALLBACK-LIFETIME-REQUEST-ADDREF-RETURN"),
			BuildReviewSource(
				TEXT("RequestContext; AddRef; Release; ReturnContext; ReturnContext(null)"),
				TEXT("fallback creation, balanced ownership, final cleanup, and null safety")));

		FContextCleanupObservation CleanupObservation;
		ScriptEngine->SetContextUserDataCleanupCallback(
			CleanupContextUserData,
			ContextCleanupObservationSlot);

		asIScriptContext* Context = ScriptEngine->RequestContext();
		ASSERT_THAT(IsNotNull(
			Context,
			TEXT("Context fallback product should create a context through RequestContext")));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			ScriptEngine,
			Context->GetEngine(),
			TEXT("Requested fallback context should retain the exact engine identity")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_UNINITIALIZED),
			static_cast<int32>(Context->GetState()),
			TEXT("Requested fallback context should begin uninitialized")));
		ASSERT_THAT(IsNull(
			Context->SetUserData(
				&CleanupObservation,
				ContextCleanupObservationSlot),
			TEXT("Requested fallback context should begin with an empty cleanup slot")));

		const int32 AddRefCount = Context->AddRef();
		ASSERT_THAT(AreEqual(
			2,
			AddRefCount,
			TEXT("Context AddRef should expose the second owned reference")));
		ASSERT_THAT(AreEqual(
			1,
			Context->Release(),
			TEXT("Balanced context Release should retain the RequestContext reference")));

		asIScriptContext* const ReturnedIdentity = Context;
		ScriptEngine->ReturnContext(Context);
		Context = nullptr;
		ASSERT_THAT(AreEqual(
			1,
			CleanupObservation.CleanupCount,
			TEXT("Fallback ReturnContext should release the final reference and run cleanup exactly once")));
		ASSERT_THAT(AreEqual(
			ReturnedIdentity,
			CleanupObservation.CleanedContext,
			TEXT("Fallback ReturnContext should clean the exact requested context")));

		ScriptEngine->ReturnContext(nullptr);
		ASSERT_THAT(AreEqual(
			1,
			CleanupObservation.CleanupCount,
			TEXT("Fallback ReturnContext should ignore null without additional cleanup")));
	}

	TEST_METHOD(CallbackPairValidationRoutingAndClear)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-POOL-CALLBACK-ROUTING",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Context callback product should create a case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		PrintSource(
			*TestRunner,
			TEXT("RT-CTX-POOL-CALLBACK-ROUTING-PAIR-VALIDATION"),
			BuildReviewSource(
				TEXT("SetContextCallbacks paired/request-only/return-only/clear; RequestContext; ReturnContext"),
				TEXT("paired routing remains installed after invalid updates and clear restores fallback")));

		FContextPoolObservation Observation;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetContextCallbacks(
				RequestObservedContext,
				ReturnObservedContext,
				&Observation),
			TEXT("Context callback product should install a paired request/return callback")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetContextCallbacks(nullptr, nullptr, nullptr);
		};

		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			ScriptEngine->SetContextCallbacks(
				RequestObservedContext,
				nullptr,
				&Observation),
			TEXT("Context callback product should reject a request-only update")));
		asIScriptContext* RequestOnlyPreservedContext =
			ScriptEngine->RequestContext();
		ASSERT_THAT(IsNotNull(
			RequestOnlyPreservedContext,
			TEXT("Rejected request-only update should preserve the installed callback pair")));
		ASSERT_THAT(AreEqual(
			Observation.LastRequestedContext,
			RequestOnlyPreservedContext,
			TEXT("Preserved request callback should return its exact created context")));
		ScriptEngine->ReturnContext(RequestOnlyPreservedContext);
		ASSERT_THAT(AreEqual(
			RequestOnlyPreservedContext,
			Observation.LastReturnedContext,
			TEXT("Preserved return callback should receive the exact requested context")));

		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			ScriptEngine->SetContextCallbacks(
				nullptr,
				ReturnObservedContext,
				&Observation),
			TEXT("Context callback product should reject a return-only update")));
		asIScriptContext* ReturnOnlyPreservedContext =
			ScriptEngine->RequestContext();
		ASSERT_THAT(IsNotNull(
			ReturnOnlyPreservedContext,
			TEXT("Rejected return-only update should preserve the installed callback pair")));
		ScriptEngine->ReturnContext(ReturnOnlyPreservedContext);

		ASSERT_THAT(AreEqual(
			2,
			Observation.RequestCount,
			TEXT("Both rejected one-sided updates should preserve request routing")));
		ASSERT_THAT(AreEqual(
			2,
			Observation.ReturnCount,
			TEXT("Both rejected one-sided updates should preserve return routing")));
		ASSERT_THAT(AreEqual(
			ScriptEngine,
			Observation.RequestEngine,
			TEXT("Request callback should receive the exact engine")));
		ASSERT_THAT(AreEqual(
			ScriptEngine,
			Observation.ReturnEngine,
			TEXT("Return callback should receive the exact engine")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&Observation),
			Observation.RequestParameter,
			TEXT("Request callback should receive the exact shared parameter")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&Observation),
			Observation.ReturnParameter,
			TEXT("Return callback should receive the exact shared parameter")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			ScriptEngine->SetContextCallbacks(nullptr, nullptr, nullptr),
			TEXT("Context callback product should clear both callbacks together")));
		asIScriptContext* const FallbackContext =
			ScriptEngine->RequestContext();
		ASSERT_THAT(IsNotNull(
			FallbackContext,
			TEXT("Cleared callbacks should restore fallback context creation")));
		ASSERT_THAT(AreEqual(
			2,
			Observation.RequestCount,
			TEXT("Cleared callbacks should not route fallback requests through the observer")));
		ScriptEngine->ReturnContext(FallbackContext);
		ASSERT_THAT(AreEqual(
			2,
			Observation.ReturnCount,
			TEXT("Cleared callbacks should not route fallback returns through the observer")));
	}

	TEST_METHOD(PreparedMethodReceiverAndReferenceReturnAddress)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-METHOD-OBJECT-REFERENCE-RETURN",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Context method product should create a case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class ContextReceiver
			{
				int Value;

				ContextReceiver()
				{
					Value = 7;
				}

				int ReadPlus(int Delta)
				{
					return Value + Delta;
				}

				int& GetValueReference()
				{
					return Value;
				}
			}
			)AS");
		PrintSource(
			*TestRunner,
			TEXT("RT-CTX-METHOD-OBJECT-REFERENCE-RETURN-SCRIPT"),
			UTF8_TO_TCHAR(ScriptSource.c_str()));

		const char* const ModuleName = "NativeContextMethodReceiver";
		asIScriptModule* Module = nullptr;
		const int32 BuildResult = CompileNativeModule(
			ScriptEngine,
			ModuleName,
			ScriptSource.c_str(),
			Module);
		ASSERT_THAT(IsTrue(
			BuildResult >= 0,
			*FString::Printf(
				TEXT("Context method source should compile. Build=%d Messages={%s}"),
				BuildResult,
				*Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Context method source should publish its module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { ScriptEngine->DiscardModule(ModuleName); };

		asITypeInfo* const ReceiverType =
			Module->GetTypeInfoByDecl("ContextReceiver");
		ASSERT_THAT(IsNotNull(
			ReceiverType,
			TEXT("Context method product should resolve the exact receiver type")));
		if (ReceiverType == nullptr)
		{
			return;
		}

		asIScriptFunction* const ReadPlus =
			GetMethodByExactDecl(
				ReceiverType,
				"int ReadPlus(const int)");
		asIScriptFunction* const GetValueReference =
			GetMethodByExactDecl(
				ReceiverType,
				"int& GetValueReference()");
		ASSERT_THAT(IsNotNull(
			ReadPlus,
			TEXT("Context method product should resolve ReadPlus by exact declaration")));
		ASSERT_THAT(IsNotNull(
			GetValueReference,
			TEXT("Context method product should resolve GetValueReference by exact declaration")));
		if (ReadPlus == nullptr || GetValueReference == nullptr)
		{
			return;
		}

		void* const ReceiverPointer =
			ScriptEngine->CreateScriptObject(ReceiverType);
		ASSERT_THAT(IsNotNull(
			ReceiverPointer,
			TEXT("Context method product should create a constructed receiver")));
		if (ReceiverPointer == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->ReleaseScriptObject(
				ReceiverPointer,
				ReceiverType);
		};

		asIScriptObject* const Receiver =
			static_cast<asIScriptObject*>(ReceiverPointer);
		void* const PropertyAddress =
			Receiver->GetAddressOfProperty(0);
		ASSERT_THAT(IsNotNull(
			PropertyAddress,
			TEXT("Context method product should expose the receiver property address")));
		if (PropertyAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			7,
			*static_cast<int32*>(PropertyAddress),
			TEXT("Context method product should run the receiver constructor")));

		asIScriptContext* const Context =
			ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(
			Context,
			TEXT("Context method product should create a reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(GetValueReference),
			TEXT("Context method product should prepare the reference-returning method")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetObject(ReceiverPointer),
			TEXT("SetObject should install the exact prepared method receiver")));
		ASSERT_THAT(IsNull(
			Context->GetReturnAddress(),
			TEXT("GetReturnAddress should remain null before execution finishes")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			TEXT("Context method product should execute the reference-returning method")));
		void* const ReturnAddress = Context->GetReturnAddress();
		ASSERT_THAT(AreEqual(
			PropertyAddress,
			ReturnAddress,
			TEXT("GetReturnAddress should expose the exact referenced property")));
		if (ReturnAddress == nullptr)
		{
			Context->Unprepare();
			return;
		}
		*static_cast<int32*>(ReturnAddress) = 40;
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Context method product should unprepare the reference-returning method")));
		ASSERT_THAT(IsNull(
			Context->GetReturnAddress(),
			TEXT("GetReturnAddress should clear after unprepare")));

		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(ReadPlus),
			TEXT("Context method product should prepare the primitive-returning method")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetObject(ReceiverPointer),
			TEXT("SetObject should reinstall the same receiver for reuse")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->SetArgDWord(0, 2),
			TEXT("Context method product should set the exact method argument")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			TEXT("Context method product should execute after reference mutation")));
		ASSERT_THAT(AreEqual(
			42,
			static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Reference mutation should remain observable through the receiver")));
		ASSERT_THAT(IsNull(
			Context->GetReturnAddress(),
			TEXT("Primitive returns should not expose an address")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Context method product should unprepare its final invocation")));

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] asCContext::SetObject writes the stack frame without validating prepared state or method ownership; unsafe unprepared and global-function probes are intentionally not executed"));
	}

	TEST_METHOD(VariableTypeMetadataByTypeAndInvalidState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-CTX-VARTYPE-ARGUMENT-METADATA",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Variable-type product should create a case-owned raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FVarTypeObservation Observation;
		ASSERT_THAT(IsNull(
			ScriptEngine->SetUserData(
				&Observation,
				VarTypeObservationSlot),
			TEXT("Variable-type product should begin with an empty observation slot")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetUserData(nullptr, VarTypeObservationSlot);
		};

		const int32 VarTypeFunctionId =
			ScriptEngine->RegisterGlobalFunction(
				"int InspectVariable(const ?&in Value)",
				asFUNCTION(InspectVariableArgument),
				asCALL_GENERIC);
		const int32 OrdinaryFunctionId =
			ScriptEngine->RegisterGlobalFunction(
				"int InspectOrdinary(int Value)",
				asFUNCTION(InspectOrdinaryArgument),
				asCALL_GENERIC);
		ASSERT_THAT(IsTrue(
			VarTypeFunctionId >= 0,
			TEXT("Variable-type product should register its wildcard function")));
		ASSERT_THAT(IsTrue(
			OrdinaryFunctionId >= 0,
			TEXT("Variable-type product should register its ordinary comparison function")));
		if (VarTypeFunctionId < 0 || OrdinaryFunctionId < 0)
		{
			return;
		}

		asIScriptFunction* const VarTypeFunction =
			ScriptEngine->GetGlobalFunctionByDecl(
				"int InspectVariable(const ?&in Value)");
		asIScriptFunction* const OrdinaryFunction =
			ScriptEngine->GetFunctionById(OrdinaryFunctionId);
		ASSERT_THAT(IsNotNull(
			VarTypeFunction,
			TEXT("Variable-type product should resolve its wildcard function exactly")));
		ASSERT_THAT(IsNotNull(
			OrdinaryFunction,
			TEXT("Variable-type product should resolve its ordinary function exactly")));
		ASSERT_THAT(AreEqual(
			VarTypeFunctionId,
			VarTypeFunction != nullptr ? VarTypeFunction->GetId() : asNO_FUNCTION,
			TEXT("Wildcard declaration lookup should retain the registration identity")));
		ASSERT_THAT(AreEqual(
			OrdinaryFunctionId,
			OrdinaryFunction != nullptr ? OrdinaryFunction->GetId() : asNO_FUNCTION,
			TEXT("Ordinary registration ID should resolve the exact registered function identity")));
		if (VarTypeFunction == nullptr || OrdinaryFunction == nullptr)
		{
			return;
		}
		int32 OrdinaryParameterTypeId = asINVALID_TYPE;
		ASSERT_THAT(AreEqual(
			0,
			FCStringAnsi::Strcmp(
				OrdinaryFunction->GetName(),
				"InspectOrdinary"),
			TEXT("Ordinary registration identity should retain the exact function name")));
		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1),
			OrdinaryFunction->GetParamCount(),
			TEXT("Ordinary registration identity should retain one parameter")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			OrdinaryFunction->GetParam(
				0,
				&OrdinaryParameterTypeId),
			TEXT("Ordinary registration identity should expose its parameter metadata")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asTYPEID_INT32),
			OrdinaryParameterTypeId,
			TEXT("Ordinary registration identity should retain its exact int32 parameter")));

		asIScriptContext* const Context =
			ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(
			Context,
			TEXT("Variable-type product should create a reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		int32 IntegerValue = 42;
		double DoubleValue = 3.5;
		asBYTE BoolValue = 1;
		const FVarTypeCase Cases[] =
		{
			{ TEXT("int32"), &IntegerValue, asTYPEID_INT32, 1042 },
			{ TEXT("float64"), &DoubleValue, asTYPEID_FLOAT64, 2035 },
			{ TEXT("bool"), &BoolValue, asTYPEID_BOOL, 3001 },
		};

		PrintSource(
			*TestRunner,
			TEXT("RT-CTX-VARTYPE-ARGUMENT-METADATA-UNPREPARED"),
			BuildReviewSource(
				TEXT("SetArgVarType(0, int32*, asTYPEID_INT32) before Prepare"),
				TEXT("asCONTEXT_NOT_PREPARED without state corruption")));
		ASSERT_THAT(AreEqual(
			asCONTEXT_NOT_PREPARED,
			Context->SetArgVarType(
				0,
				&IntegerValue,
				asTYPEID_INT32),
			TEXT("SetArgVarType should reject an unprepared context")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_UNINITIALIZED),
			static_cast<int32>(Context->GetState()),
			TEXT("Unprepared SetArgVarType rejection should preserve uninitialized state")));

		PrintSource(
			*TestRunner,
			TEXT("RT-CTX-VARTYPE-ARGUMENT-METADATA-INVALID-INDEX"),
			BuildReviewSource(
				TEXT("Prepare wildcard function; SetArgVarType(1, int32*, asTYPEID_INT32)"),
				TEXT("asINVALID_ARG and context error state")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(VarTypeFunction),
			TEXT("Variable-type product should prepare its invalid-index probe")));
		ASSERT_THAT(AreEqual(
			asINVALID_ARG,
			Context->SetArgVarType(
				1,
				&IntegerValue,
				asTYPEID_INT32),
			TEXT("SetArgVarType should reject a one-past-end argument index")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_ERROR),
			static_cast<int32>(Context->GetState()),
			TEXT("Invalid wildcard index should place the context in error state")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Variable-type product should recover after invalid index")));

		PrintSource(
			*TestRunner,
			TEXT("RT-CTX-VARTYPE-ARGUMENT-METADATA-INVALID-PARAMETER-TYPE"),
			BuildReviewSource(
				TEXT("Prepare ordinary int function; SetArgVarType(0, int32*, asTYPEID_INT32)"),
				TEXT("asINVALID_TYPE and context error state")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Prepare(OrdinaryFunction),
			TEXT("Variable-type product should prepare its invalid-parameter probe")));
		ASSERT_THAT(AreEqual(
			asINVALID_TYPE,
			Context->SetArgVarType(
				0,
				&IntegerValue,
				asTYPEID_INT32),
			TEXT("SetArgVarType should reject an ordinary typed parameter")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_ERROR),
			static_cast<int32>(Context->GetState()),
			TEXT("Invalid parameter kind should place the context in error state")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			TEXT("Variable-type product should recover after invalid parameter kind")));

		for (const FVarTypeCase& Case : Cases)
		{
			Observation = FVarTypeObservation();
			const FString SourceId = MakeNativeCaseId(
				"RT-CTX-VARTYPE-ARGUMENT-METADATA",
				{ Case.CaseId });
			PrintSource(
				*TestRunner,
				*SourceId,
				BuildReviewSource(
					*FString::Printf(
						TEXT("Prepare wildcard function; SetArgVarType(0, %s pointer, type id %d); Execute"),
						Case.CaseId,
						Case.TypeId),
					TEXT("generic callback observes exact address/type and returns the type-specific value")));

			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(VarTypeFunction),
				*FString::Printf(
					TEXT("Variable-type %s case should prepare the wildcard function"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->SetArgVarType(
					0,
					Case.Address,
					Case.TypeId),
				*FString::Printf(
					TEXT("Variable-type %s case should set its pointer and type id"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*FString::Printf(
					TEXT("Variable-type %s case should execute"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				Case.ExpectedResult,
				static_cast<int32>(Context->GetReturnDWord()),
				*FString::Printf(
					TEXT("Variable-type %s case should return the exact observed value"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				1,
				Observation.CallbackCount,
				*FString::Printf(
					TEXT("Variable-type %s case should invoke exactly one callback"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				ScriptEngine,
				Observation.Engine,
				TEXT("Variable-type callback should expose the exact engine")));
			ASSERT_THAT(AreEqual(
				VarTypeFunction,
				Observation.Function,
				TEXT("Variable-type callback should expose the exact function")));
			ASSERT_THAT(AreEqual(
				Case.Address,
				Observation.Address,
				*FString::Printf(
					TEXT("Variable-type %s callback should expose the exact address"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				Case.TypeId,
				Observation.TypeId,
				*FString::Printf(
					TEXT("Variable-type %s callback should expose the exact type id"),
					Case.CaseId)));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*FString::Printf(
					TEXT("Variable-type %s case should unprepare cleanly"),
					Case.CaseId)));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
