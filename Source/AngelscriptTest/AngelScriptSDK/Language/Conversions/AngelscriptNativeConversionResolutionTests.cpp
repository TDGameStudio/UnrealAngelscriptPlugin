#include "AngelscriptNativeConversionTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::EqualAnsi;
using AngelscriptNativeTestSupport::FNativeTestEngine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConversionResolutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Conversions.Resolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FContextCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FConversionCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FOutcomeCase
	{
		const ANSICHAR* CatalogName;
		bool bExpectedCompile;
	};

	inline static constexpr FContextCase ContextCases[] =
	{
		{ "overload" },
		{ "operator" },
		{ "default_argument" },
		{ "property" },
		{ "index" },
		{ "conditional" },
	};

	inline static constexpr FConversionCase ConversionCases[] =
	{
		{ "identity" },
		{ "promotion" },
		{ "narrowing" },
		{ "constructor" },
		{ "operator" },
		{ "reference_cast" },
	};

	inline static constexpr FOutcomeCase OutcomeCases[] =
	{
		{ "exact", true },
		{ "selected_conversion", true },
		{ "ambiguous", false },
		{ "rejected", false },
	};

	static bool IsNamed(const FContextCase& Case, const ANSICHAR* Name)
	{
		return EqualAnsi(Case.CatalogName, Name);
	}

	static bool IsNamed(const FConversionCase& Case, const ANSICHAR* Name)
	{
		return EqualAnsi(Case.CatalogName, Name);
	}

	static bool IsNamed(const FOutcomeCase& Case, const ANSICHAR* Name)
	{
		return EqualAnsi(Case.CatalogName, Name);
	}

	static bool IsReferenceConversion(const FConversionCase& ConversionCase)
	{
		return IsNamed(ConversionCase, "reference_cast");
	}

	static FString SourceType(const FConversionCase& ConversionCase)
	{
		if (IsNamed(ConversionCase, "identity") || IsNamed(ConversionCase, "constructor"))
		{
			return TEXT("int");
		}

		if (IsNamed(ConversionCase, "promotion"))
		{
			return TEXT("int8");
		}

		if (IsNamed(ConversionCase, "narrowing"))
		{
			return TEXT("int64");
		}

		if (IsNamed(ConversionCase, "operator"))
		{
			return TEXT("FResolutionOperatorSource");
		}

		return TEXT("FResolutionReferenceDerived@");
	}

	static FString TargetType(
		const FConversionCase& ConversionCase,
		const FOutcomeCase& OutcomeCase)
	{
		if (IsNamed(OutcomeCase, "exact"))
		{
			return SourceType(ConversionCase);
		}

		if (IsNamed(ConversionCase, "constructor"))
		{
			return TEXT("FResolutionConstructed");
		}

		if (IsNamed(ConversionCase, "reference_cast"))
		{
			return TEXT("FResolutionReferenceBase@");
		}

		return TEXT("int");
	}

	static int32 ExpectedSourceMarker(const FConversionCase& ConversionCase)
	{
		if (IsNamed(ConversionCase, "identity"))
		{
			return 11;
		}

		if (IsNamed(ConversionCase, "promotion"))
		{
			return 12;
		}

		if (IsNamed(ConversionCase, "narrowing"))
		{
			return 13;
		}

		if (IsNamed(ConversionCase, "constructor"))
		{
			return 14;
		}

		if (IsNamed(ConversionCase, "operator"))
		{
			return 15;
		}

		return -1;
	}

	static int32 ExpectedSelectedMarker(
		const FConversionCase& ConversionCase,
		const FOutcomeCase& OutcomeCase)
	{
		if (IsNamed(OutcomeCase, "exact"))
		{
			return ExpectedSourceMarker(ConversionCase);
		}

		if (IsNamed(ConversionCase, "constructor"))
		{
			return 114;
		}

		if (IsNamed(ConversionCase, "operator"))
		{
			return 215;
		}

		return ExpectedSourceMarker(ConversionCase);
	}

	static int32 ExpectedContextResult(
		const FContextCase& ContextCase,
		const FConversionCase& ConversionCase,
		const FOutcomeCase& OutcomeCase)
	{
		const int32 Marker = ExpectedSelectedMarker(ConversionCase, OutcomeCase);
		if (IsNamed(ContextCase, "overload"))
		{
			return Marker + 100;
		}

		if (IsNamed(ContextCase, "operator"))
		{
			return Marker + 7;
		}

		if (IsNamed(ContextCase, "default_argument"))
		{
			return Marker + 5;
		}

		if (IsNamed(ContextCase, "index"))
		{
			return Marker + 1;
		}

		return Marker;
	}

	static FString ConditionalFallback(const FString& Type)
	{
		if (Type == TEXT("FResolutionConstructed"))
		{
			return TEXT("FResolutionConstructed(0)");
		}

		if (Type == TEXT("FResolutionOperatorSource"))
		{
			return TEXT("FResolutionOperatorSource(0)");
		}

		if (Type.EndsWith(TEXT("@")))
		{
			return TEXT("nullptr");
		}

		return TEXT("0");
	}

	static void AppendConversionDefinitions(
		FString& Source,
		const FConversionCase& ConversionCase,
		const FOutcomeCase& OutcomeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsNamed(ConversionCase, "constructor") && !IsNamed(OutcomeCase, "exact"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionConstructed"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Marker;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionConstructed(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tMarker = InValue + 100;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		if (IsNamed(ConversionCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionOperatorSource"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Marker;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionOperatorSource(int InMarker)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tMarker = InMarker;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opImplConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Marker + 200;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		if (IsReferenceConversion(ConversionCase))
		{
			AppendGeneratedAsLine(Source, TEXT("class FResolutionReferenceBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Marker;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class FResolutionReferenceDerived : FResolutionReferenceBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendSourceValue(
		FString& Source,
		const FConversionCase& ConversionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsNamed(ConversionCase, "identity"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint SourceValue = 11;"));
		}
		else if (IsNamed(ConversionCase, "promotion"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint8 SourceValue = 12;"));
		}
		else if (IsNamed(ConversionCase, "narrowing"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint64 SourceValue = 13;"));
		}
		else if (IsNamed(ConversionCase, "constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint SourceValue = 14;"));
		}
		else if (IsNamed(ConversionCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionOperatorSource SourceValue(15);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionReferenceDerived@ SourceValue = nullptr;"));
		}
	}

	static void AppendObservationHelper(
		FString& Source,
		const FString& Type)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int ObserveResolution(%s Value)"), *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (Type == TEXT("FResolutionConstructed") || Type == TEXT("FResolutionOperatorSource"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Marker;"));
		}
		else if (Type.EndsWith(TEXT("@")))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.Marker;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendNormalContextSurface(
		FString& Source,
		const FContextCase& ContextCase,
		const FString& Type)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FResolutionAlternative"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (IsNamed(ContextCase, "overload"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int SelectResolution(%s Value)"), *Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveResolution(Value) + 100;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int SelectResolution(FResolutionAlternative Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 999;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionOperatorContext"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint opAdd(%s Value)"), *Type));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn ObserveResolution(Value) + 7;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(FResolutionAlternative Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 999;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "default_argument"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("int DefaultResolution(%s Value, int Offset = 5)"), *Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveResolution(Value) + Offset;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int DefaultResolution(FResolutionAlternative Value, int Offset = 5)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 999 + Offset;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "property"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionHolder"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Result;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tvoid set_Value(%s Value) property"), *Type));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tResult = ObserveResolution(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tvoid set_Value(FResolutionAlternative Value) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tResult = 999;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "index"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionIndex"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint opIndex(%s Value)"), *Type));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn ObserveResolution(Value) + 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opIndex(FResolutionAlternative Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 999;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendAmbiguousContextSurface(
		FString& Source,
		const FContextCase& ContextCase,
		const FString& SourceValueType)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FResolutionAmbiguousLeft"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousLeft(%s Value)"), *SourceValueType));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FResolutionAmbiguousRight"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tFResolutionAmbiguousRight(%s Value)"), *SourceValueType));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (IsNamed(ContextCase, "overload") || IsNamed(ContextCase, "conditional"))
		{
			AppendGeneratedAsLine(Source, TEXT("int SelectAmbiguousResolution(FResolutionAmbiguousLeft Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int SelectAmbiguousResolution(FResolutionAmbiguousRight Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionAmbiguousOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(FResolutionAmbiguousLeft Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(FResolutionAmbiguousRight Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "default_argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("int DefaultAmbiguous(FResolutionAmbiguousLeft Value, int Offset = 5)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Offset;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int DefaultAmbiguous(FResolutionAmbiguousRight Value, int Offset = 5)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Offset;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "property"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionAmbiguousHolder"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tvoid set_Value(FResolutionAmbiguousLeft Value) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tvoid set_Value(FResolutionAmbiguousRight Value) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionAmbiguousIndex"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opIndex(FResolutionAmbiguousLeft Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opIndex(FResolutionAmbiguousRight Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 2;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendRejectedContextSurface(
		FString& Source,
		const FContextCase& ContextCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FResolutionRejected"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (IsNamed(ContextCase, "overload"))
		{
			AppendGeneratedAsLine(Source, TEXT("int SelectRejectedResolution(FResolutionRejected Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionRejectedOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opAdd(FResolutionRejected Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "default_argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("int DefaultRejectedResolution(FResolutionRejected Value, int Offset = 5)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Offset;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "property"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionRejectedHolder"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tvoid set_Value(FResolutionRejected Value) property"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (IsNamed(ContextCase, "index"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FResolutionRejectedIndex"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opIndex(FResolutionRejected Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendNormalRunBody(
		FString& Source,
		const FContextCase& ContextCase,
		const FConversionCase& ConversionCase,
		const FOutcomeCase& OutcomeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Type = TargetType(ConversionCase, OutcomeCase);
		AppendSourceValue(Source, ConversionCase);

		if (IsNamed(ContextCase, "overload"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectResolution(SourceValue);"));
		}
		else if (IsNamed(ContextCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionOperatorContext Context;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Context + SourceValue;"));
		}
		else if (IsNamed(ContextCase, "default_argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn DefaultResolution(SourceValue);"));
		}
		else if (IsNamed(ContextCase, "property"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionHolder Holder;"));
			AppendGeneratedAsLine(Source, TEXT("\tHolder.Value = SourceValue;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Holder.Result;"));
		}
		else if (IsNamed(ContextCase, "index"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionIndex Index;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Index[SourceValue];"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s TargetValue = true ? SourceValue : %s;"),
				*Type,
				*ConditionalFallback(Type)));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveResolution(TargetValue);"));
		}
	}

	static void AppendAmbiguousRunBody(
		FString& Source,
		const FContextCase& ContextCase,
		const FConversionCase& ConversionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceValue(Source, ConversionCase);
		if (IsNamed(ContextCase, "overload"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectAmbiguousResolution(SourceValue);"));
		}
		else if (IsNamed(ContextCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionAmbiguousOperator Context;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Context + SourceValue;"));
		}
		else if (IsNamed(ContextCase, "default_argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn DefaultAmbiguous(SourceValue);"));
		}
		else if (IsNamed(ContextCase, "property"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionAmbiguousHolder Holder;"));
			AppendGeneratedAsLine(Source, TEXT("\tHolder.Value = SourceValue;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsNamed(ContextCase, "index"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionAmbiguousIndex Index;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Index[SourceValue];"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectAmbiguousResolution(true ? SourceValue : SourceValue);"));
		}
	}

	static void AppendRejectedRunBody(
		FString& Source,
		const FContextCase& ContextCase,
		const FConversionCase& ConversionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendSourceValue(Source, ConversionCase);
		if (IsNamed(ContextCase, "overload"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectRejectedResolution(SourceValue);"));
		}
		else if (IsNamed(ContextCase, "operator"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionRejectedOperator Context;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Context + SourceValue;"));
		}
		else if (IsNamed(ContextCase, "default_argument"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn DefaultRejectedResolution(SourceValue);"));
		}
		else if (IsNamed(ContextCase, "property"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionRejectedHolder Holder;"));
			AppendGeneratedAsLine(Source, TEXT("\tHolder.Value = SourceValue;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsNamed(ContextCase, "index"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionRejectedIndex Index;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Index[SourceValue];"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionRejected TargetValue;"));
			AppendGeneratedAsLine(Source, TEXT("\tTargetValue = true ? SourceValue : TargetValue;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
	}

	static FString BuildResolutionSource(
		const FContextCase& ContextCase,
		const FConversionCase& ConversionCase,
		const FOutcomeCase& OutcomeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendConversionDefinitions(Source, ConversionCase, OutcomeCase);

		if (OutcomeCase.bExpectedCompile)
		{
			const FString Type = TargetType(ConversionCase, OutcomeCase);
			AppendObservationHelper(Source, Type);
			AppendNormalContextSurface(Source, ContextCase, Type);
		}
		else if (IsNamed(OutcomeCase, "ambiguous"))
		{
			AppendAmbiguousContextSurface(Source, ContextCase, SourceType(ConversionCase));
		}
		else
		{
			AppendRejectedContextSurface(Source, ContextCase);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunConversionResolution()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (OutcomeCase.bExpectedCompile)
		{
			AppendNormalRunBody(Source, ContextCase, ConversionCase, OutcomeCase);
		}
		else if (IsNamed(OutcomeCase, "ambiguous"))
		{
			AppendAmbiguousRunBody(Source, ContextCase, ConversionCase);
		}
		else
		{
			AppendRejectedRunBody(Source, ContextCase, ConversionCase);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

public:

	TEST_METHOD(ContextsByConversionAndOutcome)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CONV-OVERLOAD",
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
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Conversion-resolution product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FContextCase& ContextCase : ContextCases)
		{
			for (const FConversionCase& ConversionCase : ConversionCases)
			{
				for (const FOutcomeCase& OutcomeCase : OutcomeCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-CONV-OVERLOAD",
						{ ANSI_TO_TCHAR(ContextCase.CatalogName), ANSI_TO_TCHAR(ConversionCase.CatalogName),
							ANSI_TO_TCHAR(OutcomeCase.CatalogName) }));
					const FString ModuleName = TEXT("ConversionResolution_") + Case.GetId();
					const FString Source = BuildResolutionSource(ContextCase, ConversionCase, OutcomeCase);

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

					if (!OutcomeCase.bExpectedCompile)
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("ambiguous or rejected source conversion should not compile in its owning context"))));
						ASSERT_THAT(IsTrue(HasOwnedLocatedDiagnostic(NativeEngine.GetMessages(), ModuleName),
							*Case.Describe(TEXT("failed source conversion should identify its causal source location"))));
					}
					else if (BuildResult >= 0 && Module != nullptr)
					{
						asIScriptFunction* const Entry = FindNoArgumentEntry(Module, TEXT("int"), TEXT("RunConversionResolution"));
						ASSERT_THAT(IsNotNull(Entry,
							*Case.Describe(TEXT("resolution case should resolve the selected declaration exactly"))));
						if (Entry != nullptr)
						{
							int32 SelectedMarker = INDEX_NONE;
							asIScriptContext* const Context = ScriptEngine->CreateContext();
							ASSERT_THAT(IsNotNull(Context,
								*Case.Describe(TEXT("resolution case should create a context for execution diagnostics"))));
							int ExecuteResult = asERROR;
							if (Context != nullptr)
							{
								ExecuteResult = PrepareAndExecute(Context, Entry);
								if (ExecuteResult == asEXECUTION_FINISHED)
								{
									SelectedMarker = static_cast<int32>(Context->GetReturnDWord());
								}
								else
								{
									const ANSICHAR* const ExceptionString = Context->GetExceptionString();
									TestRunner->AddInfo(*Case.Describe(*FString::Printf(
										TEXT("conversion-resolution execution trace: result=%d exception='%hs' line=%d declaration='%hs'"),
										ExecuteResult,
										ExceptionString != nullptr ? ExceptionString : "",
										Context->GetExceptionLineNumber(),
										Entry->GetDeclaration())));
								}
								Context->Release();
							}
							ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
								ExecuteResult,
								*Case.Describe(TEXT("selected source conversion should execute"))));
							ASSERT_THAT(AreEqual(ExpectedContextResult(ContextCase, ConversionCase, OutcomeCase),
								SelectedMarker,
								*Case.Describe(TEXT("selected declaration should preserve the context-specific conversion marker"))));
						}
					}

					ASSERT_THAT(IsTrue(DiscardAndConfirmAbsent(*ScriptEngine, ModuleNameUtf8),
						*Case.Describe(TEXT("resolution case should discard its module"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
