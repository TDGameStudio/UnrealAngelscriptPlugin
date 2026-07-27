#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"

#include <cstdlib>

using AngelscriptNativeTestSupport::FNativeTestEngine;
using AngelscriptNativeTestSupport::ENativeValueCategory;
using AngelscriptNativeTestSupport::FNativeTypeCase;
using AngelscriptNativeTestSupport::EqualAnsi;
using AngelscriptNativeTestSupport::ResolveNumericLiteral;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNumericConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.Numeric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FFormCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FValueCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FFormCase FormCases[] =
	{
		{ "assignment" },
		{ "initializer" },
		{ "argument" },
		{ "return" },
		{ "promotion" },
		{ "explicit_cast" },
	};

	inline static constexpr FValueCase ValueCases[] =
	{
		{ "zero" },
		{ "one" },
		{ "negative" },
		{ "min" },
		{ "max" },
		{ "near_boundary" },
		{ "fractional" },
	};

	static FString BuildNumericConversionSource(
		const FString& SourceType,
		const FString& TargetType,
		const ANSICHAR* SourceLiteral,
		const FFormCase& FormCase,
		const FString& FunctionName,
		const FString& ExpectedLiteral,
		const bool bEmitExactVerifier)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendSimpleIdentityFunction(Source, TargetType, TEXT("PassTarget"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s ReturnTarget(%s Value)"), *TargetType, *SourceType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s %s()"), *TargetType, *FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s SourceValue = %hs;"), *SourceType, SourceLiteral));

		if (EqualAnsi(FormCase.CatalogName, "assignment"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted;"), *TargetType));
			AppendGeneratedAsLine(Source, TEXT("\tConverted = SourceValue;"));
		}
		else if (EqualAnsi(FormCase.CatalogName, "initializer"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = SourceValue;"), *TargetType));
		}
		else if (EqualAnsi(FormCase.CatalogName, "argument"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = PassTarget(SourceValue);"), *TargetType));
		}
		else if (EqualAnsi(FormCase.CatalogName, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = ReturnTarget(SourceValue);"), *TargetType));
		}
		else if (EqualAnsi(FormCase.CatalogName, "promotion"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = SourceValue + %s(0);"), *TargetType, *TargetType));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = %s(SourceValue);"), *TargetType, *TargetType));
		}

		AppendGeneratedAsLine(Source, TEXT("\treturn Converted;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		if (bEmitExactVerifier)
		{
			AppendGeneratedAsLine(Source, TEXT("int VerifyNumericConversion()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s ExpectedValue = %s(%s);"), *TargetType, *TargetType, *ExpectedLiteral));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s ActualValue = %s();"), *TargetType, *FunctionName));
			AppendGeneratedAsLine(Source, TEXT("\treturn ActualValue == ExpectedValue ? 1 : 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("int ObserveNumericConversion()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s ActualValue = %s();"), *TargetType, *FunctionName));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool IsNamedValue(const FValueCase& ValueCase, const ANSICHAR* Name)
	{
		return AngelscriptNativeTestSupport::EqualAnsi(ValueCase.CatalogName, Name);
	}

	static bool IsFloatingType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::FloatingPoint;
	}

	static bool IsIntegerType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger
			|| TypeCase.Category == ENativeValueCategory::UnsignedInteger;
	}

	static uint64 IntegerMaximum(const FNativeTypeCase& TypeCase)
	{
		const uint32 BitCount = static_cast<uint32>(TypeCase.WidthInBytes) * 8u;
		return BitCount == 64u ? MAX_uint64 : ((uint64(1) << BitCount) - 1u);
	}

	static int64 SignedValueForCase(
		const FNativeTypeCase& TypeCase,
		const FValueCase& ValueCase)
	{
		const uint32 BitCount = static_cast<uint32>(TypeCase.WidthInBytes) * 8u;
		if (IsNamedValue(ValueCase, "zero"))
		{
			return 0;
		}
		if (IsNamedValue(ValueCase, "one") || IsNamedValue(ValueCase, "fractional"))
		{
			return 1;
		}
		if (IsNamedValue(ValueCase, "negative"))
		{
			return -1;
		}
		if (IsNamedValue(ValueCase, "min"))
		{
			return BitCount == 64u ? MIN_int64 : -(int64(1) << (BitCount - 1u));
		}
		const int64 Maximum = BitCount == 64u
			? MAX_int64
			: ((int64(1) << (BitCount - 1u)) - 1);
		return IsNamedValue(ValueCase, "near_boundary") ? Maximum - 1 : Maximum;
	}

	static uint64 UnsignedValueForCase(
		const FNativeTypeCase& TypeCase,
		const FValueCase& ValueCase)
	{
		if (IsNamedValue(ValueCase, "zero"))
		{
			return 0;
		}
		if (IsNamedValue(ValueCase, "one") || IsNamedValue(ValueCase, "fractional"))
		{
			return 1;
		}
		if (IsNamedValue(ValueCase, "min"))
		{
			return 0;
		}
		const uint64 Maximum = IntegerMaximum(TypeCase);
		return IsNamedValue(ValueCase, "near_boundary") ? Maximum - 1 : Maximum;
	}

	static double FloatingValueForCase(
		const FNativeTypeCase& TypeCase,
		const FValueCase& ValueCase)
	{
		const ANSICHAR* const Literal = ResolveNumericLiteral(TypeCase, ValueCase.CatalogName);
		const double Parsed = std::strtod(Literal, nullptr);
		return EqualAnsi(TypeCase.CatalogName, "float32")
			? static_cast<double>(static_cast<float>(Parsed))
			: Parsed;
	}

	static bool HasPortableExactValue(
		const FNativeTypeCase& SourceCase,
		const FNativeTypeCase& TargetCase,
		const FValueCase& ValueCase)
	{
		if (!IsFloatingType(SourceCase) || !IsIntegerType(TargetCase))
		{
			return true;
		}

		if (IsNamedValue(ValueCase, "zero")
			|| IsNamedValue(ValueCase, "one")
			|| IsNamedValue(ValueCase, "fractional"))
		{
			return true;
		}

		return IsNamedValue(ValueCase, "negative") && TargetCase.Category == ENativeValueCategory::SignedInteger;
	}

	static FString BuildIndependentExpectedLiteral(
		const FNativeTypeCase& SourceCase,
		const FNativeTypeCase& TargetCase,
		const FValueCase& ValueCase)
	{
		if (TargetCase.Category == ENativeValueCategory::FloatingPoint)
		{
			if (EqualAnsi(TargetCase.CatalogName, "float32"))
			{
				const float Expected = static_cast<float>(IsFloatingType(SourceCase)
					? FloatingValueForCase(SourceCase, ValueCase)
					: SourceCase.Category == ENativeValueCategory::SignedInteger
						? static_cast<double>(SignedValueForCase(SourceCase, ValueCase))
						: static_cast<double>(UnsignedValueForCase(SourceCase, ValueCase)));
				return FString::Printf(TEXT("%.9gf"), static_cast<double>(Expected));
			}

			const double Expected = IsFloatingType(SourceCase)
				? FloatingValueForCase(SourceCase, ValueCase)
				: SourceCase.Category == ENativeValueCategory::SignedInteger
					? static_cast<double>(SignedValueForCase(SourceCase, ValueCase))
					: static_cast<double>(UnsignedValueForCase(SourceCase, ValueCase));
			return FString::Printf(TEXT("%.17g"), Expected);
		}

		if (!HasPortableExactValue(SourceCase, TargetCase, ValueCase))
		{
			return FString();
		}

		uint64 ConvertedBits = 0;
		if (IsFloatingType(SourceCase))
		{
			const double SourceValue = FloatingValueForCase(SourceCase, ValueCase);
			ConvertedBits = TargetCase.Category == ENativeValueCategory::SignedInteger
				? static_cast<uint64>(static_cast<int64>(SourceValue))
				: static_cast<uint64>(SourceValue);
		}
		else if (SourceCase.Category == ENativeValueCategory::SignedInteger)
		{
			ConvertedBits = static_cast<uint64>(SignedValueForCase(SourceCase, ValueCase));
		}
		else
		{
			ConvertedBits = UnsignedValueForCase(SourceCase, ValueCase);
		}

		const uint32 BitCount = static_cast<uint32>(TargetCase.WidthInBytes) * 8u;
		const uint64 Mask = BitCount == 64u ? MAX_uint64 : ((uint64(1) << BitCount) - 1u);
		ConvertedBits &= Mask;
		if (TargetCase.Category == ENativeValueCategory::SignedInteger)
		{
			const uint64 SignBit = BitCount == 64u ? (uint64(1) << 63u) : (uint64(1) << (BitCount - 1u));
			const int64 Expected = (ConvertedBits & SignBit) != 0
				? static_cast<int64>(ConvertedBits | ~Mask)
				: static_cast<int64>(ConvertedBits);
			return FString::Printf(TEXT("%lld"), static_cast<long long>(Expected));
		}

		return FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(ConvertedBits));
	}

public:

	TEST_METHOD(SourceTargetFormAndValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-NUMERIC",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Numeric conversion product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		int32 NumericTypeCount = 0;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (TypeCase.Category == ENativeValueCategory::SignedInteger
				|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
				|| TypeCase.Category == ENativeValueCategory::FloatingPoint)
			{
				++NumericTypeCount;
			}
		}
		const int32 ExpectedCaseCount = NumericTypeCount
			* NumericTypeCount
			* UE_ARRAY_COUNT(FormCases)
			* UE_ARRAY_COUNT(ValueCases);
		int32 ObservedCaseCount = 0;

		for (const FNativeTypeCase& SourceCase : NativeTypeCases)
		{
			if (SourceCase.Category != ENativeValueCategory::SignedInteger
				&& SourceCase.Category != ENativeValueCategory::UnsignedInteger
				&& SourceCase.Category != ENativeValueCategory::FloatingPoint)
			{
				continue;
			}

			for (const FNativeTypeCase& TargetCase : NativeTypeCases)
			{
				if (TargetCase.Category != ENativeValueCategory::SignedInteger
					&& TargetCase.Category != ENativeValueCategory::UnsignedInteger
					&& TargetCase.Category != ENativeValueCategory::FloatingPoint)
				{
					continue;
				}

				for (const FFormCase& FormCase : FormCases)
				{
					for (const FValueCase& ValueCase : ValueCases)
					{
						++ObservedCaseCount;
						const FString SourceType = ResolveNumericScriptType(SourceCase, *ScriptEngine);
						const FString TargetType = ResolveNumericScriptType(TargetCase, *ScriptEngine);
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-CONV-NUMERIC",
							{ ANSI_TO_TCHAR(SourceCase.CatalogName), ANSI_TO_TCHAR(TargetCase.CatalogName),
								ANSI_TO_TCHAR(FormCase.CatalogName), ANSI_TO_TCHAR(ValueCase.CatalogName) }));
						const FString ModuleName = TEXT("NumericConversion_") + Case.GetId();
						const FString FunctionName = TEXT("RunNumericConversion");
						const bool bPortableExact = HasPortableExactValue(SourceCase, TargetCase, ValueCase);
						const FString ExpectedLiteral = bPortableExact
							? BuildIndependentExpectedLiteral(SourceCase, TargetCase, ValueCase)
							: FString();
						const FString Source = BuildNumericConversionSource(
							SourceType,
							TargetType,
							ResolveNumericLiteral(SourceCase, ValueCase.CatalogName),
							FormCase,
							FunctionName,
							ExpectedLiteral,
							bPortableExact);

						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						Engine.Reset(*TestRunner);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							ScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							Module);

						if (BuildResult < 0)
						{
							TestRunner->AddInfo(*Case.Describe(TEXT("classification=Rejected")));
							ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(Engine.GetMessages(), ModuleName),
								*Case.Describe(TEXT("rejected numeric conversion should own one located diagnostic"))));
						}
						else
						{
							TestRunner->AddInfo(*Case.Describe(bPortableExact
								? TEXT("classification=Accepted exact-oracle=enabled")
								: TEXT("classification=Accepted exact-oracle=withheld fork-limitation=nonportable-float-to-integer")));
							if (!bPortableExact)
							{
								TestRunner->AddInfo(*Case.Describe(TEXT("[AS-REF-FORK-LIMITATION] exact floating-to-integer result is not portable for this source value; compilation, metadata, execution, and cleanup remain covered")));
							}
							ASSERT_THAT(IsNotNull(Module,
								*Case.Describe(TEXT("accepted numeric conversion should publish a module"))));
							asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TargetType, FunctionName);
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT("accepted numeric conversion should resolve an exact entry declaration"))));
							if (Entry != nullptr)
							{
								const int ExpectedTypeId = ResolveNumericPublicTypeId(TargetCase, *ScriptEngine);
								const int ActualTypeId = Entry->GetReturnTypeId();
								if (ExpectedTypeId != ActualTypeId)
								{
									TestRunner->AddInfo(*Case.Describe(*FString::Printf(
										TEXT("numeric return metadata trace: sourceSpelling='%s' semanticType='%hs' expectedTypeId=%d actualTypeId=%d declaration='%hs' floatIsFloat64=%d"),
										*TargetType,
										TargetCase.CatalogName,
										ExpectedTypeId,
										ActualTypeId,
										Entry->GetDeclaration(),
										ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64))));
								}
								ASSERT_THAT(AreEqual(ExpectedTypeId,
									ActualTypeId,
									*Case.Describe(TEXT("accepted numeric conversion should retain its target return type"))));
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
									ExecuteNoArgumentEntry(*ScriptEngine, *Entry),
									*Case.Describe(TEXT("accepted numeric conversion should execute through the selected form"))));

								asIScriptFunction* const ValueVerifier = FindNoArgumentEntry(
									Module,
									TEXT("int"),
									bPortableExact ? TEXT("VerifyNumericConversion") : TEXT("ObserveNumericConversion"));
								ASSERT_THAT(IsNotNull(ValueVerifier,
									*Case.Describe(TEXT("accepted numeric conversion should publish its independent value observer"))));
								if (ValueVerifier != nullptr)
								{
									int32 VerificationMarker = INDEX_NONE;
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
										ExecuteNoArgumentIntEntry(*ScriptEngine, *ValueVerifier, VerificationMarker),
										*Case.Describe(TEXT("numeric value verifier should execute after the selected form"))));
									if (bPortableExact)
									{
										ASSERT_THAT(AreEqual(1,
											VerificationMarker,
											*Case.Describe(TEXT("numeric conversion should match the independent C++ expected value across the selected form"))));
									}
								}
							}
						}

						ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
							*Case.Describe(TEXT("numeric conversion cell should discard its module"))));
					}
				}
			}
		}

		ASSERT_THAT(AreEqual(10, NumericTypeCount,
			TEXT("LANG-CONV-NUMERIC must retain all ten numeric source/target type definitions")));
		ASSERT_THAT(AreEqual(ExpectedCaseCount, ObservedCaseCount,
			TEXT("LANG-CONV-NUMERIC must execute every source type, target type, form, and value partition")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
