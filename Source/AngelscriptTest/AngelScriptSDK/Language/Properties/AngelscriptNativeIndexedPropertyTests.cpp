#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS_AND_TAGS(FIndexedPropertyTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.IndexedProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled,
	TEXT("#as-v238-backport"))
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;

	enum class EOperation : uint8
	{
		Read,
		Write,
		Compound,
	};

	enum class ECandidateSet : uint8
	{
		SameType,
		AdjacentNumeric,
		CrossFamily,
		CompetingPrimaryFirst,
		CompetingSecondaryFirst,
		Unrelated,
	};

	enum class EIndexAbi : uint8
	{
		Int8,
		Int16,
		Int32,
		Int64,
		UInt8,
		UInt16,
		UInt32,
		UInt64,
		Float32,
		Float64,
		Bool,
		Enum,
		Typedef,
		ReferenceToken,
	};

	struct FIndexCandidateCase
	{
		const ANSICHAR* Type;
		EIndexAbi Abi;
		bool bMatchesSource;
	};

	struct FIndexTypeCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* ScriptType;
		const ANSICHAR* ScriptLiteral;
		int32 ExpectedArgumentValue;
		FIndexCandidateCase Same;
		FIndexCandidateCase Adjacent;
		FIndexCandidateCase CrossFamily;
		FIndexCandidateCase Primary;
		FIndexCandidateCase Secondary;
	};

	struct FCandidateSetCase
	{
		const ANSICHAR* CatalogName;
		ECandidateSet Set;
	};

	struct FOperationCase
	{
		const ANSICHAR* CatalogName;
		EOperation Operation;
	};

	struct FCandidateDeclaration
	{
		FString IndexType;
		EIndexAbi Abi = EIndexAbi::Int32;
		int32 Marker = 0;
		bool bMatchesSource = false;
	};

	struct FRegisteredCandidate
	{
		FCandidateDeclaration Candidate;
		int32 GetterFunctionId = asNO_FUNCTION;
		int32 SetterFunctionId = asNO_FUNCTION;
	};

	struct FRegistrationResult
	{
		TArray<FRegisteredCandidate> Candidates;
		bool bSucceeded = false;
	};

	struct FIndexedState
	{
		TMap<int32, int32> MarkerByFunctionId;
		TMap<int32, EIndexAbi> AbiByFunctionId;
		TMap<int32, FString> DeclarationByFunctionId;
		TArray<int32> Trace;
		int32 LastFunctionId = asNO_FUNCTION;
		int32 LastArgumentTypeId = asTYPEID_VOID;
		int32 LastArgumentValue = 0;
		FString LastDeclaration;
		int32 LiveObjects = 0;
		int32 CreatedObjects = 0;
		int32 DestroyedObjects = 0;
	};

	class FIndexedPropertyObject
	{
	public:
		explicit FIndexedPropertyObject(FIndexedState& InState)
			: State(InState)
		{
			++State.LiveObjects;
			++State.CreatedObjects;
		}

		void AddRef()
		{
			++ReferenceCount;
		}

		void Release()
		{
			--ReferenceCount;
			if (ReferenceCount == 0)
			{
				delete this;
			}
		}

		FIndexedState& State;
		int32 StoredValue = 17;
		int32 LastMarker = 0;
		int32 LastIndex = 0;

	private:
		~FIndexedPropertyObject()
		{
			--State.LiveObjects;
			++State.DestroyedObjects;
		}

		int32 ReferenceCount = 1;
	};

	inline static constexpr FIndexTypeCase IndexTypeCases[] =
	{
		{
			"int8", "int8", "3", 3,
			{ "int8", EIndexAbi::Int8, true },
			{ "int16", EIndexAbi::Int16, true },
			{ "uint8", EIndexAbi::UInt8, true },
			{ "int16", EIndexAbi::Int16, true },
			{ "uint8", EIndexAbi::UInt8, true },
		},
		{
			"int16", "int16", "3", 3,
			{ "int16", EIndexAbi::Int16, true },
			{ "int", EIndexAbi::Int32, true },
			{ "uint16", EIndexAbi::UInt16, true },
			{ "int", EIndexAbi::Int32, true },
			{ "uint16", EIndexAbi::UInt16, true },
		},
		{
			"int", "int", "3", 3,
			{ "int", EIndexAbi::Int32, true },
			{ "int64", EIndexAbi::Int64, true },
			{ "uint", EIndexAbi::UInt32, true },
			{ "int64", EIndexAbi::Int64, true },
			{ "uint", EIndexAbi::UInt32, true },
		},
		{
			"int64", "int64", "3", 3,
			{ "int64", EIndexAbi::Int64, true },
			{ "int", EIndexAbi::Int32, true },
			{ "uint64", EIndexAbi::UInt64, true },
			{ "int", EIndexAbi::Int32, true },
			{ "uint64", EIndexAbi::UInt64, true },
		},
		{
			"uint8", "uint8", "3", 3,
			{ "uint8", EIndexAbi::UInt8, true },
			{ "uint16", EIndexAbi::UInt16, true },
			{ "int8", EIndexAbi::Int8, true },
			{ "uint16", EIndexAbi::UInt16, true },
			{ "int8", EIndexAbi::Int8, true },
		},
		{
			"uint16", "uint16", "3", 3,
			{ "uint16", EIndexAbi::UInt16, true },
			{ "uint", EIndexAbi::UInt32, true },
			{ "int16", EIndexAbi::Int16, true },
			{ "uint", EIndexAbi::UInt32, true },
			{ "int16", EIndexAbi::Int16, true },
		},
		{
			"uint", "uint", "3", 3,
			{ "uint", EIndexAbi::UInt32, true },
			{ "uint64", EIndexAbi::UInt64, true },
			{ "int", EIndexAbi::Int32, true },
			{ "uint64", EIndexAbi::UInt64, true },
			{ "int", EIndexAbi::Int32, true },
		},
		{
			"uint64", "uint64", "3", 3,
			{ "uint64", EIndexAbi::UInt64, true },
			{ "uint", EIndexAbi::UInt32, true },
			{ "int64", EIndexAbi::Int64, true },
			{ "uint", EIndexAbi::UInt32, true },
			{ "int64", EIndexAbi::Int64, true },
		},
		{
			"float32", "float", "3.0f", 3,
			{ "float", EIndexAbi::Float32, true },
			{ "double", EIndexAbi::Float64, true },
			{ "int", EIndexAbi::Int32, true },
			{ "double", EIndexAbi::Float64, true },
			{ "int", EIndexAbi::Int32, true },
		},
		{
			"float64", "double", "3.0", 3,
			{ "double", EIndexAbi::Float64, true },
			{ "float", EIndexAbi::Float32, true },
			{ "int64", EIndexAbi::Int64, true },
			{ "float", EIndexAbi::Float32, true },
			{ "int64", EIndexAbi::Int64, true },
		},
		{
			"bool", "bool", "true", 1,
			{ "bool", EIndexAbi::Bool, true },
			{ "int", EIndexAbi::Int32, false },
			{ "double", EIndexAbi::Float64, false },
			{ "bool", EIndexAbi::Bool, true },
			{ "int", EIndexAbi::Int32, false },
		},
		{
			"enum", "ERegisteredIndex", "ERegisteredIndex::Three", 3,
			{ "ERegisteredIndex", EIndexAbi::Enum, true },
			{ "int", EIndexAbi::Int32, false },
			{ "uint", EIndexAbi::UInt32, false },
			{ "ERegisteredIndex", EIndexAbi::Enum, true },
			{ "int", EIndexAbi::Int32, false },
		},
		{
			"typedef", "RegisteredIndexAlias", "3", 3,
			{ "RegisteredIndexAlias", EIndexAbi::Typedef, true },
			{ "int64", EIndexAbi::Int64, true },
			{ "uint", EIndexAbi::UInt32, true },
			{ "RegisteredIndexAlias", EIndexAbi::Typedef, true },
			{ "int64", EIndexAbi::Int64, true },
		},
	};

	inline static constexpr FCandidateSetCase CandidateSetCases[] =
	{
		{ "same_type", ECandidateSet::SameType },
		{ "adjacent_numeric", ECandidateSet::AdjacentNumeric },
		{ "cross_family", ECandidateSet::CrossFamily },
		{ "competing_primary_first", ECandidateSet::CompetingPrimaryFirst },
		{ "competing_secondary_first", ECandidateSet::CompetingSecondaryFirst },
		{ "unrelated", ECandidateSet::Unrelated },
	};

	inline static constexpr FOperationCase OperationCases[] =
	{
		{ "read", EOperation::Read },
		{ "write", EOperation::Write },
		{ "compound", EOperation::Compound },
	};

	inline static FIndexedState* ActiveState = nullptr;

	static bool IsCompeting(const FCandidateSetCase& CandidateSetCase)
	{
		return CandidateSetCase.Set == ECandidateSet::CompetingPrimaryFirst
			|| CandidateSetCase.Set == ECandidateSet::CompetingSecondaryFirst;
	}

	static bool NeedsGetter(const FOperationCase& OperationCase)
	{
		return OperationCase.Operation == EOperation::Read
			|| OperationCase.Operation == EOperation::Compound;
	}

	static bool NeedsSetter(const FOperationCase& OperationCase)
	{
		return OperationCase.Operation == EOperation::Write
			|| OperationCase.Operation == EOperation::Compound;
	}

	static TArray<FCandidateDeclaration> MakeCandidateDeclarations(
		const FIndexTypeCase& IndexTypeCase,
		const FCandidateSetCase& CandidateSetCase)
	{
		TArray<FCandidateDeclaration> Result;
		switch (CandidateSetCase.Set)
		{
		case ECandidateSet::SameType:
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.Same.Type),
				IndexTypeCase.Same.Abi,
				101,
				IndexTypeCase.Same.bMatchesSource,
			});
			break;
		case ECandidateSet::AdjacentNumeric:
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.Adjacent.Type),
				IndexTypeCase.Adjacent.Abi,
				201,
				IndexTypeCase.Adjacent.bMatchesSource,
			});
			break;
		case ECandidateSet::CrossFamily:
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.CrossFamily.Type),
				IndexTypeCase.CrossFamily.Abi,
				301,
				IndexTypeCase.CrossFamily.bMatchesSource,
			});
			break;
		case ECandidateSet::CompetingPrimaryFirst:
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.Primary.Type),
				IndexTypeCase.Primary.Abi,
				401,
				IndexTypeCase.Primary.bMatchesSource,
			});
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.Secondary.Type),
				IndexTypeCase.Secondary.Abi,
				402,
				IndexTypeCase.Secondary.bMatchesSource,
			});
			break;
		case ECandidateSet::CompetingSecondaryFirst:
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.Secondary.Type),
				IndexTypeCase.Secondary.Abi,
				402,
				IndexTypeCase.Secondary.bMatchesSource,
			});
			Result.Add({
				ANSI_TO_TCHAR(IndexTypeCase.Primary.Type),
				IndexTypeCase.Primary.Abi,
				401,
				IndexTypeCase.Primary.bMatchesSource,
			});
			break;
		case ECandidateSet::Unrelated:
			Result.Add({ TEXT("FRegisteredIndexToken@"), EIndexAbi::ReferenceToken, 501, false });
			break;
		default:
			checkNoEntry();
			break;
		}
		return Result;
	}

	static void AddRefIndexedProperty(FIndexedPropertyObject* Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	static void ReleaseIndexedProperty(FIndexedPropertyObject* Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	static void CreateIndexedProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || ActiveState == nullptr)
		{
			return;
		}
		Generic->SetReturnAddress(new FIndexedPropertyObject(*ActiveState));
	}

	static void ObserveIndexedProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FIndexedPropertyObject* const Object = static_cast<FIndexedPropertyObject*>(Generic->GetArgObject(0));
		const int32 Encoded = Object != nullptr
			? Object->LastMarker * 10000 + Object->LastIndex * 100 + Object->StoredValue
			: 0;
		Generic->SetReturnDWord(static_cast<asDWORD>(Encoded));
	}

	static int32 DecodeIndexArgument(asIScriptGeneric& Generic, const EIndexAbi Abi)
	{
		switch (Abi)
		{
		case EIndexAbi::Int8:
			return static_cast<int32>(static_cast<int8>(Generic.GetArgByte(0)));
		case EIndexAbi::Int16:
			return static_cast<int32>(static_cast<int16>(Generic.GetArgWord(0)));
		case EIndexAbi::Int32:
			return static_cast<int32>(Generic.GetArgDWord(0));
		case EIndexAbi::Int64:
			return static_cast<int32>(static_cast<int64>(Generic.GetArgQWord(0)));
		case EIndexAbi::UInt8:
			return static_cast<int32>(Generic.GetArgByte(0));
		case EIndexAbi::UInt16:
			return static_cast<int32>(Generic.GetArgWord(0));
		case EIndexAbi::UInt32:
			return static_cast<int32>(Generic.GetArgDWord(0));
		case EIndexAbi::UInt64:
			return static_cast<int32>(Generic.GetArgQWord(0));
		case EIndexAbi::Float32:
			return FMath::RoundToInt(Generic.GetArgFloat(0));
		case EIndexAbi::Float64:
			return FMath::RoundToInt(Generic.GetArgDouble(0));
		case EIndexAbi::Bool:
			return Generic.GetArgByte(0) != 0 ? 1 : 0;
		case EIndexAbi::Enum:
		case EIndexAbi::Typedef:
			return static_cast<int32>(Generic.GetArgDWord(0));
		case EIndexAbi::ReferenceToken:
		default:
			return 0;
		}
	}

	static bool RecordIndexedCall(
		asIScriptGeneric& Generic,
		FIndexedPropertyObject& Object,
		int32& OutMarker,
		int32& OutIndex)
	{
		if (ActiveState == nullptr || Generic.GetFunction() == nullptr)
		{
			return false;
		}

		const int32 FunctionId = Generic.GetFunction()->GetId();
		const int32* const Marker = ActiveState->MarkerByFunctionId.Find(FunctionId);
		const EIndexAbi* const Abi = ActiveState->AbiByFunctionId.Find(FunctionId);
		const FString* const Declaration = ActiveState->DeclarationByFunctionId.Find(FunctionId);
		if (Marker == nullptr || Abi == nullptr || Declaration == nullptr)
		{
			return false;
		}

		OutMarker = *Marker;
		OutIndex = DecodeIndexArgument(Generic, *Abi);
		Object.LastMarker = OutMarker;
		Object.LastIndex = OutIndex;
		ActiveState->LastFunctionId = FunctionId;
		ActiveState->LastArgumentTypeId = Generic.GetArgTypeId(0);
		ActiveState->LastArgumentValue = OutIndex;
		ActiveState->LastDeclaration = *Declaration;
		ActiveState->Trace.Add(OutMarker);
		return true;
	}

	static void GetIndexedProperty(asIScriptGeneric* Generic)
	{
		FIndexedPropertyObject* const Object = Generic != nullptr
			? static_cast<FIndexedPropertyObject*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr)
		{
			return;
		}

		int32 Marker = 0;
		int32 Index = 0;
		if (!RecordIndexedCall(*Generic, *Object, Marker, Index))
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Indexed getter registration state missing");
			}
			return;
		}
		Generic->SetReturnDWord(static_cast<asDWORD>(Marker * 100 + Index));
	}

	static void SetIndexedProperty(asIScriptGeneric* Generic)
	{
		FIndexedPropertyObject* const Object = Generic != nullptr
			? static_cast<FIndexedPropertyObject*>(Generic->GetObject())
			: nullptr;
		if (Generic == nullptr || Object == nullptr)
		{
			return;
		}

		int32 Marker = 0;
		int32 Index = 0;
		if (!RecordIndexedCall(*Generic, *Object, Marker, Index))
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Indexed setter registration state missing");
			}
			return;
		}
		Object->StoredValue = static_cast<int32>(Generic->GetArgDWord(1));
	}

	static bool RegisterSharedFixture(asIScriptEngine& ScriptEngine)
	{
		return ScriptEngine.RegisterEnum("ERegisteredIndex") >= 0
			&& ScriptEngine.RegisterEnumValue("ERegisteredIndex", "Three", 3) >= 0
			&& ScriptEngine.RegisterTypedef("RegisteredIndexAlias", "int") >= 0
			&& ScriptEngine.RegisterObjectType(
				"FRegisteredIndexToken",
				0,
				asOBJ_REF | asOBJ_NOCOUNT) >= 0
			&& ScriptEngine.RegisterObjectType("FIndexedProperty", 0, asOBJ_REF) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FIndexedProperty",
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(AddRefIndexedProperty),
				asCALL_CDECL_OBJFIRST) >= 0
			&& ScriptEngine.RegisterObjectBehaviour(
				"FIndexedProperty",
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ReleaseIndexedProperty),
				asCALL_CDECL_OBJFIRST) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FIndexedProperty CreateIndexedProperty()",
				asFUNCTION(CreateIndexedProperty),
				asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"int ObserveIndexedProperty(const FIndexedProperty& in Receiver)",
				asFUNCTION(ObserveIndexedProperty),
				asCALL_GENERIC) >= 0;
	}

	static FRegistrationResult RegisterCandidateFixture(
		asIScriptEngine& ScriptEngine,
		FIndexedState& State,
		const FIndexTypeCase& IndexTypeCase,
		const FCandidateSetCase& CandidateSetCase,
		const FOperationCase& OperationCase)
	{
		FRegistrationResult Result;
		ActiveState = &State;
		if (!RegisterSharedFixture(ScriptEngine))
		{
			return Result;
		}

		for (const FCandidateDeclaration& Candidate : MakeCandidateDeclarations(IndexTypeCase, CandidateSetCase))
		{
			FRegisteredCandidate Registered;
			Registered.Candidate = Candidate;
			if (NeedsGetter(OperationCase))
			{
				const FString Declaration = FString::Printf(
					TEXT("int get_Value(%s Index) const property"),
					*Candidate.IndexType);
				const FTCHARToUTF8 DeclarationUtf8(*Declaration);
				Registered.GetterFunctionId = ScriptEngine.RegisterObjectMethod(
					"FIndexedProperty",
					DeclarationUtf8.Get(),
					asFUNCTION(GetIndexedProperty),
					asCALL_GENERIC);
				if (Registered.GetterFunctionId < 0)
				{
					return Result;
				}
				State.MarkerByFunctionId.Add(Registered.GetterFunctionId, Candidate.Marker);
				State.AbiByFunctionId.Add(Registered.GetterFunctionId, Candidate.Abi);
				State.DeclarationByFunctionId.Add(Registered.GetterFunctionId, Declaration);
			}
			if (NeedsSetter(OperationCase))
			{
				const FString Declaration = FString::Printf(
					TEXT("void set_Value(%s Index, int Value) property"),
					*Candidate.IndexType);
				const FTCHARToUTF8 DeclarationUtf8(*Declaration);
				Registered.SetterFunctionId = ScriptEngine.RegisterObjectMethod(
					"FIndexedProperty",
					DeclarationUtf8.Get(),
					asFUNCTION(SetIndexedProperty),
					asCALL_GENERIC);
				if (Registered.SetterFunctionId < 0)
				{
					return Result;
				}
				State.MarkerByFunctionId.Add(Registered.SetterFunctionId, Candidate.Marker);
				State.AbiByFunctionId.Add(Registered.SetterFunctionId, Candidate.Abi);
				State.DeclarationByFunctionId.Add(Registered.SetterFunctionId, Declaration);
			}
			Result.Candidates.Add(MoveTemp(Registered));
		}

		Result.bSucceeded = true;
		return Result;
	}

	static void AppendIndexedOperation(
		FString& Source,
		const FOperationCase& OperationCase)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (OperationCase.Operation)
		{
		case EOperation::Read:
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value[Index];"));
			break;
		case EOperation::Write:
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value[Index] = 73;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveIndexedProperty(Receiver);"));
			break;
		case EOperation::Compound:
			AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value[Index] += 5;"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveIndexedProperty(Receiver);"));
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	static FString BuildIndexedPropertySource(
		const FIndexTypeCase& IndexTypeCase,
		const FOperationCase& OperationCase,
		const bool bConstReceiver)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunIndexedProperty()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, bConstReceiver
			? TEXT("\tconst FIndexedProperty Receiver = CreateIndexedProperty();")
			: TEXT("\tFIndexedProperty Receiver = CreateIndexedProperty();"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%hs Index = %hs;"),
			IndexTypeCase.ScriptType,
			IndexTypeCase.ScriptLiteral));
		AppendIndexedOperation(Source, OperationCase);
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunIndexedPropertyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 89;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunIndexedPropertyRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 89;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString MakeModuleName(
		const FIndexTypeCase& IndexTypeCase,
		const FCandidateSetCase& CandidateSetCase,
		const FOperationCase& OperationCase,
		const bool bConstReceiver)
	{
		return FString::Printf(
			TEXT("IndexedProperty_%hs_%hs_%hs_%s"),
			CandidateSetCase.CatalogName,
			IndexTypeCase.CatalogName,
			OperationCase.CatalogName,
			bConstReceiver ? TEXT("const") : TEXT("mutable"));
	}

	static bool IsLocatedErrorContaining(
		const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry,
		const FString& Fragment)
	{
		return Entry.Type == asMSGTYPE_ERROR
			&& Entry.Row > 0
			&& Entry.Column > 0
			&& !Entry.Section.IsEmpty()
			&& Entry.Message.Contains(Fragment);
	}

	static TArray<FString> ExpectedDiagnostics(
		const FCandidateSetCase& CandidateSetCase,
		const FOperationCase& OperationCase,
		const bool bConstReceiver,
		const TArray<FCandidateDeclaration>& Candidates)
	{
		TArray<FString> Result;
		if (IsCompeting(CandidateSetCase)
			&& (OperationCase.Operation == EOperation::Read
				|| OperationCase.Operation == EOperation::Compound))
		{
			Result.Add(TEXT("Found multiple get accessors for property 'Value'"));
			return Result;
		}

		const bool bSelectedCandidateMatches = Candidates.Num() > 0
			&& Candidates[0].bMatchesSource;
		if (!bSelectedCandidateMatches)
		{
			Result.Add(TEXT("No matching signatures"));
			Result.Add(OperationCase.Operation == EOperation::Read
				? TEXT("get_Value")
				: TEXT("set_Value"));
		}

		if (bConstReceiver
			&& OperationCase.Operation != EOperation::Read
			&& bSelectedCandidateMatches)
		{
			Result.Add(TEXT("Non-const method call on read-only object reference"));
		}

		if (OperationCase.Operation == EOperation::Compound)
		{
			Result.Add(TEXT("Compound assignments with indexed property accessors are not supported"));
		}
		return Result;
	}

	static bool ShouldCompileAndExecute(
		const FCandidateSetCase& CandidateSetCase,
		const FOperationCase& OperationCase,
		const bool bConstReceiver,
		const TArray<FCandidateDeclaration>& Candidates)
	{
		if (OperationCase.Operation == EOperation::Compound)
		{
			return false;
		}
		if (OperationCase.Operation == EOperation::Read && IsCompeting(CandidateSetCase))
		{
			return false;
		}
		if (Candidates.Num() == 0 || !Candidates[0].bMatchesSource)
		{
			return false;
		}
		return OperationCase.Operation != EOperation::Write || !bConstReceiver;
	}

	void VerifyRegistrationMetadata(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		const FOperationCase& OperationCase,
		const FRegistrationResult& Registration)
	{
		ASSERT_THAT(AreEqual(int64(3), ScriptEngine.GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			*Case.Describe(TEXT("bare SDK engine should retain registered-property mode 3"))));
		asITypeInfo* const Type = ScriptEngine.GetTypeInfoByDecl("FIndexedProperty");
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("indexed property fixture should publish its reference type"))));

		for (const FRegisteredCandidate& Registered : Registration.Candidates)
		{
			const FTCHARToUTF8 IndexTypeUtf8(*Registered.Candidate.IndexType);
			const int32 ExpectedIndexTypeId = ScriptEngine.GetTypeIdByDecl(IndexTypeUtf8.Get());
			ASSERT_THAT(IsTrue(ExpectedIndexTypeId >= 0,
				*Case.Describe(TEXT("registered index declaration should resolve its exact type ID"))));

			if (NeedsGetter(OperationCase))
			{
				asIScriptFunction* const Getter = ScriptEngine.GetFunctionById(Registered.GetterFunctionId);
				ASSERT_THAT(IsNotNull(Getter,
					*Case.Describe(TEXT("registered indexed getter should retain its function identity"))));
				if (Getter != nullptr)
				{
					int ParamTypeId = asTYPEID_VOID;
					asDWORD ParamFlags = asTM_NONE;
					ASSERT_THAT(AreEqual(asSUCCESS, Getter->GetParam(0, &ParamTypeId, &ParamFlags),
						*Case.Describe(TEXT("indexed getter should expose its index parameter"))));
					ASSERT_THAT(IsTrue(Getter->IsProperty(),
						*Case.Describe(TEXT("indexed getter should retain its property trait"))));
					ASSERT_THAT(IsTrue(Getter->IsReadOnly(),
						*Case.Describe(TEXT("indexed getter should be callable through const receivers"))));
					ASSERT_THAT(AreEqual(1, static_cast<int32>(Getter->GetParamCount()),
						*Case.Describe(TEXT("indexed getter should expose one index parameter"))));
					ASSERT_THAT(AreEqual(ExpectedIndexTypeId, ParamTypeId,
						*Case.Describe(TEXT("indexed getter metadata should retain the candidate type"))));
					ASSERT_THAT(AreEqual(static_cast<uint32>(asTM_NONE), static_cast<uint32>(ParamFlags),
						*Case.Describe(TEXT("indexed getter parameter should pass by value"))));
					ASSERT_THAT(AreEqual(asTYPEID_INT32, Getter->GetReturnTypeId(),
						*Case.Describe(TEXT("indexed getter should return int"))));
				}
			}

			if (NeedsSetter(OperationCase))
			{
				asIScriptFunction* const Setter = ScriptEngine.GetFunctionById(Registered.SetterFunctionId);
				ASSERT_THAT(IsNotNull(Setter,
					*Case.Describe(TEXT("registered indexed setter should retain its function identity"))));
				if (Setter != nullptr)
				{
					int IndexParamTypeId = asTYPEID_VOID;
					int ValueParamTypeId = asTYPEID_VOID;
					asDWORD IndexParamFlags = asTM_NONE;
					asDWORD ValueParamFlags = asTM_NONE;
					ASSERT_THAT(AreEqual(asSUCCESS, Setter->GetParam(0, &IndexParamTypeId, &IndexParamFlags),
						*Case.Describe(TEXT("indexed setter should expose its index parameter"))));
					ASSERT_THAT(AreEqual(asSUCCESS, Setter->GetParam(1, &ValueParamTypeId, &ValueParamFlags),
						*Case.Describe(TEXT("indexed setter should expose its value parameter"))));
					ASSERT_THAT(IsTrue(Setter->IsProperty(),
						*Case.Describe(TEXT("indexed setter should retain its property trait"))));
					ASSERT_THAT(IsFalse(Setter->IsReadOnly(),
						*Case.Describe(TEXT("indexed setter should remain mutable"))));
					ASSERT_THAT(AreEqual(2, static_cast<int32>(Setter->GetParamCount()),
						*Case.Describe(TEXT("indexed setter should expose index and value parameters"))));
					ASSERT_THAT(AreEqual(ExpectedIndexTypeId, IndexParamTypeId,
						*Case.Describe(TEXT("indexed setter metadata should retain the candidate type"))));
					ASSERT_THAT(AreEqual(asTYPEID_INT32, ValueParamTypeId,
						*Case.Describe(TEXT("indexed setter value parameter should remain int"))));
					ASSERT_THAT(AreEqual(static_cast<uint32>(asTM_NONE), static_cast<uint32>(IndexParamFlags),
						*Case.Describe(TEXT("indexed setter index should pass by value"))));
					ASSERT_THAT(AreEqual(static_cast<uint32>(asTM_NONE), static_cast<uint32>(ValueParamFlags),
						*Case.Describe(TEXT("indexed setter value should pass by value"))));
					ASSERT_THAT(AreEqual(asTYPEID_VOID, Setter->GetReturnTypeId(),
						*Case.Describe(TEXT("indexed setter should return void"))));
				}
			}
		}
	}

	void ExecuteCompiledCell(
		const FNativeCaseContext& Case,
		const FIndexTypeCase& IndexTypeCase,
		const FOperationCase& OperationCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FRegistrationResult& Registration,
		FIndexedState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunIndexedProperty()");
		asIScriptFunction* const Recovery = Module.GetFunctionByDecl("int RunIndexedPropertyRecovery()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("compiled indexed property cell should expose its exact entry"))));
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("compiled indexed property cell should expose same-context recovery"))));
		if (Entry == nullptr || Recovery == nullptr || Registration.Candidates.IsEmpty())
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("indexed property cell should create a context"))));
		if (Context == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Entry),
			*Case.Describe(TEXT("indexed property context should prepare its entry"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("selected indexed property accessor should execute"))));

		const FRegisteredCandidate& Selected = Registration.Candidates[0];
		const int32 ExpectedFunctionId = OperationCase.Operation == EOperation::Read
			? Selected.GetterFunctionId
			: Selected.SetterFunctionId;
		const FString* const ExpectedDeclaration = State.DeclarationByFunctionId.Find(ExpectedFunctionId);
		const int32 ExpectedReturn = OperationCase.Operation == EOperation::Read
			? Selected.Candidate.Marker * 100 + IndexTypeCase.ExpectedArgumentValue
			: Selected.Candidate.Marker * 10000 + IndexTypeCase.ExpectedArgumentValue * 100 + 73;
		ASSERT_THAT(AreEqual(ExpectedReturn, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("indexed accessor runtime result should encode selected declaration, index, and stored value"))));
		ASSERT_THAT(AreEqual(1, State.Trace.Num(),
			*Case.Describe(TEXT("indexed accessor should invoke exactly one native candidate"))));
		if (State.Trace.Num() == 1)
		{
			ASSERT_THAT(AreEqual(Selected.Candidate.Marker, State.Trace[0],
				*Case.Describe(TEXT("indexed accessor callback marker should identify the selected candidate"))));
		}
		ASSERT_THAT(AreEqual(ExpectedFunctionId, State.LastFunctionId,
			*Case.Describe(TEXT("indexed accessor callback should retain the exact selected function ID"))));
		ASSERT_THAT(AreEqual(IndexTypeCase.ExpectedArgumentValue, State.LastArgumentValue,
			*Case.Describe(TEXT("indexed accessor callback should observe the converted index value through its native ABI"))));
		ASSERT_THAT(IsTrue(State.LastArgumentTypeId >= 0,
			*Case.Describe(TEXT("indexed accessor callback should expose its concrete argument type ID"))));
		ASSERT_THAT(IsNotNull(ExpectedDeclaration,
			*Case.Describe(TEXT("selected indexed declaration should remain registered in fixture state"))));
		if (ExpectedDeclaration != nullptr)
		{
			ASSERT_THAT(AreEqual(*ExpectedDeclaration, State.LastDeclaration,
				*Case.Describe(TEXT("indexed accessor callback should retain the exact selected declaration"))));
		}

		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("indexed property context should release entry locals"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(Recovery),
			*Case.Describe(TEXT("indexed property context should prepare recovery"))));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(),
			*Case.Describe(TEXT("indexed property context should execute recovery"))));
		ASSERT_THAT(AreEqual(89, static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("indexed property recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(1, State.Trace.Num(),
			*Case.Describe(TEXT("indexed property recovery should invoke no accessor callback"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("indexed property recovery should unprepare cleanly"))));
		Context->Release();
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*Case.Describe(TEXT("indexed property context cleanup should release every receiver"))));
		ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
			*Case.Describe(TEXT("indexed property receiver construction and destruction should balance"))));
	}

	void CompileRecovery(
		const FNativeCaseContext& Case,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		asIScriptEngine& ScriptEngine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(*TestRunner, Case.GetId() + TEXT("-RECOVERY"), ModuleName, RecoverySource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("rejected indexed property source should permit a same-name recovery build"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("indexed property recovery should publish a clean module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery = RecoveryModule->GetFunctionByDecl("int RunIndexedPropertyRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("indexed property recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("indexed property recovery should create a context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("indexed property recovery should execute"))));
					ASSERT_THAT(AreEqual(89, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("indexed property recovery should return its sentinel"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

	void RunCell(
		const FNativeCaseContext& Case,
		const FIndexTypeCase& IndexTypeCase,
		const FCandidateSetCase& CandidateSetCase,
		const FOperationCase& OperationCase,
		const bool bConstReceiver,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		FIndexedState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		ActiveState = &State;
		ON_SCOPE_EXIT
		{
			ActiveState = nullptr;
		};

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("indexed property cell should create an isolated raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		const FRegistrationResult Registration = RegisterCandidateFixture(
			*ScriptEngine,
			State,
			IndexTypeCase,
			CandidateSetCase,
			OperationCase);
		ASSERT_THAT(IsTrue(Registration.bSucceeded,
			*Case.Describe(TEXT("indexed property fixture should publish every requested native declaration"))));
		if (!Registration.bSucceeded)
		{
			return;
		}
		VerifyRegistrationMetadata(Case, *ScriptEngine, OperationCase, Registration);

		const TArray<FCandidateDeclaration> Candidates = MakeCandidateDeclarations(
			IndexTypeCase,
			CandidateSetCase);
		const bool bShouldExecute = ShouldCompileAndExecute(
			CandidateSetCase,
			OperationCase,
			bConstReceiver,
			Candidates);
		const TArray<FString> Diagnostics = ExpectedDiagnostics(
			CandidateSetCase,
			OperationCase,
			bConstReceiver,
			Candidates);
		const FString ModuleName = MakeModuleName(
			IndexTypeCase,
			CandidateSetCase,
			OperationCase,
			bConstReceiver);
		const FString Source = BuildIndexedPropertySource(
			IndexTypeCase,
			OperationCase,
			bConstReceiver);
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

		if (bShouldExecute)
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.Describe(TEXT("indexed property selection cell should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("indexed property selection cell should publish a module"))));
			if (BuildResult >= 0 && Module != nullptr)
			{
				ExecuteCompiledCell(
					Case,
					IndexTypeCase,
					OperationCase,
					*ScriptEngine,
					*Module,
					Registration,
					State);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("unsupported indexed property lookup should be rejected"))));
			for (const FString& Diagnostic : Diagnostics)
			{
				const FString Expectation = FString::Printf(
					TEXT("indexed property rejection should report located diagnostic '%s'"),
					*Diagnostic);
				ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.ContainsByPredicate([&Diagnostic](const FNativeMessageEntry& Entry)
				{
					return IsLocatedErrorContaining(Entry, Diagnostic);
				}),
					*Case.Describe(*Expectation)));
			}
			ASSERT_THAT(AreEqual(0, State.Trace.Num(),
				*Case.Describe(TEXT("compile-time indexed property rejection should invoke no native candidate"))));
			ASSERT_THAT(AreEqual(0, State.LiveObjects,
				*Case.Describe(TEXT("compile-time indexed property rejection should construct no receiver"))));
		}

		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("indexed property cell should discard its isolated module"))));
		if (!bShouldExecute)
		{
			CompileRecovery(Case, Engine, *ScriptEngine, ModuleName);
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("indexed property recovery should leave no module behind"))));
		}
	}

