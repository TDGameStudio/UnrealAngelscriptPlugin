#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeThisPointerTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.ThisPointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	inline static constexpr asPWORD ThisRecorderUserDataSlot = static_cast<asPWORD>(0x4E41544454484953ull);

	struct FThisFrameObservation
	{
		FString Declaration;
		int32 ThisTypeId = asINVALID_ARG;
		void* ThisPointer = nullptr;
	};

	struct FThisObservation
	{
		TArray<FThisFrameObservation> Frames;
		FString TargetDeclaration;
	};

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class NativeDebugBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint BaseProbe()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint Local = 41;"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Local;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint VirtualProbe()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 50;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class NativeDebugDerived : NativeDebugBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint DerivedProbe()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn BaseProbe() + 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint VirtualProbe() override"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 60;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunThisVirtual()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugBase Object = NativeDebugDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.VirtualProbe();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunThisProbe()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugDerived Object = NativeDebugDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.DerivedProbe();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunThisFault()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugDerived Object = nullptr;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Object.DerivedProbe();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static void CaptureThisFrames(asCContext* Context)
	{
		if (Context == nullptr)
		{
			return;
		}
		FThisObservation* const Observation = static_cast<FThisObservation*>(Context->GetUserData(ThisRecorderUserDataSlot));
		asIScriptFunction* const CurrentFunction = Context->GetFunction(0);
		if (Observation == nullptr
			|| CurrentFunction == nullptr
			|| Observation->TargetDeclaration.IsEmpty()
			|| Observation->TargetDeclaration != UTF8_TO_TCHAR(CurrentFunction->GetDeclaration()))
		{
			return;
		}

		Observation->Frames.Reset();
		for (asUINT StackLevel = 0; StackLevel < Context->GetCallstackSize(); ++StackLevel)
		{
			FThisFrameObservation Frame;
			if (asIScriptFunction* const Function = Context->GetFunction(StackLevel))
			{
				Frame.Declaration = UTF8_TO_TCHAR(Function->GetDeclaration());
			}
			Frame.ThisTypeId = Context->GetThisTypeId(StackLevel);
			Frame.ThisPointer = Context->GetThisPointer(StackLevel);
			Observation->Frames.Add(MoveTemp(Frame));
		}
	}

