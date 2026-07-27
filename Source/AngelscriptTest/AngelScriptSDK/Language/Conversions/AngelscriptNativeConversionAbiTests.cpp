#include "AngelscriptNativeConversionTestSupport.h"

#include "Core/FunctionCallers.h"
#include "CQTest.h"
#include "HAL/UnrealMemory.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::FNativeCaseContext;
using AngelscriptNativeTestSupport::FNativeMessageCollector;
using AngelscriptNativeTestSupport::FNativeMessageEntry;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConversionAbiTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.Abi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FScriptDeclarationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FNativeStorageCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* BindingDeclaration;
		int32 ExpectedArgumentMarker;
	};

	struct FDirectionCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FManualDeclarationCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* Declaration;
	};

	struct FAbiObservation
	{
		int32 Calls = 0;
		uint64 LastBits = 0;
	};

	inline static constexpr asPWORD AbiObservationSlot = static_cast<asPWORD>(0x434F4E5641424931ull);
	inline static float Float32PropertyValue = 0.0f;
	inline static double Float64PropertyValue = 0.0;

	inline static constexpr FScriptDeclarationCase ScriptDeclarationCases[] = {
		{"float"},
		{"double"},
	};

	inline static constexpr FNativeStorageCase NativeStorageCases[] = {
		{"float32", TEXT("float32"), 101},
		{"float64", TEXT("float64"), 202},
	};

	inline static constexpr FDirectionCase DirectionCases[] = {
		{"argument"},
		{"return"},
		{"property"},
	};

	inline static constexpr FManualDeclarationCase ManualDeclarationCases[] = {
		{"argument", "int RejectAbi(float Value)"},
		{"return", "float RejectAbi()"},
		{"property", "float RejectAbiValue"},
	};

	static uint64 BitsOf(const float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 BitsOf(const double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static FAbiObservation* ActiveObservation()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FAbiObservation*>(Context->GetEngine()->GetUserData(AbiObservationSlot))
			: nullptr;
	}

	static int RecordFloat32(const float Value)
	{
		if (FAbiObservation* const Observation = ActiveObservation())
		{
			++Observation->Calls;
			Observation->LastBits = BitsOf(Value);
		}
		return 101;
	}

	static int RecordFloat64(const double Value)
	{
		if (FAbiObservation* const Observation = ActiveObservation())
		{
			++Observation->Calls;
			Observation->LastBits = BitsOf(Value);
		}
		return 202;
	}

	static float ReturnFloat32()
	{
		constexpr float Value = 3.25f;
		if (FAbiObservation* const Observation = ActiveObservation())
		{
			++Observation->Calls;
			Observation->LastBits = BitsOf(Value);
		}
		return Value;
	}

	static double ReturnFloat64()
	{
		constexpr double Value = 3.25;
		if (FAbiObservation* const Observation = ActiveObservation())
		{
			++Observation->Calls;
			Observation->LastBits = BitsOf(Value);
		}
		return Value;
	}

	static bool IsDirection(const FDirectionCase& DirectionCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(DirectionCase.CatalogName, Name) == 0;
	}

	static FString ScriptDeclaration(const FScriptDeclarationCase& DeclarationCase)
	{
		return ANSI_TO_TCHAR(DeclarationCase.CatalogName);
	}

	static uint64 ExpectedBits(const FNativeStorageCase& NativeStorageCase, const double Value)
	{
		return FCStringAnsi::Strcmp(NativeStorageCase.CatalogName, "float32") == 0
			? BitsOf(static_cast<float>(Value))
			: BitsOf(Value);
	}

	static int RegisterAbiSurface(
		asIScriptEngine& ScriptEngine,
		const FNativeStorageCase& NativeStorageCase,
		const FDirectionCase& DirectionCase)
	{
		const bool bFloat32 = FCStringAnsi::Strcmp(NativeStorageCase.CatalogName, "float32") == 0;
		if (IsDirection(DirectionCase, "argument"))
		{
			const FString Declaration = FString::Printf(
				TEXT("int CallAbi(%s Value)"), NativeStorageCase.BindingDeclaration);
			const FTCHARToUTF8 DeclarationUtf8(*Declaration);
			const ASAutoCaller::FunctionCaller Caller = bFloat32
				? ASAutoCaller::MakeFunctionCaller(RecordFloat32)
				: ASAutoCaller::MakeFunctionCaller(RecordFloat64);
			return bFloat32
				? ScriptEngine.RegisterGlobalFunction(
					DeclarationUtf8.Get(), asFUNCTION(RecordFloat32), asCALL_CDECL, *(asFunctionCaller*)&Caller)
				: ScriptEngine.RegisterGlobalFunction(
					DeclarationUtf8.Get(), asFUNCTION(RecordFloat64), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		}

		if (IsDirection(DirectionCase, "return"))
		{
			const FString Declaration = FString::Printf(
				TEXT("%s CallAbi()"), NativeStorageCase.BindingDeclaration);
			const FTCHARToUTF8 DeclarationUtf8(*Declaration);
			const ASAutoCaller::FunctionCaller Caller = bFloat32
				? ASAutoCaller::MakeFunctionCaller(ReturnFloat32)
				: ASAutoCaller::MakeFunctionCaller(ReturnFloat64);
			return bFloat32
				? ScriptEngine.RegisterGlobalFunction(
					DeclarationUtf8.Get(), asFUNCTION(ReturnFloat32), asCALL_CDECL, *(asFunctionCaller*)&Caller)
				: ScriptEngine.RegisterGlobalFunction(
					DeclarationUtf8.Get(), asFUNCTION(ReturnFloat64), asCALL_CDECL, *(asFunctionCaller*)&Caller);
		}

		const FString Declaration = FString::Printf(
			TEXT("%s AbiValue"), NativeStorageCase.BindingDeclaration);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		return ScriptEngine.RegisterGlobalProperty(
			DeclarationUtf8.Get(),
			bFloat32 ? static_cast<void*>(&Float32PropertyValue) : static_cast<void*>(&Float64PropertyValue));
	}

	static FString RegisteredFunctionDeclaration(
		const FNativeStorageCase& NativeStorageCase,
		const FDirectionCase& DirectionCase)
	{
		const TCHAR* const PublishedStorageDeclaration = FCStringAnsi::Strcmp(
			NativeStorageCase.CatalogName,
			"float64") == 0
			? TEXT("float")
			: NativeStorageCase.BindingDeclaration;
		if (IsDirection(DirectionCase, "argument"))
		{
			return FString::Printf(TEXT("int CallAbi(%s)"), PublishedStorageDeclaration);
		}
		return FString::Printf(TEXT("%s CallAbi()"), PublishedStorageDeclaration);
	}

	static FString BuildAbiSource(
		const FScriptDeclarationCase& DeclarationCase,
		const FDirectionCase& DirectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Declaration = ScriptDeclaration(DeclarationCase);
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunAbi()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsDirection(DirectionCase, "argument"))
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Value = %s(3.25);"), *Declaration, *Declaration));
			AppendGeneratedAsLine(Source, TEXT("\treturn CallAbi(Value);"));
		}
		else if (IsDirection(DirectionCase, "return"))
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Result = CallAbi();"), *Declaration));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn Result == %s(3.25) ? 1 : 0;"), *Declaration));
		}
		else
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Input = %s(6.5);"), *Declaration, *Declaration));
			AppendGeneratedAsLine(Source, TEXT("\tAbiValue = Input;"));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Output = AbiValue;"), *Declaration));
			AppendGeneratedAsLine(Source, TEXT("\treturn Output == Input ? 1 : 0;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool VerifyRegisteredMetadata(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FNativeCaseContext& Case,
		const FNativeStorageCase& NativeStorageCase,
		const FDirectionCase& DirectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assert(Test);
		const FTCHARToUTF8 TypeDeclarationUtf8(NativeStorageCase.BindingDeclaration);
		const int ExpectedTypeId = ScriptEngine.GetTypeIdByDecl(TypeDeclarationUtf8.Get());
		if (!Assert.IsTrue(ExpectedTypeId >= 0,
			*Case.Describe(TEXT("native ABI storage declaration should resolve to a type ID"))))
		{
			return false;
		}

		if (IsDirection(DirectionCase, "property"))
		{
			for (asUINT Index = 0; Index < ScriptEngine.GetGlobalPropertyCount(); ++Index)
			{
				const ANSICHAR* Name = nullptr;
				int TypeId = asINVALID_TYPE;
				if (ScriptEngine.GetGlobalPropertyByIndex(Index, &Name, nullptr, &TypeId) >= 0
					&& Name != nullptr && FCStringAnsi::Strcmp(Name, "AbiValue") == 0)
				{
					return Assert.AreEqual(ExpectedTypeId,
						TypeId,
						*Case.Describe(TEXT("registered native ABI property should retain its exact storage type")));
				}
			}
			return Assert.IsTrue(false,
				*Case.Describe(TEXT("registered native ABI property should be discoverable by its exact name")));
		}

		const FString Declaration = RegisteredFunctionDeclaration(NativeStorageCase, DirectionCase);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asIScriptFunction* const Function = GetNativeGlobalFunctionByPublishedDeclaration(
			&ScriptEngine,
			DeclarationUtf8.Get());
		if (!Assert.IsNotNull(Function,
			*Case.Describe(TEXT("registered native ABI function should publish its exact declaration"))))
		{
			Test.AddInfo(*Case.Describe(*FString::Printf(
				TEXT("ABI declaration trace: requested='%s' published='%s'"),
				*Declaration,
				*CollectGlobalFunctionDeclarations(&ScriptEngine))));
			return false;
		}

		if (IsDirection(DirectionCase, "argument"))
		{
			int TypeId = asINVALID_TYPE;
			return Assert.AreEqual(asSUCCESS,
				Function->GetParam(0, &TypeId),
				*Case.Describe(TEXT("registered native ABI argument metadata should be readable")))
				&& Assert.AreEqual(ExpectedTypeId,
					TypeId,
					*Case.Describe(TEXT("registered native ABI argument should retain its storage type")));
		}

		return Assert.AreEqual(ExpectedTypeId,
			Function->GetReturnTypeId(),
			*Case.Describe(TEXT("registered native ABI return should retain its storage type")));
	}

	static int32 ExpectedScriptReturn(
		const FNativeStorageCase& NativeStorageCase,
		const FDirectionCase& DirectionCase)
	{
		return IsDirection(DirectionCase, "argument") ? NativeStorageCase.ExpectedArgumentMarker : 1;
	}

	static uint64 ObservedNativeBits(
		const FNativeStorageCase& NativeStorageCase,
		const FDirectionCase& DirectionCase,
		const FAbiObservation& Observation)
	{
		if (IsDirection(DirectionCase, "property"))
		{
			return FCStringAnsi::Strcmp(NativeStorageCase.CatalogName, "float32") == 0
				? BitsOf(Float32PropertyValue)
				: BitsOf(Float64PropertyValue);
		}
		return Observation.LastBits;
	}

	static bool HasErrors(const FNativeMessageCollector& Messages)
	{
		for (const FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				return true;
			}
		}
		return false;
	}

	static int RegisterAmbiguousManualDeclaration(
		asIScriptEngine& ScriptEngine,
		const FManualDeclarationCase& DeclarationCase)
	{
		if (FCStringAnsi::Strcmp(DeclarationCase.CatalogName, "property") == 0)
		{
			return ScriptEngine.RegisterGlobalProperty(DeclarationCase.Declaration, &Float32PropertyValue);
		}
		return ScriptEngine.RegisterGlobalFunction(
			DeclarationCase.Declaration, asFUNCTION(RecordFloat32), asCALL_CDECL);
	}

