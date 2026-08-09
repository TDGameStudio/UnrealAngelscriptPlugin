#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

namespace
{
	template <typename MutationType>
	bool MutateCurveFloat(UCurveFloat* Curve, MutationType&& Mutation)
	{
		if (Curve == nullptr)
		{
			return false;
		}

#if WITH_EDITOR
		Curve->Modify();
#endif
		Mutation(Curve->FloatCurve);

		TArray<FRichCurveEditInfo> ChangedCurves;
		ChangedCurves.Emplace(&Curve->FloatCurve, FName(*Curve->GetName()));
		Curve->OnCurveChanged(ChangedCurves);
		return true;
	}

	FCurveKeyHandle AddConfiguredKey(
		UCurveFloat* Curve,
		float InTime,
		float InValue,
		TFunctionRef<void(FRichCurve&, FKeyHandle)> Configure)
	{
		FCurveKeyHandle Result;
		Result.KeyHandle = FKeyHandle::Invalid();
		MutateCurveFloat(Curve, [&](FRichCurve& RichCurve)
		{
			Result.KeyHandle = RichCurve.AddKey(InTime, InValue);
			Configure(RichCurve, Result.KeyHandle);
		});
		return Result;
	}
}

void URuntimeFloatCurveMixinLibrary::AddDefaultKey(FRuntimeFloatCurve& Target, float InTime, float InValue)
{
	if (Target.ExternalCurve != nullptr)
	{
		MutateCurveFloat(Target.ExternalCurve, [&](FRichCurve& RichCurve)
		{
			RichCurve.AddKey(InTime, InValue);
		});
		return;
	}

	if (FRichCurve* RichCurve = Target.GetRichCurve())
	{
		RichCurve->AddKey(InTime, InValue);
	}
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddCurveKey(UCurveFloat* Curve, float InTime, float InValue)
{
	return AddConfiguredKey(Curve, InTime, InValue, [](FRichCurve&, FKeyHandle) {});
}

void URuntimeFloatCurveMixinLibrary::AutoSetTangents(UCurveFloat* Curve)
{
	MutateCurveFloat(Curve, [](FRichCurve& RichCurve) { RichCurve.AutoSetTangents(); });
}

void URuntimeFloatCurveMixinLibrary::SetDefaultValue(UCurveFloat* Curve, float DefaultValue)
{
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve) { RichCurve.SetDefaultValue(DefaultValue); });
}

void URuntimeFloatCurveMixinLibrary::SetPreInfinityExtrap(UCurveFloat* Curve, ERichCurveExtrapolation Extrapolation)
{
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve) { RichCurve.PreInfinityExtrap = Extrapolation; });
}

void URuntimeFloatCurveMixinLibrary::SetPostInfinityExtrap(UCurveFloat* Curve, ERichCurveExtrapolation Extrapolation)
{
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve) { RichCurve.PostInfinityExtrap = Extrapolation; });
}

void URuntimeFloatCurveMixinLibrary::SetKeyInterpMode(UCurveFloat* Curve, FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)
{
	if (Curve == nullptr || !Curve->FloatCurve.IsKeyHandleValid(KeyHandle.KeyHandle))
	{
		return;
	}
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve) { RichCurve.SetKeyInterpMode(KeyHandle.KeyHandle, NewInterpMode, bAutoSetTangents); });
}

void URuntimeFloatCurveMixinLibrary::SetKeyTangentMode(UCurveFloat* Curve, FCurveKeyHandle KeyHandle, ERichCurveTangentMode NewTangentMode, bool bAutoSetTangents)
{
	if (Curve == nullptr || !Curve->FloatCurve.IsKeyHandleValid(KeyHandle.KeyHandle))
	{
		return;
	}
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve) { RichCurve.SetKeyTangentMode(KeyHandle.KeyHandle, NewTangentMode, bAutoSetTangents); });
}

void URuntimeFloatCurveMixinLibrary::SetKeyTangentWeightMode(UCurveFloat* Curve, FCurveKeyHandle KeyHandle, ERichCurveTangentWeightMode NewTangentWeightMode, bool bAutoSetTangents)
{
	if (Curve == nullptr || !Curve->FloatCurve.IsKeyHandleValid(KeyHandle.KeyHandle))
	{
		return;
	}
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve) { RichCurve.SetKeyTangentWeightMode(KeyHandle.KeyHandle, NewTangentWeightMode, bAutoSetTangents); });
}

void URuntimeFloatCurveMixinLibrary::SetKeyUserTangents(UCurveFloat* Curve, FCurveKeyHandle KeyHandle, float ArriveTangent, float LeaveTangent)
{
	if (Curve == nullptr || !Curve->FloatCurve.IsKeyHandleValid(KeyHandle.KeyHandle))
	{
		return;
	}
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve)
	{
		FRichCurveKey& Key = RichCurve.GetKey(KeyHandle.KeyHandle);
		Key.ArriveTangent = ArriveTangent;
		Key.LeaveTangent = LeaveTangent;
	});
}

