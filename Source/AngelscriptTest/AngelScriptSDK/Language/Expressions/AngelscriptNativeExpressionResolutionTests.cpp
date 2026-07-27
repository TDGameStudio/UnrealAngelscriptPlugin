#include "../References/AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExpressionResolutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Resolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FReferenceState = AngelscriptNativeReferenceTestSupport::FReferenceState;

	inline static constexpr asPWORD ResolutionStateUserDataSlot =
		static_cast<asPWORD>(0x4E4154455852534Full);
	inline static constexpr asDWORD PublicAccessMask = 0x1;
	inline static constexpr asDWORD HiddenAccessMask = 0x2;

	enum class EResolutionState : uint8
	{
		Exact,
		NamespaceQualified,
		Overload,
		Conversion,
		Missing,
		Ambiguous,
		Inaccessible,
		WrongType,
	};

	enum class EResolutionContext : uint8
	{
		Initializer,
		Assignment,
		Argument,
		Return,
		Condition,
		Index,
		MemberReceiver,
	};

	enum class EResolutionShape : uint8
	{
		Identifier,
		Call,
		Member,
		ScopedName,
	};

	struct FResolutionStateCase
	{
		const ANSICHAR* CatalogName;
		EResolutionState State;
	};

	struct FResolutionContextCase
	{
		const ANSICHAR* CatalogName;
		EResolutionContext Context;
	};

	struct FResolutionShapeCase
	{
		const ANSICHAR* CatalogName;
		EResolutionShape Shape;
	};

	struct FExpressionResolutionState
	{
		void Reset()
		{
			CallbackCalls = 0;
			LastMarker = INDEX_NONE;
			MarkerTrace.Reset();
		}

		int32 CallbackCalls = 0;
		int32 LastMarker = INDEX_NONE;
		TArray<int32> MarkerTrace;
		int32 HiddenIdentifier = 701;
		int32 HiddenScoped = 704;
	};

	inline static constexpr FResolutionStateCase StateCases[] = {
		{"exact", EResolutionState::Exact},
		{"namespace_qualified", EResolutionState::NamespaceQualified},
		{"overload", EResolutionState::Overload},
		{"conversion", EResolutionState::Conversion},
		{"missing", EResolutionState::Missing},
		{"ambiguous", EResolutionState::Ambiguous},
		{"inaccessible", EResolutionState::Inaccessible},
		{"wrong_type", EResolutionState::WrongType},
	};

	inline static constexpr FResolutionContextCase ContextCases[] = {
		{"initializer", EResolutionContext::Initializer},
		{"assignment", EResolutionContext::Assignment},
		{"argument", EResolutionContext::Argument},
		{"return", EResolutionContext::Return},
		{"condition", EResolutionContext::Condition},
		{"index", EResolutionContext::Index},
		{"member_receiver", EResolutionContext::MemberReceiver},
	};

	inline static constexpr FResolutionShapeCase ShapeCases[] = {
		{"identifier", EResolutionShape::Identifier},
		{"call", EResolutionShape::Call},
		{"member", EResolutionShape::Member},
		{"scoped_name", EResolutionShape::ScopedName},
	};

	static bool IsSuccessfulState(const FResolutionStateCase& StateCase)
	{
		return StateCase.State == EResolutionState::Exact ||
			   StateCase.State == EResolutionState::NamespaceQualified ||
			   StateCase.State == EResolutionState::Overload ||
			   StateCase.State == EResolutionState::Conversion;
	}

	static bool IsAccessMaskEnforced(asIScriptEngine& Engine)
	{
		asIScriptFunction* const HiddenCall = Engine.GetGlobalFunctionByDecl("int HiddenCall()");
		return HiddenCall != nullptr && (HiddenCall->GetAccessMask() & PublicAccessMask) == 0;
	}

	static int32 ShapeOrdinal(const FResolutionShapeCase& ShapeCase)
	{
		switch (ShapeCase.Shape)
		{
		case EResolutionShape::Identifier:
			return 1;
		case EResolutionShape::Call:
			return 2;
		case EResolutionShape::Member:
			return 3;
		case EResolutionShape::ScopedName:
			return 4;
		default:
			return 0;
		}
	}

	static int32 ExpectedMarker(
		const FResolutionStateCase& StateCase, const FResolutionShapeCase& ShapeCase)
	{
		int32 Base = 0;
		switch (StateCase.State)
		{
		case EResolutionState::Exact:
			Base = 100;
			break;
		case EResolutionState::NamespaceQualified:
			Base = 200;
			break;
		case EResolutionState::Overload:
			Base = 300;
			break;
		case EResolutionState::Conversion:
			Base = 400;
			break;
		default:
			return INDEX_NONE;
		}
		return Base + ShapeOrdinal(ShapeCase);
	}

	static FExpressionResolutionState* GetResolutionState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FExpressionResolutionState*>(
						 Generic.GetEngine()->GetUserData(ResolutionStateUserDataSlot))
				   : nullptr;
	}

	static void GenericRecordResolution(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const int32 Marker = static_cast<int32>(Generic->GetArgDWord(0));
		if (FExpressionResolutionState* const State = GetResolutionState(*Generic))
		{
			++State->CallbackCalls;
			State->LastMarker = Marker;
			State->MarkerTrace.Add(Marker);
		}
		Generic->SetReturnDWord(static_cast<asDWORD>(Marker));
	}

	static void GenericReturnZero(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(0);
		}
	}

	static bool RegisterAmbiguousAccessors(asIScriptEngine& Engine)
	{
		if (Engine.RegisterGlobalFunction("int AmbiguousIdentifier()",
				asFUNCTION(GenericReturnZero),
				asCALL_GENERIC) < 0 ||
			Engine.RegisterGlobalFunction("int AmbiguousIdentifier(int Choice = 0)",
				asFUNCTION(GenericReturnZero),
				asCALL_GENERIC) < 0 ||
			Engine.RegisterObjectMethod("FNativeCaseValue",
				"int AmbiguousMember() const",
				asFUNCTION(GenericReturnZero),
				asCALL_GENERIC) < 0 ||
			Engine.RegisterObjectMethod("FNativeCaseValue",
				"int AmbiguousMember(int Choice = 0) const",
				asFUNCTION(GenericReturnZero),
				asCALL_GENERIC) < 0)
		{
			return false;
		}

		if (Engine.SetDefaultNamespace("ResolutionAmbiguous") < 0)
		{
			return false;
		}
		const bool bRegistered =
			Engine.RegisterGlobalFunction("int AmbiguousScoped()",
				asFUNCTION(GenericReturnZero),
				asCALL_GENERIC) >= 0 &&
			Engine.RegisterGlobalFunction("int AmbiguousScoped(int Choice = 0)",
				asFUNCTION(GenericReturnZero),
				asCALL_GENERIC) >= 0;
		return Engine.SetDefaultNamespace("") >= 0 && bRegistered;
	}

	static bool RegisterHiddenSymbols(asIScriptEngine& Engine, FExpressionResolutionState& State)
	{
		const asDWORD PreviousMask = Engine.SetDefaultAccessMask(HiddenAccessMask);
		bool bRegistered =
			Engine.RegisterGlobalProperty("int HiddenIdentifier", &State.HiddenIdentifier) >= 0 &&
			Engine.RegisterGlobalFunction(
				"int HiddenCall()", asFUNCTION(GenericReturnZero), asCALL_GENERIC) >= 0 &&
			Engine.RegisterObjectProperty("FNativeCaseValue",
				"int HiddenMember",
				asOFFSET(AngelscriptNativeTestSupport::FNativeTrackedValue, Value)) >= 0;
		if (bRegistered)
		{
			bRegistered =
				Engine.SetDefaultNamespace("ResolutionHidden") >= 0 &&
				Engine.RegisterGlobalProperty("int HiddenScoped", &State.HiddenScoped) >= 0;
			const bool bRestoredNamespace = Engine.SetDefaultNamespace("") >= 0;
			bRegistered = bRegistered && bRestoredNamespace;
		}
		else
		{
			Engine.SetDefaultNamespace("");
		}
		Engine.SetDefaultAccessMask(PreviousMask);
		return bRegistered;
	}

	static bool RegisterResolutionFixtures(asIScriptEngine& Engine,
		FExpressionResolutionState& State,
		FNativeLifecycleRecorder& Lifecycle,
		FReferenceState& ReferenceState)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		Engine.SetUserData(&State, ResolutionStateUserDataSlot);
		return RegisterNativeCaseValue(Engine, Lifecycle) &&
			   RegisterReferenceFixtures(Engine, ReferenceState) &&
			   Engine.RegisterGlobalFunction("int RecordResolution(int Marker)",
				   asFUNCTION(GenericRecordResolution),
				   asCALL_GENERIC) >= 0 &&
			   RegisterAmbiguousAccessors(Engine) && RegisterHiddenSymbols(Engine, State);
	}

	static void AppendWrongValueDeclaration(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FWrongResolutionValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tbool Wrong = true;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendResolutionOwnerDeclaration(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FResolutionOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint ExactMember = 103;"));
		AppendGeneratedAsLine(Source, TEXT("\tFWrongResolutionValue WrongMember;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue ResolveMemberOverload(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(303));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tFNativeCaseValue ResolveMemberOverload(double Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(903));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tFNativeCaseValue ResolveMemberConversion(int64 Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(403));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tFNativeCaseValue ResolveAmbiguousMember(FRefRoot Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tFNativeCaseValue ResolveAmbiguousMember(FRefUnrelated Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendGlobalResolutionFunctions(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ResolveExactCall()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(102));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(
			Source, TEXT("FNativeCaseValue ResolveIdentifierOverload(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(301));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("FNativeCaseValue ResolveIdentifierOverload(double Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(901));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ResolveCallOverload(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(302));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ResolveCallOverload(double Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(902));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(
			Source, TEXT("FNativeCaseValue ResolveIdentifierConversion(int64 Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(401));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ResolveCallConversion(int64 Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(RecordResolution(402));"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(
			Source, TEXT("FNativeCaseValue ResolveAmbiguousCall(FRefRoot Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(0);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("FNativeCaseValue ResolveAmbiguousCall(FRefUnrelated Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FNativeCaseValue(0);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("FWrongResolutionValue ResolveWrongCall()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn FWrongResolutionValue();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendNamespaceResolutionDeclarations(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionExact"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tconst int ExactScoped = 104;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionQualified"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tconst int QualifiedIdentifier = 201;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tstruct FOwner"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint QualifiedMember = 203;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue ResolveCall()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(202));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionOuter"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tnamespace ResolutionInner"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tconst int QualifiedScoped = 204;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionOverload"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Resolve(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(304));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Resolve(double Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(904));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionConversion"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Resolve(int64 Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(RecordResolution(404));"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionExisting"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tconst int ExistingValue = 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionAmbiguous"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Resolve(FRefRoot Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Resolve(FRefUnrelated Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FNativeCaseValue(0);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("namespace ResolutionWrong"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFWrongResolutionValue Resolve()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn FWrongResolutionValue();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendContextHelpers(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Source, TEXT("int ObserveResolution(const FNativeCaseValue& in Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FResolutionIndexProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint opIndex(int Index) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Index;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendCommonResolutionDeclarations(FString& Source)
	{
		AppendWrongValueDeclaration(Source);
		AppendResolutionOwnerDeclaration(Source);
		AppendGlobalResolutionFunctions(Source);
		AppendNamespaceResolutionDeclarations(Source);
		AppendContextHelpers(Source);
	}

	static void AppendResolutionSetup(FString& Source,
		const FResolutionStateCase& StateCase,
		const FResolutionShapeCase& ShapeCase,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (StateCase.State)
		{
		case EResolutionState::Exact:
			if (ShapeCase.Shape == EResolutionShape::Identifier)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sint ExactIdentifier = 101;"), Indent));
			}
			else if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFResolutionOwner ResolutionOwner;"), Indent));
			}
			break;
		case EResolutionState::NamespaceQualified:
			if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%sResolutionQualified::FOwner QualifiedOwner;"), Indent));
			}
			break;
		case EResolutionState::Overload:
			if (ShapeCase.Shape == EResolutionShape::Identifier)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%sFNativeCaseValue OverloadIdentifier = "
										 "ResolveIdentifierOverload(7);"),
						Indent));
			}
			else if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFResolutionOwner ResolutionOwner;"), Indent));
			}
			break;
		case EResolutionState::Conversion:
			if (ShapeCase.Shape == EResolutionShape::Identifier)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%sFNativeCaseValue ConversionIdentifier = "
										 "ResolveIdentifierConversion(int8(7));"),
						Indent));
			}
			else if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFResolutionOwner ResolutionOwner;"), Indent));
			}
			break;
		case EResolutionState::Missing:
			if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFResolutionOwner ResolutionOwner;"), Indent));
			}
			break;
		case EResolutionState::Ambiguous:
			if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFResolutionOwner ResolutionOwner;"), Indent));
			}
			break;
		case EResolutionState::Inaccessible:
			if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFNativeCaseValue HiddenOwner(307);"), Indent));
			}
			break;
		case EResolutionState::WrongType:
			if (ShapeCase.Shape == EResolutionShape::Identifier)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%sFWrongResolutionValue WrongIdentifier;"), Indent));
			}
			else if (ShapeCase.Shape == EResolutionShape::Member)
			{
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("%sFResolutionOwner ResolutionOwner;"), Indent));
			}
			break;
		}
	}

	static FString ResolutionExpression(
		const FResolutionStateCase& StateCase, const FResolutionShapeCase& ShapeCase)
	{
		switch (StateCase.State)
		{
		case EResolutionState::Exact:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("FNativeCaseValue(RecordResolution(ExactIdentifier))");
			case EResolutionShape::Call:
				return TEXT("ResolveExactCall()");
			case EResolutionShape::Member:
				return TEXT("FNativeCaseValue(RecordResolution("
							"ResolutionOwner.ExactMember))");
			case EResolutionShape::ScopedName:
				return TEXT("FNativeCaseValue(RecordResolution("
							"ResolutionExact::ExactScoped))");
			}
			break;
		case EResolutionState::NamespaceQualified:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("FNativeCaseValue(RecordResolution("
							"ResolutionQualified::QualifiedIdentifier))");
			case EResolutionShape::Call:
				return TEXT("ResolutionQualified::ResolveCall()");
			case EResolutionShape::Member:
				return TEXT("FNativeCaseValue(RecordResolution("
							"QualifiedOwner.QualifiedMember))");
			case EResolutionShape::ScopedName:
				return TEXT("FNativeCaseValue(RecordResolution("
							"ResolutionOuter::ResolutionInner::QualifiedScoped))");
			}
			break;
		case EResolutionState::Overload:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("OverloadIdentifier");
			case EResolutionShape::Call:
				return TEXT("ResolveCallOverload(7)");
			case EResolutionShape::Member:
				return TEXT("ResolutionOwner.ResolveMemberOverload(7)");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionOverload::Resolve(7)");
			}
			break;
		case EResolutionState::Conversion:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("ConversionIdentifier");
			case EResolutionShape::Call:
				return TEXT("ResolveCallConversion(int8(7))");
			case EResolutionShape::Member:
				return TEXT("ResolutionOwner.ResolveMemberConversion(int8(7))");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionConversion::Resolve(int8(7))");
			}
			break;
		case EResolutionState::Missing:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("FNativeCaseValue(MissingIdentifier)");
			case EResolutionShape::Call:
				return TEXT("MissingCall()");
			case EResolutionShape::Member:
				return TEXT("ResolutionOwner.MissingMember");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionExisting::MissingScoped");
			}
			break;
		case EResolutionState::Ambiguous:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("FNativeCaseValue(AmbiguousIdentifier())");
			case EResolutionShape::Call:
				return TEXT("ResolveAmbiguousCall(nullptr)");
			case EResolutionShape::Member:
				return TEXT("ResolutionOwner.ResolveAmbiguousMember(nullptr)");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionAmbiguous::Resolve(nullptr)");
			}
			break;
		case EResolutionState::Inaccessible:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("FNativeCaseValue(HiddenIdentifier)");
			case EResolutionShape::Call:
				return TEXT("FNativeCaseValue(HiddenCall())");
			case EResolutionShape::Member:
				return TEXT("FNativeCaseValue(HiddenOwner.HiddenMember)");
			case EResolutionShape::ScopedName:
				return TEXT("FNativeCaseValue(ResolutionHidden::HiddenScoped)");
			}
			break;
		case EResolutionState::WrongType:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("WrongIdentifier");
			case EResolutionShape::Call:
				return TEXT("ResolveWrongCall()");
			case EResolutionShape::Member:
				return TEXT("ResolutionOwner.WrongMember");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionWrong::Resolve()");
			}
			break;
		}
		return TEXT("FNativeCaseValue(0)");
	}

	static FString DiagnosticToken(
		const FResolutionStateCase& StateCase, const FResolutionShapeCase& ShapeCase)
	{
		switch (StateCase.State)
		{
		case EResolutionState::Missing:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("MissingIdentifier");
			case EResolutionShape::Call:
				return TEXT("MissingCall");
			case EResolutionShape::Member:
				return TEXT("MissingMember");
			case EResolutionShape::ScopedName:
				return TEXT("MissingScoped");
			}
			break;
		case EResolutionState::Ambiguous:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("AmbiguousIdentifier");
			case EResolutionShape::Call:
				return TEXT("ResolveAmbiguousCall");
			case EResolutionShape::Member:
				return TEXT("ResolveAmbiguousMember");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionAmbiguous::Resolve");
			}
			break;
		case EResolutionState::Inaccessible:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("HiddenIdentifier");
			case EResolutionShape::Call:
				return TEXT("HiddenCall");
			case EResolutionShape::Member:
				return TEXT("HiddenMember");
			case EResolutionShape::ScopedName:
				return TEXT("HiddenScoped");
			}
			break;
		case EResolutionState::WrongType:
			switch (ShapeCase.Shape)
			{
			case EResolutionShape::Identifier:
				return TEXT("WrongIdentifier");
			case EResolutionShape::Call:
				return TEXT("ResolveWrongCall");
			case EResolutionShape::Member:
				return TEXT("WrongMember");
			case EResolutionShape::ScopedName:
				return TEXT("ResolutionWrong::Resolve");
			}
			break;
		default:
			break;
		}
		return FString();
	}

	static void AppendResolutionUse(FString& Source,
		const FResolutionContextCase& ContextCase,
		const FResolutionStateCase& StateCase,
		const FResolutionShapeCase& ShapeCase,
		const int32 Expected)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Expression = ResolutionExpression(StateCase, ShapeCase);
		switch (ContextCase.Context)
		{
		case EResolutionContext::Initializer:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tFNativeCaseValue Result = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result.Value;"));
			break;
		case EResolutionContext::Assignment:
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Result;"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tResult = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result.Value;"));
			break;
		case EResolutionContext::Argument:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ObserveResolution(%s);"), *Expression));
			break;
		case EResolutionContext::Condition:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tif ((%s).Value == %d)"), *Expression, Expected));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t\treturn %d;"), Expected));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn -1;"));
			break;
		case EResolutionContext::Index:
			AppendGeneratedAsLine(Source, TEXT("\tFResolutionIndexProbe Probe;"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn Probe[(%s).Value];"), *Expression));
			break;
		case EResolutionContext::MemberReceiver:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn (%s).Value;"), *Expression));
			break;
		case EResolutionContext::Return:
			checkNoEntry();
			break;
		}
	}

	static FString BuildExpressionResolutionSource(const FResolutionContextCase& ContextCase,
		const FResolutionStateCase& StateCase,
		const FResolutionShapeCase& ShapeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const int32 Expected = ExpectedMarker(StateCase, ShapeCase);
		const FString Expression = ResolutionExpression(StateCase, ShapeCase);
		FString Source;
		AppendCommonResolutionDeclarations(Source);

		if (ContextCase.Context == EResolutionContext::Return)
		{
			AppendGeneratedAsLine(Source, TEXT("FNativeCaseValue ProduceResolution()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendResolutionSetup(Source, StateCase, ShapeCase, TEXT("\t"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunExpressionResolution()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (ContextCase.Context == EResolutionContext::Return)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ProduceResolution().Value;"));
		}
		else
		{
			AppendResolutionSetup(Source, StateCase, ShapeCase, TEXT("\t"));
			AppendResolutionUse(Source, ContextCase, StateCase, ShapeCase, Expected);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionResolution()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 173;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildExpressionResolutionRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionResolution()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 173;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		int32& OutBuildResult)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* const Module =
			Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
		if (Module == nullptr)
		{
			OutBuildResult = asERROR;
			return nullptr;
		}
		Module->SetAccessMask(PublicAccessMask);
		const int32 AddResult =
			Module->AddScriptSection(ModuleNameUtf8.Get(), SourceUtf8.Get(), SourceUtf8.Length());
		OutBuildResult = AddResult >= 0 ? Module->Build() : AddResult;
		return Module;
	}

	static int32 LastSourceLineContaining(const FString& Source, const FString& Token)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
		{
			if (Lines[Index].Contains(Token))
			{
				return Index + 1;
			}
		}
		return INDEX_NONE;
	}

	static TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> LocatedErrors(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const FString& ModuleName)
	{
		TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR && Entry.Section == ModuleName && Entry.Row > 0 &&
				Entry.Column > 0)
			{
				Errors.Add(Entry);
			}
		}
		return Errors;
	}

	static asIScriptFunction* FindFunctionByFullDeclaration(
		asIScriptModule& Module, const FString& Declaration)
	{
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function != nullptr &&
				FString(UTF8_TO_TCHAR(Function->GetDeclaration(true, true, false))) == Declaration)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindFunctionByNameAndParamType(
		asIScriptModule& Module, const ANSICHAR* Name, const int32 ParamTypeId)
	{
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function == nullptr || FCStringAnsi::Strcmp(Function->GetName(), Name) != 0 ||
				Function->GetParamCount() != 1)
			{
				continue;
			}
			int32 ActualParamTypeId = asTYPEID_VOID;
			if (Function->GetParam(0, &ActualParamTypeId) >= 0 &&
				ActualParamTypeId == ParamTypeId)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindFunctionByNamespaceNameAndParamType(
		asIScriptModule& Module,
		const ANSICHAR* NameSpace,
		const ANSICHAR* Name,
		const int32 ParamTypeId)
	{
		for (asUINT Index = 0; Index < Module.GetFunctionCount(); ++Index)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(Index);
			if (Function == nullptr || FCStringAnsi::Strcmp(Function->GetName(), Name) != 0 ||
				FCStringAnsi::Strcmp(Function->GetNamespace(), NameSpace) != 0 ||
				Function->GetParamCount() != 1)
			{
				continue;
			}
			int32 ActualParamTypeId = asTYPEID_VOID;
			if (Function->GetParam(0, &ActualParamTypeId) >= 0 &&
				ActualParamTypeId == ParamTypeId)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindMethodByNameAndParamType(
		asITypeInfo& TypeInfo, const ANSICHAR* Name, const int32 ParamTypeId)
	{
		for (asUINT Index = 0; Index < TypeInfo.GetMethodCount(); ++Index)
		{
			asIScriptFunction* const Function = TypeInfo.GetMethodByIndex(Index);
			if (Function == nullptr || FCStringAnsi::Strcmp(Function->GetName(), Name) != 0 ||
				Function->GetParamCount() != 1)
			{
				continue;
			}
			int32 ActualParamTypeId = asTYPEID_VOID;
			if (Function->GetParam(0, &ActualParamTypeId) >= 0 &&
				ActualParamTypeId == ParamTypeId)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static bool HasLocalDeclaration(asIScriptFunction& Function, const FString& Name)
	{
		for (asUINT Index = 0; Index < Function.GetVarCount(); ++Index)
		{
			const char* VariableName = nullptr;
			if (Function.GetVar(Index, &VariableName) >= 0 && VariableName != nullptr &&
				FString(UTF8_TO_TCHAR(VariableName)) == Name)
			{
				return true;
			}
		}
		return false;
	}

	static bool HasGlobalDeclaration(
		asIScriptModule& Module, const FString& Name, const FString& NameSpace, const int32 TypeId)
	{
		for (asUINT Index = 0; Index < Module.GetGlobalVarCount(); ++Index)
		{
			const char* ActualName = nullptr;
			const char* ActualNamespace = nullptr;
			int ActualTypeId = asTYPEID_VOID;
			if (Module.GetGlobalVar(Index, &ActualName, &ActualNamespace, &ActualTypeId) >= 0 &&
				FString(UTF8_TO_TCHAR(ActualName != nullptr ? ActualName : "")) == Name &&
				FString(UTF8_TO_TCHAR(ActualNamespace != nullptr ? ActualNamespace : "")) ==
					NameSpace &&
				ActualTypeId == TypeId)
			{
				return true;
			}
		}
		return false;
	}

	static asIScriptFunction* ExpressionOwnerFunction(
		asIScriptModule& Module, const FResolutionContextCase& ContextCase)
	{
		return ContextCase.Context == EResolutionContext::Return
				   ? Module.GetFunctionByDecl("FNativeCaseValue ProduceResolution()")
				   : Module.GetFunctionByDecl("int RunExpressionResolution()");
	}

	void VerifyResolutionMetadata(const FNativeCaseContext& Case,
		const FResolutionContextCase& ContextCase,
		const FResolutionStateCase& StateCase,
		const FResolutionShapeCase& ShapeCase,
		asIScriptEngine& Engine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunExpressionResolution()");
		asITypeInfo* const ValueType = Engine.GetTypeInfoByDecl("FNativeCaseValue");
		ASSERT_THAT(IsNotNull(
			Entry, *Case.Describe(TEXT("successful resolution should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(
			ValueType, *Case.Describe(TEXT("resolution fixture should publish its value type"))));
		if (Entry == nullptr || ValueType == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(0,
			static_cast<int32>(Entry->GetParamCount()),
			*Case.Describe(TEXT("resolution entry should preserve its exact arity"))));
		ASSERT_THAT(AreEqual(Engine.GetTypeIdByDecl("int"),
			Entry->GetReturnTypeId(),
			*Case.Describe(TEXT("resolution entry should preserve its exact return type"))));

		const char* ValuePropertyName = nullptr;
		int32 ValuePropertyType = asTYPEID_VOID;
		ASSERT_THAT(IsTrue(ValueType->GetProperty(0, &ValuePropertyName, &ValuePropertyType) >= 0 &&
							   ValuePropertyName != nullptr &&
							   FCStringAnsi::Strcmp(ValuePropertyName, "Value") == 0 &&
							   ValuePropertyType == Engine.GetTypeIdByDecl("int"),
			*Case.Describe(TEXT("resolution terminal should retain exact int field metadata"))));

		asIScriptFunction* const Owner = ExpressionOwnerFunction(Module, ContextCase);
		ASSERT_THAT(IsNotNull(
			Owner, *Case.Describe(TEXT("resolution expression should retain an owning function"))));
		if (Owner == nullptr)
		{
			return;
		}

		if (ShapeCase.Shape == EResolutionShape::Identifier)
		{
			FString ExpectedLocal;
			if (StateCase.State == EResolutionState::Exact)
			{
				ExpectedLocal = TEXT("ExactIdentifier");
			}
			else if (StateCase.State == EResolutionState::Overload)
			{
				ExpectedLocal = TEXT("OverloadIdentifier");
			}
			else if (StateCase.State == EResolutionState::Conversion)
			{
				ExpectedLocal = TEXT("ConversionIdentifier");
			}
			if (!ExpectedLocal.IsEmpty())
			{
				ASSERT_THAT(IsTrue(HasLocalDeclaration(*Owner, ExpectedLocal),
					*Case.Describe(
						TEXT("identifier resolution should retain its exact local declaration"))));
			}
			else if (StateCase.State == EResolutionState::NamespaceQualified)
			{
				ASSERT_THAT(IsTrue(HasGlobalDeclaration(Module,
									   TEXT("QualifiedIdentifier"),
									   TEXT("ResolutionQualified"),
									   Engine.GetTypeIdByDecl("int")),
					*Case.Describe(
						TEXT("qualified identifier should retain its namespace and type"))));
			}
		}

		if (ShapeCase.Shape == EResolutionShape::Member &&
			(StateCase.State == EResolutionState::Exact ||
				StateCase.State == EResolutionState::NamespaceQualified))
		{
			const char* TypeDeclaration = StateCase.State == EResolutionState::Exact
											  ? "FResolutionOwner"
											  : "ResolutionQualified::FOwner";
			const char* PropertyName =
				StateCase.State == EResolutionState::Exact ? "ExactMember" : "QualifiedMember";
			asITypeInfo* const OwnerType = Module.GetTypeInfoByDecl(TypeDeclaration);
			ASSERT_THAT(IsNotNull(OwnerType,
				*Case.Describe(TEXT("member resolution should retain its exact owner type"))));
			if (OwnerType != nullptr)
			{
				bool bFound = false;
				for (asUINT Index = 0; Index < OwnerType->GetPropertyCount(); ++Index)
				{
					const char* ActualName = nullptr;
					int32 ActualTypeId = asTYPEID_VOID;
					if (OwnerType->GetProperty(Index, &ActualName, &ActualTypeId) >= 0 &&
						ActualName != nullptr &&
						FCStringAnsi::Strcmp(ActualName, PropertyName) == 0 &&
						ActualTypeId == Engine.GetTypeIdByDecl("int"))
					{
						bFound = true;
						break;
					}
				}
				ASSERT_THAT(IsTrue(bFound,
					*Case.Describe(
						TEXT("member resolution should retain the exact field metadata"))));
			}
		}

		FString SelectedDeclaration;
		if (StateCase.State == EResolutionState::Exact && ShapeCase.Shape == EResolutionShape::Call)
		{
			SelectedDeclaration = TEXT("FNativeCaseValue ResolveExactCall()");
		}
		else if (StateCase.State == EResolutionState::NamespaceQualified &&
				 ShapeCase.Shape == EResolutionShape::Call)
		{
			SelectedDeclaration = TEXT("FNativeCaseValue ResolutionQualified::ResolveCall()");
		}
		else if (StateCase.State == EResolutionState::Overload)
		{
			if (ShapeCase.Shape == EResolutionShape::Identifier)
			{
				SelectedDeclaration = TEXT("FNativeCaseValue ResolveIdentifierOverload(int)");
			}
			else if (ShapeCase.Shape == EResolutionShape::Call)
			{
				SelectedDeclaration = TEXT("FNativeCaseValue ResolveCallOverload(int)");
			}
			else if (ShapeCase.Shape == EResolutionShape::ScopedName)
			{
				SelectedDeclaration = TEXT("FNativeCaseValue ResolutionOverload::Resolve(int)");
			}
		}
		else if (StateCase.State == EResolutionState::Conversion)
		{
			if (ShapeCase.Shape == EResolutionShape::Identifier)
			{
				SelectedDeclaration = TEXT("FNativeCaseValue ResolveIdentifierConversion(int64)");
			}
			else if (ShapeCase.Shape == EResolutionShape::Call)
			{
				SelectedDeclaration = TEXT("FNativeCaseValue ResolveCallConversion(int64)");
			}
			else if (ShapeCase.Shape == EResolutionShape::ScopedName)
			{
				SelectedDeclaration = TEXT("FNativeCaseValue ResolutionConversion::Resolve(int64)");
			}
		}
		if (!SelectedDeclaration.IsEmpty())
		{
			asIScriptFunction* SelectedFunction = nullptr;
			if (StateCase.State == EResolutionState::Overload ||
				StateCase.State == EResolutionState::Conversion)
			{
				const bool bConversion = StateCase.State == EResolutionState::Conversion;
				const int32 ExpectedParamType =
					Engine.GetTypeIdByDecl(bConversion ? "int64" : "int");
				if (ShapeCase.Shape == EResolutionShape::ScopedName)
				{
					const ANSICHAR* ScopedType = bConversion
						? "ResolutionConversion"
						: "ResolutionOverload";
					SelectedFunction = FindFunctionByNamespaceNameAndParamType(
						Module, ScopedType, "Resolve", ExpectedParamType);
				}
				else
				{
					const ANSICHAR* FunctionName = ShapeCase.Shape == EResolutionShape::Identifier
						? (bConversion ? "ResolveIdentifierConversion" : "ResolveIdentifierOverload")
						: (bConversion ? "ResolveCallConversion" : "ResolveCallOverload");
					SelectedFunction = FindFunctionByNameAndParamType(Module, FunctionName, ExpectedParamType);
				}
			}
			else
			{
				SelectedFunction = FindFunctionByFullDeclaration(Module, SelectedDeclaration);
			}
			ASSERT_THAT(IsNotNull(SelectedFunction,
				*Case.DescribeResult("<selected declaration>",
					SelectedDeclaration,
					TEXT("missing exact selected declaration"))));
		}

		if (ShapeCase.Shape == EResolutionShape::Member &&
			(StateCase.State == EResolutionState::Overload ||
				StateCase.State == EResolutionState::Conversion))
		{
			asITypeInfo* const OwnerType = Module.GetTypeInfoByDecl("FResolutionOwner");
			const ANSICHAR* ExpectedMethodName = StateCase.State == EResolutionState::Overload
											 ? "ResolveMemberOverload"
											 : "ResolveMemberConversion";
			const int32 ExpectedParamType = Engine.GetTypeIdByDecl(
				StateCase.State == EResolutionState::Overload ? "int" : "int64");
			ASSERT_THAT(IsTrue(
				OwnerType != nullptr &&
					FindMethodByNameAndParamType(*OwnerType, ExpectedMethodName, ExpectedParamType) != nullptr,
				*Case.Describe(
					TEXT("member selection should retain its exact selected declaration"))));
		}

		if (ShapeCase.Shape == EResolutionShape::ScopedName &&
			StateCase.State == EResolutionState::Exact)
		{
			ASSERT_THAT(IsTrue(HasGlobalDeclaration(Module,
								   TEXT("ExactScoped"),
								   TEXT("ResolutionExact"),
								   Engine.GetTypeIdByDecl("int")),
				*Case.Describe(TEXT("scoped name should retain its exact namespace declaration"))));
		}
		else if (ShapeCase.Shape == EResolutionShape::ScopedName &&
				 StateCase.State == EResolutionState::NamespaceQualified)
		{
			ASSERT_THAT(IsTrue(HasGlobalDeclaration(Module,
								   TEXT("QualifiedScoped"),
								   TEXT("ResolutionOuter::ResolutionInner"),
								   Engine.GetTypeIdByDecl("int")),
				*Case.Describe(TEXT("nested scoped name should retain its complete namespace"))));
		}
	}

	void ExecuteRecovery(const FNativeCaseContext& Case,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext* ReusedContext = nullptr)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Recovery =
			GetNativeFunctionByExactDecl(&Module, "int RecoverExpressionResolution()");
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("resolution recovery should resolve by exact declaration"))));
		asIScriptContext* Context = ReusedContext;
		if (Context == nullptr)
		{
			Context = Engine.CreateContext();
		}
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("resolution recovery should obtain a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("resolution recovery should prepare"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("resolution recovery should finish"))));
			ASSERT_THAT(AreEqual(173,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("resolution recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("resolution recovery should unprepare cleanly"))));
		}
		if (ReusedContext == nullptr && Context != nullptr)
		{
			Context->Release();
		}
	}

	void ExecuteSuccessfulCell(const FNativeCaseContext& Case,
		const FResolutionContextCase& ContextCase,
		const FResolutionStateCase& StateCase,
		const FResolutionShapeCase& ShapeCase,
		FExpressionResolutionState& State,
		FNativeLifecycleRecorder& Lifecycle,
		FReferenceState& ReferenceState,
		asIScriptEngine& Engine,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyResolutionMetadata(Case, ContextCase, StateCase, ShapeCase, Engine, Module);
		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "int RunExpressionResolution()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("successful resolution should expose an executable entry"))));
		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("successful resolution should create a context"))));
		if (Entry == nullptr || Context == nullptr)
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("successful resolution should execute to completion"))));
		const int32 Expected = ExpectedMarker(StateCase, ShapeCase);
		ASSERT_THAT(AreEqual(Expected,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("successful resolution should preserve its selected value"))));
		ASSERT_THAT(AreEqual(1,
			State.CallbackCalls,
			*Case.Describe(
				TEXT("successful resolution should execute exactly one selected marker"))));
		ASSERT_THAT(AreEqual(Expected,
			State.LastMarker,
			*Case.Describe(
				TEXT("successful resolution should identify the selected declaration"))));
		ASSERT_THAT(IsTrue(State.MarkerTrace.Num() == 1 && State.MarkerTrace[0] == Expected,
			*Case.Describe(TEXT("successful resolution should execute no rejected candidate"))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("successful resolution should unprepare cleanly"))));
		ASSERT_THAT(AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("successful resolution should release every value temporary"))));
		ASSERT_THAT(AreEqual(0,
			ReferenceState.LiveObjects,
			*Case.Describe(
				TEXT("successful resolution should create no ambiguity reference object"))));
		ExecuteRecovery(Case, Engine, Module, Context);
		Context->Release();
	}

	void RunCell(FNativeTestEngine& Engine,
		FExpressionResolutionState& State,
		FNativeLifecycleRecorder& Lifecycle,
		FReferenceState& ReferenceState,
		const FResolutionContextCase& ContextCase,
		const FResolutionShapeCase& ShapeCase,
		const FResolutionStateCase& StateCase)
	{
		using namespace AngelscriptNativeTestSupport;

		State.Reset();
		ASSERT_THAT(AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			TEXT("resolution lifecycle should be empty before reset")));
		Lifecycle.Reset();
		ReferenceState.ResetCounters();
		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-RESOLUTION",
			{
				ANSI_TO_TCHAR(ContextCase.CatalogName),
				ANSI_TO_TCHAR(ShapeCase.CatalogName),
				ANSI_TO_TCHAR(StateCase.CatalogName),
			}));
		const FString ModuleName = FString::Printf(TEXT("ExpressionResolution_%hs_%hs_%hs"),
			ContextCase.CatalogName,
			ShapeCase.CatalogName,
			StateCase.CatalogName);
		const FString Source = BuildExpressionResolutionSource(ContextCase, StateCase, ShapeCase);
		int32 BuildResult = asERROR;
		asIScriptModule* Module =
			CompileAndReport(Engine, Case, Case.GetId(), ModuleName, Source, BuildResult);

		if (IsSuccessfulState(StateCase))
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.DescribeResult("<module build>",
					TEXT("successful resolution compilation"),
					Engine.GetMessagesText())));
			ASSERT_THAT(IsNotNull(
				Module, *Case.Describe(TEXT("successful resolution should publish a module"))));
			if (BuildResult >= 0 && Module != nullptr)
			{
				ExecuteSuccessfulCell(Case,
					ContextCase,
					StateCase,
					ShapeCase,
					State,
					Lifecycle,
					ReferenceState,
					*Engine.Get(),
					*Module);
			}
		}
		else if (StateCase.State == EResolutionState::Inaccessible &&
				 !IsAccessMaskEnforced(*Engine.Get()))
		{
			// Current fork registrations retain the full native access mask, so
			// module filtering cannot reject these declarations. Keep the generated
			// shapes buildable and recoverable, and record this limitation in OpenSpec.
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.DescribeResult("<module build>",
					TEXT("fork access-mask limitation should leave the source buildable"),
					Engine.GetMessagesText())));
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("rejected resolution should fail compilation"))));
			const TArray<FNativeMessageEntry> Errors =
				LocatedErrors(Engine.GetMessages(), ModuleName);
			ASSERT_THAT(AreEqual(1,
				Errors.Num(),
				*Case.DescribeResult("<located errors>",
					TEXT("one expression-owned error"),
					Engine.GetMessagesText())));
			const FString Token = DiagnosticToken(StateCase, ShapeCase);
			const int32 ExpectedLine = LastSourceLineContaining(Source, Token);
			ASSERT_THAT(IsTrue(ExpectedLine > 0,
				*Case.Describe(TEXT("rejected resolution should locate its generated token"))));
			if (Errors.Num() == 1)
			{
				ASSERT_THAT(AreEqual(ExpectedLine,
					Errors[0].Row,
					*Case.Describe(TEXT(
						"rejected resolution diagnostic should own the target expression line"))));
			}
			ASSERT_THAT(AreEqual(0,
				State.CallbackCalls,
				*Case.Describe(TEXT("rejected resolution should execute no candidate"))));
			ASSERT_THAT(AreEqual(0,
				Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("rejected resolution should retain no value object"))));
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
			const FString RecoverySource = BuildExpressionResolutionRecoverySource();
			int32 RecoveryBuildResult = asERROR;
			Module = CompileAndReport(Engine,
				Case,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				RecoveryBuildResult);
			ASSERT_THAT(IsTrue(RecoveryBuildResult >= 0,
				*Case.DescribeResult("<recovery build>",
					TEXT("successful same-name recovery"),
					Engine.GetMessagesText())));
			if (RecoveryBuildResult >= 0 && Module != nullptr)
			{
				ExecuteRecovery(Case, *Engine.Get(), *Module);
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("resolution cell should discard its isolated module"))));
		ASSERT_THAT(AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("resolution module discard should retain no value object"))));
		ASSERT_THAT(AreEqual(0,
			ReferenceState.LiveObjects,
			*Case.Describe(TEXT("resolution module discard should retain no reference object"))));
	}

public:
	TEST_METHOD(StatesByContextAndShape)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-RESOLUTION",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Runtime |
				ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Expression resolution product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FExpressionResolutionState State;
		FNativeLifecycleRecorder Lifecycle;
		FReferenceState ReferenceState;
		const bool bFixturesRegistered =
			RegisterResolutionFixtures(*ScriptEngine, State, Lifecycle, ReferenceState);
		ASSERT_THAT(IsTrue(bFixturesRegistered,
			*FNativeCaseContext(TEXT("LANG-EXPR-RESOLUTION-FIXTURES")).DescribeResult(
				"<fixture registration>",
				TEXT("core resolution fixtures"),
				Engine.GetMessagesText())));
		ASSERT_THAT(AreEqual(static_cast<asPWORD>(3),
			ScriptEngine->GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			TEXT("Expression resolution engine should retain strict property accessor mode")));

		for (const FResolutionContextCase& ContextCase : ContextCases)
		{
			for (const FResolutionShapeCase& ShapeCase : ShapeCases)
			{
				for (const FResolutionStateCase& StateCase : StateCases)
				{
					RunCell(Engine,
						State,
						Lifecycle,
						ReferenceState,
						ContextCase,
						ShapeCase,
						StateCase);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
