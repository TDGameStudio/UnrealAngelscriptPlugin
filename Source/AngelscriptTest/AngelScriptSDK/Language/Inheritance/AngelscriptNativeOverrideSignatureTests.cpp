#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FOverrideSignatureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Inheritance.OverrideSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class ESignatureDimension : uint8
	{
		ParameterType,
		ParameterCount,
		ReturnType,
		Constness,
		Visibility,
		NameHiding,
	};

	enum class ESignatureVariant : uint8
	{
		Exact,
		CompatibleOverload,
		Incompatible,
	};

	enum class ESignatureView : uint8
	{
		Base,
		Derived,
		ExplicitBase,
	};

	struct FDimensionCase
	{
		const ANSICHAR* CatalogName;
		ESignatureDimension Dimension;
	};

	struct FVariantCase
	{
		const ANSICHAR* CatalogName;
		ESignatureVariant Variant;
	};

	struct FViewCase
	{
		const ANSICHAR* CatalogName;
		ESignatureView View;
		const ANSICHAR* ProbeDeclaration;
		const ANSICHAR* RuntimeDeclaration;
	};

	struct FSignatureShape
	{
		FString BaseAccess;
		FString BaseDeclaration;
		FString BaseArgument;
		FString DerivedAccess;
		FString DerivedDeclaration;
		FString DerivedArgument;
		FString DerivedResultExpression;
		FString ExpectedDiagnostic;
		bool bBuildAccepted = true;
		bool bDerivedOverrides = false;
	};

	inline static constexpr FDimensionCase DimensionCases[] =
	{
		{ "parameter_type", ESignatureDimension::ParameterType },
		{ "parameter_count", ESignatureDimension::ParameterCount },
		{ "return_type", ESignatureDimension::ReturnType },
		{ "constness", ESignatureDimension::Constness },
		{ "visibility", ESignatureDimension::Visibility },
		{ "name_hiding", ESignatureDimension::NameHiding },
	};

	inline static constexpr FVariantCase VariantCases[] =
	{
		{ "exact", ESignatureVariant::Exact },
		{
			"compatible_overload",
			ESignatureVariant::CompatibleOverload,
		},
		{ "incompatible", ESignatureVariant::Incompatible },
	};

	inline static constexpr FViewCase ViewCases[] =
	{
		{
			"base",
			ESignatureView::Base,
			"int FSignatureBase::ProbeBase()",
			"int RunSignatureBase(FSignatureBase)",
		},
		{
			"derived",
			ESignatureView::Derived,
			"int FSignaturePrimary::ProbeDerived()",
			"int RunSignatureDerived(FSignaturePrimary)",
		},
		{
			"explicit_base",
			ESignatureView::ExplicitBase,
			"int FSignaturePrimary::ProbeExplicitBase()",
			"int RunSignatureExplicitBase(FSignaturePrimary)",
		},
	};

	static FSignatureShape MakeParameterTypeShape(
		const ESignatureVariant Variant)
	{
		FSignatureShape Shape;
		Shape.BaseDeclaration = TEXT("int Resolve(int Value)");
		Shape.BaseArgument = TEXT("1");
		Shape.DerivedDeclaration =
			TEXT("int Resolve(int Value) override");
		Shape.DerivedArgument = TEXT("1");
		Shape.DerivedResultExpression = TEXT("Resolve(1)");
		Shape.bDerivedOverrides = true;
		if (Variant == ESignatureVariant::CompatibleOverload)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(float Value)");
			Shape.DerivedArgument = TEXT("1.5f");
			Shape.DerivedResultExpression =
				TEXT("Resolve(1.5f)");
			Shape.bDerivedOverrides = false;
		}
		else if (Variant == ESignatureVariant::Incompatible)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(float Value) override");
			Shape.DerivedArgument = TEXT("1.5f");
			Shape.DerivedResultExpression =
				TEXT("Resolve(1.5f)");
			Shape.ExpectedDiagnostic =
				TEXT("marked as override but does not replace any base class or interface method");
			Shape.bBuildAccepted = false;
			Shape.bDerivedOverrides = false;
		}
		return Shape;
	}

	static FSignatureShape MakeParameterCountShape(
		const ESignatureVariant Variant)
	{
		FSignatureShape Shape;
		Shape.BaseDeclaration = TEXT("int Resolve(int Value)");
		Shape.BaseArgument = TEXT("1");
		Shape.DerivedDeclaration =
			TEXT("int Resolve(int Value) override");
		Shape.DerivedArgument = TEXT("1");
		Shape.DerivedResultExpression = TEXT("Resolve(1)");
		Shape.bDerivedOverrides = true;
		if (Variant == ESignatureVariant::CompatibleOverload)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(int Left, int Right)");
			Shape.DerivedArgument = TEXT("1, 2");
			Shape.DerivedResultExpression =
				TEXT("Resolve(1, 2)");
			Shape.bDerivedOverrides = false;
		}
		else if (Variant == ESignatureVariant::Incompatible)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(int Left, int Right) override");
			Shape.DerivedArgument = TEXT("1, 2");
			Shape.DerivedResultExpression =
				TEXT("Resolve(1, 2)");
			Shape.ExpectedDiagnostic =
				TEXT("marked as override but does not replace any base class or interface method");
			Shape.bBuildAccepted = false;
			Shape.bDerivedOverrides = false;
		}
		return Shape;
	}

	static FSignatureShape MakeReturnTypeShape(
		const ESignatureVariant Variant)
	{
		FSignatureShape Shape;
		Shape.BaseDeclaration = TEXT("int Resolve(int Value)");
		Shape.BaseArgument = TEXT("1");
		Shape.DerivedDeclaration =
			TEXT("int Resolve(int Value) override");
		Shape.DerivedArgument = TEXT("1");
		Shape.DerivedResultExpression = TEXT("Resolve(1)");
		Shape.bDerivedOverrides = true;
		if (Variant == ESignatureVariant::CompatibleOverload)
		{
			Shape.DerivedDeclaration =
				TEXT("float Resolve(float Value)");
			Shape.DerivedArgument = TEXT("1.5f");
			Shape.DerivedResultExpression =
				TEXT("int(Resolve(1.5f))");
			Shape.bDerivedOverrides = false;
		}
		else if (Variant == ESignatureVariant::Incompatible)
		{
			Shape.DerivedDeclaration =
				TEXT("float Resolve(int Value) override");
			Shape.DerivedArgument = TEXT("1");
			Shape.DerivedResultExpression =
				TEXT("int(Resolve(1))");
			Shape.ExpectedDiagnostic =
				TEXT("must have the same return type as in the base class");
			Shape.bBuildAccepted = false;
			Shape.bDerivedOverrides = false;
		}
		return Shape;
	}

	static FSignatureShape MakeConstnessShape(
		const ESignatureVariant Variant)
	{
		FSignatureShape Shape;
		Shape.BaseDeclaration =
			TEXT("int Resolve(int Value) const");
		Shape.BaseArgument = TEXT("1");
		Shape.DerivedDeclaration =
			TEXT("int Resolve(int Value) const override");
		Shape.DerivedArgument = TEXT("1");
		Shape.DerivedResultExpression = TEXT("Resolve(1)");
		Shape.bDerivedOverrides = true;
		if (Variant == ESignatureVariant::CompatibleOverload)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(int Value)");
			Shape.bDerivedOverrides = false;
		}
		else if (Variant == ESignatureVariant::Incompatible)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(int Value) override");
			Shape.ExpectedDiagnostic =
				TEXT("marked as override but does not replace any base class or interface method");
			Shape.bBuildAccepted = false;
			Shape.bDerivedOverrides = false;
		}
		return Shape;
	}

	static FSignatureShape MakeVisibilityShape(
		const ESignatureVariant Variant)
	{
		FSignatureShape Shape;
		Shape.BaseAccess = TEXT("protected ");
		Shape.BaseDeclaration = TEXT("int Resolve(int Value)");
		Shape.BaseArgument = TEXT("1");
		Shape.DerivedAccess = TEXT("protected ");
		Shape.DerivedDeclaration =
			TEXT("int Resolve(int Value) override");
		Shape.DerivedArgument = TEXT("1");
		Shape.DerivedResultExpression = TEXT("Resolve(1)");
		Shape.bDerivedOverrides = true;
		if (Variant == ESignatureVariant::CompatibleOverload)
		{
			Shape.DerivedAccess.Reset();
			Shape.DerivedDeclaration =
				TEXT("int Resolve(float Value)");
			Shape.DerivedArgument = TEXT("1.5f");
			Shape.DerivedResultExpression =
				TEXT("Resolve(1.5f)");
			Shape.bDerivedOverrides = false;
		}
		else if (Variant == ESignatureVariant::Incompatible)
		{
			Shape.BaseAccess = TEXT("private ");
			Shape.DerivedAccess.Reset();
			Shape.DerivedDeclaration =
				TEXT("int Resolve(int Value)");
			Shape.ExpectedDiagnostic =
				TEXT("Illegal call to private method");
			Shape.bBuildAccepted = false;
			Shape.bDerivedOverrides = false;
		}
		return Shape;
	}

	static FSignatureShape MakeNameHidingShape(
		const ESignatureVariant Variant)
	{
		FSignatureShape Shape;
		Shape.BaseDeclaration = TEXT("int Resolve(int Value)");
		Shape.BaseArgument = TEXT("1");
		Shape.DerivedDeclaration =
			TEXT("int Resolve(int Value) override");
		Shape.DerivedArgument = TEXT("1");
		Shape.DerivedResultExpression = TEXT("Resolve(1)");
		Shape.bDerivedOverrides = true;
		if (Variant == ESignatureVariant::CompatibleOverload)
		{
			Shape.DerivedDeclaration =
				TEXT("int Resolve(float Value)");
			Shape.DerivedArgument = TEXT("2.5f");
			Shape.DerivedResultExpression =
				TEXT("Resolve(2.5f)");
			Shape.bDerivedOverrides = false;
		}
		else if (Variant == ESignatureVariant::Incompatible)
		{
			Shape.DerivedDeclaration =
				TEXT("int ResolveRenamed(int Value) override");
			Shape.DerivedArgument = TEXT("1");
			Shape.DerivedResultExpression =
				TEXT("ResolveRenamed(1)");
			Shape.ExpectedDiagnostic =
				TEXT("marked as override but does not replace any base class or interface method");
			Shape.bBuildAccepted = false;
			Shape.bDerivedOverrides = false;
		}
		return Shape;
	}

	static FSignatureShape MakeSignatureShape(
		const FDimensionCase& Dimension,
		const FVariantCase& Variant)
	{
		switch (Dimension.Dimension)
		{
		case ESignatureDimension::ParameterType:
			return MakeParameterTypeShape(Variant.Variant);
		case ESignatureDimension::ParameterCount:
			return MakeParameterCountShape(Variant.Variant);
		case ESignatureDimension::ReturnType:
			return MakeReturnTypeShape(Variant.Variant);
		case ESignatureDimension::Constness:
			return MakeConstnessShape(Variant.Variant);
		case ESignatureDimension::Visibility:
			return MakeVisibilityShape(Variant.Variant);
		case ESignatureDimension::NameHiding:
			return MakeNameHidingShape(Variant.Variant);
		default:
			return FSignatureShape();
		}
	}

	static void AppendMethod(
		FString& Source,
		const FString& Access,
		const FString& Declaration,
		const FString& ReturnExpression)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("\t") + Access + Declaration);
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\treturn ") + ReturnExpression + TEXT(";"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendBaseType(
		FString& Source,
		const FSignatureShape& Shape)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FSignatureBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendMethod(
			Source,
			Shape.BaseAccess,
			Shape.BaseDeclaration,
			TEXT("101 + Value"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint ProbeBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\treturn Resolve(")
				+ Shape.BaseArgument + TEXT(");"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString DerivedReturnExpression(
		const FSignatureShape& Shape)
	{
		if (Shape.DerivedDeclaration.StartsWith(TEXT("float ")))
		{
			return TEXT("202.0f + float(Value)");
		}
		if (Shape.DerivedDeclaration.Contains(TEXT("Left")))
		{
			return TEXT("202 + Left + Right");
		}
		return TEXT("202 + int(Value)");
	}

	static void AppendDerivedType(
		FString& Source,
		const FSignatureShape& Shape)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("class FSignaturePrimary : FSignatureBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendMethod(
			Source,
			Shape.DerivedAccess,
			Shape.DerivedDeclaration,
			DerivedReturnExpression(Shape));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint ProbeDerived()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\treturn ")
				+ Shape.DerivedResultExpression + TEXT(";"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("\tint ProbeExplicitBase()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\treturn FSignatureBase::Resolve(")
				+ Shape.BaseArgument + TEXT(");"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRuntimeWrapper(
		FString& Source,
		const TCHAR* Name,
		const TCHAR* Type,
		const TCHAR* Probe)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("int %s(%s Object)"), Name, Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("\treturn Object.%s();"), Probe));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildOverrideSignatureSource(
		const FSignatureShape& Shape)
	{
		FString Source;
		AppendBaseType(Source, Shape);
		AppendDerivedType(Source, Shape);
		AppendRuntimeWrapper(
			Source,
			TEXT("RunSignatureBase"),
			TEXT("FSignatureBase"),
			TEXT("ProbeBase"));
		AppendRuntimeWrapper(
			Source,
			TEXT("RunSignatureDerived"),
			TEXT("FSignaturePrimary"),
			TEXT("ProbeDerived"));
		AppendRuntimeWrapper(
			Source,
			TEXT("RunSignatureExplicitBase"),
			TEXT("FSignaturePrimary"),
			TEXT("ProbeExplicitBase"));
		return Source;
	}

	static FString BuildOverrideSignatureRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FSignatureBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Resolve(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("class FSignaturePrimary : FSignatureBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Resolve(int Value) override"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Value + 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunOverrideSignatureRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 211;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(
			Test,
			SourceId,
			ModuleName,
			Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static bool HasAnyError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR;
			});
	}

	static bool HasLocatedErrorContaining(
		const FNativeTestEngine& Engine,
		const FString& ExpectedText)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[&ExpectedText](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0
					&& Entry.Message.Contains(ExpectedText);
			});
	}

	static asIScriptFunction* FindMethod(
		asITypeInfo& Type,
		const FString& Declaration)
	{
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		if (asIScriptFunction* Function = Type.GetMethodByDecl(DeclarationUtf8.Get()))
		{
			return Function;
		}
		const int32 ParameterEnd = Declaration.Find(TEXT(")"));
		if (ParameterEnd <= 0)
		{
			return nullptr;
		}
		FString NormalizedExpected = Declaration.Left(ParameterEnd);
		NormalizedExpected.ReplaceInline(TEXT("const "), TEXT(""));
		NormalizedExpected += Declaration.Mid(ParameterEnd);
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Type.GetMethodByIndex(Index);
			if (Candidate == nullptr)
			{
				continue;
			}
			const FString CandidateDeclaration = UTF8_TO_TCHAR(Candidate->GetDeclaration());
			const int32 CandidateParameterEnd = CandidateDeclaration.Find(TEXT(")"));
			if (CandidateParameterEnd <= 0)
			{
				continue;
			}
			FString NormalizedCandidate = CandidateDeclaration.Left(CandidateParameterEnd);
			const int32 ScopeSeparator = NormalizedCandidate.Find(TEXT("::"));
			if (ScopeSeparator >= 0)
			{
				const int32 ReturnSeparator = NormalizedCandidate.Find(TEXT(" "));
				if (ReturnSeparator >= 0 && ScopeSeparator > ReturnSeparator)
				{
					NormalizedCandidate =
						NormalizedCandidate.Left(ReturnSeparator + 1)
						+ NormalizedCandidate.Mid(ScopeSeparator + 2);
				}
			}
			NormalizedCandidate.ReplaceInline(TEXT("const "), TEXT(""));
			NormalizedCandidate += CandidateDeclaration.Mid(CandidateParameterEnd);
			if (NormalizedCandidate == NormalizedExpected)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindModuleFunction(
		asIScriptModule& Module,
		const FString& Declaration)
	{
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get()))
		{
			return Function;
		}
		const int32 ParameterBegin = Declaration.Find(TEXT("("));
		const int32 ParameterEnd = Declaration.Find(TEXT(")"));
		if (ParameterBegin < 0 || ParameterEnd <= ParameterBegin)
		{
			return nullptr;
		}
		const FString ConstDeclaration =
			Declaration.Left(ParameterBegin + 1)
			+ TEXT("const ")
			+ Declaration.Mid(ParameterBegin + 1, ParameterEnd - ParameterBegin - 1)
			+ Declaration.Mid(ParameterEnd);
		const FTCHARToUTF8 ConstDeclarationUtf8(*ConstDeclaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(ConstDeclarationUtf8.Get()))
		{
			return Function;
		}
		const int32 NameBegin = Declaration.Find(TEXT(" ")) + 1;
		if (NameBegin <= 0 || ParameterBegin <= NameBegin)
		{
			return nullptr;
		}
		const FString ExpectedName =
			Declaration.Mid(NameBegin, ParameterBegin - NameBegin);
		const FTCHARToUTF8 ExpectedNameUtf8(*ExpectedName);
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(Index);
			if (Candidate != nullptr
				&& Candidate->GetName() != nullptr
				&& FCStringAnsi::Strcmp(Candidate->GetName(), ExpectedNameUtf8.Get()) == 0
				&& Candidate->GetParamCount() == 1)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static FString DeclarationWithoutParameterNames(
		const FString& Declaration)
	{
		FString Result = Declaration;
		Result.ReplaceInline(TEXT(" Value"), TEXT(""));
		Result.ReplaceInline(TEXT(" Left"), TEXT(""));
		Result.ReplaceInline(TEXT(" Right"), TEXT(""));
		return Result;
	}

	static bool BytecodeCallsFunction(
		asIScriptFunction& Probe,
		const int32 ExpectedFunctionId,
		const bool bRequireDirectCall)
	{
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode =
			Probe.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(
					*reinterpret_cast<const asBYTE*>(
						&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode)
				> static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			if ((Opcode == asBC_CALL || Opcode == asBC_CALLINTF)
				&& asBC_INTARG(&Bytecode[DwordIndex])
					== ExpectedFunctionId
				&& (!bRequireDirectCall || Opcode == asBC_CALL))
			{
				return true;
			}

			const int32 InstructionSize =
				asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return false;
	}

	static FString ExpectedDerivedDeclaration(
		const FSignatureShape& Shape)
	{
		return DeclarationWithoutParameterNames(
			Shape.DerivedDeclaration.Replace(
				TEXT(" override"),
				TEXT("")));
	}

	static FString ExpectedBaseDeclaration(
		const FSignatureShape& Shape)
	{
		return DeclarationWithoutParameterNames(
			Shape.BaseDeclaration);
	}

	void VerifyMethodMetadata(
		const FNativeCaseContext& Case,
		const FDimensionCase& Dimension,
		const FVariantCase& Variant,
		const FSignatureShape& Shape,
		asITypeInfo& Base,
		asITypeInfo& Primary,
		asIScriptFunction*& OutBaseMethod,
		asIScriptFunction*& OutDerivedMethod)
	{
		const FString BaseDeclaration =
			ExpectedBaseDeclaration(Shape);
		const FString DerivedDeclaration =
			ExpectedDerivedDeclaration(Shape);
		OutBaseMethod = FindMethod(Base, BaseDeclaration);
		OutDerivedMethod = FindMethod(Primary, DerivedDeclaration);
		ASSERT_THAT(IsNotNull(OutBaseMethod,
			*Case.Describe(TEXT("signature source should publish the exact base method"))));
		ASSERT_THAT(IsNotNull(OutDerivedMethod,
			*Case.Describe(TEXT("signature source should publish the exact derived method"))));
		if (OutBaseMethod == nullptr || OutDerivedMethod == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			&Base,
			OutBaseMethod->GetObjectType(),
			*Case.Describe(TEXT("base signature should retain base ownership"))));
		ASSERT_THAT(AreEqual(
			&Primary,
			OutDerivedMethod->GetObjectType(),
			*Case.Describe(TEXT("derived signature should retain derived ownership"))));
		ASSERT_THAT(AreEqual(
			Shape.bDerivedOverrides,
			OutDerivedMethod->IsOverride(),
			*Case.Describe(TEXT("derived override metadata should match its classified variant"))));

		if (Variant.Variant == ESignatureVariant::Exact)
		{
			ASSERT_THAT(IsTrue(
				OutBaseMethod != OutDerivedMethod,
				*Case.Describe(TEXT("exact override should replace with a distinct function identity"))));
			ASSERT_THAT(AreEqual(
				OutBaseMethod->GetParamCount(),
				OutDerivedMethod->GetParamCount(),
				*Case.Describe(TEXT("exact override should preserve parameter count"))));
			ASSERT_THAT(AreEqual(
				OutBaseMethod->GetReturnTypeId(),
				OutDerivedMethod->GetReturnTypeId(),
				*Case.Describe(TEXT("exact override should preserve return type"))));
			ASSERT_THAT(AreEqual(
				OutBaseMethod->IsReadOnly(),
				OutDerivedMethod->IsReadOnly(),
				*Case.Describe(TEXT("exact override should preserve constness"))));
		}
		else
		{
			ASSERT_THAT(IsFalse(
				OutDerivedMethod->IsOverride(),
				*Case.Describe(TEXT("compatible overload should not claim an override slot"))));
			ASSERT_THAT(IsNotNull(
				FindMethod(Primary, BaseDeclaration),
				*Case.Describe(TEXT("compatible overload should retain the inherited base declaration"))));
		}

		if (Dimension.Dimension == ESignatureDimension::Visibility)
		{
			ASSERT_THAT(IsTrue(
				OutBaseMethod->IsProtected(),
				*Case.Describe(TEXT("legal visibility source should retain protected base access"))));
			ASSERT_THAT(AreEqual(
				Variant.Variant == ESignatureVariant::Exact,
				OutDerivedMethod->IsProtected(),
				*Case.Describe(TEXT("derived visibility should match the exact or widened source"))));
		}
		if (Dimension.Dimension == ESignatureDimension::Constness)
		{
			ASSERT_THAT(IsTrue(
				OutBaseMethod->IsReadOnly(),
				*Case.Describe(TEXT("constness source should publish a const base method"))));
			ASSERT_THAT(AreEqual(
				Variant.Variant == ESignatureVariant::Exact,
				OutDerivedMethod->IsReadOnly(),
				*Case.Describe(TEXT("derived constness should distinguish override from overload"))));
		}
	}

	static asIScriptFunction* ProbeForView(
		const FViewCase& View,
		asITypeInfo& Base,
		asITypeInfo& Primary)
	{
		switch (View.View)
		{
		case ESignatureView::Base:
			return Base.GetMethodByDecl("int ProbeBase()");
		case ESignatureView::Derived:
			return Primary.GetMethodByDecl("int ProbeDerived()");
		case ESignatureView::ExplicitBase:
			return Primary.GetMethodByDecl(
				"int ProbeExplicitBase()");
		default:
			return nullptr;
		}
	}

	void VerifyViewSelection(
		const FNativeCaseContext& Case,
		const FViewCase& View,
		asITypeInfo& Base,
		asITypeInfo& Primary,
		asIScriptFunction& BaseMethod,
		asIScriptFunction& DerivedMethod)
	{
		asIScriptFunction* const Probe =
			ProbeForView(View, Base, Primary);
		ASSERT_THAT(IsNotNull(Probe,
			*Case.Describe(TEXT("signature view should publish its exact probe"))));
		if (Probe == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FString(ANSI_TO_TCHAR(View.ProbeDeclaration)),
			FString(UTF8_TO_TCHAR(
				Probe->GetDeclaration())),
			*Case.Describe(TEXT("signature view probe should retain its exact declaration"))));

		asIScriptFunction* const ExpectedTarget =
			View.View == ESignatureView::Derived
				? &DerivedMethod
				: &BaseMethod;
		ASSERT_THAT(IsTrue(
			BytecodeCallsFunction(
				*Probe,
				ExpectedTarget->GetId(),
				View.View == ESignatureView::ExplicitBase),
			*Case.Describe(TEXT("signature view bytecode should name the exact selected method"))));
	}

	void VerifyRuntimeBoundary(
		const FNativeCaseContext& Case,
		const FViewCase& View,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Runtime =
			FindModuleFunction(
				Module,
				UTF8_TO_TCHAR(View.RuntimeDeclaration));
		ASSERT_THAT(IsNotNull(Runtime,
			*Case.Describe(TEXT("signature view should publish its runtime wrapper"))));
		if (Runtime == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			1u,
			Runtime->GetParamCount(),
			*Case.Describe(TEXT("signature runtime wrapper should take one object view"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("signature runtime view should create a context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(Context->Prepare(Runtime) >= 0,
			*Case.Describe(TEXT("signature runtime wrapper should prepare"))));
		ASSERT_THAT(IsTrue(Context->SetArgObject(0, nullptr) >= 0,
			*Case.Describe(TEXT("signature runtime wrapper should accept an explicit null view"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			Context->Execute(),
			*Case.Describe(TEXT("signature null view should reach the raw null-access boundary"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Null pointer access")),
			FString(UTF8_TO_TCHAR(
				Context->GetExceptionString())),
			*Case.Describe(TEXT("signature null view should own the exact exception"))));
		ASSERT_THAT(IsTrue(
			Context->GetExceptionLineNumber() > 0,
			*Case.Describe(TEXT("signature null view should retain a source line"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("signature null view should unprepare cleanly"))));
		Context->Release();
	}

	void CompileAndExecuteRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource =
			BuildOverrideSignatureRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("override signature should allow same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("override signature recovery should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Case.Describe(TEXT("override signature recovery should emit no errors"))));
		if (RecoveryModule == nullptr)
		{
			return;
		}

		asITypeInfo* const Base =
			RecoveryModule->GetTypeInfoByName("FSignatureBase");
		asITypeInfo* const Primary =
			RecoveryModule->GetTypeInfoByName(
				"FSignaturePrimary");
		ASSERT_THAT(IsNotNull(Base,
			*Case.Describe(TEXT("override recovery should publish its base type"))));
		ASSERT_THAT(IsNotNull(Primary,
			*Case.Describe(TEXT("override recovery should publish its derived type"))));
		if (Base != nullptr && Primary != nullptr)
		{
			ASSERT_THAT(AreEqual(
				Base,
				Primary->GetBaseType(),
				*Case.Describe(TEXT("override recovery should restore its base relation"))));
			asIScriptFunction* const RecoveryOverride =
				FindMethod(*Primary, TEXT("int Resolve(int)"));
			ASSERT_THAT(IsNotNull(RecoveryOverride,
				*Case.Describe(TEXT("override recovery should publish its exact method"))));
			if (RecoveryOverride != nullptr)
			{
				ASSERT_THAT(IsTrue(
					RecoveryOverride->IsOverride(),
					*Case.Describe(TEXT("override recovery should retain override metadata"))));
			}
		}

		asIScriptFunction* const Recovery =
			RecoveryModule->GetFunctionByDecl(
				"int RunOverrideSignatureRecovery()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("override recovery should publish its exact entry"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("override recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				PrepareAndExecute(Context, Recovery),
				*Case.Describe(TEXT("override recovery should finish"))));
			ASSERT_THAT(AreEqual(
				211,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("override recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("override recovery should unprepare cleanly"))));
			Context->Release();
		}
		else if (Context != nullptr)
		{
			Context->Release();
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("override recovery module should discard cleanly"))));
	}

	void RunAcceptedCombination(
		const TStaticArray<FNativeCaseContext, 3>& Cases,
		const FDimensionCase& Dimension,
		const FVariantCase& Variant,
		const FSignatureShape& Shape,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FString& Source)
	{
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module) >= 0,
			*Cases[0].Describe(TEXT("legal override signature source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Cases[0].Describe(TEXT("legal override signature should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Cases[0].Describe(TEXT("legal override signature should emit no errors"))));
		if (Module == nullptr)
		{
			return;
		}

		asITypeInfo* const Base =
			Module->GetTypeInfoByName("FSignatureBase");
		asITypeInfo* const Primary =
			Module->GetTypeInfoByName("FSignaturePrimary");
		ASSERT_THAT(IsNotNull(Base,
			*Cases[0].Describe(TEXT("override signature should publish its base type"))));
		ASSERT_THAT(IsNotNull(Primary,
			*Cases[1].Describe(TEXT("override signature should publish its derived type"))));
		if (Base != nullptr && Primary != nullptr)
		{
			ASSERT_THAT(AreEqual(
				Base,
				Primary->GetBaseType(),
				*Cases[1].Describe(TEXT("override signature should retain its exact base relation"))));
			ASSERT_THAT(IsTrue(
				Primary->DerivesFrom(Base),
				*Cases[1].Describe(TEXT("override signature should publish derived ancestry"))));

			asIScriptFunction* BaseMethod = nullptr;
			asIScriptFunction* DerivedMethod = nullptr;
			VerifyMethodMetadata(
				Cases[1],
				Dimension,
				Variant,
				Shape,
				*Base,
				*Primary,
				BaseMethod,
				DerivedMethod);
			if (BaseMethod != nullptr && DerivedMethod != nullptr)
			{
				for (int32 ViewIndex = 0;
					ViewIndex < UE_ARRAY_COUNT(ViewCases);
					++ViewIndex)
				{
					VerifyViewSelection(
						Cases[ViewIndex],
						ViewCases[ViewIndex],
						*Base,
						*Primary,
						*BaseMethod,
						*DerivedMethod);
					VerifyRuntimeBoundary(
						Cases[ViewIndex],
						ViewCases[ViewIndex],
						ScriptEngine,
						*Module);
				}
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Cases[2].Describe(TEXT("legal override signature module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Cases[2],
			Engine,
			ScriptEngine,
			ModuleName);
	}

	void RunRejectedCombination(
		const TStaticArray<FNativeCaseContext, 3>& Cases,
		const FSignatureShape& Shape,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		const FString& Source)
	{
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Cases[0].GetId(),
			ModuleName,
			Source,
			Module) < 0,
			*Cases[0].Describe(TEXT("incompatible override signature should fail compilation"))));
		ASSERT_THAT(IsTrue(
			HasLocatedErrorContaining(
				Engine,
				Shape.ExpectedDiagnostic),
			*Cases[1].Describe(TEXT("incompatible signature should own its exact located diagnostic"))));
		if (Module != nullptr)
		{
			for (const FViewCase& View : ViewCases)
			{
				ASSERT_THAT(IsNull(
					Module->GetFunctionByDecl(
						View.RuntimeDeclaration),
					*Cases[2].Describe(TEXT("incompatible signature should publish no callable runtime view"))));
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Cases[2].Describe(TEXT("incompatible signature module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Cases[2],
			Engine,
			ScriptEngine,
			ModuleName);
	}

	void RunCombination(
		const FDimensionCase& Dimension,
		const FVariantCase& Variant)
	{
		using namespace AngelscriptNativeTestSupport;

		const TStaticArray<FNativeCaseContext, 3> Cases =
		{
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-OVERRIDE-SIGNATURE",
				{
					ANSI_TO_TCHAR(Dimension.CatalogName),
					ANSI_TO_TCHAR(Variant.CatalogName),
					ANSI_TO_TCHAR(
						ViewCases[0].CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-OVERRIDE-SIGNATURE",
				{
					ANSI_TO_TCHAR(Dimension.CatalogName),
					ANSI_TO_TCHAR(Variant.CatalogName),
					ANSI_TO_TCHAR(
						ViewCases[1].CatalogName),
				})),
			FNativeCaseContext(MakeNativeCaseId(
				"LANG-INH-OVERRIDE-SIGNATURE",
				{
					ANSI_TO_TCHAR(Dimension.CatalogName),
					ANSI_TO_TCHAR(Variant.CatalogName),
					ANSI_TO_TCHAR(
						ViewCases[2].CatalogName),
				})),
		};

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Cases[0].Describe(TEXT("override signature should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FSignatureShape Shape =
			MakeSignatureShape(Dimension, Variant);
		const FString ModuleName = FString::Printf(
			TEXT("OverrideSignature_%hs_%hs"),
			Dimension.CatalogName,
			Variant.CatalogName);
		const FString Source =
			BuildOverrideSignatureSource(Shape);
		Engine.ResetMessages();
		if (Shape.bBuildAccepted)
		{
			RunAcceptedCombination(
				Cases,
				Dimension,
				Variant,
				Shape,
				Engine,
				*ScriptEngine,
				ModuleName,
				Source);
		}
		else
		{
			RunRejectedCombination(
				Cases,
				Shape,
				Engine,
				*ScriptEngine,
				ModuleName,
				Source);
		}
	}

public:
	TEST_METHOD(DimensionsByVariantAndView)
	{
		AS_NATIVE_PRODUCT("LANG-INH-OVERRIDE-SIGNATURE",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Bytecode
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FDimensionCase& Dimension : DimensionCases)
		{
			for (const FVariantCase& Variant : VariantCases)
			{
				RunCombination(Dimension, Variant);
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