public:
	TEST_METHOD(DeclarationsByStorageAndDirection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-ABI",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);

		for (const FScriptDeclarationCase& DeclarationCase : ScriptDeclarationCases)
		{
			for (const FNativeStorageCase& NativeStorageCase : NativeStorageCases)
			{
				for (const FDirectionCase& DirectionCase : DirectionCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-CONV-ABI",
						{ANSI_TO_TCHAR(DeclarationCase.CatalogName),
							ANSI_TO_TCHAR(NativeStorageCase.CatalogName),
							ANSI_TO_TCHAR(DirectionCase.CatalogName)}));
					const FString ModuleName = TEXT("ConversionAbi_") + Case.GetId();
					const FString Source = BuildAbiSource(DeclarationCase, DirectionCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

					FNativeTestEngine Engine;
					Engine.Create(*TestRunner);
					ON_SCOPE_EXIT
					{
						Engine.Destroy();
					};
					asIScriptEngine* const ScriptEngine = Engine.Get();
					ASSERT_THAT(IsNotNull(ScriptEngine,
						*Case.Describe(TEXT("ABI cell should create a fresh raw SDK engine"))));
					if (ScriptEngine == nullptr)
					{
						continue;
					}

					ASSERT_THAT(IsTrue(ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0,
						*Case.Describe(TEXT("ABI cell should retain the current double-backed float configuration"))));
					Float32PropertyValue = 0.0f;
					Float64PropertyValue = 0.0;
					FAbiObservation Observation;
					ScriptEngine->SetUserData(&Observation, AbiObservationSlot);
					ON_SCOPE_EXIT
					{
						ScriptEngine->SetUserData(nullptr, AbiObservationSlot);
					};

					Engine.Reset(*TestRunner);
					const int RegistrationResult = RegisterAbiSurface(*ScriptEngine, NativeStorageCase, DirectionCase);
					ASSERT_THAT(IsTrue(RegistrationResult >= 0,
						*Case.Describe(TEXT("matching float32/float64 manual declaration should register once on its fresh engine"))));
					if (RegistrationResult < 0)
					{
						continue;
					}

					ASSERT_THAT(IsTrue(VerifyRegisteredMetadata(
						*TestRunner, *ScriptEngine, Case, NativeStorageCase, DirectionCase),
						*Case.Describe(TEXT("ABI cell should expose matching raw registration metadata"))));
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					ASSERT_THAT(IsTrue(BuildResult >= 0 && Module != nullptr,
						*Case.DescribeResult("<build>",
							TEXT("matching ABI source should compile against its registered native surface"),
							Engine.GetMessagesText())));
					if (BuildResult < 0 || Module == nullptr)
					{
						continue;
					}

					ASSERT_THAT(IsFalse(HasErrors(Engine.GetMessages()),
						*Case.Describe(TEXT("accepted ABI source should emit no compile errors"))));
					asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int RunAbi()");
					ASSERT_THAT(IsNotNull(Entry,
						*Case.Describe(TEXT("accepted ABI source should publish its exact entry declaration"))));
					if (Entry != nullptr)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						ASSERT_THAT(IsNotNull(Context,
							*Case.Describe(TEXT("accepted ABI source should create a raw context"))));
						if (Context != nullptr)
						{
							const int ExecutionResult = PrepareAndExecute(Context, Entry);
							if (ExecutionResult != asEXECUTION_FINISHED)
							{
								const ANSICHAR* const ExceptionString = Context->GetExceptionString();
								const ANSICHAR* const EntryDeclaration = Entry->GetDeclaration();
								TestRunner->AddInfo(*Case.Describe(*FString::Printf(
									TEXT("ABI execution trace: result=%d exception=%s line=%d entry=%s native=%hs script=%hs direction=%hs"),
									ExecutionResult,
									ExceptionString != nullptr ? UTF8_TO_TCHAR(ExceptionString) : TEXT("<none>"),
									Context->GetExceptionLineNumber(),
									EntryDeclaration != nullptr ? UTF8_TO_TCHAR(EntryDeclaration) : TEXT("<none>"),
									NativeStorageCase.CatalogName,
									DeclarationCase.CatalogName,
									DirectionCase.CatalogName)));
							}
							ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, ExecutionResult,
								*Case.Describe(TEXT("accepted ABI source should execute to completion"))));
							if (ExecutionResult == asEXECUTION_FINISHED)
							{
								ASSERT_THAT(AreEqual(ExpectedScriptReturn(NativeStorageCase, DirectionCase),
									static_cast<int32>(Context->GetReturnDWord()),
									*Case.Describe(TEXT("accepted ABI source should return its native direction marker"))));
							}
							Context->Release();
						}
					}

					const int32 ExpectedCalls = IsDirection(DirectionCase, "property") ? 0 : 1;
					const double ExpectedValue = IsDirection(DirectionCase, "property") ? 6.5 : 3.25;
					ASSERT_THAT(AreEqual(ExpectedCalls,
						Observation.Calls,
						*Case.Describe(TEXT("ABI call directions should invoke the native callback exactly once"))));
					ASSERT_THAT(AreEqual(ExpectedBits(NativeStorageCase, ExpectedValue),
						ObservedNativeBits(NativeStorageCase, DirectionCase, Observation),
						*Case.Describe(TEXT("ABI direction should preserve the native storage representation bits"))));

					ASSERT_THAT(IsTrue(ScriptEngine->DiscardModule(ModuleNameUtf8.Get()) >= 0,
						*Case.Describe(TEXT("ABI cell should discard its isolated module"))));
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("ABI cell should remove its module after discard"))));
				}
			}
		}
	}

	TEST_METHOD(AmbiguousManualDeclarations)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-ABI-MANUAL-DECLARATION",
			ENativeEvidence::Diagnostic | ENativeEvidence::Metadata | ENativeEvidence::Isolation);

		for (const FManualDeclarationCase& DeclarationCase : ManualDeclarationCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"LANG-CONV-ABI-MANUAL-DECLARATION", {ANSI_TO_TCHAR(DeclarationCase.CatalogName)}));
			FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				Engine.Destroy();
			};
			asIScriptEngine* const ScriptEngine = Engine.Get();
			ASSERT_THAT(IsNotNull(ScriptEngine,
				*Case.Describe(TEXT("ambiguous manual ABI declaration should create a raw SDK engine"))));
			if (ScriptEngine == nullptr)
			{
				continue;
			}

			Engine.Reset(*TestRunner);
			const asUINT FunctionCountBefore = ScriptEngine->GetGlobalFunctionCount();
			const asUINT PropertyCountBefore = ScriptEngine->GetGlobalPropertyCount();
			const int RegistrationResult = RegisterAmbiguousManualDeclaration(*ScriptEngine, DeclarationCase);
			ASSERT_THAT(IsTrue(RegistrationResult < 0,
				*Case.Describe(TEXT("manual C++ binding should reject ambiguous float spelling"))));
			ASSERT_THAT(IsTrue(ContainsError(Engine.GetMessages(),
				TEXT("Use of 'float' in manual C++ bindings is ambiguous")),
				*Case.Describe(TEXT("manual C++ binding rejection should explain the required float32/float64 spelling"))));
			ASSERT_THAT(AreEqual(FunctionCountBefore,
				ScriptEngine->GetGlobalFunctionCount(),
				*Case.Describe(TEXT("rejected manual function declaration should not publish metadata"))));
			ASSERT_THAT(AreEqual(PropertyCountBefore,
				ScriptEngine->GetGlobalPropertyCount(),
				*Case.Describe(TEXT("rejected manual property declaration should not publish metadata"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
