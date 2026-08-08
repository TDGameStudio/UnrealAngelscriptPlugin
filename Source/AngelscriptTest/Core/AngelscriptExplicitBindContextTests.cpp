#include "CQTest.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptBindDatabase.h"
#include "Core/AngelscriptDocs.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptType.h"
#include "Binds/Helper_ToString.h"
#include "AngelscriptTestEngine.h"
#include "Bindings/AngelscriptDataTableBindingTestTypes.h"
#include "Misc/Guid.h"
#include "UObject/UObjectIterator.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	struct FExplicitBindValue
	{
		int32 Value = 0;
	};

	static int32 ExplicitGlobalValue = 42;

	static int32 CDECL ReturnExplicitValue()
	{
		return ExplicitGlobalValue;
	}

	static void CDECL NoOpExplicitDirectGeneric(asIScriptGeneric*)
	{
	}

	static FString MakeUniqueIdentifier(const TCHAR* Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}

	static bool HasGlobalProperty(asIScriptEngine& ScriptEngine, const FString& PropertyName)
	{
		for (asUINT PropertyIndex = 0; PropertyIndex < ScriptEngine.GetGlobalPropertyCount(); ++PropertyIndex)
		{
			const char* RegisteredName = nullptr;
			if (ScriptEngine.GetGlobalPropertyByIndex(PropertyIndex, &RegisteredName) >= 0 && RegisteredName != nullptr && PropertyName == ANSI_TO_TCHAR(RegisteredName))
			{
				return true;
			}
		}
		return false;
	}

	static bool UStructSnapshotPreservesEnumerationOrder(const FAngelscriptBindState& BindState)
	{
		TSet<const UScriptStruct*> SnapshotMembers;
		SnapshotMembers.Reserve(BindState.UStructTypeSnapshot.Num());
		for (const TObjectPtr<UScriptStruct>& Struct : BindState.UStructTypeSnapshot)
		{
			SnapshotMembers.Add(Struct.Get());
		}

		int32 SnapshotIndex = 0;
		for (UScriptStruct* Struct : TObjectRange<UScriptStruct>())
		{
			if (!SnapshotMembers.Contains(Struct))
			{
				continue;
			}

			if (!BindState.UStructTypeSnapshot.IsValidIndex(SnapshotIndex)
				|| BindState.UStructTypeSnapshot[SnapshotIndex] != Struct)
			{
				return false;
			}
			++SnapshotIndex;
		}

		return SnapshotIndex == BindState.UStructTypeSnapshot.Num();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptExplicitBindContextTests,
	"Angelscript.TestModule.Engine.BindingArchitecture.ExplicitContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitContextTargetsOnlyTheSelectedEngine)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both explicit-context engines should be created"), EngineA.IsValid() && EngineB.IsValid()))
		{
			return;
		}

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		bool bPassed = true;
		bPassed &= TestRunner->TestEqual(TEXT("Engine A should replay the sealed direct callback collection once"), EngineA->GetBindState()->DirectCallbackExecutionCountForTesting, 1);
		bPassed &= TestRunner->TestEqual(TEXT("Engine B should independently replay the sealed direct callback collection once"), EngineB->GetBindState()->DirectCallbackExecutionCountForTesting, 1);
		bPassed &= TestRunner->TestTrue(TEXT("Context A should retain engine A"), &BindsA.GetTargetEngine() == EngineA.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Context B should retain engine B"), &BindsB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("The contexts should expose distinct script engines"), &BindsA.GetTargetScriptEngine() != &BindsB.GetTargetScriptEngine());
		bPassed &= TestRunner->TestTrue(TEXT("Context A should expose engine A's type database"), &BindsA.GetTargetTypeDatabase() == EngineA->GetTypeDatabase());
		bPassed &= TestRunner->TestTrue(TEXT("Context B should expose engine B's bind database"), &BindsB.GetTargetBindDatabase() == EngineB->GetBindDatabase());
		bPassed &= TestRunner->TestTrue(
			TEXT("Engine A should retain its well-known TArray template type"),
			BindsA.GetTargetTypeDatabase().ArrayTemplateTypeInfo
				== EngineA->GetScriptEngine()->GetTypeInfoByName("TArray"));
		bPassed &= TestRunner->TestTrue(
			TEXT("Engine B should retain its well-known TArray template type"),
			BindsB.GetTargetTypeDatabase().ArrayTemplateTypeInfo
				== EngineB->GetScriptEngine()->GetTypeInfoByName("TArray"));
		bPassed &= TestRunner->TestTrue(
			TEXT("Each engine should own a distinct well-known TArray template type"),
			BindsA.GetTargetTypeDatabase().ArrayTemplateTypeInfo != nullptr
				&& BindsB.GetTargetTypeDatabase().ArrayTemplateTypeInfo != nullptr
				&& BindsA.GetTargetTypeDatabase().ArrayTemplateTypeInfo
					!= BindsB.GetTargetTypeDatabase().ArrayTemplateTypeInfo);
		bPassed &= TestRunner->TestTrue(
			TEXT("Engine A's frozen UStruct snapshot should preserve legacy TObjectRange order"),
			UStructSnapshotPreservesEnumerationOrder(*EngineA->GetBindState()));
		bPassed &= TestRunner->TestTrue(
			TEXT("Engine B's frozen UStruct snapshot should preserve legacy TObjectRange order"),
			UStructSnapshotPreservesEnumerationOrder(*EngineB->GetBindState()));
		bPassed &= TestRunner->TestTrue(TEXT("Explicit contexts should expose distinct ToString stores"), &BindsA.GetTargetToStringList() != &BindsB.GetTargetToStringList());
		bPassed &= TestRunner->TestTrue(TEXT("Explicit contexts should expose distinct interface signature stores"), &BindsA.GetTargetBlueprintEventSignatureRegistry() != &BindsB.GetTargetBlueprintEventSignatureRegistry());

		const int32 TypeFinderCountB = BindsB.GetTargetTypeDatabase().TypeFinders.Num();
		BindsA.GetTargetTypeDatabase().TypeFinders.Add([](FProperty*, FAngelscriptTypeUsage&)
		{
			return false;
		});
		bPassed &= TestRunner->TestEqual(TEXT("A type-finder contribution should not mutate engine B"), BindsB.GetTargetTypeDatabase().TypeFinders.Num(), TypeFinderCountB);

		const int32 BindClassCountB = BindsB.GetTargetBindDatabase().Classes.Num();
		BindsA.GetTargetBindDatabase().Classes.AddDefaulted();
		bPassed &= TestRunner->TestEqual(TEXT("A BindDB contribution should not mutate engine B"), BindsB.GetTargetBindDatabase().Classes.Num(), BindClassCountB);

		const int32 ToStringCountB = BindsB.GetTargetToStringList().Num();
		FToStringHelper::Register(
			BindsA,
			MakeUniqueIdentifier(TEXT("ExplicitToString")),
			+[](void*, FString&) {});
		bPassed &= TestRunner->TestEqual(TEXT("A ToString contribution should not mutate engine B"), BindsB.GetTargetToStringList().Num(), ToStringCountB);

		const FString TypeName = MakeUniqueIdentifier(TEXT("FExplicitBindType"));
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		FAngelscriptBinds TypeBindsA = BindsA.ValueClassForTarget<FExplicitBindValue>(TypeName, TypeFlags);
		bPassed &= TestRunner->TestTrue(TEXT("The explicit value type should register in engine A"), TypeBindsA.GetTypeInfo() != nullptr && EngineA->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)) != nullptr);
		bPassed &= TestRunner->TestTrue(TEXT("The explicit value type should not leak into engine B"), EngineB->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)) == nullptr);
		const int32 ExplicitTypeId = TypeBindsA.GetTypeInfo()->GetTypeId();
		constexpr int32 ExplicitPropertyOffset = 17;
		FAngelscriptDocs::AddUnrealDocumentationForProperty(
			*EngineA,
			ExplicitTypeId,
			ExplicitPropertyOffset,
			TEXT("Engine A reflected property documentation"));
		const TPair<int32, int32> ExplicitPropertyKey(ExplicitTypeId, ExplicitPropertyOffset);
		bPassed &= TestRunner->TestTrue(TEXT("Explicit reflected-property documentation should be stored in engine A"),
			EngineA->GetDocumentationState()->UnrealPropertyDocumentation.FindRef(ExplicitPropertyKey)
				== TEXT("Engine A reflected property documentation"));
		bPassed &= TestRunner->TestFalse(TEXT("Explicit reflected-property documentation should not leak into engine B"),
			EngineB->GetDocumentationState()->UnrealPropertyDocumentation.Contains(ExplicitPropertyKey));

		const FString EnumName = MakeUniqueIdentifier(TEXT("EExplicitBindEnum"));
		FAngelscriptBinds::FEnumBind EnumA = BindsA.EnumForTarget(EnumName);
		EnumA["OnlyInA"] = 37;
		asITypeInfo* EnumTypeA = EngineA->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*EnumName));
		bPassed &= TestRunner->TestTrue(TEXT("The explicit enum should register in engine A"), EnumA.GetTypeInfo() != nullptr && EnumTypeA != nullptr && EnumTypeA->GetEnumValueCount() == 1);
		bPassed &= TestRunner->TestTrue(TEXT("The explicit enum should not leak into engine B"), EngineB->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*EnumName)) == nullptr);

		const FString FunctionName = MakeUniqueIdentifier(TEXT("ExplicitFunction"));
		const FString FunctionDeclaration = FString::Printf(TEXT("int %s()"), *FunctionName);
		const int32 FunctionId = BindsA.BindGlobalFunctionForTarget(FunctionDeclaration, &ReturnExplicitValue);
		asIScriptFunction* FunctionA = EngineA->GetScriptEngine()->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*FunctionDeclaration));
		bPassed &= TestRunner->TestTrue(TEXT("The explicit global function should register in engine A"), FunctionId >= 0 && EngineA->GetScriptEngine()->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*FunctionDeclaration)) != nullptr);
		bPassed &= TestRunner->TestTrue(TEXT("The explicit global function should not leak into engine B"), EngineB->GetScriptEngine()->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*FunctionDeclaration)) == nullptr);
		bPassed &= TestRunner->TestTrue(TEXT("Function provenance should be written to engine A"), FunctionA != nullptr && EngineA->GetBindState()->FunctionProvenanceByPointer.Contains(FunctionA));
		bPassed &= TestRunner->TestFalse(TEXT("Function provenance should not be written to engine B"), FunctionA != nullptr && EngineB->GetBindState()->FunctionProvenanceByPointer.Contains(FunctionA));

		const FString PropertyName = MakeUniqueIdentifier(TEXT("ExplicitProperty"));
		const FString PropertyDeclaration = FString::Printf(TEXT("int %s"), *PropertyName);
		const int32 PropertyId = BindsA.BindGlobalVariableForTarget(PropertyDeclaration, &ExplicitGlobalValue);
		bPassed &= TestRunner->TestTrue(TEXT("The explicit global property should register in engine A"), PropertyId >= 0 && HasGlobalProperty(*EngineA->GetScriptEngine(), PropertyName));
		bPassed &= TestRunner->TestFalse(TEXT("The explicit global property should not leak into engine B"), HasGlobalProperty(*EngineB->GetScriptEngine(), PropertyName));
		FAngelscriptDocs::AddDocumentationForGlobalVariable(*EngineA, PropertyId, TEXT("Engine A property documentation"));
		bPassed &= TestRunner->TestTrue(TEXT("Explicit global-property documentation should be stored in engine A"),
			EngineA->GetDocumentationState()->GlobalVariableDocumentation.FindRef(PropertyId) == TEXT("Engine A property documentation"));
		bPassed &= TestRunner->TestFalse(TEXT("Explicit global-property documentation should not leak into engine B"),
			EngineB->GetDocumentationState()->GlobalVariableDocumentation.Contains(PropertyId));

		const FString FunctionBindingName = MakeUniqueIdentifier(TEXT("ExplicitClassFunctionBinding"));
		const int32 ClassFunctionBindingCountB = EngineB->GetBindState()->ClassFunctionBindings.FindOrAdd(UObject::StaticClass()).Num();
		BindsA.RegisterFunctionBindingForTarget(UObject::StaticClass(), FunctionBindingName, FAngelscriptFunctionBinding());
		bPassed &= TestRunner->TestTrue(TEXT("An explicit class-function binding should be stored in engine A"),
			EngineA->GetBindState()->ClassFunctionBindings.FindOrAdd(UObject::StaticClass()).Contains(FunctionBindingName));
		bPassed &= TestRunner->TestEqual(TEXT("An explicit class-function binding should not mutate engine B"),
			EngineB->GetBindState()->ClassFunctionBindings.FindOrAdd(UObject::StaticClass()).Num(),
			ClassFunctionBindingCountB);

		const FString GeneratedBindingName = MakeUniqueIdentifier(TEXT("ExplicitGeneratedFunctionBinding"));
		FAngelscriptFunctionBinding GeneratedBinding;
		BindsA.RegisterGeneratedFunctionBindingForTarget(
			UObject::StaticClass(),
			GeneratedBindingName,
			GeneratedBinding);
		const FAngelscriptFunctionBinding* StoredGeneratedBinding = EngineA->GetBindState()
			->ClassFunctionBindings.FindOrAdd(UObject::StaticClass()).Find(GeneratedBindingName);
		bPassed &= TestRunner->TestTrue(
			TEXT("An explicit generated binding should be stored only in engine A with generated provenance"),
			StoredGeneratedBinding != nullptr
				&& StoredGeneratedBinding->Origin == EAngelscriptFunctionBindingOrigin::Generated);
		bPassed &= TestRunner->TestFalse(
			TEXT("An explicit generated binding should not mutate engine B"),
			EngineB->GetBindState()->ClassFunctionBindings.FindOrAdd(UObject::StaticClass())
				.Contains(GeneratedBindingName));
		TestRunner->TestTrue(TEXT("All explicit binding mutations should remain engine-scoped"), bPassed);
	}

	TEST_METHOD(ReferenceClassFacadeTargetsOneEngineAndFailsClosedOnIncompatibleReuse)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(
			EngineA.IsValid() && EngineB.IsValid(),
			TEXT("Both explicit reference-class engines should be created")));

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		FAngelscriptBindState& BindStateA = BindsA.GetTargetBindState();
		TGuardValue<FName> OwnerGuard(BindStateA.ActiveBindOwnerModule, TEXT("ReferenceFacadeOwner"));
		TGuardValue<FName> ProviderGuard(BindStateA.ActiveBindProvider, TEXT("ReferenceFacade.Provider"));
		TGuardValue<EAngelscriptBindPhase> PhaseGuard(
			BindStateA.ActiveBindPhase,
			EAngelscriptBindPhase::TypeDeclarations);
		TGuardValue<const ANSICHAR*> SourceFileGuard(
			BindStateA.ActiveBindSourceFile,
			"ReferenceClassFacadeTest.cpp");
		TGuardValue<int32> SourceLineGuard(BindStateA.ActiveBindSourceLine, 4242);

		UClass* NativeClass = UObject::StaticClass();
		const FString TypeName = MakeUniqueIdentifier(TEXT("UExplicitReferenceFacade"));
		const asQWORD ExpectedFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
		FAngelscriptBinds ReferenceA = BindsA.ReferenceClassForTarget(TypeName, NativeClass);
		asITypeInfo* TypeInfoA = ReferenceA.GetTypeInfo();

		ASSERT_THAT(IsNotNull(TypeInfoA, TEXT("The explicit reference-class facade should expose its registered type")));
		ASSERT_THAT(IsFalse(ReferenceA.HasRegistrationFailure(), TEXT("A valid explicit reference-class registration should not fail")));
		ASSERT_THAT(IsTrue(
			&ReferenceA.GetTargetEngine() == EngineA.Get(),
			TEXT("The returned reference-class facade should retain engine A")));
		ASSERT_THAT(IsTrue(
			EngineB->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)) == nullptr,
			TEXT("The explicit reference class should not leak into engine B")));
		ASSERT_THAT(AreEqual(
			ExpectedFlags,
			TypeInfoA->GetFlags(),
			TEXT("The explicit reference class should preserve its exact reference flags")));
		ASSERT_THAT(AreEqual(
			NativeClass->GetStructureSize(),
			static_cast<int32>(TypeInfoA->GetSize()),
			TEXT("The explicit reference class should preserve its native size")));
		ASSERT_THAT(AreEqual(
			NativeClass->GetMinAlignment(),
			TypeInfoA->alignment,
			TEXT("The explicit reference class should preserve its native alignment")));
		ASSERT_THAT(IsTrue(
			TypeInfoA->GetUserData() == NativeClass,
			TEXT("The explicit reference class should preserve its associated UClass")));

		asCObjectType* SemanticType = static_cast<asCObjectType*>(TypeInfoA);
		SemanticType->flags |= asOBJ_EDITOR_ONLY | asOBJ_DISALLOW_INSTANTIATION;
		SemanticType->alignment = NativeClass->GetMinAlignment();

		FAngelscriptBinds CompatibleReference = BindsA.ReferenceClassForTarget(TypeName, NativeClass);
		ASSERT_THAT(IsTrue(
			CompatibleReference.GetTypeInfo() == SemanticType,
			TEXT("A compatible explicit reference-class duplicate should reuse the original type")));
		ASSERT_THAT(IsFalse(
			CompatibleReference.HasRegistrationFailure(),
			TEXT("A compatible explicit reference-class duplicate should allow provider-owned semantic flags")));

		const int32 IncompatibleAlignment = NativeClass->GetMinAlignment() == 1 ? 2 : 1;
		SemanticType->alignment = IncompatibleAlignment;
		FAngelscriptBinds IncompatibleReference = BindsA.ReferenceClassForTarget(TypeName, NativeClass);
		const FString& Diagnostic = IncompatibleReference.GetRegistrationFailureDiagnostic();

		ASSERT_THAT(IsTrue(
			IncompatibleReference.HasRegistrationFailure(),
			TEXT("An incompatible explicit reference-class duplicate should fail closed")));
		ASSERT_THAT(IsNull(
			IncompatibleReference.GetTypeInfo(),
			TEXT("A failed explicit reference-class facade should not lazily recover the incompatible type")));
		ASSERT_THAT(IsTrue(
			Diagnostic.Contains(TEXT("reference class alignment compatibility"))
				&& Diagnostic.Contains(TypeName)
				&& Diagnostic.Contains(TEXT("owner='ReferenceFacadeOwner'"))
				&& Diagnostic.Contains(TEXT("bind='ReferenceFacade.Provider'"))
				&& Diagnostic.Contains(TEXT("phase='TypeDeclarations'"))
				&& Diagnostic.Contains(TEXT("source='ReferenceClassFacadeTest.cpp:4242'")),
			TEXT("The facade-level failure diagnostic should preserve declaration and provider provenance")));
		ASSERT_THAT(IsFalse(
			BindsB.HasRegistrationFailure(),
			TEXT("Engine A's incompatible duplicate should not fail engine B's binding context")));

		const FString AssociatedTypeName = MakeUniqueIdentifier(TEXT("UExplicitReferenceAssociation"));
		FAngelscriptBinds AssociatedReference = BindsB.ReferenceClassForTarget(AssociatedTypeName, NativeClass);
		asITypeInfo* AssociatedType = AssociatedReference.GetTypeInfo();
		ASSERT_THAT(IsNotNull(
			AssociatedType,
			TEXT("Engine B should register an independent associated reference class")));
		AssociatedType->SetUserData(UClass::StaticClass());
		FAngelscriptBinds IncompatibleAssociation = BindsB.ReferenceClassForTarget(AssociatedTypeName, NativeClass);
		ASSERT_THAT(IsTrue(
			IncompatibleAssociation.HasRegistrationFailure()
				&& IncompatibleAssociation.GetTypeInfo() == nullptr
				&& IncompatibleAssociation.GetRegistrationFailureDiagnostic().Contains(
					TEXT("reference class association compatibility")),
			TEXT("A duplicate name associated with another UClass should fail closed")));
	}

	TEST_METHOD(DynamicStructValueClassPreservesLegacyFlagsLayoutAndEngineIsolation)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(
			EngineA.IsValid() && EngineB.IsValid(),
			TEXT("Both dynamic-struct registration engines should be created")));

		FAngelscriptBinds BindsA(*EngineA);
		UScriptStruct* NativeStruct = FAngelscriptBindingDataTableRow::StaticStruct();
		const FString TypeName = MakeUniqueIdentifier(TEXT("FDynamicStructFacade"));
		const FBindFlags Flags;
		FAngelscriptBinds StructBinds = BindsA.ValueClassForTarget(TypeName, NativeStruct, Flags);
		asITypeInfo* TypeInfo = StructBinds.GetTypeInfo();

		ASSERT_THAT(IsNotNull(
			TypeInfo,
			TEXT("The dynamic UScriptStruct facade should register its target value type")));
		ASSERT_THAT(AreEqual(
			static_cast<asQWORD>(asOBJ_VALUE | asOBJ_APP_CLASS),
			TypeInfo->GetFlags(),
			TEXT("The dynamic UScriptStruct facade should not add typed-CDAK flags")));

		UScriptStruct::ICppStructOps* StructOps = NativeStruct->GetCppStructOps();
		const int32 ExpectedSize = StructOps != nullptr
			? StructOps->GetSize()
			: NativeStruct->GetPropertiesSize();
		ASSERT_THAT(AreEqual(
			ExpectedSize,
			static_cast<int32>(TypeInfo->GetSize()),
			TEXT("The dynamic UScriptStruct facade should preserve reflected native size")));
		ASSERT_THAT(AreEqual(
			NativeStruct->GetMinAlignment(),
			TypeInfo->alignment,
			TEXT("The dynamic UScriptStruct facade should preserve reflected native alignment")));
		ASSERT_THAT(IsNull(
			EngineB->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)),
			TEXT("The dynamic UScriptStruct registration should not leak into engine B")));
	}

	TEST_METHOD(DirectRawFunctionFacadesReturnExactTargetResultsWithoutCrossEngineMutation)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(
			EngineA.IsValid() && EngineB.IsValid(),
			TEXT("Both direct raw-registration engines should be created")));

		FAngelscriptBinds BindsA(*EngineA);
		const FString GlobalName = MakeUniqueIdentifier(TEXT("ExplicitDirectGlobal"));
		const FString GlobalDeclaration = FString::Printf(TEXT("int %s()"), *GlobalName);
		FAngelscriptBoundFunction GlobalResult = BindsA.BindGlobalFunctionDirectForTarget(
			GlobalDeclaration,
			asFUNCTION(NoOpExplicitDirectGeneric),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make());
		asIScriptFunction* GlobalFunctionA =
			EngineA->GetScriptEngine()->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*GlobalDeclaration));

		ASSERT_THAT(IsTrue(
			GlobalResult.IsValid()
				&& &GlobalResult.GetTargetEngine() == EngineA.Get()
				&& GlobalResult.GetFunction() == GlobalFunctionA,
			TEXT("The global direct facade should return the exact engine A function")));
		ASSERT_THAT(IsNull(
			EngineB->GetScriptEngine()->GetGlobalFunctionByDecl(TCHAR_TO_ANSI(*GlobalDeclaration)),
			TEXT("The global direct facade should not register in engine B")));
		ASSERT_THAT(IsTrue(
			GlobalFunctionA != nullptr
				&& EngineA->GetBindState()->FunctionProvenanceByPointer.Contains(GlobalFunctionA),
			TEXT("The global direct facade should record provenance in engine A")));
		ASSERT_THAT(IsFalse(
			GlobalFunctionA != nullptr
				&& EngineB->GetBindState()->FunctionProvenanceByPointer.Contains(GlobalFunctionA),
			TEXT("The global direct facade should not record provenance in engine B")));

		const FString TypeName = MakeUniqueIdentifier(TEXT("FExplicitDirectMethod"));
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		FAngelscriptBinds TypeBindsA = BindsA.ValueClassForTarget<FExplicitBindValue>(TypeName, TypeFlags);
		FAngelscriptBoundFunction MethodResult = BindsA.BindMethodDirectForTarget(
			TypeName,
			"int Read() const",
			asFUNCTION(NoOpExplicitDirectGeneric),
			asCALL_GENERIC,
			ASAutoCaller::FunctionCaller::Make());
		asITypeInfo* TypeInfoA = TypeBindsA.GetTypeInfo();
		asIScriptFunction* MethodFunctionA =
			TypeInfoA != nullptr ? TypeInfoA->GetMethodByDecl("int Read() const") : nullptr;

		ASSERT_THAT(IsTrue(
			MethodResult.IsValid()
				&& &MethodResult.GetTargetEngine() == EngineA.Get()
				&& MethodResult.GetFunction() == MethodFunctionA,
			TEXT("The object-method direct facade should return the exact engine A function")));
		ASSERT_THAT(IsNull(
			EngineB->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName)),
			TEXT("The object-method direct facade should not register its owning type in engine B")));
		ASSERT_THAT(IsTrue(
			MethodFunctionA != nullptr
				&& EngineA->GetBindState()->FunctionProvenanceByPointer.Contains(MethodFunctionA),
			TEXT("The object-method direct facade should record provenance in engine A")));
		ASSERT_THAT(IsFalse(
			MethodFunctionA != nullptr
				&& EngineB->GetBindState()->FunctionProvenanceByPointer.Contains(MethodFunctionA),
			TEXT("The object-method direct facade should not record provenance in engine B")));
	}
};

#endif
