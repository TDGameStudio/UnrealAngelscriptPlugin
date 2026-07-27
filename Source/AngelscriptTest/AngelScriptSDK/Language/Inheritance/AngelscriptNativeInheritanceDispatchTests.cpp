#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FInheritanceDispatchTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Inheritance.Dispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EDispatchView : uint8
	{
		DerivedObject,
		BaseView,
		ExplicitBase,
		Owner,
		DerivedImplementation,
	};

	enum class EDispatchMember : uint8
	{
		Field,
		StableMethod,
		VirtualMethod,
		ExplicitOverride,
		GetterMethod,
		SetterMethod,
	};

	enum class EInvocationRoute : uint8
	{
		Direct,
		VirtualRoute,
		ExplicitBase,
	};

	struct FDepthCase
	{
		const ANSICHAR* CatalogName;
		int32 BaseEdges;
	};

	struct FMemberCase
	{
		const ANSICHAR* CatalogName;
		EDispatchMember Member;
	};

	struct FViewCase
	{
		const ANSICHAR* CatalogName;
		EDispatchView View;
	};

	struct FInvocationCase
	{
		const ANSICHAR* CatalogName;
		EInvocationRoute Route;
	};

	inline static constexpr FDepthCase DepthCases[] =
	{
		{ "base", 1 },
		{ "two_levels", 2 },
		{ "three_levels", 3 },
		{ "deep", 8 },
	};

	inline static constexpr FMemberCase MemberCases[] =
	{
		{ "field", EDispatchMember::Field },
		{
			"nonvirtual_method",
			EDispatchMember::StableMethod,
		},
		{
			"virtual_method",
			EDispatchMember::VirtualMethod,
		},
		{ "override", EDispatchMember::ExplicitOverride },
		{
			"getter_method",
			EDispatchMember::GetterMethod,
		},
		{
			"setter_method",
			EDispatchMember::SetterMethod,
		},
	};

	inline static constexpr FViewCase ViewCases[] =
	{
		{
			"derived_object",
			EDispatchView::DerivedObject,
		},
		{ "base_view", EDispatchView::BaseView },
		{
			"explicit_base",
			EDispatchView::ExplicitBase,
		},
		{ "owner", EDispatchView::Owner },
		{
			"derived_impl",
			EDispatchView::DerivedImplementation,
		},
	};

	inline static constexpr FInvocationCase InvocationCases[] =
	{
		{ "direct", EInvocationRoute::Direct },
		{
			"virtual_route",
			EInvocationRoute::VirtualRoute,
		},
		{
			"explicit_base",
			EInvocationRoute::ExplicitBase,
		},
	};

	static bool IsOverriddenMember(
		const FMemberCase& Member)
	{
		return Member.Member != EDispatchMember::StableMethod;
	}

	static bool UsesExplicitBase(
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		return View.View == EDispatchView::ExplicitBase
			|| Invocation.Route
				== EInvocationRoute::ExplicitBase;
	}

	static bool UsesBaseView(
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		return !UsesExplicitBase(View, Invocation)
			&& (View.View == EDispatchView::BaseView
				|| Invocation.Route
					== EInvocationRoute::VirtualRoute);
	}

	static bool UsesOwnerProbe(const FViewCase& View)
	{
		return View.View == EDispatchView::Owner;
	}

	static bool UsesDerivedMemberProbe(
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		return UsesExplicitBase(View, Invocation)
			|| View.View
				== EDispatchView::DerivedImplementation;
	}

	static FString TypeNameForLevel(
		const int32 Level,
		const int32 BaseEdges)
	{
		if (Level == 0)
		{
			return TEXT("FDispatchRoot");
		}
		if (Level == BaseEdges)
		{
			return TEXT("FDispatchPrimary");
		}
		return FString::Printf(
			TEXT("FDispatchLevel%d"),
			Level);
	}

	static FString MethodName(
		const FMemberCase& Member)
	{
		switch (Member.Member)
		{
		case EDispatchMember::Field:
			return TEXT("ReadDispatchField");
		case EDispatchMember::StableMethod:
			return TEXT("StableValue");
		case EDispatchMember::VirtualMethod:
			return TEXT("VirtualValue");
		case EDispatchMember::ExplicitOverride:
			return TEXT("OverrideValue");
		case EDispatchMember::GetterMethod:
			return TEXT("GetValue");
		case EDispatchMember::SetterMethod:
			return TEXT("SetValue");
		default:
			return FString();
		}
	}

	static FString MethodDeclaration(
		const FMemberCase& Member)
	{
		return Member.Member == EDispatchMember::SetterMethod
			? TEXT("int SetValue(int)")
			: TEXT("int ") + MethodName(Member) + TEXT("()");
	}

	static FString MethodCall(
		const FMemberCase& Member,
		const FString& Prefix = FString())
	{
		if (Member.Member == EDispatchMember::SetterMethod)
		{
			return Prefix + MethodName(Member) + TEXT("(7)");
		}
		return Prefix + MethodName(Member) + TEXT("()");
	}

	static FString DirectMemberExpression(
		const FMemberCase& Member,
		const FString& Prefix = FString())
	{
		if (Member.Member == EDispatchMember::Field)
		{
			return Prefix + TEXT("DispatchField");
		}
		return MethodCall(Member, Prefix);
	}

	static void AppendRootTarget(
		FString& Source,
		const FMemberCase& Member)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Member.Member == EDispatchMember::Field)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\tint DispatchField = 701;"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(
			Source,
			Member.Member == EDispatchMember::SetterMethod
				? TEXT("\tint SetValue(int InValue)")
				: FString::Printf(
					TEXT("\tint %s()%s"),
					*MethodName(Member),
					Member.Member
							== EDispatchMember::StableMethod
						? TEXT(" final")
						: TEXT("")));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (Member.Member == EDispatchMember::Field)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\treturn DispatchField;"));
		}
		else if (Member.Member
			== EDispatchMember::SetterMethod)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\treturn 100 + InValue;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 100;"));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendDerivedTarget(
		FString& Source,
		const FMemberCase& Member,
		const int32 Level)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!IsOverriddenMember(Member))
		{
			return;
		}
		const bool bExplicitOverride =
			Member.Member != EDispatchMember::VirtualMethod;
		AppendGeneratedAsLine(
			Source,
			Member.Member == EDispatchMember::SetterMethod
				? FString::Printf(
					TEXT("\tint SetValue(int InValue)%s"),
					bExplicitOverride
						? TEXT(" override")
						: TEXT(""))
				: FString::Printf(
					TEXT("\tint %s()%s"),
					*MethodName(Member),
					bExplicitOverride
						? TEXT(" override")
						: TEXT("")));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (Member.Member == EDispatchMember::Field)
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t\treturn DispatchField + %d;"),
					Level * 10));
		}
		else if (Member.Member
			== EDispatchMember::SetterMethod)
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t\treturn %d + InValue;"),
					100 + Level * 100));
		}
		else
		{
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t\treturn %d;"),
					100 + Level * 100));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static FString ProbeExpression(
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation,
		const bool bInsideMember)
	{
		if (UsesExplicitBase(View, Invocation))
		{
			return MethodCall(
				Member,
				TEXT("FDispatchRoot::"));
		}
		if (UsesBaseView(View, Invocation))
		{
			return Invocation.Route
					== EInvocationRoute::VirtualRoute
				? MethodCall(
					Member,
					TEXT("BaseView."))
				: DirectMemberExpression(
					Member,
					TEXT("BaseView."));
		}
		if (bInsideMember)
		{
			return DirectMemberExpression(Member);
		}
		return DirectMemberExpression(
			Member,
			TEXT("Object."));
	}

	static void AppendMemberProbe(
		FString& Source,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			TEXT("\tint ProbeMember()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		if (UsesBaseView(View, Invocation))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tFDispatchRoot BaseView = this;"));
		}
		AppendGeneratedAsLine(
			Source,
			TEXT("\t\treturn ")
				+ ProbeExpression(
					Member,
					View,
					Invocation,
					true)
				+ TEXT(";"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendTypeChain(
		FString& Source,
		const FDepthCase& Depth,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		using namespace AngelscriptNativeTestSupport;

		for (int32 Level = 0;
			Level <= Depth.BaseEdges;
			++Level)
		{
			const FString TypeName =
				TypeNameForLevel(Level, Depth.BaseEdges);
			if (Level == 0)
			{
				AppendGeneratedAsLine(
					Source,
					TEXT("class FDispatchRoot"));
			}
			else
			{
				AppendGeneratedAsLine(
					Source,
					FString::Printf(
						TEXT("class %s : %s"),
						*TypeName,
						*TypeNameForLevel(
							Level - 1,
							Depth.BaseEdges)));
			}
			AppendGeneratedAsLine(Source, TEXT("{"));
			if (Level == 0)
			{
				AppendRootTarget(Source, Member);
			}
			else
			{
				AppendDerivedTarget(
					Source,
					Member,
					Level);
			}

			const bool bAddOwnerProbe =
				Level == 0 && UsesOwnerProbe(View);
			const bool bAddDerivedProbe =
				Level == Depth.BaseEdges
				&& UsesDerivedMemberProbe(
					View,
					Invocation);
			if (bAddOwnerProbe || bAddDerivedProbe)
			{
				AppendGeneratedAsLine(Source);
				AppendMemberProbe(
					Source,
					Member,
					View,
					Invocation);
			}
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static FString ProbeParameterType(
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		if (UsesOwnerProbe(View))
		{
			return TEXT("FDispatchRoot");
		}
		if (View.View == EDispatchView::BaseView
			&& Invocation.Route
				!= EInvocationRoute::ExplicitBase)
		{
			return TEXT("FDispatchRoot");
		}
		return TEXT("FDispatchPrimary");
	}

	static FString GlobalProbeDeclaration(
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		return FString::Printf(
			TEXT("int ProbeDispatch(%s)"),
			*ProbeParameterType(
				View,
				Invocation));
	}

	static FString GlobalProbeSourceDeclaration(
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		return FString::Printf(
			TEXT("int ProbeDispatch(%s Object)"),
			*ProbeParameterType(
				View,
				Invocation));
	}

	static void AppendGlobalProbe(
		FString& Source,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			GlobalProbeSourceDeclaration(
				View,
				Invocation));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (UsesOwnerProbe(View)
			|| UsesDerivedMemberProbe(View, Invocation))
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Object.ProbeMember();"));
		}
		else
		{
			if (UsesBaseView(View, Invocation))
			{
				AppendGeneratedAsLine(
					Source,
					TEXT("\tFDispatchRoot BaseView = Object;"));
			}
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn ")
					+ ProbeExpression(
						Member,
						View,
						Invocation,
						false)
					+ TEXT(";"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildInheritanceDispatchSource(
		const FDepthCase& Depth,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		FString Source;
		AppendTypeChain(
			Source,
			Depth,
			Member,
			View,
			Invocation);
		AppendGlobalProbe(
			Source,
			Member,
			View,
			Invocation);
		return Source;
	}

	static FString BuildInheritanceDispatchRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunInheritanceDispatchRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 307;"));
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

	static bool BytecodeCallsFunction(
		asIScriptFunction& Function,
		const int32 ExpectedFunctionId,
		const bool bRequireDirect)
	{
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode =
			Function.GetByteCode(&BytecodeLength);
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
				&& (!bRequireDirect || Opcode == asBC_CALL))
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

	static bool HasBytecode(asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return false;
		}
		asUINT BytecodeLength = 0;
		return Function->GetByteCode(&BytecodeLength) != nullptr
			&& BytecodeLength > 0;
	}

	static asIScriptFunction* FindTargetMethod(
		asITypeInfo& Type,
		const FMemberCase& Member)
	{
		if (Member.Member == EDispatchMember::Field)
		{
			return nullptr;
		}
		const FTCHARToUTF8 DeclarationUtf8(
			*MethodDeclaration(Member));
		if (asIScriptFunction* Function = Type.GetMethodByDecl(DeclarationUtf8.Get()))
		{
			return Function;
		}
		if (Member.Member == EDispatchMember::SetterMethod)
		{
			if (asIScriptFunction* Function = Type.GetMethodByDecl("int SetValue(const int)"))
			{
				return Function;
			}
			if (asIScriptFunction* Function = Type.GetMethodByDecl("int SetValue(int InValue)"))
			{
				return Function;
			}
		}
		const FString ExpectedName = MethodName(Member);
		const asUINT ExpectedParameterCount =
			Member.Member == EDispatchMember::SetterMethod ? 1 : 0;
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Type.GetMethodByIndex(Index);
			if (Candidate != nullptr
				&& Candidate->GetName() != nullptr
				&& ExpectedName == UTF8_TO_TCHAR(Candidate->GetName())
				&& Candidate->GetParamCount() == ExpectedParameterCount)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindMethodByName(
		asITypeInfo& Type,
		const ANSICHAR* Name,
		const asUINT ParameterCount)
	{
		for (asUINT Index = 0; Index < Type.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Type.GetMethodByIndex(Index);
			if (Candidate != nullptr
				&& Candidate->GetName() != nullptr
				&& FCStringAnsi::Strcmp(Candidate->GetName(), Name) == 0
				&& Candidate->GetParamCount() == ParameterCount)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindGlobalProbe(
		asIScriptModule& Module,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		const FString Type = ProbeParameterType(View, Invocation);
		const FString Declaration = FString::Printf(
			TEXT("int ProbeDispatch(%s)"),
			*Type);
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get()))
		{
			return Function;
		}
		const FString ConstDeclaration = FString::Printf(
			TEXT("int ProbeDispatch(const %s)"),
			*Type);
		const FTCHARToUTF8 ConstDeclarationUtf8(*ConstDeclaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(ConstDeclarationUtf8.Get()))
		{
			return Function;
		}
		const FString NamedDeclaration = FString::Printf(
			TEXT("int ProbeDispatch(%s Object)"),
			*Type);
		const FTCHARToUTF8 NamedDeclarationUtf8(*NamedDeclaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(NamedDeclarationUtf8.Get()))
		{
			return Function;
		}
		const FString ConstNamedDeclaration = FString::Printf(
			TEXT("int ProbeDispatch(const %s Object)"),
			*Type);
		const FTCHARToUTF8 ConstNamedDeclarationUtf8(*ConstNamedDeclaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(ConstNamedDeclarationUtf8.Get()))
		{
			return Function;
		}
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(Index);
			if (Candidate != nullptr
				&& Candidate->GetName() != nullptr
				&& FCStringAnsi::Strcmp(Candidate->GetName(), "ProbeDispatch") == 0
				&& Candidate->GetParamCount() == 1)
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* ProbeWitness(
		asIScriptModule& Module,
		asITypeInfo& Root,
		asITypeInfo& Primary,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		if (UsesOwnerProbe(View))
		{
			return FindMethodByName(Root, "ProbeMember", 0);
		}
		if (UsesDerivedMemberProbe(View, Invocation))
		{
			return FindMethodByName(Primary, "ProbeMember", 0);
		}
		return FindGlobalProbe(Module, View, Invocation);
	}

	static asIScriptFunction* ExpectedCompiledTarget(
		asIScriptFunction& RootTarget,
		asIScriptFunction& PrimaryTarget,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		if (Member.Member == EDispatchMember::StableMethod
			|| UsesExplicitBase(View, Invocation)
			|| UsesBaseView(View, Invocation)
			|| UsesOwnerProbe(View))
		{
			return &RootTarget;
		}
		return &PrimaryTarget;
	}

	void VerifyChain(
		const FNativeCaseContext& Case,
		const FDepthCase& Depth,
		asIScriptModule& Module,
		asITypeInfo& Root,
		asITypeInfo& Primary)
	{
		asITypeInfo* Current = &Primary;
		for (int32 Level = Depth.BaseEdges;
			Level > 0;
			--Level)
		{
			asITypeInfo* const ExpectedBase =
				Module.GetTypeInfoByName(
					TCHAR_TO_UTF8(
						*TypeNameForLevel(
							Level - 1,
							Depth.BaseEdges)));
			ASSERT_THAT(IsNotNull(ExpectedBase,
				*Case.Describe(TEXT("dispatch chain should publish every expected level"))));
			if (ExpectedBase == nullptr || Current == nullptr)
			{
				return;
			}
			ASSERT_THAT(AreEqual(
				ExpectedBase,
				Current->GetBaseType(),
				*Case.Describe(TEXT("dispatch chain should retain every direct base edge"))));
			Current = ExpectedBase;
		}
		ASSERT_THAT(AreEqual(
			&Root,
			Current,
			*Case.Describe(TEXT("dispatch chain should terminate at the root"))));
		ASSERT_THAT(IsTrue(
			Primary.DerivesFrom(&Root),
			*Case.Describe(TEXT("dispatch primary should derive transitively from root"))));
	}

	void VerifyFieldMetadata(
		const FNativeCaseContext& Case,
		asITypeInfo& Root,
		asITypeInfo& Primary)
	{
		bool bFoundRoot = false;
		for (asUINT Index = 0;
			Index < Root.GetPropertyCount();
			++Index)
		{
			const char* Name = nullptr;
			int TypeId = asTYPEID_VOID;
			if (Root.GetProperty(
				Index,
				&Name,
				&TypeId) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(
					Name,
					"DispatchField") == 0)
			{
				bFoundRoot = true;
				ASSERT_THAT(AreEqual(
					asTYPEID_INT32,
					TypeId,
					*Case.Describe(TEXT("dispatch field should retain int metadata"))));
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundRoot,
			*Case.Describe(TEXT("dispatch root should publish its field"))));

		bool bFoundInherited = false;
		for (asUINT Index = 0;
			Index < Primary.GetPropertyCount();
			++Index)
		{
			const char* Name = nullptr;
			if (Primary.GetProperty(Index, &Name) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(
					Name,
					"DispatchField") == 0)
			{
				bFoundInherited =
					Primary.IsPropertyInherited(Index);
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundInherited,
			*Case.Describe(TEXT("dispatch primary should expose the inherited root field"))));
	}

	void VerifyVirtualSlot(
		const FNativeCaseContext& Case,
		const FMemberCase& Member,
		asIScriptFunction& RootTarget,
		asIScriptFunction& PrimaryTarget,
		asITypeInfo& Primary)
	{
		asCScriptFunction* const ConcreteRoot =
			static_cast<asCScriptFunction*>(&RootTarget);
		asCObjectType* const ConcretePrimary =
			static_cast<asCObjectType*>(&Primary);
		ASSERT_THAT(IsTrue(
			ConcreteRoot->vfTableIdx >= 0,
			*Case.Describe(TEXT("dispatch root method should own a virtual slot"))));
		if (ConcreteRoot->vfTableIdx < 0)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			static_cast<asUINT>(
				ConcreteRoot->vfTableIdx)
				< ConcretePrimary->virtualFunctionTable.GetLength(),
			*Case.Describe(TEXT("dispatch primary virtual table should contain the root slot"))));
		if (static_cast<asUINT>(ConcreteRoot->vfTableIdx)
			>= ConcretePrimary->virtualFunctionTable.GetLength())
		{
			return;
		}
		asCScriptFunction* const SlotTarget =
			ConcretePrimary->virtualFunctionTable[
				ConcreteRoot->vfTableIdx];
		ASSERT_THAT(AreEqual(
			Member.Member == EDispatchMember::StableMethod
				? &RootTarget
				: &PrimaryTarget,
			static_cast<asIScriptFunction*>(SlotTarget),
			*Case.Describe(TEXT("dispatch virtual slot should name the exact final implementation"))));
	}

	void VerifyMetadataAndSelection(
		const FNativeCaseContext& Case,
		const FDepthCase& Depth,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation,
		asIScriptModule& Module)
	{
		asITypeInfo* const Root =
			Module.GetTypeInfoByName("FDispatchRoot");
		asITypeInfo* const Primary =
			Module.GetTypeInfoByName(
				"FDispatchPrimary");
		ASSERT_THAT(IsNotNull(Root,
			*Case.Describe(TEXT("dispatch source should publish its root type"))));
		ASSERT_THAT(IsNotNull(Primary,
			*Case.Describe(TEXT("dispatch source should publish its primary type"))));
		if (Root == nullptr || Primary == nullptr)
		{
			return;
		}
		VerifyChain(
			Case,
			Depth,
			Module,
			*Root,
			*Primary);
		if (Member.Member == EDispatchMember::Field)
		{
			VerifyFieldMetadata(
				Case,
				*Root,
				*Primary);
		}

		asIScriptFunction* RootTarget = nullptr;
		asIScriptFunction* PrimaryTarget = nullptr;
		if (Member.Member != EDispatchMember::Field)
		{
			RootTarget = FindTargetMethod(*Root, Member);
			PrimaryTarget = FindTargetMethod(*Primary, Member);
		}
		if (Member.Member != EDispatchMember::Field)
		{
			ASSERT_THAT(IsNotNull(RootTarget,
				*Case.Describe(TEXT("dispatch root should publish its selected method"))));
			ASSERT_THAT(IsNotNull(PrimaryTarget,
				*Case.Describe(TEXT("dispatch primary should publish its selected method"))));
			if (RootTarget == nullptr || PrimaryTarget == nullptr)
			{
				return;
			}
		}
		if (Member.Member != EDispatchMember::Field)
		{
			ASSERT_THAT(AreEqual(
				Member.Member == EDispatchMember::StableMethod,
				RootTarget == PrimaryTarget,
				*Case.Describe(TEXT("only the stable final method should retain root identity"))));
			if (Member.Member == EDispatchMember::StableMethod)
			{
				ASSERT_THAT(IsTrue(
					RootTarget->IsFinal(),
					*Case.Describe(TEXT("stable inherited method should be explicitly final"))));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					Member.Member != EDispatchMember::VirtualMethod,
					PrimaryTarget->IsOverride(),
					*Case.Describe(TEXT("dispatch override metadata should match explicit spelling"))));
			}
			VerifyVirtualSlot(
				Case,
				Member,
				*RootTarget,
				*PrimaryTarget,
				*Primary);
		}

		asIScriptFunction* const Witness =
			ProbeWitness(
				Module,
				*Root,
				*Primary,
				View,
				Invocation);
		ASSERT_THAT(IsNotNull(Witness,
			*Case.Describe(TEXT("dispatch view should publish its exact witness"))));
		ASSERT_THAT(IsTrue(HasBytecode(Witness),
			*Case.Describe(TEXT("dispatch witness should emit bytecode"))));
		const bool bDirectField =
			Member.Member == EDispatchMember::Field
			&& !UsesExplicitBase(View, Invocation)
			&& Invocation.Route
				!= EInvocationRoute::VirtualRoute;
		if (Witness != nullptr
			&& Member.Member != EDispatchMember::Field
			&& !bDirectField)
		{
			asIScriptFunction* const ExpectedTarget =
				ExpectedCompiledTarget(
					*RootTarget,
					*PrimaryTarget,
					Member,
					View,
					Invocation);
			ASSERT_THAT(IsTrue(
				BytecodeCallsFunction(
					*Witness,
					ExpectedTarget->GetId(),
					UsesExplicitBase(
						View,
						Invocation)),
				*Case.Describe(TEXT("dispatch witness should encode the exact static call target"))));
		}
	}

	void VerifyRawRuntimeBoundary(
		const FNativeCaseContext& Case,
		const FViewCase& View,
		const FInvocationCase& Invocation,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		const FTCHARToUTF8 DeclarationUtf8(
			*GlobalProbeDeclaration(
				View,
				Invocation));
		asIScriptFunction* const Probe =
			FindGlobalProbe(Module, View, Invocation);
		ASSERT_THAT(IsNotNull(Probe,
			*Case.Describe(TEXT("dispatch source should publish its global probe"))));
		if (Probe == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			1u,
			Probe->GetParamCount(),
			*Case.Describe(TEXT("dispatch probe should accept one static object view"))));
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("dispatch probe should create a context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(Context->Prepare(Probe) >= 0,
			*Case.Describe(TEXT("dispatch probe should prepare"))));
		ASSERT_THAT(IsTrue(
			Context->SetArgObject(0, nullptr) >= 0,
			*Case.Describe(TEXT("dispatch probe should accept an explicit null view"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			Context->Execute(),
			*Case.Describe(TEXT("dispatch probe should retain the isolated raw-object boundary"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Null pointer access")),
			FString(UTF8_TO_TCHAR(
				Context->GetExceptionString())),
			*Case.Describe(TEXT("dispatch probe should own the exact raw exception"))));
		ASSERT_THAT(IsTrue(
			Context->GetExceptionLineNumber() > 0,
			*Case.Describe(TEXT("dispatch probe should retain a source line"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("dispatch probe should unprepare cleanly"))));
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
			BuildInheritanceDispatchRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("inheritance dispatch should allow same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("dispatch recovery should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Case.Describe(TEXT("dispatch recovery should emit no errors"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl(
					"int RunInheritanceDispatchRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("dispatch recovery should publish its exact entry"))));
			asIScriptContext* const Context =
				ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("dispatch recovery should create a context"))));
			if (Recovery != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, Recovery),
					*Case.Describe(TEXT("dispatch recovery should finish"))));
				ASSERT_THAT(AreEqual(
					307,
					static_cast<int32>(
						Context->GetReturnDWord()),
					*Case.Describe(TEXT("dispatch recovery should return its sentinel"))));
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Case.Describe(TEXT("dispatch recovery should unprepare cleanly"))));
				Context->Release();
			}
			else if (Context != nullptr)
			{
				Context->Release();
			}
		}
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("dispatch recovery module should discard cleanly"))));
	}

	void RunCell(
		const FDepthCase& Depth,
		const FMemberCase& Member,
		const FViewCase& View,
		const FInvocationCase& Invocation)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-INH-DISPATCH",
			{
				ANSI_TO_TCHAR(Depth.CatalogName),
				ANSI_TO_TCHAR(Invocation.CatalogName),
				ANSI_TO_TCHAR(Member.CatalogName),
				ANSI_TO_TCHAR(View.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("inheritance dispatch should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FString ModuleName = FString::Printf(
			TEXT("InheritanceDispatch_%hs_%hs_%hs_%hs"),
			Depth.CatalogName,
			Member.CatalogName,
			View.CatalogName,
			Invocation.CatalogName);
		const FString Source =
			BuildInheritanceDispatchSource(
				Depth,
				Member,
				View,
				Invocation);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module) >= 0,
			*Case.Describe(TEXT("inheritance dispatch source should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("inheritance dispatch source should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Case.Describe(TEXT("inheritance dispatch source should emit no errors"))));
		if (Module != nullptr)
		{
			VerifyMetadataAndSelection(
				Case,
				Depth,
				Member,
				View,
				Invocation,
				*Module);
			VerifyRawRuntimeBoundary(
				Case,
				View,
				Invocation,
				*ScriptEngine,
				*Module);
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("inheritance dispatch module should discard cleanly"))));
		CompileAndExecuteRecovery(
			Case,
			Engine,
			*ScriptEngine,
			ModuleName);
	}

public:
	TEST_METHOD(DepthsByMemberViewAndDispatch)
	{
		AS_NATIVE_PRODUCT("LANG-INH-DISPATCH",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Bytecode
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FDepthCase& Depth : DepthCases)
		{
			for (const FMemberCase& Member : MemberCases)
			{
				for (const FViewCase& View : ViewCases)
				{
					for (const FInvocationCase& Invocation
						: InvocationCases)
					{
						RunCell(
							Depth,
							Member,
							View,
							Invocation);
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
