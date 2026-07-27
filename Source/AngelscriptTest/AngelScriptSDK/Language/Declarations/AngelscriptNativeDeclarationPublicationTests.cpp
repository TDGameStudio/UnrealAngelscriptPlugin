#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDeclarationPublicationTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Declarations.Publication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNamedCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FGeneratedSection
	{
		FString Name;
		FString Source;
	};

	struct FDeclarationSource
	{
		FString ProviderModuleName;
		FString ProviderSource;
		TArray<FGeneratedSection> Sections;
	};

	struct FExecutionObservation
	{
		int Result = asERROR;
		int32 Value = INDEX_NONE;
		FString Exception;
	};

	inline static constexpr FNamedCase FamilyCases[] =
	{
		{ "function" }, { "method" }, { "class" }, { "struct" }, { "field" },
		{ "constructor" }, { "destructor" }, { "namespace" }, { "enum" }, { "typedef" },
		{ "funcdef" }, { "import" }, { "virtual_property" }, { "indexed_property" }, { "mixin_global" },
	};
	inline static constexpr FNamedCase ScopeCases[] =
	{
		{ "global" }, { "namespace" }, { "nested_namespace" }, { "member" }, { "multiple_sections" }, { "imported_module" },
	};
	inline static constexpr FNamedCase OrderingCases[] =
	{
		{ "before_use" }, { "forward_use" }, { "same_section" }, { "later_section" }, { "reversed_sections" }, { "rebuild" },
	};


	static bool IsNamedCase(const FNamedCase& Case, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(Case.CatalogName, Name) == 0;
	}

	static int32 GetCaseIndex(const FNamedCase& Case, const FNamedCase* Cases, const int32 Count)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (IsNamedCase(Case, Cases[Index].CatalogName))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	static int32 GetFamilyValue(const FNamedCase& FamilyCase)
	{
		return 100 + GetCaseIndex(FamilyCase, FamilyCases, UE_ARRAY_COUNT(FamilyCases));
	}

	static int32 GetScopeValue(const FNamedCase& ScopeCase)
	{
		return 200 + GetCaseIndex(ScopeCase, ScopeCases, UE_ARRAY_COUNT(ScopeCases));
	}

	static int32 GetOrderingValue(const FNamedCase& OrderingCase)
	{
		return 300 + GetCaseIndex(OrderingCase, OrderingCases, UE_ARRAY_COUNT(OrderingCases));
	}

	static void AddSection(FDeclarationSource& Result, FString Name, FString Source)
	{
		Result.Sections.Add({ MoveTemp(Name), MoveTemp(Source) });
	}

	static void AppendFunction(FString& Source, const TCHAR* Declaration, const FString& ReturnExpression)
	{
		const FString DeclarationText(Declaration);
		int32 IndentationLength = 0;
		while (IndentationLength < DeclarationText.Len() && DeclarationText[IndentationLength] == TCHAR('\t'))
		{
			++IndentationLength;
		}
		const FString Indentation = DeclarationText.Left(IndentationLength);
		AppendGeneratedAsLine(Source, DeclarationText);
		AppendGeneratedAsLine(Source, Indentation + TEXT("{"));
		AppendGeneratedAsLine(Source, Indentation + TEXT("\treturn ") + ReturnExpression + TEXT(";"));
		AppendGeneratedAsLine(Source, Indentation + TEXT("}"));
	}

	static void AppendFamilySource(
		FString& Source,
		const FNamedCase& FamilyCase,
		const int32 FamilyValue,
		const FString& ProviderModuleName)
	{
		const FString Value = FString::FromInt(FamilyValue);
		if (IsNamedCase(FamilyCase, "function"))
		{
			AppendFunction(Source, TEXT("int FamilyPublishedFunction()"), Value);
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int FamilyWitness()"), TEXT("FamilyPublishedFunction()"));
		}
		else if (IsNamedCase(FamilyCase, "method"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyMethod"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Read()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyMethod Value = FFamilyMethod();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Read();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "class"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyClass"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Read()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyClass Value = FFamilyClass();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Read();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "struct"))
		{
			AppendGeneratedAsLine(Source, TEXT("struct FFamilyStruct"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Read()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyStruct Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Read();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "field"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyField"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Published = ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyField Value = FFamilyField();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Published;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "constructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyConstructor"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Published;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyConstructor(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tPublished = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyConstructor Value = FFamilyConstructor(") + Value + TEXT(");"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Published;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "destructor"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyDestructor"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint ObjectId;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyDestructor()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(") + Value + TEXT(");"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FFamilyDestructor()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, ") + Value + TEXT(");"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyDestructor Value = FFamilyDestructor();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "namespace"))
		{
			AppendGeneratedAsLine(Source, TEXT("namespace FamilyNamespace"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendFunction(Source, TEXT("\tint Read()"), Value);
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int FamilyWitness()"), TEXT("FamilyNamespace::Read()"));
		}
		else if (IsNamedCase(FamilyCase, "enum"))
		{
			AppendGeneratedAsLine(Source, TEXT("enum EFamilyPublished"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tPublished = ") + Value);
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int FamilyWitness()"), TEXT("int(EFamilyPublished::Published)"));
		}
		else if (IsNamedCase(FamilyCase, "typedef"))
		{
			AppendGeneratedAsLine(Source, TEXT("// FamilyPublishedAlias is registered through the raw SDK before the module build."));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int FamilyWitness()"), TEXT("FamilyPublishedAlias(") + Value + TEXT(")"));
		}
		else if (IsNamedCase(FamilyCase, "funcdef"))
		{
			AppendGeneratedAsLine(Source, TEXT("funcdef int FFamilyPublishedCallback(int Value);"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int InvokeFamilyPublished(int Value)"), TEXT("Value + ") + Value);
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyPublishedCallback Callback = InvokeFamilyPublished;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Callback(0);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "import"))
		{
			AppendGeneratedAsLine(Source, TEXT("import int FamilyImportedValue() from \"") + ProviderModuleName + TEXT("\";"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int FamilyWitness()"), TEXT("FamilyImportedValue()"));
		}
		else if (IsNamedCase(FamilyCase, "virtual_property"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyVirtualProperty"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Stored = ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint Published"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tget"));
			AppendGeneratedAsLine(Source, TEXT("\t\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\t\treturn Stored;"));
			AppendGeneratedAsLine(Source, TEXT("\t\t}"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyVirtualProperty Value = FFamilyVirtualProperty();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Published;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else if (IsNamedCase(FamilyCase, "indexed_property"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FFamilyIndexedProperty"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint opIndex(int Index) const"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Index + ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyIndexedProperty Value = FFamilyIndexedProperty();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value[0];"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("struct FFamilyMixinTarget"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = ") + Value + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("mixin int ReadFamilyMixin(FFamilyMixinTarget& Self)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Self.Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int FamilyWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFFamilyMixinTarget Value;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.ReadFamilyMixin();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
	}

	static void AppendScopeSource(
		FDeclarationSource& Result,
		const FNamedCase& ScopeCase,
		const int32 ScopeValue)
	{
		FString Source;
		const FString Value = FString::FromInt(ScopeValue);
		if (IsNamedCase(ScopeCase, "global"))
		{
			AppendFunction(Source, TEXT("int ScopeWitness()"), Value);
			AddSection(Result, TEXT("ScopeGlobal"), MoveTemp(Source));
		}
		else if (IsNamedCase(ScopeCase, "namespace"))
		{
			AppendGeneratedAsLine(Source, TEXT("namespace ScopeNamespace"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendFunction(Source, TEXT("\tint Read()"), Value);
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int ScopeWitness()"), TEXT("ScopeNamespace::Read()"));
			AddSection(Result, TEXT("ScopeNamespace"), MoveTemp(Source));
		}
		else if (IsNamedCase(ScopeCase, "nested_namespace"))
		{
			AppendGeneratedAsLine(Source, TEXT("namespace ScopeOuter"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tnamespace ScopeInner"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendFunction(Source, TEXT("\t\tint Read()"), Value);
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int ScopeWitness()"), TEXT("ScopeOuter::ScopeInner::Read()"));
			AddSection(Result, TEXT("ScopeNestedNamespace"), MoveTemp(Source));
		}
		else if (IsNamedCase(ScopeCase, "member"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FScopeMember"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendFunction(Source, TEXT("\tint Read()"), Value);
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ScopeWitness()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tFScopeMember Value = FScopeMember();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Read();"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AddSection(Result, TEXT("ScopeMember"), MoveTemp(Source));
		}
		else if (IsNamedCase(ScopeCase, "multiple_sections"))
		{
			AppendGeneratedAsLine(Source, TEXT("namespace ScopeSectionProvider"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendFunction(Source, TEXT("\tint Read()"), Value);
			AppendGeneratedAsLine(Source, TEXT("}"));
			AddSection(Result, TEXT("ScopeProviderSection"), MoveTemp(Source));

			FString ConsumerSource;
			AppendFunction(ConsumerSource, TEXT("int ScopeWitness()"), TEXT("ScopeSectionProvider::Read()"));
			AddSection(Result, TEXT("ScopeConsumerSection"), MoveTemp(ConsumerSource));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("import int ScopeImportedValue() from \"") + Result.ProviderModuleName + TEXT("\";"));
			AppendGeneratedAsLine(Source);
			AppendFunction(Source, TEXT("int ScopeWitness()"), TEXT("ScopeImportedValue()"));
			AddSection(Result, TEXT("ScopeImportedModule"), MoveTemp(Source));
		}
	}

	static void AppendOrderingSource(
		FDeclarationSource& Result,
		const FNamedCase& OrderingCase,
		const int32 OrderingValue)
	{
		FString Target;
		AppendFunction(Target, TEXT("int OrderingTarget()"), FString::FromInt(OrderingValue));
		FString Consumer;
		AppendFunction(Consumer, TEXT("int OrderingWitness()"), TEXT("OrderingTarget()"));

		if (IsNamedCase(OrderingCase, "same_section"))
		{
			AppendGeneratedAsLine(Target);
			Target += Consumer;
			AddSection(Result, TEXT("OrderingSameSection"), MoveTemp(Target));
		}
		else if (IsNamedCase(OrderingCase, "before_use") || IsNamedCase(OrderingCase, "rebuild"))
		{
			AddSection(Result, TEXT("OrderingTargetBeforeConsumer"), MoveTemp(Target));
			AddSection(Result, TEXT("OrderingConsumerAfterTarget"), MoveTemp(Consumer));
		}
		else if (IsNamedCase(OrderingCase, "reversed_sections"))
		{
			AddSection(Result, TEXT("ZZOrderingTarget"), MoveTemp(Target));
			AddSection(Result, TEXT("AAOrderingConsumer"), MoveTemp(Consumer));
		}
		else
		{
			AddSection(Result, TEXT("OrderingConsumerBeforeTarget"), MoveTemp(Consumer));
			AddSection(Result, TEXT("OrderingTargetAfterConsumer"), MoveTemp(Target));
		}
	}

	static FDeclarationSource BuildDeclarationSource(
		const FNamedCase& FamilyCase,
		const FNamedCase& ScopeCase,
		const FNamedCase& OrderingCase,
		const FString& ProviderModuleName)
	{
		FDeclarationSource Result;
		Result.ProviderModuleName = ProviderModuleName;
		const int32 FamilyValue = GetFamilyValue(FamilyCase);
		const int32 ScopeValue = GetScopeValue(ScopeCase);
		if (IsNamedCase(FamilyCase, "import"))
		{
			AppendFunction(Result.ProviderSource, TEXT("int FamilyImportedValue()"), FString::FromInt(FamilyValue));
		}
		if (IsNamedCase(ScopeCase, "imported_module"))
		{
			if (!Result.ProviderSource.IsEmpty())
			{
				AppendGeneratedAsLine(Result.ProviderSource);
			}
			AppendFunction(Result.ProviderSource, TEXT("int ScopeImportedValue()"), FString::FromInt(ScopeValue));
		}

		FString FamilySource;
		AppendFamilySource(FamilySource, FamilyCase, FamilyValue, ProviderModuleName);
		AddSection(Result, TEXT("FamilyDeclaration"), MoveTemp(FamilySource));
		AppendScopeSource(Result, ScopeCase, ScopeValue);
		AppendOrderingSource(Result, OrderingCase, GetOrderingValue(OrderingCase));

		FString EntrySource;
		AppendGeneratedAsLine(EntrySource, TEXT("int Entry()"));
		AppendGeneratedAsLine(EntrySource, TEXT("{"));
		AppendGeneratedAsLine(EntrySource, TEXT("\treturn FamilyWitness() + ScopeWitness() + OrderingWitness();"));
		AppendGeneratedAsLine(EntrySource, TEXT("}"));
		AddSection(Result, TEXT("Entry"), MoveTemp(EntrySource));
		return Result;
	}

	static FString MakePrintableSource(const FDeclarationSource& Source)
	{
		FString Result;
		if (!Source.ProviderSource.IsEmpty())
		{
			AppendGeneratedAsLine(Result, TEXT("// Provider module: ") + Source.ProviderModuleName);
			Result += Source.ProviderSource;
			AppendGeneratedAsLine(Result);
		}
		for (const FGeneratedSection& Section : Source.Sections)
		{
			AppendGeneratedAsLine(Result, TEXT("// Section: ") + Section.Name);
			Result += Section.Source;
			AppendGeneratedAsLine(Result);
		}
		return Result;
	}

	static int BuildDeclarationModule(
		asIScriptEngine& ScriptEngine,
		const char* ModuleName,
		const FDeclarationSource& Source,
		asIScriptModule*& OutModule)
	{
		OutModule = ScriptEngine.GetModule(ModuleName, asGM_ALWAYS_CREATE);
		if (OutModule == nullptr)
		{
			return asNO_MODULE;
		}
		for (const FGeneratedSection& Section : Source.Sections)
		{
			const FTCHARToUTF8 SectionNameUtf8(*Section.Name);
			const FTCHARToUTF8 SectionSourceUtf8(*Section.Source);
			const int AddResult = OutModule->AddScriptSection(
				SectionNameUtf8.Get(),
				SectionSourceUtf8.Get(),
				static_cast<unsigned int>(SectionSourceUtf8.Length()));
			if (AddResult < 0)
			{
				return AddResult;
			}
		}
		return OutModule->Build();
	}

	bool BindDeclarationImports(
		FAutomationTestBase& Test,
		asIScriptModule& Consumer,
		asIScriptModule* Provider,
		const FString& ProviderModuleName,
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bAssertionsPassed = true;
		const int32 ExpectedImports =
			(Consumer.GetImportedFunctionIndexByDecl("int FamilyImportedValue()") >= 0 ? 1 : 0)
			+ (Consumer.GetImportedFunctionIndexByDecl("int ScopeImportedValue()") >= 0 ? 1 : 0);
		bAssertionsPassed &= LocalAssert.AreEqual(
			ExpectedImports,
			static_cast<int32>(Consumer.GetImportedFunctionCount()),
			*Case.Describe(TEXT("declaration source should expose exactly its selected import declarations")));
		if (ExpectedImports == 0)
		{
			return bAssertionsPassed;
		}
		bAssertionsPassed &= LocalAssert.IsNotNull(
			Provider,
			*Case.Describe(TEXT("importing declaration source should build its provider module")));
		if (Provider == nullptr)
		{
			return false;
		}

		bool bBoundAllImports = true;
		for (asUINT ImportIndex = 0; ImportIndex < Consumer.GetImportedFunctionCount(); ++ImportIndex)
		{
			const FString SourceModule = UTF8_TO_TCHAR(Consumer.GetImportedFunctionSourceModule(ImportIndex));
			bAssertionsPassed &= LocalAssert.AreEqual(
				ProviderModuleName,
				SourceModule,
				*Case.Describe(TEXT("import declaration should retain its selected provider module name")));
			asIScriptFunction* const ProviderFunction = Provider->GetFunctionByDecl(Consumer.GetImportedFunctionDeclaration(ImportIndex));
			bAssertionsPassed &= LocalAssert.IsNotNull(
				ProviderFunction,
				*Case.Describe(TEXT("provider module should expose each imported declaration exactly")));
			if (ProviderFunction == nullptr)
			{
				bBoundAllImports = false;
				continue;
			}
			const int BindResult = Consumer.BindImportedFunction(ImportIndex, ProviderFunction);
			bAssertionsPassed &= LocalAssert.AreEqual(
				asSUCCESS,
				BindResult,
				*Case.Describe(TEXT("consumer should bind each selected declaration to its raw module provider")));
			bBoundAllImports &= BindResult == asSUCCESS;
		}
		return bAssertionsPassed && bBoundAllImports;
	}

	static FExecutionObservation ExecuteEntry(asIScriptEngine& ScriptEngine, asIScriptModule* Module)
	{
		FExecutionObservation Observation;
		asIScriptFunction* const Entry = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(Module, "int Entry()");
		if (Entry == nullptr)
		{
			return Observation;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		if (Context == nullptr)
		{
			return Observation;
		}
		Observation.Result = AngelscriptNativeTestSupport::PrepareAndExecute(Context, Entry);
		Observation.Value = static_cast<int32>(Context->GetReturnDWord());
		Observation.Exception = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		Context->Release();
		return Observation;
	}

	void VerifyFamilyMetadata(
		asIScriptModule& Module,
		const FNamedCase& FamilyCase,
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		if (IsNamedCase(FamilyCase, "function"))
		{
			ASSERT_THAT(IsNotNull(Module.GetFunctionByDecl("int FamilyPublishedFunction()"), *Case.Describe(TEXT("function family should publish its exact global declaration"))));
		}
		else if (IsNamedCase(FamilyCase, "method"))
		{
			asITypeInfo* const Type = Module.GetTypeInfoByDecl("FFamilyMethod");
			ASSERT_THAT(IsNotNull(Type, *Case.Describe(TEXT("method family should publish its declaring class"))));
			ASSERT_THAT(IsNotNull(Type != nullptr ? Type->GetMethodByDecl("int Read()") : nullptr, *Case.Describe(TEXT("method family should publish its exact member declaration"))));
		}
		else if (IsNamedCase(FamilyCase, "class"))
		{
			ASSERT_THAT(IsNotNull(Module.GetTypeInfoByDecl("FFamilyClass"), *Case.Describe(TEXT("class family should publish its class type metadata"))));
		}
		else if (IsNamedCase(FamilyCase, "struct"))
		{
			ASSERT_THAT(IsNotNull(Module.GetTypeInfoByDecl("FFamilyStruct"), *Case.Describe(TEXT("struct family should publish its struct type metadata"))));
		}
		else if (IsNamedCase(FamilyCase, "field"))
		{
			asITypeInfo* const Type = Module.GetTypeInfoByDecl("FFamilyField");
			ASSERT_THAT(IsNotNull(Type, *Case.Describe(TEXT("field family should publish its owner type"))));
			ASSERT_THAT(IsTrue(Type != nullptr && Type->GetPropertyCount() == 1, *Case.Describe(TEXT("field family should publish one actual data member"))));
		}
		else if (IsNamedCase(FamilyCase, "constructor"))
		{
			ASSERT_THAT(IsNotNull(Module.GetTypeInfoByDecl("FFamilyConstructor"), *Case.Describe(TEXT("constructor family should publish its constructible class"))));
		}
		else if (IsNamedCase(FamilyCase, "destructor"))
		{
			ASSERT_THAT(IsNotNull(Module.GetTypeInfoByDecl("FFamilyDestructor"), *Case.Describe(TEXT("destructor family should publish its destructible class"))));
		}
		else if (IsNamedCase(FamilyCase, "namespace"))
		{
			ASSERT_THAT(IsNotNull(Module.GetFunctionByDecl("int FamilyNamespace::Read()"), *Case.Describe(TEXT("namespace family should publish its qualified function declaration"))));
		}
		else if (IsNamedCase(FamilyCase, "enum"))
		{
			ASSERT_THAT(IsNotNull(Module.GetTypeInfoByDecl("EFamilyPublished"), *Case.Describe(TEXT("enum family should publish its enum type metadata"))));
		}
		else if (IsNamedCase(FamilyCase, "typedef"))
		{
			ASSERT_THAT(IsTrue(Module.GetTypeIdByDecl("FamilyPublishedAlias") >= 0, *Case.Describe(TEXT("typedef family should publish its named alias type"))));
		}
		else if (IsNamedCase(FamilyCase, "funcdef"))
		{
			asITypeInfo* const Type = Module.GetTypeInfoByDecl("FFamilyPublishedCallback");
			ASSERT_THAT(IsNotNull(Type, *Case.Describe(TEXT("funcdef family should publish its callable type"))));
			ASSERT_THAT(IsNotNull(Type != nullptr ? Type->GetFuncdefSignature() : nullptr, *Case.Describe(TEXT("funcdef family should publish its signature metadata"))));
		}
		else if (IsNamedCase(FamilyCase, "import"))
		{
			ASSERT_THAT(IsTrue(Module.GetImportedFunctionIndexByDecl("int FamilyImportedValue()") >= 0, *Case.Describe(TEXT("import family should publish its imported declaration"))));
		}
		else if (IsNamedCase(FamilyCase, "virtual_property"))
		{
			asITypeInfo* const Type = Module.GetTypeInfoByDecl("FFamilyVirtualProperty");
			ASSERT_THAT(IsNotNull(Type, *Case.Describe(TEXT("virtual-property family should publish its owner type"))));
			ASSERT_THAT(IsNotNull(Type != nullptr ? Type->GetMethodByName("get_Published") : nullptr, *Case.Describe(TEXT("virtual-property family should publish its property getter"))));
		}
		else if (IsNamedCase(FamilyCase, "indexed_property"))
		{
			asITypeInfo* const Type = Module.GetTypeInfoByDecl("FFamilyIndexedProperty");
			ASSERT_THAT(IsNotNull(Type, *Case.Describe(TEXT("indexed-property family should publish its owner type"))));
			ASSERT_THAT(IsNotNull(Type != nullptr ? Type->GetMethodByName("opIndex") : nullptr, *Case.Describe(TEXT("indexed-property family should publish its index accessor"))));
		}
		else
		{
			// Mixin functions are published as module functions. The current fork
			// exposes their canonical declaration through the function object, but
			// GetFunctionByDecl() does not resolve that declaration back to the
			// extension function; keep that lookup limitation recorded separately.
			asIScriptFunction* MixinFunction = nullptr;
			FString CandidateDeclarations;
			for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
			{
				asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
				if (Candidate != nullptr && FCStringAnsi::Strcmp(Candidate->GetName(), "ReadFamilyMixin") == 0)
				{
					MixinFunction = Candidate;
					CandidateDeclarations += UTF8_TO_TCHAR(Candidate->GetDeclaration());
					CandidateDeclarations += TEXT(" | ");
				}
			}
			ASSERT_THAT(IsNotNull(MixinFunction,
				*FString::Printf(TEXT("mixin family should publish its extension function. Candidates={%s}"), *CandidateDeclarations)));
			if (MixinFunction != nullptr)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("int ReadFamilyMixin(FFamilyMixinTarget&inout)")),
					FString(UTF8_TO_TCHAR(MixinFunction->GetDeclaration())),
					*FString::Printf(TEXT("mixin family should retain its canonical declaration. Candidates={%s}"), *CandidateDeclarations)));
			}
		}
	}

public:
	TEST_METHOD(DeclarationFamiliesByScopeAndOrder)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-DECL-FAMILY-SCOPE-ORDER",
			ENativeEvidence::Compile
			| ENativeEvidence::Metadata
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Declaration publication product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle), TEXT("Declaration publication should register the raw destructor lifecycle bridge")));
		const int AliasResult = ScriptEngine->RegisterTypedef("FamilyPublishedAlias", "int");
		ASSERT_THAT(IsTrue(AliasResult >= 0,
			*FString::Printf(TEXT("Declaration publication should register its raw typedef alias. Result=%d Messages={%s}"),
				AliasResult, *Engine.GetMessagesText())));

		for (const FNamedCase& FamilyCase : FamilyCases)
		{
			for (const FNamedCase& ScopeCase : ScopeCases)
			{
				for (const FNamedCase& OrderingCase : OrderingCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId("LANG-DECL-FAMILY-SCOPE-ORDER", { ANSI_TO_TCHAR(FamilyCase.CatalogName), ANSI_TO_TCHAR(ScopeCase.CatalogName), ANSI_TO_TCHAR(OrderingCase.CatalogName) }));
					const FString ModuleName = TEXT("DeclarationPublication_") + Case.GetId().RightChop(29).Replace(TEXT("-"), TEXT("_"));
					const FString ProviderModuleName = ModuleName + TEXT("_Provider");
					const FDeclarationSource Source = BuildDeclarationSource(FamilyCase, ScopeCase, OrderingCase, ProviderModuleName);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, MakePrintableSource(Source));
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 ProviderModuleNameUtf8(*ProviderModuleName);

					asIScriptModule* Provider = nullptr;
					if (!Source.ProviderSource.IsEmpty())
					{
						const FTCHARToUTF8 ProviderSourceUtf8(*Source.ProviderSource);
						ASSERT_THAT(AreEqual(asSUCCESS, CompileNativeModule(ScriptEngine, ProviderModuleNameUtf8.Get(), ProviderSourceUtf8.Get(), Provider),
							*Case.Describe(TEXT("selected declaration imports should compile their raw provider module"))));
					}

					asIScriptModule* Module = nullptr;
					Engine.ResetMessages();
					const int BuildResult = BuildDeclarationModule(*ScriptEngine, ModuleNameUtf8.Get(), Source, Module);
					if (IsNamedCase(FamilyCase, "virtual_property") || IsNamedCase(FamilyCase, "funcdef"))
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*FString::Printf(TEXT("%s removed/current-fork declaration syntax should be rejected. BuildResult=%d Messages={%s}"),
								*Case.GetId(), BuildResult, *Engine.GetMessagesText())));
						const TCHAR* const ExpectedDiagnostic = IsNamedCase(FamilyCase, "virtual_property")
							? TEXT("Virtual property syntax has been removed")
							: TEXT("Expected identifier");
						ASSERT_THAT(IsTrue(Engine.GetMessagesText().Contains(ExpectedDiagnostic),
							*Case.Describe(TEXT("removed declaration syntax should preserve the fork diagnostic"))));
						ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
						if (Provider != nullptr)
						{
							ScriptEngine->DiscardModule(ProviderModuleNameUtf8.Get());
						}
						continue;
					}
					ASSERT_THAT(AreEqual(asSUCCESS, BuildResult,
						*FString::Printf(TEXT("%s declaration family, scope, and source-order cell should build from real sections. BuildResult=%d Messages={%s}"),
							*Case.GetId(), BuildResult, *Engine.GetMessagesText())));
					if (BuildResult == asSUCCESS && Module != nullptr)
					{
						const bool bImportsBound = BindDeclarationImports(
							*TestRunner,
							*Module,
							Provider,
							ProviderModuleName,
							Case);
						VerifyFamilyMetadata(*Module, FamilyCase, Case);
						asIScriptFunction* const ScopeWitness = Module->GetFunctionByDecl("int ScopeWitness()");
						asIScriptFunction* const OrderingTarget = Module->GetFunctionByDecl("int OrderingTarget()");
						asIScriptFunction* const OrderingWitness = Module->GetFunctionByDecl("int OrderingWitness()");
						ASSERT_THAT(IsNotNull(ScopeWitness, *Case.Describe(TEXT("selected scope should publish its exact witness function"))));
						ASSERT_THAT(IsNotNull(OrderingTarget, *Case.Describe(TEXT("selected source order should publish its producer function"))));
						ASSERT_THAT(IsNotNull(OrderingWitness, *Case.Describe(TEXT("selected source order should publish its consumer function"))));
						if (IsNamedCase(ScopeCase, "member"))
						{
							ASSERT_THAT(IsNotNull(Module->GetTypeInfoByDecl("FScopeMember"), *Case.Describe(TEXT("member scope should publish its member-owning type"))));
						}
						if (IsNamedCase(ScopeCase, "multiple_sections"))
						{
							ASSERT_THAT(IsTrue(ScopeWitness != nullptr && FString(UTF8_TO_TCHAR(ScopeWitness->GetScriptSectionName())).Contains(TEXT("ScopeConsumerSection")), *Case.Describe(TEXT("multiple-section scope should retain its consumer section metadata"))));
						}
						if (IsNamedCase(OrderingCase, "reversed_sections"))
						{
							ASSERT_THAT(IsTrue(OrderingTarget != nullptr && FString(UTF8_TO_TCHAR(OrderingTarget->GetScriptSectionName())).Contains(TEXT("ZZOrderingTarget")), *Case.Describe(TEXT("reversed-section order should retain the producer section identity"))));
						}

						Lifecycle.Reset();
						if (bImportsBound)
						{
							const FExecutionObservation FirstExecution = ExecuteEntry(*ScriptEngine, Module);
							ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, FirstExecution.Result,
								*FString::Printf(TEXT("%s declaration entry should execute every supported selected declaration. Result=%d Value=%d Exception=%s"),
									*Case.GetId(), FirstExecution.Result, FirstExecution.Value, *FirstExecution.Exception)));
							ASSERT_THAT(AreEqual(GetFamilyValue(FamilyCase) + GetScopeValue(ScopeCase) + GetOrderingValue(OrderingCase), FirstExecution.Value,
								*FString::Printf(TEXT("%s declaration entry should return values from the selected family, scope, and source order. Expected=%d Actual=%d"),
									*Case.GetId(), GetFamilyValue(FamilyCase) + GetScopeValue(ScopeCase) + GetOrderingValue(OrderingCase), FirstExecution.Value)));
							if (IsNamedCase(FamilyCase, "destructor"))
							{
								ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct) > 0 && Lifecycle.Num(ENativeLifecycleEvent::Destruct) > 0, *Case.Describe(TEXT("destructor family should execute its native lifecycle callbacks"))));
							}
						}

						if (IsNamedCase(OrderingCase, "rebuild") && bImportsBound)
						{
							ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
							ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("rebuild order should discard the first real module instance"))));
							Module = nullptr;
							const int RebuildResult = BuildDeclarationModule(*ScriptEngine, ModuleNameUtf8.Get(), Source, Module);
							ASSERT_THAT(AreEqual(asSUCCESS, RebuildResult, *Case.Describe(TEXT("rebuild order should add its sections and build a second real module instance"))));
							if (RebuildResult == asSUCCESS
								&& Module != nullptr
								&& BindDeclarationImports(
									*TestRunner,
									*Module,
									Provider,
									ProviderModuleName,
									Case))
							{
								const FExecutionObservation RebuiltExecution = ExecuteEntry(*ScriptEngine, Module);
								const int ExpectedState = asEXECUTION_FINISHED;
								ASSERT_THAT(AreEqual(ExpectedState, RebuiltExecution.Result, *Case.Describe(TEXT("rebuild order should retain the selected declaration runtime state"))));
								if (ExpectedState == asEXECUTION_FINISHED)
								{
									ASSERT_THAT(AreEqual(GetFamilyValue(FamilyCase) + GetScopeValue(ScopeCase) + GetOrderingValue(OrderingCase), RebuiltExecution.Value, *Case.Describe(TEXT("rebuild order should execute the rebuilt declaration graph"))));
								}
							}
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("declaration cell should discard its consumer module"))));
					if (Provider != nullptr)
					{
						ScriptEngine->DiscardModule(ProviderModuleNameUtf8.Get());
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ProviderModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS), *Case.Describe(TEXT("declaration cell should discard its raw provider module"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
