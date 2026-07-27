#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariableReferenceInitializationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.ReferenceInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FReferenceTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		bool bNative;
	};

	struct FReferenceSourceCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FReferenceDeclarationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FReferenceUseCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FReferenceTypeCase TypeCases[] =
	{
		{ "script_reference", "FScriptCaseReference", false },
		{ "native_reference", "FNativeCaseReference", true },
	};

	inline static constexpr FReferenceSourceCase SourceCases[] =
	{
		{ "constructed_local" },
		{ "parameter" },
		{ "function_return" },
		{ "field" },
		{ "null" },
	};

	inline static constexpr FReferenceDeclarationCase DeclarationCases[] =
	{
		{ "explicit_type" },
		{ "auto" },
		{ "const_view" },
	};

	inline static constexpr FReferenceUseCase UseCases[] =
	{
		{ "identity" },
		{ "mutation" },
		{ "argument" },
		{ "return" },
		{ "null_compare" },
	};

	static bool IsSource(const FReferenceSourceCase& SourceCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(SourceCase.CatalogName, Name) == 0;
	}

	static bool IsDeclaration(const FReferenceDeclarationCase& DeclarationCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(DeclarationCase.CatalogName, Name) == 0;
	}

	static bool IsUse(const FReferenceUseCase& UseCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(UseCase.CatalogName, Name) == 0;
	}

	static FString MakeSuffix(
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs_%hs"),
			TypeCase.CatalogName,
			SourceCase.CatalogName,
			DeclarationCase.CatalogName,
			UseCase.CatalogName);
	}

	static int32 BeginScriptCaseReference(const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			return INDEX_NONE;
		}
		const int32 ObjectId = Recorder->AllocateObjectId();
		Recorder->Record(ENativeLifecycleEvent::ValueConstruct, ObjectId, INDEX_NONE, Value);
		return ObjectId;
	}

	static void EndScriptCaseReference(const int32 ObjectId, const int32 Value)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder();
		if (Recorder != nullptr && ObjectId != INDEX_NONE)
		{
			Recorder->Record(ENativeLifecycleEvent::Destruct, ObjectId, INDEX_NONE, Value);
		}
	}

	static bool RegisterScriptReferenceLifecycle(asIScriptEngine& Engine)
	{
		const ASAutoCaller::FunctionCaller BeginCaller = ASAutoCaller::MakeFunctionCaller(BeginScriptCaseReference);
		const ASAutoCaller::FunctionCaller EndCaller = ASAutoCaller::MakeFunctionCaller(EndScriptCaseReference);
		return Engine.RegisterGlobalFunction(
			"int BeginScriptCaseReference(int Value)",
			asFUNCTION(BeginScriptCaseReference),
			asCALL_CDECL,
			*(asFunctionCaller*)&BeginCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void EndScriptCaseReference(int ObjectId, int Value)",
				asFUNCTION(EndScriptCaseReference),
				asCALL_CDECL,
				*(asFunctionCaller*)&EndCaller) >= 0;
	}

	static FString MakeFreshExpression(const FReferenceTypeCase& TypeCase)
	{
		return TypeCase.bNative
			? TEXT("CreateNativeCaseReference(11)")
			: TEXT("FScriptCaseReference(11)");
	}

	static FString MakeVariableDeclaration(
		const FReferenceTypeCase& TypeCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FString& Initializer,
		const TCHAR* Prefix = TEXT("\t"))
	{
		if (IsDeclaration(DeclarationCase, "auto"))
		{
			return FString::Printf(TEXT("%sauto Variable = %s;"), Prefix, *Initializer);
		}
		return FString::Printf(
			TEXT("%s%s%hs Variable = %s;"),
			Prefix,
			IsDeclaration(DeclarationCase, "const_view") ? TEXT("const ") : TEXT(""),
			TypeCase.ScriptType,
			*Initializer);
	}

	static bool ShouldCompile(
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase)
	{
		if (IsSource(SourceCase, "null") && IsDeclaration(DeclarationCase, "auto"))
		{
			return false;
		}
		return !(IsDeclaration(DeclarationCase, "const_view") && IsUse(UseCase, "mutation"));
	}

	static bool ShouldRaiseNullException(
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase)
	{
		return IsSource(SourceCase, "null")
			&& IsDeclaration(DeclarationCase, "explicit_type")
			&& IsUse(UseCase, "mutation");
	}

	static void AppendScriptReferenceDeclaration(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FScriptCaseReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptCaseReference(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseReference(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptCaseReference(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseReference()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndScriptCaseReference(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendPassThroughHelper(
		FString& Source,
		const FReferenceTypeCase& TypeCase,
		const FReferenceDeclarationCase& DeclarationCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s%hs PassVariableReference(%s%hs Value)"),
			IsDeclaration(DeclarationCase, "const_view") ? TEXT("const ") : TEXT(""),
			TypeCase.ScriptType,
			IsDeclaration(DeclarationCase, "const_view") ? TEXT("const ") : TEXT(""),
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendObservationHelper(
		FString& Source,
		const FReferenceTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ObserveVariableReference(const %hs Candidate, const %hs Expected)"),
			TypeCase.ScriptType,
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Candidate != Expected)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn -1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tif (Candidate == nullptr)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Candidate.Value == 11 ? 1 : -2;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendHolder(
		FString& Source,
		const FReferenceTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FVariableReferenceHolder"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs Stored;"), TypeCase.ScriptType));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFVariableReferenceHolder()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tStored = %s;"),
			*MakeFreshExpression(TypeCase)));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendVariableUse(
		FString& Source,
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase,
		const TCHAR* Prefix)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsUse(UseCase, "identity"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn Variable == Original ? 1 : -1;"),
				Prefix));
		}
		else if (IsUse(UseCase, "mutation"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sVariable.Value += 5;"), Prefix));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn Original.Value == 16 && Variable.Value == 16 ? 1 : -1;"),
				Prefix));
		}
		else if (IsUse(UseCase, "argument"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn ObserveVariableReference(Variable, Original);"),
				Prefix));
		}
		else if (IsUse(UseCase, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s%hs Returned = PassVariableReference(Variable);"),
				Prefix,
				IsDeclaration(DeclarationCase, "const_view") ? TEXT("const ") : TEXT(""),
				TypeCase.ScriptType));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn Returned == Original ? 1 : -1;"),
				Prefix));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn (Variable == nullptr) == %s ? 1 : -1;"),
				Prefix,
				IsSource(SourceCase, "null") ? TEXT("true") : TEXT("false")));
		}
	}

	static void AppendVariableAndUse(
		FString& Source,
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase,
		const FString& Initializer,
		const TCHAR* Prefix)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, MakeVariableDeclaration(
			TypeCase,
			DeclarationCase,
			Initializer,
			Prefix));
		AppendVariableUse(Source, TypeCase, SourceCase, DeclarationCase, UseCase, Prefix);
	}

	static void AppendParameterSourceFunction(
		FString& Source,
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ExerciseVariableReference(%hs Original)"),
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendVariableAndUse(
			Source,
			TypeCase,
			SourceCase,
			DeclarationCase,
			UseCase,
			TEXT("Original"),
			TEXT("\t"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("int RunVariableReferenceCell()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%hs Original = %s;"),
			TypeCase.ScriptType,
			*MakeFreshExpression(TypeCase)));
		AppendGeneratedAsLine(Source, TEXT("\treturn ExerciseVariableReference(Original);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendDirectSourceFunction(
		FString& Source,
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableReferenceCell()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		FString Initializer;
		if (IsSource(SourceCase, "field"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFVariableReferenceHolder Holder = FVariableReferenceHolder();"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs Original = Holder.Stored;"),
				TypeCase.ScriptType));
			Initializer = TEXT("Holder.Stored");
		}
		else if (IsSource(SourceCase, "null"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs Original = nullptr;"),
				TypeCase.ScriptType));
			Initializer = TEXT("nullptr");
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs Original = %s;"),
				TypeCase.ScriptType,
				*MakeFreshExpression(TypeCase)));
			Initializer = IsSource(SourceCase, "function_return")
				? TEXT("PassVariableReference(Original)")
				: TEXT("Original");
		}
		AppendVariableAndUse(
			Source,
			TypeCase,
			SourceCase,
			DeclarationCase,
			UseCase,
			Initializer,
			TEXT("\t"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildReferenceInitializationSource(
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase,
		const FReferenceUseCase& UseCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendScriptReferenceDeclaration(Source);
		if (IsSource(SourceCase, "field"))
		{
			AppendHolder(Source, TypeCase);
		}
		if (IsSource(SourceCase, "function_return") || IsUse(UseCase, "return"))
		{
			AppendPassThroughHelper(Source, TypeCase, DeclarationCase);
		}
		if (IsUse(UseCase, "argument"))
		{
			AppendObservationHelper(Source, TypeCase);
		}
		if (IsSource(SourceCase, "parameter"))
		{
			AppendParameterSourceFunction(Source, TypeCase, SourceCase, DeclarationCase, UseCase);
		}
		else
		{
			AppendDirectSourceFunction(Source, TypeCase, SourceCase, DeclarationCase, UseCase);
		}
		AppendGeneratedAsLine(Source, TEXT("int RunVariableReferenceRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunVariableReferenceRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const FString& Section)
	{
		return Messages.Entries.ContainsByPredicate(
			[&Section](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	static asIScriptFunction* FindMetadataFunction(
		asIScriptModule& Module,
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase)
	{
		if (!IsSource(SourceCase, "parameter"))
		{
			return Module.GetFunctionByDecl("int RunVariableReferenceCell()");
		}
		const FString Declaration = FString::Printf(
			TEXT("int ExerciseVariableReference(%hs Original)"),
			TypeCase.ScriptType);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		return Module.GetFunctionByDecl(DeclarationUtf8.Get());
	}

	void VerifyVariableMetadata(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FReferenceTypeCase& TypeCase,
		const FReferenceSourceCase& SourceCase,
		const FReferenceDeclarationCase& DeclarationCase)
	{
		asIScriptFunction* const Function = FindMetadataFunction(Module, TypeCase, SourceCase);
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("reference-variable metadata owner should resolve by exact declaration"))));
		if (Function == nullptr)
		{
			return;
		}

		const FString ExpectedTypeDeclaration = FString::Printf(
			TEXT("%s%hs"),
			IsDeclaration(DeclarationCase, "const_view") ? TEXT("const ") : TEXT(""),
			TypeCase.ScriptType);
		const FTCHARToUTF8 ExpectedTypeUtf8(*ExpectedTypeDeclaration);
		const int32 ExpectedTypeId = Module.GetTypeIdByDecl(ExpectedTypeUtf8.Get());
		ASSERT_THAT(IsTrue(ExpectedTypeId >= 0,
			*Case.Describe(TEXT("reference-variable expected type declaration should resolve within its defining module"))));

		int32 FoundCount = 0;
		for (asUINT Index = 0; Index < Function->GetVarCount(); ++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			if (Function->GetVar(Index, &Name, &TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Variable") == 0)
			{
				++FoundCount;
				ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
					*Case.Describe(TEXT("reference-variable metadata should preserve explicit, inferred, or const type"))));
				const char* const VariableDeclaration = Function->GetVarDecl(Index, true);
				ASSERT_THAT(IsTrue(VariableDeclaration != nullptr
					&& FString(UTF8_TO_TCHAR(VariableDeclaration)).Contains(UTF8_TO_TCHAR(TypeCase.ScriptType)),
					*Case.Describe(TEXT("reference-variable debug declaration should name the resolved reference type"))));
				if (IsDeclaration(DeclarationCase, "const_view") && VariableDeclaration != nullptr)
				{
					ASSERT_THAT(IsTrue(FString(UTF8_TO_TCHAR(VariableDeclaration)).Contains(TEXT("const")),
						*Case.Describe(TEXT("const reference-variable debug declaration should retain constness"))));
				}
			}
		}
		ASSERT_THAT(AreEqual(1, FoundCount,
			*Case.Describe(TEXT("reference-variable metadata owner should contain exactly one Variable local"))));
	}

	void ExecuteRecovery(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptContext& Context,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(
			&Module,
			"int RunVariableReferenceRecovery()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("reference-variable module should publish its exact recovery declaration"))));
		if (Recovery == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Context.Unprepare() >= 0,
			*Case.Describe(TEXT("reference-variable context should unprepare before recovery"))));
		ASSERT_THAT(IsTrue(Context.Prepare(Recovery) >= 0,
			*Case.Describe(TEXT("reference-variable context should prepare its recovery function"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context.Execute(),
			*Case.Describe(TEXT("reference-variable recovery should finish in the reused context"))));
		ASSERT_THAT(AreEqual(97, static_cast<int32>(Context.GetReturnDWord()),
			*Case.Describe(TEXT("reference-variable recovery should return its clean sentinel"))));
	}

public:
	TEST_METHOD(TypesBySourceDeclarationAndUse)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-REFERENCE-INIT",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Reference-variable product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			TEXT("Reference-variable product should register its lifecycle-aware native reference type")));
		ASSERT_THAT(IsTrue(RegisterScriptReferenceLifecycle(*ScriptEngine),
			TEXT("Reference-variable product should register script-reference lifecycle callbacks")));

		for (const FReferenceTypeCase& TypeCase : TypeCases)
		{
			for (const FReferenceSourceCase& SourceCase : SourceCases)
			{
				for (const FReferenceDeclarationCase& DeclarationCase : DeclarationCases)
				{
					for (const FReferenceUseCase& UseCase : UseCases)
					{
						Lifecycle.Reset();
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-VAR-REFERENCE-INIT",
							{
								ANSI_TO_TCHAR(DeclarationCase.CatalogName),
								ANSI_TO_TCHAR(SourceCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase.CatalogName),
								ANSI_TO_TCHAR(UseCase.CatalogName),
							}));
						const FString Suffix = MakeSuffix(TypeCase, SourceCase, DeclarationCase, UseCase);
						const FString ModuleName = TEXT("VariableReferenceInitialization_") + Suffix;
						const FString Source = BuildReferenceInitializationSource(
							TypeCase,
							SourceCase,
							DeclarationCase,
							UseCase);
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
						const bool bShouldCompile = ShouldCompile(SourceCase, DeclarationCase, UseCase);
						if (!bShouldCompile)
						{
							ASSERT_THAT(IsTrue(BuildResult < 0,
								*Case.Describe(TEXT("illegal reference declaration or const mutation should be rejected"))));
							ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
								*Case.Describe(TEXT("rejected reference-variable cell should report a located diagnostic"))));
							if (Module != nullptr)
							{
								ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "int RunVariableReferenceCell()"),
									*Case.Describe(TEXT("failed reference-variable build should not publish its entry"))));
							}

							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							const FString RecoverySource = BuildRecoverySource();
							PrintGeneratedAsSource(
								*TestRunner,
								Case.GetId() + TEXT("-RECOVERY"),
								ModuleName,
								RecoverySource);
							const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
							Engine.ResetMessages();
							asIScriptModule* RecoveryModule = nullptr;
							ASSERT_THAT(IsTrue(CompileNativeModule(
								ScriptEngine,
								ModuleNameUtf8.Get(),
								RecoverySourceUtf8.Get(),
								RecoveryModule) >= 0,
								*Case.Describe(TEXT("rejected reference-variable cell should permit same-name recovery"))));
							ASSERT_THAT(IsNotNull(RecoveryModule,
								*Case.Describe(TEXT("reference-variable recovery should publish a clean module"))));
							if (RecoveryModule != nullptr)
							{
								asIScriptContext* const RecoveryContext = ScriptEngine->CreateContext();
								ASSERT_THAT(IsNotNull(RecoveryContext,
									*Case.Describe(TEXT("reference-variable recovery should create a context"))));
								if (RecoveryContext != nullptr)
								{
									asIScriptFunction* const Recovery = GetNativeFunctionByExactDecl(
										RecoveryModule,
										"int RunVariableReferenceRecovery()");
									ASSERT_THAT(IsNotNull(Recovery,
										*Case.Describe(TEXT("clean recovery should expose its exact function"))));
									if (Recovery != nullptr)
									{
										ASSERT_THAT(AreEqual(
											static_cast<int32>(asEXECUTION_FINISHED),
											PrepareAndExecute(RecoveryContext, Recovery),
											*Case.Describe(TEXT("clean reference-variable recovery should execute"))));
										ASSERT_THAT(AreEqual(97, static_cast<int32>(RecoveryContext->GetReturnDWord()),
											*Case.Describe(TEXT("clean reference-variable recovery should return 97"))));
									}
									RecoveryContext->Release();
								}
							}
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						}
						else
						{
							ASSERT_THAT(IsTrue(BuildResult >= 0,
								*Case.Describe(TEXT("legal reference-variable cell should compile"))));
							ASSERT_THAT(IsNotNull(Module,
								*Case.Describe(TEXT("legal reference-variable cell should publish a module"))));
							if (Module != nullptr)
							{
								VerifyVariableMetadata(
									Case,
									*ScriptEngine,
									*Module,
									TypeCase,
									SourceCase,
									DeclarationCase);
								asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(
									Module,
									"int RunVariableReferenceCell()");
								ASSERT_THAT(IsNotNull(Entry,
									*Case.Describe(TEXT("legal reference-variable cell should expose its exact entry"))));
								if (Entry != nullptr)
								{
									asIScriptContext* const Context = ScriptEngine->CreateContext();
									ASSERT_THAT(IsNotNull(Context,
										*Case.Describe(TEXT("legal reference-variable cell should create a context"))));
									if (Context != nullptr)
									{
										ASSERT_THAT(IsTrue(Context->Prepare(Entry) >= 0,
											*Case.Describe(TEXT("reference-variable context should prepare its entry"))));
										const int32 ExecutionResult = Context->Execute();
										if (ShouldRaiseNullException(SourceCase, DeclarationCase, UseCase))
										{
											ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecutionResult,
												*Case.Describe(TEXT("null reference mutation should raise a script exception"))));
											ASSERT_THAT(IsTrue(Context->GetExceptionFunction() != nullptr,
												*Case.Describe(TEXT("null reference exception should retain function metadata"))));
											int ExceptionColumn = 0;
											const char* ExceptionSection = nullptr;
											ASSERT_THAT(IsTrue(Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection) > 0,
												*Case.Describe(TEXT("null reference exception should retain a source line"))));
											ASSERT_THAT(IsTrue(ExceptionColumn > 0,
												*Case.Describe(TEXT("null reference exception should retain a source column"))));
											ASSERT_THAT(IsTrue(ExceptionSection != nullptr
												&& FString(UTF8_TO_TCHAR(ExceptionSection)) == ModuleName,
												*Case.Describe(TEXT("null reference exception should retain the generated module section"))));
											ExecuteRecovery(Case, *Context, *Module);
										}
										else
										{
											ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecutionResult,
												*Case.Describe(TEXT("legal reference-variable cell should finish"))));
											ASSERT_THAT(AreEqual(1, static_cast<int32>(Context->GetReturnDWord()),
												*Case.Describe(TEXT("legal reference-variable cell should preserve identity and value"))));
										}
										Context->Unprepare();
										Context->Release();
									}
								}
							}
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						}

						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("reference-variable cell should leave no module behind"))));
						ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
							*Case.Describe(TEXT("reference-variable cell should leave no live script or native references"))));
						if (bShouldCompile && !IsSource(SourceCase, "null"))
						{
							ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct) > 0,
								*Case.Describe(TEXT("non-null reference-variable cell should record construction"))));
							ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Destruct) > 0,
								*Case.Describe(TEXT("non-null reference-variable cell should record destruction"))));
							if (TypeCase.bNative)
							{
								ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::AddRef) > 0,
									*Case.Describe(TEXT("native reference-variable cell should record retained aliases"))));
								ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Release) > 0,
									*Case.Describe(TEXT("native reference-variable cell should release retained aliases"))));
							}
						}
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
