#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FLanguageVariableScopeTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.Scope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FRelationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FScopeUsePathCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FRelationCase RelationCases[] =
	{
		{ "none" },
		{ "inner_outer" },
		{ "parameter_global" },
		{ "parameter_member" },
		{ "sibling" },
	};

	inline static constexpr FScopeUsePathCase ScopeUsePathCases[] =
	{
		{ "function_before_declaration" },
		{ "function_after_declaration" },
		{ "function_sibling_call" },
		{ "nested_block_before" },
		{ "nested_block_inside" },
		{ "nested_block_after" },
		{ "if_branch_before" },
		{ "if_branch_inside" },
		{ "if_branch_after" },
		{ "switch_case_before" },
		{ "switch_case_inside" },
		{ "switch_case_after" },
		{ "for_initializer" },
		{ "for_body" },
		{ "for_after" },
		{ "while_before" },
		{ "while_body" },
		{ "while_after" },
		{ "foreach_before" },
		{ "foreach_body" },
		{ "foreach_after" },
		{ "nested_call_caller_before" },
		{ "nested_call_callee" },
		{ "nested_call_caller_after" },
		{ "after_owner" },
	};

	static bool IsRelation(const FRelationCase& RelationCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(RelationCase.CatalogName, Name) == 0;
	}

	static bool IsPath(const FScopeUsePathCase& PathCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(PathCase.CatalogName, Name) == 0;
	}

	static bool IsParameterRelation(const FRelationCase& RelationCase)
	{
		return IsRelation(RelationCase, "parameter_global")
			|| IsRelation(RelationCase, "parameter_member");
	}

	static bool IsBeforeDeclarationPath(const FScopeUsePathCase& PathCase)
	{
		return IsPath(PathCase, "function_before_declaration")
			|| IsPath(PathCase, "nested_block_before")
			|| IsPath(PathCase, "if_branch_before")
			|| IsPath(PathCase, "switch_case_before")
			|| IsPath(PathCase, "while_before")
			|| IsPath(PathCase, "foreach_before")
			|| IsPath(PathCase, "nested_call_caller_before");
	}

	static bool IsCallTargetPath(const FScopeUsePathCase& PathCase)
	{
		return IsPath(PathCase, "function_sibling_call")
			|| IsPath(PathCase, "nested_call_callee");
	}

	static bool IsInnerTargetPath(const FScopeUsePathCase& PathCase)
	{
		return IsPath(PathCase, "nested_block_inside")
			|| IsPath(PathCase, "if_branch_inside")
			|| IsPath(PathCase, "switch_case_inside")
			|| IsPath(PathCase, "for_initializer")
			|| IsPath(PathCase, "for_body")
			|| IsPath(PathCase, "while_body")
			|| IsPath(PathCase, "foreach_body")
			|| IsCallTargetPath(PathCase);
	}

	static bool ShouldCompile(
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		if (IsPath(PathCase, "after_owner"))
		{
			return IsParameterRelation(RelationCase);
		}
		return !IsBeforeDeclarationPath(PathCase) || IsParameterRelation(RelationCase);
	}

	static int32 ExpectedValue(
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		if (IsPath(PathCase, "after_owner") && IsParameterRelation(RelationCase))
		{
			return 5;
		}
		if (IsParameterRelation(RelationCase))
		{
			return 11;
		}
		if (IsInnerTargetPath(PathCase))
		{
			if (IsRelation(RelationCase, "inner_outer"))
			{
				return 22;
			}
			if (IsRelation(RelationCase, "sibling"))
			{
				return 21;
			}
		}
		return 11;
	}

	static int32 InnerValue(const FRelationCase& RelationCase)
	{
		if (IsRelation(RelationCase, "inner_outer"))
		{
			return 22;
		}
		if (IsRelation(RelationCase, "sibling"))
		{
			return 21;
		}
		return 11;
	}

	static FString MakeSuffix(
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs"),
			RelationCase.CatalogName,
			PathCase.CatalogName);
	}

	static void AppendOuterDeclaration(
		FString& Source,
		const FRelationCase& RelationCase,
		const TCHAR* Prefix = TEXT("\t"))
	{
		using namespace AngelscriptNativeTestSupport;

		if (!IsParameterRelation(RelationCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("%sint Subject = 11;"), Prefix));
		}
	}

	static void AppendInnerDeclaration(
		FString& Source,
		const FRelationCase& RelationCase,
		const TCHAR* Prefix)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsRelation(RelationCase, "inner_outer") || IsRelation(RelationCase, "sibling"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sint Subject = %d;"),
				Prefix,
				InnerValue(RelationCase)));
		}
	}

	static void AppendCallHelper(
		FString& Source,
		const FRelationCase& RelationCase,
		const TCHAR* FunctionName)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int %s(%s)"),
			FunctionName,
			IsParameterRelation(RelationCase) ? TEXT("int Subject") : TEXT("")));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (!IsParameterRelation(RelationCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tint Subject = %d;"),
				InnerValue(RelationCase)));
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn Subject;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString MakeCallExpression(
		const FRelationCase& RelationCase,
		const TCHAR* FunctionName)
	{
		return FString::Printf(
			TEXT("%s(%s)"),
			FunctionName,
			IsParameterRelation(RelationCase) ? TEXT("Subject") : TEXT(""));
	}

	static void AppendIndentedGeneratedSource(
		FString& Source,
		const FString& Body,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		TArray<FString> Lines;
		Body.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			AppendGeneratedAsLine(Source, Line.IsEmpty() ? FString() : Indent + Line);
		}
	}

	static void AppendNestedBlockBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "nested_block_before"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, RelationCase);
		AppendGeneratedAsLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, RelationCase, TEXT("\t\t"));
		if (IsPath(PathCase, "nested_block_inside"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, IsPath(PathCase, "nested_block_before")
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}

	static void AppendIfBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "if_branch_before"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, RelationCase);
		AppendGeneratedAsLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tif (true)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, RelationCase, TEXT("\t\t"));
		if (IsPath(PathCase, "if_branch_inside"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, IsPath(PathCase, "if_branch_before")
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}

	static void AppendSwitchBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "switch_case_before"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, RelationCase);
		AppendGeneratedAsLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tswitch (1)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\tcase 1:"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, RelationCase, TEXT("\t\t"));
		if (IsPath(PathCase, "switch_case_inside"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tScopeTrace = Subject;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
		AppendGeneratedAsLine(Source, TEXT("\t\tbreak;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, IsPath(PathCase, "switch_case_before")
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}

	static void AppendForBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "for_initializer"))
		{
			if (IsParameterRelation(RelationCase))
			{
				AppendGeneratedAsLine(Source, TEXT("\tfor (int Iteration = Subject; Iteration == 11; ++Iteration)"));
			}
			else
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\tfor (int Subject = %d; Subject == %d; ++Subject)"),
					InnerValue(RelationCase),
					InnerValue(RelationCase)));
			}
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			return;
		}

		AppendOuterDeclaration(Source, RelationCase);
		AppendGeneratedAsLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tfor (int Iteration = 0; Iteration < 1; ++Iteration)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, RelationCase, TEXT("\t\t"));
		if (IsPath(PathCase, "for_body"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}

	static void AppendWhileBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "while_before"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, RelationCase);
		AppendGeneratedAsLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Iteration = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\twhile (Iteration < 1)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, RelationCase, TEXT("\t\t"));
		AppendGeneratedAsLine(Source, TEXT("\t\t++Iteration;"));
		if (IsPath(PathCase, "while_body"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tScopeTrace = Subject;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, IsPath(PathCase, "while_before")
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}

	static void AppendForeachBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "foreach_before"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
		}
		AppendOuterDeclaration(Source, RelationCase);
		AppendGeneratedAsLine(Source, TEXT("\tint ScopeTrace = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseRange Range;"));
		AppendGeneratedAsLine(Source, TEXT("\tRange.Count = 1;"));
		AppendGeneratedAsLine(Source, TEXT("\tforeach (int Item : Range)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendInnerDeclaration(Source, RelationCase, TEXT("\t\t"));
		if (IsPath(PathCase, "foreach_body"))
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject + Item;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\tScopeTrace = Subject + Item;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, IsPath(PathCase, "foreach_before")
			? TEXT("\treturn Captured + ScopeTrace - ScopeTrace;")
			: TEXT("\treturn Subject + ScopeTrace - ScopeTrace;"));
	}

	static void AppendOwnerBody(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsPath(PathCase, "after_owner"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		}
		else if (IsPath(PathCase, "function_before_declaration"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
			AppendOuterDeclaration(Source, RelationCase);
			AppendGeneratedAsLine(Source, TEXT("\treturn Captured;"));
		}
		else if (IsPath(PathCase, "function_after_declaration"))
		{
			AppendOuterDeclaration(Source, RelationCase);
			AppendGeneratedAsLine(Source, TEXT("\treturn Subject;"));
		}
		else if (IsPath(PathCase, "function_sibling_call"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s;"),
				*MakeCallExpression(RelationCase, TEXT("ScopeSibling"))));
		}
		else if (IsPath(PathCase, "nested_block_before")
			|| IsPath(PathCase, "nested_block_inside")
			|| IsPath(PathCase, "nested_block_after"))
		{
			AppendNestedBlockBody(Source, RelationCase, PathCase);
		}
		else if (IsPath(PathCase, "if_branch_before")
			|| IsPath(PathCase, "if_branch_inside")
			|| IsPath(PathCase, "if_branch_after"))
		{
			AppendIfBody(Source, RelationCase, PathCase);
		}
		else if (IsPath(PathCase, "switch_case_before")
			|| IsPath(PathCase, "switch_case_inside")
			|| IsPath(PathCase, "switch_case_after"))
		{
			AppendSwitchBody(Source, RelationCase, PathCase);
		}
		else if (IsPath(PathCase, "for_initializer")
			|| IsPath(PathCase, "for_body")
			|| IsPath(PathCase, "for_after"))
		{
			AppendForBody(Source, RelationCase, PathCase);
		}
		else if (IsPath(PathCase, "while_before")
			|| IsPath(PathCase, "while_body")
			|| IsPath(PathCase, "while_after"))
		{
			AppendWhileBody(Source, RelationCase, PathCase);
		}
		else if (IsPath(PathCase, "foreach_before")
			|| IsPath(PathCase, "foreach_body")
			|| IsPath(PathCase, "foreach_after"))
		{
			AppendForeachBody(Source, RelationCase, PathCase);
		}
		else if (IsPath(PathCase, "nested_call_caller_before"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Captured = Subject;"));
			AppendOuterDeclaration(Source, RelationCase);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tint NestedObserved = %s;"),
				*MakeCallExpression(RelationCase, TEXT("NestedScopeCall"))));
			AppendGeneratedAsLine(Source, TEXT("\treturn Captured + NestedObserved - NestedObserved;"));
		}
		else if (IsPath(PathCase, "nested_call_callee"))
		{
			AppendOuterDeclaration(Source, RelationCase);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn %s;"),
				*MakeCallExpression(RelationCase, TEXT("NestedScopeCall"))));
		}
		else
		{
			AppendOuterDeclaration(Source, RelationCase);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tint NestedObserved = %s;"),
				*MakeCallExpression(RelationCase, TEXT("NestedScopeCall"))));
			AppendGeneratedAsLine(Source, TEXT("\treturn Subject + NestedObserved - NestedObserved;"));
		}
	}

	static void AppendGlobalOwner(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsRelation(RelationCase, "parameter_global"))
		{
			AppendGeneratedAsLine(Source, TEXT("const int Subject = 5;"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ScopeOwner(%s)"),
			IsRelation(RelationCase, "parameter_global") ? TEXT("int Subject") : TEXT("")));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendOwnerBody(Source, RelationCase, PathCase);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (IsPath(PathCase, "after_owner"))
		{
			AppendGeneratedAsLine(Source, TEXT("int ReadAfterOwner()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Subject;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunScopeCell()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsPath(PathCase, "after_owner"))
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ReadAfterOwner();"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn ScopeOwner(%s);"),
				IsRelation(RelationCase, "parameter_global") ? TEXT("11") : TEXT("")));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendMemberOwner(
		FString& Source,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FScopeOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Subject = 5;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint Evaluate(int Subject)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		FString Body;
		AppendOwnerBody(Body, RelationCase, PathCase);
		AppendIndentedGeneratedSource(Source, Body, TEXT("\t"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		if (IsPath(PathCase, "after_owner"))
		{
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tint ReadAfterOwner()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Subject;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunScopeCell()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFScopeOwner Owner = FScopeOwner();"));
		AppendGeneratedAsLine(Source, IsPath(PathCase, "after_owner")
			? TEXT("\treturn Owner.ReadAfterOwner();")
			: TEXT("\treturn Owner.Evaluate(11);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildScopeSource(
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		if (IsPath(PathCase, "function_sibling_call"))
		{
			AppendCallHelper(Source, RelationCase, TEXT("ScopeSibling"));
		}
		if (IsPath(PathCase, "nested_call_caller_before")
			|| IsPath(PathCase, "nested_call_callee")
			|| IsPath(PathCase, "nested_call_caller_after"))
		{
			AppendCallHelper(Source, RelationCase, TEXT("NestedScopeCall"));
		}
		if (IsRelation(RelationCase, "parameter_member"))
		{
			AppendMemberOwner(Source, RelationCase, PathCase);
		}
		else
		{
			AppendGlobalOwner(Source, RelationCase, PathCase);
		}
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunScopeRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages, const FString& Section)
	{
		return Messages.Entries.ContainsByPredicate([&Section](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	static asIScriptFunction* FindScopeFunction(
		asIScriptModule& Module,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		if (IsCallTargetPath(PathCase))
		{
			const FString Declaration = FString::Printf(
				TEXT("int %s(%s)"),
				IsPath(PathCase, "function_sibling_call") ? TEXT("ScopeSibling") : TEXT("NestedScopeCall"),
				IsParameterRelation(RelationCase) ? TEXT("int Subject") : TEXT(""));
			const FTCHARToUTF8 DeclarationUtf8(*Declaration);
			return Module.GetFunctionByDecl(DeclarationUtf8.Get());
		}
		if (IsRelation(RelationCase, "parameter_member"))
		{
			asITypeInfo* const OwnerType = Module.GetTypeInfoByName("FScopeOwner");
			if (OwnerType == nullptr)
			{
				return nullptr;
			}
			return OwnerType->GetMethodByDecl(IsPath(PathCase, "after_owner")
				? "int ReadAfterOwner()"
				: "int Evaluate(int Subject)");
		}
		if (IsPath(PathCase, "after_owner"))
		{
			return Module.GetFunctionByDecl("int ReadAfterOwner()");
		}
		return Module.GetFunctionByDecl(IsRelation(RelationCase, "parameter_global")
			? "int ScopeOwner(int Subject)"
			: "int ScopeOwner()");
	}

	void VerifyDebugMetadata(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptModule& Module,
		const FRelationCase& RelationCase,
		const FScopeUsePathCase& PathCase)
	{
		asIScriptFunction* const Function = FindScopeFunction(Module, RelationCase, PathCase);
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("scope cell should resolve its exact metadata owner"))));
		if (Function == nullptr)
		{
			return;
		}

		if (IsPath(PathCase, "after_owner"))
		{
			if (IsRelation(RelationCase, "parameter_global"))
			{
				bool bFoundGlobal = false;
				for (asUINT Index = 0; Index < Module.GetGlobalVarCount(); ++Index)
				{
					const char* Name = nullptr;
					int TypeId = asTYPEID_VOID;
					bool bConst = false;
					if (Module.GetGlobalVar(Index, &Name, nullptr, &TypeId, &bConst) >= 0
						&& Name != nullptr
						&& FCStringAnsi::Strcmp(Name, "Subject") == 0)
					{
						bFoundGlobal = true;
						ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT32), TypeId,
							*Case.Describe(TEXT("after-owner global should retain int type metadata"))));
						ASSERT_THAT(IsTrue(bConst,
							*Case.Describe(TEXT("after-owner global should retain fork-required constness"))));
					}
				}
				ASSERT_THAT(IsTrue(bFoundGlobal,
					*Case.Describe(TEXT("after-owner global relation should publish Subject"))));
			}
			else
			{
				asITypeInfo* const OwnerType = Module.GetTypeInfoByName("FScopeOwner");
				ASSERT_THAT(IsNotNull(OwnerType,
					*Case.Describe(TEXT("after-owner member relation should publish its owner type"))));
				if (OwnerType != nullptr)
				{
					const char* Name = nullptr;
					int TypeId = asTYPEID_VOID;
					ASSERT_THAT(IsTrue(OwnerType->GetProperty(0, &Name, &TypeId) >= 0
						&& Name != nullptr
						&& FCStringAnsi::Strcmp(Name, "Subject") == 0,
						*Case.Describe(TEXT("after-owner member relation should expose Subject metadata"))));
					ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT32), TypeId,
						*Case.Describe(TEXT("after-owner member should retain int type metadata"))));
				}
			}
			return;
		}

		if (IsParameterRelation(RelationCase))
		{
			int TypeId = asTYPEID_VOID;
			const char* Name = nullptr;
			ASSERT_THAT(IsTrue(Function->GetParam(0, &TypeId, nullptr, &Name) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Subject") == 0,
				*Case.Describe(TEXT("parameter shadow should expose exact Subject parameter metadata"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT32), TypeId,
				*Case.Describe(TEXT("parameter shadow should retain int type metadata"))));
			return;
		}

		int32 SubjectLocalCount = 0;
		for (asUINT Index = 0; Index < Function->GetVarCount(); ++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			if (Function->GetVar(Index, &Name, &TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Subject") == 0)
			{
				++SubjectLocalCount;
				ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT32), TypeId,
					*Case.Describe(TEXT("shadow local should retain int debug type metadata"))));
				const char* const Declaration = Function->GetVarDecl(Index, true);
				ASSERT_THAT(IsTrue(Declaration != nullptr
					&& FString(UTF8_TO_TCHAR(Declaration)).Contains(TEXT("int Subject")),
					*Case.Describe(TEXT("shadow local should expose its exact debug declaration"))));
			}
		}
		ASSERT_THAT(IsTrue(SubjectLocalCount > 0,
			*Case.Describe(TEXT("legal non-parameter shadow cell should expose at least one Subject local"))));
	}

	void ExecuteIntFunction(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const ANSICHAR* Declaration,
		const int32 Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Function = GetNativeFunctionByExactDecl(&Module, Declaration);
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("scope module should expose its exact execution declaration"))));
		if (Function == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("scope module should create an execution context"))));
		if (Context != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Function),
				*Case.Describe(TEXT("scope execution should finish"))));
			ASSERT_THAT(AreEqual(Expected, static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("scope execution should resolve the intended declaration value"))));
			Context->Release();
		}
	}

