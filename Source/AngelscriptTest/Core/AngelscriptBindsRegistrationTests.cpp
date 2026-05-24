#include "AngelscriptBinds.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "GameFramework/Actor.h"
#include "Math/IntPoint.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptBindsRegistrationTests_Private
{
	FString GetCurrentNamespace(asIScriptEngine* ScriptEngine)
	{
		return ScriptEngine != nullptr ? FString(ANSI_TO_TCHAR(ScriptEngine->GetDefaultNamespace())) : FString();
	}

	asITypeInfo* FindTypeInfoInNamespace(asIScriptEngine* ScriptEngine, const FString& NamespaceName, const FString& TypeName)
	{
		if (ScriptEngine == nullptr)
		{
			return nullptr;
		}

		const FString PreviousNamespace = GetCurrentNamespace(ScriptEngine);
		ScriptEngine->SetDefaultNamespace(TCHAR_TO_ANSI(*NamespaceName));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetDefaultNamespace(TCHAR_TO_ANSI(*PreviousNamespace));
		};

		return ScriptEngine->GetTypeInfoByDecl(TCHAR_TO_ANSI(*TypeName));
	}

	int32 GetEnumValueCount(asITypeInfo* EnumType)
	{
		return EnumType != nullptr ? static_cast<int32>(EnumType->GetEnumValueCount()) : 0;
	}

	bool FindEnumValueByName(asITypeInfo* EnumType, const FString& ValueName, int32& OutValue)
	{
		if (EnumType == nullptr)
		{
			return false;
		}

		for (asUINT ValueIndex = 0, ValueCount = EnumType->GetEnumValueCount(); ValueIndex < ValueCount; ++ValueIndex)
		{
			int32 CurrentValue = 0;
			const char* CurrentName = EnumType->GetEnumValueByIndex(ValueIndex, &CurrentValue);
			if (CurrentName != nullptr && ValueName == ANSI_TO_TCHAR(CurrentName))
			{
				OutValue = CurrentValue;
				return true;
			}
		}

		return false;
	}

	FString MakeAutomationBindTypeName(const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("%s_%s"),
			Prefix,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptBindsRegistrationTests,
	"Angelscript.TestModule.Engine.Binds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(NamespaceGuardRestoresDefaultNamespaceAndEnumBindDeduplicatesValues)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindsRegistrationTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		if (!TestRunner->TestNotNull(TEXT("Binds namespace guard test should expose a script engine"), ScriptEngine))
		{
			return;
		}

		const FString BaselineNamespace = GetCurrentNamespace(ScriptEngine);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetDefaultNamespace(TCHAR_TO_ANSI(*BaselineNamespace));
		};

		const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
		const FString OuterNamespace = FString::Printf(TEXT("AutomationBindEnum%s"), *UniqueSuffix);
		const FString InnerNamespace = FString::Printf(TEXT("AutomationBindInner%s"), *UniqueSuffix);
		const FString EnumTypeName = TEXT("EAutomationBindEnum");

		int32 FirstTypeId = asINVALID_TYPE;
		int32 DuplicateTypeId = asINVALID_TYPE;
		{
			FAngelscriptBinds::FNamespace OuterGuard(OuterNamespace);
			if (!TestRunner->TestEqual(
					TEXT("FNamespace should set the current default namespace while the outer guard is active"),
					GetCurrentNamespace(ScriptEngine),
					OuterNamespace))
			{
				return;
			}

			{
				FAngelscriptBinds::FNamespace InnerGuard(InnerNamespace);
				if (!TestRunner->TestEqual(
						TEXT("Nested FNamespace should override the default namespace inside the inner scope"),
						GetCurrentNamespace(ScriptEngine),
						InnerNamespace))
				{
					return;
				}
			}

			if (!TestRunner->TestEqual(
					TEXT("Destroying the inner FNamespace should restore the outer namespace"),
					GetCurrentNamespace(ScriptEngine),
					OuterNamespace))
			{
				return;
			}

			FAngelscriptBinds::FEnumBind EnumBind = FAngelscriptBinds::Enum(EnumTypeName);
			EnumBind[FString(TEXT("Ready"))] = 1;
			EnumBind[FString(TEXT("Ready"))] = 99;
			EnumBind[FString(TEXT("Done"))] = 2;
			FirstTypeId = EnumBind.TypeId;

			FAngelscriptBinds::FEnumBind DuplicateEnumBind = FAngelscriptBinds::Enum(EnumTypeName);
			DuplicateTypeId = DuplicateEnumBind.TypeId;
		}

		if (!TestRunner->TestEqual(
				TEXT("Destroying the outer FNamespace should restore the baseline namespace"),
				GetCurrentNamespace(ScriptEngine),
				BaselineNamespace))
		{
			return;
		}

		asITypeInfo* GlobalEnumType = FindTypeInfoInNamespace(ScriptEngine, TEXT(""), EnumTypeName);
		asITypeInfo* NamespacedEnumType = FindTypeInfoInNamespace(ScriptEngine, OuterNamespace, EnumTypeName);

		if (!TestRunner->TestNull(TEXT("The namespaced enum should not leak into the global namespace"), GlobalEnumType))
		{
			return;
		}

		if (!TestRunner->TestNotNull(TEXT("The enum should be discoverable in the namespace that registered it"), NamespacedEnumType))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Repeated FEnumBind construction should reuse the existing enum type id"),
				DuplicateTypeId,
				FirstTypeId))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Repeated enum value registration should keep only distinct element names"),
				GetEnumValueCount(NamespacedEnumType),
				2))
		{
			return;
		}

		int32 ReadyValue = INDEX_NONE;
		int32 DoneValue = INDEX_NONE;
		if (!TestRunner->TestTrue(TEXT("The enum should contain the Ready value"), FindEnumValueByName(NamespacedEnumType, TEXT("Ready"), ReadyValue)) ||
			!TestRunner->TestTrue(TEXT("The enum should contain the Done value"), FindEnumValueByName(NamespacedEnumType, TEXT("Done"), DoneValue)))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("The first enum element should keep its original value when assigned again with the same name"),
				ReadyValue,
				1))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("The second enum element should preserve its registered value"), DoneValue, 2))
		{
			return;
		}

		}
	}

	TEST_METHOD(ReferenceAndValueClassPreserveLayoutAndReuseExistingTypeInfo)
	{
		using namespace AngelscriptTest_Core_AngelscriptBindsRegistrationTests_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* IntPointStruct = TBaseStructure<FIntPoint>::Get();
		if (!TestRunner->TestNotNull(TEXT("ReferenceAndValueClass test should expose a script engine"), ScriptEngine) ||
			!TestRunner->TestNotNull(TEXT("ReferenceAndValueClass test should resolve AActor"), ActorClass) ||
			!TestRunner->TestNotNull(TEXT("ReferenceAndValueClass test should resolve FIntPoint"), IntPointStruct))
		{
			return;
		}

		const FString ReferenceTypeName = MakeAutomationBindTypeName(TEXT("AutomationBindRefActor"));
		const FString ValueTypeName = MakeAutomationBindTypeName(TEXT("AutomationBindValuePoint"));

		FAngelscriptBinds ReferenceBinds = FAngelscriptBinds::ReferenceClass(ReferenceTypeName, ActorClass);
		asITypeInfo* ReferenceTypeInfo = ReferenceBinds.GetTypeInfo();
		if (!TestRunner->TestNotNull(TEXT("ReferenceAndValueClass test should register a reference type"), ReferenceTypeInfo))
		{
			return;
		}

		const asDWORD ReferenceFlags = ReferenceTypeInfo->GetFlags();
		TestRunner->TestEqual(
			TEXT("ReferenceClass should preserve the native class structure size"),
			static_cast<int32>(ReferenceTypeInfo->GetSize()),
			ActorClass->GetStructureSize());
		TestRunner->TestEqual(
			TEXT("ReferenceClass should preserve the native class alignment"),
			ReferenceTypeInfo->alignment,
			ActorClass->GetMinAlignment());
		TestRunner->TestTrue(
			TEXT("ReferenceClass should register the type as a reference type"),
			(ReferenceFlags & asOBJ_REF) != 0);
		TestRunner->TestTrue(
			TEXT("ReferenceClass should preserve the no-count trait"),
			(ReferenceFlags & asOBJ_NOCOUNT) != 0);
		TestRunner->TestTrue(
			TEXT("ReferenceClass should preserve the implicit-handle trait"),
			(ReferenceFlags & asOBJ_IMPLICIT_HANDLE) != 0);

		FBindFlags ValueFlags;
		ValueFlags.Alignment = alignof(FIntPoint);
		ValueFlags.bPOD = true;

		FAngelscriptBinds ValueBinds = FAngelscriptBinds::ValueClass(ValueTypeName, IntPointStruct, ValueFlags);
		asITypeInfo* ValueTypeInfo = ValueBinds.GetTypeInfo();
		if (!TestRunner->TestNotNull(TEXT("ReferenceAndValueClass test should register a value type"), ValueTypeInfo))
		{
			return;
		}

		const asDWORD RegisteredValueFlags = ValueTypeInfo->GetFlags();
		TestRunner->TestEqual(
			TEXT("ValueClass should preserve the native struct size"),
			static_cast<int32>(ValueTypeInfo->GetSize()),
			static_cast<int32>(sizeof(FIntPoint)));
		TestRunner->TestEqual(
			TEXT("ValueClass should preserve the requested struct alignment"),
			ValueTypeInfo->alignment,
			static_cast<int32>(alignof(FIntPoint)));
		TestRunner->TestTrue(
			TEXT("ValueClass should register the type as a value type"),
			(RegisteredValueFlags & asOBJ_VALUE) != 0);
		TestRunner->TestTrue(
			TEXT("ValueClass should preserve the app-class trait"),
			(RegisteredValueFlags & asOBJ_APP_CLASS) != 0);
		TestRunner->TestTrue(
			TEXT("ValueClass should preserve the POD trait when requested"),
			(RegisteredValueFlags & asOBJ_POD) != 0);

		FAngelscriptBinds ExistingValueBinds = FAngelscriptBinds::ExistingClass(ValueTypeName);
		asITypeInfo* ExistingValueTypeInfo = ExistingValueBinds.GetTypeInfo();
		if (!TestRunner->TestNotNull(TEXT("ExistingClass should find the previously registered value type"), ExistingValueTypeInfo))
		{
			return;
		}

		FAngelscriptBinds DuplicateValueBinds = FAngelscriptBinds::ValueClass(ValueTypeName, IntPointStruct, ValueFlags);
		asITypeInfo* DuplicateValueTypeInfo = DuplicateValueBinds.GetTypeInfo();
		if (!TestRunner->TestNotNull(TEXT("ValueClass should return the registered type when called with the same name twice"), DuplicateValueTypeInfo))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("ExistingClass should reuse the original value type id"),
			ExistingValueTypeInfo->GetTypeId(),
			ValueTypeInfo->GetTypeId());
		TestRunner->TestTrue(
			TEXT("ExistingClass should reuse the original value type info pointer"),
			ExistingValueTypeInfo == ValueTypeInfo);
		TestRunner->TestEqual(
			TEXT("Repeated ValueClass registration should reuse the original type id"),
			DuplicateValueTypeInfo->GetTypeId(),
			ValueTypeInfo->GetTypeId());
		TestRunner->TestTrue(
			TEXT("Repeated ValueClass registration should reuse the original type info pointer"),
			DuplicateValueTypeInfo == ValueTypeInfo);

		}
	}
};

#endif
