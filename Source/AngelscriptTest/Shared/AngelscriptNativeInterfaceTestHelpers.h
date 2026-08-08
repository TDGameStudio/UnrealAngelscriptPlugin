#pragma once

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

/**
 * Ensures that a native C++ UInterface is registered in the current AS engine
 * with its type and methods. This is needed for test-module-defined interfaces
 * (e.g. UAngelscriptNativeParentInterface) that may not be visible during the
 * main engine's Bind_Defaults phase.
 *
 * Uses the same CallInterfaceMethod + FInterfaceMethodSignature pattern as
 * Bind_BlueprintType.cpp Phase 5, but can be called dynamically in test context.
 */
namespace AngelscriptNativeInterfaceTestHelpers
{
	inline void EnsureNativeInterfaceBound(UClass* InterfaceClass)
	{
		if (InterfaceClass == nullptr || InterfaceClass == UInterface::StaticClass())
			return;
		if (!InterfaceClass->HasAnyClassFlags(CLASS_Interface | CLASS_Native))
			return;

		FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
		auto* ScriptEngine = Engine.Engine;
		if (ScriptEngine == nullptr)
			return;
		FAngelscriptBinds Binds(Engine);

		const FString TypeName = FAngelscriptType::GetBoundClassName(InterfaceClass);

		// Check if already registered
		asITypeInfo* ExistingType = ScriptEngine->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName));
		const bool bNewlyRegistered = (ExistingType == nullptr);

		// Register the interface type if not yet registered
		if (bNewlyRegistered)
		{
			FAngelscriptBinds InterfaceBinds = Binds.ReferenceClassForTarget(TypeName, InterfaceClass);
			auto* TypeInfo = (asCTypeInfo*)InterfaceBinds.GetTypeInfo();
			if (TypeInfo != nullptr)
			{
				TypeInfo->plainUserData = (SIZE_T)InterfaceClass;
			}
			ExistingType = InterfaceBinds.GetTypeInfo();

			// Set up UObject shadowType for newly registered types so opCast works.
			if (ExistingType != nullptr)
			{
				asITypeInfo* UObjectType = ScriptEngine->GetTypeInfoByName("UObject");
				if (UObjectType != nullptr)
				{
					ExistingType->CopySystemType(UObjectType);
				}
			}
		}

		if (ExistingType == nullptr)
			return;

		if (ExistingType == nullptr)
			return;

		// Register methods — uses IncludeSuper to get all methods including
		// inherited ones from parent interfaces, then registers each directly
		// on this type. The GetMethodByName skip check avoids duplicates.
		FAngelscriptBinds InterfaceBinds = Binds.ExistingClassForTarget(TypeName);

		for (TFieldIterator<UFunction> FuncIt(InterfaceClass); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;

			if (Function->GetOuter() == UInterface::StaticClass())
				continue;
			if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_BlueprintPure))
				continue;

			FString FuncName = Function->GetName();

			// Skip if already bound
			if (ExistingType->GetMethodByName(TCHAR_TO_ANSI(*FuncName)) != nullptr)
				continue;

			// Build type info
			FAngelscriptTypeUsage ReturnType;
			TArray<FAngelscriptTypeUsage> ArgumentTypes;
			TArray<FString> ArgumentNames;
			TArray<FString> ArgumentDefaults;
			bool bAllTypesValid = true;

			for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				FProperty* Property = *PropIt;
				FAngelscriptTypeUsage Type = FAngelscriptTypeUsage::FromProperty(Property);
				if (!Type.IsValid())
				{
					bAllTypesValid = false;
					break;
				}

				if (Property->PropertyFlags & CPF_ReturnParm)
					ReturnType = Type;
				else
				{
					ArgumentTypes.Add(Type);
					ArgumentNames.Add(Property->GetName());
					ArgumentDefaults.Add(TEXT("-"));
				}
			}

			if (!bAllTypesValid)
				continue;

			FString Declaration = FAngelscriptType::BuildFunctionDeclaration(
				ReturnType, FuncName, ArgumentTypes, ArgumentNames, ArgumentDefaults,
				Function->HasAnyFunctionFlags(FUNC_Const));

			FInterfaceMethodSignature* Sig = Engine.RegisterInterfaceMethodSignature(FName(*FuncName));
			InterfaceBinds.GenericMethod(Declaration, CallInterfaceMethod, Sig);
		}

		// Link parent interface methods
		UClass* SuperInterface = InterfaceClass->GetSuperClass();
		if (SuperInterface != nullptr && SuperInterface != UInterface::StaticClass()
			&& SuperInterface->HasAnyClassFlags(CLASS_Interface))
		{
			// Ensure parent is bound first
			EnsureNativeInterfaceBound(SuperInterface);

			auto SuperType = FAngelscriptType::GetByClass(SuperInterface);
			if (SuperType.IsValid())
			{
				asITypeInfo* ParentScriptType = ScriptEngine->GetTypeInfoByName(
					TCHAR_TO_ANSI(*SuperType->GetAngelscriptTypeName()));
				if (ExistingType != nullptr && ParentScriptType != nullptr)
				{
					ExistingType->CopySystemType(ParentScriptType);
				}
			}
		}
	}
}
