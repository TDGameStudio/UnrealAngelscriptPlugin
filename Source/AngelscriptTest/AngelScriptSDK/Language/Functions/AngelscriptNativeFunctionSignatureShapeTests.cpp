#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionSignatureShapeTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.SignatureShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FArityCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
	};

	struct FTypePatternCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FDirectionPatternCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FTargetCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FArityCase ArityCases[] =
	{
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
	};

	inline static constexpr FTypePatternCase TypePatternCases[] =
	{
		{ "homogeneous_int" },
		{ "homogeneous_float" },
		{ "alternating_int_float" },
		{ "alternating_float_int" },
	};

	inline static constexpr FDirectionPatternCase DirectionPatternCases[] =
	{
		{ "all_value" },
		{ "all_in" },
		{ "all_out" },
		{ "alternating_inout_out" },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "global" },
		{ "namespace_global" },
		{ "instance_method" },
	};

	static FString MakeSuffix(
		const FArityCase& ArityCase,
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase,
		const FTargetCase& TargetCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs_%hs"),
			ArityCase.CatalogName,
			TypePatternCase.CatalogName,
			DirectionPatternCase.CatalogName,
			TargetCase.CatalogName);
	}

	static bool IsFloatParameter(const FTypePatternCase& TypePatternCase, const int32 Index)
	{
		if (FCStringAnsi::Strcmp(TypePatternCase.CatalogName, "homogeneous_float") == 0)
		{
			return true;
		}
		if (FCStringAnsi::Strcmp(TypePatternCase.CatalogName, "alternating_int_float") == 0)
		{
			return (Index % 2) == 1;
		}
		if (FCStringAnsi::Strcmp(TypePatternCase.CatalogName, "alternating_float_int") == 0)
		{
			return (Index % 2) == 0;
		}
		return false;
	}

	static FString GetParameterType(
		const FTypePatternCase& TypePatternCase,
		const int32 Index)
	{
		return IsFloatParameter(TypePatternCase, Index) ? TEXT("float") : TEXT("int");
	}

	static bool IsWrittenParameter(
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		if (FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_out") == 0)
		{
			return true;
		}
		return FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "alternating_inout_out") == 0;
	}

	static bool IsReadParameter(
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		if (FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_value") == 0
			|| FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_in") == 0)
		{
			return true;
		}
		return FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "alternating_inout_out") == 0
			&& (Index % 2) == 0;
	}

	static FString GetDirectionSuffix(
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		if (FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_value") == 0)
		{
			return FString();
		}
		if (FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_in") == 0)
		{
			return TEXT("& in");
		}
		if (FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_out") == 0)
		{
			return TEXT("& out");
		}
		return (Index % 2) == 0 ? TEXT("& inout") : TEXT("& out");
	}

	static FString GetArgumentExpression(
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		const FString VariableName = FString::Printf(TEXT("A%d"), Index);
		return FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_value") != 0
			? VariableName
			: (IsFloatParameter(TypePatternCase, Index)
				? FString::Printf(TEXT("%d.5f"), Index + 3)
				: FString::FromInt(Index + 3));
	}

	static FString GetInitialStatement(
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		if (FCStringAnsi::Strcmp(DirectionPatternCase.CatalogName, "all_value") == 0)
		{
			return FString();
		}

		const FString VariableName = FString::Printf(TEXT("A%d"), Index);
		const int32 InitialIndex = FCStringAnsi::Strcmp(
			DirectionPatternCase.CatalogName,
			"all_in") == 0
			? Index + 3
			: Index + 7;
		const FString InitialValue = IsFloatParameter(TypePatternCase, Index)
			? FString::Printf(TEXT("%d.5f"), InitialIndex)
			: FString::FromInt(InitialIndex);
		return FString::Printf(
			TEXT("%s %s = %s;"),
			*GetParameterType(TypePatternCase, Index),
			*VariableName,
			*InitialValue);
	}

	static FString GetWrittenValueExpression(
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		const FString ParameterName = FString::Printf(TEXT("P%d"), Index);
		if (!IsReadParameter(DirectionPatternCase, Index))
		{
			return IsFloatParameter(TypePatternCase, Index)
				? FString::Printf(TEXT("%d.0f"), 100 + Index)
				: FString::FromInt(100 + Index);
		}
		if (IsFloatParameter(TypePatternCase, Index))
		{
			return FString::Printf(TEXT("%s + 1.0f"), *ParameterName);
		}
		return FString::Printf(TEXT("%s + 1"), *ParameterName);
	}

	static FString GetExpectedCallerValue(
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase,
		const int32 Index)
	{
		if (!IsReadParameter(DirectionPatternCase, Index))
		{
			return IsFloatParameter(TypePatternCase, Index)
				? FString::Printf(TEXT("%d.0f"), 100 + Index)
				: FString::FromInt(100 + Index);
		}
		const int32 InitialValue = Index + 7;
		if (IsFloatParameter(TypePatternCase, Index))
		{
			return FString::Printf(TEXT("%d.5f"), InitialValue + 1);
		}
		return FString::FromInt(InitialValue + 1);
	}

	static FString MakeParameters(
		const FArityCase& ArityCase,
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase)
	{
		TArray<FString> Parameters;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Parameters.Add(FString::Printf(
				TEXT("%s%s P%d"),
				*GetParameterType(TypePatternCase, Index),
				*GetDirectionSuffix(DirectionPatternCase, Index),
				Index));
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	static FString MakeCallArguments(
		const FArityCase& ArityCase,
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase)
	{
		TArray<FString> Arguments;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Arguments.Add(GetArgumentExpression(TypePatternCase, DirectionPatternCase, Index));
		}
		return FString::Join(Arguments, TEXT(", "));
	}

	static FString MakeReturnExpression(
		const FArityCase& ArityCase,
		const FTypePatternCase& TypePatternCase)
	{
		TArray<FString> Terms;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			const FString ParameterName = FString::Printf(TEXT("P%d"), Index);
			Terms.Add(IsFloatParameter(TypePatternCase, Index)
				? FString::Printf(TEXT("int(%s)"), *ParameterName)
				: ParameterName);
		}
		return FString::Join(Terms, TEXT(" + "));
	}

	static FString MakeCallerCheckExpression(
		const FArityCase& ArityCase,
		const FTypePatternCase& TypePatternCase,
		const FDirectionPatternCase& DirectionPatternCase)
	{
		TArray<FString> Checks;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			if (IsWrittenParameter(DirectionPatternCase, Index))
			{
				const FString VariableName = FString::Printf(TEXT("A%d"), Index);
				const FString ExpectedValue = GetExpectedCallerValue(
					TypePatternCase,
					DirectionPatternCase,
					Index);
				Checks.Add(FString::Printf(TEXT("%s == %s"), *VariableName, *ExpectedValue));
			}
		}
		return Checks.Num() > 0 ? FString::Join(Checks, TEXT(" && ")) : TEXT("true");
	}

	static FString BuildSource()
	{
		FString Source;
		for (const FArityCase& ArityCase : ArityCases)
		{
			for (const FTypePatternCase& TypePatternCase : TypePatternCases)
			{
				for (const FDirectionPatternCase& DirectionPatternCase : DirectionPatternCases)
				{
					for (const FTargetCase& TargetCase : TargetCases)
					{
						const FString Suffix = MakeSuffix(
							ArityCase,
							TypePatternCase,
							DirectionPatternCase,
							TargetCase);
						const FString ProbeName = TEXT("Probe_") + Suffix;
						const FString EntryName = TEXT("Run_") + Suffix;
						const FString Parameters = MakeParameters(
							ArityCase,
							TypePatternCase,
							DirectionPatternCase);
						const FString ReturnExpression = MakeReturnExpression(ArityCase, TypePatternCase);
						const FString CallArguments = MakeCallArguments(
							ArityCase,
							TypePatternCase,
							DirectionPatternCase);
						const FString CallerCheck = MakeCallerCheckExpression(
							ArityCase,
							TypePatternCase,
							DirectionPatternCase);

						if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "global") == 0)
						{
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("int %s(%s)"),
								*ProbeName,
								*Parameters));
							AppendGeneratedAsLine(Source, TEXT("{"));
							for (int32 Index = 0; Index < ArityCase.Count; ++Index)
							{
								if (IsWrittenParameter(DirectionPatternCase, Index))
								{
									AppendGeneratedAsLine(Source, FString::Printf(
										TEXT("	P%d = %s;"),
										Index,
										*GetWrittenValueExpression(
											TypePatternCase,
											DirectionPatternCase,
											Index)));
								}
							}
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("	return %s;"),
								*ReturnExpression));
							AppendGeneratedAsLine(Source, TEXT("}"));
						}
						else if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "namespace_global") == 0)
						{
							const FString NamespaceName = TEXT("N_") + Suffix;
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("namespace %s"),
								*NamespaceName));
							AppendGeneratedAsLine(Source, TEXT("{"));
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("	int %s(%s)"),
								*ProbeName,
								*Parameters));
							AppendGeneratedAsLine(Source, TEXT("	{"));
							for (int32 Index = 0; Index < ArityCase.Count; ++Index)
							{
								if (IsWrittenParameter(DirectionPatternCase, Index))
								{
									AppendGeneratedAsLine(Source, FString::Printf(
										TEXT("		P%d = %s;"),
										Index,
										*GetWrittenValueExpression(
											TypePatternCase,
											DirectionPatternCase,
											Index)));
								}
							}
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("		return %s;"),
								*ReturnExpression));
							AppendGeneratedAsLine(Source, TEXT("	}"));
							AppendGeneratedAsLine(Source, TEXT("}"));
						}
						else
						{
							const FString TypeName = TEXT("FOwner_") + Suffix;
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("struct %s"),
								*TypeName));
							AppendGeneratedAsLine(Source, TEXT("{"));
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("	int %s(%s)"),
								*ProbeName,
								*Parameters));
							AppendGeneratedAsLine(Source, TEXT("	{"));
							for (int32 Index = 0; Index < ArityCase.Count; ++Index)
							{
								if (IsWrittenParameter(DirectionPatternCase, Index))
								{
									AppendGeneratedAsLine(Source, FString::Printf(
										TEXT("		P%d = %s;"),
										Index,
										*GetWrittenValueExpression(
											TypePatternCase,
											DirectionPatternCase,
											Index)));
								}
							}
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("		return %s;"),
								*ReturnExpression));
							AppendGeneratedAsLine(Source, TEXT("	}"));
							AppendGeneratedAsLine(Source, TEXT("}"));
						}

						AppendGeneratedAsLine(Source);
						AppendGeneratedAsLine(Source, FString::Printf(
							TEXT("int %s()"),
							*EntryName));
						AppendGeneratedAsLine(Source, TEXT("{"));
						for (int32 Index = 0; Index < ArityCase.Count; ++Index)
						{
							const FString InitialStatement = GetInitialStatement(
								TypePatternCase,
								DirectionPatternCase,
								Index);
							if (!InitialStatement.IsEmpty())
							{
								AppendGeneratedAsLine(Source, TEXT("	") + InitialStatement);
							}
						}
						const FString QualifiedCall = FCStringAnsi::Strcmp(
							TargetCase.CatalogName,
							"namespace_global") == 0
							? TEXT("N_") + Suffix + TEXT("::") + ProbeName
							: (FCStringAnsi::Strcmp(TargetCase.CatalogName, "instance_method") == 0
								? TEXT("Owner.") + ProbeName
								: ProbeName);
						if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "instance_method") == 0)
						{
							AppendGeneratedAsLine(Source, FString::Printf(
								TEXT("	FOwner_%s Owner;"),
								*Suffix));
						}
						AppendGeneratedAsLine(Source, FString::Printf(
							TEXT("	int Result = %s(%s);"),
							*QualifiedCall,
							*CallArguments));
						AppendGeneratedAsLine(Source, FString::Printf(
							TEXT("	return (Result * 1000) + (%s ? 1 : 0);"),
							*CallerCheck));
						AppendGeneratedAsLine(Source, TEXT("}"));
						AppendGeneratedAsLine(Source);
					}
				}
			}
		}
		return Source;
	}

	static asIScriptFunction* FindProbe(
		asIScriptModule& Module,
		const FString& ProbeName,
		const FTargetCase& TargetCase,
		const FString& Suffix)
	{
		if (FCStringAnsi::Strcmp(TargetCase.CatalogName, "instance_method") == 0)
		{
			const FString TypeName = TEXT("FOwner_") + Suffix;
			asITypeInfo* const Type = Module.GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName));
			return Type != nullptr ? Type->GetMethodByName(TCHAR_TO_ANSI(*ProbeName)) : nullptr;
		}

		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetName(), TCHAR_TO_ANSI(*ProbeName)) == 0)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static int32 GetExpectedResult(
		const FArityCase& ArityCase,
		const FDirectionPatternCase& DirectionPatternCase)
	{
		int32 Result = 0;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			if (!IsWrittenParameter(DirectionPatternCase, Index))
			{
				Result += Index + 3;
			}
			else if (IsReadParameter(DirectionPatternCase, Index))
			{
				Result += Index + 8;
			}
			else
			{
				Result += 100 + Index;
			}
		}
		return (Result * 1000) + 1;
	}

