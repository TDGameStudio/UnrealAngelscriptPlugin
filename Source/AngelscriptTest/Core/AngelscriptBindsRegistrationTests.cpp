#include "AngelscriptBinds.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "GameFramework/Actor.h"
#include "Math/IntPoint.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptBindsRegistrationTests,
	"Angelscript.TestModule.Engine.Binds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static FString GetCurrentNamespace(asIScriptEngine* ScriptEngine)
{
	return ScriptEngine != nullptr ? FString(ANSI_TO_TCHAR(ScriptEngine->GetDefaultNamespace())) : FString();
}

static asITypeInfo* FindTypeInfoInNamespace(asIScriptEngine* ScriptEngine, const FString& NamespaceName, const FString& TypeName)
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

static int32 GetEnumValueCount(asITypeInfo* EnumType)
{
	return EnumType != nullptr ? static_cast<int32>(EnumType->GetEnumValueCount()) : 0;
}

static bool FindEnumValueByName(asITypeInfo* EnumType, const FString& ValueName, int32& OutValue)
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

static FString MakeAutomationBindTypeName(const TCHAR* Prefix)
{
	return FString::Printf(
		TEXT("%s_%s"),
		Prefix,
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
}

static void CDECL NoOpPreviousBindGuard(void*)
{
}

public:
	TEST_METHOD(NamespaceGuardRestoresDefaultNamespaceAndEnumBindDeduplicatesValues)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Binds namespace guard test should expose a script engine")));

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
			ASSERT_THAT(AreEqual(
				OuterNamespace,
				GetCurrentNamespace(ScriptEngine),
				TEXT("FNamespace should set the current default namespace while the outer guard is active")));

			{
				FAngelscriptBinds::FNamespace InnerGuard(InnerNamespace);
				ASSERT_THAT(AreEqual(
					InnerNamespace,
					GetCurrentNamespace(ScriptEngine),
					TEXT("Nested FNamespace should override the default namespace inside the inner scope")));
			}

			ASSERT_THAT(AreEqual(
				OuterNamespace,
				GetCurrentNamespace(ScriptEngine),
				TEXT("Destroying the inner FNamespace should restore the outer namespace")));

			FAngelscriptBinds::FEnumBind EnumBind = FAngelscriptBinds::Enum(EnumTypeName);
			EnumBind[FString(TEXT("Ready"))] = 1;
			EnumBind[FString(TEXT("Ready"))] = 99;
			EnumBind[FString(TEXT("Done"))] = 2;
			FirstTypeId = EnumBind.TypeId;

			FAngelscriptBinds::FEnumBind DuplicateEnumBind = FAngelscriptBinds::Enum(EnumTypeName);
			DuplicateTypeId = DuplicateEnumBind.TypeId;
		}

		ASSERT_THAT(AreEqual(
			BaselineNamespace,
			GetCurrentNamespace(ScriptEngine),
			TEXT("Destroying the outer FNamespace should restore the baseline namespace")));

		asITypeInfo* GlobalEnumType = FindTypeInfoInNamespace(ScriptEngine, TEXT(""), EnumTypeName);
		asITypeInfo* NamespacedEnumType = FindTypeInfoInNamespace(ScriptEngine, OuterNamespace, EnumTypeName);

		ASSERT_THAT(IsNull(GlobalEnumType, TEXT("The namespaced enum should not leak into the global namespace")));

		ASSERT_THAT(IsNotNull(NamespacedEnumType, TEXT("The enum should be discoverable in the namespace that registered it")));

		ASSERT_THAT(AreEqual(
			FirstTypeId,
			DuplicateTypeId,
			TEXT("Repeated FEnumBind construction should reuse the existing enum type id")));

		ASSERT_THAT(AreEqual(
			2,
			GetEnumValueCount(NamespacedEnumType),
			TEXT("Repeated enum value registration should keep only distinct element names")));

		int32 ReadyValue = INDEX_NONE;
		int32 DoneValue = INDEX_NONE;
		ASSERT_THAT(IsTrue(FindEnumValueByName(NamespacedEnumType, TEXT("Ready"), ReadyValue), TEXT("The enum should contain the Ready value")));
		ASSERT_THAT(IsTrue(FindEnumValueByName(NamespacedEnumType, TEXT("Done"), DoneValue), TEXT("The enum should contain the Done value")));

		ASSERT_THAT(AreEqual(
			1,
			ReadyValue,
			TEXT("The first enum element should keep its original value when assigned again with the same name")));

		ASSERT_THAT(AreEqual(2, DoneValue, TEXT("The second enum element should preserve its registered value")));

		}
	}

	TEST_METHOD(ReferenceAndValueClassPreserveLayoutAndReuseExistingTypeInfo)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		UClass* ActorClass = AActor::StaticClass();
		UScriptStruct* IntPointStruct = TBaseStructure<FIntPoint>::Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("ReferenceAndValueClass test should expose a script engine")));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("ReferenceAndValueClass test should resolve AActor")));
		ASSERT_THAT(IsNotNull(IntPointStruct, TEXT("ReferenceAndValueClass test should resolve FIntPoint")));

		const FString ReferenceTypeName = MakeAutomationBindTypeName(TEXT("AutomationBindRefActor"));
		const FString ValueTypeName = MakeAutomationBindTypeName(TEXT("AutomationBindValuePoint"));

		FAngelscriptBinds ReferenceBinds = FAngelscriptBinds::ReferenceClass(ReferenceTypeName, ActorClass);
		asITypeInfo* ReferenceTypeInfo = ReferenceBinds.GetTypeInfo();
		ASSERT_THAT(IsNotNull(ReferenceTypeInfo, TEXT("ReferenceAndValueClass test should register a reference type")));

		const asDWORD ReferenceFlags = ReferenceTypeInfo->GetFlags();
		ASSERT_THAT(AreEqual(ActorClass->GetStructureSize(), static_cast<int32>(ReferenceTypeInfo->GetSize()), TEXT("ReferenceClass should preserve the native class structure size")));
		ASSERT_THAT(AreEqual(ActorClass->GetMinAlignment(), ReferenceTypeInfo->alignment, TEXT("ReferenceClass should preserve the native class alignment")));
		ASSERT_THAT(IsTrue((ReferenceFlags & asOBJ_REF) != 0, TEXT("ReferenceClass should register the type as a reference type")));
		ASSERT_THAT(IsTrue((ReferenceFlags & asOBJ_NOCOUNT) != 0, TEXT("ReferenceClass should preserve the no-count trait")));
		ASSERT_THAT(IsTrue((ReferenceFlags & asOBJ_IMPLICIT_HANDLE) != 0, TEXT("ReferenceClass should preserve the implicit-handle trait")));

		FBindFlags ValueFlags;
		ValueFlags.Alignment = alignof(FIntPoint);
		ValueFlags.bPOD = true;

		FAngelscriptBinds ValueBinds = FAngelscriptBinds::ValueClass(ValueTypeName, IntPointStruct, ValueFlags);
		asITypeInfo* ValueTypeInfo = ValueBinds.GetTypeInfo();
		ASSERT_THAT(IsNotNull(ValueTypeInfo, TEXT("ReferenceAndValueClass test should register a value type")));

		const asDWORD RegisteredValueFlags = ValueTypeInfo->GetFlags();
		ASSERT_THAT(AreEqual(static_cast<int32>(sizeof(FIntPoint)), static_cast<int32>(ValueTypeInfo->GetSize()), TEXT("ValueClass should preserve the native struct size")));
		ASSERT_THAT(AreEqual(static_cast<int32>(alignof(FIntPoint)), ValueTypeInfo->alignment, TEXT("ValueClass should preserve the requested struct alignment")));
		ASSERT_THAT(IsTrue((RegisteredValueFlags & asOBJ_VALUE) != 0, TEXT("ValueClass should register the type as a value type")));
		ASSERT_THAT(IsTrue((RegisteredValueFlags & asOBJ_APP_CLASS) != 0, TEXT("ValueClass should preserve the app-class trait")));
		ASSERT_THAT(IsTrue((RegisteredValueFlags & asOBJ_POD) != 0, TEXT("ValueClass should preserve the POD trait when requested")));

		FAngelscriptBinds ExistingValueBinds = FAngelscriptBinds::ExistingClass(ValueTypeName);
		asITypeInfo* ExistingValueTypeInfo = ExistingValueBinds.GetTypeInfo();
		ASSERT_THAT(IsNotNull(ExistingValueTypeInfo, TEXT("ExistingClass should find the previously registered value type")));

		FAngelscriptBinds DuplicateValueBinds = FAngelscriptBinds::ValueClass(ValueTypeName, IntPointStruct, ValueFlags);
		asITypeInfo* DuplicateValueTypeInfo = DuplicateValueBinds.GetTypeInfo();
		ASSERT_THAT(IsNotNull(DuplicateValueTypeInfo, TEXT("ValueClass should return the registered type when called with the same name twice")));

		ASSERT_THAT(AreEqual(ValueTypeInfo->GetTypeId(), ExistingValueTypeInfo->GetTypeId(), TEXT("ExistingClass should reuse the original value type id")));
		ASSERT_THAT(IsTrue(ExistingValueTypeInfo == ValueTypeInfo, TEXT("ExistingClass should reuse the original value type info pointer")));
		ASSERT_THAT(AreEqual(ValueTypeInfo->GetTypeId(), DuplicateValueTypeInfo->GetTypeId(), TEXT("Repeated ValueClass registration should reuse the original type id")));
		ASSERT_THAT(IsTrue(DuplicateValueTypeInfo == ValueTypeInfo, TEXT("Repeated ValueClass registration should reuse the original type info pointer")));

		}
	}

	TEST_METHOD(CompileOutPreviousBindHelpersIgnoreFailedRegistration)
	{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString MissingTypeName = MakeAutomationBindTypeName(TEXT("AutomationMissingBindType"));
		FAngelscriptBinds MissingBinds = FAngelscriptBinds::ExistingClass(MissingTypeName);

		int32 FailedFunctionId = INDEX_NONE;
		{
			UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
			ON_SCOPE_EXIT
			{
				UE_SET_LOG_VERBOSITY(Angelscript, Log);
			};

			FailedFunctionId = MissingBinds.Method("void Missing()", &NoOpPreviousBindGuard);
		}

		ASSERT_THAT(IsTrue(FailedFunctionId < 0, TEXT("Binding a method on an unregistered type should fail")));
		ASSERT_THAT(AreEqual(FailedFunctionId, FAngelscriptBinds::GetPreviousFunctionId(), TEXT("Failed registration should still become the previous function id")));
		ASSERT_THAT(IsNull(FAngelscriptBinds::GetPreviousBind(), TEXT("Failed registration should not resolve to a previous script function")));

		FAngelscriptBinds::CompileOutPreviousBind();
		FAngelscriptBinds::CompileOutPreviousBindAsMethodChain();

		ASSERT_THAT(AreEqual(FailedFunctionId, FAngelscriptBinds::GetPreviousFunctionId(), TEXT("Compile-out helpers should leave the failed previous function id unchanged")));
		ASSERT_THAT(IsNull(FAngelscriptBinds::GetPreviousBind(), TEXT("Compile-out helpers should leave the failed previous bind unresolved")));

		}
	}
};

#endif
