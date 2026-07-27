#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPropertyRebuildTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Properties.Rebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FMemoryBinaryStream = AngelscriptNativeTestSupport::FMemoryBinaryStream;
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	enum class EPropertyForm : uint8
	{
		Stored,
		Registered,
		RegisteredIndexed,
	};

	struct FScenarioCase
	{
		const ANSICHAR* CatalogName;
		EPropertyForm Form;
		bool bSameSource;
	};

	struct FPathCase
	{
		const ANSICHAR* CatalogName;
		bool bSaveLoad;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FRegisteredTypeConfig
	{
		bool bGetter = true;
		bool bSetter = true;
		bool bConstGetter = true;
		const ANSICHAR* IndexType = nullptr;
		bool bExtraIndexedSetter = false;
	};

	struct FPropertyRebuildState
	{
		TArray<int32> Trace;
		int32 LiveObjects = 0;
		int32 CreatedObjects = 0;
		int32 DestroyedObjects = 0;

		void ResetTrace()
		{
			Trace.Reset();
		}
	};

	struct FObjectTypeOperandTrace
	{
		FString FunctionDeclaration;
		FString ScriptSection;
		asEBCInstr Opcode = static_cast<asEBCInstr>(0);
		asUINT DwordIndex = 0;
		asQWORD LiveOperand = 0;
	};

	class FPropertyRebuildObject
	{
	public:
		FPropertyRebuildObject(FPropertyRebuildState& InState, const int32 InValue)
			: State(InState)
			, Value(InValue)
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

		FPropertyRebuildState& State;
		int32 Value = 0;

	private:
		~FPropertyRebuildObject()
		{
			--State.LiveObjects;
			++State.DestroyedObjects;
		}

		int32 ReferenceCount = 1;
	};

	inline static constexpr FScenarioCase ScenarioCases[] =
	{
		{ "stored_same_source", EPropertyForm::Stored, true },
		{ "stored_value", EPropertyForm::Stored, false },
		{ "stored_field_type", EPropertyForm::Stored, false },
		{ "stored_field_order", EPropertyForm::Stored, false },
		{ "stored_inheritance", EPropertyForm::Stored, false },
	};

	inline static constexpr FPathCase PathCases[] =
	{
		{ "rebuild", false },
		{ "save_load", true },
	};

	inline static constexpr FObservationCase ObservationCases[] =
	{
		{ "metadata" },
		{ "runtime" },
		{ "old_handle_cleanup" },
	};

	inline static FPropertyRebuildState* ActiveState = nullptr;

	static bool IsScenario(const FScenarioCase& ScenarioCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(ScenarioCase.CatalogName, Name) == 0;
	}

	static const TCHAR* GetPropertyFormName(const EPropertyForm Form)
	{
		switch (Form)
		{
		case EPropertyForm::Stored:
			return TEXT("stored");
		case EPropertyForm::Registered:
			return TEXT("registered");
		case EPropertyForm::RegisteredIndexed:
			return TEXT("registered_indexed");
		default:
			return TEXT("unknown");
		}
	}

	static FString MakeSuffix(const FScenarioCase& ScenarioCase, const FPathCase& PathCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs"),
			ScenarioCase.CatalogName,
			PathCase.CatalogName);
	}

	static FNativeCaseContext MakeObservationCase(
		const FObservationCase& ObservationCase,
		const FPathCase& PathCase,
		const FScenarioCase& ScenarioCase)
	{
		using namespace AngelscriptNativeTestSupport;

		return FNativeCaseContext(MakeNativeCaseId(
			"LANG-PROP-REBUILD",
			{
				ANSI_TO_TCHAR(ObservationCase.CatalogName),
				ANSI_TO_TCHAR(PathCase.CatalogName),
				ANSI_TO_TCHAR(ScenarioCase.CatalogName),
			}));
	}

	static void AddRefPropertyRebuild(FPropertyRebuildObject* Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	static void ReleasePropertyRebuild(FPropertyRebuildObject* Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	static void CreatePropertyRebuild(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || ActiveState == nullptr)
		{
			return;
		}
		Generic->SetReturnAddress(new FPropertyRebuildObject(
			*ActiveState,
			static_cast<int32>(Generic->GetArgDWord(0))));
	}

	static void ObservePropertyRebuild(asIScriptGeneric* Generic)
	{
		FPropertyRebuildObject* const Object = Generic != nullptr
			? static_cast<FPropertyRebuildObject*>(Generic->GetArgObject(0))
			: nullptr;
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->Value) : 0);
		}
	}

	static void GetPropertyRebuild(asIScriptGeneric* Generic)
	{
		FPropertyRebuildObject* const Object = Generic != nullptr
			? static_cast<FPropertyRebuildObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(100);
			Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value));
		}
	}

	static void SetPropertyRebuild(asIScriptGeneric* Generic)
	{
		FPropertyRebuildObject* const Object = Generic != nullptr
			? static_cast<FPropertyRebuildObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(200);
			Object->Value = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static void GetIndexedPropertyRebuild(asIScriptGeneric* Generic)
	{
		FPropertyRebuildObject* const Object = Generic != nullptr
			? static_cast<FPropertyRebuildObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(300);
			Generic->SetReturnDWord(static_cast<asDWORD>(Object->Value + 1));
		}
	}

	static void SetIndexedPropertyRebuild(asIScriptGeneric* Generic)
	{
		FPropertyRebuildObject* const Object = Generic != nullptr
			? static_cast<FPropertyRebuildObject*>(Generic->GetObject())
			: nullptr;
		if (Generic != nullptr && Object != nullptr && ActiveState != nullptr)
		{
			ActiveState->Trace.Add(400);
			Object->Value = static_cast<int32>(Generic->GetArgDWord(1));
		}
	}

	static bool RegisterReferenceType(
		asIScriptEngine& ScriptEngine,
		const ANSICHAR* TypeName,
		const ANSICHAR* FactoryName,
		const FRegisteredTypeConfig& Config,
		FString& OutFailure)
	{
		const auto RequireRegistration = [&OutFailure, TypeName](
			const int32 Result,
			const FString& Operation)
		{
			if (Result >= 0)
			{
				return true;
			}

			OutFailure = FString::Printf(
				TEXT("Type=%hs; Operation=%s; Result=%d"),
				TypeName,
				*Operation,
				Result);
			return false;
		};

		if (!RequireRegistration(
			ScriptEngine.RegisterObjectType(TypeName, 0, asOBJ_REF),
			TEXT("RegisterObjectType(asOBJ_REF)")))
		{
			return false;
		}
		if (!RequireRegistration(
			ScriptEngine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(AddRefPropertyRebuild),
				asCALL_CDECL_OBJFIRST),
			TEXT("RegisterObjectBehaviour(ADDREF, asCALL_CDECL_OBJFIRST)")))
		{
			return false;
		}
		if (!RequireRegistration(
			ScriptEngine.RegisterObjectBehaviour(
				TypeName,
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(ReleasePropertyRebuild),
				asCALL_CDECL_OBJFIRST),
			TEXT("RegisterObjectBehaviour(RELEASE, asCALL_CDECL_OBJFIRST)")))
		{
			return false;
		}

		const FString FactoryDeclaration = FString::Printf(
			TEXT("%hs@ %hs(int Value)"),
			TypeName,
			FactoryName);
		const FTCHARToUTF8 FactoryDeclarationUtf8(*FactoryDeclaration);
		if (!RequireRegistration(
			ScriptEngine.RegisterGlobalFunction(
				FactoryDeclarationUtf8.Get(),
				asFUNCTION(CreatePropertyRebuild),
				asCALL_GENERIC),
			FString::Printf(TEXT("RegisterGlobalFunction(%s)"), *FactoryDeclaration)))
		{
			return false;
		}

		const FString ObserveDeclaration = FString::Printf(
			TEXT("int Observe%hs(const %hs@ Receiver)"),
			TypeName,
			TypeName);
		const FTCHARToUTF8 ObserveDeclarationUtf8(*ObserveDeclaration);
		if (!RequireRegistration(
			ScriptEngine.RegisterGlobalFunction(
				ObserveDeclarationUtf8.Get(),
				asFUNCTION(ObservePropertyRebuild),
				asCALL_GENERIC),
			FString::Printf(TEXT("RegisterGlobalFunction(%s)"), *ObserveDeclaration)))
		{
			return false;
		}

		if (Config.IndexType == nullptr)
		{
			if (Config.bGetter)
			{
				const char* const Declaration = Config.bConstGetter
					? "int get_Value() const"
					: "int get_Value()";
				if (!RequireRegistration(
					ScriptEngine.RegisterObjectMethod(
						TypeName,
						Declaration,
						asFUNCTION(GetPropertyRebuild),
						asCALL_GENERIC),
					FString::Printf(TEXT("RegisterObjectMethod(%hs)"), Declaration)))
				{
					return false;
				}
			}
			if (Config.bSetter
				&& !RequireRegistration(
					ScriptEngine.RegisterObjectMethod(
						TypeName,
						"void set_Value(int Value)",
						asFUNCTION(SetPropertyRebuild),
						asCALL_GENERIC),
					TEXT("RegisterObjectMethod(void set_Value(int Value))")))
			{
				return false;
			}
			return true;
		}

		if (Config.bGetter)
		{
			const FString GetterDeclaration = Config.bConstGetter
				? FString::Printf(
					TEXT("int get_Value(%hs Index) const"),
					Config.IndexType)
				: FString::Printf(
					TEXT("int get_Value(%hs Index)"),
					Config.IndexType);
			const FTCHARToUTF8 GetterDeclarationUtf8(*GetterDeclaration);
			if (!RequireRegistration(
				ScriptEngine.RegisterObjectMethod(
					TypeName,
					GetterDeclarationUtf8.Get(),
					asFUNCTION(GetIndexedPropertyRebuild),
					asCALL_GENERIC),
				FString::Printf(TEXT("RegisterObjectMethod(%s)"), *GetterDeclaration)))
			{
				return false;
			}
		}
		if (Config.bSetter || Config.bExtraIndexedSetter)
		{
			const ANSICHAR* SetterIndexType = Config.bExtraIndexedSetter ? "uint" : Config.IndexType;
			const FString SetterDeclaration = FString::Printf(
				TEXT("void set_Value(%hs Index, int Value)"),
				SetterIndexType);
			const FTCHARToUTF8 SetterDeclarationUtf8(*SetterDeclaration);
			if (!RequireRegistration(
				ScriptEngine.RegisterObjectMethod(
					TypeName,
					SetterDeclarationUtf8.Get(),
					asFUNCTION(SetIndexedPropertyRebuild),
					asCALL_GENERIC),
				FString::Printf(TEXT("RegisterObjectMethod(%s)"), *SetterDeclaration)))
			{
				return false;
			}
		}
		return true;
	}

	static FRegisteredTypeConfig MakeRegisteredConfig(
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion)
	{
		FRegisteredTypeConfig Config;
		if (ScenarioCase.Form == EPropertyForm::RegisteredIndexed)
		{
			Config.IndexType = "int";
			Config.bSetter = false;
		}
		if (IsScenario(ScenarioCase, "registered_getter_presence"))
		{
			Config.bGetter = bSecondVersion;
			Config.bSetter = !bSecondVersion;
		}
		else if (IsScenario(ScenarioCase, "registered_setter_presence"))
		{
			Config.bGetter = !bSecondVersion;
			Config.bSetter = bSecondVersion;
		}
		else if (IsScenario(ScenarioCase, "registered_constness")
			|| IsScenario(ScenarioCase, "registered_indexed_constness"))
		{
			Config.bConstGetter = bSecondVersion;
			Config.bSetter = ScenarioCase.Form == EPropertyForm::Registered;
		}
		else if (IsScenario(ScenarioCase, "registered_indexed_index_type"))
		{
			Config.IndexType = bSecondVersion ? "uint" : "int";
		}
		else if (IsScenario(ScenarioCase, "registered_indexed_overload_set"))
		{
			Config.bExtraIndexedSetter = bSecondVersion;
		}
		return Config;
	}

	static bool RegisterScenarioFixtures(
		asIScriptEngine& ScriptEngine,
		const FScenarioCase& ScenarioCase,
		FString& OutFailure)
	{
		if (ScenarioCase.Form == EPropertyForm::Stored)
		{
			return true;
		}

		const bool bUseSameType = ScenarioCase.bSameSource
			|| IsScenario(ScenarioCase, "registered_read_value")
			|| IsScenario(ScenarioCase, "registered_indexed_value");
		const ANSICHAR* TypePrefix = ScenarioCase.Form == EPropertyForm::Registered
			? "FRebuildRegistered"
			: "FRebuildIndexed";
		const FString TypeA = FString::Printf(TEXT("%hsA"), TypePrefix);
		const FString FactoryA = FString::Printf(TEXT("Create%hsA"), TypePrefix);
		const FTCHARToUTF8 TypeAUtf8(*TypeA);
		const FTCHARToUTF8 FactoryAUtf8(*FactoryA);
		if (!RegisterReferenceType(
			ScriptEngine,
			TypeAUtf8.Get(),
			FactoryAUtf8.Get(),
			MakeRegisteredConfig(ScenarioCase, false),
			OutFailure))
		{
			return false;
		}
		if (bUseSameType)
		{
			return true;
		}

		const FString TypeB = FString::Printf(TEXT("%hsB"), TypePrefix);
		const FString FactoryB = FString::Printf(TEXT("Create%hsB"), TypePrefix);
		const FTCHARToUTF8 TypeBUtf8(*TypeB);
		const FTCHARToUTF8 FactoryBUtf8(*FactoryB);
		return RegisterReferenceType(
			ScriptEngine,
			TypeBUtf8.Get(),
			FactoryBUtf8.Get(),
			MakeRegisteredConfig(ScenarioCase, true),
			OutFailure);
	}

	static FString BuildStoredSource(const FScenarioCase& ScenarioCase, const bool bSecondVersion)
	{
		using namespace AngelscriptNativeTestSupport;

		const int32 Value = bSecondVersion && !ScenarioCase.bSameSource ? 29 : 11;
		const ANSICHAR* FieldType = IsScenario(ScenarioCase, "stored_field_type") && bSecondVersion
			? "int64"
			: "int";
		const bool bStoredInDerived = IsScenario(ScenarioCase, "stored_inheritance") && bSecondVersion;
		const bool bStoredFirst = !IsScenario(ScenarioCase, "stored_field_order") || bSecondVersion;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FStoredRebuildBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (!bStoredInDerived)
		{
			if (bStoredFirst)
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t%hs Stored = %d;"),
					FieldType,
					Value));
				AppendGeneratedAsLine(Source, TEXT("\tint Padding = 5;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\tint Padding = 5;"));
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t%hs Stored = %d;"),
					FieldType,
					Value));
			}
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tint Padding = 5;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FStoredRebuildDerived : FStoredRebuildBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (bStoredInDerived)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs Stored = %d;"),
				FieldType,
				Value));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyRebuild()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tFStoredRebuildDerived Receiver = FStoredRebuildDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn int(Receiver.Stored);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString RegisteredTypeName(
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion)
	{
		const bool bUseA = !bSecondVersion
			|| ScenarioCase.bSameSource
			|| IsScenario(ScenarioCase, "registered_read_value")
			|| IsScenario(ScenarioCase, "registered_indexed_value");
		if (ScenarioCase.Form == EPropertyForm::Registered)
		{
			return FString::Printf(
				TEXT("FRebuildRegistered%s"),
				bUseA ? TEXT("A") : TEXT("B"));
		}
		return FString::Printf(
			TEXT("FRebuildIndexed%s"),
			bUseA ? TEXT("A") : TEXT("B"));
	}

	static FString BuildRegisteredSource(
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TypeName = RegisteredTypeName(ScenarioCase, bSecondVersion);
		const FString FactoryName = TEXT("Create") + TypeName;
		const FString ObserveName = TEXT("Observe") + TypeName;
		const int32 InitialValue = bSecondVersion
			&& (IsScenario(ScenarioCase, "registered_read_value")
				|| IsScenario(ScenarioCase, "registered_indexed_value"))
			? 29
			: 11;
		const bool bConstReceiver = bSecondVersion
			&& (IsScenario(ScenarioCase, "registered_constness")
				|| IsScenario(ScenarioCase, "registered_indexed_constness"));
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunPropertyRebuild()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s%s@ Receiver = %s(%d);"),
			bConstReceiver ? TEXT("const ") : TEXT(""),
			*TypeName,
			*FactoryName,
			InitialValue));

		if (ScenarioCase.Form == EPropertyForm::Registered)
		{
			if ((IsScenario(ScenarioCase, "registered_getter_presence") && !bSecondVersion)
				|| (IsScenario(ScenarioCase, "registered_setter_presence") && bSecondVersion))
			{
				AppendGeneratedAsLine(Source, TEXT("\tReceiver.Value = 73;"));
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\treturn %s(Receiver);"),
					*ObserveName));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
			}
		}
		else
		{
			const FRegisteredTypeConfig Config = MakeRegisteredConfig(ScenarioCase, bSecondVersion);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs Index = %hs(1);"),
				Config.IndexType,
				Config.IndexType));
			AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value[Index];"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildScenarioSource(
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion)
	{
		return ScenarioCase.Form == EPropertyForm::Stored
			? BuildStoredSource(ScenarioCase, bSecondVersion)
			: BuildRegisteredSource(ScenarioCase, bSecondVersion);
	}

	static int32 ExpectedRuntimeValue(
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion)
	{
		if (IsScenario(ScenarioCase, "registered_getter_presence") && !bSecondVersion)
		{
			return 73;
		}
		if (IsScenario(ScenarioCase, "registered_setter_presence") && bSecondVersion)
		{
			return 73;
		}
		const int32 BaseValue = bSecondVersion
			&& (IsScenario(ScenarioCase, "stored_value")
				|| IsScenario(ScenarioCase, "stored_field_type")
				|| IsScenario(ScenarioCase, "stored_field_order")
				|| IsScenario(ScenarioCase, "stored_inheritance")
				|| IsScenario(ScenarioCase, "registered_read_value")
				|| IsScenario(ScenarioCase, "registered_indexed_value"))
			? 29
			: 11;
		return ScenarioCase.Form == EPropertyForm::RegisteredIndexed ? BaseValue + 1 : BaseValue;
	}

	static bool SaveBytecode(
		const FNativeCaseContext& Case,
		asIScriptModule& Module,
		FMemoryBinaryStream& Stream,
		const TCHAR* VersionName,
		FNoDiscardAsserter& Assert)
	{
		const int SaveResult = Module.SaveByteCode(&Stream, false);
		const bool bResult = Assert.AreEqual(
			static_cast<int32>(asSUCCESS),
			SaveResult,
			*Case.Describe(*FString::Printf(
				TEXT("%s property module should save bytecode"),
				VersionName)));
		const bool bStreamHasData = Assert.IsTrue(
			Stream.Num() > 0,
			*Case.Describe(TEXT("property bytecode stream should be non-empty")));
		return bResult && bStreamHasData;
	}

	static int32 ExpectedTraceMarker(
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion)
	{
		if (ScenarioCase.Form == EPropertyForm::Stored)
		{
			return INDEX_NONE;
		}
		if (ScenarioCase.Form == EPropertyForm::RegisteredIndexed)
		{
			return 300;
		}
		if ((IsScenario(ScenarioCase, "registered_getter_presence") && !bSecondVersion)
			|| (IsScenario(ScenarioCase, "registered_setter_presence") && bSecondVersion))
		{
			return 200;
		}
		return 100;
	}

	void ExecuteVersion(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		const FScenarioCase& ScenarioCase,
		const bool bSecondVersion,
		const int32 ExpectedValue,
		FPropertyRebuildState& State)
	{
		using namespace AngelscriptNativeTestSupport;

		State.ResetTrace();
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunPropertyRebuild()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("property version should expose its exact runtime entry"))));
		if (Entry == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("property version should create a runtime context"))));
		if (Context != nullptr)
		{
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				PrepareAndExecute(Context, Entry),
				*Case.Describe(TEXT("property version should execute cleanly"))));
			ASSERT_THAT(AreEqual(ExpectedValue, static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("property version should expose its expected value"))));
			Context->Release();
		}
		const int32 ExpectedMarker = ExpectedTraceMarker(ScenarioCase, bSecondVersion);
		ASSERT_THAT(AreEqual(ExpectedMarker == INDEX_NONE ? 0 : 1, State.Trace.Num(),
			*Case.Describe(TEXT("property version should emit the expected accessor callback count"))));
		if (ExpectedMarker != INDEX_NONE && State.Trace.Num() == 1)
		{
			ASSERT_THAT(AreEqual(ExpectedMarker, State.Trace[0],
				*Case.Describe(TEXT("property version should select the expected accessor callback"))));
		}
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*Case.Describe(TEXT("property version execution should release every registered receiver"))));
	}

	static int32 FindProperty(
		asITypeInfo& Type,
		const ANSICHAR* Name,
		int32& OutTypeId)
	{
		for (asUINT Index = 0; Index < Type.GetPropertyCount(); ++Index)
		{
			const char* PropertyName = nullptr;
			int TypeId = asTYPEID_VOID;
			if (Type.GetProperty(Index, &PropertyName, &TypeId) >= 0
				&& PropertyName != nullptr
				&& FCStringAnsi::Strcmp(PropertyName, Name) == 0)
			{
				OutTypeId = TypeId;
				return static_cast<int32>(Index);
			}
		}
		return INDEX_NONE;
	}

	void VerifyStoredMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		asIScriptModule& Module)
	{
		asITypeInfo* const BaseType = Module.GetTypeInfoByName("FStoredRebuildBase");
		asITypeInfo* const DerivedType = Module.GetTypeInfoByName("FStoredRebuildDerived");
		ASSERT_THAT(IsNotNull(BaseType,
			*Case.Describe(TEXT("rebuilt stored property should publish its base owner"))));
		ASSERT_THAT(IsNotNull(DerivedType,
			*Case.Describe(TEXT("rebuilt stored property should publish its derived owner"))));
		if (BaseType == nullptr || DerivedType == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(BaseType, DerivedType->GetBaseType(),
			*Case.Describe(TEXT("rebuilt stored property should preserve its inheritance relation"))));
		const bool bStoredInDerived = IsScenario(ScenarioCase, "stored_inheritance");
		asITypeInfo* const OwnerType = bStoredInDerived ? DerivedType : BaseType;
		int32 TypeId = asTYPEID_VOID;
		const int32 PropertyIndex = FindProperty(*OwnerType, "Stored", TypeId);
		// The current fork retains local derived descriptors before the copied
		// base descriptors. This differs from the observed upstream 2.38
		// declaration order and is not an active current-fork upgrade contract.
		const int32 ExpectedIndex = 0;
		ASSERT_THAT(AreEqual(ExpectedIndex, PropertyIndex,
			*Case.Describe(TEXT("rebuilt stored field should expose its version-two declaration index"))));
		const int32 ExpectedTypeId = Module.GetTypeIdByDecl(
			IsScenario(ScenarioCase, "stored_field_type") ? "int64" : "int");
		ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
			*Case.Describe(TEXT("rebuilt stored field should expose its version-two type"))));
	}

	void VerifyRegisteredMetadata(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		asIScriptEngine& ScriptEngine)
	{
		const FString TypeName = RegisteredTypeName(ScenarioCase, true);
		const FTCHARToUTF8 TypeNameUtf8(*TypeName);
		asITypeInfo* const Type = ScriptEngine.GetTypeInfoByDecl(TypeNameUtf8.Get());
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("rebuilt registered property should retain its native type"))));
		if (Type == nullptr)
		{
			return;
		}

		const FRegisteredTypeConfig Config = MakeRegisteredConfig(ScenarioCase, true);
		if (ScenarioCase.Form == EPropertyForm::Registered)
		{
			const char* const GetterDeclaration = Config.bConstGetter
				? "int get_Value() const property"
				: "int get_Value() property";
			ASSERT_THAT(AreEqual(
				Config.bGetter,
				Type->GetMethodByDecl(GetterDeclaration) != nullptr,
				*Case.Describe(TEXT("rebuilt registered getter presence/constness should match version two"))));
			ASSERT_THAT(AreEqual(
				Config.bSetter,
				Type->GetMethodByDecl("void set_Value(int Value) property") != nullptr,
				*Case.Describe(TEXT("rebuilt registered setter presence should match version two"))));
			return;
		}

		const FString GetterDeclaration = Config.bConstGetter
			? FString::Printf(
				TEXT("int get_Value(%hs Index) const property"),
				Config.IndexType)
			: FString::Printf(
				TEXT("int get_Value(%hs Index) property"),
				Config.IndexType);
		const FTCHARToUTF8 GetterDeclarationUtf8(*GetterDeclaration);
		asIScriptFunction* const Getter = Type->GetMethodByDecl(GetterDeclarationUtf8.Get());
		ASSERT_THAT(IsNotNull(Getter,
			*Case.Describe(TEXT("rebuilt indexed getter should expose version-two index type/constness"))));
		if (Getter != nullptr)
		{
			int IndexTypeId = asTYPEID_VOID;
			ASSERT_THAT(IsTrue(Getter->GetParam(0, &IndexTypeId) >= 0,
				*Case.Describe(TEXT("rebuilt indexed getter should expose index metadata"))));
			ASSERT_THAT(AreEqual(ScriptEngine.GetTypeIdByDecl(Config.IndexType), IndexTypeId,
				*Case.Describe(TEXT("rebuilt indexed getter should preserve its index type"))));
			ASSERT_THAT(AreEqual(Config.bConstGetter, Getter->IsReadOnly(),
				*Case.Describe(TEXT("rebuilt indexed getter should preserve receiver constness"))));
		}
		if (IsScenario(ScenarioCase, "registered_indexed_overload_set"))
		{
			ASSERT_THAT(IsNotNull(Type->GetMethodByDecl(
				"void set_Value(uint Index, int Value) property"),
				*Case.Describe(TEXT("version-two indexed overload set should add its uint setter"))));
		}
	}

	void VerifyMetadataObservation(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunPropertyRebuild()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("current property module should publish exactly the version-two entry"))));
		if (ScenarioCase.Form == EPropertyForm::Stored)
		{
			VerifyStoredMetadata(Case, ScenarioCase, Module);
		}
		else
		{
			VerifyRegisteredMetadata(Case, ScenarioCase, ScriptEngine);
		}
	}

	void VerifyOldHandleObservation(
		const FNativeCaseContext& Case,
		asIScriptFunction& OldEntry,
		asIScriptFunction& CurrentEntry,
		const FString& ModuleName)
	{
		ASSERT_THAT(AreNotEqual(&OldEntry, &CurrentEntry,
			*Case.Describe(TEXT("rebuilt/loaded property entry should not reuse the retained old function object"))));
		ASSERT_THAT(AreNotEqual(OldEntry.GetId(), CurrentEntry.GetId(),
			*Case.Describe(TEXT("rebuilt/loaded property entry should own a new function ID"))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("int RunPropertyRebuild()")),
			FString(UTF8_TO_TCHAR(OldEntry.GetDeclaration())),
			*Case.Describe(TEXT("retained old property handle should remain metadata-readable until release"))));
		ASSERT_THAT(AreEqual(ModuleName, FString(UTF8_TO_TCHAR(OldEntry.GetModuleName())),
			*Case.Describe(TEXT("retained old property handle should preserve its original module identity"))));
	}

	static void AppendSerializedInt64(const asINT64 Value, TArray<asBYTE>& OutBytes)
	{
		asINT64 Magnitude = Value;
		const asBYTE SignBit = (Magnitude & (asINT64(1) << 63)) != 0 ? 0x80 : 0;
		if (SignBit != 0)
		{
			Magnitude = -Magnitude;
		}

		if (Magnitude < (asINT64(1) << 6))
		{
			OutBytes.Add(static_cast<asBYTE>(SignBit + Magnitude));
		}
		else if (Magnitude < (asINT64(1) << 13))
		{
			OutBytes.Add(static_cast<asBYTE>(0x40 + SignBit + (Magnitude >> 8)));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
		else if (Magnitude < (asINT64(1) << 20))
		{
			OutBytes.Add(static_cast<asBYTE>(0x60 + SignBit + (Magnitude >> 16)));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 8) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
		else if (Magnitude < (asINT64(1) << 27))
		{
			OutBytes.Add(static_cast<asBYTE>(0x70 + SignBit + (Magnitude >> 24)));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 16) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 8) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
		else if (Magnitude < (asINT64(1) << 34))
		{
			OutBytes.Add(static_cast<asBYTE>(0x78 + SignBit + (Magnitude >> 32)));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 24) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 16) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 8) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
		else if (Magnitude < (asINT64(1) << 41))
		{
			OutBytes.Add(static_cast<asBYTE>(0x7C + SignBit + (Magnitude >> 40)));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 32) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 24) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 16) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 8) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
		else if (Magnitude < (asINT64(1) << 48))
		{
			OutBytes.Add(static_cast<asBYTE>(0x7E + SignBit + (Magnitude >> 48)));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 40) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 32) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 24) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 16) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 8) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
		else
		{
			OutBytes.Add(static_cast<asBYTE>(0x7F + SignBit));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 56) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 48) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 40) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 32) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 24) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 16) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>((Magnitude >> 8) & 0xFF));
			OutBytes.Add(static_cast<asBYTE>(Magnitude & 0xFF));
		}
	}

	static void CollectObjectTypeOperandTraces(
		asIScriptFunction* Function,
		TSet<asIScriptFunction*>& SeenFunctions,
		TArray<FObjectTypeOperandTrace>& OutTraces)
	{
		if (Function == nullptr || SeenFunctions.Contains(Function))
		{
			return;
		}
		SeenFunctions.Add(Function);

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return;
		}

		for (asUINT DwordIndex = 0; DwordIndex < BytecodeLength;)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return;
			}

			if (Opcode == asBC_FinConstruct
				|| Opcode == asBC_DestructScript
				|| Opcode == asBC_CopyScript)
			{
				asQWORD Operand = 0;
				FMemory::Memcpy(&Operand, Bytecode + DwordIndex + 1, sizeof(Operand));
				FObjectTypeOperandTrace& Trace = OutTraces.AddDefaulted_GetRef();
				Trace.FunctionDeclaration = UTF8_TO_TCHAR(Function->GetDeclaration());
				Trace.ScriptSection = UTF8_TO_TCHAR(Function->GetScriptSectionName());
				Trace.Opcode = Opcode;
				Trace.DwordIndex = DwordIndex;
				Trace.LiveOperand = Operand;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
	}

	static void CollectObjectTypeOperandTraces(
		asIScriptModule& Module,
		TArray<FObjectTypeOperandTrace>& OutTraces)
	{
		OutTraces.Reset();
		TSet<asIScriptFunction*> SeenFunctions;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			CollectObjectTypeOperandTraces(
				Module.GetFunctionByIndex(FunctionIndex),
				SeenFunctions,
				OutTraces);
		}

		for (asUINT TypeIndex = 0; TypeIndex < Module.GetObjectTypeCount(); ++TypeIndex)
		{
			asITypeInfo* const Type = Module.GetObjectTypeByIndex(TypeIndex);
			if (Type == nullptr)
			{
				continue;
			}

			for (asUINT MethodIndex = 0; MethodIndex < Type->GetMethodCount(); ++MethodIndex)
			{
				CollectObjectTypeOperandTraces(
					Type->GetMethodByIndex(MethodIndex),
					SeenFunctions,
					OutTraces);
			}
			for (asUINT BehaviourIndex = 0; BehaviourIndex < Type->GetBehaviourCount(); ++BehaviourIndex)
			{
				asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
				CollectObjectTypeOperandTraces(
					Type->GetBehaviourByIndex(BehaviourIndex, &Behaviour),
					SeenFunctions,
					OutTraces);
			}
		}
	}

	static void FindSerializedPatternOffsets(
		const TArray<asBYTE>& Bytes,
		const TArray<asBYTE>& Pattern,
		TArray<int32>& OutOffsets)
	{
		OutOffsets.Reset();
		if (Pattern.IsEmpty() || Pattern.Num() > Bytes.Num())
		{
			return;
		}

		for (int32 Offset = 0; Offset <= Bytes.Num() - Pattern.Num(); ++Offset)
		{
			if (FMemory::Memcmp(Bytes.GetData() + Offset, Pattern.GetData(), Pattern.Num()) == 0)
			{
				OutOffsets.Add(Offset);
			}
		}
	}

	static FString DescribeBytecodeWindow(
		const TArray<asBYTE>& FirstBytes,
		const TArray<asBYTE>& SecondBytes,
		const int32 FirstDifference)
	{
		if (FirstDifference == INDEX_NONE)
		{
			return TEXT("<no difference>");
		}

		const int32 Start = FMath::Max(0, FirstDifference - 12);
		const int32 End = FMath::Min(FMath::Max(FirstBytes.Num(), SecondBytes.Num()), FirstDifference + 13);
		TArray<FString> Entries;
		for (int32 Offset = Start; Offset < End; ++Offset)
		{
			const FString FirstValue = FirstBytes.IsValidIndex(Offset)
				? FString::Printf(TEXT("%02x"), static_cast<uint32>(FirstBytes[Offset]))
				: TEXT("--");
			const FString SecondValue = SecondBytes.IsValidIndex(Offset)
				? FString::Printf(TEXT("%02x"), static_cast<uint32>(SecondBytes[Offset]))
				: TEXT("--");
			Entries.Add(FString::Printf(TEXT("%d:%s/%s"), Offset, *FirstValue, *SecondValue));
		}
		return FString::Join(Entries, TEXT(" "));
	}

	void ReportObjectTypeOperandSerialization(
		const FNativeCaseContext& Case,
		const TCHAR* VersionName,
		const TArray<FObjectTypeOperandTrace>& Traces,
		const FMemoryBinaryStream& Stream,
		const int32 FirstDifference)
	{
		const TArray<asBYTE>& Bytes = Stream.GetBytes();
		const bool bCanonicalStream = Bytes.IsValidIndex(1) && Bytes[1] >= 2;
		for (const FObjectTypeOperandTrace& Trace : Traces)
		{
			TArray<asBYTE> Pattern;
			Pattern.Add(static_cast<asBYTE>(Trace.Opcode));
			AppendSerializedInt64(static_cast<asINT64>(Trace.LiveOperand), Pattern);
			TArray<int32> Matches;
			FindSerializedPatternOffsets(Bytes, Pattern, Matches);

			const int32 OpcodeOffset = Matches.Num() == 1 ? Matches[0] : INDEX_NONE;
			const int32 OperandStart = OpcodeOffset == INDEX_NONE ? INDEX_NONE : OpcodeOffset + 1;
			const int32 OperandEnd = OperandStart == INDEX_NONE ? INDEX_NONE : OperandStart + Pattern.Num() - 1;
			const bool bDifferenceInOperand = FirstDifference >= OperandStart && FirstDifference < OperandEnd;
			const FString TraceInfo = FString::Printf(
				TEXT("[AS-BYTECODE-OPERAND] Id=%s Version=%s Function=%s Section=%s Opcode=%hs DwordIndex=%u LiveOperand=0x%llx MatchCount=%d OpcodeRange=[%d,%d) OperandRange=[%d,%d) FirstDifference=%d FirstDifferenceInOperand=%d CanonicalStream=%d"),
				*Case.GetId(),
				VersionName,
				*Trace.FunctionDeclaration,
				*Trace.ScriptSection,
				asBCInfo[Trace.Opcode].name,
				Trace.DwordIndex,
				static_cast<uint64>(Trace.LiveOperand),
				Matches.Num(),
				OpcodeOffset,
				OpcodeOffset == INDEX_NONE ? INDEX_NONE : OpcodeOffset + 1,
				OperandStart,
				OperandEnd,
				FirstDifference,
				bDifferenceInOperand ? 1 : 0,
				bCanonicalStream ? 1 : 0);
			TestRunner->AddInfo(TraceInfo);
			UE_LOG(LogTemp, Display, TEXT("%s"), *TraceInfo);

			if (bCanonicalStream)
			{
				ASSERT_THAT(AreEqual(0, Matches.Num(),
					*Case.Describe(TEXT("version-two bytecode must not serialize a live script-object type address"))));
			}
		}
	}

	void VerifyBytecodeRelation(
		const FNativeCaseContext& Case,
		const FScenarioCase& ScenarioCase,
		const FPathCase& PathCase,
		const FMemoryBinaryStream& First,
		const FMemoryBinaryStream& Second,
		const TArray<FObjectTypeOperandTrace>& FirstTraces,
		const TArray<FObjectTypeOperandTrace>& SecondTraces)
	{
		const TArray<asBYTE>& FirstBytes = First.GetBytes();
		const TArray<asBYTE>& SecondBytes = Second.GetBytes();
		int32 FirstDifference = INDEX_NONE;
		const int32 SharedByteCount = FMath::Min(FirstBytes.Num(), SecondBytes.Num());
		for (int32 ByteIndex = 0; ByteIndex < SharedByteCount; ++ByteIndex)
		{
			if (FirstBytes[ByteIndex] != SecondBytes[ByteIndex])
			{
				FirstDifference = ByteIndex;
				break;
			}
		}
		if (FirstDifference == INDEX_NONE && FirstBytes.Num() != SecondBytes.Num())
		{
			FirstDifference = SharedByteCount;
		}
		const int32 FirstDifferenceFirstByte = FirstBytes.IsValidIndex(FirstDifference)
			? static_cast<int32>(FirstBytes[FirstDifference])
			: INDEX_NONE;
		const int32 FirstDifferenceSecondByte = SecondBytes.IsValidIndex(FirstDifference)
			? static_cast<int32>(SecondBytes[FirstDifference])
			: INDEX_NONE;

		const FString RelationInfo = FString::Printf(
			TEXT("[AS-BYTECODE-RELATION] Id=%s SameSource=%d FirstSize=%d SecondSize=%d FirstDifference=%d FirstByte=%d SecondByte=%d"),
			*Case.GetId(),
			ScenarioCase.bSameSource ? 1 : 0,
			FirstBytes.Num(),
			SecondBytes.Num(),
			FirstDifference,
			FirstDifferenceFirstByte,
			FirstDifferenceSecondByte);
		TestRunner->AddInfo(RelationInfo);
		UE_LOG(LogTemp, Display, TEXT("%s"), *RelationInfo);
		const FString WindowInfo = FString::Printf(
			TEXT("[AS-BYTECODE-WINDOW] Id=%s FirstDifference=%d Bytes=%s"),
			*Case.GetId(),
			FirstDifference,
			*DescribeBytecodeWindow(FirstBytes, SecondBytes, FirstDifference));
		TestRunner->AddInfo(WindowInfo);
		UE_LOG(LogTemp, Display, TEXT("%s"), *WindowInfo);
		ReportObjectTypeOperandSerialization(Case, TEXT("version one"), FirstTraces, First, FirstDifference);
		ReportObjectTypeOperandSerialization(Case, TEXT("version two"), SecondTraces, Second, FirstDifference);

		if (ScenarioCase.bSameSource && !PathCase.bSaveLoad)
		{
			ASSERT_THAT(IsTrue(FirstBytes == SecondBytes,
				*Case.Describe(TEXT("unchanged property source should emit byte-identical bytecode"))));
		}
		else if (ScenarioCase.bSameSource)
		{
			ASSERT_THAT(IsFalse(FirstBytes == SecondBytes,
				*Case.Describe(
					TEXT("save/load staging uses a distinct module and section identity, so its full stream must differ; fresh-engine control covers identical identities"))));
		}
		else
		{
			ASSERT_THAT(IsFalse(FirstBytes == SecondBytes,
				*Case.Describe(TEXT("changed property source should emit distinguishable bytecode"))));
		}
	}

	void ReportCompileFailure(
		const FNativeCaseContext& Case,
		const TCHAR* VersionName,
		const int32 CompileResult,
		const FNativeTestEngine& Engine)
	{
		if (CompileResult >= 0)
		{
			return;
		}

		const FString CompileInfo = FString::Printf(
			TEXT("[AS-PROPERTY-COMPILE] Id=%s Version=%s Result=%d Diagnostics=\n%s"),
			*Case.GetId(),
			VersionName,
			CompileResult,
			*Engine.GetMessagesText());
		TestRunner->AddInfo(CompileInfo);
		UE_LOG(LogTemp, Display, TEXT("%s"), *CompileInfo);
	}

	void RunWorkflow(const FScenarioCase& ScenarioCase, const FPathCase& PathCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext MetadataCase = MakeObservationCase(
			ObservationCases[0],
			PathCase,
			ScenarioCase);
		const FNativeCaseContext RuntimeCase = MakeObservationCase(
			ObservationCases[1],
			PathCase,
			ScenarioCase);
		const FNativeCaseContext OldHandleCase = MakeObservationCase(
			ObservationCases[2],
			PathCase,
			ScenarioCase);

		FPropertyRebuildState State;
		ActiveState = &State;
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*MetadataCase.Describe(TEXT("property rebuild workflow should create an isolated raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			Engine.Destroy();
			ActiveState = nullptr;
			return;
		}
		FString FixtureFailure;
		const bool bRegisteredFixtures = RegisterScenarioFixtures(
			*ScriptEngine,
			ScenarioCase,
			FixtureFailure);
		const FString RegistrationInfo = FString::Printf(
			TEXT("[AS-PROPERTY-REGISTRATION] Id=%s Form=%s Result=%s%s"),
			*MetadataCase.GetId(),
			GetPropertyFormName(ScenarioCase.Form),
			bRegisteredFixtures ? TEXT("success") : TEXT("failure"),
			bRegisteredFixtures ? TEXT("") : *FString::Printf(TEXT("; %s"), *FixtureFailure));
		TestRunner->AddInfo(RegistrationInfo);
		UE_LOG(LogTemp, Display, TEXT("%s"), *RegistrationInfo);
		ASSERT_THAT(IsTrue(bRegisteredFixtures,
			*MetadataCase.Describe(*FString::Printf(
				TEXT("property rebuild workflow should register its exact native fixture versions: %s"),
				bRegisteredFixtures ? TEXT("success") : *FixtureFailure))));
		if (!bRegisteredFixtures)
		{
			Engine.Destroy();
			ActiveState = nullptr;
			return;
		}

		const FString Suffix = MakeSuffix(ScenarioCase, PathCase);
		const FString ModuleName = TEXT("PropertyRebuild_") + Suffix;
		const FString StagingModuleName = ModuleName + TEXT("_Staging");
		const FString FirstSource = BuildScenarioSource(ScenarioCase, false);
		const FString SecondSource = BuildScenarioSource(ScenarioCase, true);
		PrintGeneratedAsSource(
			*TestRunner,
			MetadataCase.GetId() + TEXT("-VERSION-1"),
			ModuleName,
			FirstSource);
		PrintGeneratedAsSource(
			*TestRunner,
			MetadataCase.GetId() + TEXT("-VERSION-2"),
			PathCase.bSaveLoad ? StagingModuleName : ModuleName,
			SecondSource);

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 FirstSourceUtf8(*FirstSource);
		Engine.ResetMessages();
		asIScriptModule* FirstModule = nullptr;
		const int32 FirstCompileResult = CompileNativeModule(
			ScriptEngine,
			ModuleNameUtf8.Get(),
			FirstSourceUtf8.Get(),
			FirstModule);
		ReportCompileFailure(MetadataCase, TEXT("version one"), FirstCompileResult, Engine);
		ASSERT_THAT(IsTrue(FirstCompileResult >= 0,
			*MetadataCase.Describe(TEXT("property version one should compile"))));
		ASSERT_THAT(IsNotNull(FirstModule,
			*MetadataCase.Describe(TEXT("property version one should publish a module"))));
		if (FirstModule == nullptr)
		{
			Engine.Destroy();
			ActiveState = nullptr;
			return;
		}

		FMemoryBinaryStream FirstBytecode;
		SaveBytecode(MetadataCase, *FirstModule, FirstBytecode, TEXT("version one"), this->Assert);
		TArray<FObjectTypeOperandTrace> FirstObjectTypeOperandTraces;
		CollectObjectTypeOperandTraces(*FirstModule, FirstObjectTypeOperandTraces);
		ExecuteVersion(
			RuntimeCase,
			*ScriptEngine,
			*FirstModule,
			ScenarioCase,
			false,
			ExpectedRuntimeValue(ScenarioCase, false),
			State);
		asIScriptFunction* const OldEntry = FirstModule->GetFunctionByDecl("int RunPropertyRebuild()");
		ASSERT_THAT(IsNotNull(OldEntry,
			*OldHandleCase.Describe(TEXT("property version one should expose a retainable entry"))));
		if (OldEntry == nullptr)
		{
			Engine.Destroy();
			ActiveState = nullptr;
			return;
		}
		OldEntry->AddRef();
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*OldHandleCase.Describe(TEXT("discard should remove version one from engine lookup"))));

		FMemoryBinaryStream SecondBytecode;
		TArray<FObjectTypeOperandTrace> SecondObjectTypeOperandTraces;
		asIScriptModule* CurrentModule = nullptr;
		if (!PathCase.bSaveLoad)
		{
			const FTCHARToUTF8 SecondSourceUtf8(*SecondSource);
			Engine.ResetMessages();
			const int32 SecondCompileResult = CompileNativeModule(
				ScriptEngine,
				ModuleNameUtf8.Get(),
				SecondSourceUtf8.Get(),
				CurrentModule);
			ReportCompileFailure(MetadataCase, TEXT("version two rebuild"), SecondCompileResult, Engine);
			ASSERT_THAT(IsTrue(SecondCompileResult >= 0,
				*MetadataCase.Describe(TEXT("property version two should rebuild under the same module name"))));
			ASSERT_THAT(IsNotNull(CurrentModule,
				*MetadataCase.Describe(TEXT("rebuilt property version should publish a module"))));
			if (CurrentModule != nullptr)
			{
				SaveBytecode(MetadataCase, *CurrentModule, SecondBytecode, TEXT("version two"), this->Assert);
				CollectObjectTypeOperandTraces(*CurrentModule, SecondObjectTypeOperandTraces);
			}
		}
		else
		{
			const FTCHARToUTF8 StagingModuleNameUtf8(*StagingModuleName);
			const FTCHARToUTF8 SecondSourceUtf8(*SecondSource);
			asIScriptModule* StagingModule = nullptr;
			Engine.ResetMessages();
			const int32 StagingCompileResult = CompileNativeModule(
				ScriptEngine,
				StagingModuleNameUtf8.Get(),
				SecondSourceUtf8.Get(),
				StagingModule);
			ReportCompileFailure(MetadataCase, TEXT("version two staging"), StagingCompileResult, Engine);
			ASSERT_THAT(IsTrue(StagingCompileResult >= 0,
				*MetadataCase.Describe(TEXT("property version two should compile before save/load"))));
			ASSERT_THAT(IsNotNull(StagingModule,
				*MetadataCase.Describe(TEXT("property save/load staging module should exist"))));
			if (StagingModule != nullptr)
			{
				SaveBytecode(MetadataCase, *StagingModule, SecondBytecode, TEXT("version two"), this->Assert);
				CollectObjectTypeOperandTraces(*StagingModule, SecondObjectTypeOperandTraces);
			}
			ScriptEngine->DiscardModule(StagingModuleNameUtf8.Get());
			SecondBytecode.ResetReadPosition();
			CurrentModule = ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
			ASSERT_THAT(IsNotNull(CurrentModule,
				*MetadataCase.Describe(TEXT("property save/load should create its destination module"))));
			bool bWasDebugInfoStripped = true;
			if (CurrentModule != nullptr)
			{
				ASSERT_THAT(AreEqual(
					static_cast<int32>(asSUCCESS),
					CurrentModule->LoadByteCode(&SecondBytecode, &bWasDebugInfoStripped),
					*MetadataCase.Describe(TEXT("property version two bytecode should load"))));
				ASSERT_THAT(IsFalse(bWasDebugInfoStripped,
					*MetadataCase.Describe(TEXT("property bytecode roundtrip should preserve debug metadata"))));
			}
		}

		if (CurrentModule != nullptr)
		{
			VerifyMetadataObservation(MetadataCase, ScenarioCase, *ScriptEngine, *CurrentModule);
			ExecuteVersion(
				RuntimeCase,
				*ScriptEngine,
				*CurrentModule,
				ScenarioCase,
				true,
				ExpectedRuntimeValue(ScenarioCase, true),
				State);
			asIScriptFunction* const CurrentEntry =
				CurrentModule->GetFunctionByDecl("int RunPropertyRebuild()");
			ASSERT_THAT(IsNotNull(CurrentEntry,
				*OldHandleCase.Describe(TEXT("current property module should expose its exact entry"))));
			if (CurrentEntry != nullptr)
			{
				VerifyOldHandleObservation(
					OldHandleCase,
					*OldEntry,
					*CurrentEntry,
					ModuleName);
			}
				VerifyBytecodeRelation(
					MetadataCase,
					ScenarioCase,
					PathCase,
					FirstBytecode,
					SecondBytecode,
					FirstObjectTypeOperandTraces,
					SecondObjectTypeOperandTraces);
		}

		OldEntry->Release();
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*OldHandleCase.Describe(TEXT("property workflow should discard the current module"))));
		ASSERT_THAT(AreEqual(0, State.LiveObjects,
			*OldHandleCase.Describe(TEXT("property workflow should leave no registered receiver alive"))));
		ASSERT_THAT(AreEqual(State.CreatedObjects, State.DestroyedObjects,
			*OldHandleCase.Describe(TEXT("property workflow should balance registered receiver lifecycle"))));
		Engine.Destroy();
		ActiveState = nullptr;
	}

	void RunFreshEngineDeterminismControl()
	{
		using namespace AngelscriptNativeTestSupport;

		const FScenarioCase& ScenarioCase = ScenarioCases[0];
		const FString ModuleName = TEXT("PropertyRebuildFreshEngineControl");
		const FString Source = BuildStoredSource(ScenarioCase, false);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("LANG-PROP-REBUILD-fresh-engine-control-A"),
			ModuleName,
			Source);
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("LANG-PROP-REBUILD-fresh-engine-control-B"),
			ModuleName,
			Source);

		FNativeTestEngine EngineA;
		FNativeTestEngine EngineB;
		FNativeTestEngine EngineC;
		ON_SCOPE_EXIT
		{
			EngineC.Destroy();
			EngineB.Destroy();
			EngineA.Destroy();
		};
		EngineA.Create(*TestRunner);
		EngineB.Create(*TestRunner);
		asIScriptEngine* const ScriptEngineA = EngineA.Get();
		asIScriptEngine* const ScriptEngineB = EngineB.Get();
		ASSERT_THAT(IsNotNull(ScriptEngineA,
			TEXT("fresh-engine bytecode control should create engine A")));
		ASSERT_THAT(IsNotNull(ScriptEngineB,
			TEXT("fresh-engine bytecode control should create engine B")));
		if (ScriptEngineA == nullptr || ScriptEngineB == nullptr)
		{
			return;
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* ModuleA = nullptr;
		asIScriptModule* ModuleB = nullptr;
		ASSERT_THAT(IsTrue(
			CompileNativeModule(ScriptEngineA, ModuleNameUtf8.Get(), SourceUtf8.Get(), ModuleA) >= 0,
			TEXT("fresh-engine bytecode control should compile source in engine A")));
		ASSERT_THAT(IsTrue(
			CompileNativeModule(ScriptEngineB, ModuleNameUtf8.Get(), SourceUtf8.Get(), ModuleB) >= 0,
			TEXT("fresh-engine bytecode control should compile the identical source in engine B")));
		ASSERT_THAT(IsNotNull(ModuleA,
			TEXT("fresh-engine bytecode control should publish module A")));
		ASSERT_THAT(IsNotNull(ModuleB,
			TEXT("fresh-engine bytecode control should publish module B")));
		if (ModuleA == nullptr || ModuleB == nullptr)
		{
			return;
		}

		FMemoryBinaryStream StreamA;
		FMemoryBinaryStream StreamB;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ModuleA->SaveByteCode(&StreamA, false),
			TEXT("fresh-engine bytecode control should save module A")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ModuleB->SaveByteCode(&StreamB, false),
			TEXT("fresh-engine bytecode control should save module B")));
		ASSERT_THAT(IsTrue(StreamA.GetBytes() == StreamB.GetBytes(),
			TEXT("two concurrent fresh engines must save identical source to byte-identical streams")));

		ModuleA->Discard();
		ModuleB->Discard();
		EngineA.Destroy();
		EngineB.Destroy();
		EngineC.Create(*TestRunner);
		asIScriptEngine* const ScriptEngineC = EngineC.Get();
		ASSERT_THAT(IsNotNull(ScriptEngineC,
			TEXT("fresh-engine bytecode control should create a distinct load engine")));
		if (ScriptEngineC == nullptr)
		{
			return;
		}

		StreamA.ResetReadPosition();
		asIScriptModule* const LoadedModule = ScriptEngineC->GetModule(ModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(LoadedModule,
			TEXT("fresh-engine bytecode control should create a destination module")));
		if (LoadedModule == nullptr)
		{
			return;
		}
		bool bWasDebugInfoStripped = true;
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			LoadedModule->LoadByteCode(&StreamA, &bWasDebugInfoStripped),
			TEXT("fresh-engine bytecode control should load the stream in a third engine")));
		ASSERT_THAT(IsFalse(bWasDebugInfoStripped,
			TEXT("fresh-engine bytecode control should retain debug metadata")));
		asIScriptFunction* const Entry = LoadedModule->GetFunctionByDecl("int RunPropertyRebuild()");
		ASSERT_THAT(IsNotNull(Entry,
			TEXT("fresh-engine bytecode control should restore its exact entry")));
		if (Entry == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngineC->CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			TEXT("fresh-engine bytecode control should create an execution context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT { Context->Release(); };
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			TEXT("fresh-engine bytecode control should execute after cross-engine restore")));
		ASSERT_THAT(AreEqual(
			11,
			static_cast<int32>(Context->GetReturnDWord()),
			TEXT("fresh-engine bytecode control should preserve the stored property value")));
	}

public:
	TEST_METHOD(ScenariosByPathAndObservation)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-PROP-REBUILD",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		for (const FPathCase& PathCase : PathCases)
		{
			for (const FScenarioCase& ScenarioCase : ScenarioCases)
			{
				RunWorkflow(ScenarioCase, PathCase);
			}
		}
		RunFreshEngineDeterminismControl();
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
