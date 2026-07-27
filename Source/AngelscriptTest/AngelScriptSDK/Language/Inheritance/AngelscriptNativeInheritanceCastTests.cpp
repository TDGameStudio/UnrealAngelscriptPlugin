#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FInheritanceCastTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Inheritance.Cast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static constexpr asPWORD CastStateUserDataSlot =
		static_cast<asPWORD>(0x494E484341535453ull);

	enum class ENativeCastKind : uint8
	{
		Root = 1,
		Derived = 2,
		Sibling = 3,
	};

	enum class ECastRelation : uint8
	{
		Exact,
		Upcast,
		DowncastSuccess,
		DowncastFailure,
		Sibling,
		Null,
	};

	enum class ECastUse : uint8
	{
		Assign,
		Argument,
		Return,
		IdentityCompare,
		MemberCall,
	};

	struct FConstnessCase
	{
		const ANSICHAR* CatalogName;
		bool bConst;
	};

	struct FRelationCase
	{
		const ANSICHAR* CatalogName;
		ECastRelation Relation;
	};

	struct FUseCase
	{
		const ANSICHAR* CatalogName;
		ECastUse Use;
	};

	inline static constexpr FConstnessCase ConstnessCases[] =
	{
		{ "mutable", false },
		{ "const", true },
	};

	inline static constexpr FRelationCase RelationCases[] =
	{
		{ "exact", ECastRelation::Exact },
		{ "upcast", ECastRelation::Upcast },
		{ "downcast_success", ECastRelation::DowncastSuccess },
		{ "downcast_failure", ECastRelation::DowncastFailure },
		{ "sibling", ECastRelation::Sibling },
		{ "null", ECastRelation::Null },
	};

	inline static constexpr FUseCase UseCases[] =
	{
		{ "assign", ECastUse::Assign },
		{ "argument", ECastUse::Argument },
		{ "return", ECastUse::Return },
		{ "identity_compare", ECastUse::IdentityCompare },
		{ "member_call", ECastUse::MemberCall },
	};

	struct FNativeCastState
	{
		int32 Created = 0;
		int32 Destroyed = 0;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		int32 MutableCastCalls = 0;
		int32 ConstCastCalls = 0;
		int32 MemberCalls = 0;
		int32 LiveObjects = 0;
		int32 NextObjectId = 1;
		TArray<int32> CreatedIds;
		TArray<int32> DestroyedIds;

		void Reset()
		{
			Created = 0;
			Destroyed = 0;
			AddRefCalls = 0;
			ReleaseCalls = 0;
			MutableCastCalls = 0;
			ConstCastCalls = 0;
			MemberCalls = 0;
			LiveObjects = 0;
			NextObjectId = 1;
			CreatedIds.Reset();
			DestroyedIds.Reset();
		}
	};

	class FNativeCastRoot
	{
	public:
		FNativeCastRoot(
			FNativeCastState& InState,
			const ENativeCastKind InKind)
			: State(InState)
			, Kind(InKind)
			, ObjectId(State.NextObjectId++)
		{
			++State.Created;
			++State.LiveObjects;
			State.CreatedIds.Add(ObjectId);
		}

		virtual ~FNativeCastRoot()
		{
			++State.Destroyed;
			--State.LiveObjects;
			State.DestroyedIds.Add(ObjectId);
		}

		void AddRef()
		{
			++ReferenceCount;
			++State.AddRefCalls;
		}

		void Release()
		{
			++State.ReleaseCalls;
			--ReferenceCount;
			if (ReferenceCount == 0)
			{
				delete this;
			}
		}

		int32 GetKind() const
		{
			++State.MemberCalls;
			return static_cast<int32>(Kind);
		}

		int32 GetIdentity() const
		{
			++State.MemberCalls;
			return ObjectId;
		}

		FNativeCastState& GetState() const
		{
			return State;
		}

		ENativeCastKind GetNativeKind() const
		{
			return Kind;
		}

	private:
		FNativeCastState& State;
		ENativeCastKind Kind;
		int32 ObjectId = INDEX_NONE;
		int32 ReferenceCount = 1;
	};

	class FNativeCastDerived final : public FNativeCastRoot
	{
	public:
		explicit FNativeCastDerived(FNativeCastState& State)
			: FNativeCastRoot(
				State,
				ENativeCastKind::Derived)
		{
		}
	};

	class FNativeCastSibling final : public FNativeCastRoot
	{
	public:
		explicit FNativeCastSibling(FNativeCastState& State)
			: FNativeCastRoot(
				State,
				ENativeCastKind::Sibling)
		{
		}
	};

	static FNativeCastState* GetCastState(
		asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
			? static_cast<FNativeCastState*>(
				Generic.GetEngine()->GetUserData(
					CastStateUserDataSlot))
			: nullptr;
	}

	static void GenericAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FNativeCastRoot* const Object =
				static_cast<FNativeCastRoot*>(
					Generic->GetObject()))
			{
				Object->AddRef();
			}
		}
	}

	static void GenericRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FNativeCastRoot* const Object =
				static_cast<FNativeCastRoot*>(
					Generic->GetObject()))
			{
				Object->Release();
			}
		}
	}

	static void GenericGetKind(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FNativeCastRoot* const Object =
			static_cast<const FNativeCastRoot*>(
				Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr
				? static_cast<asDWORD>(Object->GetKind())
				: 0);
	}

	static void GenericGetIdentity(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FNativeCastRoot* const Object =
			static_cast<const FNativeCastRoot*>(
				Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr
				? static_cast<asDWORD>(
					Object->GetIdentity())
				: 0);
	}

	static void ReturnNewObject(
		asIScriptGeneric& Generic,
		FNativeCastRoot* Object)
	{
		Generic.SetReturnAddress(Object);
	}

	static void GenericMakeRoot(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FNativeCastState* const State =
			GetCastState(*Generic);
		ReturnNewObject(
			*Generic,
			State != nullptr
				? new FNativeCastRoot(
					*State,
					ENativeCastKind::Root)
				: nullptr);
	}

	static void GenericMakeDerived(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FNativeCastState* const State =
			GetCastState(*Generic);
		ReturnNewObject(
			*Generic,
			State != nullptr
				? new FNativeCastDerived(*State)
				: nullptr);
	}

	static void GenericMakeDerivedAsRoot(
		asIScriptGeneric* Generic)
	{
		GenericMakeDerived(Generic);
	}

	static void GenericMakeSiblingAsRoot(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FNativeCastState* const State =
			GetCastState(*Generic);
		ReturnNewObject(
			*Generic,
			State != nullptr
				? new FNativeCastSibling(*State)
				: nullptr);
	}

	static FNativeCastRoot* AddReferenceForCast(
		FNativeCastRoot* Object,
		const bool bConst)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
			if (bConst)
			{
				++Object->GetState().ConstCastCalls;
			}
			else
			{
				++Object->GetState().MutableCastCalls;
			}
		}
		return Object;
	}

	static FNativeCastDerived* DowncastToDerived(
		FNativeCastRoot* Object,
		const bool bConst)
	{
		if (Object == nullptr)
		{
			return nullptr;
		}
		if (bConst)
		{
			++Object->GetState().ConstCastCalls;
		}
		else
		{
			++Object->GetState().MutableCastCalls;
		}
		FNativeCastDerived* const Derived =
			Object->GetNativeKind() == ENativeCastKind::Derived
				? static_cast<FNativeCastDerived*>(Object)
				: nullptr;
		if (Derived != nullptr)
		{
			Derived->AddRef();
		}
		return Derived;
	}

	static void GenericDerivedToRoot(
		asIScriptGeneric* Generic,
		const bool bConst)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FNativeCastRoot* const Object =
			static_cast<FNativeCastRoot*>(
				Generic->GetObject());
		Generic->SetReturnAddress(
			AddReferenceForCast(Object, bConst));
	}

	static void GenericDerivedToRootMutable(
		asIScriptGeneric* Generic)
	{
		GenericDerivedToRoot(Generic, false);
	}

	static void GenericDerivedToRootConst(
		asIScriptGeneric* Generic)
	{
		GenericDerivedToRoot(Generic, true);
	}

	static void GenericSiblingToRootMutable(
		asIScriptGeneric* Generic)
	{
		GenericDerivedToRoot(Generic, false);
	}

	static void GenericSiblingToRootConst(
		asIScriptGeneric* Generic)
	{
		GenericDerivedToRoot(Generic, true);
	}

	static void GenericRootToDerived(
		asIScriptGeneric* Generic,
		const bool bConst)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FNativeCastRoot* const Object =
			static_cast<FNativeCastRoot*>(
				Generic->GetObject());
		Generic->SetReturnAddress(
			DowncastToDerived(Object, bConst));
	}

	static void GenericRootToDerivedMutable(
		asIScriptGeneric* Generic)
	{
		GenericRootToDerived(Generic, false);
	}

	static void GenericRootToDerivedConst(
		asIScriptGeneric* Generic)
	{
		GenericRootToDerived(Generic, true);
	}

	static bool RegisterReferenceType(
		asIScriptEngine& Engine,
		const ANSICHAR* TypeName)
	{
		return Engine.RegisterObjectType(
			TypeName,
			0,
			asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
			&& Engine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(GenericAddRef),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(GenericRelease),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"int GetKind() const",
				asFUNCTION(GenericGetKind),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				TypeName,
				"int GetIdentity() const",
				asFUNCTION(GenericGetIdentity),
				asCALL_GENERIC) >= 0;
	}

	static bool RegisterCastMethods(asIScriptEngine& Engine)
	{
		return Engine.RegisterObjectMethod(
			"FCastDerived",
			"FCastRoot opImplCast()",
			asFUNCTION(GenericDerivedToRootMutable),
			asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FCastDerived",
				"const FCastRoot opImplCast() const",
				asFUNCTION(GenericDerivedToRootConst),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FCastSibling",
				"FCastRoot opImplCast()",
				asFUNCTION(GenericSiblingToRootMutable),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FCastSibling",
				"const FCastRoot opImplCast() const",
				asFUNCTION(GenericSiblingToRootConst),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FCastRoot",
				"FCastDerived opCast()",
				asFUNCTION(GenericRootToDerivedMutable),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterObjectMethod(
				"FCastRoot",
				"const FCastDerived opCast() const",
				asFUNCTION(GenericRootToDerivedConst),
				asCALL_GENERIC) >= 0;
	}

	static bool RegisterFactories(asIScriptEngine& Engine)
	{
		return Engine.RegisterGlobalFunction(
			"FCastRoot MakeCastRoot()",
			asFUNCTION(GenericMakeRoot),
			asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FCastDerived MakeCastDerived()",
				asFUNCTION(GenericMakeDerived),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FCastRoot MakeCastDerivedAsRoot()",
				asFUNCTION(GenericMakeDerivedAsRoot),
				asCALL_GENERIC) >= 0
			&& Engine.RegisterGlobalFunction(
				"FCastRoot MakeCastSiblingAsRoot()",
				asFUNCTION(GenericMakeSiblingAsRoot),
				asCALL_GENERIC) >= 0;
	}

	static bool RegisterCastFixtures(
		asIScriptEngine& Engine,
		FNativeCastState& State)
	{
		Engine.SetUserData(
			&State,
			CastStateUserDataSlot);
		return RegisterReferenceType(
			Engine,
			"FCastRoot")
			&& RegisterReferenceType(
				Engine,
				"FCastDerived")
			&& RegisterReferenceType(
				Engine,
				"FCastSibling")
			&& RegisterCastMethods(Engine)
			&& RegisterFactories(Engine);
	}

	static bool IsSuccessfulRelation(
		const FRelationCase& Relation)
	{
		return Relation.Relation == ECastRelation::Exact
			|| Relation.Relation == ECastRelation::Upcast
			|| Relation.Relation == ECastRelation::DowncastSuccess;
	}

	static FString SourceType(
		const FRelationCase& Relation)
	{
		if (Relation.Relation == ECastRelation::Exact
			|| Relation.Relation == ECastRelation::Upcast)
		{
			return TEXT("FCastDerived");
		}
		return TEXT("FCastRoot");
	}

	static FString TargetType(
		const FRelationCase& Relation)
	{
		return Relation.Relation == ECastRelation::Upcast
			? TEXT("FCastRoot")
			: TEXT("FCastDerived");
	}

	static FString SourceExpression(
		const FRelationCase& Relation)
	{
		switch (Relation.Relation)
		{
		case ECastRelation::Exact:
		case ECastRelation::Upcast:
			return TEXT("MakeCastDerived()");
		case ECastRelation::DowncastSuccess:
			return TEXT("MakeCastDerivedAsRoot()");
		case ECastRelation::DowncastFailure:
			return TEXT("MakeCastRoot()");
		case ECastRelation::Sibling:
			return TEXT("MakeCastSiblingAsRoot()");
		case ECastRelation::Null:
		default:
			return TEXT("nullptr");
		}
	}

	static FString CastExpression(
		const FRelationCase& Relation,
		const TCHAR* VariableName)
	{
		return VariableName;
	}

	static FString QualifiedType(
		const FString& Type,
		const FConstnessCase& Constness)
	{
		return Constness.bConst
			? TEXT("const ") + Type
			: Type;
	}

	static void AppendObservationHelper(
		FString& Source,
		const FString& Target,
		const FConstnessCase& Constness)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("int ObserveCast(%s Value)"),
				*QualifiedType(Target, Constness)));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn Value == nullptr ? -1 : Value.GetKind();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReturnHelper(
		FString& Source,
		const FString& SourceTypeName,
		const FString& Target,
		const FConstnessCase& Constness,
		const FRelationCase& Relation)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("%s ReturnCast(%s Value)"),
				*QualifiedType(Target, Constness),
				*QualifiedType(
					SourceTypeName,
					Constness)));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\treturn ")
				+ CastExpression(
					Relation,
					TEXT("Value"))
				+ TEXT(";"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendRunFunction(
		FString& Source,
		const FConstnessCase& Constness,
		const FRelationCase& Relation,
		const FUseCase& Use)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString SourceTypeName =
			SourceType(Relation);
		const FString Target =
			TargetType(Relation);
		const FString Cast =
			CastExpression(Relation, TEXT("Source"));
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunInheritanceCast()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(
				TEXT("\t%s Source = %s;"),
				*QualifiedType(
					SourceTypeName,
					Constness),
				*SourceExpression(Relation)));

		switch (Use.Use)
		{
		case ECastUse::Assign:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%s Result = %s;"),
					*QualifiedType(Target, Constness),
					*Cast));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Result == nullptr ? -1 : Result.GetKind();"));
			break;
		case ECastUse::Argument:
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn ObserveCast(")
					+ Cast + TEXT(");"));
			break;
		case ECastUse::Return:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%s Result = ReturnCast(Source);"),
					*QualifiedType(Target, Constness)));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Result == nullptr ? -1 : Result.GetKind();"));
			break;
		case ECastUse::IdentityCompare:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%s Result = %s;"),
					*QualifiedType(Target, Constness),
					*Cast));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tif (Result == nullptr)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\treturn Source == nullptr ? 1 : 0;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Result.GetIdentity() == Source.GetIdentity() ? 1 : 0;"));
			break;
		case ECastUse::MemberCall:
			AppendGeneratedAsLine(
				Source,
				FString::Printf(
					TEXT("\t%s Result = %s;"),
					*QualifiedType(Target, Constness),
					*Cast));
			AppendGeneratedAsLine(
				Source,
				TEXT("\treturn Result == nullptr ? -1 : Result.GetKind();"));
			break;
		default:
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildInheritanceCastSource(
		const FConstnessCase& Constness,
		const FRelationCase& Relation,
		const FUseCase& Use)
	{
		FString Source;
		const FString SourceTypeName =
			SourceType(Relation);
		const FString Target =
			TargetType(Relation);
		AppendObservationHelper(
			Source,
			Target,
			Constness);
		AppendReturnHelper(
			Source,
			SourceTypeName,
			Target,
			Constness,
			Relation);
		AppendRunFunction(
			Source,
			Constness,
			Relation,
			Use);
		return Source;
	}

	static FString BuildInheritanceCastRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			TEXT("int RunInheritanceCastRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 251;"));
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

	static int32 ExpectedRuntimeValue(
		const FRelationCase& Relation,
		const FUseCase& Use)
	{
		if (Use.Use == ECastUse::IdentityCompare)
		{
			if (IsSuccessfulRelation(Relation)
				|| Relation.Relation == ECastRelation::Null)
			{
				return 1;
			}
			return 0;
		}
		return IsSuccessfulRelation(Relation) ? 2 : -1;
	}

	static bool RelationInvokesCast(
		const FRelationCase& Relation)
	{
		return Relation.Relation != ECastRelation::Exact
			&& Relation.Relation != ECastRelation::Null;
	}

	static bool ShouldExecuteRuntime(
		const FRelationCase& Relation)
	{
		return Relation.Relation == ECastRelation::Exact
			|| Relation.Relation == ECastRelation::Null;
	}

	void VerifyTypeMetadata(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine)
	{
		asITypeInfo* const Root =
			ScriptEngine.GetTypeInfoByDecl("FCastRoot");
		asITypeInfo* const Derived =
			ScriptEngine.GetTypeInfoByDecl("FCastDerived");
		asITypeInfo* const Sibling =
			ScriptEngine.GetTypeInfoByDecl("FCastSibling");
		ASSERT_THAT(IsNotNull(Root,
			*Case.Describe(TEXT("cast fixtures should publish the root type"))));
		ASSERT_THAT(IsNotNull(Derived,
			*Case.Describe(TEXT("cast fixtures should publish the derived type"))));
		ASSERT_THAT(IsNotNull(Sibling,
			*Case.Describe(TEXT("cast fixtures should publish the sibling type"))));
		if (Root == nullptr || Derived == nullptr || Sibling == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			(Root->GetFlags() & asOBJ_REF) != 0
				&& (Derived->GetFlags() & asOBJ_REF) != 0
				&& (Sibling->GetFlags() & asOBJ_REF) != 0,
			*Case.Describe(TEXT("cast fixtures should retain reference semantics"))));
		ASSERT_THAT(IsNotNull(
			Root->GetMethodByDecl("FCastDerived opCast()"),
			*Case.Describe(TEXT("root should publish mutable downcast metadata"))));
		ASSERT_THAT(IsNotNull(
			Root->GetMethodByDecl(
				"const FCastDerived opCast() const"),
			*Case.Describe(TEXT("root should publish const downcast metadata"))));
		ASSERT_THAT(IsNotNull(
			Derived->GetMethodByDecl(
				"FCastRoot opImplCast()"),
			*Case.Describe(TEXT("derived should publish mutable upcast metadata"))));
		ASSERT_THAT(IsNotNull(
			Derived->GetMethodByDecl(
				"const FCastRoot opImplCast() const"),
			*Case.Describe(TEXT("derived should publish const upcast metadata"))));
		ASSERT_THAT(IsNotNull(
			Sibling->GetMethodByDecl(
				"FCastRoot opImplCast()"),
			*Case.Describe(TEXT("sibling should publish its root-view cast metadata"))));
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FConstnessCase& Constness,
		const FRelationCase& Relation,
		const FNativeCastState& State)
	{
		const bool bHasObject =
			Relation.Relation != ECastRelation::Null;
		ASSERT_THAT(AreEqual(
			bHasObject ? 1 : 0,
			State.Created,
			*Case.Describe(TEXT("cast source should create exactly its requested runtime object"))));
		ASSERT_THAT(AreEqual(
			State.Created,
			State.Destroyed,
			*Case.Describe(TEXT("cast source should destroy every created runtime object"))));
		ASSERT_THAT(AreEqual(
			0,
			State.LiveObjects,
			*Case.Describe(TEXT("cast source should leave no live runtime object"))));
		ASSERT_THAT(AreEqual(
			State.CreatedIds,
			State.DestroyedIds,
			*Case.Describe(TEXT("cast source should destroy the exact created identity"))));
		ASSERT_THAT(AreEqual(
			State.AddRefCalls + State.Created,
			State.ReleaseCalls,
			*Case.Describe(TEXT("cast source should balance initial and added references"))));

		const int32 ExpectedCastCalls =
			RelationInvokesCast(Relation) ? 1 : 0;
		ASSERT_THAT(AreEqual(
			Constness.bConst ? 0 : ExpectedCastCalls,
			State.MutableCastCalls,
			*Case.Describe(TEXT("mutable cast callback count should match source constness"))));
		ASSERT_THAT(AreEqual(
			Constness.bConst ? ExpectedCastCalls : 0,
			State.ConstCastCalls,
			*Case.Describe(TEXT("const cast callback count should match source constness"))));
	}

	void ExecuteCell(
		const FNativeCaseContext& Case,
		const FConstnessCase& Constness,
		const FRelationCase& Relation,
		const FUseCase& Use,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeCastState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl(
				"int RunInheritanceCast()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("cast source should publish its exact entry"))));
		if (Entry == nullptr)
		{
			return;
		}
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("cast source should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("native registered inheritance cast should finish"))));
		ASSERT_THAT(AreEqual(
			ExpectedRuntimeValue(Relation, Use),
			static_cast<int32>(
				Context->GetReturnDWord()),
			*Case.Describe(TEXT("cast result should match relation and use-site semantics"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("cast context should release all local handles"))));
		Context->Release();
		VerifyLifecycle(
			Case,
			Constness,
			Relation,
			State);
	}

	void CompileAndExecuteRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName,
		FNativeCastState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource =
			BuildInheritanceCastRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileAndReport(
			*TestRunner,
			ScriptEngine,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource,
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("inheritance cast should allow same-name recovery"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("inheritance cast recovery should publish its module"))));
		ASSERT_THAT(IsFalse(HasAnyError(Engine),
			*Case.Describe(TEXT("inheritance cast recovery should emit no errors"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery =
				RecoveryModule->GetFunctionByDecl(
					"int RunInheritanceCastRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("cast recovery should publish its exact entry"))));
			asIScriptContext* const Context =
				ScriptEngine.CreateContext();
			ASSERT_THAT(IsNotNull(Context,
				*Case.Describe(TEXT("cast recovery should create a context"))));
			if (Recovery != nullptr && Context != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asEXECUTION_FINISHED),
					PrepareAndExecute(Context, Recovery),
					*Case.Describe(TEXT("cast recovery should finish"))));
				ASSERT_THAT(AreEqual(
					251,
					static_cast<int32>(
						Context->GetReturnDWord()),
					*Case.Describe(TEXT("cast recovery should return its sentinel"))));
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					Context->Unprepare(),
					*Case.Describe(TEXT("cast recovery should unprepare cleanly"))));
				Context->Release();
			}
			else if (Context != nullptr)
			{
				Context->Release();
			}
		}
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*Case.Describe(TEXT("cast recovery should create no native fixture object"))));

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine.GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("cast recovery module should discard cleanly"))));
	}

	void RunCell(
		const FConstnessCase& Constness,
		const FRelationCase& Relation,
		const FUseCase& Use)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-INH-CAST",
			{
				ANSI_TO_TCHAR(Constness.CatalogName),
				ANSI_TO_TCHAR(Relation.CatalogName),
				ANSI_TO_TCHAR(Use.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("inheritance cast should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeCastState State;
		ASSERT_THAT(IsTrue(RegisterCastFixtures(
			*ScriptEngine,
			State),
			*Case.Describe(TEXT("inheritance cast should register its native type family"))));
		VerifyTypeMetadata(Case, *ScriptEngine);
		const FString ModuleName = FString::Printf(
			TEXT("InheritanceCast_%hs_%hs_%hs"),
			Constness.CatalogName,
			Relation.CatalogName,
			Use.CatalogName);
		const FString Source =
			BuildInheritanceCastSource(
				Constness,
				Relation,
				Use);
		Engine.ResetMessages();
		State.Reset();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		const bool bExpectCompile =
			Relation.Relation == ECastRelation::Exact
			|| Relation.Relation == ECastRelation::Upcast;
		if (bExpectCompile)
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.Describe(TEXT("inheritance cast source should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("inheritance cast source should publish its module"))));
			ASSERT_THAT(IsFalse(HasAnyError(Engine),
				*Case.Describe(TEXT("inheritance cast source should emit no errors"))));
			if (Module != nullptr && ShouldExecuteRuntime(Relation))
			{
				ExecuteCell(
					Case,
					Constness,
					Relation,
					Use,
					*ScriptEngine,
					*Module,
					State);
			}
			else if (Module != nullptr)
			{
				ASSERT_THAT(AreEqual(
					0,
					State.LiveObjects,
					*Case.Describe(TEXT("cast compile-only boundary should execute no native fixture object"))));
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0 || HasAnyError(Engine),
				*Case.Describe(TEXT("current fork should reject explicit native downcast syntax"))));
			ASSERT_THAT(IsTrue(HasAnyError(Engine),
				*Case.Describe(TEXT("native downcast rejection should publish a diagnostic"))));
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("inheritance cast module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*Case.Describe(TEXT("inheritance cast discard should retain no object"))));
		CompileAndExecuteRecovery(
			Case,
			Engine,
			*ScriptEngine,
			ModuleName,
			State);
	}

public:
	TEST_METHOD(RelationsByConstnessAndUse)
	{
		AS_NATIVE_PRODUCT("LANG-INH-CAST",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FConstnessCase& Constness : ConstnessCases)
		{
			for (const FRelationCase& Relation : RelationCases)
			{
				for (const FUseCase& Use : UseCases)
				{
					RunCell(Constness, Relation, Use);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
