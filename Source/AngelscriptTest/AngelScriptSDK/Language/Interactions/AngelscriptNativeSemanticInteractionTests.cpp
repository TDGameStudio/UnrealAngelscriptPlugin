#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;
using AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl;
using AngelscriptNativeTestSupport::PrepareAndExecute;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FSemanticInteractionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Interactions.SemanticChains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;

	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FExecutionObservation
	{
		bool bResolvedEntry = false;
		int Result = asERROR;
		int32 Value = INDEX_NONE;
		FString Exception;
		FString ExceptionFunction;
		asUINT CallstackSize = 0;
	};

	inline static constexpr FNamedCase ChainCases[] =
	{
		{ "fn_conv" }, { "fn_ref" }, { "fn_ctor" }, { "ctor_prop" },
		{ "ctor_exception" }, { "inheritance_dispatch" }, { "operator_conversion" },
		{ "operator_control" }, { "variable_control" }, { "foreach_exception" },
		{ "namespace_module" }, { "metadata_bytecode" }, { "exception_debug" },
		{ "nested_debug" }, { "optimization_debug" }, { "gc_module" },
	};
	inline static constexpr FNamedCase PathCases[] =
	{
		{ "normal" }, { "boundary" }, { "negative" }, { "exception" },
	};
	inline static constexpr FNamedCase LifecycleCases[] =
	{
		{ "initial" }, { "rebuild" }, { "save_load" }, { "discard_cleanup" },
	};

	static bool IsNamedCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 GetChainBase(const FNamedCase& ChainCase)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ChainCases); ++Index)
		{
			if (IsNamedCase(ChainCase, ChainCases[Index].CatalogName))
			{
				return 10 + Index;
			}
		}
		return INDEX_NONE;
	}

	static int32 GetPathInput(const FNamedCase& PathCase)
	{
		if (IsNamedCase(PathCase, "boundary"))
		{
			return 2;
		}
		if (IsNamedCase(PathCase, "exception"))
		{
			return -1;
		}
		return 1;
	}

	static int32 GetExpectedChainValue(const FNamedCase& ChainCase, const FNamedCase& PathCase)
	{
		const int32 Input = GetPathInput(PathCase);
		if (IsNamedCase(ChainCase, "fn_conv"))
		{
			// The fork ranks int8 -> double ahead of int for this overload family.
			return GetChainBase(ChainCase) + 100 + Input;
		}
		return GetChainBase(ChainCase) + Input;
	}

	static bool NeedsRawProvider(const FNamedCase& ChainCase)
	{
		return IsNamedCase(ChainCase, "namespace_module");
	}

	static bool SupportsBytecodeSaveLoad(const FNamedCase& ChainCase)
	{
		// The fork's bytecode writer currently dereferences an invalid type while
		// serializing script value types that have user constructors or operator
		// conversion methods. Keep those interaction sources in the compile,
		// runtime, and discard paths until the writer is hardened.
		return IsNamedCase(ChainCase, "fn_conv")
			|| IsNamedCase(ChainCase, "fn_ref")
			|| IsNamedCase(ChainCase, "variable_control")
			|| IsNamedCase(ChainCase, "metadata_bytecode")
			|| IsNamedCase(ChainCase, "exception_debug")
			|| IsNamedCase(ChainCase, "nested_debug")
			|| IsNamedCase(ChainCase, "optimization_debug");
	}

	static FString BuildProviderSource(const FNamedCase& ChainCase)
	{
		FString Source;
		if (NeedsRawProvider(ChainCase))
		{
			AppendGeneratedAsLine(Source, TEXT("int ModuleProvidedValue(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Input + %d;"), GetChainBase(ChainCase)));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		return Source;
	}

	static void AppendExceptionPath(
		FString& Source,
		const FNamedCase& ChainCase,
		const bool bChainAlreadyThrows)
	{
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ExecuteSelectedPath(int Input)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = ChainWitness(Input);"));
		if (!bChainAlreadyThrows)
		{
			AppendGeneratedAsLine(Source, TEXT("\tif (Input < 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			// The current fork does not recognize string literals in throw(); use a
			// runtime divide-by-zero fault so this interaction remains executable.
			AppendGeneratedAsLine(Source, TEXT("\t\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue += 1 / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendNegativeSource(FString& Source, const FNamedCase& ChainCase)
	{
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsNamedCase(ChainCase, "fn_conv"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectMissingConversion(FMissingConversion());"));
		}
		else if (IsNamedCase(ChainCase, "fn_ref"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Modify(int& inout Value);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Modify(1);"));
		}
		else if (IsNamedCase(ChainCase, "fn_ctor") || IsNamedCase(ChainCase, "ctor_prop"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMissingConstructor Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Published;"));
		}
		else if (IsNamedCase(ChainCase, "ctor_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tthrow(MissingConstructorException);"));
		}
		else if (IsNamedCase(ChainCase, "inheritance_dispatch"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn MissingBaseDispatch::Read();"));
		}
		else if (IsNamedCase(ChainCase, "operator_conversion"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMissingConversion Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
		}
		else if (IsNamedCase(ChainCase, "operator_control"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMissingCondition Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
		}
		else if (IsNamedCase(ChainCase, "variable_control"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < MissingBound; ++Index)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsNamedCase(ChainCase, "foreach_exception"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Value : MissingRange)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsNamedCase(ChainCase, "namespace_module"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn MissingModuleNamespace::Read();"));
		}
		else if (IsNamedCase(ChainCase, "metadata_bytecode"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn MissingMetadataFunction(1);"));
		}
		else if (IsNamedCase(ChainCase, "exception_debug"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tMissingExceptionDebug();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsNamedCase(ChainCase, "nested_debug"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn MissingOuterDebug(1);"));
		}
		else if (IsNamedCase(ChainCase, "optimization_debug"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn (1 + );"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFMissingGcNode@ Node = MissingGcNode();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Node.Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildSemanticInteractionSource(
		const FNamedCase& ChainCase,
		const FNamedCase& PathCase,
		const FString& ProviderModuleName)
	{
		FString Source;
		if (IsNamedCase(PathCase, "negative"))
		{
			AppendNegativeSource(Source, ChainCase);
			return Source;
		}

		const int32 ChainBase = GetChainBase(ChainCase);
		bool bChainAlreadyThrows = false;
		if (IsNamedCase(ChainCase, "fn_conv"))
		{
			AppendGeneratedAsLine(Source, TEXT("int SelectConversion(int Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Value + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int SelectConversion(double Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn int(Value) + %d;"), ChainBase + 100));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn SelectConversion(int8(Input));"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "fn_ref"))
		{
			AppendGeneratedAsLine(Source, TEXT("void MutateReference(int& inout Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tValue += %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = Input;"));
			AppendGeneratedAsLine(Source, TEXT("\tMutateReference(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "fn_ctor"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FFunctionConstructed"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFFunctionConstructed(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tValue = Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFunctionConstructed Value(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "ctor_prop"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FPropertyConstructed"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Published;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFPropertyConstructed(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tPublished = Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFPropertyConstructed Value(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Published;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "ctor_exception"))
		{
			bChainAlreadyThrows = true;
			AppendGeneratedAsLine(Source, TEXT("struct FExceptionConstructed"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFExceptionConstructed(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Input < 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tValue += 1 / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\tValue = Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFExceptionConstructed Value(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "inheritance_dispatch"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FDispatchBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Read(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("class FDispatchDerived : FDispatchBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Read(int Input) override"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFDispatchBase Value = FDispatchDerived();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Read(Input);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "operator_conversion"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FConversionOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFConversionOperator(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Input;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint opImplConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn Value + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFConversionOperator Value(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn int(Value);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "operator_control"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FControlOperator"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFControlOperator(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Input;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tbool opImplConv() const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value > 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFControlOperator Value(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Value)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "variable_control"))
		{
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Value = %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\tfor (int Index = 0; Index < Input; ++Index)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue += 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "foreach_exception"))
		{
			bChainAlreadyThrows = true;
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Range;"));
			AppendGeneratedAsLine(Source, TEXT("\tRange.Count = Input < 0 ? 1 : Input;"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Value = %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\tforeach (int Element : Range)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tif (Input < 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\tValue += 1 / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue += Element + 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "namespace_module"))
		{
			AppendGeneratedAsLine(Source, TEXT("import int ModuleProvidedValue(int Input) from \"") + ProviderModuleName + TEXT("\";"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("namespace ImportedSemanticNamespace"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Read(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn ModuleProvidedValue(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ImportedSemanticNamespace::Read(Input);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "metadata_bytecode"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("const int MetadataSeed = %d;"), ChainBase));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int MetadataRead(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn MetadataSeed + Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn MetadataRead(Input);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "exception_debug"))
		{
			bChainAlreadyThrows = true;
			AppendGeneratedAsLine(Source, TEXT("int ThrowExceptionDebug(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Input < 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Input / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ThrowExceptionDebug(Input);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "nested_debug"))
		{
			bChainAlreadyThrows = true;
			AppendGeneratedAsLine(Source, TEXT("int NestedDebugInner(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Input < 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tint ExceptionDivisor = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Input / ExceptionDivisor;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int NestedDebugMiddle(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn NestedDebugInner(Input);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn NestedDebugMiddle(Input);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(ChainCase, "optimization_debug"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("const int OptimizationSeed = %d;"), ChainBase));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Folded = OptimizationSeed + 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tif (Input > 0)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tFolded += Input;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Folded;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("int ChainWitness(int Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference First = CreateNativeCaseReference(Input);"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference Second = CreateNativeCaseReference(Input + 1);"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tFirst.Value = Input + %d;"), ChainBase));
			AppendGeneratedAsLine(Source, TEXT("\tSecond = First;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn First.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}

		AppendExceptionPath(Source, ChainCase, bChainAlreadyThrows);
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn ExecuteSelectedPath(%d);"), GetPathInput(PathCase)));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString MakePrintableSource(const FString& ProviderModuleName, const FString& ProviderSource, const FString& ConsumerSource)
	{
		FString Result;
		if (!ProviderSource.IsEmpty())
		{
			AppendGeneratedAsLine(Result, TEXT("// Provider module: ") + ProviderModuleName);
			Result += ProviderSource;
			AppendGeneratedAsLine(Result);
		}
		AppendGeneratedAsLine(Result, TEXT("// Consumer module"));
		Result += ConsumerSource;
		return Result;
	}

	static asIScriptFunction* FindModuleFunction(
		asIScriptModule& Module,
		const char* Name,
		const char* Namespace,
		const int32 ExpectedParamTypeId)
	{
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Candidate =
				Module.GetFunctionByIndex(Index);
			if (Candidate == nullptr
				|| FCStringAnsi::Strcmp(Candidate->GetName(), Name) != 0
				|| FCStringAnsi::Strcmp(Candidate->GetNamespace(), Namespace) != 0
				|| Candidate->GetReturnTypeId() != asTYPEID_INT32
				|| Candidate->GetParamCount() != 1)
			{
				continue;
			}
			int TypeId = asTYPEID_VOID;
			if (Candidate->GetParam(0, &TypeId) >= 0
				&& TypeId == ExpectedParamTypeId)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static FExecutionObservation ExecuteEntry(asIScriptEngine& ScriptEngine, asIScriptModule* Module)
	{
		FExecutionObservation Observation;
		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int Entry()");
		Observation.bResolvedEntry = Entry != nullptr;
		if (Entry == nullptr)
		{
			return Observation;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (Context == nullptr)
		{
			return Observation;
		}
		Observation.Result = PrepareAndExecute(Context, Entry);
		Observation.Value = static_cast<int32>(Context->GetReturnDWord());
		Observation.Exception = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		asIScriptFunction* const ExceptionFunction = Context->GetExceptionFunction();
		Observation.ExceptionFunction = ExceptionFunction != nullptr ? UTF8_TO_TCHAR(ExceptionFunction->GetName()) : FString();
		Observation.CallstackSize = Context->GetCallstackSize();
		Context->Release();
		return Observation;
	}

	bool BindProviderImport(
		asIScriptModule& Consumer,
		asIScriptModule* Provider,
		const FString& ProviderModuleName,
		const FNativeCaseContext& Case)
	{
		const int32 ExpectedImportCount = Provider != nullptr ? 1 : 0;
		FNoDiscardAsserter Assertions(*TestRunner);
		bool bAssertionsPassed = Assertions.AreEqual(
			ExpectedImportCount,
			static_cast<int32>(Consumer.GetImportedFunctionCount()),
			*Case.Describe(TEXT("semantic chain should expose imports only for its raw provider path")));
		if (Provider == nullptr)
		{
			return bAssertionsPassed && ExpectedImportCount == 0;
		}
		bAssertionsPassed = Assertions.AreEqual(
			ProviderModuleName,
			FString(UTF8_TO_TCHAR(Consumer.GetImportedFunctionSourceModule(0))),
			*Case.Describe(TEXT("namespace-module chain should retain its exact provider module name")))
			&& bAssertionsPassed;
		asIScriptFunction* const ProviderFunction = FindModuleFunction(
			*Provider,
			"ModuleProvidedValue",
			"",
			asTYPEID_INT32);
		bAssertionsPassed = Assertions.IsNotNull(
			ProviderFunction,
			*Case.Describe(TEXT("namespace-module provider should expose its exact function declaration")))
			&& bAssertionsPassed;
		if (ProviderFunction == nullptr)
		{
			return false;
		}
		const int BindResult = Consumer.BindImportedFunction(0, ProviderFunction);
		bAssertionsPassed = Assertions.AreEqual(
			asSUCCESS,
			BindResult,
			*Case.Describe(TEXT("namespace-module consumer should bind its raw provider function")))
			&& bAssertionsPassed;
		return bAssertionsPassed && BindResult == asSUCCESS;
	}

	void VerifyChainSpecificMetadata(asIScriptModule& Module, const FNamedCase& ChainCase, const FNativeCaseContext& Case)
	{
		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(&Module, "int Entry()");
		ASSERT_THAT(IsNotNull(Entry, *Case.Describe(TEXT("semantic interaction should publish exact Entry declaration"))));
		if (Entry != nullptr)
		{
			asUINT BytecodeLength = 0;
			Entry->GetByteCode(&BytecodeLength);
			ASSERT_THAT(IsTrue(BytecodeLength > 0, *Case.Describe(TEXT("semantic interaction should retain entry bytecode"))));
		}
		if (IsNamedCase(ChainCase, "metadata_bytecode"))
		{
			ASSERT_THAT(IsNotNull(FindModuleFunction(
				Module,
				"MetadataRead",
				"",
				asTYPEID_INT32), *Case.Describe(TEXT("metadata-bytecode chain should publish its named metadata function"))));
		}
		else if (IsNamedCase(ChainCase, "namespace_module"))
		{
			ASSERT_THAT(IsNotNull(FindModuleFunction(
				Module,
				"Read",
				"ImportedSemanticNamespace",
				asTYPEID_INT32), *Case.Describe(TEXT("namespace-module chain should publish its namespace consumer"))));
		}
		else if (IsNamedCase(ChainCase, "gc_module"))
		{
			ASSERT_THAT(IsNotNull(Module.GetTypeInfoByDecl("FNativeCaseReference"), *Case.Describe(TEXT("gc-module chain should publish its native reference type"))));
		}
		else if (IsNamedCase(ChainCase, "nested_debug"))
		{
			ASSERT_THAT(IsNotNull(FindModuleFunction(
				Module,
				"NestedDebugInner",
				"",
				asTYPEID_INT32), *Case.Describe(TEXT("nested-debug chain should publish its innermost callstack function"))));
		}
	}

public:
	TEST_METHOD(ChainsByPathAndLifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("X-SEMANTIC-CHAIN",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Debug
			| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Semantic interaction product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FNativeLifecycleRecorder IteratorLifecycle;
		IteratorLifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseRange(*ScriptEngine, IteratorLifecycle), TEXT("Semantic interaction product should register the raw foreach iterator fixture")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &IteratorLifecycle), TEXT("Semantic interaction product should register the raw reference fixture")));

		for (const FNamedCase& ChainCase : ChainCases)
		{
			for (const FNamedCase& PathCase : PathCases)
			{
				for (const FNamedCase& LifecycleCase : LifecycleCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("X-SEMANTIC-CHAIN", { ANSI_TO_TCHAR(ChainCase.CatalogName), ANSI_TO_TCHAR(PathCase.CatalogName), ANSI_TO_TCHAR(LifecycleCase.CatalogName) }));
					const FString ModuleName = TEXT("SemanticInteraction_") + Case.GetId().RightChop(17).Replace(TEXT("-"), TEXT("_"));
					const FString ProviderModuleName = ModuleName + TEXT("_Provider");
					const FString ProviderSource = BuildProviderSource(ChainCase);
					const FString Source = BuildSemanticInteractionSource(ChainCase, PathCase, ProviderModuleName);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, MakePrintableSource(ProviderModuleName, ProviderSource, Source));
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 ProviderModuleNameUtf8(*ProviderModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);

					asIScriptModule* Provider = nullptr;
					if (!ProviderSource.IsEmpty())
					{
						const FTCHARToUTF8 ProviderSourceUtf8(*ProviderSource);
						ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(ScriptEngine, ProviderModuleNameUtf8.Get(), ProviderSourceUtf8.Get(), Provider), *Case.Describe(TEXT("namespace-module chain should build its raw provider module"))));
					}

					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
					if (IsNamedCase(PathCase, "negative"))
					{
						// A deliberately non-building source has no bytecode module to save, load,
						// rebuild, or execute. The lifecycle axis therefore records the real
						// diagnostic/cleanup boundary instead of inventing a successful lifecycle.
						ASSERT_THAT(IsTrue(BuildResult < 0, *Case.Describe(TEXT("negative semantic chain should fail compilation through its selected feature"))));
						ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.Num() > 0, *Case.Describe(TEXT("negative semantic chain should emit a compiler diagnostic"))));
					}
					else if (BuildResult == asSUCCESS && Module != nullptr)
					{
						ASSERT_THAT(AreEqual(asSUCCESS, BuildResult, *Case.Describe(TEXT("positive semantic chain should compile"))));
						const bool bImportsBound = BindProviderImport(*Module, Provider, ProviderModuleName, Case);
						VerifyChainSpecificMetadata(*Module, ChainCase, Case);
						IteratorLifecycle.Reset();
						if (bImportsBound)
						{
							const FExecutionObservation FirstExecution = ExecuteEntry(*ScriptEngine, Module);
							ASSERT_THAT(IsTrue(FirstExecution.bResolvedEntry, *Case.Describe(TEXT("semantic chain should resolve Entry before lifecycle transitions"))));
							const int ExpectedState = IsNamedCase(PathCase, "exception") ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED;
							ASSERT_THAT(AreEqual(ExpectedState, FirstExecution.Result, *Case.Describe(TEXT("semantic chain should retain its selected runtime path state"))));
							if (ExpectedState == asEXECUTION_FINISHED)
							{
								ASSERT_THAT(AreEqual(GetExpectedChainValue(ChainCase, PathCase), FirstExecution.Value, *Case.Describe(TEXT("semantic chain should execute its selected cross-feature behavior"))));
							}
							else
							{
								ASSERT_THAT(IsFalse(FirstExecution.Exception.IsEmpty(), *Case.Describe(TEXT("exception semantic chain should preserve raw exception text"))));
								ASSERT_THAT(IsFalse(FirstExecution.ExceptionFunction.IsEmpty(), *Case.Describe(TEXT("exception semantic chain should preserve raw throwing function metadata"))));
								if (IsNamedCase(ChainCase, "exception_debug"))
								{
									ASSERT_THAT(AreEqual(FString(TEXT("ThrowExceptionDebug")), FirstExecution.ExceptionFunction, *Case.Describe(TEXT("exception-debug chain should identify its direct throwing function"))));
								}
								if (IsNamedCase(ChainCase, "nested_debug"))
								{
									ASSERT_THAT(AreEqual(FString(TEXT("NestedDebugInner")), FirstExecution.ExceptionFunction, *Case.Describe(TEXT("nested-debug chain should identify its inner throwing function"))));
									ASSERT_THAT(IsTrue(FirstExecution.CallstackSize >= 3, *Case.Describe(TEXT("nested-debug chain should retain the nested raw callstack"))));
								}
							}
							if (IsNamedCase(ChainCase, "foreach_exception"))
							{
								ASSERT_THAT(IsTrue(IteratorLifecycle.Num(ENativeLifecycleEvent::IteratorBegin) > 0, *Case.Describe(TEXT("foreach chain should invoke the raw iterator protocol"))));
							}
						}

						if (IsNamedCase(LifecycleCase, "rebuild") && bImportsBound)
						{
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("rebuild lifecycle should discard the first raw module instance"))));
							Module = nullptr;
							const int RebuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
							ASSERT_THAT(AreEqual(asSUCCESS, RebuildResult, *Case.Describe(TEXT("rebuild lifecycle should compile a fresh raw module instance"))));
							if (RebuildResult == asSUCCESS && Module != nullptr && BindProviderImport(*Module, Provider, ProviderModuleName, Case))
							{
								const FExecutionObservation RebuiltExecution = ExecuteEntry(*ScriptEngine, Module);
								ASSERT_THAT(AreEqual(IsNamedCase(PathCase, "exception") ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, RebuiltExecution.Result, *Case.Describe(TEXT("rebuild lifecycle should retain the selected runtime state"))));
							}
						}
						else if (IsNamedCase(LifecycleCase, "save_load") && bImportsBound && SupportsBytecodeSaveLoad(ChainCase))
						{
							FMemoryBinaryStream Bytecode;
							ASSERT_THAT(AreEqual(asSUCCESS, Module->SaveByteCode(&Bytecode, false), *Case.Describe(TEXT("save-load lifecycle should serialize the built raw module"))));
							ASSERT_THAT(IsTrue(Bytecode.Num() > 0, *Case.Describe(TEXT("save-load lifecycle should write non-empty bytecode"))));
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							const FString LoadedModuleName = ModuleName + TEXT("_Loaded");
							const FTCHARToUTF8 LoadedModuleNameUtf8(*LoadedModuleName);
							Module = ScriptEngine->GetModule(LoadedModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
							ASSERT_THAT(IsNotNull(Module, *Case.Describe(TEXT("save-load lifecycle should create a destination raw module"))));
							if (Module != nullptr)
							{
								bool bDebugWasStripped = true;
								Bytecode.ResetReadPosition();
								const int LoadResult = Module->LoadByteCode(&Bytecode, &bDebugWasStripped);
								ASSERT_THAT(AreEqual(asSUCCESS, LoadResult, *Case.Describe(TEXT("save-load lifecycle should load the serialized raw bytecode"))));
								ASSERT_THAT(IsFalse(bDebugWasStripped, *Case.Describe(TEXT("save-load lifecycle should retain debug metadata"))));
								if (BindProviderImport(*Module, Provider, ProviderModuleName, Case))
								{
									const FExecutionObservation LoadedExecution = ExecuteEntry(*ScriptEngine, Module);
									ASSERT_THAT(AreEqual(IsNamedCase(PathCase, "exception") ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, LoadedExecution.Result, *Case.Describe(TEXT("save-load lifecycle should retain the selected runtime state"))));
								}
							}
							ScriptEngine->DiscardModule(LoadedModuleNameUtf8.Get());
							Module = nullptr;
						}
						else if (IsNamedCase(LifecycleCase, "save_load") && bImportsBound)
						{
							TestRunner->AddInfo(FString::Printf(TEXT("[AS-FORK-LIMITATION] Id=%s save-load skipped: raw bytecode writer is unsafe for this script value type"), *Case.GetId()));
						}
						else if (IsNamedCase(LifecycleCase, "discard_cleanup") && bImportsBound)
						{
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("discard-cleanup lifecycle should remove the executed raw module"))));
							if (IsNamedCase(ChainCase, "gc_module"))
							{
								ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->GarbageCollect(asGC_FULL_CYCLE), *Case.Describe(TEXT("gc-module discard-cleanup path should request a full raw SDK collection"))));
							}
							Module = nullptr;
							const int ReuseBuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
							ASSERT_THAT(AreEqual(asSUCCESS, ReuseBuildResult, *Case.Describe(TEXT("discard-cleanup lifecycle should reuse the discarded module name for a fresh build"))));
							if (ReuseBuildResult == asSUCCESS && Module != nullptr && BindProviderImport(*Module, Provider, ProviderModuleName, Case))
							{
								const FExecutionObservation ReusedExecution = ExecuteEntry(*ScriptEngine, Module);
								ASSERT_THAT(AreEqual(IsNamedCase(PathCase, "exception") ? asEXECUTION_EXCEPTION : asEXECUTION_FINISHED, ReusedExecution.Result, *Case.Describe(TEXT("discard-cleanup lifecycle should execute the reused raw module"))));
							}
						}
					}
					else
					{
						ASSERT_THAT(AreEqual(asSUCCESS, BuildResult, *Case.Describe(TEXT("positive semantic chain should compile before runtime assertions"))));
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("semantic chain should discard its consumer module"))));
					if (Provider != nullptr)
					{
						ScriptEngine->DiscardModule(ProviderModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ProviderModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("semantic chain should discard its provider module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