public:
	TEST_METHOD(IndexTypesByCandidateSetOperationAndReceiver)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-INDEXED",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		for (const FCandidateSetCase& CandidateSetCase : CandidateSetCases)
		{
			for (const FIndexTypeCase& IndexTypeCase : IndexTypeCases)
			{
				for (const FOperationCase& OperationCase : OperationCases)
				{
					for (const bool bConstReceiver : { false, true })
					{
						const FNativeCaseContext Case(MakeNativeCaseId(
							"LANG-PROP-INDEXED",
							{
								ANSI_TO_TCHAR(CandidateSetCase.CatalogName),
								ANSI_TO_TCHAR(IndexTypeCase.CatalogName),
								ANSI_TO_TCHAR(OperationCase.CatalogName),
								bConstReceiver ? TEXT("const") : TEXT("mutable"),
							}));
						FIndexedState State;
						FNativeTestEngine Engine;
						Engine.Create(*TestRunner);
						RunCell(
							Case,
							IndexTypeCase,
							CandidateSetCase,
							OperationCase,
							bConstReceiver,
							Engine,
							State);
						Engine.Destroy();
						ASSERT_THAT(AreEqual(0, State.LiveObjects,
							*Case.Describe(TEXT("destroying the isolated engine should leave no indexed property object"))));
						ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
							*Case.Describe(TEXT("isolated indexed property engine should balance all receiver objects"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
