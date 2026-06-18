#include "Misc/DefaultValueHelper.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Helper_CppType.h"

#include "CollisionQueryParams.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_QueryMobilityType((int32)FAngelscriptBinds::EOrder::Early - 1, []
{
	auto TraceType_ = FAngelscriptBinds::Enum("EQueryMobilityType");
	TraceType_["Any"]     = EQueryMobilityType::Any;
	TraceType_["Static"]  = EQueryMobilityType::Static;
	TraceType_["Dynamic"] = EQueryMobilityType::Dynamic;
});

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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionQueryParams_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;

	auto FCollisionQueryParams_ = FAngelscriptBinds::ValueClass<FCollisionQueryParams>("FCollisionQueryParams", Flags);
	FAngelscriptType::Register(MakeShared<FCollisionQueryParamsType>());

	FCollisionQueryParams_.Constructor("void f()", [](FCollisionQueryParams* Address)
	{
		new(Address) FCollisionQueryParams();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionQueryParams_, "FCollisionQueryParams");

	FCollisionQueryParams_.Constructor("void f(const FCollisionQueryParams& Other)", [](FCollisionQueryParams* Address, const FCollisionQueryParams& Other)
	{
		new(Address) FCollisionQueryParams(Other);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionQueryParams_, "FCollisionQueryParams");

	FCollisionQueryParams_.Method("FCollisionQueryParams& opAssign(const FCollisionQueryParams& Other)", METHODPR_TRIVIAL(FCollisionQueryParams&, FCollisionQueryParams, operator=, (const FCollisionQueryParams&)));

	{
		FAngelscriptBinds::FNamespace ns("FCollisionQueryParams");
		FAngelscriptBinds::BindGlobalVariable("const FCollisionQueryParams DefaultQueryParam", &FCollisionQueryParams::DefaultQueryParam);
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionQueryParams_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FCollisionQueryParams_ = FAngelscriptBinds::ExistingClass("FCollisionQueryParams");

	FCollisionQueryParams_.Constructor("void f(FName InTraceTag, bool bInTraceComplex, const AActor InIgnoreActor)", [](FCollisionQueryParams* Address, FName InTraceTag, bool bInTraceComplex, const AActor* InIgnoreActor)
	{
		new(Address) FCollisionQueryParams(InTraceTag, bInTraceComplex, InIgnoreActor);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionQueryParams_, "FCollisionQueryParams");

	FCollisionQueryParams_.Property("FName TraceTag", &FCollisionQueryParams::TraceTag);
	FCollisionQueryParams_.Property("FName OwnerTag", &FCollisionQueryParams::OwnerTag);
	FCollisionQueryParams_.Property("bool bTraceComplex", &FCollisionQueryParams::bTraceComplex);
	FCollisionQueryParams_.Property("bool bFindInitialOverlaps", &FCollisionQueryParams::bFindInitialOverlaps);
	FCollisionQueryParams_.Property("bool bReturnFaceIndex", &FCollisionQueryParams::bReturnFaceIndex);
	FCollisionQueryParams_.Property("bool bReturnPhysicalMaterial", &FCollisionQueryParams::bReturnPhysicalMaterial);
	FCollisionQueryParams_.Property("bool bIgnoreBlocks", &FCollisionQueryParams::bIgnoreBlocks);
	FCollisionQueryParams_.Property("bool bIgnoreTouches", &FCollisionQueryParams::bIgnoreTouches);
	FCollisionQueryParams_.Property("bool bSkipNarrowPhase", &FCollisionQueryParams::bSkipNarrowPhase);
	FCollisionQueryParams_.Property("EQueryMobilityType MobilityType", &FCollisionQueryParams::MobilityType);
	FCollisionQueryParams_.Property("uint8 IgnoreMask", &FCollisionQueryParams::IgnoreMask);

	FCollisionQueryParams_.Method("TArray<uint32> GetIgnoredComponents() const",
		[](FCollisionQueryParams* Address) -> TArray<uint32>
		{
			const FCollisionQueryParams::IgnoreComponentsArrayType& IgnoredComponents = Address->GetIgnoredComponents();;
			TArray<uint32> OutArray;
			OutArray.SetNumUninitialized(IgnoredComponents.Num());
			FMemory::Memcpy(OutArray.GetData(), IgnoredComponents.GetData(), IgnoredComponents.Num() * sizeof(IgnoredComponents[0]));
			return OutArray;
		});

	FCollisionQueryParams_.Method("TArray<uint32> GetIgnoredActors() const",
		[](FCollisionQueryParams* Address) -> TArray<uint32>
		{
			const FCollisionQueryParams::IgnoreActorsArrayType& IgnoredActors = Address->GetIgnoredSourceObjects();
			TArray<uint32> OutArray;
			OutArray.SetNumUninitialized(IgnoredActors.Num());
			FMemory::Memcpy(OutArray.GetData(), IgnoredActors.GetData(), IgnoredActors.Num() * sizeof(IgnoredActors[0]));
			return OutArray;
		});

	FCollisionQueryParams_.Method("void ClearIgnoredComponents()", METHOD_TRIVIAL(FCollisionQueryParams, ClearIgnoredComponents));
	FCollisionQueryParams_.Method("void ClearIgnoredActors()", METHOD_TRIVIAL(FCollisionQueryParams, ClearIgnoredSourceObjects));
	FCollisionQueryParams_.Method("void SetNumIgnoredComponents(int32 NewNum)", METHOD_TRIVIAL(FCollisionQueryParams, SetNumIgnoredComponents));
	FCollisionQueryParams_.Method("void AddIgnoredActor(const AActor InIgnoreActor)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const AActor*)));
	FCollisionQueryParams_.Method("void AddIgnoredActor(const uint32 InIgnoreActorID)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const uint32)));
	FCollisionQueryParams_.Method("void AddIgnoredActors(const TArray<AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<AActor*>&)));
	FCollisionQueryParams_.Method("void AddIgnoredActors(const TArray<const AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<const AActor*>&)));
	FCollisionQueryParams_.Method("void AddIgnoredComponent(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent, (const UPrimitiveComponent*)));
	FCollisionQueryParams_.Method("void AddIgnoredComponents(const TArray<UPrimitiveComponent>& InIgnoreComponents)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponents, (const TArray<UPrimitiveComponent*>&)));
	FCollisionQueryParams_.Method("void AddIgnoredComponent_LikelyDuplicatedRoot(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent_LikelyDuplicatedRoot, (const UPrimitiveComponent*)));
	FCollisionQueryParams_.Method("FString ToString() const", METHOD_TRIVIAL(FCollisionQueryParams, ToString));
});

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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionEnabledMask_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bPOD = true;

	auto FCollisionEnabledMask_ = FAngelscriptBinds::ValueClass<FCollisionEnabledMask>("FCollisionEnabledMask", Flags);
	FAngelscriptType::Register(MakeShared<FCollisionEnabledMaskType>());

	FCollisionEnabledMask_.Constructor("void f()", [](FCollisionEnabledMask* Address)
	{
		new(Address) FCollisionEnabledMask();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionEnabledMask_, "FCollisionEnabledMask");

	FCollisionEnabledMask_.Constructor("void f(ECollisionEnabled CollisionEnabled)", [](FCollisionEnabledMask* Address, ECollisionEnabled::Type CollisionEnabled)
	{
		new(Address) FCollisionEnabledMask(CollisionEnabled);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionEnabledMask_, "FCollisionEnabledMask");

	FCollisionEnabledMask_.Property("int8 Bits", &FCollisionEnabledMask::Bits);
});

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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FComponentQueryParams_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;

	auto FComponentQueryParams_ = FAngelscriptBinds::ValueClass<FComponentQueryParams>("FComponentQueryParams", Flags);
	FAngelscriptType::Register(MakeShared<FComponentQueryParamsType>());

	FComponentQueryParams_.Constructor("void f()", [](FComponentQueryParams* Address)
	{
		new(Address) FComponentQueryParams();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FComponentQueryParams_, "FComponentQueryParams");

	FComponentQueryParams_.Constructor("void f(const FComponentQueryParams& Other)", [](FComponentQueryParams* Address, const FComponentQueryParams& Other)
	{
		new(Address) FComponentQueryParams(Other);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FComponentQueryParams_, "FComponentQueryParams");

	FComponentQueryParams_.Method("FComponentQueryParams& opAssign(const FComponentQueryParams& Other)", METHODPR_TRIVIAL(FComponentQueryParams&, FComponentQueryParams, operator=, (const FComponentQueryParams&)));

	{
		FAngelscriptBinds::FNamespace ns("FComponentQueryParams");
		FAngelscriptBinds::BindGlobalVariable("const FComponentQueryParams DefaultComponentQueryParams", &FComponentQueryParams::DefaultComponentQueryParams);
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FComponentQueryParams_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FComponentQueryParams_ = FAngelscriptBinds::ExistingClass("FComponentQueryParams");

	FComponentQueryParams_.Constructor("void f(FName InTraceTag, const AActor InIgnoreActor, FCollisionEnabledMask CollisionEnabledMask)", [](FComponentQueryParams* Address, FName InTraceTag, const AActor* InIgnoreActor, FCollisionEnabledMask CollisionEnabledMask)
	{
		new(Address) FComponentQueryParams(InTraceTag, FComponentQueryParams::GetUnknownStatId(), InIgnoreActor, CollisionEnabledMask);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FComponentQueryParams_, "FComponentQueryParams");

	FComponentQueryParams_.Property("FName TraceTag", &FComponentQueryParams::TraceTag);
	FComponentQueryParams_.Property("FName OwnerTag", &FComponentQueryParams::OwnerTag);
	FComponentQueryParams_.Property("bool bTraceComplex", &FComponentQueryParams::bTraceComplex);
	FComponentQueryParams_.Property("bool bFindInitialOverlaps", &FComponentQueryParams::bFindInitialOverlaps);
	FComponentQueryParams_.Property("bool bReturnFaceIndex", &FComponentQueryParams::bReturnFaceIndex);
	FComponentQueryParams_.Property("bool bReturnPhysicalMaterial", &FComponentQueryParams::bReturnPhysicalMaterial);
	FComponentQueryParams_.Property("bool bIgnoreBlocks", &FComponentQueryParams::bIgnoreBlocks);
	FComponentQueryParams_.Property("bool bIgnoreTouches", &FComponentQueryParams::bIgnoreTouches);
	FComponentQueryParams_.Property("bool bSkipNarrowPhase", &FComponentQueryParams::bSkipNarrowPhase);
	FComponentQueryParams_.Property("EQueryMobilityType MobilityType", &FComponentQueryParams::MobilityType);
	FComponentQueryParams_.Property("uint8 IgnoreMask", &FComponentQueryParams::IgnoreMask);
	FComponentQueryParams_.Property("FCollisionEnabledMask ShapeCollisionMask", &FComponentQueryParams::ShapeCollisionMask);

	FComponentQueryParams_.Method("TArray<uint32> GetIgnoredComponents() const",
		[](FComponentQueryParams* Address) -> TArray<uint32>
		{
			const FComponentQueryParams::IgnoreComponentsArrayType& IgnoredComponents = Address->GetIgnoredComponents();
			TArray<uint32> OutArray;
			OutArray.SetNumUninitialized(IgnoredComponents.Num());
			FMemory::Memcpy(OutArray.GetData(), IgnoredComponents.GetData(), IgnoredComponents.Num() * sizeof(IgnoredComponents[0]));
			return OutArray;
		});

	FComponentQueryParams_.Method("TArray<uint32> GetIgnoredActors() const",
		[](FComponentQueryParams* Address) -> TArray<uint32>
		{
			const FComponentQueryParams::IgnoreActorsArrayType& IgnoredActors = Address->GetIgnoredSourceObjects();
			TArray<uint32> OutArray;
			OutArray.SetNumUninitialized(IgnoredActors.Num());
			FMemory::Memcpy(OutArray.GetData(), IgnoredActors.GetData(), IgnoredActors.Num() * sizeof(IgnoredActors[0]));
			return OutArray;
		});

	FComponentQueryParams_.Method("void ClearIgnoredComponents()", METHOD_TRIVIAL(FComponentQueryParams, ClearIgnoredComponents));
	FComponentQueryParams_.Method("void ClearIgnoredActors()", METHOD_TRIVIAL(FComponentQueryParams, ClearIgnoredSourceObjects));
	FComponentQueryParams_.Method("void SetNumIgnoredComponents(int32 NewNum)", METHOD_TRIVIAL(FComponentQueryParams, SetNumIgnoredComponents));
	FComponentQueryParams_.Method("void AddIgnoredActor(const AActor InIgnoreActor)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const AActor*)));
	FComponentQueryParams_.Method("void AddIgnoredActor(const uint32 InIgnoreActorID)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActor, (const uint32)));
	FComponentQueryParams_.Method("void AddIgnoredActors(const TArray<AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<AActor*>&)));
	FComponentQueryParams_.Method("void AddIgnoredActors(const TArray<const AActor>& InIgnoreActors)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredActors, (const TArray<const AActor*>&)));
	FComponentQueryParams_.Method("void AddIgnoredComponent(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent, (const UPrimitiveComponent*)));
	FComponentQueryParams_.Method("void AddIgnoredComponents(const TArray<UPrimitiveComponent>& InIgnoreComponents)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponents, (const TArray<UPrimitiveComponent*>&)));
	FComponentQueryParams_.Method("void AddIgnoredComponent_LikelyDuplicatedRoot(const UPrimitiveComponent InIgnoreComponent)", METHODPR_TRIVIAL(void, FCollisionQueryParams, AddIgnoredComponent_LikelyDuplicatedRoot, (const UPrimitiveComponent*)));
	FComponentQueryParams_.Method("FString ToString() const", METHOD_TRIVIAL(FCollisionQueryParams, ToString));
});



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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionResponseParams_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bPOD = true;

	auto FCollisionResponseParams_ = FAngelscriptBinds::ValueClass<FCollisionResponseParams>("FCollisionResponseParams", Flags);
	FAngelscriptType::Register(MakeShared<FCollisionResponseParamsType>());

	FCollisionResponseParams_.Constructor("void f()", [](FCollisionResponseParams* Address)
	{
		new(Address) FCollisionResponseParams();
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionResponseParams_, "FCollisionResponseParams");

	FCollisionResponseParams_.Constructor("void f(ECollisionResponse DefaultResponse)", [](FCollisionResponseParams* Address, ECollisionResponse DefaultResponse)
	{
		new(Address) FCollisionResponseParams(DefaultResponse);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionResponseParams_, "FCollisionResponseParams");

	{
		FAngelscriptBinds::FNamespace ns("FCollisionResponseParams");
		FAngelscriptBinds::BindGlobalVariable("const FCollisionResponseParams DefaultResponseParam", &FCollisionResponseParams::DefaultResponseParam);
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionResponseParams_Late((int32)FAngelscriptBinds::EOrder::Late + 1, []
{
	auto FCollisionResponseParams_ = FAngelscriptBinds::ExistingClass("FCollisionResponseParams");

	FCollisionResponseParams_.Constructor("void f(const FCollisionResponseContainer& ResponseContainer)", [](FCollisionResponseParams* Address, const FCollisionResponseContainer& ResponseContainer)
	{
		new(Address) FCollisionResponseParams(ResponseContainer);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionResponseParams_, "FCollisionResponseParams");
});


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

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionObjectQueryParams_InitType((int32)FAngelscriptBinds::EOrder::Early - 1, []
{
	auto InitType_ = FAngelscriptBinds::Enum("ECollisionObjectQueryInitType");
	InitType_["AllObjects"] = FCollisionObjectQueryParams::InitType::AllObjects;
	InitType_["AllStaticObjects"] = FCollisionObjectQueryParams::InitType::AllStaticObjects;
	InitType_["AllDynamicObjects"] = FCollisionObjectQueryParams::InitType::AllDynamicObjects;
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionObjectQueryParams_Early(FAngelscriptBinds::EOrder::Early, []
{
	FBindFlags Flags;
	Flags.bPOD = true;

	auto FCollisionObjectQueryParams_ = FAngelscriptBinds::ValueClass<FCollisionObjectQueryParams>("FCollisionObjectQueryParams", Flags);
	FAngelscriptType::Register(MakeShared<FCollisionObjectQueryParamsType>());

	FCollisionObjectQueryParams_.Constructor("void f()", [](FCollisionObjectQueryParams* Address)
	{
		new(Address) FCollisionObjectQueryParams();
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionObjectQueryParams_, "FCollisionObjectQueryParams");

	FCollisionObjectQueryParams_.Constructor("void f(ECollisionChannel QueryChannel)", [](FCollisionQueryParams* Address, ECollisionChannel QueryChannel)
	{
		new(Address) FCollisionObjectQueryParams(QueryChannel);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionObjectQueryParams_, "FCollisionObjectQueryParams");

	{
		FAngelscriptBinds::FNamespace ns("FCollisionObjectQueryParams");
		FAngelscriptBinds::BindGlobalVariable("const FCollisionObjectQueryParams DefaultObjectQueryParam", &FCollisionObjectQueryParams::DefaultObjectQueryParam);
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionObjectQueryParams_Late(FAngelscriptBinds::EOrder::Late, []
{
	auto FCollisionObjectQueryParams_ = FAngelscriptBinds::ExistingClass("FCollisionObjectQueryParams");

	FCollisionObjectQueryParams_.Constructor("void f(ECollisionObjectQueryInitType QueryType)", [](FCollisionObjectQueryParams* Address, FCollisionObjectQueryParams::InitType QueryType)
	{
		new(Address) FCollisionObjectQueryParams(QueryType);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionObjectQueryParams_, "FCollisionObjectQueryParams");

	FCollisionObjectQueryParams_.Constructor("void f(int32 InObjectTypesToQuery)", [](FCollisionObjectQueryParams* Address, int32 InObjectTypesToQuery)
	{
		new(Address) FCollisionObjectQueryParams(InObjectTypesToQuery);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionObjectQueryParams_, "FCollisionObjectQueryParams");

	FCollisionObjectQueryParams_.Property("int32 ObjectTypesToQuery", &FCollisionObjectQueryParams::ObjectTypesToQuery);
	FCollisionObjectQueryParams_.Property("uint8 IgnoreMask", &FCollisionObjectQueryParams::IgnoreMask);

	FCollisionObjectQueryParams_.Method("void AddObjectTypesToQuery(ECollisionChannel QueryChannel)", METHOD_TRIVIAL(FCollisionObjectQueryParams, AddObjectTypesToQuery));
	FCollisionObjectQueryParams_.Method("void RemoveObjectTypesToQuery(ECollisionChannel QueryChannel)", METHOD_TRIVIAL(FCollisionObjectQueryParams, RemoveObjectTypesToQuery));
	FCollisionObjectQueryParams_.Method("int64 GetObjectTypesToQuery() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, GetObjectTypesToQuery));
	FCollisionObjectQueryParams_.Method("void SetObjectTypesToQuery(int64 InObjectTypesToQuery)", METHOD_TRIVIAL(FCollisionObjectQueryParams, SetObjectTypesToQuery));
	FCollisionObjectQueryParams_.Method("int64 GetQueryBitfield64() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, GetQueryBitfield64));
	FCollisionObjectQueryParams_.Method("bool IsValid() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, IsValid));
	FCollisionObjectQueryParams_.Method("void DoVerify() const", METHOD_TRIVIAL(FCollisionObjectQueryParams, DoVerify));

	{
		FAngelscriptBinds::FNamespace ns("FCollisionObjectQueryParams");
		FAngelscriptBinds::BindGlobalFunction("bool IsValidObjectQuery(ECollisionChannel QueryChannel) no_discard", FUNC_TRIVIAL(FCollisionObjectQueryParams::IsValidObjectQuery));
		FAngelscriptBinds::BindGlobalFunction("ECollisionObjectQueryInitType GetCollisionChannelFromOverlapFilter(EOverlapFilterOption Filter) no_discard", FUNC_TRIVIAL(FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter));
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCollisionResponseContainer((int32)FAngelscriptBinds::EOrder::Late, []
{
	auto FCollisionResponseContainer_ = FAngelscriptBinds::ExistingClass("FCollisionResponseContainer");

	FCollisionResponseContainer_.Constructor("void f(ECollisionResponse DefaultResponse)", [](FCollisionResponseContainer* Address, ECollisionResponse DefaultResponse)
	{
		new(Address) FCollisionResponseContainer(DefaultResponse);
	});
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FCollisionResponseContainer_, "FCollisionResponseContainer");

	FCollisionResponseContainer_.Method("bool SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, SetResponse));
	FCollisionResponseContainer_.Method("bool SetAllChannels(ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, SetAllChannels));
	FCollisionResponseContainer_.Method("bool ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)", METHOD_TRIVIAL(FCollisionResponseContainer, ReplaceChannels));
	FCollisionResponseContainer_.Method("ECollisionResponse GetResponse(ECollisionChannel Channel) const", METHOD_TRIVIAL(FCollisionResponseContainer, GetResponse));
	FCollisionResponseContainer_.Method("bool opEquals(const FCollisionResponseContainer& Other) const", METHODPR_TRIVIAL(bool, FCollisionResponseContainer, operator==, (const FCollisionResponseContainer&) const));

	{
		FAngelscriptBinds::FNamespace ns("FCollisionResponseContainer");
		FAngelscriptBinds::BindGlobalFunction("const FCollisionResponseContainer& GetDefaultResponseContainer()", FUNC_TRIVIAL(FCollisionResponseContainer::GetDefaultResponseContainer));
		FAngelscriptBinds::BindGlobalFunction("FCollisionResponseContainer CreateMinContainer(const FCollisionResponseContainer& A, const FCollisionResponseContainer& B)", FUNC_TRIVIAL(FCollisionResponseContainer::CreateMinContainer));
	}
});
