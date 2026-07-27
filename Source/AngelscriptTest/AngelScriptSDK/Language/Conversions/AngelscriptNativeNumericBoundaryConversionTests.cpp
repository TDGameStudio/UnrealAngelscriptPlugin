#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"
#include <limits>

using AngelscriptNativeTestSupport::ENativeScalarAccessor;
using AngelscriptNativeTestSupport::ENativeEvidence;
using AngelscriptNativeTestSupport::ENativeValueCategory;
using AngelscriptNativeTestSupport::EqualAnsi;
using AngelscriptNativeTestSupport::FNativeCaseContext;
using AngelscriptNativeTestSupport::FNativeTestEngine;
using AngelscriptNativeTestSupport::FNativeTypeCase;
using AngelscriptNativeTestSupport::NativeTypeCases;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNumericBoundaryConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.NumericBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FFormCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FValueCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FSourceCase SourceCases[] =
	{
		{ "float32" },
		{ "float64" },
	};

	inline static constexpr FFormCase FormCases[] =
	{
		{ "assignment" },
		{ "argument" },
		{ "return" },
		{ "explicit_cast" },
	};

	inline static constexpr FValueCase FiniteSpecialValueCases[] =
	{
		{ "positive_zero" },
		{ "negative_zero" },
		{ "subnormal" },
	};

	inline static constexpr FValueCase NonFiniteValueCases[] =
	{
		{ "positive_infinity" },
		{ "negative_infinity" },
		{ "nan" },
	};

	inline static constexpr FValueCase FiniteRangeValueCases[] =
	{
		{ "above_target_max" },
		{ "below_target_min" },
	};

	static const FNativeTypeCase* FindNativeType(const ANSICHAR* CatalogName)
	{
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (EqualAnsi(TypeCase.CatalogName, CatalogName))
			{
				return &TypeCase;
			}
		}

		return nullptr;
	}

	static bool IsNamedValue(const FValueCase& ValueCase, const ANSICHAR* Name)
	{
		return EqualAnsi(ValueCase.CatalogName, Name);
	}

	static bool IsNonFiniteValue(const FValueCase& ValueCase)
	{
		return IsNamedValue(ValueCase, "positive_infinity")
			|| IsNamedValue(ValueCase, "negative_infinity")
			|| IsNamedValue(ValueCase, "nan");
	}

	static uint64 FloatBits(const float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 FloatBits(const double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 ReadReturnBits(asIScriptContext& Context, const FNativeTypeCase& TargetCase)
	{
		switch (TargetCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.GetReturnByte();
		case ENativeScalarAccessor::Word:
			return Context.GetReturnWord();
		case ENativeScalarAccessor::DWord:
			return Context.GetReturnDWord();
		case ENativeScalarAccessor::QWord:
			return Context.GetReturnQWord();
		case ENativeScalarAccessor::Float:
			return FloatBits(Context.GetReturnFloat());
		case ENativeScalarAccessor::Double:
			return FloatBits(Context.GetReturnDouble());
		default:
			return 0;
		}
	}

	static uint64 ExpectedFiniteBits(
		const FSourceCase& SourceCase,
		const FNativeTypeCase& TargetCase,
		const FValueCase& ValueCase)
	{
		if (IsNamedValue(ValueCase, "positive_zero"))
		{
			return 0;
		}

		if (IsNamedValue(ValueCase, "negative_zero"))
		{
			if (TargetCase.Accessor == ENativeScalarAccessor::Float)
			{
				return 0x80000000ull;
			}
			if (TargetCase.Accessor == ENativeScalarAccessor::Double)
			{
				return 0x8000000000000000ull;
			}
			return 0;
		}

		if (IsNamedValue(ValueCase, "subnormal"))
		{
			if (TargetCase.Accessor == ENativeScalarAccessor::Float)
			{
				return EqualAnsi(SourceCase.CatalogName, "float32")
					? FloatBits(std::numeric_limits<float>::denorm_min())
					: 0;
			}
			if (TargetCase.Accessor == ENativeScalarAccessor::Double)
			{
				return EqualAnsi(SourceCase.CatalogName, "float32")
					? FloatBits(static_cast<double>(std::numeric_limits<float>::denorm_min()))
					: FloatBits(std::numeric_limits<double>::denorm_min());
			}
			return 0;
		}

		if (TargetCase.Accessor == ENativeScalarAccessor::Float)
		{
			return IsNamedValue(ValueCase, "above_target_max")
				? 0x7f800000ull
				: 0xff800000ull;
		}

		return 0;
	}

	static void AppendSpecialSourceValue(
		FString& Source,
		const FSourceCase& SourceCase,
		const FString& SourceType,
		const FValueCase& ValueCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsNamedValue(ValueCase, "positive_zero"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = 0.0;"), *SourceType));
			return;
		}

		if (IsNamedValue(ValueCase, "negative_zero"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = -0.0;"), *SourceType));
			return;
		}

		if (IsNamedValue(ValueCase, "subnormal"))
		{
			const TCHAR* const Literal = EqualAnsi(SourceCase.CatalogName, "float32")
				? TEXT("1.40129846e-45f")
				: TEXT("4.9406564584124654e-324");
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = %s;"), *SourceType, Literal));
			return;
		}

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Unit = 1.0;"), *SourceType));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Zero = 0.0;"), *SourceType));
		if (IsNamedValue(ValueCase, "positive_infinity"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = Unit / Zero;"), *SourceType));
		}
		else if (IsNamedValue(ValueCase, "negative_infinity"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = -Unit / Zero;"), *SourceType));
		}
		else if (IsNamedValue(ValueCase, "nan"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = Zero / Zero;"), *SourceType));
		}
		else if (IsNamedValue(ValueCase, "above_target_max"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = 6.805646932e+38;"), *SourceType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = -6.805646932e+38;"), *SourceType));
		}
	}

	static FString BuildNumericBoundaryConversionSource(
		const FSourceCase& SourceCase,
		const FString& SourceType,
		const FString& TargetType,
		const FFormCase& FormCase,
		const FValueCase& ValueCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendSimpleIdentityFunction(Source, TargetType, TEXT("PassNumericBoundaryTarget"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s ReturnNumericBoundaryTarget(%s Value)"), *TargetType, *SourceType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s RunNumericBoundaryConversion()"), *TargetType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSpecialSourceValue(Source, SourceCase, SourceType, ValueCase);

		if (EqualAnsi(FormCase.CatalogName, "assignment"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted;"), *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\tConverted = SourceValue;"));
		}
		else if (EqualAnsi(FormCase.CatalogName, "argument"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = PassNumericBoundaryTarget(SourceValue);"), *TargetType));
		}
		else if (EqualAnsi(FormCase.CatalogName, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = ReturnNumericBoundaryTarget(SourceValue);"), *TargetType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = %s(SourceValue);"), *TargetType, *TargetType));
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn Converted;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static void ExecuteAndVerifyFiniteCell(
		FAutomationTestBase& Test,
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Entry,
		const FSourceCase& SourceCase,
		const FNativeTypeCase& TargetCase,
		const FValueCase& ValueCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assertions(Test);
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assertions.IsNotNull(Context,
			*Case.Describe(TEXT("defined floating boundary cell should create an execution context"))))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (!Assertions.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, &Entry),
			*Case.Describe(TEXT("defined floating boundary cell should finish"))))
		{
			return;
		}
		if (!Assertions.AreEqual(ExpectedFiniteBits(SourceCase, TargetCase, ValueCase),
			ReadReturnBits(*Context, TargetCase),
			*Case.Describe(TEXT("defined floating boundary cell should preserve the exact target bits"))))
		{
			return;
		}
		if (!Assertions.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("defined floating boundary cell should release its conversion temporaries"))))
		{
			return;
		}
	}

	static void ExecuteAndVerifyNonFiniteCell(
		FAutomationTestBase& Test,
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptFunction& Entry)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter Assertions(Test);
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (!Assertions.IsNotNull(Context,
			*Case.Describe(TEXT("non-finite source cell should create an execution context"))))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		if (!Assertions.AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
			PrepareAndExecute(Context, &Entry),
			*Case.Describe(TEXT("non-finite source construction should raise the raw SDK divide-by-zero exception"))))
		{
			return;
		}
		if (!Assertions.AreEqual(FString(TEXT("Divide by zero")),
			FString(UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "")),
			*Case.Describe(TEXT("non-finite source construction should retain the exact exception text"))))
		{
			return;
		}
		if (!Assertions.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("non-finite source exception should release the conversion context"))))
		{
			return;
		}
	}

	void CompileAndVerifyCell(
		FAutomationTestBase& Test,
		FNativeTestEngine& NativeEngine,
		asIScriptEngine& ScriptEngine,
		const FNativeCaseContext& Case,
		const FSourceCase& SourceCase,
		const FString& SourceType,
		const FNativeTypeCase& TargetCase,
		const FString& TargetType,
		const FFormCase& FormCase,
		const FValueCase& ValueCase,
		const bool bExpectNonFinite)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString ModuleName = TEXT("NumericBoundaryConversion_") + Case.GetId();
		const FString Source = BuildNumericBoundaryConversionSource(
			SourceCase, SourceType, TargetType, FormCase, ValueCase);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		NativeEngine.Reset(Test);
		PrintGeneratedAsSource(Test, Case.GetId(), ModuleName, Source);
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			Module);

		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*Case.Describe(TEXT("defined floating boundary conversion should compile"))));
		if (BuildResult >= 0 && Module != nullptr)
		{
			asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TargetType, TEXT("RunNumericBoundaryConversion"));
			ASSERT_THAT(IsNotNull(Entry,
				*Case.Describe(TEXT("defined floating conversion should resolve exactly"))));
			if (Entry != nullptr)
			{
				const int ExpectedTypeId = ResolveNumericPublicTypeId(TargetCase, ScriptEngine);
				const int ActualTypeId = Entry->GetReturnTypeId();
				if (ExpectedTypeId != ActualTypeId)
				{
					Test.AddInfo(*Case.Describe(*FString::Printf(
						TEXT("floating return metadata trace: sourceSpelling='%s' semanticType='%hs' expectedTypeId=%d actualTypeId=%d declaration='%hs' floatIsFloat64=%d"),
						*TargetType,
						TargetCase.CatalogName,
						ExpectedTypeId,
						ActualTypeId,
						Entry->GetDeclaration(),
						ScriptEngine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64))));
				}
				ASSERT_THAT(AreEqual(ExpectedTypeId,
					ActualTypeId,
					*Case.Describe(TEXT("defined floating conversion should retain target metadata"))));
				if (bExpectNonFinite)
				{
					ExecuteAndVerifyNonFiniteCell(Test, Case, ScriptEngine, *Entry);
				}
				else
				{
					ExecuteAndVerifyFiniteCell(Test, Case, ScriptEngine, *Entry, SourceCase, TargetCase, ValueCase);
				}
			}
		}

		ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(ScriptEngine, ModuleNameUtf8),
			*Case.Describe(TEXT("floating conversion cell should discard its module"))));
	}

	void RunSpecialValueCells(
		FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const ANSICHAR* ProductId,
		const FValueCase* Values,
		const int32 ValueCount)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Numeric special-value conversion product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 NumericTargetCount = 0;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (TypeCase.Category == ENativeValueCategory::SignedInteger
				|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
				|| TypeCase.Category == ENativeValueCategory::FloatingPoint)
			{
				++NumericTargetCount;
			}
		}
		int32 ObservedCaseCount = 0;

		for (const FSourceCase& SourceCase : SourceCases)
		{
			const FNativeTypeCase* const SourceTypeCase = FindNativeType(SourceCase.CatalogName);
			ASSERT_THAT(IsNotNull(SourceTypeCase, TEXT("numeric special source should map to one native type definition")));
			if (SourceTypeCase == nullptr)
			{
				continue;
			}

			const FString SourceType = ResolveNumericScriptType(*SourceTypeCase, *ScriptEngine);
			for (const FNativeTypeCase& TargetCase : NativeTypeCases)
			{
				if (TargetCase.Category != ENativeValueCategory::SignedInteger
					&& TargetCase.Category != ENativeValueCategory::UnsignedInteger
					&& TargetCase.Category != ENativeValueCategory::FloatingPoint)
				{
					continue;
				}

				const FString TargetType = ResolveNumericScriptType(TargetCase, *ScriptEngine);
				for (const FFormCase& FormCase : FormCases)
				{
					for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
					{
						const FValueCase& ValueCase = Values[ValueIndex];
						++ObservedCaseCount;
						const FNativeCaseContext Case(MakeNativeCaseId(ProductId,
							{ ANSI_TO_TCHAR(SourceCase.CatalogName), ANSI_TO_TCHAR(TargetCase.CatalogName),
								ANSI_TO_TCHAR(FormCase.CatalogName), ANSI_TO_TCHAR(ValueCase.CatalogName) }));
						CompileAndVerifyCell(
							Test,
							Engine,
							*ScriptEngine,
							Case,
							SourceCase,
							SourceType,
							TargetCase,
							TargetType,
							FormCase,
							ValueCase,
							IsNonFiniteValue(ValueCase));
					}
				}
			}
		}

		const int32 ExpectedCaseCount = UE_ARRAY_COUNT(SourceCases)
			* NumericTargetCount
			* UE_ARRAY_COUNT(FormCases)
			* ValueCount;
		ASSERT_THAT(AreEqual(ExpectedCaseCount, ObservedCaseCount,
			FString::Printf(TEXT("%hs must execute every source, numeric target, form, and value cell"), ProductId)));
	}