void URuntimeFloatCurveMixinLibrary::SetKeyUserTangentWeights(UCurveFloat* Curve, FCurveKeyHandle KeyHandle, float ArriveTangentWeight, float LeaveTangentWeight)
{
	if (Curve == nullptr || !Curve->FloatCurve.IsKeyHandleValid(KeyHandle.KeyHandle))
	{
		return;
	}
	MutateCurveFloat(Curve, [&](FRichCurve& RichCurve)
	{
		FRichCurveKey& Key = RichCurve.GetKey(KeyHandle.KeyHandle);
		Key.ArriveTangentWeight = ArriveTangentWeight;
		Key.LeaveTangentWeight = LeaveTangentWeight;
	});
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddConstantCurveKey(UCurveFloat* Curve, float InTime, float InValue)
{
	return AddConfiguredKey(Curve, InTime, InValue, [](FRichCurve& RichCurve, FKeyHandle Handle)
	{
		RichCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Constant, false);
	});
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddLinearCurveKey(UCurveFloat* Curve, float InTime, float InValue)
{
	return AddConfiguredKey(Curve, InTime, InValue, [](FRichCurve&, FKeyHandle) {});
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddAutoCurveKey(UCurveFloat* Curve, float InTime, float InValue)
{
	return AddConfiguredKey(Curve, InTime, InValue, [](FRichCurve& RichCurve, FKeyHandle Handle)
	{
		RichCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic, false);
		RichCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Auto, false);
	});
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddSmartAutoCurveKey(UCurveFloat* Curve, float InTime, float InValue)
{
	return AddConfiguredKey(Curve, InTime, InValue, [](FRichCurve& RichCurve, FKeyHandle Handle)
	{
		RichCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic, false);
		RichCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_SmartAuto, false);
	});
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddCurveKeyTangent(UCurveFloat* Curve, float InTime, float InValue, float Tangent)
{
	return AddConfiguredKey(Curve, InTime, InValue, [&](FRichCurve& RichCurve, FKeyHandle Handle)
	{
		RichCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic, false);
		RichCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_User, false);
		FRichCurveKey& Key = RichCurve.GetKey(Handle);
		Key.ArriveTangent = Tangent;
		Key.LeaveTangent = Tangent;
	});
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddCurveKeyBrokenTangent(UCurveFloat* Curve, float InTime, float InValue, float ArriveTangent, float LeaveTangent)
{
	return AddConfiguredKey(Curve, InTime, InValue, [&](FRichCurve& RichCurve, FKeyHandle Handle)
	{
		RichCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic, false);
		RichCurve.SetKeyTangentMode(Handle, ERichCurveTangentMode::RCTM_Break, false);
		FRichCurveKey& Key = RichCurve.GetKey(Handle);
		Key.ArriveTangent = ArriveTangent;
		Key.LeaveTangent = LeaveTangent;
	});
}

namespace
{
	FCurveKeyHandle AddWeightedKey(UCurveFloat* Curve, float InTime, float InValue, bool bBrokenTangent,
		float ArriveTangent, float LeaveTangent, float ArriveTangentWeight, float LeaveTangentWeight,
		ERichCurveTangentWeightMode WeightMode)
	{
		return AddConfiguredKey(Curve, InTime, InValue, [&](FRichCurve& RichCurve, FKeyHandle Handle)
		{
			RichCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic, false);
			RichCurve.SetKeyTangentMode(Handle, bBrokenTangent ? ERichCurveTangentMode::RCTM_Break : ERichCurveTangentMode::RCTM_User, false);
			FRichCurveKey& Key = RichCurve.GetKey(Handle);
			Key.ArriveTangent = ArriveTangent;
			Key.ArriveTangentWeight = ArriveTangentWeight;
			Key.LeaveTangent = LeaveTangent;
			Key.LeaveTangentWeight = LeaveTangentWeight;
			Key.TangentWeightMode = WeightMode;
		});
	}
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddCurveKeyWeightedArriveTangent(UCurveFloat* Curve, float InTime, float InValue, bool bBrokenTangent, float ArriveTangent, float LeaveTangent, float ArriveTangentWeight, float LeaveTangentWeight)
{
	return AddWeightedKey(Curve, InTime, InValue, bBrokenTangent, ArriveTangent, LeaveTangent, ArriveTangentWeight, LeaveTangentWeight, ERichCurveTangentWeightMode::RCTWM_WeightedArrive);
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddCurveKeyWeightedLeaveTangent(UCurveFloat* Curve, float InTime, float InValue, bool bBrokenTangent, float ArriveTangent, float LeaveTangent, float ArriveTangentWeight, float LeaveTangentWeight)
{
	return AddWeightedKey(Curve, InTime, InValue, bBrokenTangent, ArriveTangent, LeaveTangent, ArriveTangentWeight, LeaveTangentWeight, ERichCurveTangentWeightMode::RCTWM_WeightedLeave);
}

FCurveKeyHandle URuntimeFloatCurveMixinLibrary::AddCurveKeyWeightedBothTangent(UCurveFloat* Curve, float InTime, float InValue, bool bBrokenTangent, float ArriveTangent, float LeaveTangent, float ArriveTangentWeight, float LeaveTangentWeight)
{
	return AddWeightedKey(Curve, InTime, InValue, bBrokenTangent, ArriveTangent, LeaveTangent, ArriveTangentWeight, LeaveTangentWeight, ERichCurveTangentWeightMode::RCTWM_WeightedBoth);
}
