#include "AngelscriptBinds.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/LevelStreaming.h"

#include "FunctionLibraries/AngelscriptLevelStreamingLibrary.h"
#include "FunctionLibraries/RuntimeCurveLinearColorMixinLibrary.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"

namespace
{
	FString CanonicalizeFunctionDeclaration(
		const ANSICHAR* Declaration,
		const bool bFloatIsFloat64)
	{
		const FString Source = UTF8_TO_TCHAR(Declaration != nullptr ? Declaration : "");
		FString Canonical;
		Canonical.Reserve(Source.Len());

		for (int32 Index = 0; Index < Source.Len();)
		{
			const TCHAR Character = Source[Index];
			if (FChar::IsWhitespace(Character))
			{
				++Index;
				continue;
			}

			if (FChar::IsAlpha(Character) || Character == TEXT('_'))
			{
				const int32 IdentifierStart = Index++;
				while (Index < Source.Len()
					&& (FChar::IsAlnum(Source[Index]) || Source[Index] == TEXT('_')))
				{
					++Index;
				}

				const FStringView Identifier(&Source[IdentifierStart], Index - IdentifierStart);
				if (Identifier.Equals(TEXTVIEW("float")))
				{
					Canonical += bFloatIsFloat64 ? TEXT("float64") : TEXT("float32");
				}
				else if (Identifier.Equals(TEXTVIEW("double")))
				{
					Canonical += TEXT("float64");
				}
				else
				{
					Canonical.AppendChars(Identifier.GetData(), Identifier.Len());
				}
				continue;
			}

			Canonical.AppendChar(Character);
			++Index;
		}
		return Canonical;
	}

