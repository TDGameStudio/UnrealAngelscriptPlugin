#include "CQTest.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptDocs.h"
#include "Core/AngelscriptEngine.h"
#include "AngelscriptTestEngine.h"
#include "Binds/Helper_FunctionSignature.h"
#include "FunctionLibraries/SubsystemLibrary.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"
#include "UObject/UObjectGlobals.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptDirectBindFluentTests,
	"Angelscript.TestModule.Engine.BindingArchitecture.Fluent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FDirectFluentValue
	{
		int32 Value = 0;

		int32 GetValue() const
		{
			return Value;
		}
	};

	struct FDirectFluentLargerValue
	{
		int64 Value = 0;
		int64 Extra = 0;
	};

	struct FExplicitSignatureMarkerType final : FAngelscriptType
	{
		virtual FString GetAngelscriptTypeName() const override
		{
			return TEXT("int32");
		}
	};

	static int32 CDECL GetExternalValue(FDirectFluentValue* Object)
	{
		return Object->Value;
	}

	static void CDECL GenericTouch(asIScriptGeneric* Generic)
	{
		(void)Generic;
	}

	static void CDECL ConstructDefault(FDirectFluentValue* Address)
	{
		new (Address) FDirectFluentValue();
	}

	static void CDECL ConstructFromInt(FDirectFluentValue* Address, int32 Value)
	{
		new (Address) FDirectFluentValue();
		Address->Value = Value;
	}

	static void CDECL DestructValue(FDirectFluentValue* Address)
	{
		Address->~FDirectFluentValue();
	}

	static FString MakeFluentTypeName()
	{
		return FString::Printf(TEXT("FDirectFluentValue_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}

public:
	TEST_METHOD(FunctionSignatureUsesExplicitTypeDatabaseOutsideTargetScope)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both explicit-signature engines should be created"), EngineA.IsValid() && EngineB.IsValid()))
		{
			return;
		}

		FAngelscriptTypeDatabase& DatabaseA = *EngineA->GetTypeDatabase();
		TSharedRef<FAngelscriptType> MarkerType = MakeShared<FExplicitSignatureMarkerType>();
		DatabaseA.TypeFinders.Insert(
			[MarkerType](FProperty*, FAngelscriptTypeUsage& Usage)
			{
				Usage.Type = MarkerType;
				return true;
			},
			0);

		UFunction* Function = USubsystemLibrary::StaticClass()->FindFunctionByName(TEXT("GetEngineSubsystem"));
		TSharedPtr<FAngelscriptType> HostType = DatabaseA.TypesByClass.FindRef(USubsystemLibrary::StaticClass());
		if (!TestRunner->TestTrue(TEXT("Explicit-signature test should find GetEngineSubsystem"), Function != nullptr)
			|| !TestRunner->TestTrue(TEXT("Explicit-signature test should resolve its host type from database A"), HostType.IsValid()))
		{
			return;
		}

		FAngelscriptMethodBind DatabaseBind;
		DatabaseBind.Declaration = TEXT("int32 GetEngineSubsystem(int32 Class)");
		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			FAngelscriptFunctionSignature FunctionSignature(DatabaseA, HostType.ToSharedRef(), Function);
			FAngelscriptFunctionSignature DatabaseSignature;
			DatabaseSignature.InitFromDB(DatabaseA, HostType.ToSharedRef(), Function, DatabaseBind, true);

			bool bPassed = true;
			bPassed &= TestRunner->TestTrue(
				TEXT("Explicit constructor should resolve every reflected property through database A"),
				FunctionSignature.ReturnType.Type == MarkerType
					&& FunctionSignature.ArgumentTypes.Num() > 0
					&& FunctionSignature.ArgumentTypes[0].Type == MarkerType);
			bPassed &= TestRunner->TestTrue(
				TEXT("Explicit database initialization should resolve every reflected property through database A"),
				DatabaseSignature.ReturnType.Type == MarkerType
					&& DatabaseSignature.ArgumentTypes.Num() > 0
					&& DatabaseSignature.ArgumentTypes[0].Type == MarkerType);
			TestRunner->TestTrue(TEXT("Explicit signature construction should ignore the ambient engine-B scope"), bPassed);
		}
	}

	TEST_METHOD(FunctionSignatureModificationTargetsExactBoundFunction)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both exact-signature engines should be created"), EngineA.IsValid() && EngineB.IsValid()))
		{
			return;
		}

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		const FString FunctionName = FString::Printf(
			TEXT("ExactFunctionSignatureProbe_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		const FString Declaration = FString::Printf(TEXT("void %s(int32 Value)"), *FunctionName);
		FAngelscriptBoundFunction FunctionA = BindsA.BindGlobalGenericFunctionForTarget(Declaration, &GenericTouch);
		FAngelscriptBoundFunction FunctionB = BindsB.BindGlobalGenericFunctionForTarget(Declaration, &GenericTouch);
		if (!TestRunner->TestTrue(TEXT("Both exact-signature functions should be bound"), FunctionA.IsValid() && FunctionB.IsValid()))
		{
			return;
		}

		UFunction* UnrealFunction = NewObject<UFunction>(GetTransientPackage());
		UnrealFunction->FunctionFlags |= FUNC_EditorOnly;
		UnrealFunction->SetMetaData(NAME_Signature_ToolTip, TEXT("Exact signature documentation"));
		UnrealFunction->SetMetaData(NAME_Signature_Category, TEXT("BindingArchitecture"));
		UnrealFunction->SetMetaData(NAME_UnsafeDuringActorConstruction, TEXT("true"));

		FAngelscriptFunctionSignature Signature;
		Signature.Function = UnrealFunction;
		Signature.WorldContextArgument = 0;
		Signature.DeterminesOutputTypeArgument = 0;
		Signature.bNotAngelscriptProperty = true;
		Signature.bBlueprintProtected = true;
		Signature.bDeprecated = true;
		Signature.DeprecationMessage = TEXT("Use ExactReplacement");
		Signature.bStaticInScript = true;
		Signature.ClassName = TEXT("ExactSignatureNamespace");
		Signature.ScriptName = FunctionName;
		static_cast<asCScriptFunction*>(FunctionA.GetFunction())->SetProperty(true);
		static_cast<asCScriptFunction*>(FunctionB.GetFunction())->SetProperty(true);

		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			Signature.ModifyScriptFunction(FunctionA);
		}

		asCScriptFunction* ScriptFunctionA = static_cast<asCScriptFunction*>(FunctionA.GetFunction());
		asCScriptFunction* ScriptFunctionB = static_cast<asCScriptFunction*>(FunctionB.GetFunction());
		bool bPassed = true;
		bPassed &= TestRunner->TestEqual(TEXT("World context should target the exact function"), int32(ScriptFunctionA->hiddenArgumentIndex), 0);
		bPassed &= TestRunner->TestEqual(TEXT("Output inference should target the exact function"), int32(ScriptFunctionA->determinesOutputTypeArgumentIndex), 0);
		bPassed &= TestRunner->TestFalse(TEXT("Property suppression should target the exact function"), ScriptFunctionA->IsProperty());
		bPassed &= TestRunner->TestTrue(TEXT("Blueprint protection should target the exact function"), ScriptFunctionA->IsProtected());
		bPassed &= TestRunner->TestTrue(TEXT("World-context trait should target the exact function"), ScriptFunctionA->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT));
		bPassed &= TestRunner->TestTrue(TEXT("Editor-only should target the exact function"), ScriptFunctionA->traits.GetTrait(asTRAIT_EDITOR_ONLY));
		bPassed &= TestRunner->TestTrue(TEXT("Deprecation should target the exact function"), ScriptFunctionA->traits.GetTrait(asTRAIT_DEPRECATED));
		bPassed &= TestRunner->TestEqual(
			TEXT("Deprecation message should target the exact function"),
			FString(UTF8_TO_TCHAR(ScriptFunctionA->deprecationMessage.AddressOf())),
			FString(TEXT("Use ExactReplacement")));
		bPassed &= TestRunner->TestTrue(TEXT("Unsafe-construction should target the exact function"), ScriptFunctionA->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION));
		bPassed &= TestRunner->TestEqual(
			TEXT("Documentation should target the exact engine"),
			FAngelscriptDocs::GetUnrealDocumentation(*EngineA, FunctionA.GetFunctionId()),
			FString(TEXT("Exact signature documentation")));
		bPassed &= TestRunner->TestEqual(TEXT("Ambient engine function should keep its visible argument"), int32(ScriptFunctionB->hiddenArgumentIndex), -1);
		bPassed &= TestRunner->TestFalse(TEXT("Ambient engine function should not receive output inference"), ScriptFunctionB->determinesOutputTypeArgumentIndex == 0);
		bPassed &= TestRunner->TestTrue(TEXT("Ambient engine function should remain a property accessor"), ScriptFunctionB->IsProperty());
		bPassed &= TestRunner->TestFalse(TEXT("Ambient engine function should not receive Blueprint protection"), ScriptFunctionB->IsProtected());
		bPassed &= TestRunner->TestTrue(
			TEXT("Ambient engine function should not receive exact-engine documentation"),
			FAngelscriptDocs::GetUnrealDocumentation(*EngineB, FunctionB.GetFunctionId()).IsEmpty());
		FAngelscriptBoundFunction InvalidFunction;
		Signature.ModifyScriptFunction(InvalidFunction);
		bPassed &= TestRunner->TestFalse(
			TEXT("Function signature modification should safely ignore an invalid exact result"),
			InvalidFunction.IsValid());
		TestRunner->TestTrue(TEXT("Function signature mutation should remain exact across engine scopes"), bPassed);
	}

	TEST_METHOD(InterleavedFunctionAndPropertyResultsRetainExactEngineTargets)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both fluent-result engines should be created"), EngineA.IsValid() && EngineB.IsValid()))
		{
			return;
		}

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		const FString TypeName = MakeFluentTypeName();
		FAngelscriptBinds TypeA = BindsA.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);
		FAngelscriptBinds TypeB = BindsB.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);

		FAngelscriptBoundFunction FunctionA = TypeA.Method("int32 GetValue() const", &FDirectFluentValue::GetValue);
		FAngelscriptBoundFunction FunctionB = TypeB.Method("int32 GetValue() const", &FDirectFluentValue::GetValue);
		FunctionA.NoDiscard();

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("Both direct method results should be valid"), FunctionA.IsValid() && FunctionB.IsValid());
		bPassed &= TestRunner->TestTrue(TEXT("Function A should retain engine A"), &FunctionA.GetTargetEngine() == EngineA.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Function B should retain engine B"), &FunctionB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("NoDiscard should mutate the exact engine-A function"), static_cast<asCScriptFunction*>(FunctionA.GetFunction())->traits.GetTrait(asTRAIT_NODISCARD));
		bPassed &= TestRunner->TestFalse(TEXT("Interleaved engine-B registration must not receive engine-A traits"), static_cast<asCScriptFunction*>(FunctionB.GetFunction())->traits.GetTrait(asTRAIT_NODISCARD));

		int32 GlobalA = 11;
		int32 GlobalB = 22;
		const FString PropertyName = FString::Printf(TEXT("DirectFluentProperty_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
		const FString PropertyDeclaration = FString::Printf(TEXT("int %s"), *PropertyName);
		FAngelscriptBoundProperty PropertyA = BindsA.BindGlobalVariableForTarget(PropertyDeclaration, &GlobalA);
		FAngelscriptBoundProperty PropertyB = BindsB.BindGlobalVariableForTarget(PropertyDeclaration, &GlobalB);
		PropertyA.PureConstant(77);

		bPassed &= TestRunner->TestTrue(TEXT("Both direct property results should be valid"), PropertyA.IsValid() && PropertyB.IsValid());
		bPassed &= TestRunner->TestTrue(TEXT("Property A should retain engine A"), &PropertyA.GetTargetEngine() == EngineA.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Property B should retain engine B"), &PropertyB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("PureConstant should mutate only engine A's exact property"), EngineA->Engine->globalProperties[PropertyA.GetPropertyId()]->isPureConstant && EngineA->Engine->globalProperties[PropertyA.GetPropertyId()]->storage == 77);
		bPassed &= TestRunner->TestFalse(TEXT("Engine B's interleaved property should remain mutable"), EngineB->Engine->globalProperties[PropertyB.GetPropertyId()]->isPureConstant);
		TestRunner->TestTrue(TEXT("Direct fluent results should remain exact across interleaved engines"), bPassed);
	}

	TEST_METHOD(AllCallableRegistrationKindsReturnExactResults)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both registration-kind engines should be created"), EngineA.IsValid() && EngineB.IsValid()))
		{
			return;
		}

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		FBindFlags TypeFlags;
		const FString TypeName = MakeFluentTypeName();
		FAngelscriptBinds TypeA = BindsA.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);
		FAngelscriptBinds TypeB = BindsB.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);

		FAngelscriptBoundFunction ExternalA = TypeA.Method("int32 GetExternalValue() const", &GetExternalValue);
		FAngelscriptBoundFunction ExternalB = TypeB.Method("int32 GetExternalValue() const", &GetExternalValue);
		FAngelscriptBoundFunction GenericA = TypeA.GenericMethod("void GenericTouch()", &GenericTouch, nullptr);
		FAngelscriptBoundFunction GenericB = TypeB.GenericMethod("void GenericTouch()", &GenericTouch, nullptr);
		FAngelscriptBoundFunction ConstructorA = TypeA.Constructor("void f()", &ConstructDefault);
		FAngelscriptBoundFunction ConstructorB = TypeB.Constructor("void f()", &ConstructDefault);
		FAngelscriptBoundFunction ImplicitA = TypeA.ImplicitConstructor("void f(int32 Value)", &ConstructFromInt);
		FAngelscriptBoundFunction ImplicitB = TypeB.ImplicitConstructor("void f(int32 Value)", &ConstructFromInt);
		FAngelscriptBoundFunction DestructorA = TypeA.Destructor("void f()", &DestructValue);
		FAngelscriptBoundFunction DestructorB = TypeB.Destructor("void f()", &DestructValue);

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("External method results should be exact and valid"), ExternalA.IsValid() && ExternalB.IsValid() && &ExternalA.GetTargetEngine() == EngineA.Get() && &ExternalB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Generic method results should be exact and valid"), GenericA.IsValid() && GenericB.IsValid() && &GenericA.GetTargetEngine() == EngineA.Get() && &GenericB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Constructor results should be exact and valid"), ConstructorA.IsValid() && ConstructorB.IsValid() && &ConstructorA.GetTargetEngine() == EngineA.Get() && &ConstructorB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Implicit-constructor results should be exact and valid"), ImplicitA.IsValid() && ImplicitB.IsValid() && &ImplicitA.GetTargetEngine() == EngineA.Get() && &ImplicitB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Destructor results should be exact and valid"), DestructorA.IsValid() && DestructorB.IsValid() && &DestructorA.GetTargetEngine() == EngineA.Get() && &DestructorB.GetTargetEngine() == EngineB.Get());
		bPassed &= TestRunner->TestTrue(TEXT("Only the requested registration should be marked implicit"), static_cast<asCScriptFunction*>(ImplicitA.GetFunction())->traits.GetTrait(asTRAIT_IMPLICITCONSTRUCTOR));
		bPassed &= TestRunner->TestFalse(TEXT("Interleaved constructors must not inherit the implicit trait"), static_cast<asCScriptFunction*>(ConstructorB.GetFunction())->traits.GetTrait(asTRAIT_IMPLICITCONSTRUCTOR));
		TestRunner->TestTrue(TEXT("Every callable registration kind should return its exact engine-local result"), bPassed);
	}

	TEST_METHOD(ChainedTraitsMutateOnlyTheStoredFunction)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both trait engines should be created"), EngineA.IsValid() && EngineB.IsValid()))
		{
			return;
		}

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		const FString TypeName = MakeFluentTypeName();
		FAngelscriptBinds TypeA = BindsA.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);
		FAngelscriptBinds TypeB = BindsB.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);