public:
	TEST_METHOD(SignaturesByArityTypeDirectionAndTarget)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-SIGNATURE-SHAPE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Function signature-shape product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString GeneratedSource = BuildSource();
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("LANG-FN-SIGNATURE-SHAPE"),
			TEXT("FunctionSignatureShapes"),
			GeneratedSource);
		const FTCHARToUTF8 GeneratedSourceUtf8(*GeneratedSource);
		{
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				"FunctionSignatureShapes",
				std::string(GeneratedSourceUtf8.Get(), GeneratedSourceUtf8.Length()));
			ASSERT_THAT(IsTrue(Module.IsValid(),
				TEXT("all signature-shape combinations should compile as one generated module")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(GeneratedSource);
				return;
			}

			for (const FArityCase& ArityCase : ArityCases)
			{
				for (const FTypePatternCase& TypePatternCase : TypePatternCases)
				{
					for (const FDirectionPatternCase& DirectionPatternCase : DirectionPatternCases)
					{
						for (const FTargetCase& TargetCase : TargetCases)
						{
							const FString Suffix = MakeSuffix(
								ArityCase,
								TypePatternCase,
								DirectionPatternCase,
								TargetCase);
							const FString CaseId = MakeNativeCaseId(
								"LANG-FN-SIGNATURE-SHAPE",
								{
									ANSI_TO_TCHAR(TargetCase.CatalogName),
									ANSI_TO_TCHAR(DirectionPatternCase.CatalogName),
									ANSI_TO_TCHAR(TypePatternCase.CatalogName),
									ANSI_TO_TCHAR(ArityCase.CatalogName)
								});
							const FNativeCaseContext Case(CaseId);
							Engine.ResetMessages();
							const FString ProbeName = TEXT("Probe_") + Suffix;
							asIScriptFunction* const Probe = FindProbe(
								*Module,
								ProbeName,
								TargetCase,
								Suffix);
							ASSERT_THAT(IsNotNull(Probe,
								*Case.Describe(TEXT("every signature combination should publish its probe"))));
							if (Probe != nullptr)
							{
								ASSERT_THAT(AreEqual(
									ArityCase.Count,
									static_cast<int32>(Probe->GetParamCount()),
									*Case.Describe(TEXT("probe metadata should preserve the generated parameter count"))));
								for (int32 Index = 0; Index < ArityCase.Count; ++Index)
								{
									int TypeId = 0;
									asDWORD Flags = 0;
									ASSERT_THAT(AreEqual(
										asSUCCESS,
										Probe->GetParam(Index, &TypeId, &Flags),
										*Case.Describe(TEXT("each parameter metadata slot should be queryable"))));
									const bool bFloatParameter = IsFloatParameter(TypePatternCase, Index);
									const bool bFloatIsFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
									const FString ExpectedTypeDeclaration = bFloatParameter
										? (bFloatIsFloat64 ? TEXT("double") : TEXT("float"))
										: TEXT("int");
									const int ExpectedTypeId = ScriptEngine->GetTypeIdByDecl(
										TCHAR_TO_ANSI(*ExpectedTypeDeclaration));
									ASSERT_THAT(AreEqual(
										ExpectedTypeId,
										TypeId,
										*Case.Describe(TEXT("parameter metadata should preserve each generated scalar type"))));
									const FString Direction = GetDirectionSuffix(DirectionPatternCase, Index);
									const asDWORD ExpectedFlags = Direction.IsEmpty()
										? asTM_NONE
										: (Direction.Contains(TEXT("out"))
											? (Direction.Contains(TEXT("inout")) ? asTM_INOUTREF : asTM_OUTREF)
											: asTM_INREF);
									ASSERT_THAT(AreEqual(
										ExpectedFlags,
										Flags & 3u,
										*Case.Describe(TEXT("parameter metadata should preserve each direction pattern"))));
								}
							}

							const FString EntryName = TEXT("Run_") + Suffix;
							const FString EntryDeclaration = FString::Printf(
								TEXT("int %s()"),
								*EntryName);
							AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
								*TestRunner,
								ScriptEngine,
								Module,
								TCHAR_TO_ANSI(*EntryDeclaration));
							ASSERT_THAT(IsTrue(Invoker.IsValid(),
								*Case.Describe(TEXT("every signature entry should resolve by exact declaration"))));
							if (Invoker.IsValid())
							{
				const int32 Expected = GetExpectedResult(
					ArityCase,
					DirectionPatternCase);
								ASSERT_THAT(AreEqual(
									Expected,
									Invoker.CallAndReturn<int32>(-1),
									*Case.Describe(TEXT("parameter transfer, return sum, and caller writeback should match"))));
							}
						}
					}
				}
			}
		}

		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule("FunctionSignatureShapes", asGM_ONLY_IF_EXISTS),
			TEXT("signature-shape module should be discarded after all combinations")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