	bool HasExactMethod(asITypeInfo* TypeInfo, const ANSICHAR* Declaration)
	{
		if (TypeInfo == nullptr || Declaration == nullptr)
		{
			return false;
		}

		asIScriptEngine* ScriptEngine = TypeInfo->GetEngine();
		const bool bFloatIsFloat64 = ScriptEngine != nullptr
			&& ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Expected = CanonicalizeFunctionDeclaration(
			Declaration,
			bFloatIsFloat64);
		for (asUINT Index = 0; Index < TypeInfo->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Method = TypeInfo->GetMethodByIndex(Index);
			if (Method != nullptr
				&& CanonicalizeFunctionDeclaration(
					Method->GetDeclaration(false, false, false, false),
					bFloatIsFloat64) == Expected)
			{
				return true;
			}
		}
		return false;
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FunctionLibraryMixins(
	TEXT("FunctionLibraryMixins.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	[](FAngelscriptBinds& Binds)
{
	auto LevelStreaming_ = Binds.ExistingClassForTarget("ULevelStreaming");
#if WITH_EDITOR
	// Phase 2 of Bind_Defaults (EOrder::Late+100) auto-registers this member via the
	// UAngelscriptLevelStreamingLibrary ScriptMixin-annotated UFUNCTION. We run at
	// EOrder::Late+110, so guard against re-registering the same signature — otherwise
	// AngelScript returns asALREADY_REGISTERED and the asIScriptEngine is left in a
	// broken state (breaks MultiEngine/DependencyInjection tests that rebind on clone).
	asITypeInfo* LevelStreamingType = LevelStreaming_.GetTypeInfo();
	if (!HasExactMethod(LevelStreamingType, "bool GetShouldBeVisibleInEditor() const"))
	{
		LevelStreaming_.Method("bool GetShouldBeVisibleInEditor() const", [](const ULevelStreaming* LevelStreaming) -> bool
		{
			return UAngelscriptLevelStreamingLibrary::GetShouldBeVisibleInEditor(LevelStreaming);
		});
	}
#endif

	auto RuntimeCurveLinearColor_ = Binds.ExistingClassForTarget("FRuntimeCurveLinearColor");
	asITypeInfo* RuntimeCurveLinearColorType = RuntimeCurveLinearColor_.GetTypeInfo();
	if (!HasExactMethod(RuntimeCurveLinearColorType, "void AddDefaultKey(float32, FLinearColor)"))
	{
		RuntimeCurveLinearColor_.Method(
			"void AddDefaultKey(float32 InTime, FLinearColor InColor)",
			[](FRuntimeCurveLinearColor* Target, float InTime, const FLinearColor& InColor)
			{
				Target->ColorCurves[0].AddKey(InTime, InColor.R);
				Target->ColorCurves[1].AddKey(InTime, InColor.G);
				Target->ColorCurves[2].AddKey(InTime, InColor.B);
				Target->ColorCurves[3].AddKey(InTime, InColor.A);
			});
	}

	auto RuntimeFloatCurve_ = Binds.ExistingClassForTarget("FRuntimeFloatCurve");
	asITypeInfo* RuntimeFloatCurveType = RuntimeFloatCurve_.GetTypeInfo();
	if (!HasExactMethod(RuntimeFloatCurveType, "void AddDefaultKey(float32, float32)"))
	{
		RuntimeFloatCurve_.Method(
			"void AddDefaultKey(float32 InTime, float32 InValue)",
			[](FRuntimeFloatCurve* Target, float InTime, float InValue)
			{
				URuntimeFloatCurveMixinLibrary::AddDefaultKey(*Target, InTime, InValue);
			});
	}
	if (!HasExactMethod(RuntimeFloatCurveType, "int GetNumKeys() const"))
	{
		RuntimeFloatCurve_.Method(
			"int GetNumKeys() const",
			[](const FRuntimeFloatCurve* Target) -> int32
			{
				return URuntimeFloatCurveMixinLibrary::GetNumKeys(*Target);
			});
	}
	// Bind_Defaults (EOrder::Late+100) may already have registered these exact
	// ScriptMixin overloads. Compare the complete declaration so another valid
	// overload with the same name never suppresses the supplement.
	auto CurveFloat_ = Binds.ExistingClassForTarget("UCurveFloat");
	asITypeInfo* CurveFloatType = CurveFloat_.GetTypeInfo();
	if (!HasExactMethod(CurveFloatType, "FCurveKeyHandle AddAutoCurveKey(float32, float32)"))
	{
		CurveFloat_.Method(
			"FCurveKeyHandle AddAutoCurveKey(float32 InTime, float32 InValue)",
			[](UCurveFloat* Curve, float InTime, float InValue) -> FCurveKeyHandle
			{
				return URuntimeFloatCurveMixinLibrary::AddAutoCurveKey(Curve, InTime, InValue);
			});
	}
	if (!HasExactMethod(CurveFloatType, "void SetKeyInterpMode(FCurveKeyHandle, ERichCurveInterpMode, bool)"))
	{
		CurveFloat_.Method(
			"void SetKeyInterpMode(FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)",
			[](UCurveFloat* Curve, FCurveKeyHandle KeyHandle, ERichCurveInterpMode NewInterpMode, bool bAutoSetTangents)
			{
				URuntimeFloatCurveMixinLibrary::SetKeyInterpMode(Curve, KeyHandle, NewInterpMode, bAutoSetTangents);
			});
	}

	FAngelscriptBinds::FNamespace RuntimeCurveLinearColorHelperNs(
		Binds.GetTargetEngine(),
		"URuntimeCurveLinearColorMixinLibrary");
	Binds.BindGlobalFunctionForTarget(
		"void AddDefaultKey(FRuntimeCurveLinearColor& Target, float32 InTime, FLinearColor InColor)",
		[](FRuntimeCurveLinearColor& Target, float InTime, const FLinearColor& InColor)
		{
			URuntimeCurveLinearColorMixinLibrary::AddDefaultKey(Target, InTime, InColor);
		});

	FAngelscriptBinds::FNamespace RuntimeFloatCurveHelperNs(
		Binds.GetTargetEngine(),
		"URuntimeFloatCurveMixinLibrary");
	Binds.BindGlobalFunctionForTarget(
		"void GetTimeRange(const FRuntimeFloatCurve& Target, float32&out MinTime, float32&out MaxTime)",
		[](const FRuntimeFloatCurve& Target, float& MinTime, float& MaxTime)
		{
			URuntimeFloatCurveMixinLibrary::GetTimeRange(Target, MinTime, MaxTime);
		});
});
