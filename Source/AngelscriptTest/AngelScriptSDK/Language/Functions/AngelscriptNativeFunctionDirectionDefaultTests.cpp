#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

using namespace AngelscriptNativeTestSupport;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionDirectionDefaultTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.DirectionDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FDirectionCase
	{
		const ANSICHAR* Name;
		const ANSICHAR* DeclarationSuffix;
		asDWORD ExpectedFlags;
		bool bReads;
		bool bWrites;
	};

	struct FDefaultStateCase
	{
		const ANSICHAR* Name;
		bool bHasDefault;
		bool bOmitCall;
		bool bExpectedCompile;
	};

	struct FTargetCase
	{
		const ANSICHAR* Name;
	};

	inline static constexpr FDirectionCase DirectionCases[] =
	{
		{ "value", "", asTM_CONST, true, false },
		{ "in", "& in", asTM_INREF, true, false },
		{ "out", "& out", asTM_OUTREF, false, true },
		{ "inout", "& inout", asTM_INOUTREF, true, true },
	};

	inline static constexpr FDefaultStateCase DefaultStateCases[] =
	{
		{ "none_explicit", false, false, true },
		{ "none_omitted", false, true, false },
		{ "present_explicit", true, false, true },
		{ "present_omitted", true, true, true },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "global" },
		{ "namespace_global" },
		{ "instance_method" },
	};


	static bool IsPrimitiveType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger
			|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
			|| TypeCase.Category == ENativeValueCategory::FloatingPoint
			|| TypeCase.Category == ENativeValueCategory::Boolean;
	}

	static FString TypeName(const FNativeTypeCase& TypeCase)
	{
		// The fork keeps the legacy float/double spellings but resolves bare
		// float according to the engine policy. Use the explicit spellings so
		// these primitive cells characterize both widths independently.
		if (FCStringAnsi::Strcmp(TypeCase.CatalogName, "float32") == 0)
		{
			return TEXT("float32");
		}
		if (FCStringAnsi::Strcmp(TypeCase.CatalogName, "float64") == 0)
		{
			return TEXT("float64");
		}
		return ANSI_TO_TCHAR(TypeCase.ScriptType);
	}

	static bool ShouldCompile(
		const FNativeTypeCase& TypeCase,
		const FDirectionCase& DirectionCase,
		const FDefaultStateCase& DefaultStateCase)
	{
		if (!DefaultStateCase.bExpectedCompile)
		{
			return false;
		}

		// In this fork, a default value can satisfy a by-value or read-only in
		// reference parameter when the call omits it. Out and inout references
		// still require an addressable argument.
		if (!DefaultStateCase.bOmitCall || !DefaultStateCase.bHasDefault)
		{
			return true;
		}
		if (FCStringAnsi::Strcmp(DirectionCase.Name, "value") == 0)
		{
			return true;
		}
		if (FCStringAnsi::Strcmp(DirectionCase.Name, "in") != 0)
		{
			return false;
		}

		// The fork only applies the omitted default when the literal already
		// has the parameter's exact primitive type. Narrow integer and unsigned
		// cells therefore remain negative characterization cases for now.
		return FCStringAnsi::Strcmp(TypeCase.CatalogName, "int") == 0
			|| FCStringAnsi::Strcmp(TypeCase.CatalogName, "float32") == 0
			|| FCStringAnsi::Strcmp(TypeCase.CatalogName, "float64") == 0
			|| FCStringAnsi::Strcmp(TypeCase.CatalogName, "bool") == 0;
	}

	static FString MakeSuffix(
		const FNativeTypeCase& TypeCase,
		const FDirectionCase& DirectionCase,
		const FDefaultStateCase& DefaultStateCase,
		const FTargetCase& TargetCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs_%hs"),
			TypeCase.CatalogName,
			DirectionCase.Name,
			DefaultStateCase.Name,
			TargetCase.Name);
	}

	static FString MakeModuleName(const FString& Suffix)
	{
		return TEXT("FunctionDirectionDefault_") + Suffix;
	}

	static FString MakeProbeName(const FString& Suffix)
	{
		return TEXT("Probe_") + Suffix;
	}

	static FString MakeEntryName(const FString& Suffix)
	{
		return TEXT("Entry_") + Suffix;
	}

	static FString MakeReadExpression(const FNativeTypeCase& TypeCase, const TCHAR* ValueName, const FString& ExpectedLiteral)
	{
		return FString::Printf(
			TEXT("(%s == %s)"),
			ValueName,
			*ExpectedLiteral);
	}

	static void AppendIndentedGeneratedSource(FString& Source, const FString& Body, const int32 IndentLevel)
	{
		TArray<FString> Lines;
		Body.ParseIntoArrayLines(Lines, false);
		FString Prefix;
		for (int32 IndentIndex = 0; IndentIndex < IndentLevel; ++IndentIndex)
		{
			Prefix += TEXT("\t");
		}
		for (const FString& Line : Lines)
		{
			AppendGeneratedAsLine(Source, Line.IsEmpty() ? FString() : Prefix + Line);
		}
	}

	static FString BuildSource(
		const FNativeTypeCase& TypeCase,
		const FDirectionCase& DirectionCase,
		const FDefaultStateCase& DefaultStateCase,
		const FTargetCase& TargetCase)
	{
		const FString Suffix = MakeSuffix(TypeCase, DirectionCase, DefaultStateCase, TargetCase);
		const FString ProbeName = MakeProbeName(Suffix);
		const FString EntryName = MakeEntryName(Suffix);
		const FString ScriptType = TypeName(TypeCase);
		const FString DefaultSuffix = DefaultStateCase.bHasDefault
			? FString::Printf(TEXT(" = %hs"), TypeCase.OneLiteral)
			: FString();
		const FString ExplicitExpected = ANSI_TO_TCHAR(TypeCase.ZeroLiteral);
		const FString OmittedExpected = ANSI_TO_TCHAR(TypeCase.OneLiteral);
		const FString ExpectedRead = DefaultStateCase.bOmitCall ? OmittedExpected : ExplicitExpected;
		const FString Parameter = FString::Printf(
			TEXT("%s%hs Value%s"),
			*ScriptType,
			DirectionCase.DeclarationSuffix,
			*DefaultSuffix);

		FString Source;
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("// type=%hs direction=%hs default_state=%hs target=%hs expected_compile=%s"),
			TypeCase.CatalogName,
			DirectionCase.Name,
			DefaultStateCase.Name,
			TargetCase.Name,
			ShouldCompile(TypeCase, DirectionCase, DefaultStateCase) ? TEXT("true") : TEXT("false")));

		const FString ProbeBody = [&]()
		{
			FString Body;
			if (DirectionCase.bReads)
			{
				AppendGeneratedAsLine(Body, FString::Printf(
					TEXT("\tif (!%s)"),
					*MakeReadExpression(TypeCase, TEXT("Value"), ExpectedRead)));
				AppendGeneratedAsLine(Body, TEXT("\t{"));
				AppendGeneratedAsLine(Body, TEXT("\t\treturn false;"));
				AppendGeneratedAsLine(Body, TEXT("\t}"));
			}
			if (DirectionCase.bWrites)
			{
				AppendGeneratedAsLine(Body, FString::Printf(
					TEXT("\tValue = %hs;"),
					TypeCase.OneLiteral));
			}
			AppendGeneratedAsLine(Body, TEXT("\treturn true;"));
			return Body;
		}();

		const FString ProbeDeclaration = FString::Printf(
			TEXT("bool %s(%s)"),
			*ProbeName,
			*Parameter);
		const FString EntryDeclaration = FString::Printf(TEXT("bool %s()"), *EntryName);

		if (FCStringAnsi::Strcmp(TargetCase.Name, "namespace_global") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("namespace DirectionDefaultNamespace"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\t") + ProbeDeclaration);
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendIndentedGeneratedSource(Source, ProbeBody, 1);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (FCStringAnsi::Strcmp(TargetCase.Name, "instance_method") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FDirectionDefaultReceiver"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\t") + ProbeDeclaration);
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendIndentedGeneratedSource(Source, ProbeBody, 1);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else
		{
			AppendGeneratedAsLine(Source, ProbeDeclaration);
			AppendGeneratedAsLine(Source, TEXT("{"));
			Source += ProbeBody;
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, EntryDeclaration);
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (FCStringAnsi::Strcmp(TargetCase.Name, "instance_method") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFDirectionDefaultReceiver Receiver;"));
		}
		if (!DefaultStateCase.bOmitCall)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Value = %hs;"),
				*ScriptType,
				TypeCase.ZeroLiteral));
		}

		FString Call;
		if (DefaultStateCase.bOmitCall)
		{
			Call = TEXT("()" );
		}
		else
		{
			Call = TEXT("(Value)");
		}

		FString CallExpression;
		if (FCStringAnsi::Strcmp(TargetCase.Name, "namespace_global") == 0)
		{
			CallExpression = TEXT("DirectionDefaultNamespace::") + ProbeName + Call;
		}
		else if (FCStringAnsi::Strcmp(TargetCase.Name, "instance_method") == 0)
		{
			CallExpression = TEXT("Receiver.") + ProbeName + Call;
		}
		else
		{
			CallExpression = ProbeName + Call;
		}

		const FString WritebackSuffix = (!DefaultStateCase.bOmitCall && DirectionCase.bWrites)
			? FString::Printf(TEXT(" && (Value == %hs)"), TypeCase.OneLiteral)
			: FString();
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\treturn %s%s;"),
			*CallExpression,
			*WritebackSuffix));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static asIScriptFunction* FindProbe(
		asIScriptModule& Module,
		const FString& ProbeName,
		const FTargetCase& TargetCase)
	{
		if (FCStringAnsi::Strcmp(TargetCase.Name, "instance_method") == 0)
		{
			asITypeInfo* const Type = Module.GetTypeInfoByName("FDirectionDefaultReceiver");
			return Type != nullptr ? Type->GetMethodByName(TCHAR_TO_ANSI(*ProbeName)) : nullptr;
		}

		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetName(), TCHAR_TO_ANSI(*ProbeName)) == 0
				&& (FCStringAnsi::Strcmp(TargetCase.Name, "global") == 0
					|| FCStringAnsi::Strcmp(Function->GetNamespace(), "DirectionDefaultNamespace") == 0))
			{
				return Function;
			}
		}
		return nullptr;
	}

	static FString ExpectedTargetProbeName(
		const FNativeTypeCase& TypeCase,
		const FDirectionCase& DirectionCase,
		const FDefaultStateCase& DefaultStateCase,
		const FTargetCase& TargetCase)
	{
		return MakeProbeName(MakeSuffix(TypeCase, DirectionCase, DefaultStateCase, TargetCase));
	}

