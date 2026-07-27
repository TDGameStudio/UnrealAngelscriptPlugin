#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"
#include <limits>

using AngelscriptNativeTestSupport::EqualAnsi;
using AngelscriptNativeTestSupport::ENativeValueCategory;
using AngelscriptNativeTestSupport::FNativeTestEngine;
using AngelscriptNativeTestSupport::FNativeTypeCase;
using AngelscriptNativeTestSupport::NativeTypeCases;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEnumAliasConversionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.EnumAlias",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
	};

	struct FTargetCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
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
		{ "enum", "EConversionEnum" },
		{ "alias_int8", "AliasInt8" },
		{ "alias_int", "AliasInt" },
		{ "alias_int64", "AliasInt64" },
		{ "alias_uint", "AliasUInt" },
		{ "alias_uint64", "AliasUInt64" },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "enum", "EConversionEnum" },
		{ "int8", "int8" },
		{ "int", "int" },
		{ "int64", "int64" },
		{ "uint", "uint" },
		{ "uint64", "uint64" },
		{ "float64", "float64" },
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
		{ "near_min" },
		{ "near_max" },
	};

	static FString TargetTypeForCase(
		const FTargetCase& TargetCase,
		const asIScriptEngine& Engine)
	{
		if (EqualAnsi(TargetCase.CatalogName, "float64"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
				? TEXT("float")
				: TEXT("float64");
		}

		return ANSI_TO_TCHAR(TargetCase.ScriptType);
	}

	static const ANSICHAR* LiteralForSource(
		const FSourceCase& SourceCase,
		const FValueCase& ValueCase)
	{
		const bool bZero = EqualAnsi(ValueCase.CatalogName, "zero");
		const bool bOne = EqualAnsi(ValueCase.CatalogName, "one");
		const bool bNegative = EqualAnsi(ValueCase.CatalogName, "negative");
		const bool bNearMinimum = EqualAnsi(ValueCase.CatalogName, "near_min");

		if (EqualAnsi(SourceCase.CatalogName, "enum"))
		{
			return bZero ? "EConversionEnum::Zero"
				: bOne ? "EConversionEnum::One"
				: bNegative ? "EConversionEnum::Negative"
				: bNearMinimum ? "EConversionEnum::NearMinimum"
				: "EConversionEnum::NearMaximum";
		}

		const ANSICHAR* Value = bZero ? "0"
			: bOne ? "1"
			: bNegative ? "-1"
			: bNearMinimum ? "-126"
			: "126";
		return Value;
	}

	static int64 SourceValueForCase(
		const FSourceCase& SourceCase,
		const FValueCase& ValueCase)
	{
		const bool bZero = EqualAnsi(ValueCase.CatalogName, "zero");
		const bool bOne = EqualAnsi(ValueCase.CatalogName, "one");
		const bool bNegative = EqualAnsi(ValueCase.CatalogName, "negative");
		const bool bNearMinimum = EqualAnsi(ValueCase.CatalogName, "near_min");
		const int64 DeclaredValue = bZero ? 0
			: bOne ? 1
			: bNegative ? -1
			: bNearMinimum ? -126
			: 126;

		if (EqualAnsi(SourceCase.CatalogName, "alias_uint"))
		{
			return static_cast<uint32>(DeclaredValue);
		}

		if (EqualAnsi(SourceCase.CatalogName, "alias_uint64"))
		{
			return static_cast<int64>(static_cast<uint64>(DeclaredValue));
		}

		return DeclaredValue;
	}

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

	static uint64 NormalizeUnsigned(const int64 Value, const uint8 WidthInBytes)
	{
		const uint64 Mask = WidthInBytes == sizeof(uint64)
			? std::numeric_limits<uint64>::max()
			: ((uint64(1) << (WidthInBytes * 8)) - 1);
		return static_cast<uint64>(Value) & Mask;
	}

	static int64 NormalizeSigned(const int64 Value, const uint8 WidthInBytes)
	{
		const uint64 UnsignedValue = NormalizeUnsigned(Value, WidthInBytes);
		const uint64 SignBit = uint64(1) << (WidthInBytes * 8 - 1);
		if ((UnsignedValue & SignBit) == 0)
		{
			return static_cast<int64>(UnsignedValue);
		}

		const uint64 SignExtended = UnsignedValue | ~((uint64(1) << (WidthInBytes * 8)) - 1);
		return static_cast<int64>(SignExtended);
	}

	static FString BuildIndependentExpectedLiteral(
		const FSourceCase& SourceCase,
		const FTargetCase& TargetCase,
		const FValueCase& ValueCase)
	{
		const int64 SourceValue = SourceValueForCase(SourceCase, ValueCase);
		if (EqualAnsi(TargetCase.CatalogName, "enum"))
		{
			return EqualAnsi(ValueCase.CatalogName, "zero") ? TEXT("EConversionEnum::Zero")
				: EqualAnsi(ValueCase.CatalogName, "one") ? TEXT("EConversionEnum::One")
				: EqualAnsi(ValueCase.CatalogName, "negative") ? TEXT("EConversionEnum::Negative")
				: EqualAnsi(ValueCase.CatalogName, "near_min") ? TEXT("EConversionEnum::NearMinimum")
				: TEXT("EConversionEnum::NearMaximum");
		}

		if (EqualAnsi(TargetCase.CatalogName, "float64"))
		{
			const bool bUnsigned64Negative = EqualAnsi(SourceCase.CatalogName, "alias_uint64")
				&& EqualAnsi(ValueCase.CatalogName, "negative");
			const double FloatingValue = bUnsigned64Negative
				? 18446744073709551615.0
				: static_cast<double>(SourceValue);
			return FString::Printf(TEXT("%.17g"), FloatingValue);
		}

		const FNativeTypeCase* const NativeTarget = FindNativeType(TargetCase.CatalogName);
		if (NativeTarget == nullptr)
		{
			return TEXT("0");
		}

		if (NativeTarget->Category == ENativeValueCategory::UnsignedInteger)
		{
			return LexToString(NormalizeUnsigned(SourceValue, NativeTarget->WidthInBytes));
		}

		return LexToString(NormalizeSigned(SourceValue, NativeTarget->WidthInBytes));
	}

	static FString BuildEnumAliasConversionSource(
		const FSourceCase& SourceCase,
		const FString& TargetType,
		const FFormCase& FormCase,
		const FValueCase& ValueCase,
		const FString& FunctionName,
		const FString& ExpectedLiteral)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("enum EConversionEnum"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNearMinimum = -126,"));
		AppendGeneratedAsLine(Source, TEXT("\tNegative = -1,"));
		AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
		AppendGeneratedAsLine(Source, TEXT("\tOne = 1,"));
		AppendGeneratedAsLine(Source, TEXT("\tNearMaximum = 126"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("typedef int8 AliasInt8;"));
		AppendGeneratedAsLine(Source, TEXT("typedef int AliasInt;"));
		AppendGeneratedAsLine(Source, TEXT("typedef int64 AliasInt64;"));
		AppendGeneratedAsLine(Source, TEXT("typedef uint AliasUInt;"));
		AppendGeneratedAsLine(Source, TEXT("typedef uint64 AliasUInt64;"));
		AppendGeneratedAsLine(Source);
		AppendSimpleIdentityFunction(Source, TargetType, TEXT("PassEnumAliasTarget"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s ReturnEnumAliasTarget(%hs Value)"), *TargetType, SourceCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s %s()"), *TargetType, *FunctionName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%hs SourceValue = %hs;"), SourceCase.ScriptType, LiteralForSource(SourceCase, ValueCase)));

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
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = PassEnumAliasTarget(SourceValue);"), *TargetType));
		}
		else if (EqualAnsi(FormCase.CatalogName, "return"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Converted = ReturnEnumAliasTarget(SourceValue);"), *TargetType));
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
		AppendGeneratedAsLine(Source, TEXT("int VerifyEnumAliasConversion()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s ExpectedValue = %s;"), *TargetType, *ExpectedLiteral));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s ActualValue = %s();"), *TargetType, *FunctionName));
		AppendGeneratedAsLine(Source, TEXT("\treturn ActualValue == ExpectedValue ? 1 : 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

public:

	TEST_METHOD(SourcesByTargetFormAndValue)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-ENUM-ALIAS",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine NativeEngine;
		NativeEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			NativeEngine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = NativeEngine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Enum and alias conversion product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FSourceCase& SourceCase : SourceCases)
		{
			for (const FTargetCase& TargetCase : TargetCases)
			{
				for (const FFormCase& FormCase : FormCases)
				{
					for (const FValueCase& ValueCase : ValueCases)
					{
						const FString TargetType = TargetTypeForCase(TargetCase, *ScriptEngine);
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-CONV-ENUM-ALIAS",
							{ ANSI_TO_TCHAR(SourceCase.CatalogName), ANSI_TO_TCHAR(TargetCase.CatalogName),
								ANSI_TO_TCHAR(FormCase.CatalogName), ANSI_TO_TCHAR(ValueCase.CatalogName) }));
						const FString ModuleName = TEXT("EnumAliasConversion_") + Case.GetId();
						const FString ExpectedLiteral = BuildIndependentExpectedLiteral(
							SourceCase, TargetCase, ValueCase);
		const FString Source = BuildEnumAliasConversionSource(
							SourceCase,
							TargetType,
							FormCase,
							ValueCase,
							TEXT("RunEnumAliasConversion"),
							ExpectedLiteral);

						const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
						const FTCHARToUTF8 SourceUtf8(*Source);
						NativeEngine.Reset(*TestRunner);
						PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
						asIScriptModule* Module = nullptr;
						const int BuildResult = CompileNativeModule(
							ScriptEngine,
							ModuleNameUtf8.Get(),
							SourceUtf8.Get(),
							Module);

						if (BuildResult < 0)
						{
							ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(NativeEngine.GetMessages(), ModuleName),
								*Case.Describe(TEXT("nominal enum rejection should identify one conversion site"))));
						}
						else
						{
							ASSERT_THAT(IsNotNull(Module,
								*Case.Describe(TEXT("accepted enum or alias conversion should publish a module"))));
							asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TargetType, TEXT("RunEnumAliasConversion"));
							ASSERT_THAT(IsNotNull(Entry,
								*Case.Describe(TEXT("accepted enum or alias conversion should resolve exactly"))));
							if (Entry != nullptr)
							{
								ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl(TCHAR_TO_ANSI(*TargetType)),
									Entry->GetReturnTypeId(),
									*Case.Describe(TEXT("accepted enum or alias conversion should retain target metadata"))));
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
									ExecuteNoArgumentEntry(*ScriptEngine, *Entry),
									*Case.Describe(TEXT("accepted enum or alias conversion should execute"))));

								asIScriptFunction* const ValueVerifier = FindNoArgumentEntry(
									Module,
									TEXT("int"),
									TEXT("VerifyEnumAliasConversion"));
								ASSERT_THAT(IsNotNull(ValueVerifier,
									*Case.Describe(TEXT("accepted enum or alias conversion should publish its value verifier"))));
								if (ValueVerifier != nullptr)
								{
									int32 VerificationMarker = INDEX_NONE;
									ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
										ExecuteNoArgumentIntEntry(*ScriptEngine, *ValueVerifier, VerificationMarker),
										*Case.Describe(TEXT("enum or alias value verifier should execute after the selected form"))));
									ASSERT_THAT(AreEqual(1,
										VerificationMarker,
										*Case.Describe(TEXT("enum or alias conversion should preserve its converted value across the selected form"))));
								}
							}
						}

						ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
							*Case.Describe(TEXT("enum or alias cell should discard its module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
