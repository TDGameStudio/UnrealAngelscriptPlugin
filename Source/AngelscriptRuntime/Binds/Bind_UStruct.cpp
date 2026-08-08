#include "Bind_UStruct.h"

#include "Engine/NetSerialization.h"
#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptType.h"

#include "Helper_PropertyBind.h"
#include "Helper_StructType.h"
#include "AngelscriptDocs.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/ScriptMacros.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/GarbageCollection.h"
//#include "GarbageCollectionSchema.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_objecttype.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#include "StaticJIT/AngelscriptStaticJIT.h"
#include "Binds/Bind_Helpers.h"
#include "ClassGenerator/ASClass.h"
#include "ClassGenerator/ASStruct.h"

/**
 * Table-driven native UStruct declaration, lifecycle, assignment, and reflected-property surfaces.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <StructType> Value();                                                                                | Default-constructs an eligible reflected native struct using its C++ struct operations or the reflective         |
 * |                                                                                                      | fallback. Expanded from the bind database, or from ForceAngelscriptBind/Blueprint-visible reflected structs when |
 * |                                                                                                      | the database is disabled.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <StructType> Value(const <StructType>& Other);                                                       | Copy-constructs an eligible struct when its native operations permit copying; the reflective path supplies the   |
 * |                                                                                                      | fallback.                                                                                                        |
 * |                                                                                                      | @param Other Struct value to copy.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | Value = Other;                                                                                       | Assigns an eligible struct when its native operations permit copying; the reflective path supplies the fallback. |
 * |                                                                                                      | @param Other Struct value to copy.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <PropertyType> Value.<ScriptPropertyName>;                                                           | Exposes each eligible reflected property with its bind-database declaration or resolved AngelScript type.        |
 * |                                                                                                      | ScriptName metadata can rename the member; read/write policy, deprecation metadata, editor-only filtering, and   |
 * |                                                                                                      | the reflected offset remain authoritative.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

static const FName NAME_Struct_Tooltip("ToolTip");

static void BindStructBehaviors(FAngelscriptBinds& Binds, const FString& TypeName, UScriptStruct* Struct)
{
	auto* Ops = Struct->GetCppStructOps();

#if AS_CAN_GENERATE_JIT
	const ANSICHAR* NativeTypeName = FScriptFunctionNativeForm::AllocateAnsiTypeName(TypeName);
#else
	const ANSICHAR* NativeTypeName = nullptr;
#endif

	if (Ops != nullptr)
	{
		// Bind constructor
		if (Ops->HasNoopConstructor())
		{
			// Binding an empty function here is a precaution, in case we are going to be reusing
			// the bytecode between platforms and this function isn't a no-op on other platforms, we still want to generate
			// calls to it even if it does nothing.
			Binds.Constructor("void f()", &FAngelscriptUStructBinds::NoopConstruct).NativeConstructor(NativeTypeName, true);
		}
		else if (Ops->HasZeroConstructor())
		{
			Binds.Constructor("void f()", &FAngelscriptUStructBinds::ZeroConstruct, Ops).NativeConstructor(NativeTypeName, true);
		}
		else
		{
			Binds.Constructor("void f()", &FAngelscriptUStructBinds::Construct, Ops).NativeConstructor(NativeTypeName, true);
		}

		// Bind destructor
		if (Ops->HasDestructor())
		{
			Binds.Destructor("void f()", &FAngelscriptUStructBinds::Destruct, Ops).NativeDestructor(NativeTypeName, true);
		}
		else
		{
			// Binding an empty function here is a precaution, in case we are going to be reusing
			// the bytecode between platforms and this function isn't a no-op on other platforms, we still want to generate
			// calls to it even if it does nothing.
			Binds.Destructor("void f()", &FAngelscriptUStructBinds::NoopDestruct).NativeDestructor(NativeTypeName, true);
		}

		// Bind copy operations
		FString CopyConstructDecl = FString::Printf(TEXT("void f(const %s& Other)"), *TypeName);
		FString AssignDecl = FString::Printf(TEXT("%s& opAssign(const %s& Other)"), *TypeName, *TypeName);
		if (Ops->IsPlainOldData())
		{
			Binds.Constructor(CopyConstructDecl, &FAngelscriptUStructBinds::PodCopyConstruct, Ops)
				.NativeConstructor(NativeTypeName, true);

			Binds.Method(AssignDecl, &FAngelscriptUStructBinds::PodAssign, Ops).NativeAssignment(NativeTypeName, true);
		}
		else if (Ops->HasCopy())
		{
			if (Ops->HasNoopConstructor())
			{
				Binds.Constructor(CopyConstructDecl, &FAngelscriptUStructBinds::CopyConstructWithoutInitialization, Ops)
					.NativeConstructor(NativeTypeName, true);
			}
			else if (Ops->HasZeroConstructor())
			{
				Binds.Constructor(CopyConstructDecl, &FAngelscriptUStructBinds::CopyConstructWithZeroInitialization, Ops)
					.NativeConstructor(NativeTypeName, true);
			}
			else
			{
				Binds.Constructor(CopyConstructDecl, &FAngelscriptUStructBinds::CopyConstructWithInitialization, Ops)
					.NativeConstructor(NativeTypeName, true);
			}

			Binds.Method(AssignDecl, &FAngelscriptUStructBinds::CopyAssign, Ops).NativeAssignment(NativeTypeName, true);
		}
	}
	else
	{
		// Bind constructor
		Binds.Constructor("void f()", &FAngelscriptUStructBinds::GenericConstruct, Struct)
			.NativeConstructor(NativeTypeName, true);

		// Bind destructor
		Binds.Destructor("void f()", &FAngelscriptUStructBinds::GenericDestruct, Struct)
			.NativeDestructor(NativeTypeName, true);

		// Bind copy operations
		FString CopyConstructDecl = FString::Printf(TEXT("void f(const %s& Other)"), *TypeName);
		Binds.Constructor(CopyConstructDecl, &FAngelscriptUStructBinds::GenericCopyConstruct, Struct)
			.NativeConstructor(NativeTypeName, true);

		FString AssignDecl = FString::Printf(TEXT("%s& opAssign(const %s& Other)"), *TypeName, *TypeName);
		Binds.Method(AssignDecl, &FAngelscriptUStructBinds::GenericAssign, Struct).NativeAssignment(NativeTypeName, true);
	}
}

static void DeclareStructType(
	FAngelscriptBinds& Binds,
	const FString& TypeName,
	UScriptStruct* Struct,
	FBindFlags BindFlags)
{
	auto StructBinds = Binds.ValueClassForTarget(TypeName, Struct, BindFlags);
	asITypeInfo* ScriptTypeInfo = StructBinds.GetTypeInfo();
	if (ScriptTypeInfo != nullptr)
		ScriptTypeInfo->SetUserData(Struct);
}

static void RegisterStructType(
	FAngelscriptBinds& Binds,
	const FString& TypeName,
	UScriptStruct* Struct)
{
	auto StructBinds = Binds.ExistingClassForTarget(TypeName);
	asITypeInfo* ScriptTypeInfo = StructBinds.GetTypeInfo();
	if (ScriptTypeInfo == nullptr)
		return;

	auto Type = MakeShared<FUStructType>(Struct, TypeName, Binds.GetTargetBindDatabase());
	Type->ScriptTypeInfo = ScriptTypeInfo;
	Binds.RegisterTypeForTarget(Type);
}

struct FUStructPropertyTypeFinder
{
	FAngelscriptTypeDatabase* TargetTypeDatabase = nullptr;

	bool operator()(FProperty* Property, FAngelscriptTypeUsage& Usage) const
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty == nullptr)
			return false;

		const TSharedRef<FAngelscriptType>* RegisteredType =
			TargetTypeDatabase->TypesByData.Find(StructProperty->Struct);
		if (RegisteredType != nullptr)
		{
			Usage.Type = RegisteredType->ToSharedPtr();
			return true;
		}

		auto* ScriptStruct = Cast<UASStruct>(StructProperty->Struct);
		if (ScriptStruct != nullptr && ScriptStruct->ScriptType != nullptr)
		{
			Usage.Type = TargetTypeDatabase->ScriptStructType;
			Usage.ScriptClass = ScriptStruct->ScriptType;
			return true;
		}

		return false;
	}
};

#if WITH_EDITOR && !AS_USE_BIND_DB
static void AddPropertyDocumentationForTarget(
	FAngelscriptEngine& Engine,
	int32 TypeId,
	int32 PropertyOffset,
	FStringView Documentation)
{
	FAngelscriptDocumentationState* DocumentationState = Engine.GetDocumentationState();
	check(DocumentationState != nullptr);
	DocumentationState->UnrealPropertyDocumentation.Add(
		TPair<int32, int32>(TypeId, PropertyOffset),
		FString(Documentation));
}
#endif

static void BindStructTypeLookups(FAngelscriptBinds& Binds)
{
	// Script structs should be generically typed
	Binds.GetTargetTypeDatabase().ScriptStructType = MakeShared<FUStructType>(
		nullptr,
		TEXT(""),
		Binds.GetTargetBindDatabase());

	// Register a type finder into the type system that
	// can look up a StructProperty's inner angelscript type.
	Binds.RegisterTypeFinderForTarget(FUStructPropertyTypeFinder{&Binds.GetTargetTypeDatabase()});
}

#if AS_USE_BIND_DB
static void BindStructDeclarations(FAngelscriptBinds& Binds)
{
	for (FAngelscriptStructBind& DBBind : Binds.GetTargetBindDatabase().Structs)
	{
		UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *DBBind.UnrealPath);
		if (Struct == nullptr)
			continue;

		DBBind.ResolvedStruct = Struct;

		FBindFlags BindFlags;
		if (Struct->StructFlags & STRUCT_IsPlainOldData)
			BindFlags.ExtraFlags |= asOBJ_POD;

		DeclareStructType(Binds, DBBind.TypeName, Struct, BindFlags);
	}
}

static void BindStructTypeInfrastructure(FAngelscriptBinds& Binds)
{
	for (FAngelscriptStructBind& DBBind : Binds.GetTargetBindDatabase().Structs)
	{
		if (DBBind.ResolvedStruct != nullptr)
			RegisterStructType(Binds, DBBind.TypeName, DBBind.ResolvedStruct);
	}

	BindStructTypeLookups(Binds);
}

static void BindStructReflection(FAngelscriptBinds& TargetBinds)
{
	FAngelscriptTypeDatabase& TargetTypeDatabase = TargetBinds.GetTargetTypeDatabase();
	for (FAngelscriptStructBind& DBBind : TargetBinds.GetTargetBindDatabase().Structs)
	{
		UScriptStruct* Struct = DBBind.ResolvedStruct;
		if (Struct == nullptr)
			continue;

		const TSharedRef<FAngelscriptType>* Type = TargetTypeDatabase.TypesByData.Find(Struct);
		if (Type == nullptr)
			continue;

		auto Binds = TargetBinds.ExistingClassForTarget(DBBind.TypeName);
		BindStructBehaviors(Binds, DBBind.TypeName, Struct);

		for (auto& DBProp : DBBind.Properties)
		{
			FProperty* Property = Struct->FindPropertyByName(*DBProp.UnrealPath);
			if (Property == nullptr)
				continue;

			if (DBProp.Declaration.Len() != 0)
			{
				Binds.Property(DBProp.Declaration, (SIZE_T)Property->GetOffset_ForUFunction());
			}
			else
			{
				FAngelscriptTypeUsage Usage =
					FAngelscriptTypeUsage::FromProperty(TargetTypeDatabase, Property);
				if (!Usage.IsValid())
					continue;

				FAngelscriptType::FBindParams Params;
				Params.BindClass = &Binds;
				Params.NameOverride = DBProp.UnrealPath;
				Params.bCanRead = DBProp.bCanRead;
				Params.bCanWrite = DBProp.bCanWrite;
				Usage.Type->BindProperty(Usage, Params, Property);
			}
		}
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_TypeDeclarations(
	TEXT("UStruct.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindStructDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_TypeInfrastructure(
	TEXT("UStruct.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindStructTypeInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_ReflectionBindings(
	TEXT("UStruct.ReflectionBindings"),
	EAngelscriptBindPhase::ReflectionBindings,
	&BindStructReflection);
#else // if !AS_USE_BIND_DB

static const FName NAME_Meta_ForceAngelscriptBind("ForceAngelscriptBind");

static const FName NAME_STRUCT_BlueprintType("BlueprintType");
static const FName NAME_STRUCT_NotBlueprintType("NotBlueprintType");
static const FName NAME_STRUCT_NoAutoAngelscriptBind("NoAutoAngelscriptBind");
static const FName NAME_STRUCT_NotInAngelscript("NotInAngelscript");

struct FGetBoxSphereBounds3f
{
	static UScriptStruct* Get();
};

struct FGetBox3f
{
	static UScriptStruct* Get();
};

struct FGetSphere
{
	static UScriptStruct* Get();
};

struct FGetSphere3f
{
	static UScriptStruct* Get();
};

struct FGetIntVector2
{
	static UScriptStruct* Get();
};

static bool ShouldBindEngineType(UScriptStruct* Struct)
{
	if (Struct == nullptr)
		return false;

	const FString StructCppName = Struct->GetStructCPPName();
	if (StructCppName == TEXT("FBox") || StructCppName == TEXT("FBoxSphereBounds"))
		return false;

	if (Struct == TBaseStructure<FVector>::Get())
		return false;
	if (Struct == TVariantStructure<FVector3f>::Get())
		return false;
	if (Struct == TBaseStructure<FQuat>::Get())
		return false;
	if (Struct == TVariantStructure<FQuat4f>::Get())
		return false;
	if (Struct == TBaseStructure<FTransform>::Get())
		return false;
	if (Struct == TVariantStructure<FTransform3f>::Get())
		return false;
	if (Struct == TBaseStructure<FRotator>::Get())
		return false;
	if (Struct == TVariantStructure<FRotator3f>::Get())
		return false;
	if (Struct == FGetBox::Get())
		return false;
	if (Struct == FGetBox3f::Get())
		return false;
	if (Struct == TBaseStructure<FLinearColor>::Get())
		return false;
	if (Struct == TBaseStructure<FVector2D>::Get())
		return false;
	if (Struct == TVariantStructure<FVector2f>::Get())
		return false;
	if (Struct == TBaseStructure<FVector4>::Get())
		return false;
	if (Struct == TVariantStructure<FVector4f>::Get())
		return false;
	if (Struct == TBaseStructure<FIntPoint>::Get())
		return false;
	if (Struct == TBaseStructure<FIntVector>::Get())
		return false;
	if (Struct == TBaseStructure<FIntVector4>::Get())
		return false;
	if (Struct == FGetIntVector2::Get())
		return false;
	if (Struct == TBaseStructure<FRandomStream>::Get())
		return false;
	if (Struct == FGetSphere::Get())
		return false;
	if (Struct == FGetSphere3f::Get())
		return false;
	if (Struct == FGetBoxSphereBounds::Get())
		return false;
	if (Struct == FGetBoxSphereBounds3f::Get())
		return false;

	if ((Struct->StructFlags & STRUCT_NoExport))
	{
	}
	else
	{
		// Only bind native structs, not checking for NoExport because those are always from C++ (but might not have the native flag)
		if (!(Struct->StructFlags & STRUCT_Native))
			return false;
	}
	
	// Force binds always gets bound
	if (Struct->HasMetaData(NAME_Meta_ForceAngelscriptBind))
		return true;

	// Allowing opting out of automatic bind
	if (Struct->HasMetaData(NAME_STRUCT_NoAutoAngelscriptBind))
		return false;
	if (Struct->HasMetaData(NAME_STRUCT_NotInAngelscript))
		return false;

	// BlueprintType always gets bound
	if (Struct->GetBoolMetaData(NAME_STRUCT_BlueprintType))
		return true;
	if (Struct->GetBoolMetaData(NAME_STRUCT_NotBlueprintType))
		return false;

	// If the class has any BlueprintVisible properties, also bind it
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
			return true;
		if (Property->HasAnyPropertyFlags(CPF_Edit))
			return true;
		if (Property->HasAnyPropertyFlags(CPF_BlueprintAssignable))
			return true;
	}

	return false;
}

static void ForceBindStruct(const TCHAR* Path)
{
	if (auto* Struct = FindObject<UStruct>(nullptr, Path))
		Struct->SetMetaData(NAME_Meta_ForceAngelscriptBind, TEXT(""));
}

static void HardCodeCallingMetaForUnrealStructs()
{
	ForceBindStruct(TEXT("/Script/Engine.OverlapResult"));
}

static const TArray<TObjectPtr<UScriptStruct>>& GetOrCaptureUStructTypes(FAngelscriptBinds& Binds)
{
	FAngelscriptBindState& BindState = Binds.GetTargetBindState();
	if (!BindState.bUStructTypeSnapshotCaptured)
	{
		HardCodeCallingMetaForUnrealStructs();

		for (UScriptStruct* Struct : TObjectRange<UScriptStruct>())
		{
			if (ShouldBindEngineType(Struct))
				BindState.UStructTypeSnapshot.Add(Struct);
		}
		BindState.bUStructTypeSnapshotCaptured = true;
	}

	return BindState.UStructTypeSnapshot;
}

static void BindStructDeclarations(FAngelscriptBinds& Binds)
{
	for (const TObjectPtr<UScriptStruct>& StructPtr : GetOrCaptureUStructTypes(Binds))
	{
		UScriptStruct* Struct = StructPtr.Get();
		const FString TypeName = Struct->GetStructCPPName();

		// Bind into angelscript engine
		FBindFlags BindFlags;
		if (Struct->StructFlags & STRUCT_IsPlainOldData)
			BindFlags.ExtraFlags |= asOBJ_POD;
		DeclareStructType(Binds, TypeName, Struct, BindFlags);
	}
}

static void BindStructTypeInfrastructure(FAngelscriptBinds& Binds)
{
	for (const TObjectPtr<UScriptStruct>& StructPtr : GetOrCaptureUStructTypes(Binds))
	{
		UScriptStruct* Struct = StructPtr.Get();
		RegisterStructType(Binds, Struct->GetStructCPPName(), Struct);
	}

	BindStructTypeLookups(Binds);
}

static const FName NAME_Property_Struct_ScriptName("ScriptName");
static const FName NAME_Property_Struct_DeprecatedProperty("DeprecatedProperty");
static const FName NAME_Property_Struct_DeprecationMessage("DeprecationMessage");
static void BindStructReflection(FAngelscriptBinds& TargetBinds)
{
	FAngelscriptTypeDatabase& TargetTypeDatabase = TargetBinds.GetTargetTypeDatabase();
	FAngelscriptBindDatabase& TargetBindDatabase = TargetBinds.GetTargetBindDatabase();
	FAngelscriptEngine& TargetEngine = TargetBinds.GetTargetEngine();

	for (const TObjectPtr<UScriptStruct>& StructPtr : GetOrCaptureUStructTypes(TargetBinds))
	{
		UScriptStruct* Struct = StructPtr.Get();

		const TSharedRef<FAngelscriptType>* Type = TargetTypeDatabase.TypesByData.Find(Struct);
		if (Type == nullptr)
			continue;

		FString TypeName = (*Type)->GetAngelscriptTypeName();
		auto Binds = TargetBinds.ExistingClassForTarget(TypeName);

		BindStructBehaviors(Binds, TypeName, Struct);

		auto* ScriptType = Binds.GetTypeInfo();
		if (ScriptType == nullptr)
			continue;

#if WITH_EDITOR
		const FString& Tooltip = Struct->GetMetaData(NAME_Struct_Tooltip);
		if (Tooltip.Len() != 0)
			FAngelscriptDocs::AddUnrealDocumentationForType(TargetEngine, ScriptType->GetTypeId(), Tooltip);
#endif

		FAngelscriptStructBind DBBind;
		DBBind.TypeName = TypeName;
		DBBind.UnrealPath = Struct->GetPathName();

		// Bind actual properties
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Property = *It;

			FAngelscriptType::FBindParams Params = GetPropertyBindParams(Property);
			Params.BindClass = &Binds;

			if (!Params.bCanRead && !Params.bCanWrite && !Params.bCanEdit)
				continue;

			// Don't bind editor-only stuff in simulate cooked mode
			if (!TargetEngine.ShouldUseEditorScripts() && Property->HasAnyPropertyFlags(CPF_EditorOnly))
				continue;

			// Bind using angelscript type system otherwise
			FAngelscriptTypeUsage Usage =
				FAngelscriptTypeUsage::FromProperty(TargetTypeDatabase, Property);
			if (!Usage.IsValid())
				continue;

			// Don't bind properties that have a Get or Set accessor bound already
			FString PropertyName = Property->GetName();

#if WITH_EDITOR
			const FString& ScriptName = Property->GetMetaData(NAME_Property_Struct_ScriptName);
			if (ScriptName.Len() != 0)
				PropertyName = ScriptName;
#endif

			if (Usage.Type->BindProperty(Usage, Params, Property))
			{
				// Need to replicate the BindProperty in the database
				FAngelscriptPropertyBind DBProp;
				DBProp.UnrealPath = Property->GetName();
				DBProp.bCanWrite = Params.bCanWrite;
				DBProp.bCanRead = Params.bCanRead;
				DBProp.bCanEdit = Params.bCanEdit;

				if (!Property->HasAnyPropertyFlags(CPF_EditorOnly))
					DBBind.Properties.Add(DBProp);
				continue;
			}

#if WITH_EDITOR
			bool bIsDeprecated = Property->HasMetaData(NAME_Property_Struct_DeprecatedProperty);
			FString DeprecationMessage;
			if (bIsDeprecated)
				DeprecationMessage = Property->GetMetaData(NAME_Property_Struct_DeprecationMessage);

			const FString& PropertyTooltip = Property->GetMetaData(NAME_Struct_Tooltip);
			if (PropertyTooltip.Len() != 0)
			{
				AddPropertyDocumentationForTarget(
					TargetEngine,
					ScriptType->GetTypeId(),
					Property->GetOffset_ForUFunction(),
					PropertyTooltip);
			}

			bool bIsEditorOnly = false;
			if (Property->HasAnyPropertyFlags(CPF_EditorOnly))
				bIsEditorOnly = true;
#endif

			FAngelscriptPropertyBind DBProp;
			DBProp.UnrealPath = Property->GetName();

			FString PropertyType = Usage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable);
			FString Declaration = FString::Printf(TEXT("%s %s"), *PropertyType, *PropertyName);
			Binds.Property(Declaration, Property->GetOffset_ForUFunction(), Params);

			// Simple declarations can be stored in the database by declaration
			DBProp.Declaration = Declaration;

			if (!Property->HasAnyPropertyFlags(CPF_EditorOnly))
				DBBind.Properties.Add(DBProp);

#if WITH_EDITOR
			if (bIsDeprecated || bIsEditorOnly)
			{
				auto* ObjectType = (asCObjectType*)Binds.GetTypeInfo();
				if (ObjectType != nullptr)
				{
					asCObjectProperty* ScriptProperty = ObjectType->GetFirstProperty(TCHAR_TO_ANSI(*PropertyName));
					if (ScriptProperty != nullptr)
					{
						if (bIsDeprecated)
						{
							ScriptProperty->isDeprecated = true;
							ScriptProperty->DeprecationMessage = TCHAR_TO_ANSI(*DeprecationMessage);
						}

						if (bIsEditorOnly)
						{
							ScriptProperty->isEditorOnly = true;
						}
					}
					else
					{
						ensure(false);
					}
				}
			}
#endif
		}

		// TODO: We need some way of determining whether this struct
		// even exists in cooked, but I can't come up with one right now,
		// so we'll just rely on ignoring it in cooked.
		TargetBindDatabase.Structs.Add(DBBind);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_TypeDeclarations(
	TEXT("UStruct.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindStructDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_TypeInfrastructure(
	TEXT("UStruct.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindStructTypeInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_ReflectionBindings(
	TEXT("UStruct.ReflectionBindings"),
	EAngelscriptBindPhase::ReflectionBindings,
	&BindStructReflection);
#endif // AS_USE_BIND_DB
