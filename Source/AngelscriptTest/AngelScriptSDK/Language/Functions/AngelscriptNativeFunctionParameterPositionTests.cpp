#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FFunctionParameterPositionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.ParameterPositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	struct FPositionCase
	{
		const ANSICHAR* CatalogName;
		int32 Index;
	};

	inline static constexpr FPositionCase PositionCases[] =
	{
		{ "first", 0 },
		{ "middle", 1 },
		{ "last", 2 },
	};

	static FString MakeName(
		const TCHAR* Prefix,
		const FNativeTypeCase& TypeCase,
		const FPositionCase& PositionCase,
		const FNativeDirectionCase& DirectionCase)
	{
		return FString::Printf(
			TEXT("%s_%hs_%hs_%hs"),
			Prefix,
			TypeCase.CatalogName,
			PositionCase.CatalogName,
			DirectionCase.CatalogName);
	}

	static FNativeTypeCase EffectiveTypeCase(const FNativeTypeCase& TypeCase)
	{
		FNativeTypeCase Result = TypeCase;
		if (Result.Category == ENativeValueCategory::FloatingPoint)
		{
			Result.ScriptType = Result.CatalogName;
		}
		return Result;
	}

	static FString MakeParameterDeclaration(
		const FNativeTypeCase& TypeCase,
		const FPositionCase& PositionCase,
		const FNativeDirectionCase& DirectionCase)
	{
		TArray<FString> Parameters =
		{
			TEXT("int FirstSentinel"),
			TEXT("int MiddleSentinel"),
			TEXT("int LastSentinel"),
		};
		Parameters[PositionCase.Index] = FString::Printf(
			TEXT("%hs%s Target"),
			TypeCase.ScriptType,
			ANSI_TO_TCHAR(DirectionCase.DeclarationSuffix));
		return FString::Join(Parameters, TEXT(", "));
	}

	static FString MakeCallArguments(const FPositionCase& PositionCase)
	{
		TArray<FString> Arguments = { TEXT("101"), TEXT("202"), TEXT("303") };
		Arguments[PositionCase.Index] = TEXT("Target");
		return FString::Join(Arguments, TEXT(", "));
	}

	static FString MakeSentinelExpression(const FPositionCase& PositionCase)
	{
		TArray<FString> Expressions;
		if (PositionCase.Index != 0)
		{
			Expressions.Add(TEXT("FirstSentinel == 101"));
		}
		if (PositionCase.Index != 1)
		{
			Expressions.Add(TEXT("MiddleSentinel == 202"));
		}
		if (PositionCase.Index != 2)
		{
			Expressions.Add(TEXT("LastSentinel == 303"));
		}
		return FString::Join(Expressions, TEXT(" && "));
	}

	static FString BuildParameterPositionSource()
	{
		FString Source;
		AppendCoreLanguageTypeDeclarations(Source);
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (!IsCoreValueTypeCase(TypeCase))
			{
				continue;
			}
			const FNativeTypeCase EffectiveType = EffectiveTypeCase(TypeCase);

			for (const FPositionCase& PositionCase : PositionCases)
			{
				for (const FNativeDirectionCase& DirectionCase : NativeDirectionCases)
				{
					const FString ProbeName = MakeName(TEXT("Probe"), TypeCase, PositionCase, DirectionCase);
					const FString EntryName = MakeName(TEXT("Run"), TypeCase, PositionCase, DirectionCase);
					const bool bObjectValue = IsObjectValueTypeCase(TypeCase);
					const ANSICHAR* WriteLiteral = bObjectValue
						? "1"
						: (DirectionCase.TypeModifier == asTM_INOUTREF
							&& TypeCase.Category == ENativeValueCategory::Boolean
							? TypeCase.ZeroLiteral
							: TypeCase.NearBoundaryLiteral);
					const ANSICHAR* InitialLiteral = bObjectValue
						? "1"
						: (DirectionCase.bReadsValue ? TypeCase.OneLiteral : TypeCase.ZeroLiteral);
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("bool %s(%s)"),
						*ProbeName,
						*MakeParameterDeclaration(EffectiveType, PositionCase, DirectionCase)));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tbool SentinelsMatch = %s;"), *MakeSentinelExpression(PositionCase)));
					if (DirectionCase.bReadsValue)
					{
						AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tbool InputMatches = %s;"), *MakeTypeReadExpression(EffectiveType, TEXT("Target"), bObjectValue ? "1" : nullptr)));
					}
					if (DirectionCase.bWritesValue)
					{
						AppendGeneratedAsLine(Source, TEXT("\t") + MakeTypeAssignStatement(EffectiveType, TEXT("Target"), WriteLiteral));
					}
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("\treturn SentinelsMatch%s;"),
						DirectionCase.bReadsValue ? TEXT(" && InputMatches") : TEXT("")));
					AppendGeneratedAsLine(Source, TEXT("}"));
					AppendGeneratedAsLine(Source);

					AppendGeneratedAsLine(Source, FString::Printf(TEXT("bool %s()"), *EntryName));
					AppendGeneratedAsLine(Source, TEXT("{"));
					AppendGeneratedAsLine(Source, TEXT("\t") + MakeTypeInitialStatement(
						EffectiveType,
						TEXT("Target"),
						InitialLiteral));
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tbool Result = %s(%s);"), *ProbeName, *MakeCallArguments(PositionCase)));
					const ANSICHAR* ExpectedAfterCall = DirectionCase.bWritesValue || bObjectValue ? WriteLiteral : TypeCase.OneLiteral;
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("\treturn Result && (%s);"),
						*MakeTypeReadExpression(EffectiveType, TEXT("Target"), bObjectValue ? "1" : ExpectedAfterCall)));
					AppendGeneratedAsLine(Source, TEXT("}"));
					AppendGeneratedAsLine(Source);
				}
			}
		}
		return Source;
	}

