#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::FNativeTestEngine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FObjectCastTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.ObjectCast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	enum class EObjectCastKind : uint8
	{
		Base,
		Derived,
		Unrelated,
		Null,
	};

	struct FKindCase
	{
		const ANSICHAR* CatalogName;
		EObjectCastKind Kind;
	};

	struct FViewCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
	};

	struct FFormCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FObjectCastState
	{
		int32 NextIdentity = 1;
		int32 Created = 0;
		int32 Destroyed = 0;
		int32 Live = 0;
		int32 CastCalls = 0;

		void Reset()
		{
			NextIdentity = 1;
			Created = 0;
			Destroyed = 0;
			Live = 0;
			CastCalls = 0;
		}
	};

	class FObjectCastNativeBase
	{
	public:
		FObjectCastNativeBase(FObjectCastState& InState, const EObjectCastKind InKind)
			: State(InState)
			, Kind(InKind)
			, Identity(State.NextIdentity++)
		{
			++State.Created;
			++State.Live;
		}

		virtual ~FObjectCastNativeBase()
		{
			++State.Destroyed;
			--State.Live;
		}

		void AddRef()
		{
			++ReferenceCount;
		}

		void Release()
		{
			if (--ReferenceCount == 0)
			{
				delete this;
			}
		}

		EObjectCastKind GetKind() const
		{
			return Kind;
		}

		int32 GetIdentity() const
		{
			return Identity;
		}

		int32 GetField() const
		{
			return (static_cast<int32>(Kind) + 1) * 1000 + Identity;
		}

		FObjectCastState& GetState() const
		{
			return State;
		}

	private:
		FObjectCastState& State;
		EObjectCastKind Kind;
		int32 Identity = 0;
		int32 ReferenceCount = 1;
	};

	class FObjectCastNativeDerived final : public FObjectCastNativeBase
	{
	public:
		explicit FObjectCastNativeDerived(FObjectCastState& State)
			: FObjectCastNativeBase(State, EObjectCastKind::Derived)
		{
		}
	};

	class FObjectCastNativeUnrelated final : public FObjectCastNativeBase
	{
	public:
		explicit FObjectCastNativeUnrelated(FObjectCastState& State)
			: FObjectCastNativeBase(State, EObjectCastKind::Unrelated)
		{
		}
	};

	inline static constexpr asPWORD ObjectCastStateUserDataSlot = 0x4f434153;

	inline static constexpr FKindCase RuntimeKindCases[] =
	{
		{ "base", EObjectCastKind::Base },
		{ "derived", EObjectCastKind::Derived },
		{ "unrelated", EObjectCastKind::Unrelated },
		{ "null", EObjectCastKind::Null },
	};

	inline static constexpr FViewCase ViewCases[] =
	{
		{ "base", "FObjectCastBase" },
		{ "derived", "FObjectCastDerived" },
		{ "unrelated", "FObjectCastUnrelated" },
	};

	inline static constexpr FFormCase FormCases[] =
	{
		{ "assignment" },
		{ "initializer" },
		{ "argument" },
		{ "return" },
		{ "explicit_cast" },
	};

	static bool IsNamed(const ANSICHAR* Value, const ANSICHAR* Name)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(Value, Name);
	}

	static bool IsNamed(const FViewCase& ViewCase, const ANSICHAR* Name)
	{
		return IsNamed(ViewCase.CatalogName, Name);
	}

	static bool IsNamed(const FFormCase& FormCase, const ANSICHAR* Name)
	{
		return IsNamed(FormCase.CatalogName, Name);
	}

	static FObjectCastState* GetObjectCastState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
			? static_cast<FObjectCastState*>(Generic.GetEngine()->GetUserData(ObjectCastStateUserDataSlot))
			: nullptr;
	}

	static void GenericAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FObjectCastNativeBase* const Object = static_cast<FObjectCastNativeBase*>(Generic->GetObject()))
			{
				Object->AddRef();
			}
		}
	}

	static void GenericRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FObjectCastNativeBase* const Object = static_cast<FObjectCastNativeBase*>(Generic->GetObject()))
			{
				Object->Release();
			}
		}
	}

	static void GenericGetIdentity(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			const FObjectCastNativeBase* const Object = static_cast<const FObjectCastNativeBase*>(Generic->GetObject());
			Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->GetIdentity()) : 0);
		}
	}

	static void GenericGetField(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			const FObjectCastNativeBase* const Object = static_cast<const FObjectCastNativeBase*>(Generic->GetObject());
			Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->GetField()) : 0);
		}
	}

	static void ReturnNativeObject(asIScriptGeneric& Generic, FObjectCastNativeBase* Object)
	{
		Generic.SetReturnAddress(Object);
	}

	static void GenericMakeBase(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FObjectCastState* const State = GetObjectCastState(*Generic);
		ReturnNativeObject(*Generic, State != nullptr ? new FObjectCastNativeBase(*State, EObjectCastKind::Base) : nullptr);
	}

	static void GenericMakeDerived(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FObjectCastState* const State = GetObjectCastState(*Generic);
		ReturnNativeObject(*Generic, State != nullptr ? new FObjectCastNativeDerived(*State) : nullptr);
	}

	static void GenericMakeUnrelated(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FObjectCastState* const State = GetObjectCastState(*Generic);
		ReturnNativeObject(*Generic, State != nullptr ? new FObjectCastNativeUnrelated(*State) : nullptr);
	}

	static void GenericMakeNull(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			ReturnNativeObject(*Generic, nullptr);
		}
	}

	template <typename TargetType>
	static TargetType* CastObject(
		FObjectCastNativeBase* Object,
		const EObjectCastKind TargetKind)
	{
		if (Object == nullptr)
		{
			return nullptr;
		}

		++Object->GetState().CastCalls;
		if (Object->GetKind() != TargetKind)
		{
			return nullptr;
		}
		TargetType* const Result = static_cast<TargetType*>(Object);
		Result->AddRef();
		return Result;
	}

	static FObjectCastNativeBase* CastToBase(FObjectCastNativeBase* Object)
	{
		if (Object != nullptr)
		{
			++Object->GetState().CastCalls;
			Object->AddRef();
		}
		return Object;
	}

	static void GenericDerivedToBase(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnAddress(CastToBase(static_cast<FObjectCastNativeBase*>(Generic->GetObject())));
		}
	}

	static void GenericUnrelatedToBase(asIScriptGeneric* Generic)
	{
		GenericDerivedToBase(Generic);
	}

	static void GenericBaseToDerived(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnAddress(CastObject<FObjectCastNativeDerived>(
				static_cast<FObjectCastNativeBase*>(Generic->GetObject()),
				EObjectCastKind::Derived));
		}
	}

	static void GenericBaseToUnrelated(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnAddress(CastObject<FObjectCastNativeUnrelated>(
				static_cast<FObjectCastNativeBase*>(Generic->GetObject()),
				EObjectCastKind::Unrelated));
		}
	}

	static bool RegisterReferenceType(
		asIScriptEngine& Engine,
		const ANSICHAR* TypeName,
		FString& OutFailedOperation)
	{
		auto RequireRegistration = [&OutFailedOperation, TypeName](const int Result, const ANSICHAR* Operation)
		{
			if (Result >= 0)
			{
				return true;
			}

			OutFailedOperation = FString::Printf(TEXT("%hs:%hs result=%d"), TypeName, Operation, Result);
			return false;
		};

		// Current-fork scripts use implicit handles for native reference classes.
		// The cast declarations below intentionally omit '@', so the raw SDK
		// fixture must publish the same contract as project-native bindings.
		return RequireRegistration(Engine.RegisterObjectType(TypeName, 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE), "type")
			&& RequireRegistration(Engine.RegisterObjectBehaviour(TypeName, asBEHAVE_ADDREF, "void f()", asFUNCTION(GenericAddRef), asCALL_GENERIC), "addref")
			&& RequireRegistration(Engine.RegisterObjectBehaviour(TypeName, asBEHAVE_RELEASE, "void f()", asFUNCTION(GenericRelease), asCALL_GENERIC), "release")
			&& RequireRegistration(Engine.RegisterObjectMethod(TypeName, "int GetIdentity() const", asFUNCTION(GenericGetIdentity), asCALL_GENERIC), "identity")
			&& RequireRegistration(Engine.RegisterObjectMethod(TypeName, "int GetField() const", asFUNCTION(GenericGetField), asCALL_GENERIC), "field");
	}

	static bool RegisterObjectCastFixtures(
		asIScriptEngine& Engine,
		FObjectCastState& State,
		FString& OutFailedOperation)
	{
		Engine.SetUserData(&State, ObjectCastStateUserDataSlot);
		if (!RegisterReferenceType(Engine, "FObjectCastBase", OutFailedOperation)
			|| !RegisterReferenceType(Engine, "FObjectCastDerived", OutFailedOperation)
			|| !RegisterReferenceType(Engine, "FObjectCastUnrelated", OutFailedOperation))
		{
			return false;
		}

		auto RequireRegistration = [&OutFailedOperation](const int Result, const TCHAR* Operation)
		{
			if (Result >= 0)
			{
				return true;
			}

			OutFailedOperation = FString::Printf(TEXT("%s result=%d"), Operation, Result);
			return false;
		};

		return RequireRegistration(Engine.RegisterObjectMethod("FObjectCastDerived", "FObjectCastBase opImplCast()", asFUNCTION(GenericDerivedToBase), asCALL_GENERIC), TEXT("derived implicit cast"))
			&& RequireRegistration(Engine.RegisterObjectMethod("FObjectCastUnrelated", "FObjectCastBase opImplCast()", asFUNCTION(GenericUnrelatedToBase), asCALL_GENERIC), TEXT("unrelated implicit cast"))
			&& RequireRegistration(Engine.RegisterObjectMethod("FObjectCastBase", "FObjectCastDerived opCast()", asFUNCTION(GenericBaseToDerived), asCALL_GENERIC), TEXT("base-to-derived cast"))
			&& RequireRegistration(Engine.RegisterObjectMethod("FObjectCastBase", "FObjectCastUnrelated opCast()", asFUNCTION(GenericBaseToUnrelated), asCALL_GENERIC), TEXT("base-to-unrelated cast"))
			&& RequireRegistration(Engine.RegisterGlobalFunction("FObjectCastBase MakeObjectCastBase()", asFUNCTION(GenericMakeBase), asCALL_GENERIC), TEXT("base factory"))
			&& RequireRegistration(Engine.RegisterGlobalFunction("FObjectCastBase MakeObjectCastDerived()", asFUNCTION(GenericMakeDerived), asCALL_GENERIC), TEXT("derived factory"))
			&& RequireRegistration(Engine.RegisterGlobalFunction("FObjectCastBase MakeObjectCastUnrelated()", asFUNCTION(GenericMakeUnrelated), asCALL_GENERIC), TEXT("unrelated factory"))
			&& RequireRegistration(Engine.RegisterGlobalFunction("FObjectCastBase MakeObjectCastNull()", asFUNCTION(GenericMakeNull), asCALL_GENERIC), TEXT("null factory"));
	}

	static FString RuntimeFactory(const FKindCase& RuntimeKindCase)
	{
		switch (RuntimeKindCase.Kind)
		{
		case EObjectCastKind::Base:
			return TEXT("MakeObjectCastBase()");
		case EObjectCastKind::Derived:
			return TEXT("MakeObjectCastDerived()");
		case EObjectCastKind::Unrelated:
			return TEXT("MakeObjectCastUnrelated()");
		case EObjectCastKind::Null:
		default:
			return TEXT("MakeObjectCastNull()");
		}
	}

	static FString SourceExpression(const FViewCase& SourceViewCase)
	{
		return IsNamed(SourceViewCase, "base")
			? TEXT("RuntimeValue")
			: FString::Printf(TEXT("cast<%hs>(RuntimeValue)"), SourceViewCase.ScriptType);
	}

	static bool IsImplicitlyConvertible(const FViewCase& SourceViewCase, const FViewCase& TargetViewCase)
	{
		return IsNamed(SourceViewCase, TargetViewCase.CatalogName)
			|| (IsNamed(TargetViewCase, "base") && !IsNamed(SourceViewCase, "base"));
	}

	static bool ShouldCompile(const FViewCase& SourceViewCase, const FViewCase& TargetViewCase, const FFormCase& FormCase)
	{
		return IsNamed(FormCase, "explicit_cast")
			? (IsImplicitlyConvertible(SourceViewCase, TargetViewCase)
				|| (IsNamed(SourceViewCase, "base") && !IsNamed(TargetViewCase, "base")))
			: IsImplicitlyConvertible(SourceViewCase, TargetViewCase);
	}

	static bool IsVisibleThroughView(const EObjectCastKind RuntimeKind, const FViewCase& ViewCase)
	{
		return RuntimeKind != EObjectCastKind::Null
			&& (IsNamed(ViewCase, "base")
				|| (RuntimeKind == EObjectCastKind::Derived && IsNamed(ViewCase, "derived"))
				|| (RuntimeKind == EObjectCastKind::Unrelated && IsNamed(ViewCase, "unrelated")));
	}

	static int32 ExpectedRuntimeResult(
		const FKindCase& RuntimeKindCase,
		const FViewCase& SourceViewCase,
		const FViewCase& TargetViewCase)
	{
		return IsVisibleThroughView(RuntimeKindCase.Kind, SourceViewCase)
			&& IsVisibleThroughView(RuntimeKindCase.Kind, TargetViewCase)
			? (static_cast<int32>(RuntimeKindCase.Kind) + 1) * 1000 + 1
			: -1;
	}

	static FString BuildObjectCastSource(
		const FKindCase& RuntimeKindCase,
		const FViewCase& SourceViewCase,
		const FViewCase& TargetViewCase,
		const FFormCase& FormCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString ConvertedSource = IsNamed(FormCase, "explicit_cast")
			? FString::Printf(TEXT("cast<%hs>(SourceValue)"), TargetViewCase.ScriptType)
			: TEXT("SourceValue");

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int ObserveObjectCastTarget(%hs Value)"), TargetViewCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.GetField();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%hs ReturnObjectCastTarget(%hs Value)"), TargetViewCase.ScriptType, SourceViewCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, IsNamed(FormCase, "explicit_cast")
			? FString::Printf(TEXT("\treturn cast<%hs>(Value);"), TargetViewCase.ScriptType)
			: TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunObjectCast()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFObjectCastBase RuntimeValue = ") + RuntimeFactory(RuntimeKindCase) + TEXT(";"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs SourceValue = %s;"), SourceViewCase.ScriptType, *SourceExpression(SourceViewCase)));

		if (IsNamed(FormCase, "assignment"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs TargetValue;"), TargetViewCase.ScriptType));
			AppendGeneratedAsLine(Source, TEXT("\tTargetValue = SourceValue;"));
		}
		else if (IsNamed(FormCase, "initializer"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs TargetValue = SourceValue;"), TargetViewCase.ScriptType));
		}
		else if (IsNamed(FormCase, "argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveObjectCastTarget(") + ConvertedSource + TEXT(");"));
		}
		else if (IsNamed(FormCase, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs TargetValue = ReturnObjectCastTarget(SourceValue);"), TargetViewCase.ScriptType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs TargetValue = %s;"), TargetViewCase.ScriptType, *ConvertedSource));
		}

		if (!IsNamed(FormCase, "argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (TargetValue == nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn -1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn TargetValue.GetIdentity() == SourceValue.GetIdentity() ? TargetValue.GetField() : -2;"));
		}

		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static void VerifyFixtureMetadata(
		FAutomationTestBase& Test,
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptEngine& Engine)
	{
		FNoDiscardAsserter Assertions(Test);
		for (const FViewCase& ViewCase : ViewCases)
		{
			asITypeInfo* const Type = Engine.GetTypeInfoByDecl(ViewCase.ScriptType);
			if (!Assertions.IsNotNull(Type,
				*Case.Describe(TEXT("object cast fixture should publish each exact native reference view"))))
			{
				continue;
			}
			const asDWORD TypeFlags = Type->GetFlags();
			if (!Assertions.IsTrue((TypeFlags & asOBJ_REF) != 0,
				*Case.Describe(TEXT("object cast fixture should retain reference semantics"))))
			{
				continue;
			}
			if (!Assertions.IsTrue((TypeFlags & asOBJ_IMPLICIT_HANDLE) != 0,
				*Case.Describe(TEXT("object cast fixture should retain its current-fork implicit-handle contract"))))
			{
				continue;
			}
			if (!Assertions.IsNotNull(Type->GetMethodByDecl("int GetIdentity() const"),
				*Case.Describe(TEXT("object cast fixture should expose identity observation metadata"))))
			{
				continue;
			}
		}
	}