public:
	TEST_METHOD(DirectionAndDefaultArgumentCells)
	{
		AS_NATIVE_PRODUCT("LANG-FN-DIRECTION-DEFAULT",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
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

		FNoDiscardAsserter Assertions(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine,
			TEXT("Function direction/default product should create a raw SDK engine")))
		{
			return;
		}

		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (!IsPrimitiveType(TypeCase))
			{
				continue;
			}

			for (const FDirectionCase& DirectionCase : DirectionCases)
			{
				for (const FDefaultStateCase& DefaultStateCase : DefaultStateCases)
				{
					for (const FTargetCase& TargetCase : TargetCases)
					{
						const FString Suffix = MakeSuffix(TypeCase, DirectionCase, DefaultStateCase, TargetCase);
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-FN-DIRECTION-DEFAULT",
							{
								ANSI_TO_TCHAR(TypeCase.CatalogName),
								ANSI_TO_TCHAR(DirectionCase.Name),
								ANSI_TO_TCHAR(DefaultStateCase.Name),
								ANSI_TO_TCHAR(TargetCase.Name),
							}));
						const FString ModuleName = MakeModuleName(Suffix);
						const FString Source = BuildSource(TypeCase, DirectionCase, DefaultStateCase, TargetCase);
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
						if (!ShouldCompile(TypeCase, DirectionCase, DefaultStateCase))
						{
							(void)Assertions.IsTrue(
								BuildResult < 0,
								*Case.Describe(TEXT("invalid omitted/default direction cell should reject compilation")));
							(void)Assertions.IsTrue(
								!Engine.GetMessagesText().IsEmpty(),
								*Case.Describe(TEXT("invalid direction/default cell should retain a diagnostic")));
							if (BuildResult >= 0)
							{
								(void)Assertions.IsNull(
									Module,
									*Case.Describe(TEXT("rejected direction/default cell should not publish a module")));
							}
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							(void)Assertions.IsNull(
								ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
								*Case.Describe(TEXT("rejected direction/default cell should be discarded")));
							continue;
						}

						(void)Assertions.IsTrue(
							BuildResult >= 0,
							*Case.Describe(TEXT("valid direction/default cell should compile")));
						if (BuildResult < 0 || !Assertions.IsNotNull(
							Module,
							*Case.Describe(TEXT("valid direction/default cell should publish a module"))))
						{
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							continue;
						}

						const FString ProbeName = ExpectedTargetProbeName(TypeCase, DirectionCase, DefaultStateCase, TargetCase);
						asIScriptFunction* const Probe = FindProbe(*Module, ProbeName, TargetCase);
						if (Assertions.IsNotNull(
							Probe,
							*Case.Describe(TEXT("valid direction/default probe should be published"))))
						{
							int TypeId = 0;
							asDWORD Flags = asTM_NONE;
							const char* ParameterName = nullptr;
							const char* DefaultArgument = nullptr;
							(void)Assertions.AreEqual(
								asSUCCESS,
								Probe->GetParam(0, &TypeId, &Flags, &ParameterName, &DefaultArgument),
								*Case.Describe(TEXT("direction/default probe metadata should be queryable")));
							const FTCHARToUTF8 TypeNameUtf8(*TypeName(TypeCase));
							(void)Assertions.AreEqual(
								ScriptEngine->GetTypeIdByDecl(TypeNameUtf8.Get()),
								TypeId,
								*Case.Describe(TEXT("direction/default probe should retain its primitive type")));
							(void)Assertions.AreEqual(
								DirectionCase.ExpectedFlags,
								Flags,
								*Case.Describe(TEXT("direction/default probe should retain its normalized parameter mode")));
							(void)Assertions.AreEqual(
								FString(TEXT("Value")),
								FString(UTF8_TO_TCHAR(ParameterName != nullptr ? ParameterName : "")),
								*Case.Describe(TEXT("direction/default probe should retain the parameter name")));
							const FString ActualDefault = FString(UTF8_TO_TCHAR(DefaultArgument != nullptr ? DefaultArgument : ""));
							const FString ExpectedDefault = DefaultStateCase.bHasDefault
								? FString(ANSI_TO_TCHAR(TypeCase.OneLiteral))
								: FString();
							(void)Assertions.AreEqual(
								ExpectedDefault,
								ActualDefault,
								*Case.Describe(TEXT("direction/default probe should retain default metadata")));
						}

						const FString EntryDeclaration = FString::Printf(
							TEXT("bool %s()"),
							*MakeEntryName(Suffix));
						AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
							*TestRunner,
							ScriptEngine,
							Module,
							TCHAR_TO_ANSI(*EntryDeclaration));
						if (Assertions.IsTrue(
							Invoker.IsValid(),
							*Case.Describe(TEXT("direction/default entry should resolve by exact declaration"))))
						{
							(void)Assertions.IsTrue(
								Invoker.CallAndReturn<bool>(false),
								*Case.Describe(TEXT("direction/default entry should observe transfer and writeback")));
						}

						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						(void)Assertions.IsNull(
							ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("direction/default cell should discard its module")));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
