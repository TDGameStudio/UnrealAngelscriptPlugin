#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FGenericInterfaceDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.Embedding.GenericInterfaceDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 CurrentForkGlobalObjectTypeId = -1;

	struct FGenericProbe
	{
		int32 Value = 0;
	};

	struct FAuxiliaryToken
	{
		int32 Marker = 0;
	};

	struct FImplicitNoCountProbe
	{
		int32 Value = 83;
	};

	struct FImplicitCountedProbe
	{
		int32 Value = 97;
		int32 ReferenceCount = 1;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
	};

	struct FGenericObservation
	{
		asIScriptEngine* Engine = nullptr;
		asIScriptFunction* Function = nullptr;
		void* Auxiliary = nullptr;
		void* Object = nullptr;
		int32 ObjectTypeId = asTYPEID_VOID;
		int32 ArgumentCount = INDEX_NONE;
		int32 ReturnTypeId = asTYPEID_VOID;
		asDWORD ReturnFlags = 0;
		int32 SetReturnResult = asERROR;
		int32 CallbackCount = 0;
	};

	struct FIntrospectionCase
	{
		const TCHAR* TargetId;
		const TCHAR* ArityId;
		const TCHAR* AuxiliaryId;
		bool bObjectMethod;
		bool bProvideAuxiliary;
		int32 Arity;
	};

	static void ConstructGenericProbe(FGenericProbe* Address)
	{
		new (Address) FGenericProbe();
	}

	static void ResetObservation()
	{
		Observation = FGenericObservation();
	}

	static int32 ReadArgumentSum(asIScriptGeneric& Generic)
	{
		int32 Result = 0;
		for (int32 ArgumentIndex = 0; ArgumentIndex < Generic.GetArgCount(); ++ArgumentIndex)
		{
			Result += static_cast<int32>(Generic.GetArgDWord(static_cast<asUINT>(ArgumentIndex)));
		}
		return Result;
	}

	static void CaptureCommonObservation(asIScriptGeneric& Generic)
	{
		Observation.Engine = Generic.GetEngine();
		Observation.Function = Generic.GetFunction();
		Observation.Auxiliary = Generic.GetAuxiliary();
		Observation.Object = Generic.GetObject();
		Observation.ObjectTypeId = Generic.GetObjectTypeId();
		Observation.ArgumentCount = Generic.GetArgCount();
		Observation.ReturnTypeId = Generic.GetReturnTypeId(&Observation.ReturnFlags);
		++Observation.CallbackCount;
	}

	static void IntrospectAndReturnInt(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		CaptureCommonObservation(*Generic);
		const FGenericProbe* const Object =
			static_cast<const FGenericProbe*>(Generic->GetObject());
		const int32 Result = ReadArgumentSum(*Generic)
			+ (Object != nullptr ? Object->Value : 0);
		Observation.SetReturnResult =
			Generic->SetReturnDWord(static_cast<asDWORD>(Result));
	}

	static void ReturnObjectByValue(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		CaptureCommonObservation(*Generic);
		Observation.SetReturnResult =
			Generic->SetReturnObject(Generic->GetArgObject(0));
	}

	static void ReadImplicitNoCountByValue(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		CaptureCommonObservation(*Generic);
		const FImplicitNoCountProbe* const Probe =
			static_cast<const FImplicitNoCountProbe*>(Generic->GetArgObject(0));
		Observation.SetReturnResult = Generic->SetReturnDWord(
			static_cast<asDWORD>(Probe != nullptr ? Probe->Value : INDEX_NONE));
	}

	static void AddRefImplicitCounted(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FImplicitCountedProbe* const Probe =
			static_cast<FImplicitCountedProbe*>(Generic->GetObject());
		if (Probe != nullptr)
		{
			++Probe->ReferenceCount;
			++Probe->AddRefCalls;
		}
	}

	static void ReleaseImplicitCounted(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FImplicitCountedProbe* const Probe =
			static_cast<FImplicitCountedProbe*>(Generic->GetObject());
		if (Probe != nullptr)
		{
			--Probe->ReferenceCount;
			++Probe->ReleaseCalls;
		}
	}

	static void ReadImplicitCountedByValue(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		CaptureCommonObservation(*Generic);
		const FImplicitCountedProbe* const Probe =
			static_cast<const FImplicitCountedProbe*>(Generic->GetArgObject(0));
		Observation.SetReturnResult = Generic->SetReturnDWord(
			static_cast<asDWORD>(Probe != nullptr ? Probe->Value : INDEX_NONE));
	}

	static FString BuildArguments(const int32 Arity)
	{
		switch (Arity)
		{
		case 0:
			return FString();
		case 1:
			return TEXT("10");
		case 2:
			return TEXT("10, 20");
		default:
			return FString();
		}
	}

	static FString BuildIntrospectionSource(
		const FIntrospectionCase& Case,
		const FString& FunctionName)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (Case.bObjectMethod)
		{
			AppendGeneratedAsLine(Source, TEXT("\tGenericProbe Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value = 5;"));
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\treturn Value.%s(%s);"),
					*FunctionName,
					*BuildArguments(Case.Arity)));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\treturn %s(%s);"),
					*FunctionName,
					*BuildArguments(Case.Arity)));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildObjectReturnSource(const FString& FunctionName)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tGenericProbe Input;"));
		AppendGeneratedAsLine(Source, TEXT("\tInput.Value = 42;"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("\tGenericProbe Output = %s(Input);"),
				*FunctionName));
		AppendGeneratedAsLine(Source, TEXT("\treturn Output.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString MakeIntrospectionFunctionName(
		const FIntrospectionCase& Case)
	{
		return FString::Printf(
			TEXT("Inspect_%s_%s_%s"),
			Case.bObjectMethod ? TEXT("Method") : TEXT("Global"),
			Case.ArityId,
			Case.AuxiliaryId);
	}

	static FString MakeParameterDeclaration(const int32 Arity)
	{
		switch (Arity)
		{
		case 0:
			return FString();
		case 1:
			return TEXT("int First");
		case 2:
			return TEXT("int First, int Second");
		default:
			return FString();
		}
	}

	static bool RegisterGenericContracts(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine)
	{
		FNoDiscardAsserter Assert(Test);
		const ASAutoCaller::FunctionCaller ConstructorCaller =
			ASAutoCaller::MakeFunctionCaller(ConstructGenericProbe);
		bool bSuccess = true;
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"GenericProbe",
				sizeof(FGenericProbe),
				asOBJ_VALUE
					| asOBJ_POD
					| asGetTypeTraits<FGenericProbe>()
					| asOBJ_APP_CLASS_ALLINTS) >= 0,
			TEXT("Generic interface product should register its value type"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				"GenericProbe",
				asBEHAVE_CONSTRUCT,
				"void f()",
				asFUNCTION(ConstructGenericProbe),
				asCALL_CDECL_OBJLAST,
				*(asFunctionCaller*)&ConstructorCaller) >= 0,
			TEXT("Generic interface product should register its value constructor"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectProperty(
				"GenericProbe",
				"int Value",
				asOFFSET(FGenericProbe, Value)) >= 0,
			TEXT("Generic interface product should register its observable property"));

		const FIntrospectionCase Cases[] =
		{
			{ TEXT("global"), TEXT("zero"), TEXT("absent"), false, false, 0 },
			{ TEXT("global"), TEXT("zero"), TEXT("provided"), false, true, 0 },
			{ TEXT("global"), TEXT("one"), TEXT("absent"), false, false, 1 },
			{ TEXT("global"), TEXT("one"), TEXT("provided"), false, true, 1 },
			{ TEXT("global"), TEXT("two"), TEXT("absent"), false, false, 2 },
			{ TEXT("global"), TEXT("two"), TEXT("provided"), false, true, 2 },
			{ TEXT("object_method"), TEXT("zero"), TEXT("absent"), true, false, 0 },
			{ TEXT("object_method"), TEXT("zero"), TEXT("provided"), true, true, 0 },
			{ TEXT("object_method"), TEXT("one"), TEXT("absent"), true, false, 1 },
			{ TEXT("object_method"), TEXT("one"), TEXT("provided"), true, true, 1 },
			{ TEXT("object_method"), TEXT("two"), TEXT("absent"), true, false, 2 },
			{ TEXT("object_method"), TEXT("two"), TEXT("provided"), true, true, 2 },
		};

		for (const FIntrospectionCase& Case : Cases)
		{
			const FString FunctionName = MakeIntrospectionFunctionName(Case);
			const FString Declaration = FString::Printf(
				TEXT("int %s(%s)"),
				*FunctionName,
				*MakeParameterDeclaration(Case.Arity));
			const FTCHARToUTF8 DeclarationUtf8(*Declaration);
			void* const Auxiliary =
				Case.bProvideAuxiliary ? static_cast<void*>(&AuxiliaryToken) : nullptr;
			const int32 RegisterResult = Case.bObjectMethod
				? ScriptEngine.RegisterObjectMethod(
					"GenericProbe",
					DeclarationUtf8.Get(),
					asFUNCTION(IntrospectAndReturnInt),
					asCALL_GENERIC,
					nullptr,
					Auxiliary)
				: ScriptEngine.RegisterGlobalFunction(
					DeclarationUtf8.Get(),
					asFUNCTION(IntrospectAndReturnInt),
					asCALL_GENERIC,
					nullptr,
					Auxiliary);
			bSuccess &= Assert.IsTrue(
				RegisterResult >= 0,
				*FString::Printf(
					TEXT("Generic interface product should register %s"),
					*Declaration));
		}

		for (const bool bProvideAuxiliary : { false, true })
		{
			const FString FunctionName = bProvideAuxiliary
				? TEXT("EchoGenericProbe_Provided")
				: TEXT("EchoGenericProbe_Absent");
			const FString Declaration = FString::Printf(
				TEXT("GenericProbe %s(GenericProbe Input)"),
				*FunctionName);
			const FTCHARToUTF8 DeclarationUtf8(*Declaration);
			const int32 RegisterResult = ScriptEngine.RegisterGlobalFunction(
				DeclarationUtf8.Get(),
				asFUNCTION(ReturnObjectByValue),
				asCALL_GENERIC,
				nullptr,
				bProvideAuxiliary ? static_cast<void*>(&AuxiliaryToken) : nullptr);
			bSuccess &= Assert.IsTrue(
				RegisterResult >= 0,
				*FString::Printf(
					TEXT("Generic object-return product should register %s"),
					*Declaration));
		}

		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"ImplicitNoCountProbe",
				0,
				asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("Generic no-count implicit-handle product should register its reference type"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterGlobalProperty(
				"ImplicitNoCountProbe ImplicitNoCountValue",
				&ImplicitNoCountProbePointer) >= 0,
			TEXT("Generic no-count implicit-handle product should register its global value"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterGlobalFunction(
				"int ReadImplicitNoCountByValue(ImplicitNoCountProbe Value)",
				asFUNCTION(ReadImplicitNoCountByValue),
				asCALL_GENERIC) >= 0,
			TEXT("Generic no-count implicit-handle product should register its by-value callback"));

		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"ImplicitCountedProbe",
				0,
				asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0,
			TEXT("Generic counted implicit-handle control should register its reference type"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				"ImplicitCountedProbe",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(AddRefImplicitCounted),
				asCALL_GENERIC) >= 0,
			TEXT("Generic counted implicit-handle control should register addref"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectBehaviour(
				"ImplicitCountedProbe",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ReleaseImplicitCounted),
				asCALL_GENERIC) >= 0,
			TEXT("Generic counted implicit-handle control should register release"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterGlobalProperty(
				"ImplicitCountedProbe ImplicitCountedValue",
				&ImplicitCountedProbePointer) >= 0,
			TEXT("Generic counted implicit-handle control should register its global value"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterGlobalFunction(
				"int ReadImplicitCountedByValue(ImplicitCountedProbe Value)",
				asFUNCTION(ReadImplicitCountedByValue),
				asCALL_GENERIC) >= 0,
			TEXT("Generic counted implicit-handle control should register its by-value callback"));

		return bSuccess;
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static FGenericObservation Observation;
	inline static FAuxiliaryToken AuxiliaryToken{ 0x2A };
	inline static FImplicitNoCountProbe ImplicitNoCountProbe;
	inline static FImplicitNoCountProbe* ImplicitNoCountProbePointer =
		&ImplicitNoCountProbe;
	inline static FImplicitCountedProbe ImplicitCountedProbe;
	inline static FImplicitCountedProbe* ImplicitCountedProbePointer =
		&ImplicitCountedProbe;
	inline static bool bContractsRegistered = false;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine != nullptr)
		{
			bContractsRegistered =
				RegisterGenericContracts(*TestRunner, *ScriptEngine);
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		bContractsRegistered = false;
		ResetObservation();
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
		ResetObservation();
	}

	TEST_METHOD(MetadataByTargetArityAndAuxiliaryRegistration)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("EMBED-GENERIC-METADATA-CALLBACK",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Generic metadata product should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		const int32 GenericProbeTypeId =
			ScriptEngine->GetTypeIdByDecl("GenericProbe");
		const FIntrospectionCase Cases[] =
		{
			{ TEXT("global"), TEXT("zero"), TEXT("absent"), false, false, 0 },
			{ TEXT("global"), TEXT("zero"), TEXT("provided"), false, true, 0 },
			{ TEXT("global"), TEXT("one"), TEXT("absent"), false, false, 1 },
			{ TEXT("global"), TEXT("one"), TEXT("provided"), false, true, 1 },
			{ TEXT("global"), TEXT("two"), TEXT("absent"), false, false, 2 },
			{ TEXT("global"), TEXT("two"), TEXT("provided"), false, true, 2 },
			{ TEXT("object_method"), TEXT("zero"), TEXT("absent"), true, false, 0 },
			{ TEXT("object_method"), TEXT("zero"), TEXT("provided"), true, true, 0 },
			{ TEXT("object_method"), TEXT("one"), TEXT("absent"), true, false, 1 },
			{ TEXT("object_method"), TEXT("one"), TEXT("provided"), true, true, 1 },
			{ TEXT("object_method"), TEXT("two"), TEXT("absent"), true, false, 2 },
			{ TEXT("object_method"), TEXT("two"), TEXT("provided"), true, true, 2 },
		};

		int32 ObservedCaseCount = 0;
		for (const FIntrospectionCase& Case : Cases)
		{
			ResetObservation();
			const FString FunctionName = MakeIntrospectionFunctionName(Case);
			const FString CaseId = MakeNativeCaseId(
				"EMBED-GENERIC-METADATA-CALLBACK",
				{ Case.TargetId, Case.ArityId, Case.AuxiliaryId });
			const FString ModuleName = FString::Printf(
				TEXT("GenericMetadata_%s_%s_%s"),
				Case.TargetId,
				Case.ArityId,
				Case.AuxiliaryId);
			const FString Source = BuildIntrospectionSource(Case, FunctionName);
			PrintGeneratedAsSource(
				*TestRunner,
				*CaseId,
				*ModuleName,
				Source);

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get());
			if (!Module.IsValid())
			{
				continue;
			}

			int32 Result = INDEX_NONE;
			if (!ExecuteScriptFunction(
				*TestRunner,
				ScriptEngine,
				Module,
				"int Entry()",
				Result))
			{
				continue;
			}

			const int32 ExpectedResult =
				(Case.bObjectMethod ? 5 : 0)
				+ (Case.Arity >= 1 ? 10 : 0)
				+ (Case.Arity >= 2 ? 20 : 0);
			ASSERT_THAT(AreEqual(
				ExpectedResult,
				Result,
				TEXT("Generic metadata product should return the exact callback result")));
			ASSERT_THAT(AreEqual(
				1,
				Observation.CallbackCount,
				TEXT("Generic metadata product should invoke exactly one callback")));
			ASSERT_THAT(AreEqual(
				ScriptEngine,
				Observation.Engine,
				TEXT("Generic metadata product should expose the owning engine")));
			ASSERT_THAT(IsNotNull(
				Observation.Function,
				TEXT("Generic metadata product should expose the active system function")));
			ASSERT_THAT(AreEqual(
				Case.Arity,
				Observation.ArgumentCount,
				TEXT("Generic metadata product should expose the exact argument count")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asTYPEID_INT32),
				Observation.ReturnTypeId,
				TEXT("Generic metadata product should expose the exact return type")));
			ASSERT_THAT(AreEqual(
				static_cast<asDWORD>(0),
				Observation.ReturnFlags,
				TEXT("Generic metadata product should expose no reference flags for int returns")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				Observation.SetReturnResult,
				TEXT("Generic metadata product should accept its exact integer return setter")));
			ASSERT_THAT(AreEqual(
				Case.bObjectMethod ? GenericProbeTypeId : CurrentForkGlobalObjectTypeId,
				Observation.ObjectTypeId,
				TEXT("Generic metadata product should expose the registered receiver type or the current-fork minus-one global sentinel")));
			if (Case.bObjectMethod)
			{
				ASSERT_THAT(IsNotNull(
					Observation.Object,
					TEXT("Generic object-method callback should expose its receiver")));
			}
			else
			{
				ASSERT_THAT(IsNull(
					Observation.Object,
					TEXT("Generic global callback should not expose an object receiver")));
			}

			ASSERT_THAT(IsNull(
				Observation.Auxiliary,
				TEXT("Current fork should expose the recorded null generic auxiliary contract")));
			if (Case.bProvideAuxiliary)
			{
				TestRunner->AddInfo(
					TEXT("[AS-FORK-LIMITATION] RegisterGlobalFunction/RegisterObjectMethod accepted non-null auxiliary data, but asIScriptGeneric::GetAuxiliary remains null because asCScriptFunction::GetAuxiliary is a fork no-op"));
			}
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Module.Discard(),
				*FString::Printf(
					TEXT("%s should explicitly discard its generated module"),
					*CaseId)));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(
					ModuleNameUtf8.Get(),
					asGM_ONLY_IF_EXISTS),
				*FString::Printf(
					TEXT("%s module should be absent before the next generic metadata cell"),
					*CaseId)));
			++ObservedCaseCount;
		}

		ASSERT_THAT(AreEqual(
			12,
			ObservedCaseCount,
			TEXT("Target, arity, and auxiliary registration should execute every generic metadata cell")));
	}

	TEST_METHOD(ObjectReturnsByAuxiliaryRegistration)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("EMBED-GENERIC-OBJECT-RETURN",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Generic object-return product should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		const int32 GenericProbeTypeId =
			ScriptEngine->GetTypeIdByDecl("GenericProbe");
		int32 ObservedCaseCount = 0;
		for (const bool bProvideAuxiliary : { false, true })
		{
			ResetObservation();
			const TCHAR* const AuxiliaryId =
				bProvideAuxiliary ? TEXT("provided") : TEXT("absent");
			const FString FunctionName = bProvideAuxiliary
				? TEXT("EchoGenericProbe_Provided")
				: TEXT("EchoGenericProbe_Absent");
			const FString CaseId = MakeNativeCaseId(
				"EMBED-GENERIC-OBJECT-RETURN",
				{ AuxiliaryId });
			const FString ModuleName = FString::Printf(
				TEXT("GenericObjectReturn_%s"),
				AuxiliaryId);
			const FString Source = BuildObjectReturnSource(FunctionName);
			PrintGeneratedAsSource(
				*TestRunner,
				*CaseId,
				*ModuleName,
				Source);

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get());
			if (!Module.IsValid())
			{
				continue;
			}

			int32 Result = INDEX_NONE;
			if (!ExecuteScriptFunction(
				*TestRunner,
				ScriptEngine,
				Module,
				"int Entry()",
				Result))
			{
				continue;
			}

			ASSERT_THAT(AreEqual(
				42,
				Result,
				TEXT("Generic object-return product should copy the exact object value")));
			ASSERT_THAT(AreEqual(
				1,
				Observation.CallbackCount,
				TEXT("Generic object-return product should invoke exactly one callback")));
			ASSERT_THAT(AreEqual(
				1,
				Observation.ArgumentCount,
				TEXT("Generic object-return product should expose one object argument")));
			ASSERT_THAT(AreEqual(
				GenericProbeTypeId,
				Observation.ReturnTypeId,
				TEXT("Generic object-return product should expose the exact value-object return type")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asSUCCESS),
				Observation.SetReturnResult,
				TEXT("SetReturnObject should accept the exact registered value object")));
			ASSERT_THAT(IsNull(
				Observation.Auxiliary,
				TEXT("Current fork should retain the null generic auxiliary contract")));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Module.Discard(),
				*FString::Printf(
					TEXT("%s should explicitly discard its generated module"),
					*CaseId)));
			ASSERT_THAT(IsNull(
				ScriptEngine->GetModule(
					ModuleNameUtf8.Get(),
					asGM_ONLY_IF_EXISTS),
				*FString::Printf(
					TEXT("%s module should be absent before the next object-return cell"),
					*CaseId)));
			++ObservedCaseCount;
		}

		ASSERT_THAT(AreEqual(
			2,
			ObservedCaseCount,
			TEXT("Auxiliary registration state should execute every generic object-return cell")));
	}

	TEST_METHOD(NoCountImplicitHandleByValueDoesNotInvokeMissingRelease)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT_PART(
			"EMBED-GENERIC-OBJECT-RETURN",
			"no_count_implicit_handle_by_value");

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("Generic no-count implicit-handle test should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		ResetObservation();
		ImplicitNoCountProbe.Value = 83;
		ImplicitCountedProbe.Value = 97;
		ImplicitCountedProbe.ReferenceCount = 1;
		ImplicitCountedProbe.AddRefCalls = 0;
		ImplicitCountedProbe.ReleaseCalls = 0;
		ASSERT_THAT(IsTrue(
			ImplicitNoCountProbePointer == &ImplicitNoCountProbe,
			TEXT("Generic no-count implicit-handle application property should begin with its registered owner")));
		ASSERT_THAT(IsTrue(
			ImplicitCountedProbePointer == &ImplicitCountedProbe,
			TEXT("Generic counted implicit-handle control should begin with its registered owner")));
		const std::string Source = ASTEST_AS_ANSI(R"AS(
			int NoCountEntry()
			{
				return ReadImplicitNoCountByValue(ImplicitNoCountValue);
			}

			int CountedEntry()
			{
				return ReadImplicitCountedByValue(ImplicitCountedValue);
			}
			)AS");
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("EMBED-GENERIC-OBJECT-RETURN-no-count-implicit-handle-by-value"),
			TEXT("GenericNoCountImplicitHandleByValue"),
			UTF8_TO_TCHAR(Source.c_str()));
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"GenericNoCountImplicitHandleByValue",
			Source);
		if (!Module.IsValid())
		{
			return;
		}

		int32 Result = INDEX_NONE;
		ASSERT_THAT(IsTrue(
			ExecuteScriptFunction(
				*TestRunner,
				ScriptEngine,
				Module,
				"int NoCountEntry()",
				Result),
			TEXT("Generic no-count implicit-handle by-value callback should execute without a release behavior")));
		ASSERT_THAT(AreEqual(
			83,
			Result,
			TEXT("Generic no-count implicit-handle callback should receive the exact object")));
		ASSERT_THAT(AreEqual(
			1,
			Observation.CallbackCount,
			TEXT("Generic no-count implicit-handle callback should execute exactly once")));
		ASSERT_THAT(AreEqual(
			1,
			Observation.ArgumentCount,
			TEXT("Generic no-count implicit-handle callback should expose one argument")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Observation.SetReturnResult,
			TEXT("Generic no-count implicit-handle callback should publish its return value")));
		ASSERT_THAT(AreEqual(
			83,
			ImplicitNoCountProbe.Value,
			TEXT("Generic no-count implicit-handle cleanup should not release or mutate application ownership")));
		ASSERT_THAT(IsTrue(
			ImplicitNoCountProbePointer == &ImplicitNoCountProbe,
			TEXT("Generic no-count implicit-handle cleanup should preserve the registered application owner")));

		ResetObservation();
		Result = INDEX_NONE;
		ASSERT_THAT(IsTrue(
			ExecuteScriptFunction(
				*TestRunner,
				ScriptEngine,
				Module,
				"int CountedEntry()",
				Result),
			TEXT("Generic counted implicit-handle control should execute through the same by-value path")));
		ASSERT_THAT(AreEqual(
			97,
			Result,
			TEXT("Generic counted implicit-handle callback should receive the exact object")));
		ASSERT_THAT(AreEqual(
			1,
			Observation.CallbackCount,
			TEXT("Generic counted implicit-handle callback should execute exactly once")));
		ASSERT_THAT(AreEqual(
			1,
			Observation.ArgumentCount,
			TEXT("Generic counted implicit-handle callback should expose one argument")));
		ASSERT_THAT(AreEqual(
			1,
			ImplicitCountedProbe.AddRefCalls,
			TEXT("Generic counted implicit-handle transfer should retain exactly one moved owner")));
		ASSERT_THAT(AreEqual(
			1,
			ImplicitCountedProbe.ReleaseCalls,
			TEXT("Generic counted implicit-handle cleanup should release exactly one moved owner")));
		ASSERT_THAT(AreEqual(
			1,
			ImplicitCountedProbe.ReferenceCount,
			TEXT("Generic counted implicit-handle cleanup should restore the application baseline")));
		ASSERT_THAT(IsTrue(
			ImplicitCountedProbePointer == &ImplicitCountedProbe,
			TEXT("Generic counted implicit-handle cleanup should preserve the registered application owner")));

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Module.Discard(),
			TEXT("Generic no-count implicit-handle test should explicitly discard its module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"GenericNoCountImplicitHandleByValue",
				asGM_ONLY_IF_EXISTS),
			TEXT("Generic no-count implicit-handle module should be absent after cleanup")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
