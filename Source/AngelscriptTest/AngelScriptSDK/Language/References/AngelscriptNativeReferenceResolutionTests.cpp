#include "AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_callfunc.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FReferenceResolutionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.References.Resolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FReferenceRoot =
		AngelscriptNativeReferenceTestSupport::FReferenceRoot;
	using FReferenceState =
		AngelscriptNativeReferenceTestSupport::FReferenceState;

	inline static constexpr asPWORD ResolutionStateUserDataSlot =
		static_cast<asPWORD>(0x5245465245534F4Cull);

	enum class ECandidateSet : uint8
	{
		MutableExact,
		ConstPair,
		ValueVsInput,
		BaseVsDerived,
		ReturnCovariance,
		NullPair,
		NumericConversion,
		CompetingConversions,
		MissingCandidate,
		IncompatibleCandidate,
	};

	enum class EResolutionSource : uint8
	{
		MutableLValue,
		ConstLValue,
		Temporary,
		Field,
		Parameter,
		BaseView,
		DerivedView,
		Null,
	};

	enum class ECallSite : uint8
	{
		Direct,
		Helper,
	};

	struct FCandidateCase
	{
		const ANSICHAR* CatalogName;
		ECandidateSet Set;
	};

	struct FSourceCase
	{
		const ANSICHAR* CatalogName;
		EResolutionSource Source;
	};

	struct FSiteCase
	{
		const ANSICHAR* CatalogName;
		ECallSite Site;
	};

	struct FResolutionState
	{
		int32 Calls = 0;
		int32 SelectedMarker = 0;
		int32 SelectedIdentity = 0;
		FString RegistrationDetails;

		void Reset()
		{
			Calls = 0;
			SelectedMarker = 0;
			SelectedIdentity = 0;
			RegistrationDetails.Reset();
		}
	};

	struct FRegisteredCandidate
	{
		FString Declaration;
		int32 Marker = 0;
		asIScriptFunction* Function = nullptr;
	};

	inline static constexpr FCandidateCase CandidateCases[] =
	{
		{
			"mutable_exact",
			ECandidateSet::MutableExact,
		},
		{ "const_pair", ECandidateSet::ConstPair },
		{ "value_vs_in", ECandidateSet::ValueVsInput },
		{
			"base_vs_derived",
			ECandidateSet::BaseVsDerived,
		},
		{
			"return_covariance",
			ECandidateSet::ReturnCovariance,
		},
		{ "null_pair", ECandidateSet::NullPair },
		{
			"numeric_conversion",
			ECandidateSet::NumericConversion,
		},
		{
			"competing_conversions",
			ECandidateSet::CompetingConversions,
		},
		{
			"missing_candidate",
			ECandidateSet::MissingCandidate,
		},
		{
			"incompatible_candidate",
			ECandidateSet::IncompatibleCandidate,
		},
	};

	inline static constexpr FSourceCase SourceCases[] =
	{
		{
			"mutable_lvalue",
			EResolutionSource::MutableLValue,
		},
		{
			"const_lvalue",
			EResolutionSource::ConstLValue,
		},
		{ "temporary", EResolutionSource::Temporary },
		{ "field", EResolutionSource::Field },
		{ "parameter", EResolutionSource::Parameter },
		{ "base_view", EResolutionSource::BaseView },
		{
			"derived_view",
			EResolutionSource::DerivedView,
		},
		{ "null", EResolutionSource::Null },
	};

	inline static constexpr FSiteCase SiteCases[] =
	{
		{ "direct", ECallSite::Direct },
		{ "helper", ECallSite::Helper },
	};

	static FResolutionState* GetResolutionState(
		asIScriptEngine& ScriptEngine)
	{
		return static_cast<FResolutionState*>(
			ScriptEngine.GetUserData(
				ResolutionStateUserDataSlot));
	}

	static FResolutionState* GetResolutionState(
		asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
			? GetResolutionState(*Generic.GetEngine())
			: nullptr;
	}

	static int32 GetFunctionMarker(
		asIScriptFunction* const Function)
	{
		return Function != nullptr
			? static_cast<int32>(
				reinterpret_cast<UPTRINT>(
					Function->GetUserData()))
			: 0;
	}

	static void RecordSelection(
		asIScriptGeneric& Generic,
		FReferenceRoot* const Object)
	{
		FResolutionState* const State =
			GetResolutionState(Generic);
		if (State == nullptr)
		{
			return;
		}
		++State->Calls;
		State->SelectedMarker =
			GetFunctionMarker(
				Generic.GetFunction());
		State->SelectedIdentity =
			Object != nullptr
				? Object->GetIdentity()
				: 0;
	}

	static void GenericSelectMarker(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const int32 Marker =
			GetFunctionMarker(
				Generic->GetFunction());
		FReferenceRoot* Object = nullptr;
		if (Marker == 302)
		{
			void* const ArgumentAddress =
				Generic->GetArgAddress(0);
			const int32 ArgumentTypeId =
				Generic->GetArgTypeId(0);
			Object = (ArgumentTypeId & asTYPEID_OBJHANDLE) != 0
				? (ArgumentAddress != nullptr
					? *static_cast<FReferenceRoot**>(
						ArgumentAddress)
					: nullptr)
				: static_cast<FReferenceRoot*>(
					ArgumentAddress);
		}
		else if (Marker < 700
			|| Marker >= 900)
		{
			Object =
				static_cast<FReferenceRoot*>(
					Generic->GetArgObject(0));
		}
		RecordSelection(
			*Generic,
			Object);
		Generic->SetReturnDWord(
			static_cast<asDWORD>(Marker));
	}

	static void GenericSelectCovariant(
		asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceRoot* const Object =
			static_cast<FReferenceRoot*>(
				Generic->GetArgObject(0));
		RecordSelection(
			*Generic,
			Object);
		if (Object != nullptr)
		{
			Object->AddRef();
		}
		Generic->SetReturnAddress(Object);
	}

	static bool RegisterCandidate(
		asIScriptEngine& ScriptEngine,
		FResolutionState& State,
		const ANSICHAR* const Declaration,
		const int32 Marker,
		const asSFuncPtr& Callback,
		TArray<FRegisteredCandidate>& OutCandidates)
	{
		const int RegistrationResult =
			ScriptEngine.RegisterGlobalFunction(
			Declaration,
			Callback,
			asCALL_GENERIC);
		if (RegistrationResult < 0)
		{
			State.RegistrationDetails += FString::Printf(
				TEXT("%s -> %d; "),
				UTF8_TO_TCHAR(Declaration),
				RegistrationResult);
			return false;
		}
		asIScriptFunction* const Function =
			ScriptEngine.GetFunctionById(
				RegistrationResult);
		if (Function == nullptr)
		{
			State.RegistrationDetails += FString::Printf(
				TEXT("%s -> %d (no function); "),
				UTF8_TO_TCHAR(Declaration),
				RegistrationResult);
			return false;
		}
		State.RegistrationDetails += FString::Printf(
			TEXT("%s -> Id=%d Canonical=%s; "),
			UTF8_TO_TCHAR(Declaration),
			RegistrationResult,
			UTF8_TO_TCHAR(Function->GetDeclaration(
				true,
				false,
				true)));
		Function->SetUserData(
			reinterpret_cast<void*>(
				static_cast<UPTRINT>(Marker)));
		OutCandidates.Add(
			{
				UTF8_TO_TCHAR(Declaration),
				Marker,
				Function,
			});
		return true;
	}

	static bool RegisterCandidateSet(
		asIScriptEngine& ScriptEngine,
		FResolutionState& State,
		const FCandidateCase& Candidate,
		TArray<FRegisteredCandidate>& OutCandidates)
	{
		ScriptEngine.SetUserData(
			&State,
			ResolutionStateUserDataSlot);
		switch (Candidate.Set)
		{
		case ECandidateSet::MutableExact:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(FRefRoot Value)",
				101,
				asFUNCTION(GenericSelectMarker),
				OutCandidates);
		case ECandidateSet::ConstPair:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(FRefRoot Value)",
				201,
				asFUNCTION(GenericSelectMarker),
				OutCandidates)
				&& RegisterCandidate(
					ScriptEngine,
					State,
					"int SelectReference(const FRefRoot Value)",
					202,
					asFUNCTION(GenericSelectMarker),
					OutCandidates);
		case ECandidateSet::ValueVsInput:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(FRefRoot Value)",
				301,
				asFUNCTION(GenericSelectMarker),
				OutCandidates)
				&& RegisterCandidate(
					ScriptEngine,
					State,
					"int SelectReference(const ?&in Value)",
					302,
					asFUNCTION(GenericSelectMarker),
					OutCandidates);
		case ECandidateSet::BaseVsDerived:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(const FRefRoot Value)",
				401,
				asFUNCTION(GenericSelectMarker),
				OutCandidates)
				&& RegisterCandidate(
					ScriptEngine,
					State,
					"int SelectReference(const FRefDerived Value)",
					402,
					asFUNCTION(GenericSelectMarker),
					OutCandidates);
		case ECandidateSet::ReturnCovariance:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"const FRefRoot SelectReference(const FRefRoot Value)",
				501,
				asFUNCTION(GenericSelectCovariant),
				OutCandidates)
				&& RegisterCandidate(
					ScriptEngine,
					State,
					"const FRefDerived SelectReference(const FRefDerived Value)",
					502,
					asFUNCTION(GenericSelectCovariant),
					OutCandidates);
		case ECandidateSet::NullPair:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(const FRefRoot Value)",
				601,
				asFUNCTION(GenericSelectMarker),
				OutCandidates)
				&& RegisterCandidate(
					ScriptEngine,
					State,
					"int SelectReference(const FRefUnrelated Value)",
					602,
					asFUNCTION(GenericSelectMarker),
					OutCandidates);
		case ECandidateSet::NumericConversion:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(int64 Value)",
				701,
				asFUNCTION(GenericSelectMarker),
				OutCandidates);
		case ECandidateSet::CompetingConversions:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(int64 Value)",
				801,
				asFUNCTION(GenericSelectMarker),
				OutCandidates)
				&& RegisterCandidate(
					ScriptEngine,
					State,
					"int SelectReference(double Value)",
					802,
					asFUNCTION(GenericSelectMarker),
					OutCandidates);
		case ECandidateSet::MissingCandidate:
			return true;
		case ECandidateSet::IncompatibleCandidate:
			return RegisterCandidate(
				ScriptEngine,
				State,
				"int SelectReference(FRefUnrelated Value)",
				901,
				asFUNCTION(GenericSelectMarker),
				OutCandidates);
		default:
			return false;
		}
	}

	static bool IsCovariantSet(
		const FCandidateCase& Candidate)
	{
		return Candidate.Set
			== ECandidateSet::ReturnCovariance;
	}

	static bool IsNumericSet(
		const FCandidateCase& Candidate)
	{
		return Candidate.Set
				== ECandidateSet::NumericConversion
			|| Candidate.Set
				== ECandidateSet::CompetingConversions;
	}

	static bool IsCompileFailure(
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		if (Candidate.Set
				== ECandidateSet::MissingCandidate
			|| Candidate.Set
				== ECandidateSet::IncompatibleCandidate)
		{
			return true;
		}
		if (Candidate.Set
				== ECandidateSet::MutableExact
			&& Source.Source
				== EResolutionSource::ConstLValue)
		{
			return true;
		}
		if (Candidate.Set
				== ECandidateSet::ConstPair
			&& Source.Source
				!= EResolutionSource::ConstLValue)
		{
			return true;
		}
		return Candidate.Set
				== ECandidateSet::NullPair
			&& Source.Source
				== EResolutionSource::Null
			&& Site.Site == ECallSite::Direct;
	}

	static int32 ExpectedMarker(
		const FCandidateCase& Candidate,
		const FSourceCase& Source)
	{
		switch (Candidate.Set)
		{
		case ECandidateSet::MutableExact:
			return 101;
		case ECandidateSet::ConstPair:
			return Source.Source
					== EResolutionSource::ConstLValue
				? 202
				: 201;
		case ECandidateSet::ValueVsInput:
			return Source.Source
						== EResolutionSource::ConstLValue
				? 302
				: 301;
		case ECandidateSet::BaseVsDerived:
			return Source.Source
					== EResolutionSource::DerivedView
				? 402
				: 401;
		case ECandidateSet::ReturnCovariance:
			return Source.Source
					== EResolutionSource::DerivedView
				? 502
				: 501;
		case ECandidateSet::NullPair:
			return 601;
		case ECandidateSet::NumericConversion:
			return 701;
		case ECandidateSet::CompetingConversions:
			// The current fork publishes the `double` registration as the
			// canonical `float` overload. Its bytecode must therefore select
			// that published candidate; the generic callback identity is
			// asserted independently below.
			return 802;
		default:
			return 0;
		}
	}

	static FString StaticSourceType(
		const FSourceCase& Source)
	{
		if (Source.Source
			== EResolutionSource::DerivedView)
		{
			return TEXT("FRefDerived");
		}
		return TEXT("FRefRoot");
	}

	static FString SourceParameter(
		const FSourceCase& Source)
	{
		if (Source.Source
			== EResolutionSource::ConstLValue)
		{
			return TEXT("const FRefRoot& in Source");
		}
		return StaticSourceType(Source)
			+ TEXT(" Source");
	}

	static FString CallArgument(
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site,
		const FString& Expression)
	{
		if (IsNumericSet(Candidate))
		{
			return Source.Source
					== EResolutionSource::Null
				? TEXT("0")
				: TEXT("(") + Expression
					+ TEXT(" == nullptr ? 0 : ")
					+ Expression
					+ TEXT(".GetValue())");
		}
		if (Candidate.Set
				== ECandidateSet::NullPair
			&& Source.Source
				== EResolutionSource::Null
			&& Site.Site == ECallSite::Direct)
		{
			return TEXT("nullptr");
		}
		return Expression;
	}

	static FString ResultType(
		const FSourceCase& Source)
	{
		return Source.Source
				== EResolutionSource::DerivedView
			? TEXT("const FRefDerived")
			: TEXT("const FRefRoot");
	}

	static void AppendSelectionBody(
		FString& Script,
		const FString& Indent,
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site,
		const FString& Expression)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Argument =
			CallArgument(
				Candidate,
				Source,
				Site,
				Expression);
		if (!IsCovariantSet(Candidate))
		{
			AppendGeneratedAsLine(
				Script,
				Indent
					+ TEXT("return SelectReference(")
					+ Argument
					+ TEXT(");"));
			return;
		}
		AppendGeneratedAsLine(
			Script,
			Indent + ResultType(Source)
				+ TEXT(" Result = SelectReference(")
				+ Argument
				+ TEXT(");"));
		if (Source.Source
			== EResolutionSource::Null)
		{
			AppendGeneratedAsLine(
				Script,
				Indent
					+ TEXT("return Result == nullptr ? 1 : 0;"));
			return;
		}
		const int32 ExpectedKind =
			Source.Source
					== EResolutionSource::DerivedView
				|| Source.Source
					== EResolutionSource::BaseView
			? 202
			: 101;
		AppendGeneratedAsLine(
			Script,
			Indent
				+ FString::Printf(
					TEXT("return Result != nullptr && Result.GetKind() == %d"),
					ExpectedKind));
		if (Source.Source
			!= EResolutionSource::Temporary)
		{
			AppendGeneratedAsLine(
				Script,
				Indent
					+ TEXT("\t&& SameReference(Result, ")
					+ Expression
					+ TEXT(")"));
		}
		AppendGeneratedAsLine(
			Script,
			Indent + TEXT("\t? 1"));
		AppendGeneratedAsLine(
			Script,
			Indent + TEXT("\t: 0;"));
	}

	static void AppendFieldType(
		FString& Script,
		const FSourceCase& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Source.Source
			!= EResolutionSource::Field)
		{
			return;
		}
		AppendGeneratedAsLine(
			Script,
			TEXT("struct FResolutionOwner"));
		AppendGeneratedAsLine(Script, TEXT("{"));
		AppendGeneratedAsLine(
			Script,
			TEXT("\tFRefRoot Field;"));
		AppendGeneratedAsLine(Script, TEXT("}"));
		AppendGeneratedAsLine(Script);
	}

	static void AppendHelperSite(
		FString& Script,
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Site.Site != ECallSite::Helper)
		{
			return;
		}
		AppendGeneratedAsLine(
			Script,
			TEXT("int InvokeThroughHelper(")
				+ SourceParameter(Source)
				+ TEXT(")"));
		AppendGeneratedAsLine(Script, TEXT("{"));
		AppendSelectionBody(
			Script,
			TEXT("\t"),
			Candidate,
			Source,
			Site,
			TEXT("Source"));
		AppendGeneratedAsLine(Script, TEXT("}"));
		AppendGeneratedAsLine(Script);
	}

	static void AppendParameterSite(
		FString& Script,
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		if (Source.Source
			!= EResolutionSource::Parameter)
		{
			return;
		}
		AppendGeneratedAsLine(
			Script,
			TEXT("int InvokeParameter(FRefRoot Source)"));
		AppendGeneratedAsLine(Script, TEXT("{"));
		if (Site.Site == ECallSite::Helper)
		{
			AppendGeneratedAsLine(
				Script,
				TEXT("\treturn InvokeThroughHelper(Source);"));
		}
		else
		{
			AppendSelectionBody(
				Script,
				TEXT("\t"),
				Candidate,
				Source,
				Site,
				TEXT("Source"));
		}
		AppendGeneratedAsLine(Script, TEXT("}"));
		AppendGeneratedAsLine(Script);
	}

	static FString SourceExpression(
		const FSourceCase& Source)
	{
		switch (Source.Source)
		{
		case EResolutionSource::MutableLValue:
		case EResolutionSource::ConstLValue:
			return TEXT("Source");
		case EResolutionSource::Temporary:
			return TEXT("MakeRefRoot(33)");
		case EResolutionSource::Field:
			return TEXT("Owner.Field");
		case EResolutionSource::Parameter:
			return TEXT("MakeRefRoot(35)");
		case EResolutionSource::BaseView:
			return TEXT("Source");
		case EResolutionSource::DerivedView:
			return TEXT("Source");
		case EResolutionSource::Null:
			return TEXT("Source");
		default:
			return TEXT("Source");
		}
	}

	static void AppendEntrySourceSetup(
		FString& Script,
		const FSourceCase& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (Source.Source)
		{
		case EResolutionSource::MutableLValue:
			AppendGeneratedAsLine(
				Script,
				TEXT("\tFRefRoot Source = MakeRefRoot(31);"));
			break;
		case EResolutionSource::ConstLValue:
			AppendGeneratedAsLine(
				Script,
				TEXT("\tconst FRefRoot Source = MakeRefRoot(32);"));
			break;
		case EResolutionSource::Field:
			AppendGeneratedAsLine(
				Script,
				TEXT("\tFResolutionOwner Owner;"));
			AppendGeneratedAsLine(
				Script,
				TEXT("\tOwner.Field = MakeRefRoot(34);"));
			break;
		case EResolutionSource::BaseView:
			AppendGeneratedAsLine(
				Script,
				TEXT("\tFRefRoot Source = MakeRefDerivedAsRoot(36);"));
			break;
		case EResolutionSource::DerivedView:
			AppendGeneratedAsLine(
				Script,
				TEXT("\tFRefDerived Source = MakeRefDerived(37);"));
			break;
		case EResolutionSource::Null:
			AppendGeneratedAsLine(
				Script,
				TEXT("\tFRefRoot Source = nullptr;"));
			break;
		default:
			break;
		}
	}

	static void AppendResolutionEntry(
		FString& Script,
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(
			Script,
			TEXT("int RunReferenceResolution()"));
		AppendGeneratedAsLine(Script, TEXT("{"));
		AppendEntrySourceSetup(
			Script,
			Source);
		const FString Expression =
			SourceExpression(Source);
		if (Source.Source
			== EResolutionSource::Parameter)
		{
			AppendGeneratedAsLine(
				Script,
				TEXT("\treturn InvokeParameter(")
					+ Expression + TEXT(");"));
		}
		else if (Site.Site == ECallSite::Helper)
		{
			AppendGeneratedAsLine(
				Script,
				TEXT("\treturn InvokeThroughHelper(")
					+ Expression + TEXT(");"));
		}
		else
		{
			AppendSelectionBody(
				Script,
				TEXT("\t"),
				Candidate,
				Source,
				Site,
				Expression);
		}
		AppendGeneratedAsLine(Script, TEXT("}"));
		AppendGeneratedAsLine(Script);
		AppendGeneratedAsLine(
			Script,
			TEXT("int RecoverReferenceResolution()"));
		AppendGeneratedAsLine(Script, TEXT("{"));
		AppendGeneratedAsLine(Script, TEXT("\treturn 923;"));
		AppendGeneratedAsLine(Script, TEXT("}"));
	}

	static FString BuildReferenceResolutionSource(
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		FString Script;
		AppendFieldType(
			Script,
			Source);
		AppendHelperSite(
			Script,
			Candidate,
			Source,
			Site);
		AppendParameterSite(
			Script,
			Candidate,
			Source,
			Site);
		AppendResolutionEntry(
			Script,
			Candidate,
			Source,
			Site);
		return Script;
	}

	static FString BuildReferenceResolutionRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Script;
		AppendGeneratedAsLine(
			Script,
			TEXT("int RecoverReferenceResolution()"));
		AppendGeneratedAsLine(Script, TEXT("{"));
		AppendGeneratedAsLine(Script, TEXT("\treturn 923;"));
		AppendGeneratedAsLine(Script, TEXT("}"));
		return Script;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::
			PrintGeneratedAsSource(
				Test,
				SourceId,
				ModuleName,
				Source);
		const FTCHARToUTF8 ModuleNameUtf8(
			*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::
			CompileNativeModule(
				&ScriptEngine,
				ModuleNameUtf8.Get(),
				SourceUtf8.Get(),
				OutModule);
	}

	static asIScriptFunction* CandidateByMarker(
		const TArray<FRegisteredCandidate>& Candidates,
		const int32 Marker)
	{
		for (const FRegisteredCandidate& Candidate
			: Candidates)
		{
			if (Candidate.Marker == Marker)
			{
				return Candidate.Function;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* SelectionWitness(
		asIScriptModule& Module,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		if (Site.Site == ECallSite::Helper)
		{
			const FString Declaration =
				TEXT("int InvokeThroughHelper(")
				+ SourceParameter(Source)
				+ TEXT(")");
			return Module.GetFunctionByDecl(
				TCHAR_TO_ANSI(*Declaration));
		}
		if (Source.Source
			== EResolutionSource::Parameter)
		{
			return Module.GetFunctionByDecl(
				"int InvokeParameter(FRefRoot Source)");
		}
		return Module.GetFunctionByDecl(
			"int RunReferenceResolution()");
	}

	static bool BytecodeCallsCandidate(
		asIScriptFunction& Witness,
		asIScriptFunction& Expected)
	{
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode =
			Witness.GetByteCode(
				&BytecodeLength);
		if (Bytecode == nullptr
			|| BytecodeLength == 0)
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
				> static_cast<int32>(
					asBC_MAXBYTECODE))
			{
				return false;
			}
			if (Opcode == asBC_CALLSYS)
			{
				asIScriptFunction* const Called =
					static_cast<asIScriptFunction*>(
						reinterpret_cast<asCScriptFunction*>(
							asBC_PTRARG(
								&Bytecode[DwordIndex])));
				if (Called == &Expected)
				{
					return true;
				}
			}
			else if ((Opcode == asBC_CALL
					|| Opcode == asBC_CALLINTF)
				&& asBC_INTARG(
					&Bytecode[DwordIndex])
					== Expected.GetId())
			{
				return true;
			}
			const int32 InstructionSize =
				asBCTypeSize[
					asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				return false;
			}
			DwordIndex +=
				static_cast<asUINT>(
					InstructionSize);
		}
		return false;
	}

	static FString DescribeBytecodeCalls(
		asIScriptFunction& Witness)
	{
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode =
			Witness.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr
			|| BytecodeLength == 0)
		{
			return TEXT("no bytecode");
		}
		TArray<FString> Calls;
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
				return TEXT("invalid opcode");
			}
			if (Opcode == asBC_CALLSYS
				|| Opcode == asBC_Thiscall1)
			{
				asCScriptFunction* const Called =
					reinterpret_cast<asCScriptFunction*>(
						asBC_PTRARG(&Bytecode[DwordIndex]));
				Calls.Add(FString::Printf(
					TEXT("%u:%s:%s(Id=%d)"),
					DwordIndex,
					ANSI_TO_TCHAR(asBCInfo[Opcode].name),
					Called != nullptr
						? UTF8_TO_TCHAR(
							Called->GetDeclaration(
								true,
								false,
								true))
						: TEXT("null"),
					Called != nullptr
						? Called->GetId()
						: -1));
			}
			const int32 InstructionSize =
				asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				return TEXT("invalid instruction size");
			}
			DwordIndex +=
				static_cast<asUINT>(InstructionSize);
		}
		return Calls.IsEmpty()
			? TEXT("no system call instructions")
			: FString::Join(Calls, TEXT("; "));
	}

	void VerifyCandidateMetadata(
		const FNativeCaseContext& Case,
		const TArray<FRegisteredCandidate>& Candidates,
		FAutomationTestBase& OutputTest)
	{
		for (const FRegisteredCandidate& Candidate
			: Candidates)
		{
			ASSERT_THAT(IsNotNull(
				Candidate.Function,
				*Case.Describe(TEXT("reference resolution should retain every registered candidate"))));
			if (Candidate.Function == nullptr)
			{
				continue;
			}
			ASSERT_THAT(AreEqual(
				Candidate.Marker,
				GetFunctionMarker(
					Candidate.Function),
				*Case.Describe(TEXT("reference resolution candidate should retain its stable native marker"))));
			const FString ActualDeclaration =
				UTF8_TO_TCHAR(Candidate.Function->GetDeclaration(
					true,
					false,
					true));
			const bool bCurrentForkCanonicalizesDouble =
				Candidate.Declaration
					== TEXT("int SelectReference(double Value)");
			const FString ExpectedDeclaration =
				bCurrentForkCanonicalizesDouble
					? TEXT("int SelectReference(float Value)")
					: Candidate.Declaration;
			if (bCurrentForkCanonicalizesDouble)
			{
				OutputTest.AddInfo(FString::Printf(
					TEXT("[AS-REF-FORK-LIMITATION] Registered='%s' Canonical='%s' Behavior=double-native-registration-is-exposed-as-float"),
					*Candidate.Declaration,
					*ActualDeclaration));
			}
			ASSERT_THAT(AreEqual(
				ExpectedDeclaration,
				ActualDeclaration,
				*(bCurrentForkCanonicalizesDouble
					? Case.DescribeResult(
						TEXT("reference resolution should retain the documented current fork double canonicalization"),
						Candidate.Declaration,
						ActualDeclaration)
					: Case.DescribeResult(
						TEXT("reference resolution candidate should retain its exact declaration"),
						Candidate.Declaration,
						ActualDeclaration))));
		}
	}

	void RecordGenericParameterLayout(
		FAutomationTestBase& OutputTest,
		const TArray<FRegisteredCandidate>& Candidates)
	{
		for (const FRegisteredCandidate& Candidate
			: Candidates)
		{
			const asCScriptFunction* const InternalFunction =
				static_cast<const asCScriptFunction*>(
					Candidate.Function);
			if (InternalFunction == nullptr
				|| InternalFunction->parameterTypes.GetLength() == 0)
			{
				continue;
			}
			const asCDataType& Parameter =
				InternalFunction->parameterTypes[0];
			const asCTypeInfo* const TypeInfo =
				Parameter.GetTypeInfo();
			const asQWORD TypeFlags =
				TypeInfo != nullptr
					? TypeInfo->GetFlags()
					: 0;
			const bool bIsImplicitHandle =
				(TypeFlags & asOBJ_IMPLICIT_HANDLE) != 0;
			const asUINT CleanArgumentCount =
				InternalFunction->sysFuncIntf != nullptr
					? InternalFunction->sysFuncIntf->cleanArgs.GetLength()
					: 0;
			OutputTest.AddInfo(FString::Printf(
				TEXT("[AS-REF-GENERIC-PARAMETER] Candidate=%s Id=%d IsObject=%d IsFuncdef=%d IsReference=%d IsObjectHandle=%d TypeFlags=0x%llX IsImplicitHandle=%d CleanArgs=%u"),
				*Candidate.Declaration,
				Candidate.Function != nullptr
					? Candidate.Function->GetId()
					: -1,
				Parameter.IsObject() ? 1 : 0,
				Parameter.IsFuncdef() ? 1 : 0,
				Parameter.IsReference() ? 1 : 0,
				Parameter.IsObjectHandle() ? 1 : 0,
				static_cast<unsigned long long>(TypeFlags),
				bIsImplicitHandle ? 1 : 0,
				CleanArgumentCount));
		}
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RecoverReferenceResolution()");
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("reference resolution should publish recovery"))));
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("reference resolution recovery should create a context"))));
		if (Recovery != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("reference resolution recovery should prepare"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference resolution recovery should finish"))));
			ASSERT_THAT(AreEqual(
				923,
				static_cast<int32>(
					Context->GetReturnDWord()),
				*Case.Describe(TEXT("reference resolution recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference resolution recovery should unprepare"))));
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void CompileFailedBuildRecovery(
		const FNativeCaseContext& Case,
		FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		const FString Source =
			BuildReferenceResolutionRecoverySource();
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		ASSERT_THAT(IsTrue(
			CompileAndReport(
				*TestRunner,
				ScriptEngine,
				Case.GetId()
					+ TEXT("-RECOVERY"),
				ModuleName,
				Source,
				Module) >= 0,
			*Case.Describe(TEXT("reference resolution failed-build recovery should compile"))));
		ASSERT_THAT(IsNotNull(Module,
			*Case.Describe(TEXT("reference resolution failed-build recovery should publish a module"))));
		if (Module != nullptr)
		{
			ExecuteRecovery(
				Case,
				ScriptEngine,
				*Module);
		}
		AngelscriptNativeReferenceTestSupport::
			DiscardReferenceModule(
				ScriptEngine,
				ModuleName);
	}

	void ExecuteSelection(
		const FNativeCaseContext& Case,
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const int32 Marker,
		FResolutionState& ResolutionState,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl(
				"int RunReferenceResolution()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl(
				"int RecoverReferenceResolution()");
		asIScriptContext* const Context =
			ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("reference resolution should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("reference resolution should publish same-context recovery"))));
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("reference resolution should create a context"))));
		if (Entry != nullptr
			&& Recovery != nullptr
			&& Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Entry),
				*Case.Describe(TEXT("reference resolution entry should prepare"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference resolution entry should finish"))));
			ASSERT_THAT(AreEqual(
				IsCovariantSet(Candidate)
					? 1
					: Marker,
				static_cast<int32>(
					Context->GetReturnDWord()),
				*Case.Describe(TEXT("reference resolution entry should return the selected marker or covariance invariant"))));
			ASSERT_THAT(AreEqual(
				1,
				ResolutionState.Calls,
				*Case.Describe(TEXT("reference resolution should invoke exactly one selected candidate"))));
			ASSERT_THAT(AreEqual(
				Marker,
				ResolutionState.SelectedMarker,
				*Case.Describe(TEXT("reference resolution runtime callback should identify the exact selected candidate"))));
			if (Source.Source
				!= EResolutionSource::Null)
			{
				ASSERT_THAT(IsTrue(
					IsNumericSet(Candidate)
						|| ResolutionState.SelectedIdentity > 0,
					*Case.Describe(TEXT("reference resolution should observe the selected source identity for object candidates"))));
			}
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference resolution entry should unprepare"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("reference resolution recovery should prepare on the same context"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("reference resolution recovery should finish"))));
			ASSERT_THAT(AreEqual(
				923,
				static_cast<int32>(
					Context->GetReturnDWord()),
				*Case.Describe(TEXT("reference resolution recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(
				asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("reference resolution recovery should unprepare"))));
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void RunCell(
		const FCandidateCase& Candidate,
		const FSourceCase& Source,
		const FSiteCase& Site)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-REF-RESOLUTION",
			{
				ANSI_TO_TCHAR(Candidate.CatalogName),
				ANSI_TO_TCHAR(Site.CatalogName),
				ANSI_TO_TCHAR(Source.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine =
			Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("reference resolution cell should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		FReferenceState ReferenceState;
		ReferenceState.ResetCounters();
		ASSERT_THAT(IsTrue(
			RegisterReferenceFixtures(
				*ScriptEngine,
				ReferenceState),
			*Case.Describe(TEXT("reference resolution cell should register core reference fixtures"))));
		FResolutionState ResolutionState;
		ResolutionState.Reset();
		TArray<FRegisteredCandidate> Candidates;
		const bool bCandidatesRegistered =
			RegisterCandidateSet(
				*ScriptEngine,
				ResolutionState,
				Candidate,
				Candidates);
		if (IsNumericSet(Candidate))
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[AS-REF-REGISTRATION] Id=%s Candidate=%s Registered=%s Details={%s} Messages={%s}"),
				*Case.GetId(),
				ANSI_TO_TCHAR(Candidate.CatalogName),
				bCandidatesRegistered ? TEXT("true") : TEXT("false"),
				*ResolutionState.RegistrationDetails,
				*Engine.GetMessagesText()));
		}
		ASSERT_THAT(IsTrue(
			bCandidatesRegistered,
			*Case.DescribeResult(
				TEXT("reference resolution candidate registration"),
				TEXT("successful registration of the exact candidate family"),
				ResolutionState.RegistrationDetails
					+ Engine.GetMessagesText())));
		VerifyCandidateMetadata(
			Case,
			Candidates,
			*TestRunner);
		const FString ModuleName = FString::Printf(
			TEXT("ReferenceResolution_%s"),
			*Case.GetId());
		const FString Script =
			BuildReferenceResolutionSource(
				Candidate,
				Source,
				Site);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId(),
				ModuleName,
				Script,
				Module);
		if (IsCompileFailure(
			Candidate,
			Source,
			Site))
		{
			ASSERT_THAT(IsTrue(
				BuildResult < 0,
				*Case.DescribeResult(
					TEXT("reference resolution rejection source"),
					TEXT("negative build result"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			ASSERT_THAT(IsTrue(
				HasDiagnosticContaining(
					Engine,
					TEXT("SelectReference"))
					|| HasDiagnosticContaining(
						Engine,
						TEXT("const"))
					|| HasDiagnosticContaining(
						Engine,
						TEXT("ambiguous"))
					|| HasDiagnosticContaining(
						Engine,
						TEXT("candidate")),
				*Case.DescribeResult(
					TEXT("reference resolution rejection diagnostic"),
					TEXT("located call or qualifier diagnostic"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			ASSERT_THAT(AreEqual(
				0,
				ResolutionState.Calls,
				*Case.Describe(TEXT("reference resolution rejection should execute no candidate"))));
			DiscardReferenceModule(
				*ScriptEngine,
				ModuleName);
			CompileFailedBuildRecovery(
				Case,
				Engine,
				*ScriptEngine,
				ModuleName);
		}
		else
		{
			ASSERT_THAT(IsTrue(
				BuildResult >= 0,
				*Case.DescribeResult(
					TEXT("reference resolution legal source"),
					TEXT("successful build"),
					DescribeReferenceBuild(
						Engine,
						BuildResult))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("reference resolution legal source should publish a module"))));
			ASSERT_THAT(IsFalse(
				HasAnyError(Engine),
				*Case.Describe(TEXT("reference resolution legal source should emit no errors"))));
			if (Module != nullptr)
			{
				RecordGenericParameterLayout(
					*TestRunner,
					Candidates);
				const int32 Marker =
					ExpectedMarker(
						Candidate,
						Source);
				asIScriptFunction* const Expected =
					CandidateByMarker(
						Candidates,
						Marker);
				asIScriptFunction* const Witness =
					SelectionWitness(
						*Module,
						Source,
						Site);
				ASSERT_THAT(IsNotNull(Expected,
					*Case.Describe(TEXT("reference resolution should identify its expected candidate metadata"))));
				ASSERT_THAT(IsNotNull(Witness,
					*Case.Describe(TEXT("reference resolution should publish a bytecode-bearing call-site witness"))));
				if (Expected != nullptr
					&& Witness != nullptr)
				{
					const FString ExpectedDeclaration =
						UTF8_TO_TCHAR(Expected->GetDeclaration(
							true,
							false,
							true));
					const FString WitnessDeclaration =
						UTF8_TO_TCHAR(Witness->GetDeclaration(
							true,
							false,
							true));
					ASSERT_THAT(IsTrue(
						BytecodeCallsCandidate(
							*Witness,
							*Expected),
						*Case.DescribeResult(
							TEXT("reference resolution bytecode should name the exact selected candidate"),
							FString::Printf(
								TEXT("Expected=%s (Id=%d), Witness=%s (Id=%d)"),
								*ExpectedDeclaration,
								Expected->GetId(),
								*WitnessDeclaration,
								Witness->GetId()),
							DescribeBytecodeCalls(
								*Witness))));
				}
				ExecuteSelection(
					Case,
					Candidate,
					Source,
					Marker,
					ResolutionState,
					*ScriptEngine,
					*Module);
			}
			DiscardReferenceModule(
				*ScriptEngine,
				ModuleName);
		}
		ReferenceState.BreakAllCycles();
		ReferenceState.ReleaseRetainedNativeObject();
		ASSERT_THAT(AreEqual(
			0,
			ReferenceState.LiveObjects,
			*Case.DescribeResult(
				TEXT("reference resolution cleanup"),
				TEXT("Live=0 after module discard"),
				DescribeReferenceState(ReferenceState))));
		ASSERT_THAT(AreEqual(
			ReferenceState.Created,
			ReferenceState.Destroyed,
			*Case.Describe(TEXT("reference resolution cell should destroy every created identity"))));
	}

public:
	TEST_METHOD(CandidateSetsBySourceAndSite)
	{
		AS_NATIVE_PRODUCT("LANG-REF-RESOLUTION",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile
				| AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic
				| AngelscriptNativeTestSupport::ENativeEvidence::Runtime
				| AngelscriptNativeTestSupport::ENativeEvidence::Metadata
				| AngelscriptNativeTestSupport::ENativeEvidence::Lifecycle
				| AngelscriptNativeTestSupport::ENativeEvidence::Cleanup);

		for (const FCandidateCase& Candidate
			: CandidateCases)
		{
			for (const FSiteCase& Site : SiteCases)
			{
				for (const FSourceCase& Source
					: SourceCases)
				{
					RunCell(
						Candidate,
						Source,
						Site);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
