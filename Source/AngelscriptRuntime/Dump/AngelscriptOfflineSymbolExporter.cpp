#include "AngelscriptOfflineSymbolExporter.h"

#include "AngelscriptOfflineContractIdentity.h"
#include "AngelscriptOfflineAdapterExporter.h"
#include "AngelscriptOfflineSymbolMetadata.h"
#include "AngelscriptEngine.h"
#include "AngelscriptInclude.h"
#include "AngelscriptType.h"
#include "Algo/Unique.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		struct FObservedType
		{
			asITypeInfo* TypeInfo = nullptr;
			ESymbolKind SymbolKind = ESymbolKind::Type;
			ETypeKind TypeKind = ETypeKind::Reference;
			FString StableId;
			FString CompleteDeclaration;
		};

		FString FromAnsi(const char* Value)
		{
			return Value == nullptr ? FString() : FString(UTF8_TO_TCHAR(Value));
		}

		FString GetTypeDeclaration(asIScriptEngine& Engine, const int TypeId)
		{
			// AngelScript represents the wildcard `?` parameter with -1. This fork's
			// GetTypeDeclaration() indexes the primitive token table before checking
			// for that sentinel, so forwarding -1 would dereference invalid data.
			if (TypeId < asTYPEID_VOID)
			{
				return TEXT("?");
			}

			// Likewise, only ask the engine to format object type ids that still
			// resolve in the final engine state. A stale/foreign id must remain an
			// explicit, deterministic declaration instead of crashing an export.
			if (TypeId > asTYPEID_LAST_PRIMITIVE
				&& Engine.GetTypeInfoById(TypeId) == nullptr)
			{
				return FString::Printf(TEXT("<unresolved-type-id:%d>"), TypeId);
			}

			return FromAnsi(Engine.GetTypeDeclaration(TypeId, true));
		}

		ETypeKind ClassifyType(const asQWORD Flags)
		{
			if ((Flags & asOBJ_ENUM) != 0)
			{
				return ETypeKind::Enum;
			}
			if ((Flags & asOBJ_TYPEDEF) != 0)
			{
				return ETypeKind::Typedef;
			}
			if ((Flags & asOBJ_FUNCDEF) != 0)
			{
				return ETypeKind::Funcdef;
			}
			if ((Flags & asOBJ_TEMPLATE) != 0)
			{
				return ETypeKind::Template;
			}
			if ((Flags & asOBJ_VALUE) != 0)
			{
				return ETypeKind::Value;
			}
			return ETypeKind::Reference;
		}

		ESymbolKind ClassifySymbol(const asQWORD Flags)
		{
			if ((Flags & asOBJ_TYPEDEF) != 0)
			{
				return ESymbolKind::Typedef;
			}
			if ((Flags & asOBJ_FUNCDEF) != 0)
			{
				return ESymbolKind::Funcdef;
			}
			return ESymbolKind::Type;
		}

		void AddFlag(
			TArray<FString>& Output,
			const asQWORD Flags,
			const asQWORD Flag,
			const TCHAR* Name)
		{
			if ((Flags & Flag) != 0)
			{
				Output.Add(Name);
			}
		}

		TArray<FString> GetTypeFlags(const asQWORD Flags)
		{
			TArray<FString> Result;
			AddFlag(Result, Flags, asOBJ_REF, TEXT("ref"));
			AddFlag(Result, Flags, asOBJ_VALUE, TEXT("value"));
			AddFlag(Result, Flags, asOBJ_GC, TEXT("gc"));
			AddFlag(Result, Flags, asOBJ_POD, TEXT("pod"));
			AddFlag(Result, Flags, asOBJ_NOHANDLE, TEXT("no-handle"));
			AddFlag(Result, Flags, asOBJ_SCOPED, TEXT("scoped"));
			AddFlag(Result, Flags, asOBJ_TEMPLATE, TEXT("template"));
			AddFlag(Result, Flags, asOBJ_ASHANDLE, TEXT("as-handle"));
			AddFlag(Result, Flags, asOBJ_NOCOUNT, TEXT("no-count"));
			AddFlag(Result, Flags, asOBJ_IMPLICIT_HANDLE, TEXT("implicit-handle"));
			AddFlag(Result, Flags, asOBJ_SCRIPT_OBJECT, TEXT("script-object"));
			AddFlag(Result, Flags, asOBJ_SHARED, TEXT("shared"));
			AddFlag(Result, Flags, asOBJ_NOINHERIT, TEXT("no-inherit"));
			AddFlag(Result, Flags, asOBJ_FUNCDEF, TEXT("funcdef"));
			AddFlag(Result, Flags, asOBJ_ENUM, TEXT("enum"));
			AddFlag(Result, Flags, asOBJ_TYPEDEF, TEXT("typedef"));
			AddFlag(Result, Flags, asOBJ_ABSTRACT, TEXT("abstract"));
			AddFlag(Result, Flags, asOBJ_DISALLOW_INSTANTIATION, TEXT("no-instantiate"));
			AddFlag(Result, Flags, asOBJ_EDITOR_ONLY, TEXT("editor-only"));
			Result.Sort();
			return Result;
		}

		FString BehaviorToString(const asEBehaviours Behavior)
		{
			switch (Behavior)
			{
			case asBEHAVE_CONSTRUCT: return TEXT("construct");
			case asBEHAVE_LIST_CONSTRUCT: return TEXT("list-construct");
			case asBEHAVE_DESTRUCT: return TEXT("destruct");
			case asBEHAVE_FACTORY: return TEXT("factory");
			case asBEHAVE_LIST_FACTORY: return TEXT("list-factory");
			case asBEHAVE_ADDREF: return TEXT("addref");
			case asBEHAVE_RELEASE: return TEXT("release");
			case asBEHAVE_GET_WEAKREF_FLAG: return TEXT("get-weakref-flag");
			case asBEHAVE_TEMPLATE_CALLBACK: return TEXT("template-callback");
			case asBEHAVE_GETREFCOUNT: return TEXT("get-refcount");
			case asBEHAVE_SETGCFLAG: return TEXT("set-gc-flag");
			case asBEHAVE_GETGCFLAG: return TEXT("get-gc-flag");
			case asBEHAVE_ENUMREFS: return TEXT("enum-refs");
			case asBEHAVE_RELEASEREFS: return TEXT("release-refs");
			default: return TEXT("unknown");
			}
		}

		ECallableKind ClassifyBehavior(const asEBehaviours Behavior)
		{
			switch (Behavior)
			{
			case asBEHAVE_CONSTRUCT:
			case asBEHAVE_LIST_CONSTRUCT:
				return ECallableKind::Constructor;
			case asBEHAVE_DESTRUCT:
				return ECallableKind::Destructor;
			case asBEHAVE_FACTORY:
			case asBEHAVE_LIST_FACTORY:
				return ECallableKind::Factory;
			default:
				return ECallableKind::Behavior;
			}
		}

		EParameterDirection GetDirection(const asDWORD Flags)
		{
			switch (Flags & 3u)
			{
			case asTM_INREF: return EParameterDirection::In;
			case asTM_OUTREF: return EParameterDirection::Out;
			case asTM_INOUTREF: return EParameterDirection::InOut;
			default: return EParameterDirection::Value;
			}
		}

		FString BuildCallableDeclaration(
			asIScriptEngine& Engine,
			asIScriptFunction& Function,
			const ECallableKind CallableKind)
		{
			asDWORD ReturnFlags = asTM_NONE;
			const int ReturnTypeId = Function.GetReturnTypeId(&ReturnFlags);
			FString ReturnType = GetTypeDeclaration(Engine, ReturnTypeId);
			if ((ReturnFlags & 3u) != asTM_NONE)
			{
				ReturnType.Append(TEXT("&"));
			}

			FString FunctionName = FromAnsi(Function.GetName());
			if (FunctionName.IsEmpty())
			{
				FunctionName = TEXT("<anonymous>");
			}

			FString Declaration;
			const bool bOmitVoidReturn =
				ReturnTypeId == asTYPEID_VOID
				&& (CallableKind == ECallableKind::Constructor
					|| CallableKind == ECallableKind::Destructor);
			if (!bOmitVoidReturn)
			{
				Declaration.Append(ReturnType);
				Declaration.AppendChar(TEXT(' '));
			}
			Declaration.Append(FunctionName);
			Declaration.AppendChar(TEXT('('));

			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function.GetParamCount();
				++ParameterIndex)
			{
				if (ParameterIndex > 0)
				{
					Declaration.Append(TEXT(", "));
				}

				int TypeId = asTYPEID_VOID;
				asDWORD Flags = asTM_NONE;
				const char* Name = nullptr;
				const char* DefaultExpression = nullptr;
				Function.GetParam(
					ParameterIndex,
					&TypeId,
					&Flags,
					&Name,
					&DefaultExpression);

				FString ParameterType = GetTypeDeclaration(Engine, TypeId);
				if ((Flags & asTM_CONST) != 0
					&& GetDirection(Flags) != EParameterDirection::Value
					&& !ParameterType.StartsWith(TEXT("const ")))
				{
					ParameterType.InsertAt(0, TEXT("const "));
				}
				Declaration.Append(ParameterType);
				switch (GetDirection(Flags))
				{
				case EParameterDirection::In:
					Declaration.Append(TEXT("&in"));
					break;
				case EParameterDirection::Out:
					Declaration.Append(TEXT("&out"));
					break;
				case EParameterDirection::InOut:
					Declaration.Append(TEXT("&inout"));
					break;
				default:
					break;
				}

				const FString ParameterName = FromAnsi(Name);
				if (!ParameterName.IsEmpty())
				{
					Declaration.AppendChar(TEXT(' '));
					Declaration.Append(ParameterName);
				}
				if (DefaultExpression != nullptr)
				{
					Declaration.Append(TEXT(" = "));
					Declaration.Append(FromAnsi(DefaultExpression));
				}
			}
			Declaration.AppendChar(TEXT(')'));
			if (Function.IsReadOnly())
			{
				Declaration.Append(TEXT(" const"));
			}
			if (Function.IsProperty())
			{
				Declaration.Append(TEXT(" property"));
			}
			if (Function.IsFinal())
			{
				Declaration.Append(TEXT(" final"));
			}
			if (Function.IsOverride())
			{
				Declaration.Append(TEXT(" override"));
			}
			return Declaration;
		}

		FString GetAccess(const asIScriptFunction& Function)
		{
			if (Function.IsPrivate())
			{
				return TEXT("private");
			}
			if (Function.IsProtected())
			{
				return TEXT("protected");
			}
			return TEXT("public");
		}

		bool HasSingleParameterOfType(
			asIScriptEngine& Engine,
			asIScriptFunction& Function,
			asITypeInfo& OwnerType)
		{
			if (Function.GetParamCount() != 1)
			{
				return false;
			}

			int ParameterTypeId = asTYPEID_VOID;
			Function.GetParam(0, &ParameterTypeId);
			return Engine.GetTypeInfoById(ParameterTypeId) == &OwnerType;
		}

		bool IsIntegralTypeId(const int TypeId)
		{
			return TypeId >= asTYPEID_INT8 && TypeId <= asTYPEID_UINT64;
		}

		void InferTypeTraitsFromCallable(
			asIScriptEngine& Engine,
			asITypeInfo& OwnerType,
			asIScriptFunction& Function,
			const ECallableKind CallableKind,
			const asEBehaviours Behavior,
			FTypeTraitsRecord& Traits)
		{
			const FString Name = FromAnsi(Function.GetName());
			const bool bSingleOwnerParameter =
				HasSingleParameterOfType(Engine, Function, OwnerType);
			const int ReturnTypeId = Function.GetReturnTypeId();

			if (CallableKind == ECallableKind::Constructor
				|| CallableKind == ECallableKind::Factory)
			{
				Traits.bConstructible = true;
				if (bSingleOwnerParameter)
				{
					Traits.bCopyConstructible = true;
				}
			}
			if (CallableKind == ECallableKind::Destructor
				|| Behavior == asBEHAVE_DESTRUCT)
			{
				Traits.bDestructible = true;
			}
			if (Name == TEXT("opAssign") && bSingleOwnerParameter)
			{
				Traits.bCopyAssignable = true;
			}
			if (bSingleOwnerParameter
				&& ((Name == TEXT("opEquals")
						&& ReturnTypeId == asTYPEID_BOOL)
					|| ((Name == TEXT("opCmp")
							|| Name == TEXT("Compare"))
						&& IsIntegralTypeId(ReturnTypeId))))
			{
				Traits.bComparable = true;
			}
			if ((Name == TEXT("GetHash") || Name == TEXT("Hash"))
				&& Function.GetParamCount() == 0
				&& IsIntegralTypeId(ReturnTypeId))
			{
				Traits.bHashable = true;
			}
		}

		void InferTypeTraitsFromFinalEngineTypeUsage(
			asIScriptEngine& ScriptEngine,
			asITypeInfo& TypeInfo,
			FTypeTraitsRecord& Traits)
		{
			// FAngelscriptTypeUsage resolves through the process-wide plugin engine.
			// ExportHostSurface also accepts isolated AngelScript engines in tests and
			// tools, so only consult the UE type system when this is the exact engine
			// whose final registration state is being observed.
			if (!FAngelscriptEngine::IsInitialized()
				|| FAngelscriptEngine::Get().GetScriptEngine() != &ScriptEngine)
			{
				return;
			}

			const FAngelscriptTypeUsage Usage =
				FAngelscriptTypeUsage::FromTypeId(TypeInfo.GetTypeId());
			if (!Usage.IsValid())
			{
				return;
			}

			// These capabilities are the same final semantic checks used by the UE
			// container bindings. Public methods alone are insufficient evidence:
			// for example FTopLevelAssetPath is hashable through its UScriptStruct
			// ops without exposing a GetHash() AngelScript method.
			Traits.bConstructible |= Usage.CanConstruct();
			Traits.bDestructible |= Usage.CanDestruct();
			if (Usage.CanCopy())
			{
				Traits.bCopyConstructible = true;
				Traits.bCopyAssignable = true;
			}
			Traits.bComparable |= Usage.CanCompare();
			Traits.bHashable |= Usage.CanHashValue();
			Traits.bTemplateEligible |= Usage.CanBeTemplateSubType();
		}

		bool AddSymbol(
			FSymbolExportResult& Result,
			TSet<FString>& SeenStableIds,
			FSymbolRecord Symbol)
		{
			if (Symbol.StableId.IsEmpty())
			{
				Result.Error = TEXT("Observed a declaration without a stable identity");
				return false;
			}
			if (SeenStableIds.Contains(Symbol.StableId))
			{
				return true;
			}
			SeenStableIds.Add(Symbol.StableId);
			Result.Symbols.Add(MoveTemp(Symbol));
			return true;
		}

		FSymbolRecord MakeCallableRecord(
			asIScriptEngine& Engine,
			asIScriptFunction& Function,
			const FString& OwnerStableId,
			const ECallableKind CallableKind,
			const FString& Behavior,
			const FOriginRecord& Origin = FOriginRecord())
		{
			FSymbolRecord Symbol;
			Symbol.Kind = ESymbolKind::Callable;
			Symbol.Callable.OwnerStableId = OwnerStableId;
			Symbol.Callable.Namespace = FromAnsi(Function.GetNamespace());
			Symbol.Callable.Name = FromAnsi(Function.GetName());
			Symbol.Callable.Declaration =
				BuildCallableDeclaration(Engine, Function, CallableKind);
			Symbol.Callable.ReturnType =
				GetTypeDeclaration(Engine, Function.GetReturnTypeId());
			Symbol.Callable.Access = GetAccess(Function);
			Symbol.Callable.Behavior = Behavior;
			Symbol.Callable.Kind = CallableKind;
			Symbol.Callable.Availability = EAvailability::Available;
			Symbol.Callable.Origin = Origin;
			Symbol.Callable.bConst = Function.IsReadOnly();
			Symbol.Callable.bStatic = Function.GetObjectType() == nullptr;
			Symbol.Callable.bPropertyAccessor = Function.IsProperty();
			Symbol.Callable.bFinal = Function.IsFinal();
			Symbol.Callable.bOverride = Function.IsOverride();

			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function.GetParamCount();
				++ParameterIndex)
			{
				int TypeId = asTYPEID_VOID;
				asDWORD Flags = asTM_NONE;
				const char* Name = nullptr;
				const char* DefaultExpression = nullptr;
				Function.GetParam(
					ParameterIndex,
					&TypeId,
					&Flags,
					&Name,
					&DefaultExpression);

				FParameterRecord Parameter;
				Parameter.Name = FromAnsi(Name);
				Parameter.TypeDeclaration = GetTypeDeclaration(Engine, TypeId);
				Parameter.Direction = GetDirection(Flags);
				Parameter.DefaultExpression = FromAnsi(DefaultExpression);
				Parameter.bHasDefault = DefaultExpression != nullptr;
				Parameter.bConst =
					((Flags & asTM_CONST) != 0
						&& Parameter.Direction != EParameterDirection::Value)
					|| (TypeId & asTYPEID_HANDLETOCONST) != 0;
				Parameter.bReference = Parameter.Direction != EParameterDirection::Value;
				Parameter.bHandle = (TypeId & asTYPEID_OBJHANDLE) != 0;
				// These legacy manual globals have no UFunction metadata to
				// supplement later. Mark their path parameter in the observed
				// contract so consumers can use stable callable identity
				// instead of treating a same-named script function as a load.
				if (Parameter.Name.Equals(TEXT("Name"), ESearchCase::IgnoreCase))
				{
					if (Symbol.Callable.Name == TEXT("LoadObject"))
					{
						Parameter.ResourceKind = TEXT("load-object");
					}
					else if (Symbol.Callable.Name == TEXT("LoadClass"))
					{
						Parameter.ResourceKind = TEXT("load-class");
					}
				}
				Symbol.Callable.Parameters.Add(MoveTemp(Parameter));
			}

			FSymbolIdentityInput Identity;
			Identity.Kind = ESymbolKind::Callable;
			Identity.Namespace = Symbol.Callable.Namespace;
			Identity.OwnerStableId = OwnerStableId;
			Identity.CompleteDeclaration = Symbol.Callable.Declaration;
			Symbol.CanonicalIdentity = MakeCanonicalSymbolIdentity(Identity);
			Symbol.StableId = MakeStableSymbolId(Identity);
			Symbol.Callable.StableId = Symbol.StableId;
			Symbol.Origin = Symbol.Callable.Origin;
			return Symbol;
		}

		FSymbolRecord MakePropertyRecord(
			const ESymbolKind SymbolKind,
			const FString& OwnerStableId,
			const FString& Namespace,
			const FString& Name,
			const FString& TypeDeclaration,
			const FString& CompleteDeclaration,
			const FString& Access,
			const bool bConst,
			const bool bStatic,
			const FOriginRecord& Origin = FOriginRecord())
		{
			FSymbolRecord Symbol;
			Symbol.Kind = SymbolKind;
			Symbol.Property.OwnerStableId = OwnerStableId;
			Symbol.Property.Namespace = Namespace;
			Symbol.Property.Name = Name;
			Symbol.Property.TypeDeclaration = TypeDeclaration;
			Symbol.Property.CompleteDeclaration = CompleteDeclaration;
			Symbol.Property.Access = Access;
			Symbol.Property.Availability = EAvailability::Available;
			Symbol.Property.Origin = Origin;
			Symbol.Property.bConst = bConst;
			Symbol.Property.bStatic = bStatic;

			FSymbolIdentityInput Identity;
			Identity.Kind = SymbolKind;
			Identity.Namespace = Namespace;
			Identity.OwnerStableId = OwnerStableId;
			Identity.CompleteDeclaration = CompleteDeclaration;
			Symbol.CanonicalIdentity = MakeCanonicalSymbolIdentity(Identity);
			Symbol.StableId = MakeStableSymbolId(Identity);
			Symbol.Property.StableId = Symbol.StableId;
			Symbol.Origin = Symbol.Property.Origin;
			return Symbol;
		}

		FString ResolveTypeStableId(
			const TMap<asITypeInfo*, FString>& StableIds,
			asITypeInfo* TypeInfo)
		{
			if (TypeInfo == nullptr)
			{
				return FString();
			}
			if (const FString* StableId = StableIds.Find(TypeInfo))
			{
				return *StableId;
			}
			return FString();
		}
	}

	FSymbolExportResult FAngelscriptOfflineSymbolExporter::ExportHostSurface(
		asIScriptEngine& ScriptEngine)
	{
		FSymbolExportResult Result;
		Result.SymbolScope.State = TEXT("final-engine-observed");
		Result.SymbolScope.Included.Add(TEXT("asIScriptEngine.final-registration"));

		const int StringFactoryReturnTypeId =
			ScriptEngine.GetStringFactoryReturnTypeId();
		asITypeInfo* const StringFactoryType =
			StringFactoryReturnTypeId >= 0
				? ScriptEngine.GetTypeInfoById(StringFactoryReturnTypeId)
				: nullptr;

		TArray<FObservedType> ObservedTypes;
		TSet<asITypeInfo*> SeenTypes;
		auto ObserveType = [&](asITypeInfo* TypeInfo)
		{
			if (TypeInfo == nullptr || SeenTypes.Contains(TypeInfo))
			{
				return;
			}
			SeenTypes.Add(TypeInfo);

			FObservedType Observed;
			Observed.TypeInfo = TypeInfo;
			Observed.SymbolKind = ClassifySymbol(TypeInfo->GetFlags());
			Observed.TypeKind = ClassifyType(TypeInfo->GetFlags());
			if (Observed.TypeKind == ETypeKind::Typedef)
			{
				Observed.CompleteDeclaration = FString::Printf(
					TEXT("typedef %s %s"),
					*GetTypeDeclaration(
						ScriptEngine,
						TypeInfo->GetTypedefTypeId()),
					*FromAnsi(TypeInfo->GetName()));
			}
			else if (Observed.TypeKind == ETypeKind::Funcdef)
			{
				if (asIScriptFunction* Signature = TypeInfo->GetFuncdefSignature())
				{
					Observed.CompleteDeclaration = BuildCallableDeclaration(
						ScriptEngine,
						*Signature,
						ECallableKind::GlobalFunction);
				}
			}
			else
			{
				Observed.CompleteDeclaration =
					GetTypeDeclaration(ScriptEngine, TypeInfo->GetTypeId());
			}
			if (Observed.CompleteDeclaration.IsEmpty())
			{
				Observed.CompleteDeclaration =
					NormalizeNamespace(FromAnsi(TypeInfo->GetNamespace()));
				if (!Observed.CompleteDeclaration.IsEmpty())
				{
					Observed.CompleteDeclaration.Append(TEXT("::"));
				}
				Observed.CompleteDeclaration.Append(FromAnsi(TypeInfo->GetName()));
			}

			FSymbolIdentityInput Identity;
			Identity.Kind = Observed.SymbolKind;
			Identity.Namespace = FromAnsi(TypeInfo->GetNamespace());
			Identity.CompleteDeclaration = Observed.CompleteDeclaration;
			Observed.StableId = MakeStableSymbolId(Identity);
			ObservedTypes.Add(MoveTemp(Observed));
		};

		for (asUINT Index = 0; Index < ScriptEngine.GetObjectTypeCount(); ++Index)
		{
			ObserveType(ScriptEngine.GetObjectTypeByIndex(Index));
		}
		for (asUINT Index = 0; Index < ScriptEngine.GetEnumCount(); ++Index)
		{
			ObserveType(ScriptEngine.GetEnumByIndex(Index));
		}
		for (asUINT Index = 0; Index < ScriptEngine.GetFuncdefCount(); ++Index)
		{
			ObserveType(ScriptEngine.GetFuncdefByIndex(Index));
		}
		for (asUINT Index = 0; Index < ScriptEngine.GetTypedefCount(); ++Index)
		{
			ObserveType(ScriptEngine.GetTypedefByIndex(Index));
		}

		TMap<asITypeInfo*, FString> TypeStableIds;
		FObservedHostMetadata ObservedMetadata;
		for (const FObservedType& Observed : ObservedTypes)
		{
			TypeStableIds.Add(Observed.TypeInfo, Observed.StableId);
			ObservedMetadata.TypesByStableId.Add(
				Observed.StableId,
				Observed.TypeInfo);
		}

		TSet<FString> SeenStableIds;
		const TCHAR* PrimitiveDeclarations[] = {
			TEXT("void"),
			TEXT("bool"),
			TEXT("int8"),
			TEXT("int16"),
			TEXT("int"),
			TEXT("int64"),
			TEXT("uint8"),
			TEXT("uint16"),
			TEXT("uint"),
			TEXT("uint64"),
			TEXT("float"),
			TEXT("double"),
		};
		for (const TCHAR* Declaration : PrimitiveDeclarations)
		{
			const FTCHARToUTF8 DeclarationUtf8(Declaration);
			const int TypeId = ScriptEngine.GetTypeIdByDecl(DeclarationUtf8.Get());
			if (TypeId < 0)
			{
				continue;
			}

			FSymbolRecord Symbol;
			Symbol.Kind = ESymbolKind::Type;
			Symbol.Type.Kind = ETypeKind::Primitive;
			Symbol.Type.Name = Declaration;
			Symbol.Type.CompleteDeclaration = Declaration;
			Symbol.Type.Availability = EAvailability::Available;
			Symbol.Type.Origin.Layer = EOriginLayer::HostSurface;
			Symbol.Type.Origin.Kind = EOriginKind::Unknown;
			Symbol.Type.CompileSize =
				TypeId == asTYPEID_VOID ? 0 : ScriptEngine.GetSizeOfPrimitiveType(TypeId);
			Symbol.Type.CompileAlignment =
				Symbol.Type.CompileSize <= 0 ? 0 : Symbol.Type.CompileSize;

			FSymbolIdentityInput Identity;
			Identity.Kind = ESymbolKind::Type;
			Identity.CompleteDeclaration = Declaration;
			Symbol.CanonicalIdentity = MakeCanonicalSymbolIdentity(Identity);
			Symbol.StableId = MakeStableSymbolId(Identity);
			Symbol.Type.StableId = Symbol.StableId;
			Symbol.Origin = Symbol.Type.Origin;
			if (!AddSymbol(Result, SeenStableIds, MoveTemp(Symbol)))
			{
				return Result;
			}
		}

		for (const FObservedType& Observed : ObservedTypes)
		{
			asITypeInfo& TypeInfo = *Observed.TypeInfo;
			const asQWORD Flags = TypeInfo.GetFlags();

			FSymbolRecord Symbol;
			Symbol.Kind = Observed.SymbolKind;
			Symbol.StableId = Observed.StableId;
			Symbol.Type.StableId = Observed.StableId;
			Symbol.Type.Namespace = FromAnsi(TypeInfo.GetNamespace());
			Symbol.Type.Name = FromAnsi(TypeInfo.GetName());
			Symbol.Type.CompleteDeclaration = Observed.CompleteDeclaration;
			Symbol.Type.BaseStableId =
				ResolveTypeStableId(TypeStableIds, TypeInfo.GetBaseType());
			Symbol.Type.Kind = Observed.TypeKind;
			Symbol.Type.Availability =
				(Flags & asOBJ_EDITOR_ONLY) != 0
					? EAvailability::EditorOnly
					: EAvailability::Available;
			Symbol.Type.Origin.Layer = EOriginLayer::HostSurface;
			Symbol.Type.Origin.Kind = EOriginKind::Unknown;
			Symbol.Type.CompileSize = static_cast<int64>(TypeInfo.GetSize());
			Symbol.Type.CompileAlignment = TypeInfo.alignment;
			Symbol.Type.bHandle =
				(Flags & asOBJ_REF) != 0 && (Flags & asOBJ_NOHANDLE) == 0;
			Symbol.Type.bTemplateDefinition = (Flags & asOBJ_TEMPLATE) != 0;
			Symbol.Type.Flags = GetTypeFlags(Flags);
			if (&TypeInfo == StringFactoryType)
			{
				Symbol.Type.Flags.Add(TEXT("string-factory"));
				Symbol.Type.Flags.Sort();
			}
			Symbol.Type.Traits.bConstructible =
				(Flags & asOBJ_DISALLOW_INSTANTIATION) == 0;
			Symbol.Type.Traits.bDestructible = true;
			Symbol.Type.Traits.bCopyConstructible =
				(Flags & (asOBJ_POD | asOBJ_APP_CLASS_COPY_CONSTRUCTOR)) != 0;
			Symbol.Type.Traits.bCopyAssignable =
				(Flags & (asOBJ_POD | asOBJ_APP_CLASS_ASSIGNMENT)) != 0;
			Symbol.Type.Traits.bGarbageCollected = (Flags & asOBJ_GC) != 0;
			Symbol.Type.Traits.bTemplateEligible =
				(Flags & asOBJ_DISALLOW_INSTANTIATION) == 0;
			InferTypeTraitsFromFinalEngineTypeUsage(
				ScriptEngine,
				TypeInfo,
				Symbol.Type.Traits);
			Symbol.Origin = Symbol.Type.Origin;

			for (asUINT InterfaceIndex = 0;
				InterfaceIndex < TypeInfo.GetInterfaceCount();
				++InterfaceIndex)
			{
				Symbol.Type.InterfaceStableIds.Add(
					ResolveTypeStableId(
						TypeStableIds,
						TypeInfo.GetInterface(InterfaceIndex)));
			}
			Symbol.Type.InterfaceStableIds.RemoveAll([](const FString& Value)
			{
				return Value.IsEmpty();
			});
			Symbol.Type.InterfaceStableIds.Sort();

			for (asUINT SubtypeIndex = 0;
				SubtypeIndex < TypeInfo.GetSubTypeCount();
				++SubtypeIndex)
			{
				Symbol.Type.TemplateSubtypeDeclarations.Add(
					GetTypeDeclaration(
						ScriptEngine,
						TypeInfo.GetSubTypeId(SubtypeIndex)));
			}

			for (asUINT EnumIndex = 0;
				EnumIndex < TypeInfo.GetEnumValueCount();
				++EnumIndex)
			{
				int EnumValue = 0;
				const FString EnumName =
					FromAnsi(TypeInfo.GetEnumValueByIndex(EnumIndex, &EnumValue));
				FSymbolIdentityInput EnumIdentity;
				EnumIdentity.Kind = ESymbolKind::EnumValue;
				EnumIdentity.Namespace = Symbol.Type.Namespace;
				EnumIdentity.OwnerStableId = Symbol.StableId;
				EnumIdentity.CompleteDeclaration =
					FString::Printf(TEXT("%s=%d"), *EnumName, EnumValue);
				Symbol.Type.EnumValues.Add({
					MakeStableSymbolId(EnumIdentity),
					EnumName,
					EnumValue,
				});
			}
			Symbol.Type.EnumValues.Sort([](
				const FEnumValueRecord& Left,
				const FEnumValueRecord& Right)
			{
				return Left.StableId < Right.StableId;
			});

			if (Observed.TypeKind == ETypeKind::Typedef)
			{
				Symbol.Type.TemplateSubtypeDeclarations.Add(
					GetTypeDeclaration(
						ScriptEngine,
						TypeInfo.GetTypedefTypeId()));
			}
			FSymbolIdentityInput TypeIdentity;
			TypeIdentity.Kind = Observed.SymbolKind;
			TypeIdentity.Namespace = Symbol.Type.Namespace;
			TypeIdentity.CompleteDeclaration = Observed.CompleteDeclaration;
			Symbol.CanonicalIdentity = MakeCanonicalSymbolIdentity(TypeIdentity);

			const int32 TypeRecordIndex = Result.Symbols.Num();
			if (!AddSymbol(Result, SeenStableIds, MoveTemp(Symbol)))
			{
				return Result;
			}
			if (Result.Symbols.Num() == TypeRecordIndex)
			{
				continue;
			}
			const FString ExportedTypeNamespace =
				Result.Symbols[TypeRecordIndex].Type.Namespace;
			TArray<FString> MemberStableIds;

			for (asUINT PropertyIndex = 0;
				PropertyIndex < TypeInfo.GetPropertyCount();
				++PropertyIndex)
			{
				const char* Name = nullptr;
				int TypeId = asTYPEID_VOID;
				bool bPrivate = false;
				bool bProtected = false;
				bool bReference = false;
				TypeInfo.GetProperty(
					PropertyIndex,
					&Name,
					&TypeId,
					&bPrivate,
					&bProtected,
					nullptr,
					&bReference);

				const FString Access =
					bPrivate ? TEXT("private")
					: bProtected ? TEXT("protected")
					: TEXT("public");
				FSymbolRecord Property = MakePropertyRecord(
					ESymbolKind::Property,
					Observed.StableId,
					ExportedTypeNamespace,
					FromAnsi(Name),
					GetTypeDeclaration(ScriptEngine, TypeId),
					FromAnsi(TypeInfo.GetPropertyDeclaration(PropertyIndex, true)),
					Access,
					false,
					false);
				Property.Property.bReadable = true;
				Property.Property.bWritable = true;
				MemberStableIds.Add(Property.StableId);
				if (!AddSymbol(Result, SeenStableIds, MoveTemp(Property)))
				{
					return Result;
				}
			}

			for (asUINT MethodIndex = 0;
				MethodIndex < TypeInfo.GetMethodCount();
				++MethodIndex)
			{
				if (asIScriptFunction* Method = TypeInfo.GetMethodByIndex(MethodIndex))
				{
					FSymbolRecord Callable = MakeCallableRecord(
						ScriptEngine,
						*Method,
						Observed.StableId,
						ECallableKind::Method,
						FString());
					ObservedMetadata.FunctionsByStableId.Add(
						Callable.StableId,
						Method);
					InferTypeTraitsFromCallable(
						ScriptEngine,
						TypeInfo,
						*Method,
						ECallableKind::Method,
						asBEHAVE_MAX,
						Result.Symbols[TypeRecordIndex].Type.Traits);
					MemberStableIds.Add(Callable.StableId);
					if (!AddSymbol(Result, SeenStableIds, MoveTemp(Callable)))
					{
						return Result;
					}
				}
			}

			for (asUINT FactoryIndex = 0;
				FactoryIndex < TypeInfo.GetFactoryCount();
				++FactoryIndex)
			{
				if (asIScriptFunction* Factory = TypeInfo.GetFactoryByIndex(FactoryIndex))
				{
					FSymbolRecord Callable = MakeCallableRecord(
						ScriptEngine,
						*Factory,
						Observed.StableId,
						ECallableKind::Factory,
						TEXT("factory"));
					ObservedMetadata.FunctionsByStableId.Add(
						Callable.StableId,
						Factory);
					InferTypeTraitsFromCallable(
						ScriptEngine,
						TypeInfo,
						*Factory,
						ECallableKind::Factory,
						asBEHAVE_FACTORY,
						Result.Symbols[TypeRecordIndex].Type.Traits);
					MemberStableIds.Add(Callable.StableId);
					if (!AddSymbol(Result, SeenStableIds, MoveTemp(Callable)))
					{
						return Result;
					}
				}
			}

			for (asUINT BehaviorIndex = 0;
				BehaviorIndex < TypeInfo.GetBehaviourCount();
				++BehaviorIndex)
			{
				asEBehaviours Behavior = asBEHAVE_MAX;
				if (asIScriptFunction* Function =
					TypeInfo.GetBehaviourByIndex(BehaviorIndex, &Behavior))
				{
					FSymbolRecord Callable = MakeCallableRecord(
						ScriptEngine,
						*Function,
						Observed.StableId,
						ClassifyBehavior(Behavior),
						BehaviorToString(Behavior));
					ObservedMetadata.FunctionsByStableId.Add(
						Callable.StableId,
						Function);
					InferTypeTraitsFromCallable(
						ScriptEngine,
						TypeInfo,
						*Function,
						ClassifyBehavior(Behavior),
						Behavior,
						Result.Symbols[TypeRecordIndex].Type.Traits);
					MemberStableIds.Add(Callable.StableId);
					if (!AddSymbol(Result, SeenStableIds, MoveTemp(Callable)))
					{
						return Result;
					}
				}
			}
			MemberStableIds.Sort();
			MemberStableIds.SetNum(Algo::Unique(MemberStableIds));
			Result.Symbols[TypeRecordIndex].Type.MemberStableIds =
				MoveTemp(MemberStableIds);
		}

		for (asUINT FunctionIndex = 0;
			FunctionIndex < ScriptEngine.GetGlobalFunctionCount();
			++FunctionIndex)
		{
			if (asIScriptFunction* Function =
				ScriptEngine.GetGlobalFunctionByIndex(FunctionIndex))
			{
				FSymbolRecord Callable = MakeCallableRecord(
					ScriptEngine,
					*Function,
					FString(),
					ECallableKind::GlobalFunction,
					FString());
				ObservedMetadata.FunctionsByStableId.Add(
					Callable.StableId,
					Function);
				if (!AddSymbol(
					Result,
					SeenStableIds,
					MoveTemp(Callable)))
				{
					return Result;
				}
			}
		}

		for (asUINT PropertyIndex = 0;
			PropertyIndex < ScriptEngine.GetGlobalPropertyCount();
			++PropertyIndex)
		{
			const char* Name = nullptr;
			const char* Namespace = nullptr;
			int TypeId = asTYPEID_VOID;
			bool bConst = false;
			if (ScriptEngine.GetGlobalPropertyByIndex(
				PropertyIndex,
				&Name,
				&Namespace,
				&TypeId,
				&bConst) < 0)
			{
				Result.Error = FString::Printf(
					TEXT("Failed to enumerate global property %u"),
					PropertyIndex);
				return Result;
			}

			const FString TypeDeclaration =
				GetTypeDeclaration(ScriptEngine, TypeId);
			const FString CompleteDeclaration = FString::Printf(
				TEXT("%s%s %s"),
				bConst ? TEXT("const ") : TEXT(""),
				*TypeDeclaration,
				*FromAnsi(Name));
			if (!AddSymbol(
				Result,
				SeenStableIds,
				MakePropertyRecord(
					ESymbolKind::Global,
					FString(),
					FromAnsi(Namespace),
					FromAnsi(Name),
					TypeDeclaration,
					CompleteDeclaration,
					TEXT("public"),
					bConst,
					true)))
			{
				return Result;
			}
		}

		SupplementWithCurrentUnrealMetadata(
			ScriptEngine,
			ObservedMetadata,
			Result.Symbols);
		Result.Adapters =
			FAngelscriptOfflineAdapterExporter::ExportAndAssign(
				Result.Symbols);

		Result.Symbols.Sort([](
			const FSymbolRecord& Left,
			const FSymbolRecord& Right)
		{
			if (Left.StableId != Right.StableId)
			{
				return Left.StableId < Right.StableId;
			}
			return static_cast<uint8>(Left.Kind) < static_cast<uint8>(Right.Kind);
		});
		Result.SymbolScope.bComplete = true;
		Result.bSuccess = true;
		return Result;
	}

	FSymbolExportResult FAngelscriptOfflineSymbolExporter::ExportScriptBaseline(
		FAngelscriptEngine& AngelscriptEngine)
	{
		FSymbolExportResult Result;
		Result.SymbolScope.State = TEXT("active-script-baseline-observed");
		asIScriptEngine* const ScriptEngine =
			AngelscriptEngine.GetScriptEngine();
		if (ScriptEngine == nullptr)
		{
			Result.Error = TEXT(
				"Cannot export a script baseline without a script engine");
			return Result;
		}

		struct FObservedModule
		{
			TSharedRef<FAngelscriptModuleDesc> Description;
			asIScriptModule* ScriptModule = nullptr;
			FOriginRecord Origin;
		};

		TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules =
			AngelscriptEngine.GetActiveModules();
		ActiveModules.Sort([](
			const TSharedRef<FAngelscriptModuleDesc>& Left,
			const TSharedRef<FAngelscriptModuleDesc>& Right)
		{
			return Left->ModuleName.Compare(
				Right->ModuleName,
				ESearchCase::CaseSensitive) < 0;
		});

		TArray<FObservedModule> Modules;
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			if (Module->ScriptModule == nullptr
				|| Module->bCompileError
				|| Module->bModuleSwapInError)
			{
				Result.SymbolScope.Skipped.Add(
					FString::Printf(
						TEXT("%s:not-a-successful-active-module"),
						*Module->ModuleName));
				continue;
			}

			TArray<FString> VirtualSourceIdentities;
			for (const FAngelscriptModuleDesc::FCodeSection& Section :
				Module->Code)
			{
				FString Identity = Section.VirtualPath;
				if (Identity.IsEmpty())
				{
					Identity = Section.RelativeFilename;
				}
				if (Identity.IsEmpty())
				{
					Identity = FPaths::GetCleanFilename(
						Section.AbsoluteFilename);
				}
				Identity = NormalizeSemanticPath(Identity);
				if (!Identity.IsEmpty())
				{
					VirtualSourceIdentities.Add(MoveTemp(Identity));
				}
			}
			VirtualSourceIdentities.Sort();
			VirtualSourceIdentities.SetNum(
				Algo::Unique(VirtualSourceIdentities));
			const FString StableModuleId = MakeStableModuleId(
				Module->ModuleName,
				FString::Join(
					VirtualSourceIdentities,
					TEXT("\n")));

			FObservedModule Observed{Module};
			Observed.ScriptModule = Module->ScriptModule;
			Observed.Origin.Layer = EOriginLayer::ScriptBaseline;
			Observed.Origin.Kind = EOriginKind::Script;
			Observed.Origin.Module = Module->ModuleName;
			Observed.Origin.StableModuleId = StableModuleId;
			Modules.Add(MoveTemp(Observed));
			Result.SymbolScope.Included.Add(StableModuleId);
		}

		struct FBaselineType
		{
			FObservedType Type;
			FOriginRecord Origin;
		};

		TArray<FBaselineType> ObservedTypes;
		TSet<asITypeInfo*> SeenTypes;
		auto ObserveType = [&](
			asITypeInfo* TypeInfo,
			const FOriginRecord& Origin,
			const TOptional<ESymbolKind> SymbolKindOverride =
				TOptional<ESymbolKind>(),
			const TOptional<ETypeKind> TypeKindOverride =
				TOptional<ETypeKind>())
		{
			if (TypeInfo == nullptr || SeenTypes.Contains(TypeInfo))
			{
				return;
			}
			SeenTypes.Add(TypeInfo);

			FBaselineType Baseline;
			Baseline.Origin = Origin;
			Baseline.Type.TypeInfo = TypeInfo;
			Baseline.Type.SymbolKind = SymbolKindOverride.IsSet()
				? SymbolKindOverride.GetValue()
				: ClassifySymbol(TypeInfo->GetFlags());
			Baseline.Type.TypeKind = TypeKindOverride.IsSet()
				? TypeKindOverride.GetValue()
				: ClassifyType(TypeInfo->GetFlags());
			if (Baseline.Type.TypeKind == ETypeKind::Typedef)
			{
				Baseline.Type.CompleteDeclaration = FString::Printf(
					TEXT("typedef %s %s"),
					*GetTypeDeclaration(
						*ScriptEngine,
						TypeInfo->GetTypedefTypeId()),
					*FromAnsi(TypeInfo->GetName()));
			}
			else if (
				Baseline.Type.TypeKind == ETypeKind::Funcdef
				|| Baseline.Type.TypeKind == ETypeKind::Delegate)
			{
				if (asIScriptFunction* Signature =
					TypeInfo->GetFuncdefSignature())
				{
					Baseline.Type.CompleteDeclaration =
						BuildCallableDeclaration(
							*ScriptEngine,
							*Signature,
							ECallableKind::GlobalFunction);
				}
			}
			else
			{
				Baseline.Type.CompleteDeclaration =
					GetTypeDeclaration(
						*ScriptEngine,
						TypeInfo->GetTypeId());
			}
			if (Baseline.Type.CompleteDeclaration.IsEmpty())
			{
				Baseline.Type.CompleteDeclaration =
					NormalizeNamespace(
						FromAnsi(TypeInfo->GetNamespace()));
				if (!Baseline.Type.CompleteDeclaration.IsEmpty())
				{
					Baseline.Type.CompleteDeclaration.Append(TEXT("::"));
				}
				Baseline.Type.CompleteDeclaration.Append(
					FromAnsi(TypeInfo->GetName()));
			}

			FSymbolIdentityInput Identity;
			Identity.Kind = Baseline.Type.SymbolKind;
			Identity.Namespace = FromAnsi(TypeInfo->GetNamespace());
			Identity.CompleteDeclaration =
				Baseline.Type.CompleteDeclaration;
			Baseline.Type.StableId = MakeStableSymbolId(Identity);
			ObservedTypes.Add(MoveTemp(Baseline));
		};

		for (const FObservedModule& Module : Modules)
		{
			TSet<asITypeInfo*> DelegateTypes;
			for (const TSharedRef<FAngelscriptDelegateDesc>& Delegate :
				Module.Description->Delegates)
			{
				if (Delegate->ScriptType != nullptr)
				{
					DelegateTypes.Add(Delegate->ScriptType);
				}
			}

			for (asUINT Index = 0;
				Index < Module.ScriptModule->GetObjectTypeCount();
				++Index)
			{
				ObserveType(
					Module.ScriptModule->GetObjectTypeByIndex(Index),
					Module.Origin);
			}
			for (asUINT Index = 0;
				Index < Module.ScriptModule->GetEnumCount();
				++Index)
			{
				ObserveType(
					Module.ScriptModule->GetEnumByIndex(Index),
					Module.Origin);
			}
			for (asUINT Index = 0;
				Index < Module.ScriptModule->GetTypedefCount();
				++Index)
			{
				ObserveType(
					Module.ScriptModule->GetTypedefByIndex(Index),
					Module.Origin);
			}
			for (asUINT Index = 0;
				Index < ScriptEngine->GetFuncdefCount();
				++Index)
			{
				asITypeInfo* TypeInfo =
					ScriptEngine->GetFuncdefByIndex(Index);
				if (TypeInfo == nullptr
					|| TypeInfo->GetModule() != Module.ScriptModule)
				{
					continue;
				}
				if (DelegateTypes.Contains(TypeInfo))
				{
					ObserveType(
						TypeInfo,
						Module.Origin,
						ESymbolKind::Delegate,
						ETypeKind::Delegate);
				}
				else
				{
					ObserveType(TypeInfo, Module.Origin);
				}
			}
		}

		TMap<asITypeInfo*, FString> TypeStableIds;
		for (const FBaselineType& Observed : ObservedTypes)
		{
			TypeStableIds.Add(
				Observed.Type.TypeInfo,
				Observed.Type.StableId);
		}

		auto ResolveAnyTypeStableId =
			[&](asITypeInfo* TypeInfo) -> FString
		{
			if (TypeInfo == nullptr)
			{
				return FString();
			}
			if (const FString* StableId =
				TypeStableIds.Find(TypeInfo))
			{
				return *StableId;
			}

			FSymbolIdentityInput Identity;
			Identity.Kind = ClassifySymbol(TypeInfo->GetFlags());
			Identity.Namespace = FromAnsi(TypeInfo->GetNamespace());
			if ((TypeInfo->GetFlags() & asOBJ_TYPEDEF) != 0)
			{
				Identity.CompleteDeclaration = FString::Printf(
					TEXT("typedef %s %s"),
					*GetTypeDeclaration(
						*ScriptEngine,
						TypeInfo->GetTypedefTypeId()),
					*FromAnsi(TypeInfo->GetName()));
			}
			else if ((TypeInfo->GetFlags() & asOBJ_FUNCDEF) != 0)
			{
				if (asIScriptFunction* Signature =
					TypeInfo->GetFuncdefSignature())
				{
					Identity.CompleteDeclaration =
						BuildCallableDeclaration(
							*ScriptEngine,
							*Signature,
							ECallableKind::GlobalFunction);
				}
			}
			else
			{
				Identity.CompleteDeclaration = GetTypeDeclaration(
					*ScriptEngine,
					TypeInfo->GetTypeId());
			}
			return MakeStableSymbolId(Identity);
		};

		TSet<FString> SeenStableIds;
		for (const FBaselineType& Observed : ObservedTypes)
		{
			asITypeInfo& TypeInfo = *Observed.Type.TypeInfo;
			const asQWORD Flags = TypeInfo.GetFlags();

			FSymbolRecord Symbol;
			Symbol.Kind = Observed.Type.SymbolKind;
			Symbol.StableId = Observed.Type.StableId;
			Symbol.Type.StableId = Symbol.StableId;
			Symbol.Type.Namespace = FromAnsi(TypeInfo.GetNamespace());
			Symbol.Type.Name = FromAnsi(TypeInfo.GetName());
			Symbol.Type.CompleteDeclaration =
				Observed.Type.CompleteDeclaration;
			Symbol.Type.BaseStableId =
				ResolveAnyTypeStableId(TypeInfo.GetBaseType());
			Symbol.Type.Kind = Observed.Type.TypeKind;
			Symbol.Type.Availability =
				(Flags & asOBJ_EDITOR_ONLY) != 0
					? EAvailability::EditorOnly
					: EAvailability::Available;
			Symbol.Type.Origin = Observed.Origin;
			Symbol.Type.CompileSize =
				static_cast<int64>(TypeInfo.GetSize());
			Symbol.Type.CompileAlignment = TypeInfo.alignment;
			Symbol.Type.bHandle =
				(Flags & asOBJ_REF) != 0
				&& (Flags & asOBJ_NOHANDLE) == 0;
			Symbol.Type.bTemplateDefinition =
				(Flags & asOBJ_TEMPLATE) != 0;
			Symbol.Type.Flags = GetTypeFlags(Flags);
			Symbol.Type.Traits.bConstructible =
				(Flags & asOBJ_DISALLOW_INSTANTIATION) == 0;
			Symbol.Type.Traits.bDestructible = true;
			Symbol.Type.Traits.bCopyConstructible =
				(Flags
					& (asOBJ_POD
						| asOBJ_APP_CLASS_COPY_CONSTRUCTOR)) != 0;
			Symbol.Type.Traits.bCopyAssignable =
				(Flags
					& (asOBJ_POD
						| asOBJ_APP_CLASS_ASSIGNMENT)) != 0;
			Symbol.Type.Traits.bGarbageCollected =
				(Flags & asOBJ_GC) != 0;
			Symbol.Type.Traits.bTemplateEligible =
				(Flags & asOBJ_DISALLOW_INSTANTIATION) == 0;
			Symbol.Origin = Observed.Origin;

			for (asUINT InterfaceIndex = 0;
				InterfaceIndex < TypeInfo.GetInterfaceCount();
				++InterfaceIndex)
			{
				Symbol.Type.InterfaceStableIds.Add(
					ResolveAnyTypeStableId(
						TypeInfo.GetInterface(InterfaceIndex)));
			}
			Symbol.Type.InterfaceStableIds.RemoveAll(
				[](const FString& Value)
				{
					return Value.IsEmpty();
				});
			Symbol.Type.InterfaceStableIds.Sort();

			for (asUINT SubtypeIndex = 0;
				SubtypeIndex < TypeInfo.GetSubTypeCount();
				++SubtypeIndex)
			{
				Symbol.Type.TemplateSubtypeDeclarations.Add(
					GetTypeDeclaration(
						*ScriptEngine,
						TypeInfo.GetSubTypeId(SubtypeIndex)));
			}
			if (Observed.Type.TypeKind == ETypeKind::Typedef)
			{
				Symbol.Type.TemplateSubtypeDeclarations.Add(
					GetTypeDeclaration(
						*ScriptEngine,
						TypeInfo.GetTypedefTypeId()));
			}

			for (asUINT EnumIndex = 0;
				EnumIndex < TypeInfo.GetEnumValueCount();
				++EnumIndex)
			{
				int EnumValue = 0;
				const FString EnumName = FromAnsi(
					TypeInfo.GetEnumValueByIndex(
						EnumIndex,
						&EnumValue));
				FSymbolIdentityInput EnumIdentity;
				EnumIdentity.Kind = ESymbolKind::EnumValue;
				EnumIdentity.Namespace = Symbol.Type.Namespace;
				EnumIdentity.OwnerStableId = Symbol.StableId;
				EnumIdentity.CompleteDeclaration = FString::Printf(
					TEXT("%s=%d"),
					*EnumName,
					EnumValue);
				Symbol.Type.EnumValues.Add({
					MakeStableSymbolId(EnumIdentity),
					EnumName,
					EnumValue,
				});
			}
			Symbol.Type.EnumValues.Sort([](
				const FEnumValueRecord& Left,
				const FEnumValueRecord& Right)
			{
				return Left.StableId < Right.StableId;
			});

			FSymbolIdentityInput TypeIdentity;
			TypeIdentity.Kind = Observed.Type.SymbolKind;
			TypeIdentity.Namespace = Symbol.Type.Namespace;
			TypeIdentity.CompleteDeclaration =
				Observed.Type.CompleteDeclaration;
			Symbol.CanonicalIdentity =
				MakeCanonicalSymbolIdentity(TypeIdentity);

			const int32 TypeRecordIndex = Result.Symbols.Num();
			if (!AddSymbol(
				Result,
				SeenStableIds,
				MoveTemp(Symbol)))
			{
				return Result;
			}
			if (Result.Symbols.Num() == TypeRecordIndex)
			{
				continue;
			}

			TArray<FString> MemberStableIds;
			for (asUINT PropertyIndex = 0;
				PropertyIndex < TypeInfo.GetPropertyCount();
				++PropertyIndex)
			{
				const char* Name = nullptr;
				int TypeId = asTYPEID_VOID;
				bool bPrivate = false;
				bool bProtected = false;
				TypeInfo.GetProperty(
					PropertyIndex,
					&Name,
					&TypeId,
					&bPrivate,
					&bProtected);
				FSymbolRecord Property = MakePropertyRecord(
					ESymbolKind::Property,
					Observed.Type.StableId,
					Result.Symbols[TypeRecordIndex].Type.Namespace,
					FromAnsi(Name),
					GetTypeDeclaration(*ScriptEngine, TypeId),
					FromAnsi(TypeInfo.GetPropertyDeclaration(
						PropertyIndex,
						true)),
					bPrivate ? TEXT("private")
						: bProtected ? TEXT("protected")
						: TEXT("public"),
					false,
					false,
					Observed.Origin);
				MemberStableIds.Add(Property.StableId);
				if (!AddSymbol(
					Result,
					SeenStableIds,
					MoveTemp(Property)))
				{
					return Result;
				}
			}

			for (asUINT MethodIndex = 0;
				MethodIndex < TypeInfo.GetMethodCount();
				++MethodIndex)
			{
				if (asIScriptFunction* Method =
					TypeInfo.GetMethodByIndex(MethodIndex))
				{
					FSymbolRecord Callable = MakeCallableRecord(
						*ScriptEngine,
						*Method,
						Observed.Type.StableId,
						ECallableKind::Method,
						FString(),
						Observed.Origin);
					InferTypeTraitsFromCallable(
						*ScriptEngine,
						TypeInfo,
						*Method,
						ECallableKind::Method,
						asBEHAVE_MAX,
						Result.Symbols[TypeRecordIndex].Type.Traits);
					MemberStableIds.Add(Callable.StableId);
					if (!AddSymbol(
						Result,
						SeenStableIds,
						MoveTemp(Callable)))
					{
						return Result;
					}
				}
			}
			MemberStableIds.Sort();
			MemberStableIds.SetNum(Algo::Unique(MemberStableIds));
			Result.Symbols[TypeRecordIndex].Type.MemberStableIds =
				MoveTemp(MemberStableIds);
		}

		for (const FObservedModule& Module : Modules)
		{
			for (asUINT FunctionIndex = 0;
				FunctionIndex < Module.ScriptModule->GetFunctionCount();
				++FunctionIndex)
			{
				if (asIScriptFunction* Function =
					Module.ScriptModule->GetFunctionByIndex(
						FunctionIndex))
				{
					if (!AddSymbol(
						Result,
						SeenStableIds,
						MakeCallableRecord(
							*ScriptEngine,
							*Function,
							FString(),
							ECallableKind::GlobalFunction,
							FString(),
							Module.Origin)))
					{
						return Result;
					}
				}
			}

			for (asUINT GlobalIndex = 0;
				GlobalIndex < Module.ScriptModule->GetGlobalVarCount();
				++GlobalIndex)
			{
				const char* Name = nullptr;
				const char* Namespace = nullptr;
				int TypeId = asTYPEID_VOID;
				bool bConst = false;
				if (Module.ScriptModule->GetGlobalVar(
					GlobalIndex,
					&Name,
					&Namespace,
					&TypeId,
					&bConst) < 0)
				{
					Result.Error = FString::Printf(
						TEXT(
							"Failed to enumerate global %u in module '%s'"),
						GlobalIndex,
						*Module.Description->ModuleName);
					return Result;
				}
				if (!AddSymbol(
					Result,
					SeenStableIds,
					MakePropertyRecord(
						ESymbolKind::Global,
						FString(),
						FromAnsi(Namespace),
						FromAnsi(Name),
						GetTypeDeclaration(*ScriptEngine, TypeId),
						FromAnsi(
							Module.ScriptModule
								->GetGlobalVarDeclaration(
									GlobalIndex,
									true)),
						TEXT("public"),
						bConst,
						true,
						Module.Origin)))
				{
					return Result;
				}
			}
		}

		Result.SymbolScope.Included.Sort();
		Result.SymbolScope.Included.SetNum(
			Algo::Unique(Result.SymbolScope.Included));
		Result.SymbolScope.Skipped.Sort();
		Result.Symbols.Sort([](
			const FSymbolRecord& Left,
			const FSymbolRecord& Right)
		{
			if (Left.StableId != Right.StableId)
			{
				return Left.StableId < Right.StableId;
			}
			return static_cast<uint8>(Left.Kind)
				< static_cast<uint8>(Right.Kind);
		});
		Result.SymbolScope.bComplete = true;
		Result.bSuccess = true;
		return Result;
	}
}
