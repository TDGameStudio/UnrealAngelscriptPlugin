#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FFunctionArgumentSourceTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Functions.ArgumentSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{

private:
	using FNativeDirectionCase = AngelscriptNativeTestSupport::FNativeDirectionCase;
	using FNativeMessageCollector = AngelscriptNativeTestSupport::FNativeMessageCollector;
	using FNativeMessageEntry = AngelscriptNativeTestSupport::FNativeMessageEntry;

	struct FArgumentSourceCase
	{
		const ANSICHAR* CatalogName;
		bool bObjectSource;
		bool bMutableLValue;
		bool bConstSource;
		bool bNullSource;
		bool bCurrentRuntimeClassException;
	};

	inline static constexpr FArgumentSourceCase SourceCases[] =
	{
		{ "literal", false, false, false, false, false },
		{ "local_lvalue", false, true, false, false, false },
		{ "const_local", false, false, true, false, false },
		{ "global_const", false, false, true, false, false },
		{ "field", false, true, false, false, false },
		{ "function_return", false, false, false, false, false },
		{ "arithmetic_expression", false, false, false, false, false },
		{ "conditional_expression", false, false, false, false, false },
		{ "null", true, false, false, true, false },
		{ "base_view", true, true, false, false, true },
		{ "derived_view", true, true, false, false, true },
	};

	static bool IsDirection(const FNativeDirectionCase& DirectionCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(DirectionCase.CatalogName, Name) == 0;
	}

	static bool ShouldCompile(
		const FArgumentSourceCase& SourceCase,
		const FNativeDirectionCase& DirectionCase)
	{
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "derived_view") == 0)
		{
			return IsDirection(DirectionCase, "value");
		}
		if (IsDirection(DirectionCase, "value") || IsDirection(DirectionCase, "in"))
		{
			return true;
		}
		return SourceCase.bMutableLValue && !SourceCase.bConstSource && !SourceCase.bNullSource;
	}

	static FString MakeSuffix(
		const FArgumentSourceCase& SourceCase,
		const FNativeDirectionCase& DirectionCase)
	{
		return FString::Printf(TEXT("%hs_%hs"), DirectionCase.CatalogName, SourceCase.CatalogName);
	}

	static void AppendCommonDeclarations(FString& Source)
	{
		AppendGeneratedAsLine(Source, TEXT("const int GlobalConstValue = 7;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ProvideValue()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FHolder"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FDerived : FBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendProbe(
		FString& Source,
		const FString& ProbeName,
		const FArgumentSourceCase& SourceCase,
		const FNativeDirectionCase& DirectionCase)
	{
		const ANSICHAR* const TypeName = SourceCase.bObjectSource
			? (SourceCase.bNullSource ? "FNativeCaseReference" : "FBase")
			: "int";
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int %s(%hs%hs Value)"),
			*ProbeName,
			TypeName,
			DirectionCase.DeclarationSuffix));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (DirectionCase.bWritesValue)
		{
			if (SourceCase.bObjectSource)
			{
				AppendGeneratedAsLine(Source, TEXT("\tValue = FDerived();"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tValue = 9;"));
			}
		}
		if (SourceCase.bObjectSource)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Value == nullptr ? 1 : Value.Value;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString MakeArgumentExpression(const FArgumentSourceCase& SourceCase)
	{
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "literal") == 0)
		{
			return TEXT("7");
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "local_lvalue") == 0
			|| FCStringAnsi::Strcmp(SourceCase.CatalogName, "const_local") == 0)
		{
			return TEXT("Value");
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "global_const") == 0)
		{
			return TEXT("GlobalConstValue");
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "field") == 0)
		{
			return TEXT("Holder.Value");
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "function_return") == 0)
		{
			return TEXT("ProvideValue()");
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "arithmetic_expression") == 0)
		{
			return TEXT("3 + 4");
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "conditional_expression") == 0)
		{
			return TEXT("true ? 7 : 8");
		}
		if (SourceCase.bNullSource)
		{
			return TEXT("NullValue");
		}
		return TEXT("Value");
	}

	static void AppendEntryLocals(FString& Source, const FArgumentSourceCase& SourceCase)
	{
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "local_lvalue") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 7;"));
		}
		else if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "const_local") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tconst int Value = 7;"));
		}
		else if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "field") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFHolder Holder;"));
		}
		else if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "base_view") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFBase Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value = 7;"));
		}
		else if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "derived_view") == 0)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFDerived Value;"));
			AppendGeneratedAsLine(Source, TEXT("\tValue.Value = 7;"));
		}
		else if (SourceCase.bNullSource)
		{
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseReference NullValue = nullptr;"));
		}
	}

	static FString MakePostCallExpression(
		const FArgumentSourceCase& SourceCase,
		const FNativeDirectionCase& DirectionCase,
		const FString& CallExpression)
	{
		if (!DirectionCase.bWritesValue)
		{
			return CallExpression;
		}
		if (SourceCase.bObjectSource)
		{
			return FString::Printf(TEXT("%s + (Value == nullptr ? 100 : Value.Value)"), *CallExpression);
		}
		if (FCStringAnsi::Strcmp(SourceCase.CatalogName, "field") == 0)
		{
			return FString::Printf(TEXT("%s + Holder.Value"), *CallExpression);
		}
		return FString::Printf(TEXT("%s + Value"), *CallExpression);
	}

	static FString BuildArgumentSource(
		const FArgumentSourceCase& SourceCase,
		const FNativeDirectionCase& DirectionCase)
	{
		const FString Suffix = MakeSuffix(SourceCase, DirectionCase);
		const FString ProbeName = TEXT("Probe_") + Suffix;
		const FString EntryName = TEXT("Run_") + Suffix;
		FString Source;
		AppendCommonDeclarations(Source);
		AppendProbe(Source, ProbeName, SourceCase, DirectionCase);
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("int %s()"), *EntryName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendEntryLocals(Source, SourceCase);
		const FString CallExpression = FString::Printf(
			TEXT("%s(%s)"),
			*ProbeName,
			*MakeArgumentExpression(SourceCase));
		AppendGeneratedAsLine(Source, TEXT("\treturn ") + MakePostCallExpression(SourceCase, DirectionCase, CallExpression) + TEXT(";"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages, const FString& ModuleName)
	{
		return Messages.Entries.ContainsByPredicate([&ModuleName](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == ModuleName
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

public:
	TEST_METHOD(ArgumentSourcesByDirection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-FN-ARG-SOURCE",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Lifecycle);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Function argument-source product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine),
			TEXT("Function argument-source product should register its native reference fixture")));

		for (const FNativeDirectionCase& DirectionCase : NativeDirectionCases)
		{
			for (const FArgumentSourceCase& SourceCase : SourceCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-FN-ARG-SOURCE",
					{ ANSI_TO_TCHAR(DirectionCase.CatalogName), ANSI_TO_TCHAR(SourceCase.CatalogName) }));
				const FString Suffix = MakeSuffix(SourceCase, DirectionCase);
				const FString ModuleName = TEXT("FunctionArgumentSource_") + Suffix;
				const FString Source = BuildArgumentSource(SourceCase, DirectionCase);
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
				if (BuildResult < 0)
				{
					TestRunner->AddInfo(Engine.GetMessagesText());
				}

				if (!ShouldCompile(SourceCase, DirectionCase))
				{
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("illegal argument source and direction should fail compilation"))));
					ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
						*Case.Describe(TEXT("illegal argument source should report a located call-site diagnostic"))));
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("illegal argument-source cell should leave no retained module"))));
					continue;
				}

				ASSERT_THAT(IsTrue(BuildResult >= 0,
					*Case.Describe(TEXT("legal argument source and direction should compile"))));
				ASSERT_THAT(IsNotNull(Module,
					*Case.Describe(TEXT("legal argument-source cell should publish a module"))));
				if (BuildResult >= 0 && Module != nullptr)
				{
					const FString ProbeName = TEXT("Probe_") + Suffix;
					const TArray<asIScriptFunction*> Probes = FindNativeFunctionsByName(Module, TCHAR_TO_ANSI(*ProbeName));
					ASSERT_THAT(AreEqual(1, Probes.Num(),
						*Case.Describe(TEXT("argument-source cell should publish exactly one probe with its stable name"))));
					asIScriptFunction* const Probe = Probes.Num() == 1 ? Probes[0] : nullptr;
					ASSERT_THAT(IsNotNull(Probe,
						*Case.Describe(TEXT("argument-source probe should be published"))));
					if (Probe != nullptr)
					{
						int TypeId = 0;
						asDWORD Flags = 0;
						ASSERT_THAT(AreEqual(asSUCCESS, Probe->GetParam(0, &TypeId, &Flags),
							*Case.Describe(TEXT("argument-source parameter metadata query should succeed"))));
						ASSERT_THAT(AreEqual(static_cast<uint32>(DirectionCase.TypeModifier), static_cast<uint32>(Flags & 3u),
							*Case.Describe(TEXT("argument-source parameter direction metadata should be exact"))));
					}

					const FString EntryDeclaration = FString::Printf(TEXT("int Run_%s()"), *Suffix);
					asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, TCHAR_TO_ANSI(*EntryDeclaration));
					ASSERT_THAT(IsNotNull(Entry,
						*Case.Describe(TEXT("argument-source entry should resolve by exact declaration"))));
					if (Entry != nullptr)
					{
						asIScriptContext* const Context = ScriptEngine->CreateContext();
						ASSERT_THAT(IsNotNull(Context,
							*Case.Describe(TEXT("argument-source cell should create an execution context"))));
						if (Context != nullptr)
						{
							const int ExecuteResult = PrepareAndExecute(Context, Entry);
							if (SourceCase.bCurrentRuntimeClassException)
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult,
									*Case.Describe(TEXT("base or derived view should preserve the current isolated-engine class exception"))));
								ASSERT_THAT(AreEqual(FString(TEXT("Null pointer access")), FString(UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "")),
									*Case.Describe(TEXT("base or derived view should preserve current exception text"))));
							}
							else
							{
								ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
									*Case.Describe(TEXT("legal argument-source call should finish"))));
								if (ExecuteResult == asEXECUTION_FINISHED)
								{
									const int32 Result = static_cast<int32>(Context->GetReturnDWord());
									if (DirectionCase.bWritesValue)
									{
										ASSERT_THAT(AreEqual(18, Result,
											*Case.Describe(TEXT("writable source should observe both the callee result and writeback value"))));
									}
									else
									{
										const int32 Expected = SourceCase.bNullSource ? 1 : 7;
										ASSERT_THAT(AreEqual(Expected, Result,
											*Case.Describe(TEXT("readable source should transfer the selected value exactly"))));
									}
								}
							}
							Context->Release();
						}
					}
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("argument-source cell should discard module, objects, and context state"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
