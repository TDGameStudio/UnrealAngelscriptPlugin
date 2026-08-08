#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Helper_CppType.h"

#include "CollisionQueryParams.h"

#include "Bind_FCollisionQueryParams_Functions.h"

struct FCollisionQueryParamsType : TAngelscriptCppType<FCollisionQueryParams>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FCollisionQueryParams");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

struct FCollisionEnabledMaskType : TAngelscriptCppType<FCollisionEnabledMask>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FCollisionEnabledMask");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

struct FComponentQueryParamsType : TAngelscriptCppType<FComponentQueryParams>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FComponentQueryParams");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

struct FCollisionResponseParamsType : TAngelscriptCppType<FCollisionResponseParams>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FCollisionResponseParams");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

struct FCollisionObjectQueryParamsType : TAngelscriptCppType<FCollisionObjectQueryParams>
{
	FString GetAngelscriptTypeName() const override
	{
		return TEXT("FCollisionObjectQueryParams");
	}

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindCollisionQueryParamsTypeDeclarations(FAngelscriptBinds& Binds)
	{
		auto QueryMobilityType = Binds.EnumForTarget("EQueryMobilityType");
		QueryMobilityType["Any"] = EQueryMobilityType::Any;
		QueryMobilityType["Static"] = EQueryMobilityType::Static;
		QueryMobilityType["Dynamic"] = EQueryMobilityType::Dynamic;

		auto CollisionObjectQueryInitType = Binds.EnumForTarget("ECollisionObjectQueryInitType");
		CollisionObjectQueryInitType["AllObjects"] = FCollisionObjectQueryParams::InitType::AllObjects;
		CollisionObjectQueryInitType["AllStaticObjects"] = FCollisionObjectQueryParams::InitType::AllStaticObjects;
		CollisionObjectQueryInitType["AllDynamicObjects"] = FCollisionObjectQueryParams::InitType::AllDynamicObjects;

		FBindFlags QueryParamsFlags;
		Binds.ValueClassForTarget<FCollisionQueryParams>("FCollisionQueryParams", QueryParamsFlags);

		FBindFlags CollisionEnabledMaskFlags;
		CollisionEnabledMaskFlags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionEnabledMask>("FCollisionEnabledMask", CollisionEnabledMaskFlags);

		FBindFlags ComponentQueryParamsFlags;
		Binds.ValueClassForTarget<FComponentQueryParams>("FComponentQueryParams", ComponentQueryParamsFlags);

		FBindFlags CollisionResponseParamsFlags;
		CollisionResponseParamsFlags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionResponseParams>("FCollisionResponseParams", CollisionResponseParamsFlags);

		FBindFlags CollisionObjectQueryParamsFlags;
		CollisionObjectQueryParamsFlags.bPOD = true;
		Binds.ValueClassForTarget<FCollisionObjectQueryParams>("FCollisionObjectQueryParams", CollisionObjectQueryParamsFlags);
	}

	void BindCollisionQueryParamsTypeInfrastructure(FAngelscriptBinds& Binds)
	{
		Binds.RegisterTypeForTarget(MakeShared<FCollisionQueryParamsType>());
		Binds.RegisterTypeForTarget(MakeShared<FCollisionEnabledMaskType>());
		Binds.RegisterTypeForTarget(MakeShared<FComponentQueryParamsType>());
		Binds.RegisterTypeForTarget(MakeShared<FCollisionResponseParamsType>());
		Binds.RegisterTypeForTarget(MakeShared<FCollisionObjectQueryParamsType>());
	}

	void BindFCollisionQueryParamsEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionQueryParams = Binds.ExistingClassForTarget("FCollisionQueryParams");
		CollisionQueryParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParams)
			.NoDiscard()
			.NativeConstructor("FCollisionQueryParams", true);
		CollisionQueryParams.Constructor("void f(const FCollisionQueryParams& Other)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParamsCopy)
			.NoDiscard()
			.NativeConstructor("FCollisionQueryParams", true);
		CollisionQueryParams.Method("FCollisionQueryParams& opAssign(const FCollisionQueryParams& Other)", METHODPR_TRIVIAL(FCollisionQueryParams&, FCollisionQueryParams, operator=, (const FCollisionQueryParams&)));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionQueryParams");
		Binds.BindGlobalVariableForTarget("const FCollisionQueryParams DefaultQueryParam", &FCollisionQueryParams::DefaultQueryParam);
	}

	void BindFCollisionEnabledMaskEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionEnabledMask = Binds.ExistingClassForTarget("FCollisionEnabledMask");
		CollisionEnabledMask.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionEnabledMask)
			.NoDiscard()
			.NativeConstructor("FCollisionEnabledMask", true);
		CollisionEnabledMask.Constructor("void f(ECollisionEnabled CollisionEnabled)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionEnabledMaskFromCollisionEnabled)
			.NoDiscard()
			.NativeConstructor("FCollisionEnabledMask", true);
		CollisionEnabledMask.Property("int8 Bits", &FCollisionEnabledMask::Bits);
	}

	void BindFComponentQueryParamsEarly(FAngelscriptBinds& Binds)
	{
		auto ComponentQueryParams = Binds.ExistingClassForTarget("FComponentQueryParams");
		ComponentQueryParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParams)
			.NoDiscard()
			.NativeConstructor("FComponentQueryParams", true);
		ComponentQueryParams.Constructor("void f(const FComponentQueryParams& Other)", &FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParamsCopy)
			.NoDiscard()
			.NativeConstructor("FComponentQueryParams", true);
		ComponentQueryParams.Method("FComponentQueryParams& opAssign(const FComponentQueryParams& Other)", METHODPR_TRIVIAL(FComponentQueryParams&, FComponentQueryParams, operator=, (const FComponentQueryParams&)));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FComponentQueryParams");
		Binds.BindGlobalVariableForTarget("const FComponentQueryParams DefaultComponentQueryParams", &FComponentQueryParams::DefaultComponentQueryParams);
	}

	void BindFCollisionResponseParamsEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionResponseParams = Binds.ExistingClassForTarget("FCollisionResponseParams");
		CollisionResponseParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParams)
			.NativeConstructor("FCollisionResponseParams", true);
		CollisionResponseParams.Constructor("void f(ECollisionResponse DefaultResponse)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParamsFromDefaultResponse)
			.NativeConstructor("FCollisionResponseParams", true);

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionResponseParams");
		Binds.BindGlobalVariableForTarget("const FCollisionResponseParams DefaultResponseParam", &FCollisionResponseParams::DefaultResponseParam);
	}

	void BindFCollisionObjectQueryParamsEarly(FAngelscriptBinds& Binds)
	{
		auto CollisionObjectQueryParams = Binds.ExistingClassForTarget("FCollisionObjectQueryParams");
		CollisionObjectQueryParams.Constructor("void f()", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParams)
			.NativeConstructor("FCollisionObjectQueryParams", true);
		CollisionObjectQueryParams.Constructor("void f(ECollisionChannel QueryChannel)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromChannel)
			.NativeConstructor("FCollisionObjectQueryParams", true);

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionObjectQueryParams");
		Binds.BindGlobalVariableForTarget("const FCollisionObjectQueryParams DefaultObjectQueryParam", &FCollisionObjectQueryParams::DefaultObjectQueryParam);
	}

	void BindFCollisionQueryParamsLate(FAngelscriptBinds& Binds)
	{
		auto CollisionQueryParams = Binds.ExistingClassForTarget("FCollisionQueryParams");
		CollisionQueryParams.Constructor("void f(FName InTraceTag, bool bInTraceComplex, const AActor InIgnoreActor)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionQueryParamsFromTraceTag)
			.NoDiscard()
			.NativeConstructor("FCollisionQueryParams", true);
		CollisionQueryParams.Property("FName TraceTag", &FCollisionQueryParams::TraceTag);
		CollisionQueryParams.Property("FName OwnerTag", &FCollisionQueryParams::OwnerTag);
		CollisionQueryParams.Property("bool bTraceComplex", &FCollisionQueryParams::bTraceComplex);
		CollisionQueryParams.Property("bool bFindInitialOverlaps", &FCollisionQueryParams::bFindInitialOverlaps);
		CollisionQueryParams.Property("bool bReturnFaceIndex", &FCollisionQueryParams::bReturnFaceIndex);
		CollisionQueryParams.Property("bool bReturnPhysicalMaterial", &FCollisionQueryParams::bReturnPhysicalMaterial);
		CollisionQueryParams.Property("bool bIgnoreBlocks", &FCollisionQueryParams::bIgnoreBlocks);
		CollisionQueryParams.Property("bool bIgnoreTouches", &FCollisionQueryParams::bIgnoreTouches);
		CollisionQueryParams.Property("bool bSkipNarrowPhase", &FCollisionQueryParams::bSkipNarrowPhase);
		CollisionQueryParams.Property("EQueryMobilityType MobilityType", &FCollisionQueryParams::MobilityType);
		CollisionQueryParams.Property("uint8 IgnoreMask", &FCollisionQueryParams::IgnoreMask);
		CollisionQueryParams.Method("TArray<uint32> GetIgnoredComponents() const", &FAngelscriptFCollisionQueryParamsBinds::GetCollisionQueryParamsIgnoredComponents);
		CollisionQueryParams.Method("TArray<uint32> GetIgnoredActors() const", &FAngelscriptFCollisionQueryParamsBinds::GetCollisionQueryParamsIgnoredActors);
		CollisionQueryParams.Method("void ClearIgnoredComponents()", METHOD_TRIVIAL(FCollisionQueryParams, ClearIgnoredComponents));
		CollisionQueryParams.Method("void ClearIgnoredActors()", METHOD_TRIVIAL(FCollisionQueryParams, ClearIgnoredSourceObjects));
		CollisionQueryParams.Method("void SetNumIgnoredComponents(int32 NewNum)", METHOD_TRIVIAL(FCollisionQueryParams, SetNumIgnoredComponents));
		CollisionQueryParams.Method("void AddIgnoredActor(const AActor InIgnoreActor)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const AActor*)));
		CollisionQueryParams.Method("void AddIgnoredActor(const uint32 InIgnoreActorID)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const uint32)));
		CollisionQueryParams.Method("void AddIgnoredActors(const TArray<AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<AActor*>&)));
		CollisionQueryParams.Method("void AddIgnoredActors(const TArray<const AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<const AActor*>&)));
		CollisionQueryParams.Method("void AddIgnoredComponent(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent, (const UPrimitiveComponent*)));
		CollisionQueryParams.Method("void AddIgnoredComponents(const TArray<UPrimitiveComponent>& InIgnoreComponents)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponents, (const TArray<UPrimitiveComponent*>&)));
		CollisionQueryParams.Method("void AddIgnoredComponent_LikelyDuplicatedRoot(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent_LikelyDuplicatedRoot, (const UPrimitiveComponent*)));
		CollisionQueryParams.Method("FString ToString() const", METHOD_TRIVIAL(FCollisionQueryParams, ToString));
	}

	void BindFComponentQueryParamsLate(FAngelscriptBinds& Binds)
	{
		auto ComponentQueryParams = Binds.ExistingClassForTarget("FComponentQueryParams");
		ComponentQueryParams.Constructor("void f(FName InTraceTag, const AActor InIgnoreActor, FCollisionEnabledMask CollisionEnabledMask)", &FAngelscriptFCollisionQueryParamsBinds::ConstructComponentQueryParamsFromTraceTag)
			.NoDiscard()
			.NativeConstructor("FComponentQueryParams", true);
		ComponentQueryParams.Property("FName TraceTag", &FComponentQueryParams::TraceTag);
		ComponentQueryParams.Property("FName OwnerTag", &FComponentQueryParams::OwnerTag);
		ComponentQueryParams.Property("bool bTraceComplex", &FComponentQueryParams::bTraceComplex);
		ComponentQueryParams.Property("bool bFindInitialOverlaps", &FComponentQueryParams::bFindInitialOverlaps);
		ComponentQueryParams.Property("bool bReturnFaceIndex", &FComponentQueryParams::bReturnFaceIndex);
		ComponentQueryParams.Property("bool bReturnPhysicalMaterial", &FComponentQueryParams::bReturnPhysicalMaterial);
		ComponentQueryParams.Property("bool bIgnoreBlocks", &FComponentQueryParams::bIgnoreBlocks);
		ComponentQueryParams.Property("bool bIgnoreTouches", &FComponentQueryParams::bIgnoreTouches);
		ComponentQueryParams.Property("bool bSkipNarrowPhase", &FComponentQueryParams::bSkipNarrowPhase);
		ComponentQueryParams.Property("EQueryMobilityType MobilityType", &FComponentQueryParams::MobilityType);
		ComponentQueryParams.Property("uint8 IgnoreMask", &FComponentQueryParams::IgnoreMask);
		ComponentQueryParams.Property("FCollisionEnabledMask ShapeCollisionMask", &FComponentQueryParams::ShapeCollisionMask);
		ComponentQueryParams.Method("TArray<uint32> GetIgnoredComponents() const", &FAngelscriptFCollisionQueryParamsBinds::GetComponentQueryParamsIgnoredComponents);
		ComponentQueryParams.Method("TArray<uint32> GetIgnoredActors() const", &FAngelscriptFCollisionQueryParamsBinds::GetComponentQueryParamsIgnoredActors);
		ComponentQueryParams.Method("void ClearIgnoredComponents()", METHOD_TRIVIAL(FComponentQueryParams, ClearIgnoredComponents));
		ComponentQueryParams.Method("void ClearIgnoredActors()", METHOD_TRIVIAL(FComponentQueryParams, ClearIgnoredSourceObjects));
		ComponentQueryParams.Method("void SetNumIgnoredComponents(int32 NewNum)", METHOD_TRIVIAL(FComponentQueryParams, SetNumIgnoredComponents));
		ComponentQueryParams.Method("void AddIgnoredActor(const AActor InIgnoreActor)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const AActor*)));
		ComponentQueryParams.Method("void AddIgnoredActor(const uint32 InIgnoreActorID)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const uint32)));
		ComponentQueryParams.Method("void AddIgnoredActors(const TArray<AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<AActor*>&)));
		ComponentQueryParams.Method("void AddIgnoredActors(const TArray<const AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<const AActor*>&)));
		ComponentQueryParams.Method("void AddIgnoredComponent(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent, (const UPrimitiveComponent*)));
		ComponentQueryParams.Method("void AddIgnoredComponents(const TArray<UPrimitiveComponent>& InIgnoreComponents)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponents, (const TArray<UPrimitiveComponent*>&)));
		ComponentQueryParams.Method("void AddIgnoredComponent_LikelyDuplicatedRoot(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent_LikelyDuplicatedRoot, (const UPrimitiveComponent*)));
		ComponentQueryParams.Method("FString ToString() const", METHOD_TRIVIAL(FCollisionQueryParams, ToString));
	}

	void BindFCollisionObjectQueryParamsLate(FAngelscriptBinds& Binds)
	{
		auto CollisionObjectQueryParams = Binds.ExistingClassForTarget("FCollisionObjectQueryParams");
		CollisionObjectQueryParams.Constructor("void f(ECollisionObjectQueryInitType QueryType)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromInitType)
			.NativeConstructor("FCollisionObjectQueryParams", true);
		CollisionObjectQueryParams.Constructor("void f(int32 InObjectTypesToQuery)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionObjectQueryParamsFromObjectTypes)
			.NativeConstructor("FCollisionObjectQueryParams", true);
		CollisionObjectQueryParams.Property("int32 ObjectTypesToQuery", &FCollisionObjectQueryParams::ObjectTypesToQuery);
		CollisionObjectQueryParams.Property("uint8 IgnoreMask", &FCollisionObjectQueryParams::IgnoreMask);
		CollisionObjectQueryParams.Method("void AddObjectTypesToQuery(ECollisionChannel QueryChannel)", METHOD_TRIVIAL(FCollisionObjectQueryParams, AddObjectTypesToQuery));
		CollisionObjectQueryParams.Method("void RemoveObjectTypesToQuery(ECollisionChannel QueryChannel)", METHOD_TRIVIAL(FCollisionObjectQueryParams, RemoveObjectTypesToQuery));
		CollisionObjectQueryParams.Method("int64 GetObjectTypesToQuery() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, GetObjectTypesToQuery));
		CollisionObjectQueryParams.Method("void SetObjectTypesToQuery(int64 InObjectTypesToQuery)", METHOD_TRIVIAL(FCollisionObjectQueryParams, SetObjectTypesToQuery));
		CollisionObjectQueryParams.Method("int64 GetQueryBitfield64() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, GetQueryBitfield64));
		CollisionObjectQueryParams.Method("bool IsValid() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, IsValid));
		CollisionObjectQueryParams.Method("void DoVerify() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, DoVerify));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionObjectQueryParams");
		Binds.BindGlobalFunctionForTarget("bool IsValidObjectQuery(ECollisionChannel QueryChannel) no_discard", FUNC_TRIVIAL(FCollisionObjectQueryParams::IsValidObjectQuery));
		Binds.BindGlobalFunctionForTarget("ECollisionObjectQueryInitType GetCollisionChannelFromOverlapFilter(EOverlapFilterOption Filter) no_discard", FUNC_TRIVIAL(FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter));
	}

	void BindFCollisionResponseContainer(FAngelscriptBinds& Binds)
	{
		auto CollisionResponseContainer = Binds.ExistingClassForTarget("FCollisionResponseContainer");
		CollisionResponseContainer.Constructor("void f(ECollisionResponse DefaultResponse)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseContainer)
			.NativeConstructor("FCollisionResponseContainer", true);
		CollisionResponseContainer.Method("bool SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, SetResponse));
		CollisionResponseContainer.Method("bool SetAllChannels(ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, SetAllChannels));
		CollisionResponseContainer.Method("bool ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, ReplaceChannels));
		CollisionResponseContainer.Method("ECollisionResponse GetResponse(ECollisionChannel Channel) const", METHOD_TRIVIAL(FCollisionResponseContainer, GetResponse));
		CollisionResponseContainer.Method("bool opEquals(const FCollisionResponseContainer& Other) const", METHODPR_TRIVIAL(bool, FCollisionResponseContainer, operator==, (const FCollisionResponseContainer&) const));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FCollisionResponseContainer");
		Binds.BindGlobalFunctionForTarget("const FCollisionResponseContainer& GetDefaultResponseContainer()", FUNC_TRIVIAL(FCollisionResponseContainer::GetDefaultResponseContainer));
		Binds.BindGlobalFunctionForTarget("FCollisionResponseContainer CreateMinContainer(const FCollisionResponseContainer& A, const FCollisionResponseContainer& B)", FUNC_TRIVIAL(FCollisionResponseContainer::CreateMinContainer));
	}

	void BindFCollisionResponseParamsLate(FAngelscriptBinds& Binds)
	{
		auto CollisionResponseParams = Binds.ExistingClassForTarget("FCollisionResponseParams");
		CollisionResponseParams.Constructor("void f(const FCollisionResponseContainer& ResponseContainer)", &FAngelscriptFCollisionQueryParamsBinds::ConstructCollisionResponseParamsFromContainer)
			.NativeConstructor("FCollisionResponseParams", true);
	}

	void BindCollisionQueryParamsManualBindings(FAngelscriptBinds& Binds)
	{
		BindFCollisionQueryParamsEarly(Binds);
		BindFCollisionEnabledMaskEarly(Binds);
		BindFComponentQueryParamsEarly(Binds);
		BindFCollisionResponseParamsEarly(Binds);
		BindFCollisionObjectQueryParamsEarly(Binds);

		BindFCollisionQueryParamsLate(Binds);
		BindFComponentQueryParamsLate(Binds);
		BindFCollisionObjectQueryParamsLate(Binds);
		BindFCollisionResponseContainer(Binds);
		BindFCollisionResponseParamsLate(Binds);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionQueryParams_TypeDeclarations(
	TEXT("FCollisionQueryParams.TypeDeclarations"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindCollisionQueryParamsTypeDeclarations);

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionQueryParams_TypeInfrastructure(
	TEXT("FCollisionQueryParams.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindCollisionQueryParamsTypeInfrastructure);

AS_FORCE_LINK const FAngelscriptBind Bind_FCollisionQueryParams_ManualBindings(
	TEXT("FCollisionQueryParams.ManualBindings"),
	EAngelscriptBindPhase::ManualBindings,
	&BindCollisionQueryParamsManualBindings);