public:
	TEST_METHOD(CallsByFrameQueryAndState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-THIS-CALL-FRAME",
			ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Receiver debug product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = TEXT("NativeDebugThisPointer");
		const FString Source = BuildSource();
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		PrintGeneratedAsSource(*TestRunner, TEXT("DBG-THIS-CALL-FRAME"), ModuleName, Source);
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("Receiver debug source should compile. Build=%d Messages={%s}"),
				BuildResult, *Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(Module, TEXT("Receiver debug source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		};

		asIScriptFunction* const Probe = GetNativeFunctionByExactDecl(Module, "int RunThisProbe()");
		asIScriptFunction* const Virtual = GetNativeFunctionByExactDecl(Module, "int RunThisVirtual()");
		asIScriptFunction* const Fault = GetNativeFunctionByExactDecl(Module, "int RunThisFault()");
		ASSERT_THAT(IsNotNull(Probe, TEXT("Receiver debug product should resolve the member-call entry exactly")));
		ASSERT_THAT(IsNotNull(Virtual, TEXT("Receiver debug product should resolve the virtual base-view entry exactly")));
		ASSERT_THAT(IsNotNull(Fault, TEXT("Receiver debug product should resolve the null-receiver entry exactly")));
		if (Probe == nullptr || Virtual == nullptr || Fault == nullptr)
		{
			return;
		}

		asITypeInfo* const BaseType = Module->GetTypeInfoByName("NativeDebugBase");
		asITypeInfo* const DerivedType = Module->GetTypeInfoByName("NativeDebugDerived");
		const int32 BaseTypeId = BaseType != nullptr ? BaseType->GetTypeId() : asINVALID_TYPE;
		const int32 DerivedTypeId = DerivedType != nullptr ? DerivedType->GetTypeId() : asINVALID_TYPE;
		ASSERT_THAT(IsNotNull(BaseType, TEXT("Receiver debug product should publish the base receiver type")));
		ASSERT_THAT(IsNotNull(DerivedType, TEXT("Receiver debug product should publish the derived receiver type")));

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Receiver debug product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		asCContext* const RawContext = static_cast<asCContext*>(Context);
		FThisObservation Observation;
		Observation.TargetDeclaration = TEXT("int NativeDebugBase::BaseProbe()");
		Context->SetUserData(&Observation, ThisRecorderUserDataSlot);
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureThisFrames), TEXT("Receiver debug product should install its raw frame observer")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Probe), TEXT("Receiver debug member-call path should finish")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Receiver debug member-call path should return through base and derived methods")));
		ASSERT_THAT(IsTrue(Observation.Frames.Num() >= 3, TEXT("Receiver debug observer should retain base, derived, and global frames")));
		if (Observation.Frames.Num() >= 3)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("int NativeDebugBase::BaseProbe()")), Observation.Frames[0].Declaration, TEXT("Receiver debug current frame should be the base method")));
			ASSERT_THAT(AreEqual(BaseTypeId, Observation.Frames[0].ThisTypeId, TEXT("Receiver debug base frame should expose its declared receiver type")));
			ASSERT_THAT(IsNotNull(Observation.Frames[0].ThisPointer, TEXT("Receiver debug base frame should expose the live receiver pointer")));
			ASSERT_THAT(AreEqual(FString(TEXT("int NativeDebugDerived::DerivedProbe()")), Observation.Frames[1].Declaration, TEXT("Receiver debug caller frame should be the derived method")));
			ASSERT_THAT(AreEqual(DerivedTypeId, Observation.Frames[1].ThisTypeId, TEXT("Receiver debug derived frame should expose its declared receiver type")));
			ASSERT_THAT(AreEqual(Observation.Frames[0].ThisPointer, Observation.Frames[1].ThisPointer, TEXT("Receiver debug base and derived frames should retain object pointer identity")));
			ASSERT_THAT(AreEqual(FString(TEXT("int RunThisProbe()")), Observation.Frames[2].Declaration, TEXT("Receiver debug root frame should be the global entry")));
			ASSERT_THAT(AreEqual(0, Observation.Frames[2].ThisTypeId, TEXT("Receiver debug global frame should report no receiver type")));
			ASSERT_THAT(IsNull(Observation.Frames[2].ThisPointer, TEXT("Receiver debug global frame should report no receiver pointer")));
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Receiver debug member-call path should unprepare before the fault path")));

		Observation.Frames.Reset();
		Observation.TargetDeclaration = TEXT("int NativeDebugDerived::VirtualProbe()");
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Virtual), TEXT("Receiver debug virtual base-view path should finish")));
		ASSERT_THAT(AreEqual(60, static_cast<int32>(Context->GetReturnDWord()), TEXT("Receiver debug virtual base-view path should dispatch to the derived override")));
		ASSERT_THAT(IsTrue(Observation.Frames.Num() >= 2, TEXT("Receiver debug virtual base-view observer should retain derived and global frames")));
		if (Observation.Frames.Num() >= 2)
		{
			ASSERT_THAT(AreEqual(FString(TEXT("int NativeDebugDerived::VirtualProbe()")), Observation.Frames[0].Declaration, TEXT("Receiver debug virtual current frame should expose the derived override declaration")));
			ASSERT_THAT(AreEqual(DerivedTypeId, Observation.Frames[0].ThisTypeId, TEXT("Receiver debug virtual current frame should expose the derived receiver type")));
			ASSERT_THAT(IsNotNull(Observation.Frames[0].ThisPointer, TEXT("Receiver debug virtual current frame should expose the live derived receiver")));
			ASSERT_THAT(AreEqual(FString(TEXT("int RunThisVirtual()")), Observation.Frames[1].Declaration, TEXT("Receiver debug virtual root should retain its global declaration")));
			ASSERT_THAT(AreEqual(0, Observation.Frames[1].ThisTypeId, TEXT("Receiver debug virtual global root should have no receiver type")));
			ASSERT_THAT(IsNull(Observation.Frames[1].ThisPointer, TEXT("Receiver debug virtual global root should have no receiver pointer")));
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Receiver debug virtual base-view path should unprepare before the fault path")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), PrepareAndExecute(Context, Fault), TEXT("Receiver debug null-receiver path should report the script exception")));
		ASSERT_THAT(AreEqual(0, Context->GetThisTypeId(0), TEXT("Receiver debug null-receiver global fault frame should have no receiver type")));
		ASSERT_THAT(IsNull(Context->GetThisPointer(0), TEXT("Receiver debug null-receiver global fault frame should have no receiver pointer")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Receiver debug fault path should unprepare for invalid-state checks")));
		ASSERT_THAT(AreEqual(asINVALID_ARG, Context->GetThisTypeId(0), TEXT("Receiver debug unprepared type query should return the documented invalid value")));
		ASSERT_THAT(IsNull(Context->GetThisPointer(0), TEXT("Receiver debug unprepared pointer query should be null")));
		RawContext->ClearLineCallback();
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
