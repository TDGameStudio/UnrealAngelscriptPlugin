#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDeclarationCollisionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Declarations.Collisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	enum class EDeclarationShape : uint8
	{
		GlobalFunctionNoArgs,
		GlobalFunctionOneArg,
		ObjectType,
		EnumType,
		MethodNoArgs,
		MethodOneArg,
		Field,
		PropertyGet,
		PropertySet,
	};

	struct FPairCase
	{
		const ANSICHAR* CatalogName;
		EDeclarationShape Left;
		EDeclarationShape Right;
		bool bMemberOwner;
		bool bLegalInSameOwner;
	};

	struct FNamespaceRelationCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* LeftNamespace;
		const TCHAR* RightNamespace;
		bool bSameOwner;
	};

	struct FInsertionOrderCase
	{
		const ANSICHAR* CatalogName;
		bool bLeftFirst;
	};

	static void AppendGeneratedAsLine(FString& Source, const FString& Line = FString())
	{
		AngelscriptNativeTestSupport::AppendGeneratedAsLine(Source, Line);
	}

	inline static constexpr FPairCase PairCases[] =
	{
		{ "function_overload", EDeclarationShape::GlobalFunctionNoArgs, EDeclarationShape::GlobalFunctionOneArg, false, true },
		{ "function_duplicate", EDeclarationShape::GlobalFunctionNoArgs, EDeclarationShape::GlobalFunctionNoArgs, false, false },
		{ "function_type", EDeclarationShape::GlobalFunctionNoArgs, EDeclarationShape::ObjectType, false, false },
		{ "function_enum", EDeclarationShape::GlobalFunctionNoArgs, EDeclarationShape::EnumType, false, false },
		{ "type_duplicate", EDeclarationShape::ObjectType, EDeclarationShape::ObjectType, false, false },
		{ "type_enum", EDeclarationShape::ObjectType, EDeclarationShape::EnumType, false, false },
		{ "enum_duplicate", EDeclarationShape::EnumType, EDeclarationShape::EnumType, false, false },
		{ "method_overload", EDeclarationShape::MethodNoArgs, EDeclarationShape::MethodOneArg, true, true },
		{ "method_duplicate", EDeclarationShape::MethodNoArgs, EDeclarationShape::MethodNoArgs, true, false },
		// The current fork rejects a field that reuses a method name; retain the
		// source as an enabled negative collision rather than calling it legal.
		{ "method_field", EDeclarationShape::MethodNoArgs, EDeclarationShape::Field, true, false },
		// Automatic property syntax was removed by the current fork; retain the
		// method/property source as an enabled negative declaration product.
		{ "method_property", EDeclarationShape::MethodNoArgs, EDeclarationShape::PropertyGet, true, false },
		{ "field_duplicate", EDeclarationShape::Field, EDeclarationShape::Field, true, false },
		{ "field_property", EDeclarationShape::Field, EDeclarationShape::PropertyGet, true, false },
		{ "property_get_set", EDeclarationShape::PropertyGet, EDeclarationShape::PropertySet, true, false },
		{ "property_duplicate_get", EDeclarationShape::PropertyGet, EDeclarationShape::PropertyGet, true, false },
		{ "property_duplicate_set", EDeclarationShape::PropertySet, EDeclarationShape::PropertySet, true, false },
	};

	inline static constexpr FNamespaceRelationCase NamespaceRelationCases[] =
	{
		{ "same", TEXT("Collision"), TEXT("Collision"), true },
		{ "different", TEXT("CollisionLeft"), TEXT("CollisionRight"), false },
		{ "nested", TEXT("CollisionOuter"), TEXT("CollisionOuter::CollisionInner"), false },
	};

	inline static constexpr FInsertionOrderCase InsertionOrderCases[] =
	{
		{ "left_then_right", true },
		{ "right_then_left", false },
	};


	static FString MakeSuffix(
		const FInsertionOrderCase& InsertionOrderCase,
		const FNamespaceRelationCase& NamespaceRelationCase,
		const FPairCase& PairCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			InsertionOrderCase.CatalogName,
			NamespaceRelationCase.CatalogName,
			PairCase.CatalogName);
	}

	static FString Indent(const int32 Depth)
	{
		FString Result;
		for (int32 Index = 0; Index < Depth; ++Index)
		{
			Result += TEXT("\t");
		}
		return Result;
	}

	static int32 AppendNamespaceOpen(FString& Source, const FString& Namespace)
	{
		TArray<FString> Parts;
		Namespace.ParseIntoArray(Parts, TEXT("::"), true);
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			const FString Prefix = Indent(Index);
			AppendGeneratedAsLine(Source, Prefix + TEXT("namespace ") + Parts[Index]);
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
		}
		return Parts.Num();
	}

	static void AppendNamespaceClose(FString& Source, const int32 Depth)
	{
		for (int32 Index = Depth - 1; Index >= 0; --Index)
		{
			AppendGeneratedAsLine(Source, Indent(Index) + TEXT("}"));
		}
		AppendGeneratedAsLine(Source);
	}

	static void AppendGlobalDeclaration(
		FString& Source,
		const EDeclarationShape Shape,
		const int32 Depth)
	{
		const FString Prefix = Indent(Depth);
		switch (Shape)
		{
		case EDeclarationShape::GlobalFunctionNoArgs:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash()"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\treturn 11;"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		case EDeclarationShape::GlobalFunctionOneArg:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash(int Value)"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\treturn Value + 12;"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		case EDeclarationShape::ObjectType:
			AppendGeneratedAsLine(Source, Prefix + TEXT("class Clash"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\tint Value = 13;"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		case EDeclarationShape::EnumType:
			AppendGeneratedAsLine(Source, Prefix + TEXT("enum Clash"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\tClashValue = 14"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		default:
			checkNoEntry();
			break;
		}
		AppendGeneratedAsLine(Source);
	}

	static void AppendMemberDeclaration(
		FString& Source,
		const EDeclarationShape Shape,
		const int32 Depth)
	{
		const FString Prefix = Indent(Depth);
		switch (Shape)
		{
		case EDeclarationShape::MethodNoArgs:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash()"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\treturn 21;"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		case EDeclarationShape::MethodOneArg:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash(int Value)"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\treturn Value + 22;"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		case EDeclarationShape::Field:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash = 23;"));
			break;
		case EDeclarationShape::PropertyGet:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\tget"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\t{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\t\treturn 24;"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\t}"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		case EDeclarationShape::PropertySet:
			AppendGeneratedAsLine(Source, Prefix + TEXT("int Clash"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\tset"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\t{"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("\t}"));
			AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
			break;
		default:
			checkNoEntry();
			break;
		}
		AppendGeneratedAsLine(Source);
	}

	static void AppendMemberOwner(
		FString& Source,
		const FString& Namespace,
		const EDeclarationShape First,
		const EDeclarationShape* Second)
	{
		const int32 NamespaceDepth = AppendNamespaceOpen(Source, Namespace);
		const FString Prefix = Indent(NamespaceDepth);
		AppendGeneratedAsLine(Source, Prefix + TEXT("class FOwner"));
		AppendGeneratedAsLine(Source, Prefix + TEXT("{"));
		AppendMemberDeclaration(Source, First, NamespaceDepth + 1);
		if (Second != nullptr)
		{
			AppendMemberDeclaration(Source, *Second, NamespaceDepth + 1);
		}
		AppendGeneratedAsLine(Source, Prefix + TEXT("}"));
		AppendNamespaceClose(Source, NamespaceDepth);
	}

	static void AppendGlobalOwner(
		FString& Source,
		const FString& Namespace,
		const EDeclarationShape First,
		const EDeclarationShape* Second)
	{
		const int32 NamespaceDepth = AppendNamespaceOpen(Source, Namespace);
		AppendGlobalDeclaration(Source, First, NamespaceDepth);
		if (Second != nullptr)
		{
			AppendGlobalDeclaration(Source, *Second, NamespaceDepth);
		}
		AppendNamespaceClose(Source, NamespaceDepth);
	}

	static FString BuildCollisionSource(
		const FInsertionOrderCase& InsertionOrderCase,
		const FNamespaceRelationCase& NamespaceRelationCase,
		const FPairCase& PairCase)
	{
		const EDeclarationShape First = InsertionOrderCase.bLeftFirst ? PairCase.Left : PairCase.Right;
		const EDeclarationShape Second = InsertionOrderCase.bLeftFirst ? PairCase.Right : PairCase.Left;
		FString Source;
		if (NamespaceRelationCase.bSameOwner)
		{
			if (PairCase.bMemberOwner)
			{
				AppendMemberOwner(Source, NamespaceRelationCase.LeftNamespace, First, &Second);
			}
			else
			{
				AppendGlobalOwner(Source, NamespaceRelationCase.LeftNamespace, First, &Second);
			}
		}
		else
		{
			if (PairCase.bMemberOwner)
			{
				AppendMemberOwner(Source, NamespaceRelationCase.LeftNamespace, First, nullptr);
				AppendMemberOwner(Source, NamespaceRelationCase.RightNamespace, Second, nullptr);
			}
			else
			{
				AppendGlobalOwner(Source, NamespaceRelationCase.LeftNamespace, First, nullptr);
				AppendGlobalOwner(Source, NamespaceRelationCase.RightNamespace, Second, nullptr);
			}
		}

		AppendGeneratedAsLine(Source, TEXT("int RunCollisionPublication()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 71;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunCollisionRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 91;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages, const FString& Section)
	{
		return Messages.Entries.ContainsByPredicate([&Section](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == Section
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	static asITypeInfo* FindObjectType(
		asIScriptModule& Module,
		const ANSICHAR* Name,
		const FString& Namespace)
	{
		for (asUINT Index = 0; Index < Module.GetObjectTypeCount(); ++Index)
		{
			asITypeInfo* const Type = Module.GetObjectTypeByIndex(Index);
			if (Type != nullptr
				&& FCStringAnsi::Strcmp(Type->GetName(), Name) == 0
				&& FString(UTF8_TO_TCHAR(Type->GetNamespace())) == Namespace)
			{
				return Type;
			}
		}
		return nullptr;
	}

	static asITypeInfo* FindEnumType(
		asIScriptModule& Module,
		const ANSICHAR* Name,
		const FString& Namespace)
	{
		for (asUINT Index = 0; Index < Module.GetEnumCount(); ++Index)
		{
			asITypeInfo* const Type = Module.GetEnumByIndex(Index);
			if (Type != nullptr
				&& FCStringAnsi::Strcmp(Type->GetName(), Name) == 0
				&& FString(UTF8_TO_TCHAR(Type->GetNamespace())) == Namespace)
			{
				return Type;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindGlobalFunction(
		asIScriptModule& Module,
		const FString& Namespace,
		const asUINT ParamCount)
	{
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetName(), "Clash") == 0
				&& FString(UTF8_TO_TCHAR(Function->GetNamespace())) == Namespace
				&& Function->GetParamCount() == ParamCount)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static bool TypeHasField(asITypeInfo& Type)
	{
		for (asUINT Index = 0; Index < Type.GetPropertyCount(); ++Index)
		{
			const char* Name = nullptr;
			if (Type.GetProperty(Index, &Name) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, "Clash") == 0)
			{
				return true;
			}
		}
		return false;
	}

	static asIScriptFunction* FindMethod(
		asITypeInfo& Type,
		const ANSICHAR* Name,
		const asUINT ParamCount)
	{
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Function = Type.GetMethodByIndex(Index);
			if (Function != nullptr
				&& FCStringAnsi::Strcmp(Function->GetName(), Name) == 0
				&& Function->GetParamCount() == ParamCount)
			{
				return Function;
			}
		}
		return nullptr;
	}

	void VerifyShapePublished(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptModule& Module,
		const FString& Namespace,
		const EDeclarationShape Shape,
		const bool bMemberOwner)
	{
		if (!bMemberOwner)
		{
			switch (Shape)
			{
			case EDeclarationShape::GlobalFunctionNoArgs:
				ASSERT_THAT(IsNotNull(FindGlobalFunction(Module, Namespace, 0),
					*Case.Describe(TEXT("zero-parameter global collision participant should publish in the expected namespace"))));
				break;
			case EDeclarationShape::GlobalFunctionOneArg:
				ASSERT_THAT(IsNotNull(FindGlobalFunction(Module, Namespace, 1),
					*Case.Describe(TEXT("one-parameter global collision participant should publish in the expected namespace"))));
				break;
			case EDeclarationShape::ObjectType:
				ASSERT_THAT(IsNotNull(FindObjectType(Module, "Clash", Namespace),
					*Case.Describe(TEXT("object-type collision participant should publish in the expected namespace"))));
				break;
			case EDeclarationShape::EnumType:
				ASSERT_THAT(IsNotNull(FindEnumType(Module, "Clash", Namespace),
					*Case.Describe(TEXT("enum collision participant should publish in the expected namespace"))));
				break;
			default:
				ASSERT_THAT(IsTrue(false,
					*Case.Describe(TEXT("global collision participant should use a global declaration shape"))));
				break;
			}
			return;
		}

		asITypeInfo* const Owner = FindObjectType(Module, "FOwner", Namespace);
		ASSERT_THAT(IsNotNull(Owner,
			*Case.Describe(TEXT("member collision participant should publish its owner type"))));
		if (Owner == nullptr)
		{
			return;
		}
		switch (Shape)
		{
		case EDeclarationShape::MethodNoArgs:
			ASSERT_THAT(IsNotNull(FindMethod(*Owner, "Clash", 0),
				*Case.Describe(TEXT("zero-parameter method participant should publish"))));
			break;
		case EDeclarationShape::MethodOneArg:
			ASSERT_THAT(IsNotNull(FindMethod(*Owner, "Clash", 1),
				*Case.Describe(TEXT("one-parameter method participant should publish"))));
			break;
		case EDeclarationShape::Field:
			ASSERT_THAT(IsTrue(TypeHasField(*Owner),
				*Case.Describe(TEXT("field collision participant should publish"))));
			break;
		case EDeclarationShape::PropertyGet:
			ASSERT_THAT(IsNotNull(FindMethod(*Owner, "get_Clash", 0),
				*Case.Describe(TEXT("virtual-property getter participant should publish"))));
			break;
		case EDeclarationShape::PropertySet:
			ASSERT_THAT(IsNotNull(FindMethod(*Owner, "set_Clash", 1),
				*Case.Describe(TEXT("virtual-property setter participant should publish"))));
			break;
		default:
			ASSERT_THAT(IsTrue(false,
				*Case.Describe(TEXT("member collision participant should use a member declaration shape"))));
			break;
		}
	}

	void VerifyRunResult(
		const AngelscriptNativeTestSupport::FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const ANSICHAR* Declaration,
		const int32 Expected)
	{
		asIScriptFunction* const Function = AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(&Module, Declaration);
		ASSERT_THAT(IsNotNull(Function,
			*Case.Describe(TEXT("collision module should expose its exact execution probe"))));
		if (Function == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("collision module should create an execution context"))));
		if (Context != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), AngelscriptNativeTestSupport::PrepareAndExecute(Context, Function),
				*Case.Describe(TEXT("collision module execution probe should finish"))));
			ASSERT_THAT(AreEqual(Expected, static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("collision module execution probe should preserve its generation marker"))));
			Context->Release();
		}
	}

public:
	TEST_METHOD(PairsByNamespaceRelationAndInsertionOrder)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-DECL-COLLISION",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Declaration collision product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		for (const FInsertionOrderCase& InsertionOrderCase : InsertionOrderCases)
		{
			for (const FNamespaceRelationCase& NamespaceRelationCase : NamespaceRelationCases)
			{
				for (const FPairCase& PairCase : PairCases)
				{
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-DECL-COLLISION",
						{
							ANSI_TO_TCHAR(InsertionOrderCase.CatalogName),
							ANSI_TO_TCHAR(NamespaceRelationCase.CatalogName),
							ANSI_TO_TCHAR(PairCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(InsertionOrderCase, NamespaceRelationCase, PairCase);
					const FString ModuleName = TEXT("DeclarationCollision_") + Suffix;
					const FString Source = BuildCollisionSource(InsertionOrderCase, NamespaceRelationCase, PairCase);
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
					const bool bUsesRemovedPropertySyntax =
						PairCase.Left == EDeclarationShape::PropertyGet
						|| PairCase.Left == EDeclarationShape::PropertySet
						|| PairCase.Right == EDeclarationShape::PropertyGet
						|| PairCase.Right == EDeclarationShape::PropertySet;
					const bool bShouldCompile = !bUsesRemovedPropertySyntax
						&& (!NamespaceRelationCase.bSameOwner || PairCase.bLegalInSameOwner);
					if (bShouldCompile)
					{
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*FString::Printf(TEXT("%s legal overload, accessor pair, or separated names should compile. BuildResult=%d Messages={%s}"),
								*Case.GetId(), BuildResult, *Engine.GetMessagesText())));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("legal declaration pair should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							const EDeclarationShape First = InsertionOrderCase.bLeftFirst ? PairCase.Left : PairCase.Right;
							const EDeclarationShape Second = InsertionOrderCase.bLeftFirst ? PairCase.Right : PairCase.Left;
							VerifyShapePublished(Case, *Module, NamespaceRelationCase.LeftNamespace, First, PairCase.bMemberOwner);
							VerifyShapePublished(Case, *Module, NamespaceRelationCase.RightNamespace, Second, PairCase.bMemberOwner);
							VerifyRunResult(Case, *ScriptEngine, *Module, "int RunCollisionPublication()", 71);
						}
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("illegal same-owner declaration collision should be rejected"))));
						ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
							*Case.Describe(TEXT("illegal declaration collision should report one located owning error"))));
						if (Module != nullptr)
						{
							ASSERT_THAT(IsNull(GetNativeFunctionByExactDecl(Module, "int RunCollisionPublication()"),
								*Case.Describe(TEXT("failed collision build should not publish its execution probe"))));
						}
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("collision cell should discard its first module generation"))));

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
						*Case.Describe(TEXT("discarded collision module name should accept a clean recovery build"))));
					ASSERT_THAT(IsNotNull(RecoveryModule,
						*Case.Describe(TEXT("clean collision recovery should publish a module"))));
					if (RecoveryModule != nullptr)
					{
						VerifyRunResult(Case, *ScriptEngine, *RecoveryModule, "int RunCollisionRecovery()", 91);
					}
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("collision recovery should leave no module behind"))));
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
