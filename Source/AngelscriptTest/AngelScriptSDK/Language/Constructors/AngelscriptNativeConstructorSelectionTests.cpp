#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorSelectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent =
		AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeCaseReference =
		AngelscriptNativeTestSupport::FNativeCaseReference;
	using FNativeLifecycleEntry =
		AngelscriptNativeTestSupport::FNativeLifecycleEntry;
	using FNativeLifecycleRecorder =
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;

	static constexpr asPWORD ConstructorSelectionStateUserDataSlot =
		static_cast<asPWORD>(0x43544F5253454C45ull);

	struct FObjectCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FKindCase
	{
		const ANSICHAR* CatalogName;
		int32 Marker;
	};

	struct FCallCase
	{
		const ANSICHAR* CatalogName;
		int32 RouteMarker;
	};

	inline static constexpr FObjectCase ObjectCases[] =
	{
		{ "script_value" },
		{ "script_reference" },
		{ "base" },
		{ "derived" },
		{ "native_value" },
		{ "native_reference" },
	};

	inline static constexpr FKindCase KindCases[] =
	{
		{ "implicit_default", 101 },
		{ "declared_default", 201 },
		{ "parameterized", 301 },
		{ "overloaded", 401 },
		{ "copy", 501 },
		{ "conversion", 601 },
	};

	inline static constexpr FCallCase CallCases[] =
	{
		{ "local", 11 },
		{ "temporary", 12 },
		{ "field", 13 },
		{ "return", 14 },
		{ "argument", 15 },
		{ "base_call", 16 },
		{ "copy_declaration", 17 },
		{ "assignment", 18 },
	};

	struct FConstructorSelectionState
	{
		TArray<int32> SelectedMarkers;
		TArray<int32> RouteMarkers;
		TArray<int32> TransferSourceValues;
		TArray<int32> TransferTargetValues;
		int32 ScriptCopyConstructorCount = 0;

		void Reset()
		{
			SelectedMarkers.Reset();
			RouteMarkers.Reset();
			TransferSourceValues.Reset();
			TransferTargetValues.Reset();
			ScriptCopyConstructorCount = 0;
		}
	};

	static FString DescribeMarkers(const TArray<int32>& Markers)
	{
		FString Description = TEXT("[");
		for (int32 Index = 0; Index < Markers.Num(); ++Index)
		{
			if (Index > 0)
			{
				Description += TEXT(", ");
			}
			Description += LexToString(Markers[Index]);
		}
		Description += TEXT("]");
		return Description;
	}

	static FConstructorSelectionState* GetState(
		asIScriptEngine* ScriptEngine)
	{
		return ScriptEngine != nullptr
			? static_cast<FConstructorSelectionState*>(
				ScriptEngine->GetUserData(
					ConstructorSelectionStateUserDataSlot))
			: nullptr;
	}

	static FConstructorSelectionState* GetActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr ? GetState(Context->GetEngine()) : nullptr;
	}

	static void RecordConstructorKindSelected(const int32 Marker)
	{
		if (FConstructorSelectionState* const State = GetActiveState())
		{
			State->SelectedMarkers.Add(Marker);
		}
	}

	static void RecordConstructorCallRoute(const int32 Marker)
	{
		if (FConstructorSelectionState* const State = GetActiveState())
		{
			State->RouteMarkers.Add(Marker);
		}
	}

	static void RecordConstructorTransferObservation(
		const int32 SourceValue,
		const int32 TargetValue)
	{
		if (FConstructorSelectionState* const State = GetActiveState())
		{
			State->TransferSourceValues.Add(SourceValue);
			State->TransferTargetValues.Add(TargetValue);
		}
	}

	static void RecordConstructorScriptCopy()
	{
		if (FConstructorSelectionState* const State = GetActiveState())
		{
			++State->ScriptCopyConstructorCount;
		}
	}

	static bool RegisterConstructorSelectionBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorSelectionState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			ConstructorSelectionStateUserDataSlot);
		const ASAutoCaller::FunctionCaller SelectionCaller =
			ASAutoCaller::MakeFunctionCaller(RecordConstructorKindSelected);
		const ASAutoCaller::FunctionCaller RouteCaller =
			ASAutoCaller::MakeFunctionCaller(RecordConstructorCallRoute);
		const ASAutoCaller::FunctionCaller TransferCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorTransferObservation);
		const ASAutoCaller::FunctionCaller ScriptCopyCaller =
			ASAutoCaller::MakeFunctionCaller(RecordConstructorScriptCopy);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorKindSelected(int Marker)",
			asFUNCTION(RecordConstructorKindSelected),
			asCALL_CDECL,
			*(asFunctionCaller*)&SelectionCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorCallRoute(int Marker)",
				asFUNCTION(RecordConstructorCallRoute),
				asCALL_CDECL,
				*(asFunctionCaller*)&RouteCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorTransferObservation(int SourceValue, int TargetValue)",
				asFUNCTION(RecordConstructorTransferObservation),
				asCALL_CDECL,
				*(asFunctionCaller*)&TransferCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorScriptCopy()",
				asFUNCTION(RecordConstructorScriptCopy),
				asCALL_CDECL,
				*(asFunctionCaller*)&ScriptCopyCaller) >= 0;
	}

	static void CreateNativeReference(
		asIScriptGeneric* Generic,
		const int32 Value)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FNativeLifecycleRecorder* const Recorder =
			static_cast<FNativeLifecycleRecorder*>(
				Generic->GetEngine()->GetUserData(
					AngelscriptNativeTestSupport::
						NativeLifecycleRecorderUserDataSlot));
		if (Recorder != nullptr)
		{
			Generic->SetReturnAddress(
				new FNativeCaseReference(*Recorder, Value));
		}
	}

	static void MakeNativeReferenceImplicit_Generic(
		asIScriptGeneric* Generic)
	{
		CreateNativeReference(Generic, 0);
	}

	static void MakeNativeReferenceDeclared_Generic(
		asIScriptGeneric* Generic)
	{
		CreateNativeReference(Generic, 7);
	}

	static void MakeNativeReferenceParameterized_Generic(
		asIScriptGeneric* Generic)
	{
		CreateNativeReference(
			Generic,
			static_cast<int32>(Generic->GetArgDWord(0)));
	}

	static void MakeNativeReferenceOverloaded_Generic(
		asIScriptGeneric* Generic)
	{
		CreateNativeReference(
			Generic,
			static_cast<int32>(Generic->GetArgDWord(0))
				+ static_cast<int32>(Generic->GetArgDWord(1)));
	}

	static void MakeNativeReferenceConversion_Generic(
		asIScriptGeneric* Generic)
	{
		CreateNativeReference(
			Generic,
			static_cast<int32>(Generic->GetArgQWord(0)));
	}

	static bool RegisterNativeReferenceFactories(
		asIScriptEngine& ScriptEngine)
	{
		return ScriptEngine.RegisterGlobalFunction(
			"FNativeCaseReference MakeNativeReferenceImplicit()",
			asFUNCTION(MakeNativeReferenceImplicit_Generic),
			asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FNativeCaseReference MakeNativeReferenceDeclared()",
				asFUNCTION(MakeNativeReferenceDeclared_Generic),
				asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FNativeCaseReference MakeNativeReferenceParameterized(int Value)",
				asFUNCTION(MakeNativeReferenceParameterized_Generic),
				asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FNativeCaseReference MakeNativeReferenceOverloaded(int Left, int Right)",
				asFUNCTION(MakeNativeReferenceOverloaded_Generic),
				asCALL_GENERIC) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"FNativeCaseReference MakeNativeReferenceConversion(int64 Value)",
				asFUNCTION(MakeNativeReferenceConversion_Generic),
				asCALL_GENERIC) >= 0;
	}

	static bool IsObject(
		const FObjectCase& ObjectCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			ObjectCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsKind(
		const FKindCase& KindCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			KindCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsCall(
		const FCallCase& CallCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			CallCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsValueObject(const FObjectCase& ObjectCase)
	{
		return IsObject(ObjectCase, "script_value")
			|| IsObject(ObjectCase, "native_value");
	}

	static bool IsReferenceObject(const FObjectCase& ObjectCase)
	{
		return !IsValueObject(ObjectCase);
	}

	static bool IsScriptObject(const FObjectCase& ObjectCase)
	{
		return !IsObject(ObjectCase, "native_value")
			&& !IsObject(ObjectCase, "native_reference");
	}

	static bool UsesRestrictedRawScriptReferenceLifetime(
		const FObjectCase& ObjectCase)
	{
		return IsScriptObject(ObjectCase)
			&& IsReferenceObject(ObjectCase);
	}

	static bool ExpectedBuild(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		return !IsCall(CallCase, "base_call")
			|| (IsObject(ObjectCase, "derived")
				&& !IsKind(KindCase, "implicit_default"));
	}

	static FString ObjectTypeName(const FObjectCase& ObjectCase)
	{
		if (IsObject(ObjectCase, "native_value"))
		{
			return TEXT("FNativeCaseValue");
		}
		if (IsObject(ObjectCase, "native_reference"))
		{
			return TEXT("FNativeCaseReference");
		}
		return TEXT("FKindObject");
	}

	static void AppendScriptConstructor(
		FString& Source,
		const FKindCase& KindCase,
		const TCHAR* TypeName,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsKind(KindCase, "declared_default"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s()"),
				Indent,
				TypeName));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("{"));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tValue = 7;"));
		}
		else if (IsKind(KindCase, "parameterized"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s(int InValue)"),
				Indent,
				TypeName));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("{"));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tValue = InValue;"));
		}
		else if (IsKind(KindCase, "overloaded"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s(int Left, int Right)"),
				Indent,
				TypeName));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("{"));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tValue = Left + Right;"));
		}
		else if (IsKind(KindCase, "conversion"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s(int64 InValue)"),
				Indent,
				TypeName));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("{"));
			AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tValue = int(InValue);"));
		}
		else
		{
			return;
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s\tRecordConstructorKindSelected(%d);"),
			Indent,
			KindCase.Marker));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendScriptDefaultConstructor(
		FString& Source,
		const TCHAR* TypeName,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s%s()"),
			Indent,
			TypeName));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("{"));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendScriptCopyConstructor(
		FString& Source,
		const TCHAR* TypeName,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s%s(const %s& Other)"),
			Indent,
			TypeName,
			TypeName));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("{"));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tValue = Other.Value;"));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tPayload = Other.Payload;"));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("\tRecordConstructorScriptCopy();"));
		AppendGeneratedAsLine(Source, FString(Indent) + TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendScriptObjectType(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bValue = IsObject(ObjectCase, "script_value");
		const bool bDerived = IsObject(ObjectCase, "derived");
		if (bDerived)
		{
			AppendGeneratedAsLine(Source, TEXT("class FKindBase"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint BaseValue = 2;"));
			AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue BasePayload;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s FKindObject%s"),
			bValue ? TEXT("struct") : TEXT("class"),
			bDerived ? TEXT(" : FKindBase") : TEXT("")));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendScriptConstructor(
			Source,
			KindCase,
			TEXT("FKindObject"),
			TEXT("\t"));
		if (bValue)
		{
			if (IsKind(KindCase, "copy"))
			{
				AppendScriptDefaultConstructor(
					Source,
					TEXT("FKindObject"),
					TEXT("\t"));
			}
			AppendScriptCopyConstructor(
				Source,
				TEXT("FKindObject"),
				TEXT("\t"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		if (IsObject(ObjectCase, "base"))
		{
			AppendGeneratedAsLine(Source, TEXT("class FKindDerivedWitness : FKindObject"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendBaseCallTypes(
		FString& Source,
		const FKindCase& KindCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FKindBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		AppendScriptConstructor(
			Source,
			KindCase,
			TEXT("FKindBase"),
			TEXT("\t"));
		if (IsKind(KindCase, "copy"))
		{
			AppendScriptDefaultConstructor(
				Source,
				TEXT("FKindBase"),
				TEXT("\t"));
			AppendScriptCopyConstructor(
				Source,
				TEXT("FKindBase"),
				TEXT("\t"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FKindObject : FKindBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsKind(KindCase, "implicit_default")
			|| IsKind(KindCase, "declared_default"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper();"));
			if (IsKind(KindCase, "implicit_default"))
			{
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("\t\tRecordConstructorKindSelected(%d);"),
					KindCase.Marker));
			}
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsKind(KindCase, "parameterized"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper(InValue);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsKind(KindCase, "overloaded"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject(int Left, int Right)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper(Left, Right);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else if (IsKind(KindCase, "copy"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject(const FKindBase& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper(Other);"));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tRecordConstructorKindSelected(%d);"),
				KindCase.Marker));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject(int8 InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tsuper(int64(InValue));"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString SelectedExpression(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase)
	{
		const FString TypeName = ObjectTypeName(ObjectCase);
		if (IsObject(ObjectCase, "native_reference"))
		{
			if (IsKind(KindCase, "implicit_default"))
			{
				return TEXT("MakeNativeReferenceImplicit()");
			}
			if (IsKind(KindCase, "declared_default"))
			{
				return TEXT("MakeNativeReferenceDeclared()");
			}
			if (IsKind(KindCase, "parameterized"))
			{
				return TEXT("MakeNativeReferenceParameterized(7)");
			}
			if (IsKind(KindCase, "overloaded"))
			{
				return TEXT("MakeNativeReferenceOverloaded(3, 4)");
			}
			return TEXT("MakeNativeReferenceConversion(int64(int8(7)))");
		}
		if (IsKind(KindCase, "implicit_default"))
		{
			return TypeName + TEXT("()");
		}
		if (IsKind(KindCase, "declared_default"))
		{
			return TypeName + TEXT("()");
		}
		if (IsKind(KindCase, "parameterized"))
		{
			return TypeName + TEXT("(7)");
		}
		if (IsKind(KindCase, "overloaded"))
		{
			return TypeName + TEXT("(3, 4)");
		}
		return TypeName + TEXT("(int64(int8(7)))");
	}

	static bool SelectionMarkerComesFromConstructor(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase)
	{
		return IsScriptObject(ObjectCase)
			&& (IsKind(KindCase, "declared_default")
				|| IsKind(KindCase, "parameterized")
				|| IsKind(KindCase, "overloaded")
				|| IsKind(KindCase, "conversion"));
	}

	static int32 ExpectedSelectionMarkerCount(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		if (!SelectionMarkerComesFromConstructor(ObjectCase, KindCase)
			|| !IsKind(KindCase, "declared_default"))
		{
			return 1;
		}
		if (IsScriptObject(ObjectCase)
			&& IsCall(CallCase, "assignment"))
		{
			return 2;
		}
		if (IsObject(ObjectCase, "script_value")
			&& (IsCall(CallCase, "field")
				|| IsCall(CallCase, "temporary")
				|| IsCall(CallCase, "return")))
		{
			return 2;
		}
		return 1;
	}

	static int32 ExpectedScriptCopyConstructorCount(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		if (!IsObject(ObjectCase, "script_value"))
		{
			return 0;
		}
		if (IsCall(CallCase, "field"))
		{
			return IsKind(KindCase, "copy") ? 2 : 1;
		}
		if (IsCall(CallCase, "copy_declaration"))
		{
			return IsKind(KindCase, "copy") ? 2 : 1;
		}
		if (IsKind(KindCase, "copy"))
		{
			return 1;
		}
		return 0;
	}

	static bool IsScriptValueCopyTraceCell(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		return ExpectedScriptCopyConstructorCount(
			ObjectCase,
			KindCase,
			CallCase) > 0;
	}

	static void AppendSelectionMarkerIfNeeded(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		if (!SelectionMarkerComesFromConstructor(ObjectCase, KindCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sRecordConstructorKindSelected(%d);"),
				Indent,
				KindCase.Marker));
		}
	}

	static void AppendReadFunction(
		FString& Source,
		const FObjectCase& ObjectCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TypeName = ObjectTypeName(ObjectCase);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ReadKindObject(%s Value)"),
			*TypeName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendSelectedConstruction(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FString& VariableName,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TypeName = ObjectTypeName(ObjectCase);
		if (IsKind(KindCase, "copy"))
		{
			if (IsReferenceObject(ObjectCase))
			{
				const FString SourceExpression =
					IsObject(ObjectCase, "native_reference")
						? TEXT("MakeNativeReferenceImplicit()")
						: TypeName + TEXT("()");
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("%s%s CopySource = %s;"),
					Indent,
					*TypeName,
					*SourceExpression));
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("%sCopySource.Value = 7;"),
					Indent));
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("%s%s %s = CopySource;"),
					Indent,
					*TypeName,
					*VariableName));
			}
			else
			{
				if (IsObject(ObjectCase, "native_value"))
				{
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("%s%s CopySource(7);"),
						Indent,
						*TypeName));
				}
				else
				{
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("%s%s CopySource;"),
						Indent,
						*TypeName));
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("%sCopySource.Value = 7;"),
						Indent));
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("%sCopySource.Payload.Value = 7;"),
						Indent));
				}
				AppendGeneratedAsLine(Source, FString::Printf(
					TEXT("%s%s %s(CopySource);"),
					Indent,
					*TypeName,
					*VariableName));
			}
			AppendSelectionMarkerIfNeeded(
				Source,
				ObjectCase,
				KindCase,
				Indent);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sCopySource.Value = 9;"),
				Indent));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sRecordConstructorTransferObservation(CopySource.Value, %s.Value);"),
				Indent,
				*VariableName));
			return;
		}

		if (IsKind(KindCase, "implicit_default")
			&& IsValueObject(ObjectCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s %s;"),
				Indent,
				*TypeName,
				*VariableName));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s %s = %s;"),
				Indent,
				*TypeName,
				*VariableName,
				*SelectedExpression(ObjectCase, KindCase)));
		}
		AppendSelectionMarkerIfNeeded(
			Source,
			ObjectCase,
			KindCase,
			Indent);
	}

	static void AppendMakeSelectedFunction(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TypeName = ObjectTypeName(ObjectCase);
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%s MakeSelectedKindObject()"),
			*TypeName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSelectedConstruction(
			Source,
			ObjectCase,
			KindCase,
			TEXT("Selected"),
			TEXT("\t"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Selected;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendLocalCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSelectedConstruction(
			Source,
			ObjectCase,
			KindCase,
			TEXT("Selected"),
			TEXT("\t"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn Selected.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendTemporaryCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendMakeSelectedFunction(Source, ObjectCase, KindCase);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn ReadKindObject(MakeSelectedKindObject());"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendFieldCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendMakeSelectedFunction(Source, ObjectCase, KindCase);
		AppendGeneratedAsLine(Source, TEXT("struct FKindFieldOwner"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%s Stored = MakeSelectedKindObject();"),
			*ObjectTypeName(ObjectCase)));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFKindFieldOwner Owner = FKindFieldOwner();"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn Owner.Stored.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendReturnCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendMakeSelectedFunction(Source, ObjectCase, KindCase);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn MakeSelectedKindObject().Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendArgumentCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSelectedConstruction(
			Source,
			ObjectCase,
			KindCase,
			TEXT("Selected"),
			TEXT("\t"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn ReadKindObject(Selected);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendCopyDeclarationCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TypeName = ObjectTypeName(ObjectCase);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSelectedConstruction(
			Source,
			ObjectCase,
			KindCase,
			TEXT("Selected"),
			TEXT("\t"));
		if (IsReferenceObject(ObjectCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Copy = Selected;"),
				*TypeName));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Copy(Selected);"),
				*TypeName));
		}
		AppendGeneratedAsLine(Source, TEXT("\tSelected.Value = 9;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordConstructorTransferObservation(Selected.Value, Copy.Value);"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn Copy.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendAssignmentCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString TypeName = ObjectTypeName(ObjectCase);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSelectedConstruction(
			Source,
			ObjectCase,
			KindCase,
			TEXT("Selected"),
			TEXT("\t"));
		if (IsReferenceObject(ObjectCase))
		{
			const FString TargetExpression =
				IsObject(ObjectCase, "native_reference")
					? TEXT("MakeNativeReferenceImplicit()")
					: TypeName + TEXT("()");
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Target = %s;"),
				*TypeName,
				*TargetExpression));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s Target;"),
				*TypeName));
		}
		AppendGeneratedAsLine(Source, TEXT("\tTarget = Selected;"));
		AppendGeneratedAsLine(Source, TEXT("\tSelected.Value = 9;"));
		AppendGeneratedAsLine(
			Source,
			TEXT("\tRecordConstructorTransferObservation(Selected.Value, Target.Value);"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn Target.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendBaseCall(
		FString& Source,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendBaseCallTypes(Source, KindCase);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsKind(KindCase, "implicit_default")
			|| IsKind(KindCase, "declared_default"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject Selected = FKindObject();"));
		}
		else if (IsKind(KindCase, "parameterized"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject Selected = FKindObject(7);"));
		}
		else if (IsKind(KindCase, "overloaded"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject Selected = FKindObject(3, 4);"));
		}
		else if (IsKind(KindCase, "copy"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindBase Source = FKindBase();"));
			AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 7;"));
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject Selected = FKindObject(Source);"));
			AppendGeneratedAsLine(Source, TEXT("\tSource.Value = 9;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\tRecordConstructorTransferObservation(Source.Value, Selected.Value);"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tFKindObject Selected = FKindObject(int8(7));"));
		}
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn Selected.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendInvalidBaseCall(
		FString& Source,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelection()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tsuper();"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorCallRoute(%d);"),
			CallCase.RouteMarker));
		AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static void AppendEntryForCall(
		FString& Source,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		if (IsCall(CallCase, "local"))
		{
			AppendLocalCall(Source, ObjectCase, KindCase, CallCase);
		}
		else if (IsCall(CallCase, "temporary"))
		{
			AppendTemporaryCall(Source, ObjectCase, KindCase, CallCase);
		}
		else if (IsCall(CallCase, "field"))
		{
			AppendFieldCall(Source, ObjectCase, KindCase, CallCase);
		}
		else if (IsCall(CallCase, "return"))
		{
			AppendReturnCall(Source, ObjectCase, KindCase, CallCase);
		}
		else if (IsCall(CallCase, "argument"))
		{
			AppendArgumentCall(Source, ObjectCase, KindCase, CallCase);
		}
		else if (IsCall(CallCase, "copy_declaration"))
		{
			AppendCopyDeclarationCall(
				Source,
				ObjectCase,
				KindCase,
				CallCase);
		}
		else
		{
			AppendAssignmentCall(
				Source,
				ObjectCase,
				KindCase,
				CallCase);
		}
	}

	static void AppendRecoveryFunction(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorSelectionRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorSelectionSource(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		FString Source;
		if (IsCall(CallCase, "base_call"))
		{
			if (IsObject(ObjectCase, "derived"))
			{
				AppendBaseCall(Source, KindCase, CallCase);
			}
			else
			{
				if (IsScriptObject(ObjectCase))
				{
					AppendScriptObjectType(Source, ObjectCase, KindCase);
				}
				AppendInvalidBaseCall(Source, CallCase);
			}
			AppendRecoveryFunction(Source);
			return Source;
		}

		if (IsScriptObject(ObjectCase))
		{
			AppendScriptObjectType(Source, ObjectCase, KindCase);
		}
		AppendReadFunction(Source, ObjectCase);
		AppendEntryForCall(Source, ObjectCase, KindCase, CallCase);
		AppendRecoveryFunction(Source);
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

		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static asIScriptFunction* FindConstructor(
		asITypeInfo& Type,
		asIScriptModule& Module,
		const FKindCase& KindCase)
	{
		int32 ParamCount = 0;
		FString ParamType;
		if (IsKind(KindCase, "parameterized"))
		{
			ParamCount = 1;
			ParamType = TEXT("int");
		}
		else if (IsKind(KindCase, "overloaded"))
		{
			ParamCount = 2;
			ParamType = TEXT("int");
		}
		else if (IsKind(KindCase, "copy"))
		{
			ParamCount = 1;
			ParamType = UTF8_TO_TCHAR(Type.GetName());
		}
		else if (IsKind(KindCase, "conversion"))
		{
			ParamCount = 1;
			ParamType = TEXT("int64");
		}

		const FTCHARToUTF8 ParamTypeUtf8(*ParamType);
		const int32 ExpectedTypeId =
			ParamCount > 0
				? Module.GetTypeIdByDecl(ParamTypeUtf8.Get())
				: asTYPEID_VOID;
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function == nullptr
				|| Behaviour != asBEHAVE_CONSTRUCT
				|| static_cast<int32>(Function->GetParamCount())
					!= ParamCount)
			{
				continue;
			}
			bool bMatches = true;
			for (int32 ParameterIndex = 0;
				ParameterIndex < ParamCount;
				++ParameterIndex)
			{
				int TypeId = asTYPEID_VOID;
				if (Function->GetParam(
					static_cast<asUINT>(ParameterIndex),
					&TypeId) < 0
					|| TypeId != ExpectedTypeId)
				{
					bMatches = false;
					break;
				}
			}
			if (bMatches)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static asIScriptFunction* FindDestructor(asITypeInfo& Type)
	{
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function != nullptr && Behaviour == asBEHAVE_DESTRUCT)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static bool DefaultDestructorReleasesHandleMember(
		asIScriptFunction& Destructor,
		const asITypeInfo& ExpectedHandleType)
	{
		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Destructor.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		enum class EReleaseStep : uint8
		{
			ExpectNull,
			ExpectThis,
			ExpectMemberAddress,
			ExpectTypedReferenceCopy,
		};
		EReleaseStep Step = EReleaseStep::ExpectNull;
		const asPWORD ExpectedOperand =
			reinterpret_cast<asPWORD>(&ExpectedHandleType);
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				return false;
			}

			if (Step == EReleaseStep::ExpectNull)
			{
				if (Opcode == asBC_PshNull)
				{
					Step = EReleaseStep::ExpectThis;
				}
			}
			else if (Step == EReleaseStep::ExpectThis)
			{
				if (Opcode == asBC_PshVPtr
					&& asBC_SWORDARG0(&Bytecode[DwordIndex]) == 0)
				{
					Step = EReleaseStep::ExpectMemberAddress;
				}
				else
				{
					Step = Opcode == asBC_PshNull
						? EReleaseStep::ExpectThis
						: EReleaseStep::ExpectNull;
				}
			}
			else if (Step == EReleaseStep::ExpectMemberAddress)
			{
				Step = Opcode == asBC_ADDSi
					? EReleaseStep::ExpectTypedReferenceCopy
					: EReleaseStep::ExpectNull;
			}
			else if (Step == EReleaseStep::ExpectTypedReferenceCopy)
			{
				if (Opcode == asBC_REFCPY
					&& asBC_PTRARG(&Bytecode[DwordIndex]) == ExpectedOperand)
				{
					return true;
				}
				else
				{
					Step = EReleaseStep::ExpectNull;
				}
			}
			else
			{
				Step = EReleaseStep::ExpectNull;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return false;
	}

	static FString DescribeDefaultDestructorBytecode(
		asIScriptFunction& Destructor)
	{
		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Destructor.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return TEXT("<empty>");
		}

		TArray<FString> Instructions;
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(
				*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:<invalid=%u>"),
					DwordIndex,
					static_cast<uint32>(static_cast<asBYTE>(Opcode))));
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0
				|| DwordIndex + static_cast<asUINT>(InstructionSize) > BytecodeLength)
			{
				Instructions.Add(FString::Printf(
					TEXT("%u:%hs<size=%d outside=%u>"),
					DwordIndex,
					asBCInfo[Opcode].name,
					InstructionSize,
					BytecodeLength));
				break;
			}

			FString Detail;
			if (Opcode == asBC_REFCPY)
			{
				Detail = FString::Printf(
					TEXT(" operand=%p"),
					reinterpret_cast<const void*>(static_cast<UPTRINT>(
						asBC_PTRARG(&Bytecode[DwordIndex]))));
			}
			else if (Opcode == asBC_PshVPtr || Opcode == asBC_ADDSi)
			{
				Detail = FString::Printf(
					TEXT(" arg=%d"),
					static_cast<int32>(asBC_SWORDARG0(&Bytecode[DwordIndex])));
			}
			Instructions.Add(FString::Printf(
				TEXT("%u:%hs%s"),
				DwordIndex,
				asBCInfo[Opcode].name,
				*Detail));
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return FString::Join(Instructions, TEXT("; "));
	}

	static const ANSICHAR* NativeReferenceFactoryDeclaration(
		const FKindCase& KindCase)
	{
		if (IsKind(KindCase, "implicit_default")
			|| IsKind(KindCase, "copy"))
		{
			return "FNativeCaseReference MakeNativeReferenceImplicit()";
		}
		if (IsKind(KindCase, "declared_default"))
		{
			return "FNativeCaseReference MakeNativeReferenceDeclared()";
		}
		if (IsKind(KindCase, "parameterized"))
		{
			return "FNativeCaseReference MakeNativeReferenceParameterized(int)";
		}
		if (IsKind(KindCase, "overloaded"))
		{
			return "FNativeCaseReference MakeNativeReferenceOverloaded(int, int)";
		}
		return "FNativeCaseReference MakeNativeReferenceConversion(int64)";
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		ASSERT_THAT(IsNotNull(
			Module.GetFunctionByDecl("int RunConstructorSelection()"),
			*Case.Describe(TEXT("constructor-selection module should publish its exact entry"))));
		if (IsObject(ObjectCase, "native_reference")
			&& IsCall(CallCase, "field"))
		{
			asITypeInfo* const OwnerType =
				Module.GetTypeInfoByName("FKindFieldOwner");
			asITypeInfo* const NativeReferenceType =
				ScriptEngine.GetTypeInfoByDecl("FNativeCaseReference");
			ASSERT_THAT(IsNotNull(OwnerType,
				*Case.Describe(TEXT("native-reference field should publish its value owner type"))));
			ASSERT_THAT(IsNotNull(NativeReferenceType,
				*Case.Describe(TEXT("native-reference field should expose its exact counted handle type"))));
			if (OwnerType != nullptr && NativeReferenceType != nullptr)
			{
				asIScriptFunction* const Destructor = FindDestructor(*OwnerType);
				ASSERT_THAT(IsNotNull(Destructor,
					*Case.Describe(TEXT("native-reference field owner should publish a default destructor"))));
				if (Destructor != nullptr)
				{
					const bool bReleasesHandle =
						DefaultDestructorReleasesHandleMember(
							*Destructor,
							*NativeReferenceType);
					if (!bReleasesHandle)
					{
						TestRunner->AddInfo(FString::Printf(
							TEXT("[%s] native-reference owner destructor expected-handle=%p bytecode=[%s]"),
							*Case.GetId(),
							NativeReferenceType,
							*DescribeDefaultDestructorBytecode(*Destructor)));
					}
					ASSERT_THAT(IsTrue(bReleasesHandle,
						*Case.Describe(TEXT("native-reference field owner destructor should release and clear its direct handle member"))));
				}
			}
		}
		if (IsObject(ObjectCase, "native_reference"))
		{
			const ANSICHAR* const FactoryDeclaration =
				NativeReferenceFactoryDeclaration(KindCase);
			asIScriptFunction* const Factory =
				AngelscriptNativeTestSupport::GetNativeGlobalFunctionByPublishedDeclaration(
					&ScriptEngine,
					FactoryDeclaration);
			if (Factory == nullptr)
			{
				TestRunner->AddInfo(FString::Printf(
					TEXT("[%s] native-reference factory lookup requested '%s'; engine exposes {%s}"),
					*Case.GetId(),
					UTF8_TO_TCHAR(FactoryDeclaration),
					*AngelscriptNativeTestSupport::
						CollectGlobalFunctionDeclarations(&ScriptEngine)));
			}
			ASSERT_THAT(IsNotNull(Factory,
				*Case.Describe(TEXT("native-reference selection should preserve its registered factory declaration"))));
			return;
		}

		asITypeInfo* Type = nullptr;
		if (IsCall(CallCase, "base_call")
			&& IsObject(ObjectCase, "derived"))
		{
			Type = Module.GetTypeInfoByName("FKindBase");
		}
		else if (IsObject(ObjectCase, "native_value"))
		{
			Type = ScriptEngine.GetTypeInfoByDecl("FNativeCaseValue");
		}
		else
		{
			Type = Module.GetTypeInfoByName("FKindObject");
		}
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("constructor-selection module should publish the selected object type"))));
		if (Type == nullptr)
		{
			return;
		}

		const bool bExpectedValue =
			IsObject(ObjectCase, "script_value")
			|| IsObject(ObjectCase, "native_value");
		ASSERT_THAT(AreEqual(
			bExpectedValue,
			(Type->GetFlags() & asOBJ_VALUE) != 0,
			*Case.Describe(TEXT("constructor-selection metadata should preserve value/reference kind"))));
		if (IsObject(ObjectCase, "derived")
			&& !IsCall(CallCase, "base_call"))
		{
			ASSERT_THAT(IsNotNull(Type->GetBaseType(),
				*Case.Describe(TEXT("derived constructor-selection type should preserve its base relation"))));
		}
		if (IsObject(ObjectCase, "base"))
		{
			asITypeInfo* const DerivedWitness =
				Module.GetTypeInfoByName("FKindDerivedWitness");
			ASSERT_THAT(IsNotNull(DerivedWitness,
				*Case.Describe(TEXT("base constructor-selection type should publish a derived witness"))));
			if (DerivedWitness != nullptr)
			{
				ASSERT_THAT(AreEqual(Type, DerivedWitness->GetBaseType(),
					*Case.Describe(TEXT("base constructor-selection type should be the witness's exact base"))));
			}
		}

		if (IsKind(KindCase, "copy")
			&& IsReferenceObject(ObjectCase))
		{
			return;
		}
		asIScriptFunction* const Constructor =
			FindConstructor(*Type, Module, KindCase);
		ASSERT_THAT(IsNotNull(Constructor,
			*Case.Describe(TEXT("constructor-selection metadata should resolve the selected behavior"))));
		if (Constructor != nullptr)
		{
			ASSERT_THAT(IsTrue(Constructor->GetDeclaration() != nullptr,
				*Case.Describe(TEXT("selected constructor should expose its exact declaration"))));
		}
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		const int32 DestructionCount = Lifecycle.Num(ENativeLifecycleEvent::Destruct);
		if (UsesRestrictedRawScriptReferenceLifetime(ObjectCase))
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-selection raw script-reference lifetime live=%d construct=%d destruct=%d entries=[%s]"),
				*Case.GetId(),
				Lifecycle.GetLiveObjectCount(),
				ConstructionCount,
				DestructionCount,
				*CollectNativeLifecycleEntries(Lifecycle)));
		}
		if (Lifecycle.GetLiveObjectCount() != 0
			|| ConstructionCount != DestructionCount)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-selection lifecycle live=%d construct=%d destruct=%d entries=[%s]"),
				*Case.GetId(),
				Lifecycle.GetLiveObjectCount(),
				ConstructionCount,
				DestructionCount,
				*CollectNativeLifecycleEntries(Lifecycle)));
		}
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-selection execution should leave no tracked object alive"))));
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("constructor-selection call should construct real tracked storage"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			DestructionCount,
			*Case.Describe(TEXT("constructor-selection storage should balance destruction"))));
		if (IsObject(ObjectCase, "native_value")
			&& (IsKind(KindCase, "copy")
				|| IsCall(CallCase, "copy_declaration")))
		{
			ASSERT_THAT(IsTrue(
				Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct) > 0,
				*Case.Describe(TEXT("value-object copy selection/call should invoke copy construction"))));
		}
		if (IsValueObject(ObjectCase)
			&& IsCall(CallCase, "assignment"))
		{
			ASSERT_THAT(IsTrue(
				Lifecycle.Num(ENativeLifecycleEvent::Assign) > 0,
				*Case.Describe(TEXT("value-object assignment call should invoke assignment"))));
		}
		if (IsObject(ObjectCase, "native_reference"))
		{
			ASSERT_THAT(IsTrue(
				Lifecycle.Num(ENativeLifecycleEvent::AddRef) > 0,
				*Case.Describe(TEXT("native-reference construction/transfer should add a reference"))));
			ASSERT_THAT(IsTrue(
				Lifecycle.Num(ENativeLifecycleEvent::Release) > 0,
				*Case.Describe(TEXT("native-reference construction/transfer should release references"))));
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-selection destructor should identify constructed storage"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-selection storage should not be destroyed twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("constructor-selection lifecycle identities should balance"))));
	}

	void ExecuteModule(
		const FNativeCaseContext& Case,
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FConstructorSelectionState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(
			Case,
			ObjectCase,
			KindCase,
			CallCase,
			ScriptEngine,
			Module);
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorSelection()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RunConstructorSelectionRecovery()");
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("constructor-selection cell should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		const int ExecuteResult = PrepareAndExecute(Context, Entry);
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			const char* const ExceptionText = Context->GetExceptionString();
			asIScriptFunction* const ExceptionFunction =
				Context->GetExceptionFunction();
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-selection entry result=%d exception='%s' function='%s' line=%d"),
				*Case.GetId(),
				ExecuteResult,
				UTF8_TO_TCHAR(ExceptionText != nullptr ? ExceptionText : ""),
				UTF8_TO_TCHAR(ExceptionFunction != nullptr
					? ExceptionFunction->GetDeclaration()
					: ""),
				Context->GetExceptionLineNumber()));
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("constructor-selection entry should finish"))));
		const int32 ExpectedValue =
			IsKind(KindCase, "implicit_default")
				|| (IsKind(KindCase, "declared_default")
					&& IsObject(ObjectCase, "native_value"))
				? 0
				: 7;
		const bool bHasTransferMutation =
			IsKind(KindCase, "copy")
			|| IsCall(CallCase, "copy_declaration")
			|| IsCall(CallCase, "assignment");
		const bool bAliasesTransferredStorage =
			IsReferenceObject(ObjectCase)
			&& !IsCall(CallCase, "base_call");
		const int32 ExpectedReturnedValue =
			bAliasesTransferredStorage && bHasTransferMutation
				? 9
				: ExpectedValue;
		ASSERT_THAT(AreEqual(
			ExpectedReturnedValue,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-selection entry should preserve its selected value"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-selection context should release all storage"))));
		if (IsScriptValueCopyTraceCell(ObjectCase, KindCase, CallCase))
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-selection script-value copy trace script-copy=%d native-copy=%d native-assign=%d entries=[%s]"),
				*Case.GetId(),
				State.ScriptCopyConstructorCount,
				Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct),
				Lifecycle.Num(ENativeLifecycleEvent::Assign),
				*CollectNativeLifecycleEntries(Lifecycle)));
		}
		if (IsObject(ObjectCase, "script_value"))
		{
			const int32 ExpectedScriptCopies =
				ExpectedScriptCopyConstructorCount(
					ObjectCase,
					KindCase,
					CallCase);
			ASSERT_THAT(AreEqual(
				ExpectedScriptCopies,
				State.ScriptCopyConstructorCount,
				*Case.Describe(TEXT("script-value constructor selection should invoke each expected explicit copy constructor"))));
			if (ExpectedScriptCopies > 0)
			{
				ASSERT_THAT(IsTrue(
					Lifecycle.Num(ENativeLifecycleEvent::Assign)
						>= ExpectedScriptCopies,
					*Case.Describe(TEXT("script-value copy construction should execute the embedded native assignment body"))));
			}
		}
		const int32 ExpectedSelectedMarkerCount = ExpectedSelectionMarkerCount(
			ObjectCase,
			KindCase,
			CallCase);
		if (State.SelectedMarkers.Num() != ExpectedSelectedMarkerCount)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-selection selected-kind markers expected-count=%d expected-kind=%d actual=%s"),
				*Case.GetId(),
				ExpectedSelectedMarkerCount,
				KindCase.Marker,
				*DescribeMarkers(State.SelectedMarkers)));
		}
		ASSERT_THAT(AreEqual(
			ExpectedSelectedMarkerCount,
			State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("constructor-selection cell should record every expected selected kind"))));
		for (const int32 Marker : State.SelectedMarkers)
		{
			ASSERT_THAT(AreEqual(
				KindCase.Marker,
				Marker,
				*Case.Describe(TEXT("constructor-selection marker should identify its constructed kind"))));
		}
		ASSERT_THAT(AreEqual(1, State.RouteMarkers.Num(),
			*Case.Describe(TEXT("constructor-selection cell should record one call route"))));
		if (State.RouteMarkers.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				CallCase.RouteMarker,
				State.RouteMarkers[0],
				*Case.Describe(TEXT("constructor-selection route marker should identify the call form"))));
		}
		const int32 ExpectedTransferObservations =
			(IsKind(KindCase, "copy") ? 1 : 0)
			+ (IsCall(CallCase, "copy_declaration")
				|| IsCall(CallCase, "assignment")
					? 1
					: 0);
		ASSERT_THAT(AreEqual(
			ExpectedTransferObservations,
			State.TransferSourceValues.Num(),
			*Case.Describe(TEXT("constructor-selection cell should record every post-transfer mutation"))));
		ASSERT_THAT(AreEqual(
			ExpectedTransferObservations,
			State.TransferTargetValues.Num(),
			*Case.Describe(TEXT("constructor-selection cell should record every post-transfer target"))));
		for (int32 Index = 0;
			Index < ExpectedTransferObservations
				&& Index < State.TransferSourceValues.Num()
				&& Index < State.TransferTargetValues.Num();
			++Index)
		{
			ASSERT_THAT(AreEqual(
				9,
				State.TransferSourceValues[Index],
				*Case.Describe(TEXT("post-transfer source mutation should reach its sentinel"))));
			const int32 ExpectedTarget =
				bAliasesTransferredStorage
					? 9
					: ExpectedValue;
			ASSERT_THAT(AreEqual(
				ExpectedTarget,
				State.TransferTargetValues[Index],
				*Case.Describe(TEXT("value targets should remain independent while references retain identity"))));
		}
		VerifyLifecycle(
			Case,
			ObjectCase,
			KindCase,
			CallCase,
			Lifecycle);

		const int32 SelectedBeforeRecovery =
			State.SelectedMarkers.Num();
		const int32 RoutesBeforeRecovery =
			State.RouteMarkers.Num();
		const int32 TransfersBeforeRecovery =
			State.TransferSourceValues.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Case.Describe(TEXT("constructor-selection context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Case.Describe(TEXT("constructor-selection recovery should finish"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-selection recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			SelectedBeforeRecovery,
			State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("constructor-selection recovery should select no object"))));
		ASSERT_THAT(AreEqual(
			RoutesBeforeRecovery,
			State.RouteMarkers.Num(),
			*Case.Describe(TEXT("constructor-selection recovery should traverse no call route"))));
		ASSERT_THAT(AreEqual(
			TransfersBeforeRecovery,
			State.TransferSourceValues.Num(),
			*Case.Describe(TEXT("constructor-selection recovery should perform no transfer mutation"))));
		ASSERT_THAT(AreEqual(
			TransfersBeforeRecovery,
			State.TransferTargetValues.Num(),
			*Case.Describe(TEXT("constructor-selection recovery should observe no transfer target"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-selection recovery should unprepare cleanly"))));
		Context->Release();
	}

	void RunCell(
		const FObjectCase& ObjectCase,
		const FKindCase& KindCase,
		const FCallCase& CallCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-CTOR-KIND-CALL",
			{
				ANSI_TO_TCHAR(CallCase.CatalogName),
				ANSI_TO_TCHAR(KindCase.CatalogName),
				ANSI_TO_TCHAR(ObjectCase.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("constructor-selection cell should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FConstructorSelectionState State;
		FNativeLifecycleRecorder Lifecycle;
		ASSERT_THAT(IsTrue(RegisterConstructorSelectionBridge(*ScriptEngine, State),
			*Case.Describe(TEXT("constructor-selection cell should register its trace bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			*Case.Describe(TEXT("constructor-selection cell should register native values"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			*Case.Describe(TEXT("constructor-selection cell should register native references"))));
		ASSERT_THAT(IsTrue(RegisterNativeReferenceFactories(*ScriptEngine),
			*Case.Describe(TEXT("constructor-selection cell should register native-reference factories"))));

		const FString ModuleName = FString::Printf(
			TEXT("ConstructorSelection_%hs_%hs_%hs"),
			ObjectCase.CatalogName,
			KindCase.CatalogName,
			CallCase.CatalogName);
		const FString Source = BuildConstructorSelectionSource(
			ObjectCase,
			KindCase,
			CallCase);
		Engine.ResetMessages();
		State.Reset();
		Lifecycle.Reset();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		const bool bExpectedBuild =
			ExpectedBuild(ObjectCase, KindCase, CallCase);
		if (bExpectedBuild != (BuildResult >= 0))
		{
			const FString CompilerMessages = Engine.GetMessagesText();
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s] constructor-selection build expectation=%s result=%d diagnostics:\n%s"),
				*Case.GetId(),
				bExpectedBuild ? TEXT("success") : TEXT("failure"),
				BuildResult,
				CompilerMessages.IsEmpty()
					? TEXT("<no compiler diagnostics>")
					: *CompilerMessages));
		}
		ASSERT_THAT(AreEqual(
			bExpectedBuild,
			BuildResult >= 0,
			*Case.Describe(TEXT("constructor-selection build result should match the object/call constraint"))));

		if (bExpectedBuild)
		{
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("legal constructor-selection cell should publish its module"))));
			if (Module != nullptr)
			{
				ExecuteModule(
					Case,
					ObjectCase,
					KindCase,
					CallCase,
					*ScriptEngine,
					*Module,
					State,
					Lifecycle);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
			*Case.Describe(TEXT("unsupported base-call cell should fail to compile"))));
			ASSERT_THAT(IsTrue(Engine.GetMessages().Entries.ContainsByPredicate(
				[&ObjectCase, &KindCase](const FNativeMessageEntry& Entry)
				{
					return Entry.Type == asMSGTYPE_ERROR
						&& Entry.Row > 0
						&& Entry.Column > 0
						&& (Entry.Message.Contains(TEXT("super"))
							|| (IsObject(ObjectCase, "derived")
								&& IsKind(KindCase, "implicit_default")
								&& Entry.Message.Contains(TEXT("unsafe during construction"))));
				}),
				*Case.Describe(TEXT("unsupported base call should own a located diagnostic"))));
			ASSERT_THAT(AreEqual(0, State.SelectedMarkers.Num(),
				*Case.Describe(TEXT("invalid base call should select no constructor"))));
			ASSERT_THAT(AreEqual(0, State.RouteMarkers.Num(),
				*Case.Describe(TEXT("invalid base call should traverse no runtime route"))));
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("invalid base call should construct no tracked storage"))));

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			const FCallCase& RecoveryCall = CallCases[0];
			const FString RecoverySource =
				BuildConstructorSelectionSource(
					ObjectCase,
					KindCase,
					RecoveryCall);
			Engine.ResetMessages();
			State.Reset();
			Lifecycle.Reset();
			Module = nullptr;
			ASSERT_THAT(IsTrue(CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				Module) >= 0,
				*Case.Describe(TEXT("invalid base call should allow same-object/kind local recovery"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("constructor-selection recovery should publish its module"))));
			if (Module != nullptr)
			{
				ExecuteModule(
					Case,
					ObjectCase,
					KindCase,
					RecoveryCall,
					*ScriptEngine,
					*Module,
					State,
					Lifecycle);
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("constructor-selection module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-selection module discard should leave no live object"))));
	}

public:
	TEST_METHOD(ObjectKindsByConstructorAndCall)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CTOR-KIND-CALL",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		for (const FCallCase& CallCase : CallCases)
		{
			for (const FKindCase& KindCase : KindCases)
			{
				for (const FObjectCase& ObjectCase : ObjectCases)
				{
					RunCell(ObjectCase, KindCase, CallCase);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
