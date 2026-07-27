#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPropertyVisibilityTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.Visibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;

	struct FOperationCase
	{
		const ANSICHAR* CatalogName;
		int32 ExpectedValue;
	};

	struct FVisibilityCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* DeclarationPrefix;
		bool bPrivate;
		bool bProtected;
	};

	struct FAccessPathCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FOperationCase OperationCases[] =
	{
		{ "read", 41 },
		{ "write", 67 },
	};

	inline static constexpr FVisibilityCase VisibilityCases[] =
	{
		{ "default", TEXT(""), false, false },
		{ "private", TEXT("private "), true, false },
		{ "protected", TEXT("protected "), false, true },
	};

	inline static constexpr FAccessPathCase AccessPathCases[] =
	{
		{ "owner_method" },
		{ "owner_constructor" },
		{ "owner_destructor" },
		{ "accessor_body" },
		{ "direct_derived" },
		{ "deep_derived" },
		{ "unrelated_type" },
		{ "global_same_module" },
		{ "base_view_from_derived" },
		{ "derived_view_from_global" },
	};

	static bool IsNamed(const ANSICHAR* Value, const ANSICHAR* Expected)
	{
		return FCStringAnsi::Strcmp(Value, Expected) == 0;
	}

	static bool IsRead(const FOperationCase& OperationCase)
	{
		return IsNamed(OperationCase.CatalogName, "read");
	}

	static bool IsPath(const FAccessPathCase& AccessPathCase, const ANSICHAR* Name)
	{
		return IsNamed(AccessPathCase.CatalogName, Name);
	}

	static bool IsOwnerPath(const FAccessPathCase& AccessPathCase)
	{
		return IsPath(AccessPathCase, "owner_method")
			|| IsPath(AccessPathCase, "owner_constructor")
			|| IsPath(AccessPathCase, "owner_destructor")
			|| IsPath(AccessPathCase, "accessor_body");
	}

	static bool IsDerivedPath(const FAccessPathCase& AccessPathCase)
	{
		return IsPath(AccessPathCase, "direct_derived")
			|| IsPath(AccessPathCase, "deep_derived")
			|| IsPath(AccessPathCase, "base_view_from_derived");
	}

	static bool ShouldCompile(
		const FVisibilityCase& VisibilityCase,
		const FAccessPathCase& AccessPathCase)
	{
		if (IsOwnerPath(AccessPathCase))
		{
			return true;
		}
		if (IsDerivedPath(AccessPathCase))
		{
			return !VisibilityCase.bPrivate;
		}
		return !VisibilityCase.bPrivate && !VisibilityCase.bProtected;
	}

	inline static int32 DestructorObservation = -1;

	static void RecordPropertyVisibilityDestructor(const int32 Value)
	{
		DestructorObservation = Value;
	}

	static int32 ReadPropertyVisibilityDestructor()
	{
		return DestructorObservation;
	}

	static bool RegisterObservationBridge(asIScriptEngine& Engine)
	{
		const ASAutoCaller::FunctionCaller RecordCaller =
			ASAutoCaller::MakeFunctionCaller(RecordPropertyVisibilityDestructor);
		const ASAutoCaller::FunctionCaller ReadCaller =
			ASAutoCaller::MakeFunctionCaller(ReadPropertyVisibilityDestructor);
		return Engine.RegisterGlobalFunction(
			"void RecordPropertyVisibilityDestructor(int Value)",
			asFUNCTION(RecordPropertyVisibilityDestructor),
			asCALL_CDECL,
			*(asFunctionCaller*)&RecordCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"int ReadPropertyVisibilityDestructor()",
				asFUNCTION(ReadPropertyVisibilityDestructor),
				asCALL_CDECL,
				*(asFunctionCaller*)&ReadCaller) >= 0;
	}

	static FString MakeSuffix(
		const FOperationCase& OperationCase,
		const FVisibilityCase& VisibilityCase,
		const FAccessPathCase& AccessPathCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			AccessPathCase.CatalogName,
			OperationCase.CatalogName,
			VisibilityCase.CatalogName);
	}

	static void AppendReadOrWrite(
		FString& Source,
		const FOperationCase& OperationCase,
		const FString& ReceiverExpression,
		const int32 IndentLevel,
		const bool bReturnValue)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Indent = FString::ChrN(IndentLevel, TEXT('\t'));
		if (IsRead(OperationCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sint Observed = %sVisibleValue;"),
				*Indent,
				*ReceiverExpression));
			if (bReturnValue)
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("return Observed;"));
			}
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%sVisibleValue = %d;"),
				*Indent,
				*ReceiverExpression,
				OperationCase.ExpectedValue));
			if (bReturnValue)
			{
				AppendGeneratedAsLine(Source, Indent + TEXT("return ReadVisibleValue();"));
			}
		}
	}

	static void AppendOwnerType(
		FString& Source,
		const FOperationCase& OperationCase,
		const FVisibilityCase& VisibilityCase,
		const FAccessPathCase& AccessPathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FVisibilityOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%sint VisibleValue = 41;"),
			VisibilityCase.DeclarationPrefix));
		AppendGeneratedAsLine(Source, TEXT("\tint ConstructorObservation = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint ReadVisibleValue() const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn VisibleValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));

		if (IsPath(AccessPathCase, "owner_method"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint ProbeOwnerMethod()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, true);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsPath(AccessPathCase, "owner_constructor"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, false);
			AppendGeneratedAsLine(Source, IsRead(OperationCase)
				? TEXT("\t\tConstructorObservation = Observed;")
				: TEXT("\t\tConstructorObservation = ReadVisibleValue();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsPath(AccessPathCase, "owner_destructor"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FVisibilityOwner()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, false);
			AppendGeneratedAsLine(Source, IsRead(OperationCase)
				? TEXT("\t\tRecordPropertyVisibilityDestructor(Observed);")
				: TEXT("\t\tRecordPropertyVisibilityDestructor(ReadVisibleValue());"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsPath(AccessPathCase, "accessor_body"))
		{
			AppendGeneratedAsLine(Source);
			if (IsRead(OperationCase))
			{
				AppendGeneratedAsLine(Source, TEXT("\tint ExplicitReadAccessor() const"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, true);
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tvoid ExplicitWriteAccessor()"));
				AppendGeneratedAsLine(Source, TEXT("\t{"));
				AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, false);
				AppendGeneratedAsLine(Source, TEXT("\t}"));
			}
		}
		if (!IsPath(AccessPathCase, "owner_constructor"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDerivedTypes(
		FString& Source,
		const FOperationCase& OperationCase,
		const FAccessPathCase& AccessPathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FVisibilityDerived : FVisibilityOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (IsPath(AccessPathCase, "direct_derived"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint ProbeDirectDerived()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, true);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsPath(AccessPathCase, "base_view_from_derived"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint ProbeBaseView(FVisibilityOwner& inout BaseView)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, OperationCase, TEXT("BaseView."), 2, false);
			AppendGeneratedAsLine(Source, IsRead(OperationCase)
				? TEXT("\t\treturn Observed;")
				: TEXT("\t\treturn BaseView.ReadVisibleValue();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("class FVisibilityDeepDerived : FVisibilityDerived"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDeepDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (IsPath(AccessPathCase, "deep_derived"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint ProbeDeepDerived()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendReadOrWrite(Source, OperationCase, TEXT(""), 2, true);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendUnrelatedType(
		FString& Source,
		const FOperationCase& OperationCase,
		const FAccessPathCase& AccessPathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!IsPath(AccessPathCase, "unrelated_type"))
		{
			return;
		}
		AppendGeneratedAsLine(Source, TEXT("class FVisibilityUnrelated"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFVisibilityUnrelated()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint ProbeUnrelated(FVisibilityOwner& inout Receiver)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendReadOrWrite(Source, OperationCase, TEXT("Receiver."), 2, false);
		AppendGeneratedAsLine(Source, IsRead(OperationCase)
			? TEXT("\t\treturn Observed;")
			: TEXT("\t\treturn Receiver.ReadVisibleValue();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRunFunction(
		FString& Source,
		const FOperationCase& OperationCase,
		const FAccessPathCase& AccessPathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunPropertyVisibility()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsPath(AccessPathCase, "owner_method"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner Receiver = FVisibilityOwner();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ProbeOwnerMethod();"));
		}
		else if (IsPath(AccessPathCase, "owner_constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner Receiver = FVisibilityOwner();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ConstructorObservation;"));
		}
		else if (IsPath(AccessPathCase, "owner_destructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tFVisibilityOwner Receiver = FVisibilityOwner();"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ReadPropertyVisibilityDestructor();"));
		}
		else if (IsPath(AccessPathCase, "accessor_body"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner Receiver = FVisibilityOwner();"));
			if (IsRead(OperationCase))
			{
				AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ExplicitReadAccessor();"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tReceiver.ExplicitWriteAccessor();"));
				AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ReadVisibleValue();"));
			}
		}
		else if (IsPath(AccessPathCase, "direct_derived"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDerived Receiver = FVisibilityDerived();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ProbeDirectDerived();"));
		}
		else if (IsPath(AccessPathCase, "deep_derived"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDeepDerived Receiver = FVisibilityDeepDerived();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ProbeDeepDerived();"));
		}
		else if (IsPath(AccessPathCase, "unrelated_type"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner Receiver = FVisibilityOwner();"));
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityUnrelated Observer = FVisibilityUnrelated();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Observer.ProbeUnrelated(Receiver);"));
		}
		else if (IsPath(AccessPathCase, "base_view_from_derived"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityDerived Receiver = FVisibilityDerived();"));
			AppendGeneratedAsLine(Source, TEXT("\tFVisibilityOwner BaseView = Receiver;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.ProbeBaseView(BaseView);"));
		}
		else
		{
			const bool bDerivedReceiver = IsPath(AccessPathCase, "derived_view_from_global");
			AppendGeneratedAsLine(Source, bDerivedReceiver
				? TEXT("\tFVisibilityDerived Receiver = FVisibilityDerived();")
				: TEXT("\tFVisibilityOwner Receiver = FVisibilityOwner();"));
			AppendReadOrWrite(Source, OperationCase, TEXT("Receiver."), 1, false);
			AppendGeneratedAsLine(Source, IsRead(OperationCase)
				? TEXT("\treturn Observed;")
				: TEXT("\treturn Receiver.ReadVisibleValue();"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildPropertyVisibilitySource(
		const FOperationCase& OperationCase,
		const FVisibilityCase& VisibilityCase,
		const FAccessPathCase& AccessPathCase)
	{
		FString Source;
		AppendOwnerType(Source, OperationCase, VisibilityCase, AccessPathCase);
		AppendDerivedTypes(Source, OperationCase, AccessPathCase);
		AppendUnrelatedType(Source, OperationCase, AccessPathCase);
		AppendRunFunction(Source, OperationCase, AccessPathCase);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyVisibilityRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 109;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedVisibilityError(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const FVisibilityCase& VisibilityCase,
		const FAccessPathCase& AccessPathCase)
	{
		const bool bInheritedPrivateAccess = VisibilityCase.bPrivate
			&& (IsPath(AccessPathCase, "direct_derived")
				|| IsPath(AccessPathCase, "deep_derived"));
		const FString Expected = bInheritedPrivateAccess
			? TEXT("Illegal access to inherited private property 'VisibleValue'")
			: VisibilityCase.bPrivate
				? TEXT("Illegal access to private property 'VisibleValue'")
				: TEXT("Illegal access to protected property 'VisibleValue'");
		return Messages.Entries.ContainsByPredicate([&Expected](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Section.IsEmpty()
				&& Entry.Message.Contains(Expected);
		});
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FVisibilityCase& VisibilityCase,
		asIScriptModule& Module)
	{
		asITypeInfo* const OwnerType = Module.GetTypeInfoByName("FVisibilityOwner");
		asITypeInfo* const DerivedType = Module.GetTypeInfoByName("FVisibilityDerived");
		asITypeInfo* const DeepDerivedType = Module.GetTypeInfoByName("FVisibilityDeepDerived");
		ASSERT_THAT(IsNotNull(OwnerType,
			*Case.Describe(TEXT("visibility cell should publish its owner type"))));
		ASSERT_THAT(IsNotNull(DerivedType,
			*Case.Describe(TEXT("visibility cell should publish its direct derived type"))));
		ASSERT_THAT(IsNotNull(DeepDerivedType,
			*Case.Describe(TEXT("visibility cell should publish its deep derived type"))));
		if (OwnerType == nullptr)
		{
			return;
		}

		bool bFound = false;
		for (asUINT PropertyIndex = 0; PropertyIndex < OwnerType->GetPropertyCount(); ++PropertyIndex)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			bool bPrivate = false;
			bool bProtected = false;
			if (OwnerType->GetProperty(
				PropertyIndex,
				&Name,
				&TypeId,
				&bPrivate,
				&bProtected) >= 0
				&& Name != nullptr
				&& IsNamed(Name, "VisibleValue"))
			{
				bFound = true;
				ASSERT_THAT(AreEqual(asTYPEID_INT32, TypeId,
					*Case.Describe(TEXT("visible field should retain int metadata"))));
				ASSERT_THAT(AreEqual(VisibilityCase.bPrivate, bPrivate,
					*Case.Describe(TEXT("visible field should retain its private metadata flag"))));
				ASSERT_THAT(AreEqual(VisibilityCase.bProtected, bProtected,
					*Case.Describe(TEXT("visible field should retain its protected metadata flag"))));
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFound,
			*Case.Describe(TEXT("owner metadata should expose the exact VisibleValue field"))));
		if (DerivedType != nullptr)
		{
			ASSERT_THAT(AreEqual(OwnerType, DerivedType->GetBaseType(),
				*Case.Describe(TEXT("direct derived metadata should identify the owner base"))));
		}
		if (DerivedType != nullptr && DeepDerivedType != nullptr)
		{
			ASSERT_THAT(AreEqual(DerivedType, DeepDerivedType->GetBaseType(),
				*Case.Describe(TEXT("deep derived metadata should preserve both inheritance levels"))));
		}
	}

	void ExecuteLegalCell(
		const FNativeCaseContext& Case,
		const FOperationCase& OperationCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunPropertyVisibility()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("legal visibility cell should expose its exact entry declaration"))));
		if (Entry == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("legal visibility cell should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		const int ExecuteResult = PrepareAndExecute(Context, Entry);
		const char* const ExceptionText = Context->GetExceptionString();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
			*FString::Printf(TEXT("%s. ExecuteResult=%d Exception=%s"),
				*Case.Describe(TEXT("legal visibility access should finish")),
				ExecuteResult,
				UTF8_TO_TCHAR(ExceptionText != nullptr ? ExceptionText : ""))));
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			return;
		}
		ASSERT_THAT(AreEqual(OperationCase.ExpectedValue, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("visibility result should prove the selected read or completed write"))));
	}

	void CompileRecovery(
		const FNativeCaseContext& Case,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-RECOVERY"), ModuleName, RecoverySource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("rejected visibility access should permit a same-name recovery build"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("visibility recovery should publish a clean module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery = RecoveryModule->GetFunctionByDecl("int RunPropertyVisibilityRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("visibility recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("visibility recovery should create a context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("visibility recovery should execute"))));
					ASSERT_THAT(AreEqual(109, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("visibility recovery should contain no failed-build state"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

public:
	TEST_METHOD(OperationsByVisibilityAndAccessPath)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-VISIBILITY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Property-visibility product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterObservationBridge(*ScriptEngine),
			TEXT("Property-visibility product should register its destructor observation bridge")));
		for (const FOperationCase& OperationCase : OperationCases)
		{
			for (const FVisibilityCase& VisibilityCase : VisibilityCases)
			{
				for (const FAccessPathCase& AccessPathCase : AccessPathCases)
				{
					DestructorObservation = -1;
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-PROP-VISIBILITY",
						{
							ANSI_TO_TCHAR(AccessPathCase.CatalogName),
							ANSI_TO_TCHAR(OperationCase.CatalogName),
							ANSI_TO_TCHAR(VisibilityCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(OperationCase, VisibilityCase, AccessPathCase);
					const FString ModuleName = TEXT("PropertyVisibility_") + Suffix;
					const FString Source = BuildPropertyVisibilitySource(
						OperationCase,
						VisibilityCase,
						AccessPathCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					const bool bShouldCompile = ShouldCompile(VisibilityCase, AccessPathCase);
					if (bShouldCompile)
					{
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*FString::Printf(TEXT("%s. BuildResult=%d Messages={%s}"),
								*Case.Describe(TEXT("legal property-visibility cell should compile")),
								BuildResult,
								*Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("legal property-visibility cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							VerifyMetadata(Case, VisibilityCase, *Module);
							ExecuteLegalCell(Case, OperationCase, *ScriptEngine, *Module);
						}
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*FString::Printf(TEXT("%s. BuildResult=%d Messages={%s}"),
								*Case.Describe(TEXT("inaccessible property-visibility cell should be rejected")),
								BuildResult,
								*Engine.GetMessagesText())));
						ASSERT_THAT(IsTrue(HasLocatedVisibilityError(Engine.GetMessages(), VisibilityCase, AccessPathCase),
							*Case.Describe(TEXT("inaccessible field should report its exact located visibility diagnostic"))));
						if (Module != nullptr)
						{
							ASSERT_THAT(IsNull(Module->GetFunctionByDecl("int RunPropertyVisibility()"),
								*Case.Describe(TEXT("failed visibility build should publish no callable probe"))));
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("property-visibility cell should discard its isolated module"))));
					if (!bShouldCompile)
					{
						CompileRecovery(Case, Engine, *ScriptEngine, ModuleName);
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("visibility recovery should leave no module behind"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