#if AS_CAN_GENERATE_JIT
		const int32 NativeFormCountBeforeTest = FScriptFunctionNativeForm::NumNativeForms();
#endif

		FAngelscriptBoundFunction FunctionA = TypeA.Method("int32 GetValue() const", &FDirectFluentValue::GetValue);
		FAngelscriptBoundFunction FunctionB = TypeB.Method("int32 GetValue() const", &FDirectFluentValue::GetValue);
		EngineA->bCollectStaticJITCompatibilityBinds = true;
		FunctionA
			.EditorOnly()
			.Deprecated("Use Replacement")
			.PropertyAccessor()
			.GeneratedAccessor()
			.NoDiscard()
			.WorldContext()
			.Callable(false)
			.ForceConstArgumentExpressions()
			.DeterminesOutputType(0)
			.PassScriptFunctionAsFirstParam()
			.Documentation(TEXT("Engine A documentation"), TEXT("BindingArchitecture"))
			.NativeMethod("FDirectFluentValue::GetValue", true)
			.CompileOutEntirely();

		asCScriptFunction* ScriptFunctionA = static_cast<asCScriptFunction*>(FunctionA.GetFunction());
		asCScriptFunction* ScriptFunctionB = static_cast<asCScriptFunction*>(FunctionB.GetFunction());
		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("EditorOnly should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_EDITOR_ONLY));
		bPassed &= TestRunner->TestTrue(TEXT("Deprecated should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_DEPRECATED));
#if WITH_EDITOR
		bPassed &= TestRunner->TestTrue(TEXT("Deprecated should retain its message"), FCStringAnsi::Strcmp(ScriptFunctionA->deprecationMessage.AddressOf(), "Use Replacement") == 0);
#endif
		bPassed &= TestRunner->TestTrue(TEXT("PropertyAccessor should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_PROPERTY));
		bPassed &= TestRunner->TestTrue(TEXT("GeneratedAccessor should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_GENERATED_FUNCTION));
		bPassed &= TestRunner->TestTrue(TEXT("NoDiscard should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_NODISCARD));
		bPassed &= TestRunner->TestTrue(TEXT("WorldContext should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT));
		bPassed &= TestRunner->TestTrue(TEXT("Callable(false) should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_NOT_CALLABLE));
		bPassed &= TestRunner->TestTrue(TEXT("ForceConstArgumentExpressions should target the stored function"), ScriptFunctionA->traits.GetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS));
		bPassed &= TestRunner->TestEqual(TEXT("DeterminesOutputType should target the stored function"), static_cast<int32>(ScriptFunctionA->determinesOutputTypeArgumentIndex), 0);
		bPassed &= TestRunner->TestEqual(TEXT("Script-function metadata injection should target the stored function"), ScriptFunctionA->sysFuncIntf->passFirstParamMetaData, asEFirstParamMetaData::ScriptFunction);
		bPassed &= TestRunner->TestEqual(TEXT("CompileOutEntirely should target the stored function"), ScriptFunctionA->compileOutType, asECompileOutType::CompileOutEntirely);
		bPassed &= TestRunner->TestEqual(TEXT("Documentation should be stored by the exact engine"), FAngelscriptDocs::GetUnrealDocumentation(*EngineA, FunctionA.GetFunctionId()), FString(TEXT("Engine A documentation")));
		bPassed &= TestRunner->TestTrue(TEXT("Equivalent ids in another engine must not receive documentation"), FAngelscriptDocs::GetUnrealDocumentation(*EngineB, FunctionB.GetFunctionId()).IsEmpty());
#if AS_CAN_GENERATE_JIT
		FScriptFunctionNativeForm* NativeFormA = FScriptFunctionNativeForm::GetNativeForm(FunctionA.GetFunction());
		bPassed &= TestRunner->TestTrue(TEXT("Native metadata should be attached to the exact engine-A function"), NativeFormA != nullptr && NativeFormA->IsTrivialFunction(EScriptFunctionCallMethod::NativeCall));
		bPassed &= TestRunner->TestTrue(TEXT("Equivalent functions in another engine must not receive native metadata"), FScriptFunctionNativeForm::GetNativeForm(FunctionB.GetFunction()) == nullptr);
#endif
		bPassed &= TestRunner->TestFalse(TEXT("The interleaved engine-B function must retain its default traits"), ScriptFunctionB->traits.GetTrait(asTRAIT_EDITOR_ONLY) || ScriptFunctionB->traits.GetTrait(asTRAIT_DEPRECATED) || ScriptFunctionB->traits.GetTrait(asTRAIT_PROPERTY) || ScriptFunctionB->traits.GetTrait(asTRAIT_GENERATED_FUNCTION) || ScriptFunctionB->traits.GetTrait(asTRAIT_NODISCARD) || ScriptFunctionB->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT) || ScriptFunctionB->traits.GetTrait(asTRAIT_NOT_CALLABLE) || ScriptFunctionB->traits.GetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS));
		bPassed &= TestRunner->TestEqual(TEXT("The interleaved engine-B function should still compile normally"), ScriptFunctionB->compileOutType, asECompileOutType::CompileCalls);

		FAngelscriptBoundFunction ObjectMetadata = TypeB.Method("int32 GetObjectMetadata() const", &FDirectFluentValue::GetValue);
		ObjectMetadata
			.PassScriptObjectTypeAsFirstParam()
			.CompileOutAsMethodChain();
		asCScriptFunction* ObjectMetadataFunction = static_cast<asCScriptFunction*>(ObjectMetadata.GetFunction());
		bPassed &= TestRunner->TestEqual(TEXT("Script-object metadata injection should target its result"), ObjectMetadataFunction->sysFuncIntf->passFirstParamMetaData, asEFirstParamMetaData::ScriptObjectType);
		bPassed &= TestRunner->TestEqual(TEXT("CompileOutAsMethodChain should target its result"), ObjectMetadataFunction->compileOutType, asECompileOutType::CompileOutAsMethodChain);
#if AS_CAN_GENERATE_JIT
		EngineB->bCollectStaticJITCompatibilityBinds = true;
		FunctionB
			.NativeMethod("FDirectFluentValue::GetValue", false)
			.Documentation(TEXT("Engine B documentation"), TEXT("BindingArchitecture"));
		FScriptFunctionNativeForm* NativeFormB = FScriptFunctionNativeForm::GetNativeForm(FunctionB.GetFunction());
		bPassed &= TestRunner->TestTrue(TEXT("Both live engines should own one native form"), NativeFormB != nullptr && FScriptFunctionNativeForm::NumNativeForms() == NativeFormCountBeforeTest + 2);
		EngineA.Reset();
		bPassed &= TestRunner->TestTrue(TEXT("Destroying engine A should release only engine A's native form"), FScriptFunctionNativeForm::NumNativeForms() == NativeFormCountBeforeTest + 1);
		bPassed &= TestRunner->TestTrue(TEXT("Engine B's native form should survive engine A teardown"), FScriptFunctionNativeForm::GetNativeForm(FunctionB.GetFunction()) == NativeFormB);
		bPassed &= TestRunner->TestEqual(TEXT("Engine B documentation should survive engine A teardown"), FAngelscriptDocs::GetUnrealDocumentation(*EngineB, FunctionB.GetFunctionId()), FString(TEXT("Engine B documentation")));
#endif
		TestRunner->TestTrue(TEXT("Chained traits should mutate only exact stored functions"), bPassed);
	}

	TEST_METHOD(ExactNativeFormsExposeConcreteKindsConfigurationsAndOwnership)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(EngineA.IsValid() && EngineB.IsValid(), TEXT("Both specialized-native-metadata engines should be created")));

		UFunction* ReflectedFunction = AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor"));
		ASSERT_THAT(IsNotNull(ReflectedFunction, TEXT("The specialized native UFunction fixture should exist")));

		FAngelscriptBinds BindsA(*EngineA);
		FAngelscriptBinds BindsB(*EngineB);
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		const FString TypeName = MakeFluentTypeName();
		FAngelscriptBinds TypeA = BindsA.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);
		FAngelscriptBinds TypeB = BindsB.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);

		TArray<FAngelscriptBoundFunction> TargetFunctions;
		TArray<FAngelscriptBoundFunction> AdjacentFunctions;
		TArray<FAngelscriptBoundFunction> OtherEngineFunctions;
		TargetFunctions.Reserve(17);
		AdjacentFunctions.Reserve(17);
		OtherEngineFunctions.Reserve(17);
		auto RegisterInterleavedFunctions = [&](const TCHAR* FormName)
		{
			const FString TargetDeclaration = FString::Printf(TEXT("int32 %sTarget() const"), FormName);
			const FString AdjacentDeclaration = FString::Printf(TEXT("int32 %sAdjacent() const"), FormName);
			TargetFunctions.Add(TypeA.Method(TargetDeclaration, &FDirectFluentValue::GetValue));
			AdjacentFunctions.Add(TypeA.Method(AdjacentDeclaration, &FDirectFluentValue::GetValue));
			OtherEngineFunctions.Add(TypeB.Method(TargetDeclaration, &FDirectFluentValue::GetValue));
			return TargetFunctions.Num() - 1;
		};

		const int32 ConstructorIndex = RegisterInterleavedFunctions(TEXT("NativeConstructor"));
		const int32 DestructorIndex = RegisterInterleavedFunctions(TEXT("NativeDestructor"));
		const int32 AssignmentIndex = RegisterInterleavedFunctions(TEXT("NativeAssignment"));
		const int32 UObjectCastIndex = RegisterInterleavedFunctions(TEXT("NativeUObjectCast"));
		const int32 MethodIndex = RegisterInterleavedFunctions(TEXT("NativeMethod"));
		const int32 FunctionIndex = RegisterInterleavedFunctions(TEXT("NativeFunction"));
		const int32 FunctionHeaderIndex = RegisterInterleavedFunctions(TEXT("NativeFunctionHeader"));
		const int32 UFunctionIndex = RegisterInterleavedFunctions(TEXT("NativeUFunction"));
		const int32 TArrayIndexIndex = RegisterInterleavedFunctions(TEXT("NativeTArrayIndex"));
		const int32 IteratorCreateIndex = RegisterInterleavedFunctions(TEXT("NativeTArrayIteratorCreate"));
		const int32 IteratorProceedIndex = RegisterInterleavedFunctions(TEXT("NativeTArrayIteratorProceed"));
		const int32 TemplateCallIndex = RegisterInterleavedFunctions(TEXT("NativeTemplateInstantiatedCall"));
		const int32 DelegateExecuteIndex = RegisterInterleavedFunctions(TEXT("NativeDelegateExecute"));
		const int32 MulticastExecuteIndex = RegisterInterleavedFunctions(TEXT("NativeMulticastExecute"));
		const int32 EventExecuteIndex = RegisterInterleavedFunctions(TEXT("NativeEventFunctionExecute"));
		const int32 PushArgumentIndex = RegisterInterleavedFunctions(TEXT("NativePushArgument"));
		const int32 PushArgumentRefIndex = RegisterInterleavedFunctions(TEXT("NativePushArgumentRef"));

#if AS_CAN_GENERATE_JIT && WITH_DEV_AUTOMATION_TESTS
		const int32 NativeFormCountBeforeTest = FScriptFunctionNativeForm::NumNativeForms();
		EngineA->bCollectStaticJITCompatibilityBinds = false;
		AdjacentFunctions[MethodIndex]
			.NativeMethod("FDirectFluentValue::DisabledGeneration", true);
		ASSERT_THAT(IsNull(
			FScriptFunctionNativeForm::GetNativeForm(AdjacentFunctions[MethodIndex].GetFunction()),
			TEXT("A valid result must remain form-free when precompiled-data generation is disabled")));
		ASSERT_THAT(AreEqual(
			NativeFormCountBeforeTest,
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Disabled precompiled-data generation must not change the native-form count")));
#endif
		EngineA->bCollectStaticJITCompatibilityBinds = true;
		TargetFunctions[ConstructorIndex]
			.NativeConstructor("FDirectFluentValue", true, "0");
		TargetFunctions[DestructorIndex]
			.NativeDestructor("FDirectFluentValue", false);
		TargetFunctions[AssignmentIndex]
			.NativeAssignment("FDirectFluentValue", true);
		TargetFunctions[UObjectCastIndex]
			.NativeUObjectCast(TEXT("UObject"), true);
		TargetFunctions[MethodIndex]
			.NativeMethod("FDirectFluentValue::Method", true);
		TargetFunctions[FunctionIndex]
			.NativeFunction("FDirectFluentValue::Function", false);
		TargetFunctions[FunctionHeaderIndex]
			.NativeFunctionHeader("FDirectFluentValue::FunctionHeader", "FluentFunctionHeader.h", true);
		TargetFunctions[UFunctionIndex]
			.NativeUFunction(ReflectedFunction, TEXT("K2_DestroyActor"), true);
		TargetFunctions[TArrayIndexIndex]
			.NativeTArrayIndex();
		TargetFunctions[IteratorCreateIndex]
			.NativeTArrayIteratorCreate();
		TargetFunctions[IteratorProceedIndex]
			.NativeTArrayIteratorProceed();
		TargetFunctions[TemplateCallIndex]
			.NativeTemplateInstantiatedCall("FDirectFluentValue::CompareOnly", false, true, false);
		TargetFunctions[DelegateExecuteIndex]
			.NativeDelegateExecute();
		TargetFunctions[MulticastExecuteIndex]
			.NativeMulticastExecute();
		TargetFunctions[EventExecuteIndex]
			.NativeEventFunctionExecute();
		TargetFunctions[PushArgumentIndex]
			.NativePushArgument();
		TargetFunctions[PushArgumentRefIndex]
			.NativePushArgumentRef();

		FAngelscriptBoundFunction InvalidResult;
		FAngelscriptBoundFunction* ChainedInvalidResult = &InvalidResult
			.NativeConstructor("FDirectFluentValue::InvalidConstruct", true, "FDirectFluentValue::InvalidCustomConstruct")
			.NativeDestructor("FDirectFluentValue::InvalidDestruct", true)
			.NativeAssignment("FDirectFluentValue::InvalidAssign", true)
			.NativeUObjectCast(TEXT("UInvalidCastTarget"), false)
			.NativeMethod("FDirectFluentValue::InvalidMethod", true)
			.NativeFunction("FDirectFluentValue::InvalidFunction", true)
			.NativeFunctionHeader("FDirectFluentValue::InvalidFunctionHeader", "InvalidFluentFunctionHeader.h", true)
			.NativeUFunction(ReflectedFunction, TEXT("InvalidK2DestroyActor"), true)
			.NativeTArrayIndex()
			.NativeTArrayIteratorCreate()
			.NativeTArrayIteratorProceed()
			.NativeTemplateInstantiatedCall("FDirectFluentValue::InvalidTemplateCall", true, true, true)
			.NativeDelegateExecute()
			.NativeMulticastExecute()
			.NativeEventFunctionExecute()
			.NativePushArgument()
			.NativePushArgumentRef();

		ASSERT_THAT(IsTrue(ChainedInvalidResult == &InvalidResult, TEXT("Every exact native fluent method should return the same invalid result")));
#if AS_CAN_GENERATE_JIT && WITH_DEV_AUTOMATION_TESTS
		TArray<EAngelscriptNativeFormKind> ExpectedKinds;
		ExpectedKinds.SetNum(TargetFunctions.Num());
		ExpectedKinds[ConstructorIndex] = EAngelscriptNativeFormKind::Constructor;
		ExpectedKinds[DestructorIndex] = EAngelscriptNativeFormKind::Destructor;
		ExpectedKinds[AssignmentIndex] = EAngelscriptNativeFormKind::Assignment;
		ExpectedKinds[UObjectCastIndex] = EAngelscriptNativeFormKind::UObjectCast;
		ExpectedKinds[MethodIndex] = EAngelscriptNativeFormKind::Method;
		ExpectedKinds[FunctionIndex] = EAngelscriptNativeFormKind::Function;
		ExpectedKinds[FunctionHeaderIndex] = EAngelscriptNativeFormKind::FunctionHeader;
		ExpectedKinds[UFunctionIndex] = EAngelscriptNativeFormKind::UFunction;
		ExpectedKinds[TArrayIndexIndex] = EAngelscriptNativeFormKind::TArrayIndex;
		ExpectedKinds[IteratorCreateIndex] = EAngelscriptNativeFormKind::TArrayIteratorCreate;
		ExpectedKinds[IteratorProceedIndex] = EAngelscriptNativeFormKind::TArrayIteratorProceed;
		ExpectedKinds[TemplateCallIndex] = EAngelscriptNativeFormKind::TemplateInstantiatedCall;
		ExpectedKinds[DelegateExecuteIndex] = EAngelscriptNativeFormKind::DelegateExecute;
		ExpectedKinds[MulticastExecuteIndex] = EAngelscriptNativeFormKind::MulticastExecute;
		ExpectedKinds[EventExecuteIndex] = EAngelscriptNativeFormKind::EventFunctionExecute;
		ExpectedKinds[PushArgumentIndex] = EAngelscriptNativeFormKind::PushArgument;
		ExpectedKinds[PushArgumentRefIndex] = EAngelscriptNativeFormKind::PushArgumentRef;

		TArray<FString> ExpectedNames;
		ExpectedNames.SetNum(TargetFunctions.Num());
		ExpectedNames[ConstructorIndex] = TEXT("FDirectFluentValue");
		ExpectedNames[DestructorIndex] = TEXT("FDirectFluentValue");
		ExpectedNames[AssignmentIndex] = TEXT("FDirectFluentValue");
		ExpectedNames[MethodIndex] = TEXT("FDirectFluentValue::Method");
		ExpectedNames[FunctionIndex] = TEXT("FDirectFluentValue::Function");
		ExpectedNames[FunctionHeaderIndex] = TEXT("FDirectFluentValue::FunctionHeader");
		ExpectedNames[UFunctionIndex] = TEXT("K2_DestroyActor");
		ExpectedNames[TemplateCallIndex] = TEXT("FDirectFluentValue::CompareOnly");

		TArray<bool> ExpectedTrivial;
		ExpectedTrivial.Init(false, TargetFunctions.Num());
		ExpectedTrivial[ConstructorIndex] = true;
		ExpectedTrivial[AssignmentIndex] = true;
		ExpectedTrivial[MethodIndex] = true;
		ExpectedTrivial[FunctionHeaderIndex] = true;
		ExpectedTrivial[UFunctionIndex] = true;

		TArray<FAngelscriptNativeFormDebugInfo> ObservedForms;
		ObservedForms.Reserve(TargetFunctions.Num());
		for (int32 FormIndex = 0; FormIndex < TargetFunctions.Num(); ++FormIndex)
		{
			FScriptFunctionNativeForm* NativeForm = FScriptFunctionNativeForm::GetNativeForm(TargetFunctions[FormIndex].GetFunction());
			ASSERT_THAT(IsNotNull(
				NativeForm,
				TEXT("Each exact native form should attach to its exact target")));
			const FAngelscriptNativeFormDebugInfo ObservedForm = NativeForm->GetDebugInfoForTesting();
			ObservedForms.Add(ObservedForm);
			ASSERT_THAT(IsTrue(
				ObservedForm.Kind == ExpectedKinds[FormIndex],
				TEXT("Each exact fluent entry must construct its matching concrete native form")));
			ASSERT_THAT(AreEqual(
				ExpectedNames[FormIndex],
				ObservedForm.Name,
				TEXT("Each named native form must preserve its constructor name")));
			ASSERT_THAT(IsTrue(
				ObservedForm.bTrivial == ExpectedTrivial[FormIndex],
				TEXT("Each native form must preserve its trivial-call configuration")));
			ASSERT_THAT(IsNull(
				FScriptFunctionNativeForm::GetNativeForm(AdjacentFunctions[FormIndex].GetFunction()),
				TEXT("A neighboring same-engine function must not receive specialized native metadata")));
			ASSERT_THAT(IsNull(
				FScriptFunctionNativeForm::GetNativeForm(OtherEngineFunctions[FormIndex].GetFunction()),
				TEXT("The equivalent function in another engine must not receive specialized native metadata")));
		}

		ASSERT_THAT(AreEqual(
			FString(TEXT("0")),
			ObservedForms[ConstructorIndex].CustomForm,
			TEXT("The constructor form must preserve its custom form")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("UObject")),
			ObservedForms[UObjectCastIndex].TargetType,
			TEXT("The UObject cast form must preserve its target type")));
		ASSERT_THAT(IsTrue(
			ObservedForms[UObjectCastIndex].bGuaranteed,
			TEXT("The UObject cast form must preserve its guaranteed flag")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("FluentFunctionHeader.h")),
			ObservedForms[FunctionHeaderIndex].Header,
			TEXT("The function-header form must preserve its header")));
		ASSERT_THAT(IsTrue(
			ObservedForms[UFunctionIndex].UnrealFunction == ReflectedFunction,
			TEXT("The UFunction form must preserve its reflected function pointer")));
		ASSERT_THAT(IsTrue(
			ObservedForms[TemplateCallIndex].bNeedsCompare && !ObservedForms[TemplateCallIndex].bNeedsCopy,
			TEXT("The first template form must preserve compare-only configuration")));
		ASSERT_THAT(AreEqual(
			NativeFormCountBeforeTest + TargetFunctions.Num(),
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Engine A should own exactly the specialized forms requested")));

		TargetFunctions[TemplateCallIndex]
			.NativeTemplateInstantiatedCall("FDirectFluentValue::CopyOnly", true, false, true);
		ASSERT_THAT(AreEqual(
			NativeFormCountBeforeTest + TargetFunctions.Num(),
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Replacing a native form on the same function must not increase the native-form count")));
		const FAngelscriptNativeFormDebugInfo ReplacedTemplateForm =
			FScriptFunctionNativeForm::GetNativeForm(TargetFunctions[TemplateCallIndex].GetFunction())->GetDebugInfoForTesting();
		ASSERT_THAT(IsTrue(
			ReplacedTemplateForm.Kind == EAngelscriptNativeFormKind::TemplateInstantiatedCall,
			TEXT("Replacing a template form must retain the concrete template kind")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("FDirectFluentValue::CopyOnly")),
			ReplacedTemplateForm.Name,
			TEXT("Replacing a native form must expose the latest name")));
		ASSERT_THAT(IsTrue(
			ReplacedTemplateForm.bTrivial && !ReplacedTemplateForm.bNeedsCompare && ReplacedTemplateForm.bNeedsCopy,
			TEXT("Replacing a native form must expose the latest copy-only configuration")));

		EngineB->bCollectStaticJITCompatibilityBinds = true;
		OtherEngineFunctions[FunctionHeaderIndex]
			.NativeFunctionHeader("FDirectFluentValue::EngineBFunctionHeader", "EngineBFluentFunctionHeader.h", false);
		FScriptFunctionNativeForm* EngineBForm = FScriptFunctionNativeForm::GetNativeForm(OtherEngineFunctions[FunctionHeaderIndex].GetFunction());
		ASSERT_THAT(IsNotNull(EngineBForm, TEXT("Engine B should own its independently requested specialized form")));
		const FAngelscriptNativeFormDebugInfo EngineBFormInfo = EngineBForm->GetDebugInfoForTesting();
		ASSERT_THAT(IsTrue(
			EngineBFormInfo.Kind == EAngelscriptNativeFormKind::FunctionHeader
				&& EngineBFormInfo.Name == TEXT("FDirectFluentValue::EngineBFunctionHeader")
				&& EngineBFormInfo.Header == TEXT("EngineBFluentFunctionHeader.h")
				&& !EngineBFormInfo.bTrivial,
			TEXT("Engine B must retain its independent concrete form and configuration")));
		EngineA.Reset();
		ASSERT_THAT(AreEqual(
			NativeFormCountBeforeTest + 1,
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Destroying engine A should release only its specialized forms")));
		ASSERT_THAT(IsTrue(
			FScriptFunctionNativeForm::GetNativeForm(OtherEngineFunctions[FunctionHeaderIndex].GetFunction()) == EngineBForm,
			TEXT("Engine B's specialized form should survive engine A teardown")));
		EngineB.Reset();
		ASSERT_THAT(AreEqual(
			NativeFormCountBeforeTest,
			FScriptFunctionNativeForm::NumNativeForms(),
			TEXT("Destroying both engines should release every specialized form")));
#endif
	}

	TEST_METHOD(CompatibleTypeReuseAndIncompatibleReuseFailClosed)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> CompatibleEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> IncompatibleEngine = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("Both type-reuse engines should be created"), CompatibleEngine.IsValid() && IncompatibleEngine.IsValid()))
		{
			return;
		}

		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		const FString CompatibleName = MakeFluentTypeName();
		FAngelscriptBinds CompatibleRoot(*CompatibleEngine);
		CompatibleRoot.ValueClassForTarget<FDirectFluentValue>(CompatibleName, TypeFlags);
		CompatibleRoot.ValueClassForTarget<FDirectFluentValue>(CompatibleName, TypeFlags);

		const FString IncompatibleName = MakeFluentTypeName();
		FAngelscriptBinds IncompatibleRoot(*IncompatibleEngine);
		IncompatibleRoot.ValueClassForTarget<FDirectFluentValue>(IncompatibleName, TypeFlags);
		IncompatibleRoot.ValueClassForTarget<FDirectFluentLargerValue>(IncompatibleName, TypeFlags);

		asITypeInfo* OriginalType = IncompatibleEngine->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*IncompatibleName));
		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("Compatible repeated type declarations should remain valid"), CompatibleRoot.HasRegistrationFailure());
		bPassed &= TestRunner->TestTrue(TEXT("Incompatible repeated type declarations should fail closed"), IncompatibleRoot.HasRegistrationFailure());
		bPassed &= TestRunner->TestTrue(TEXT("The type compatibility diagnostic should retain the declaration"), IncompatibleRoot.GetRegistrationFailureDiagnostic().Contains(IncompatibleName) && IncompatibleRoot.GetRegistrationFailureDiagnostic().Contains(TEXT("compatibility")));
		bPassed &= TestRunner->TestTrue(TEXT("Incompatible reuse must preserve the original registered type"), OriginalType != nullptr && OriginalType->GetSize() == sizeof(FDirectFluentValue));
		TestRunner->TestTrue(TEXT("Repeated direct type registration should be compatible or fail without mutation"), bPassed);
	}

	TEST_METHOD(InvalidResultsPreserveTheFirstFailureAndMutateNothingElse)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("The invalid-result engine should be created"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptBinds Binds(*Engine);
		FBindFlags TypeFlags;
		TypeFlags.bPOD = true;
		const FString TypeName = MakeFluentTypeName();
		FAngelscriptBinds Type = Binds.ValueClassForTarget<FDirectFluentValue>(TypeName, TypeFlags);
		FAngelscriptBoundFunction Untouched = Type.Method("int32 Untouched() const", &FDirectFluentValue::GetValue);
		FAngelscriptBoundFunction First = Type.Method("int32 Duplicate() const", &FDirectFluentValue::GetValue);
		TestRunner->AddExpectedErrorPlain(
			TEXT("and 'int32 Duplicate() const' (Code: asALREADY_REGISTERED"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		FAngelscriptBoundFunction Invalid = Type.Method("int32 Duplicate() const", &FDirectFluentValue::GetValue);
		const FString FirstDiagnostic = Binds.GetRegistrationFailureDiagnostic();
		Engine->bCollectStaticJITCompatibilityBinds = true;
		Invalid
			.EditorOnly()
			.NoDiscard()
			.Documentation(TEXT("Must not be recorded"))
			.NativeMethod("FDirectFluentValue::GetValue", true)
			.CompileOutEntirely();
		FAngelscriptBoundFunction PostFailure = Type.Method("int32 PostFailure() const", &FDirectFluentValue::GetValue);

		asITypeInfo* TypeInfo = Engine->GetScriptEngine()->GetTypeInfoByName(TCHAR_TO_ANSI(*TypeName));
		asCScriptFunction* UntouchedFunction = static_cast<asCScriptFunction*>(Untouched.GetFunction());
		asCScriptFunction* FirstFunction = static_cast<asCScriptFunction*>(First.GetFunction());
		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("The rejected registration should return an invalid result"), Invalid.IsValid());
		bPassed &= TestRunner->TestFalse(TEXT("Registrations after the first failure should also return invalid results"), PostFailure.IsValid());
		bPassed &= TestRunner->TestTrue(TEXT("The binding context should retain its first failure"), Binds.HasRegistrationFailure() && Binds.GetRegistrationFailureDiagnostic() == FirstDiagnostic && FirstDiagnostic.Contains(TEXT("Duplicate")));
		bPassed &= TestRunner->TestFalse(TEXT("Traits chained on an invalid result must not target an earlier function"), UntouchedFunction->traits.GetTrait(asTRAIT_EDITOR_ONLY) || UntouchedFunction->traits.GetTrait(asTRAIT_NODISCARD) || UntouchedFunction->compileOutType != asECompileOutType::CompileCalls);
		bPassed &= TestRunner->TestFalse(TEXT("Traits chained on an invalid result must not target the same-declaration predecessor"), FirstFunction->traits.GetTrait(asTRAIT_EDITOR_ONLY) || FirstFunction->traits.GetTrait(asTRAIT_NODISCARD) || FirstFunction->compileOutType != asECompileOutType::CompileCalls);
		bPassed &= TestRunner->TestTrue(TEXT("Invalid documentation must not create an entry"), FAngelscriptDocs::GetUnrealDocumentation(*Engine, Invalid.GetFunctionId()).IsEmpty());
#if AS_CAN_GENERATE_JIT
		bPassed &= TestRunner->TestTrue(TEXT("Invalid native metadata must not attach to another function"), FScriptFunctionNativeForm::GetNativeForm(Untouched.GetFunction()) == nullptr && FScriptFunctionNativeForm::GetNativeForm(First.GetFunction()) == nullptr);
#endif
		bPassed &= TestRunner->TestTrue(TEXT("Registration after failure must not mutate the object type"), TypeInfo != nullptr && TypeInfo->GetMethodByDecl("int32 PostFailure() const") == nullptr);
		TestRunner->TestTrue(TEXT("Invalid fluent results should preserve the first failure and mutate nothing else"), bPassed);
	}
};

#endif