public:
	TEST_METHOD(RelationsByScopeUsePath)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-SHADOW",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Debug);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Variable-scope product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder IteratorRecorder;
		IteratorRecorder.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseRange(*ScriptEngine, IteratorRecorder),
			TEXT("Variable-scope product should register its one-iteration foreach fixture")));

		for (const FRelationCase& RelationCase : RelationCases)
		{
			for (const FScopeUsePathCase& PathCase : ScopeUsePathCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"LANG-VAR-SHADOW",
					{
						ANSI_TO_TCHAR(RelationCase.CatalogName),
						ANSI_TO_TCHAR(PathCase.CatalogName),
					}));
				const FString Suffix = MakeSuffix(RelationCase, PathCase);
				const FString ModuleName = TEXT("VariableScope_") + Suffix;
				const FString Source = BuildScopeSource(RelationCase, PathCase);
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
				const bool bShouldCompile = ShouldCompile(RelationCase, PathCase);
				if (bShouldCompile)
				{
					ASSERT_THAT(IsTrue(BuildResult >= 0,
						*Case.Describe(TEXT("legal scope-use path should compile"))));
					ASSERT_THAT(IsNotNull(Module,
						*Case.Describe(TEXT("legal scope-use path should publish a module"))));
					if (Module != nullptr)
					{
						VerifyDebugMetadata(Case, *Module, RelationCase, PathCase);
						ExecuteIntFunction(
							Case,
							*ScriptEngine,
							*Module,
							"int RunScopeCell()",
							ExpectedValue(RelationCase, PathCase));
					}
				}
				else
				{
					ASSERT_THAT(IsTrue(BuildResult < 0,
						*Case.Describe(TEXT("out-of-scope or use-before-declaration path should be rejected"))));
					ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
						*Case.Describe(TEXT("rejected scope-use path should report a located diagnostic"))));
					if (Module != nullptr)
					{
						ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "int RunScopeCell()"),
							*Case.Describe(TEXT("failed scope build should not publish its execution entry"))));
					}
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("scope cell should discard its isolated module"))));
				if (!bShouldCompile)
				{
					const FString RecoverySource = BuildRecoverySource();
					PrintGeneratedAsSource(
						*TestRunner,
						Case.GetId() + TEXT("-RECOVERY"),
						ModuleName,
						RecoverySource);
					const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
					Engine.ResetMessages();
					asIScriptModule* RecoveryModule = nullptr;
					ASSERT_THAT(IsTrue(CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						RecoverySourceUtf8.Get(),
						RecoveryModule) >= 0,
						*Case.Describe(TEXT("rejected scope cell should permit a same-name recovery build"))));
					ASSERT_THAT(IsNotNull(RecoveryModule,
						*Case.Describe(TEXT("scope recovery should publish a clean module"))));
					if (RecoveryModule != nullptr)
					{
						ExecuteIntFunction(
							Case,
							*ScriptEngine,
							*RecoveryModule,
							"int RunScopeRecovery()",
							97);
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("scope recovery should leave no module behind"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