public:

	TEST_METHOD(RuntimeKindsBySourceTargetAndForm)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-OBJECT-CAST",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Object cast product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FObjectCastState State;
		FString FailedRegistration;
		const bool bRegistered = RegisterObjectCastFixtures(*ScriptEngine, State, FailedRegistration);
		ASSERT_THAT(IsTrue(bRegistered,
			*FString::Printf(
				TEXT("Object cast product should register only raw SDK reference fixtures; failed='%s'; diagnostics='%s'"),
				*FailedRegistration,
				*CollectMessages(NativeEngine.GetMessages()))));
		if (!bRegistered)
		{
			return;
		}
		if (ScriptEngine->GetTypeInfoByDecl("FObjectCastBase") == nullptr)
		{
			return;
		}

		for (const FKindCase& RuntimeKindCase : RuntimeKindCases)
		{
			for (const FViewCase& SourceViewCase : ViewCases)
			{
				for (const FViewCase& TargetViewCase : ViewCases)
				{
					for (const FFormCase& FormCase : FormCases)
					{
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-CONV-OBJECT-CAST",
							{ ANSI_TO_TCHAR(RuntimeKindCase.CatalogName), ANSI_TO_TCHAR(SourceViewCase.CatalogName),
								ANSI_TO_TCHAR(TargetViewCase.CatalogName), ANSI_TO_TCHAR(FormCase.CatalogName) }));
						const FString ModuleName = TEXT("ObjectCast_") + Case.GetId();
						const FString Source = BuildObjectCastSource(RuntimeKindCase, SourceViewCase, TargetViewCase, FormCase);

						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						NativeEngine.Reset(*TestRunner);
						State.Reset();
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);

						if (!ShouldCompile(SourceViewCase, TargetViewCase, FormCase))
						{
							ASSERT_THAT(IsTrue(BuildResult < 0,
								*Case.Describe(TEXT("unsupported implicit object-view conversion should fail compilation"))));
							ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(NativeEngine.GetMessages(), ModuleName),
								*Case.Describe(TEXT("illegal object view conversion should own a located diagnostic"))));
						}
						else if (BuildResult >= 0 && Module != nullptr)
						{
							VerifyFixtureMetadata(*TestRunner, Case, *ScriptEngine);
							asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TEXT("int"), TEXT("RunObjectCast"));
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT("object cast cell should resolve its exact entry declaration"))));
							if (Entry != nullptr)
							{
								int32 Result = INDEX_NONE;
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
									ExecuteNoArgumentIntEntry(*ScriptEngine, *Entry, Result),
									*Case.Describe(TEXT("native object cast cell should finish without a script-class construction exception"))));
								ASSERT_THAT(AreEqual(ExpectedRuntimeResult(RuntimeKindCase, SourceViewCase, TargetViewCase), Result,
									*Case.Describe(TEXT("object cast cell should preserve null state, identity, field value, argument, and return behavior"))));
							}
						}

						ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
							*Case.Describe(TEXT("object cast cell should discard its module and references"))));
						ASSERT_THAT(AreEqual(0, State.Live,
							*Case.Describe(TEXT("object cast cell should release every native reference"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