public:
	TEST_METHOD(ParameterTypesByPositionAndDirection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-PARAM-POSITION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function parameter-position product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Function parameter-position product should register its core typedef through the raw SDK API")));

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Function parameter-position product should register its local native value fixture")));
		const FString GeneratedSource = BuildParameterPositionSource();
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("LANG-FN-PARAM-POSITION"),
			TEXT("FunctionParameterPositions"),
			GeneratedSource);
		const FTCHARToUTF8 GeneratedSourceUtf8(*GeneratedSource);
		{
			FScopedNativeModule Module(
				*TestRunner,
				Engine,
				"FunctionParameterPositions",
				std::string(GeneratedSourceUtf8.Get(), GeneratedSourceUtf8.Length()));
			ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Function parameter-position product should compile all type, position, and direction cells")));
			if (!Module.IsValid())
			{
				TestRunner->AddInfo(GeneratedSource);
				return;
			}

			for (const FNativeTypeCase& TypeCase : NativeTypeCases)
			{
				if (!IsCoreValueTypeCase(TypeCase))
				{
					continue;
				}
				for (const FPositionCase& PositionCase : PositionCases)
				{
					for (const FNativeDirectionCase& DirectionCase : NativeDirectionCases)
					{
						const FString CaseId = MakeNativeCaseId(
							"LANG-FN-PARAM-POSITION",
							{
								ANSI_TO_TCHAR(DirectionCase.CatalogName),
								ANSI_TO_TCHAR(PositionCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase.CatalogName)
							});
						const FNativeCaseContext Case(CaseId);
						const FString ProbeName = MakeName(TEXT("Probe"), TypeCase, PositionCase, DirectionCase);
						const FNativeTypeCase EffectiveType = EffectiveTypeCase(TypeCase);
						const FString CanonicalParameters =
							MakeParameterDeclaration(EffectiveType, PositionCase, DirectionCase);
						const FString ProbeDeclaration = FString::Printf(
							TEXT("bool %s(%s)"),
							*ProbeName,
							*CanonicalParameters);
						asIScriptFunction* const Probe =
							Module->GetFunctionByDecl(TCHAR_TO_ANSI(*ProbeDeclaration));
						ASSERT_THAT(IsNotNull(Probe, *Case.Describe(TEXT("three-parameter probe should be published"))));
						if (Probe != nullptr)
						{
							ASSERT_THAT(AreEqual(3, static_cast<int32>(Probe->GetParamCount()),
								*Case.Describe(TEXT("probe metadata should preserve the three parameter slots"))));
							int TargetTypeId = 0;
							asDWORD TargetFlags = 0;
							const int ParamResult = Probe->GetParam(PositionCase.Index, &TargetTypeId, &TargetFlags);
							ASSERT_THAT(AreEqual(asSUCCESS, ParamResult,
								*Case.Describe(TEXT("target slot metadata should be queryable"))));
							const int32 ExpectedDirection = IsObjectValueTypeCase(TypeCase)
								&& DirectionCase.TypeModifier == asTM_NONE
								? static_cast<int32>(asTM_INOUTREF)
								: static_cast<int32>(DirectionCase.TypeModifier);
							ASSERT_THAT(AreEqual(
								ExpectedDirection,
								static_cast<int32>(TargetFlags & 3u),
								*Case.Describe(TEXT("target slot metadata should preserve the requested reference direction"))));
							ASSERT_THAT(AreEqual(Probe, ScriptEngine->GetFunctionById(Probe->GetId()),
								*Case.Describe(TEXT("probe exact declaration should round-trip to the same function"))));
						}

						const FString EntryName = MakeName(TEXT("Run"), TypeCase, PositionCase, DirectionCase);
						const FString EntryDeclaration = FString::Printf(TEXT("bool %s()"), *EntryName);
						AngelscriptSDKTestSupport::FSdkFunctionInvoker Invoker(
							*TestRunner,
							ScriptEngine,
							Module,
							TCHAR_TO_ANSI(*EntryDeclaration));
						ASSERT_THAT(IsTrue(Invoker.IsValid(), *Case.Describe(TEXT("entry exact declaration should resolve"))));
						if (Invoker.IsValid())
						{
							ASSERT_THAT(IsTrue(Invoker.CallAndReturn<bool>(false),
								*Case.Describe(TEXT("sentinel slots, transfer, and writeback should all match"))));
						}
					}
				}
			}
		}

		ASSERT_THAT(IsNull(ScriptEngine->GetModule("FunctionParameterPositions", asGM_ONLY_IF_EXISTS),
			TEXT("Function parameter-position product should discard its generated module")));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			TEXT("Function parameter-position product should release every native value")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
