#include "Support/AngelscriptNativeExecutionTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FCallingConventionTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.CallingConvention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 DoubleNativeValue(int32 Value)
	{
		return Value * 2;
	}

	static void TripleGenericValue(asIScriptGeneric* Generic)
	{
		const int32 Value = *static_cast<int32*>(Generic->GetAddressOfArg(0));
		Generic->SetReturnDWord(static_cast<asDWORD>(Value * 3));
	}

	struct FNativeAdder
	{
		int32 Base = 0;

		int32 Add(int32 Delta) const
		{
			return Base + Delta;
		}
	};

	struct FObjectLastProbe
	{
		int32 Value = 0;
		int32 Cookie = 0x13572468;
	};

	struct FObjectLastObservation
	{
		int32 ConstructorCount = 0;
		int32 CopyConstructorCount = 0;
		int32 DestructorCount = 0;
		int32 NativeCallCount = 0;
		int32 LastObjectValue = INDEX_NONE;
		int32 LastLeft = INDEX_NONE;
		int64 LastWide = MIN_int64;
		int32 LastRight = INDEX_NONE;

		void Reset()
		{
			*this = FObjectLastObservation();
		}
	};

	static void ConstructObjectLastProbe(FObjectLastProbe* Address)
	{
		new (Address) FObjectLastProbe();
		ObjectLastObservation.ConstructorCount++;
	}

	static void ConstructObjectLastProbeWithValue(
		const int32 Value,
		FObjectLastProbe* Address)
	{
		new (Address) FObjectLastProbe();
		Address->Value = Value;
		ObjectLastObservation.ConstructorCount++;
		ObjectLastObservation.LastObjectValue = Address->Value;
	}

	static void ConstructObjectLastProbeWithSentinels(
		const int32 Left,
		const int64 Wide,
		const int32 Right,
		FObjectLastProbe* Address)
	{
		new (Address) FObjectLastProbe();
		Address->Value = Left + static_cast<int32>(Wide) + Right;
		ObjectLastObservation.ConstructorCount++;
		ObjectLastObservation.LastObjectValue = Address->Value;
		ObjectLastObservation.LastLeft = Left;
		ObjectLastObservation.LastWide = Wide;
		ObjectLastObservation.LastRight = Right;
	}

	static void CopyConstructObjectLastProbe(
		const FObjectLastProbe& Other,
		FObjectLastProbe* Address)
	{
		new (Address) FObjectLastProbe(Other);
		ObjectLastObservation.CopyConstructorCount++;
		ObjectLastObservation.LastObjectValue = Other.Value;
	}

	static void DestructObjectLastProbe(FObjectLastProbe* Address)
	{
		ObjectLastObservation.DestructorCount++;
		Address->~FObjectLastProbe();
	}

	static int32 FoldObjectLastSentinels(
		const int32 Left,
		const int64 Wide,
		const int32 Right,
		const FObjectLastProbe* Object)
	{
		ObjectLastObservation.NativeCallCount++;
		ObjectLastObservation.LastObjectValue = Object != nullptr ? Object->Value : INDEX_NONE;
		ObjectLastObservation.LastLeft = Left;
		ObjectLastObservation.LastWide = Wide;
		ObjectLastObservation.LastRight = Right;
		return ObjectLastObservation.LastObjectValue
			+ Left
			+ static_cast<int32>(Wide)
			+ Right;
	}

	static FObjectLastProbe ShiftObjectLastProbe(
		const int32 Delta,
		const FObjectLastProbe* Object)
	{
		ObjectLastObservation.NativeCallCount++;
		ObjectLastObservation.LastObjectValue = Object != nullptr ? Object->Value : INDEX_NONE;
		ObjectLastObservation.LastLeft = Delta;
		// The automatic caller placement-constructs this by-value result directly
		// in the VM return slot, so account for the matching registered destructor.
		ObjectLastObservation.ConstructorCount++;
		FObjectLastProbe Result;
		Result.Value = ObjectLastObservation.LastObjectValue + Delta;
		Result.Cookie = Object != nullptr ? Object->Cookie : 0;
		return Result;
	}

	static int32 RaiseObjectLastException(
		const int32 Marker,
		const FObjectLastProbe* Object)
	{
		ObjectLastObservation.NativeCallCount++;
		ObjectLastObservation.LastLeft = Marker;
		ObjectLastObservation.LastObjectValue = Object != nullptr ? Object->Value : INDEX_NONE;
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("object-last native sentinel exception");
		}
		return -1;
	}

	static void ConstructNativeAdder(FNativeAdder* Address)
	{
		new (Address) FNativeAdder();
	}

	static void ConstructNativeAdderWithBase(
		const int32 Base,
		FNativeAdder* Address)
	{
		new (Address) FNativeAdder();
		Address->Base = Base;
		LastObjectLastConstructorBase = Base;
	}

	static void ReportNativeSource(
		FAutomationTestBase& Test,
		const TCHAR* SourceId,
		const ANSICHAR* ModuleName,
		const std::string& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			SourceId,
			UTF8_TO_TCHAR(ModuleName),
			UTF8_TO_TCHAR(Source.c_str()));
	}

	void AssertIndependentRegistrationAbsence(
		const ANSICHAR* TypeDeclaration)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine IndependentEngine;
		IndependentEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IndependentEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			IndependentEngine.Get(),
			TEXT("SDK calling-convention test should create an independent engine")));
		if (IndependentEngine.Get() == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<asUINT>(1),
			IndependentEngine.Get()->GetGlobalFunctionCount(),
			TEXT("Independent calling-convention engine should retain only the support assert(bool) registration")));
		asIScriptFunction* const SupportFunction =
			IndependentEngine.Get()->GetGlobalFunctionByIndex(0);
		ASSERT_THAT(IsNotNull(
			SupportFunction,
			TEXT("Independent calling-convention engine should publish its sole support registration")));
		if (SupportFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("assert")),
				FString(UTF8_TO_TCHAR(SupportFunction->GetName())),
				TEXT("Independent calling-convention engine should not contain a product global registration")));
		}
		if (TypeDeclaration != nullptr)
		{
			ASSERT_THAT(IsNull(
				IndependentEngine.Get()->GetTypeInfoByDecl(TypeDeclaration),
				TEXT("SDK calling-convention object registration should remain engine-local")));
		}
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static int32 CDeclFunctionId = asNO_FUNCTION;
	inline static int32 GenericFunctionId = asNO_FUNCTION;
	inline static int32 NativeAdderConstructorId = asNO_FUNCTION;
	inline static int32 NativeAdderMethodId = asNO_FUNCTION;
	inline static bool bNativeAdderRegistered = false;
	inline static int32 LastObjectLastConstructorBase = INDEX_NONE;
	inline static FObjectLastObservation ObjectLastObservation;
	inline static bool bObjectLastProbeRegistered = false;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(DoubleNativeValue);
		const int RegisterResult = ScriptEngine->RegisterGlobalFunction(
			"int DoubleNativeValue(int Value)",
			asFUNCTION(DoubleNativeValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&Caller);
		CDeclFunctionId = RegisterResult;
		if (CDeclFunctionId < 0)
		{
			TestRunner->AddError(TEXT("SDK calling-convention CDecl test should register the native function"));
		}

		const int GenericResult = ScriptEngine->RegisterGlobalFunction(
			"int TripleGenericValue(int Value)",
			asFUNCTION(TripleGenericValue),
			asCALL_GENERIC);
		GenericFunctionId = GenericResult;
		if (GenericFunctionId < 0)
		{
			TestRunner->AddError(TEXT("SDK calling-convention generic test should register the generic function"));
		}

		const int TypeResult = ScriptEngine->RegisterObjectType(
			"NativeAdder",
			sizeof(FNativeAdder),
			asOBJ_VALUE
				| asOBJ_POD
				| asGetTypeTraits<FNativeAdder>()
				| asOBJ_APP_CLASS_ALLINTS);
		const ASAutoCaller::FunctionCaller DefaultConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructNativeAdder);
		const ASAutoCaller::FunctionCaller ValueConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructNativeAdderWithBase);
		const ASAutoCaller::FunctionCaller MethodCaller =
			ASAutoCaller::MakeFunctionCaller(&FNativeAdder::Add);
		const int DefaultConstructorResult =
			ScriptEngine->RegisterObjectBehaviour(
				"NativeAdder",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ConstructNativeAdder),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&DefaultConstructorCaller);
		const int ValueConstructorResult =
			ScriptEngine->RegisterObjectBehaviour(
				"NativeAdder",
				asBEHAVE_CONSTRUCT,
				"void f(int Base)",
				asFUNCTION(ConstructNativeAdderWithBase),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ValueConstructorCaller);
		const int PropertyResult = ScriptEngine->RegisterObjectProperty(
			"NativeAdder",
			"int Base",
			asOFFSET(FNativeAdder, Base));
		const int MethodResult = ScriptEngine->RegisterObjectMethod(
			"NativeAdder",
			"int Add(int Delta) const",
			asMETHODPR(FNativeAdder, Add, (int32) const, int32),
			asCALL_THISCALL,
			*(asFunctionCaller*)&MethodCaller);
		NativeAdderConstructorId = ValueConstructorResult;
		NativeAdderMethodId = MethodResult;
		bNativeAdderRegistered = TypeResult >= 0
			&& DefaultConstructorResult >= 0
			&& ValueConstructorResult >= 0
			&& PropertyResult >= 0
			&& MethodResult >= 0;
		if (!bNativeAdderRegistered)
		{
			TestRunner->AddError(TEXT("SDK calling-convention thiscall test should register the value type and method"));
		}

		const ASAutoCaller::FunctionCaller ProbeDefaultConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructObjectLastProbe);
		const ASAutoCaller::FunctionCaller ProbeValueConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructObjectLastProbeWithValue);
		const ASAutoCaller::FunctionCaller ProbeSentinelConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructObjectLastProbeWithSentinels);
		const ASAutoCaller::FunctionCaller ProbeCopyConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(CopyConstructObjectLastProbe);
		const ASAutoCaller::FunctionCaller ProbeDestructorCaller =
			ASAutoCaller::MakeFunctionCaller(DestructObjectLastProbe);
		const ASAutoCaller::FunctionCaller ProbeFoldCaller =
			ASAutoCaller::MakeFunctionCaller(FoldObjectLastSentinels);
		const ASAutoCaller::FunctionCaller ProbeShiftCaller =
			ASAutoCaller::MakeFunctionCaller(ShiftObjectLastProbe);
		const ASAutoCaller::FunctionCaller ProbeExceptionCaller =
			ASAutoCaller::MakeFunctionCaller(RaiseObjectLastException);
		bObjectLastProbeRegistered =
			ScriptEngine->RegisterObjectType(
				"ObjectLastProbe",
				sizeof(FObjectLastProbe),
				asOBJ_VALUE | asGetTypeTraits<FObjectLastProbe>() | asOBJ_APP_CLASS_ALLINTS) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"ObjectLastProbe",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ConstructObjectLastProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeDefaultConstructorCaller) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"ObjectLastProbe",
				asBEHAVE_CONSTRUCT,
				"void f(int Value)",
				asFUNCTION(ConstructObjectLastProbeWithValue),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeValueConstructorCaller) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"ObjectLastProbe",
				asBEHAVE_CONSTRUCT,
				"void f(int Left, int64 Wide, int Right)",
				asFUNCTION(ConstructObjectLastProbeWithSentinels),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeSentinelConstructorCaller) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"ObjectLastProbe",
				asBEHAVE_CONSTRUCT,
				"void f(const ObjectLastProbe& in Other)",
				asFUNCTION(CopyConstructObjectLastProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeCopyConstructorCaller) >= 0
			&& ScriptEngine->RegisterObjectBehaviour(
				"ObjectLastProbe",
				asBEHAVE_DESTRUCT,
				"void f()",
				asFUNCTION(DestructObjectLastProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeDestructorCaller) >= 0
			&& ScriptEngine->RegisterObjectProperty(
				"ObjectLastProbe",
				"int Value",
				asOFFSET(FObjectLastProbe, Value)) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"ObjectLastProbe",
				"int Fold(int Left, int64 Wide, int Right) const",
				asFUNCTION(FoldObjectLastSentinels),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeFoldCaller) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"ObjectLastProbe",
				"ObjectLastProbe Shifted(int Delta) const",
				asFUNCTION(ShiftObjectLastProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeShiftCaller) >= 0
			&& ScriptEngine->RegisterObjectMethod(
				"ObjectLastProbe",
				"int Raise(int Marker) const",
				asFUNCTION(RaiseObjectLastException),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ProbeExceptionCaller) >= 0;
		if (!bObjectLastProbeRegistered)
		{
			TestRunner->AddError(TEXT("SDK calling-convention object-last depth fixture should register every supported native shape"));
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		CDeclFunctionId = asNO_FUNCTION;
		GenericFunctionId = asNO_FUNCTION;
		NativeAdderConstructorId = asNO_FUNCTION;
		NativeAdderMethodId = asNO_FUNCTION;
		bNativeAdderRegistered = false;
		LastObjectLastConstructorBase = INDEX_NONE;
		ObjectLastObservation.Reset();
		bObjectLastProbeRegistered = false;
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
		LastObjectLastConstructorBase = INDEX_NONE;
		ObjectLastObservation.Reset();
	}

	TEST_METHOD(CallingConventionCDecl)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("EMBED-CALLING-CONVENTION-DISPATCH",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("SDK calling-convention CDecl test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			CDeclFunctionId >= 0,
			TEXT("SDK calling-convention CDecl test should retain its immutable registration surface")));
		if (CDeclFunctionId < 0)
		{
			return;
		}
		asIScriptFunction* const RegisteredFunction =
			ScriptEngine->GetFunctionById(CDeclFunctionId);
		ASSERT_THAT(IsNotNull(
			RegisteredFunction,
			TEXT("SDK calling-convention CDecl test should resolve the returned function ID")));
		if (RegisteredFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("int DoubleNativeValue(int)")),
				FString(UTF8_TO_TCHAR(RegisteredFunction->GetDeclaration())),
				TEXT("SDK calling-convention CDecl test should preserve the exact registered declaration")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				RegisteredFunction->GetParamCount(),
				TEXT("SDK calling-convention CDecl test should preserve its single integer parameter")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTYPEID_INT32),
				RegisteredFunction->GetReturnTypeId(),
				TEXT("SDK calling-convention CDecl test should preserve its integer return metadata")));
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return DoubleNativeValue(21);
			}
			)AS");
		ReportNativeSource(
			*TestRunner,
			TEXT("EMBED-CALLING-CONVENTION-DISPATCH-CDECL"),
			"SDKCallingConvCDecl",
			Source);
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKCallingConvCDecl",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			42,
			Result,
			TEXT("SDK calling-convention CDecl test should preserve native CDecl calls")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("SDK calling-convention CDecl test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("SDKCallingConvCDecl", asGM_ONLY_IF_EXISTS),
			TEXT("SDK calling-convention CDecl module should be absent after discard")));

		AssertIndependentRegistrationAbsence(nullptr);
	}

	TEST_METHOD(CallingConventionGeneric)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-CALLING-CONVENTION-DISPATCH",
			"generic");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("SDK calling-convention generic test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			GenericFunctionId >= 0,
			TEXT("SDK calling-convention generic test should retain its immutable registration surface")));
		if (GenericFunctionId < 0)
		{
			return;
		}
		asIScriptFunction* const RegisteredFunction =
			ScriptEngine->GetFunctionById(GenericFunctionId);
		ASSERT_THAT(IsNotNull(
			RegisteredFunction,
			TEXT("SDK calling-convention generic test should resolve the returned function ID")));
		if (RegisteredFunction != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("int TripleGenericValue(int)")),
				FString(UTF8_TO_TCHAR(RegisteredFunction->GetDeclaration())),
				TEXT("SDK calling-convention generic test should preserve the exact registered declaration")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				RegisteredFunction->GetParamCount(),
				TEXT("SDK calling-convention generic test should preserve its single integer parameter")));
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				return TripleGenericValue(14);
			}
			)AS");
		ReportNativeSource(
			*TestRunner,
			TEXT("EMBED-CALLING-CONVENTION-DISPATCH-GENERIC"),
			"SDKCallingConvGeneric",
			Source);
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKCallingConvGeneric",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			42,
			Result,
			TEXT("SDK calling-convention generic test should preserve generic callback execution")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("SDK calling-convention generic test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("SDKCallingConvGeneric", asGM_ONLY_IF_EXISTS),
			TEXT("SDK calling-convention generic module should be absent after discard")));
		AssertIndependentRegistrationAbsence(nullptr);
	}

	TEST_METHOD(CallingConventionThiscall)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-CALLING-CONVENTION-DISPATCH",
			"thiscall");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("SDK calling-convention thiscall test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			bNativeAdderRegistered,
			TEXT("SDK calling-convention thiscall test should retain its immutable registration surface")));
		if (!bNativeAdderRegistered)
		{
			return;
		}
		asIScriptFunction* const RegisteredMethod =
			ScriptEngine->GetFunctionById(NativeAdderMethodId);
		ASSERT_THAT(IsNotNull(
			RegisteredMethod,
			TEXT("SDK calling-convention thiscall test should resolve the returned method ID")));
		if (RegisteredMethod != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("int Add(int) const")),
				FString(UTF8_TO_TCHAR(RegisteredMethod->GetDeclaration(false, false, false))),
				TEXT("SDK calling-convention thiscall test should preserve the exact method declaration")));
			asITypeInfo* const ObjectType = RegisteredMethod->GetObjectType();
			ASSERT_THAT(IsNotNull(
				ObjectType,
				TEXT("SDK calling-convention thiscall method should retain its object owner")));
			if (ObjectType != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("NativeAdder")),
					FString(UTF8_TO_TCHAR(ObjectType->GetName())),
					TEXT("SDK calling-convention thiscall method should retain its object owner name")));
			}
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				NativeAdder Value;
				Value.Base = 39;
				return Value.Add(3);
			}
			)AS");
		ReportNativeSource(
			*TestRunner,
			TEXT("EMBED-CALLING-CONVENTION-DISPATCH-THISCALL"),
			"SDKCallingConvThiscall",
			Source);
		AngelscriptNativeTestSupport::FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKCallingConvThiscall",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(
			*TestRunner,
			ScriptEngine,
			Module,
			"int Entry()",
			Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			42,
			Result,
			TEXT("SDK calling-convention thiscall test should execute the registered method")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("SDK calling-convention thiscall test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("SDKCallingConvThiscall", asGM_ONLY_IF_EXISTS),
			TEXT("SDK calling-convention thiscall module should be absent after discard")));
		AssertIndependentRegistrationAbsence("NativeAdder");
	}

	TEST_METHOD(CallingConventionCDeclObjectLast)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-CALLING-CONVENTION-DISPATCH",
			"cdecl_object_last");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("SDK calling-convention object-last test should create a standalone engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			bNativeAdderRegistered,
			TEXT("SDK calling-convention object-last test should retain its immutable registration surface")));
		if (!bNativeAdderRegistered)
		{
			return;
		}
		asIScriptFunction* const RegisteredConstructor =
			ScriptEngine->GetFunctionById(NativeAdderConstructorId);
		ASSERT_THAT(IsNotNull(
			RegisteredConstructor,
			TEXT("SDK calling-convention object-last test should resolve the returned constructor ID")));
		if (RegisteredConstructor != nullptr)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("NativeAdder(int)")),
				FString(UTF8_TO_TCHAR(RegisteredConstructor->GetDeclaration(false, false, false))),
				TEXT("SDK calling-convention object-last constructor should publish its exact canonical behavior declaration")));
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(1),
				RegisteredConstructor->GetParamCount(),
				TEXT("SDK calling-convention object-last constructor should retain its integer parameter metadata")));
			asITypeInfo* const ObjectType = RegisteredConstructor->GetObjectType();
			ASSERT_THAT(IsNotNull(
				ObjectType,
				TEXT("SDK calling-convention object-last constructor should retain its object owner")));
			if (ObjectType != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("NativeAdder")),
					FString(UTF8_TO_TCHAR(ObjectType->GetName())),
					TEXT("SDK calling-convention object-last constructor should retain its object owner name")));
			}
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int Entry()
			{
				NativeAdder Value(39);
				return Value.Add(3);
			}
			)AS");
		ReportNativeSource(
			*TestRunner,
			TEXT("EMBED-CALLING-CONVENTION-DISPATCH-CDECL-OBJECT-LAST"),
			"SDKCallingConvCDeclObjectLast",
			Source);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKCallingConvCDeclObjectLast",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(
			*TestRunner,
			ScriptEngine,
			Module,
			"int Entry()",
			Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			39,
			LastObjectLastConstructorBase,
			TEXT("SDK calling-convention object-last constructor should receive its declared integer argument")));
		ASSERT_THAT(AreEqual(
			42,
			Result,
			TEXT("SDK calling-convention object-last constructor should initialize its destination before thiscall dispatch")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("SDK calling-convention object-last test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("SDKCallingConvCDeclObjectLast", asGM_ONLY_IF_EXISTS),
			TEXT("SDK calling-convention object-last module should be absent after discard")));
		AssertIndependentRegistrationAbsence("NativeAdder");
	}

	TEST_METHOD(ObjectLastSupportedShapesPreserveSentinelsAndReturnStorage)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-CALLING-CONVENTION-DISPATCH",
			"cdecl_object_last_supported_shapes");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Object-last supported-shape test should create a standalone engine")));
		ASSERT_THAT(IsTrue(
			bObjectLastProbeRegistered,
			TEXT("Object-last supported-shape test should retain its immutable registration surface")));
		if (ScriptEngine == nullptr || !bObjectLastProbeRegistered)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int DefaultConstructorEntry()
			{
				ObjectLastProbe Value;
				return Value.Value;
			}

			int ScalarConstructorEntry()
			{
				ObjectLastProbe Value(41);
				return Value.Value;
			}

			int MultiArgumentConstructorEntry()
			{
				ObjectLastProbe Value(11, int64(1000000007), 23);
				return Value.Value;
			}

			int ExternalMethodEntry()
			{
				ObjectLastProbe Value(31);
				return Value.Fold(7, int64(1000000009), 13);
			}

			int ReturnStorageEntry()
			{
				ObjectLastProbe Value(37);
				ObjectLastProbe Shifted = Value.Shifted(5);
				return Shifted.Value;
			}

			int ValueCopyEntry()
			{
				ObjectLastProbe Original(43);
				ObjectLastProbe Copy(Original);
				return Copy.Value;
			}
			)AS");
		ReportNativeSource(
			*TestRunner,
			TEXT("EMBED-CALLING-CONVENTION-DISPATCH-CDECL-OBJECT-LAST-SHAPES"),
			"SDKCallingConvObjectLastShapes",
			Source);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKCallingConvObjectLastShapes",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		struct FSupportedShapeCase
		{
			const char* Declaration;
			int32 ExpectedResult;
			int32 ExpectedConstructorCount;
			int32 ExpectedCopyConstructorCount;
			int32 ExpectedDestructorCount;
			int32 ExpectedNativeCallCount;
			int32 ExpectedLastObjectValue;
			int32 ExpectedLastLeft;
			int64 ExpectedLastWide;
			int32 ExpectedLastRight;
		};
		const FSupportedShapeCase Cases[] =
		{
			{"int DefaultConstructorEntry()", 0, 1, 0, 1, 0, INDEX_NONE, INDEX_NONE, MIN_int64, INDEX_NONE},
			{"int ScalarConstructorEntry()", 41, 1, 0, 1, 0, 41, INDEX_NONE, MIN_int64, INDEX_NONE},
			{"int MultiArgumentConstructorEntry()", 1000000041, 1, 0, 1, 0, 1000000041, 11, 1000000007, 23},
			{"int ExternalMethodEntry()", 1000000060, 1, 0, 1, 1, 31, 7, 1000000009, 13},
			{"int ReturnStorageEntry()", 42, 2, 0, 2, 1, 37, 5, MIN_int64, INDEX_NONE},
			{"int ValueCopyEntry()", 43, 1, 1, 2, 0, 43, INDEX_NONE, MIN_int64, INDEX_NONE},
		};

		for (const FSupportedShapeCase& Case : Cases)
		{
			ObjectLastObservation.Reset();
			int32 Result = 0;
			ASSERT_THAT(IsTrue(
				ExecuteScriptFunction(
					*TestRunner,
					ScriptEngine,
					Module,
					Case.Declaration,
					Result),
				TEXT("Object-last supported shape should execute through its exact declaration")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedResult,
				Result,
				TEXT("Object-last supported shape should preserve its exact sentinel result")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedConstructorCount,
				ObjectLastObservation.ConstructorCount,
				TEXT("Object-last supported shape should invoke its exact constructor count")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedCopyConstructorCount,
				ObjectLastObservation.CopyConstructorCount,
				TEXT("Object-last supported shape should invoke its exact copy-constructor count")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedDestructorCount,
				ObjectLastObservation.DestructorCount,
				TEXT("Object-last supported shape should invoke its exact destructor count")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedNativeCallCount,
				ObjectLastObservation.NativeCallCount,
				TEXT("Object-last supported shape should invoke its exact native-method count")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedLastObjectValue,
				ObjectLastObservation.LastObjectValue,
				TEXT("Object-last supported shape should preserve its exact object value")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedLastLeft,
				ObjectLastObservation.LastLeft,
				TEXT("Object-last supported shape should preserve its exact first explicit argument")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedLastWide,
				ObjectLastObservation.LastWide,
				TEXT("Object-last supported shape should preserve its exact pointer-width-crossing argument")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedLastRight,
				ObjectLastObservation.LastRight,
				TEXT("Object-last supported shape should preserve its exact final explicit argument")));
			ASSERT_THAT(AreEqual(
				ObjectLastObservation.ConstructorCount + ObjectLastObservation.CopyConstructorCount,
				ObjectLastObservation.DestructorCount,
				TEXT("Object-last supported shape should destroy every constructed value exactly once")));
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Object-last supported-shape test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("SDKCallingConvObjectLastShapes", asGM_ONLY_IF_EXISTS),
			TEXT("Object-last supported-shape module should be absent after discard")));
		AssertIndependentRegistrationAbsence("ObjectLastProbe");
	}

	TEST_METHOD(ObjectLastNativeExceptionCleansAndContextCanBeReused)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-CALLING-CONVENTION-DISPATCH",
			"cdecl_object_last_exception_recovery");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Object-last exception test should create a standalone engine")));
		ASSERT_THAT(IsTrue(
			bObjectLastProbeRegistered,
			TEXT("Object-last exception test should retain its immutable registration surface")));
		if (ScriptEngine == nullptr || !bObjectLastProbeRegistered)
		{
			return;
		}

		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int FailingEntry()
			{
				ObjectLastProbe Value(59);
				return Value.Raise(97);
			}

			int RecoveryEntry()
			{
				ObjectLastProbe Value(71);
				return Value.Fold(3, int64(5), 7);
			}
			)AS");
		ReportNativeSource(
			*TestRunner,
			TEXT("EMBED-CALLING-CONVENTION-DISPATCH-CDECL-OBJECT-LAST-EXCEPTION"),
			"SDKCallingConvObjectLastException",
			Source);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKCallingConvObjectLastException",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const FailingFunction =
			GetNativeFunctionByExactDecl(Module.Get(), "int FailingEntry()");
		asIScriptFunction* const RecoveryFunction =
			GetNativeFunctionByExactDecl(Module.Get(), "int RecoveryEntry()");
		ASSERT_THAT(IsNotNull(
			FailingFunction,
			TEXT("Object-last exception test should resolve its failing entry")));
		ASSERT_THAT(IsNotNull(
			RecoveryFunction,
			TEXT("Object-last exception test should resolve its recovery entry")));
		if (FailingFunction == nullptr || RecoveryFunction == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(
			Context,
			TEXT("Object-last exception test should create one reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			PrepareAndExecute(Context, FailingFunction),
			TEXT("Object-last native exception should terminate the first execution")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("object-last native sentinel exception")),
			FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
			TEXT("Object-last native exception should preserve its stable diagnostic")));
		ASSERT_THAT(AreEqual(
			97,
			ObjectLastObservation.LastLeft,
			TEXT("Object-last native exception should receive the exact explicit marker")));
		ASSERT_THAT(AreEqual(
			59,
			ObjectLastObservation.LastObjectValue,
			TEXT("Object-last native exception should receive the exact object last")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Unprepare(),
			TEXT("Object-last native exception should explicitly retire its exceptional frame before recovery")));
		ASSERT_THAT(AreEqual(
			ObjectLastObservation.ConstructorCount,
			ObjectLastObservation.DestructorCount,
			TEXT("Object-last exceptional-frame retirement should destroy every constructed value exactly once")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Prepare(RecoveryFunction),
			TEXT("Object-last exception context should prepare a second function")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			TEXT("Object-last exception context should remain reusable")));
		ASSERT_THAT(AreEqual(
			86,
			static_cast<int32>(Context->GetReturnDWord()),
			TEXT("Object-last recovery execution should preserve object and explicit arguments")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Unprepare(),
			TEXT("Object-last recovery context should unprepare before module cleanup")));
		ASSERT_THAT(AreEqual(
			ObjectLastObservation.ConstructorCount,
			ObjectLastObservation.DestructorCount,
			TEXT("Object-last recovery should leave no live values")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Object-last exception test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("SDKCallingConvObjectLastException", asGM_ONLY_IF_EXISTS),
			TEXT("Object-last exception module should be absent after cleanup")));
		AssertIndependentRegistrationAbsence("ObjectLastProbe");
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
