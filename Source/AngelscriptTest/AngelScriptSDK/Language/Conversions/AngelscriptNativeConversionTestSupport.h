#pragma once

#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeDiagnosticTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

namespace AngelscriptNativeTestSupport
{
	inline FString ResolveNumericScriptType(
		const FNativeTypeCase& TypeCase,
		const asIScriptEngine& Engine)
	{
		if (EqualAnsi(TypeCase.CatalogName, "float32"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
				? TEXT("float32")
				: TEXT("float");
		}

		if (EqualAnsi(TypeCase.CatalogName, "float64"))
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
				? TEXT("float")
				: TEXT("float64");
		}

		return ANSI_TO_TCHAR(TypeCase.ScriptType);
	}

	// The current manual-binding parser deliberately rejects the ambiguous
	// spelling `float` while float is double-backed. Generated script may still
	// publish that spelling, so metadata assertions resolve the semantic scalar
	// through its unambiguous public API name instead.
	inline int ResolveNumericPublicTypeId(
		const FNativeTypeCase& TypeCase,
		const asIScriptEngine& Engine)
	{
		const ANSICHAR* const PublicTypeName = TypeCase.Category == ENativeValueCategory::FloatingPoint
			? TypeCase.CatalogName
			: TypeCase.ScriptType;
		return Engine.GetTypeIdByDecl(PublicTypeName);
	}

	inline const ANSICHAR* ResolveNumericLiteral(
		const FNativeTypeCase& TypeCase,
		const ANSICHAR* ValueName)
	{
		if (EqualAnsi(ValueName, "zero"))
		{
			return TypeCase.ZeroLiteral;
		}

		if (EqualAnsi(ValueName, "one"))
		{
			return TypeCase.OneLiteral;
		}

		if (EqualAnsi(ValueName, "negative"))
		{
			return TypeCase.Category == ENativeValueCategory::UnsignedInteger
				? TypeCase.MaximumLiteral
				: "-1";
		}

		if (EqualAnsi(ValueName, "min"))
		{
			return TypeCase.MinimumLiteral;
		}

		if (EqualAnsi(ValueName, "max"))
		{
			return TypeCase.MaximumLiteral;
		}

		if (EqualAnsi(ValueName, "near_boundary"))
		{
			return TypeCase.NearBoundaryLiteral;
		}

		if (TypeCase.Category == ENativeValueCategory::FloatingPoint)
		{
			return EqualAnsi(TypeCase.CatalogName, "float32")
				? "1.5f"
				: "1.5";
		}

		return "1";
	}

	inline bool HasOwnedLocatedDiagnostic(
		const FNativeMessageCollector& Messages,
		const FString& ModuleName)
	{
		return Messages.Entries.ContainsByPredicate([&ModuleName](const FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Section == ModuleName
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Message.IsEmpty();
		});
	}

	inline asIScriptFunction* FindNoArgumentEntry(
		asIScriptModule* Module,
		const FString& ReturnType,
		const FString& FunctionName)
	{
		const FString Declaration = ReturnType + TEXT(" ") + FunctionName + TEXT("()");
		const FTCHARToUTF8 DeclarationUtf8(*Declaration);
		return GetNativeFunctionByExactDecl(Module, DeclarationUtf8.Get());
	}

	inline int ExecuteNoArgumentEntry(
		asIScriptEngine& Engine,
		asIScriptFunction& Function)
	{
		asIScriptContext* const Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			return asERROR;
		}

		const int Result = PrepareAndExecute(Context, &Function);
		Context->Release();
		return Result;
	}

	// Conversion products frequently return an int marker so their generated
	// source can prove the converted value, rather than only reaching the VM
	// execution boundary. Keep the context alive until the caller has observed
	// the return slot; ExecuteNoArgumentEntry intentionally cannot do that.
	inline int ExecuteNoArgumentIntEntry(
		asIScriptEngine& Engine,
		asIScriptFunction& Function,
		int32& OutValue)
	{
		OutValue = INDEX_NONE;
		asIScriptContext* const Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			return asERROR;
		}

		const int Result = PrepareAndExecute(Context, &Function);
		if (Result == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}

		Context->Release();
		return Result;
	}

	inline bool DiscardAndConfirmAbsent(
		asIScriptEngine& Engine,
		const FTCHARToUTF8& ModuleNameUtf8)
	{
		const int DiscardResult = Engine.DiscardModule(ModuleNameUtf8.Get());
		return DiscardResult >= 0
			&& Engine.GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) == nullptr;
	}

	inline void AppendSimpleIdentityFunction(
		FString& Source,
		const FString& TypeName,
		const FString& Name)
	{
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("%s %s(%s Value)"), *TypeName, *Name, *TypeName));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}
}