public:

	TEST_METHOD(FiniteValuesBySourceTargetAndForm)
	{
		AS_NATIVE_PRODUCT("LANG-CONV-FLOAT-FINITE-SPECIAL",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		RunSpecialValueCells(
			*TestRunner,
			Engine,
			"LANG-CONV-FLOAT-FINITE-SPECIAL",
			FiniteSpecialValueCases,
			UE_ARRAY_COUNT(FiniteSpecialValueCases));
	}

	TEST_METHOD(NonFiniteConstructionBySourceTargetAndForm)
	{
		AS_NATIVE_PRODUCT("LANG-CONV-NONFINITE-PRECONVERSION",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		RunSpecialValueCells(
			*TestRunner,
			Engine,
			"LANG-CONV-NONFINITE-PRECONVERSION",
			NonFiniteValueCases,
			UE_ARRAY_COUNT(NonFiniteValueCases));
	}

	TEST_METHOD(Float64ToFloat32RangeByFormAndDirection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Float64-to-float32 finite range product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FSourceCase SourceCase = { "float64" };
		const FNativeTypeCase* const SourceTypeCase = FindNativeType(SourceCase.CatalogName);
		const FNativeTypeCase* const TargetTypeCase = FindNativeType("float32");
		ASSERT_THAT(IsNotNull(SourceTypeCase, TEXT("finite range product should resolve the float64 source type")));
		ASSERT_THAT(IsNotNull(TargetTypeCase, TEXT("finite range product should resolve the float32 target type")));
		if (SourceTypeCase == nullptr || TargetTypeCase == nullptr)
		{
			return;
		}

		const FString SourceType = ResolveNumericScriptType(*SourceTypeCase, *ScriptEngine);
		const FString TargetType = ResolveNumericScriptType(*TargetTypeCase, *ScriptEngine);
		int32 ObservedCaseCount = 0;
		for (const FFormCase& FormCase : FormCases)
		{
			for (const FValueCase& ValueCase : FiniteRangeValueCases)
			{
				++ObservedCaseCount;
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-CONV-FLOAT64-TO-FLOAT32-RANGE",
					{ ANSI_TO_TCHAR(FormCase.CatalogName), ANSI_TO_TCHAR(ValueCase.CatalogName) }));
				CompileAndVerifyCell(
					*TestRunner,
					Engine,
					*ScriptEngine,
					Case,
					SourceCase,
					SourceType,
					*TargetTypeCase,
					TargetType,
					FormCase,
					ValueCase,
					false);
			}
		}

		ASSERT_THAT(AreEqual(
			UE_ARRAY_COUNT(FormCases) * UE_ARRAY_COUNT(FiniteRangeValueCases),
			ObservedCaseCount,
			TEXT("LANG-CONV-FLOAT64-TO-FLOAT32-RANGE must execute both range boundaries for every form")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
