#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageUStructTests
// -----------------------------------------------------------------------------
// Comprehensive USTRUCT coverage for AngelScript, following the matrix from
// OpenSpec: test-coverage-matrix-consolidation/coverage-matrix.md section 1 (USTRUCT).
//
// Coverage matrix index:
//
// | TEST_METHOD | Coverage axis | Execution surface | Notes |
// |---|---|---|---|
// | UStructBasicDeclaration | USTRUCT(), script-only struct, nested struct | Actor BeginPlay + reflection path reads | Baseline declaration smoke |
// | UStructDeclarationAndConstructionEdgeMatrix | Empty USTRUCT, script-only structs, constructors, copy initialization | Actor BeginPlay, UFUNCTION, delegate | Construction/lifecycle edge cases |
// | UStructEmptyContainerShapeMatrix | Empty USTRUCT in array/map/set members and function shapes | Actor BeginPlay + property reflection | Empty type container shape |
// | UStructNamespacedDeclarationAndReflection | USTRUCT inside namespace | UPROPERTY, UFUNCTION parameter, return | Namespaced generated type identity |
// | UStructSpecifiers | BlueprintType and reflected metadata | UScriptStruct metadata/flags | Positive specifier path |
// | UStructUnsupportedSpecifiers | Atomic, Immutable, NoExport | Expected compile diagnostics | Unsupported script-side specifiers |
// | UStructPropertySpecifierFlagMatrix | UPROPERTY flags inside USTRUCT | FProperty flag reflection | Edit/save/transient/visibility style flags |
// | UStructMembers | Primitive/string/name/text/math/object member fields | Actor BeginPlay + reflection path reads | Core member type baseline |
// | UStructExtendedMemberTypeMatrix | Extended text, math, object, soft/weak refs | Actor BeginPlay + reflection path reads | Wider member type coverage |
// | UStructEnumTextAndPropertyFlags | Enum, FText, member flags | FProperty metadata/flags + runtime values | Enum/text member matrix |
// | UStructDefaultValueTypeMatrix | Bool, numeric, text/name, enum, math, null, container defaults | CDO/default object reflection | Default value matrix |
// | UStructBlueprintGeneratedClassBoundary | USTRUCT(BlueprintType) property on transient Blueprint child | Blueprint generated class CDO + spawned actor | Inherited struct default/runtime round-trip |
// | UStructOptionalAndSpecifierCombinations | TOptional<FStruct> as USTRUCT member | Actor BeginPlay + optional property reads | Member optional baseline |
// | UStructValueSemantics | Copy, assignment, comparison, defaults | Actor BeginPlay + reflection path reads | Value semantics baseline |
// | UStructOperators | opEquals, opAdd, opAssign, opCmp, opIndex | Actor BeginPlay + reflected result fields | Operator overload matrix |
// | UStructMemberMethodInvocationMatrix | Const/non-const methods, mutation, struct return | Actor BeginPlay + reflection path reads | Non-operator method baseline |
// | UStructAsParameter | FStruct value, const &in, &out, &inout parameters | Script call path + reflected result fields | Script-side function parameter shapes |
// | UStructUFunctionParameterInvocation | FStruct value/in/out/inout via reflected UFUNCTION | FFunctionInvoker/caller buffers | Native reflection invocation path |
// | UStructUFunctionReturnInvocation | FStruct returned from reflected UFUNCTION | FFunctionInvoker return slot | Native return buffer path |
// | UStructDelegateParameterRoundTrip | FStruct delegate value/in/out/inout/return | BindUFunction + Execute | Real event argument buffer path |
// | UStructDelegateContainerRoundTrip | TArray/TMap/TSet<FStruct> delegate value/in/out/inout/return | BindUFunction + Execute | Container delegate baseline |
// | UStructExtendedMapDelegatePermutationMatrix | TMap<bool,FStruct>, TMap<FStruct,bool>, TMap<FStruct,float> delegate shapes | BindUFunction + Execute | Primitive map delegate permutations |
// | UStructMapKeyValueDelegatePermutationMatrix | FName/FString/float/UObject/FStruct map delegate permutations | BindUFunction + Execute | Extended key/value delegate permutations |
// | UStructAsReturn | FStruct returned from script methods | Actor BeginPlay + reflection path reads | Script-side return baseline |
// | UStructFunctionShapeMatrix | Local/value/in/out/inout/return/container shapes | Actor BeginPlay + reflected result fields | Script-side function shape matrix |
// | UStructContainerParameterShapeMatrix | TArray/TMap/TSet<FStruct> script parameter/return shapes | Actor BeginPlay + reflected result fields | Script-side container parameter shapes |
// | UStructContainerMemberShapeMatrix | USTRUCT members directly owning struct arrays/maps/sets | Actor BeginPlay + nested property reads | Struct-owned container members |
// | UStructExtendedMapMemberPermutationMatrix | USTRUCT member map permutations beyond int/FStruct | Actor BeginPlay + nested property reads | Extended map member permutations |
// | UStructReflectedContainerParameterInvocation | TArray/TMap/TSet<FStruct> reflected UFUNCTION value/in/out/inout/return | FFunctionInvoker/caller buffers | Native container caller-buffer path |
// | UStructInContainers | TArray<FStruct>, TMap with struct values | Actor BeginPlay + container reflection | Legacy container baseline |
// | UStructHashableMapKeyAndSetElement | Hashable FStruct as TMap key/TSet element | Actor BeginPlay + container reflection | Hash/opEquals positive path |
// | UStructKeyContainerParameterAndReturnMatrix | TMap<FStruct,int> and TSet<FStruct> UFUNCTION parameters/returns | FFunctionInvoker/caller buffers | Struct-key reflected container path |
// | UStructStructToStructMapParameterAndReturnMatrix | TMap<FStruct,FStruct> UFUNCTION value/in/out/inout/return | FFunctionInvoker/caller buffers | Struct-key/struct-value reflected map path |
// | UStructMapKeyValueShapeMatrix | FStruct/FName/FString map key-value combinations | Actor BeginPlay + property reflection | Script-side map key/value operations |
// | UStructMapKeyValueParameterAndReturnMatrix | FName/FString/FStruct/UObject map parameter/return combinations | FFunctionInvoker/caller buffers | Extended reflected map key/value path |
// | UStructMapPrimitiveKeyValueParameterAndReturnMatrix | TMap<bool,FStruct>, TMap<FStruct,bool>, TMap<FStruct,float> | FFunctionInvoker/caller buffers | Primitive reflected map key/value path |
// | UStructOptionalReturnMatrix | Optional struct return paths | Script execution + reflected optional reads | Optional return coverage |
// | UStructTypeIdentityAcrossReflectionSites | Same UScriptStruct across properties/functions/containers | Static reflection identity checks | Identity audit; execution covered elsewhere |
// | UStructUnsupportedCombinationBoundaries | Nested containers, invalid hash keys/elements, optional parameter boundaries | Expected compile diagnostics | Negative/boundary matrix |
// | UStructNested | Nested struct within struct | Actor BeginPlay + nested path reads | Legacy nested baseline |
// | UStructNestedDefaultsReflection | Nested defaults and reflected nested members | CDO/default object reflection | Nested default matrix |
// | UStructMetadataAliasAndDeprecationMatrix | ScriptName, DeprecatedProperty, DeprecationMessage member metadata | FProperty metadata + CDO nested default reflection | Alias/deprecation metadata baseline |
// | UStructAdvancedMetadata | DisplayName, ToolTip, ShortToolTip, custom and clamp/ui/unit metadata | FProperty metadata reflection | Advanced metadata baseline |
//
// Current follow-up focus while this matrix is still being closed:
//
// | Gap | Target surface | Notes |
// |---|---|---|
// | Optional boundary reconciliation | Unsupported optional parameters/nesting | Do not mark docs until verified |
// | Matrix-to-document reconciliation | OpenSpec test-coverage-matrix-consolidation USTRUCT rows | Update markers only after implementation/static review |
//
// Pattern: spawn AS actor, drive members via AngelScript, validate through
// FProperty reflection (GetByPath/SetByPath/VerifyByPath).
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageUStructTest,
	"Angelscript.TestModule.Coverage.UStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FScriptFloatProperty = FDoubleProperty;
	using FScriptFloatValue = double;

	struct FScopedTransientBlueprint
	{
		UBlueprint* Blueprint = nullptr;
		UPackage* Package = nullptr;

		~FScopedTransientBlueprint()
		{
			Cleanup();
		}

		bool CreateAndCompile(FAutomationTestBase& Test, UClass* ParentClass, FStringView Suffix)
		{
			FNoDiscardAsserter LocalAssert(Test);
			if (!LocalAssert.IsNotNull(ParentClass, TEXT("UStruct Blueprint boundary test should have a script parent class")))
			{
				return false;
			}

			const FString PackagePath = FString::Printf(
				TEXT("/Temp/AngelscriptCoverageUStruct_%.*s_%s"),
				Suffix.Len(),
				Suffix.GetData(),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			Package = CreatePackage(*PackagePath);
			if (!LocalAssert.IsNotNull(Package, TEXT("UStruct Blueprint boundary test should create a transient package")))
			{
				return false;
			}

			Package->SetFlags(RF_Transient);
			const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

			Blueprint = FKismetEditorUtilities::CreateBlueprint(
				ParentClass,
				Package,
				BlueprintName,
				BPTYPE_Normal,
				UBlueprint::StaticClass(),
				UBlueprintGeneratedClass::StaticClass(),
				TEXT("AngelscriptCoverageUStructTests"));
			if (!LocalAssert.IsNotNull(Blueprint, TEXT("UStruct Blueprint boundary test should create a transient Blueprint asset")))
			{
				return false;
			}

			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			return LocalAssert.IsNotNull(Blueprint->GeneratedClass.Get(), TEXT("UStruct Blueprint boundary test should compile a generated class"));
		}

		UClass* GetGeneratedClass() const
		{
			return Blueprint != nullptr ? Blueprint->GeneratedClass.Get() : nullptr;
		}

		void Cleanup()
		{
			if (Blueprint == nullptr)
			{
				if (Package != nullptr)
				{
					Package->MarkAsGarbage();
					CollectGarbage(RF_NoFlags, true);
					Package = nullptr;
				}
				return;
			}

			if (UClass* BlueprintClass = Blueprint->GeneratedClass)
			{
				BlueprintClass->MarkAsGarbage();
			}

			if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
			{
				BlueprintPackage->MarkAsGarbage();
			}

			Blueprint->MarkAsGarbage();
			CollectGarbage(RF_NoFlags, true);
			Blueprint = nullptr;
			Package = nullptr;
		}
	};

	template <typename KeyPropertyType, typename KeyValueType>
	static bool GetMapStructValueByPath(
		FAutomationTestBase& Test,
		UObject* Object,
		FStringView Path,
		const KeyValueType& Key,
		const FStructProperty*& OutStructProperty,
		const void*& OutValueAddress)
	{
		FPropertyBindingPathIndirection Leaf;
		if (!ResolvePathOnObject(Test, Object, Path, Leaf))
		{
			return false;
		}

		const FMapProperty* MapProperty = CastField<const FMapProperty>(Leaf.GetProperty());
		if (!ExpectNotNull(Test,
				*FString::Printf(TEXT("Property '%.*s' should be a TMap"), Path.Len(), Path.GetData()),
				MapProperty))
		{
			return false;
		}

		const KeyPropertyType* KeyProperty = CastField<const KeyPropertyType>(MapProperty->KeyProp);
		if (!ExpectNotNull(Test,
				*FString::Printf(TEXT("TMap key property at '%.*s' should match the expected FProperty type"), Path.Len(), Path.GetData()),
				KeyProperty))
		{
			return false;
		}

		OutStructProperty = CastField<const FStructProperty>(MapProperty->ValueProp);
		if (!ExpectNotNull(Test,
				*FString::Printf(TEXT("TMap value property at '%.*s' should be FStructProperty"), Path.Len(), Path.GetData()),
				OutStructProperty))
		{
			return false;
		}

		FScriptMapHelper Helper(MapProperty, Leaf.GetPropertyAddress());
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const KeyValueType ActualKey = KeyProperty->GetPropertyValue(Helper.GetKeyPtr(SparseIndex));
			if (ActualKey == Key)
			{
				OutValueAddress = Helper.GetValuePtr(SparseIndex);
				return true;
			}
		}

		FailTest(Test, FString::Printf(
			TEXT("TMap at '%.*s' does not contain the expected key"),
			Path.Len(), Path.GetData()));
		return false;
	}

	static bool GetMapStructValueByPath(
		FAutomationTestBase& Test,
		UObject* Object,
		FStringView Path,
		int32 Key,
		const FStructProperty*& OutStructProperty,
		const void*& OutValueAddress)
	{
		return GetMapStructValueByPath<FIntProperty, int32>(
			Test,
			Object,
			Path,
			Key,
			OutStructProperty,
			OutValueAddress);
	}

	static bool ExpectPropertyFlags(
		const FProperty* Property,
		EPropertyFlags RequiredFlags)
	{
		return Property != nullptr && Property->HasAllPropertyFlags(RequiredFlags);
	}

	static bool ExpectDelegateParameterFlags(
		FAutomationTestBase& Test,
		const FProperty* Property,
		const TCHAR* ContextLabel,
		EPropertyFlags RequiredFlags)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		bPassed &= LocalAssert.IsTrue(
			Property->HasAnyPropertyFlags(CPF_Parm),
			*FString::Printf(TEXT("%s delegate parameter should carry CPF_Parm"), ContextLabel));

		if (RequiredFlags != CPF_None)
		{
			bPassed &= LocalAssert.IsTrue(
				Property->HasAllPropertyFlags(RequiredFlags),
				*FString::Printf(TEXT("%s delegate parameter should carry the expected flags"), ContextLabel));
		}

		return bPassed;
	}

	template <typename KeyPropertyType, typename ValuePropertyType>
	static bool ExpectDelegateMapParameter(
		FAutomationTestBase& Test,
		const FDelegateProperty* DelegateProperty,
		const TCHAR* ContextLabel,
		EPropertyFlags RequiredFlags,
		FMapProperty*& OutMapProperty)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		OutMapProperty = nullptr;

		bPassed &= LocalAssert.IsNotNull(
			DelegateProperty,
			*FString::Printf(TEXT("%s delegate property should reflect"), ContextLabel));
		if (DelegateProperty == nullptr)
		{
			return false;
		}

		UFunction* SignatureFunction = DelegateProperty->SignatureFunction.Get();
		bPassed &= LocalAssert.IsNotNull(
			SignatureFunction,
			*FString::Printf(TEXT("%s delegate should keep its signature function"), ContextLabel));
		if (SignatureFunction == nullptr)
		{
			return false;
		}

		OutMapProperty = FindFProperty<FMapProperty>(SignatureFunction, TEXT("Items"));
		bPassed &= LocalAssert.IsNotNull(
			OutMapProperty,
			*FString::Printf(TEXT("%s delegate parameter should reflect as FMapProperty"), ContextLabel));
		if (OutMapProperty == nullptr)
		{
			return false;
		}

		bPassed &= ExpectDelegateParameterFlags(Test, OutMapProperty, ContextLabel, RequiredFlags);
		bPassed &= LocalAssert.IsNotNull(
			CastField<KeyPropertyType>(OutMapProperty->KeyProp),
			*FString::Printf(TEXT("%s delegate key should reflect as the expected property type"), ContextLabel));
		bPassed &= LocalAssert.IsNotNull(
			CastField<ValuePropertyType>(OutMapProperty->ValueProp),
			*FString::Printf(TEXT("%s delegate value should reflect as the expected property type"), ContextLabel));

		return bPassed;
	}

	template <typename KeyPropertyType, typename ValuePropertyType>
	static bool ExpectDelegateMapReturn(
		FAutomationTestBase& Test,
		const FDelegateProperty* DelegateProperty,
		const TCHAR* ContextLabel,
		FMapProperty*& OutMapProperty)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		OutMapProperty = nullptr;

		bPassed &= LocalAssert.IsNotNull(
			DelegateProperty,
			*FString::Printf(TEXT("%s delegate property should reflect"), ContextLabel));
		if (DelegateProperty == nullptr)
		{
			return false;
		}

		UFunction* SignatureFunction = DelegateProperty->SignatureFunction.Get();
		bPassed &= LocalAssert.IsNotNull(
			SignatureFunction,
			*FString::Printf(TEXT("%s delegate should keep its signature function"), ContextLabel));
		if (SignatureFunction == nullptr)
		{
			return false;
		}

		OutMapProperty = CastField<FMapProperty>(SignatureFunction->GetReturnProperty());
		bPassed &= LocalAssert.IsNotNull(
			OutMapProperty,
			*FString::Printf(TEXT("%s delegate return should reflect as FMapProperty"), ContextLabel));
		if (OutMapProperty == nullptr)
		{
			return false;
		}

		bPassed &= LocalAssert.IsTrue(
			OutMapProperty->HasAnyPropertyFlags(CPF_ReturnParm),
			*FString::Printf(TEXT("%s delegate return should carry CPF_ReturnParm"), ContextLabel));
		bPassed &= LocalAssert.IsNotNull(
			CastField<KeyPropertyType>(OutMapProperty->KeyProp),
			*FString::Printf(TEXT("%s delegate return key should reflect as the expected property type"), ContextLabel));
		bPassed &= LocalAssert.IsNotNull(
			CastField<ValuePropertyType>(OutMapProperty->ValueProp),
			*FString::Printf(TEXT("%s delegate return value should reflect as the expected property type"), ContextLabel));

		return bPassed;
	}

	template <typename KeyPropertyType, typename ValuePropertyType>
	static bool ExpectDelegateMapPermutation(
		FAutomationTestBase& Test,
		UClass* ScriptClass,
		const TCHAR* ValueSignalName,
		const TCHAR* InSignalName,
		const TCHAR* OutSignalName,
		const TCHAR* InoutSignalName,
		const TCHAR* ReturnSignalName,
		const TCHAR* ContextLabel,
		FMapProperty*& OutValueParameter,
		FMapProperty*& OutInParameter,
		FMapProperty*& OutOutParameter,
		FMapProperty*& OutInoutParameter,
		FMapProperty*& OutReturnProperty)
	{
		const FDelegateProperty* ValueSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, ValueSignalName);
		const FDelegateProperty* InSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, InSignalName);
		const FDelegateProperty* OutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, OutSignalName);
		const FDelegateProperty* InoutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, InoutSignalName);
		const FDelegateProperty* ReturnSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, ReturnSignalName);

		bool bPassed = true;
		bPassed &= ExpectDelegateMapParameter<KeyPropertyType, ValuePropertyType>(
			Test,
			ValueSignalProperty,
			*FString::Printf(TEXT("%s value"), ContextLabel),
			CPF_None,
			OutValueParameter);
		bPassed &= ExpectDelegateMapParameter<KeyPropertyType, ValuePropertyType>(
			Test,
			InSignalProperty,
			*FString::Printf(TEXT("%s const-ref"), ContextLabel),
			CPF_ConstParm | CPF_OutParm,
			OutInParameter);
		bPassed &= ExpectDelegateMapParameter<KeyPropertyType, ValuePropertyType>(
			Test,
			OutSignalProperty,
			*FString::Printf(TEXT("%s out"), ContextLabel),
			CPF_OutParm,
			OutOutParameter);
		bPassed &= ExpectDelegateMapParameter<KeyPropertyType, ValuePropertyType>(
			Test,
			InoutSignalProperty,
			*FString::Printf(TEXT("%s inout"), ContextLabel),
			CPF_ReferenceParm | CPF_OutParm,
			OutInoutParameter);
		bPassed &= ExpectDelegateMapReturn<KeyPropertyType, ValuePropertyType>(
			Test,
			ReturnSignalProperty,
			*FString::Printf(TEXT("%s return"), ContextLabel),
			OutReturnProperty);
		return bPassed;
	}

	template <typename ElementPropertyType>
	static bool ExpectDelegateArrayParameter(
		FAutomationTestBase& Test,
		const FDelegateProperty* DelegateProperty,
		const TCHAR* ContextLabel,
		EPropertyFlags RequiredFlags,
		FArrayProperty*& OutArrayProperty)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		OutArrayProperty = nullptr;

		bPassed &= LocalAssert.IsNotNull(
			DelegateProperty,
			*FString::Printf(TEXT("%s delegate property should reflect"), ContextLabel));
		if (DelegateProperty == nullptr)
		{
			return false;
		}

		UFunction* SignatureFunction = DelegateProperty->SignatureFunction.Get();
		bPassed &= LocalAssert.IsNotNull(
			SignatureFunction,
			*FString::Printf(TEXT("%s delegate should keep its signature function"), ContextLabel));
		if (SignatureFunction == nullptr)
		{
			return false;
		}

		OutArrayProperty = FindFProperty<FArrayProperty>(SignatureFunction, TEXT("Items"));
		bPassed &= LocalAssert.IsNotNull(
			OutArrayProperty,
			*FString::Printf(TEXT("%s delegate parameter should reflect as FArrayProperty"), ContextLabel));
		if (OutArrayProperty == nullptr)
		{
			return false;
		}

		bPassed &= ExpectDelegateParameterFlags(Test, OutArrayProperty, ContextLabel, RequiredFlags);
		bPassed &= LocalAssert.IsNotNull(
			CastField<ElementPropertyType>(OutArrayProperty->Inner),
			*FString::Printf(TEXT("%s delegate element should reflect as the expected property type"), ContextLabel));

		return bPassed;
	}

	template <typename ElementPropertyType>
	static bool ExpectDelegateArrayReturn(
		FAutomationTestBase& Test,
		const FDelegateProperty* DelegateProperty,
		const TCHAR* ContextLabel,
		FArrayProperty*& OutArrayProperty)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		OutArrayProperty = nullptr;

		bPassed &= LocalAssert.IsNotNull(
			DelegateProperty,
			*FString::Printf(TEXT("%s delegate property should reflect"), ContextLabel));
		if (DelegateProperty == nullptr)
		{
			return false;
		}

		UFunction* SignatureFunction = DelegateProperty->SignatureFunction.Get();
		bPassed &= LocalAssert.IsNotNull(
			SignatureFunction,
			*FString::Printf(TEXT("%s delegate should keep its signature function"), ContextLabel));
		if (SignatureFunction == nullptr)
		{
			return false;
		}

		OutArrayProperty = CastField<FArrayProperty>(SignatureFunction->GetReturnProperty());
		bPassed &= LocalAssert.IsNotNull(
			OutArrayProperty,
			*FString::Printf(TEXT("%s delegate return should reflect as FArrayProperty"), ContextLabel));
		if (OutArrayProperty == nullptr)
		{
			return false;
		}

		bPassed &= LocalAssert.IsTrue(
			OutArrayProperty->HasAnyPropertyFlags(CPF_ReturnParm),
			*FString::Printf(TEXT("%s delegate return should carry CPF_ReturnParm"), ContextLabel));
		bPassed &= LocalAssert.IsNotNull(
			CastField<ElementPropertyType>(OutArrayProperty->Inner),
			*FString::Printf(TEXT("%s delegate return element should reflect as the expected property type"), ContextLabel));

		return bPassed;
	}

	template <typename ElementPropertyType>
	static bool ExpectDelegateArrayPermutation(
		FAutomationTestBase& Test,
		UClass* ScriptClass,
		const TCHAR* ValueSignalName,
		const TCHAR* InSignalName,
		const TCHAR* OutSignalName,
		const TCHAR* InoutSignalName,
		const TCHAR* ReturnSignalName,
		const TCHAR* ContextLabel,
		FArrayProperty*& OutValueParameter,
		FArrayProperty*& OutInParameter,
		FArrayProperty*& OutOutParameter,
		FArrayProperty*& OutInoutParameter,
		FArrayProperty*& OutReturnProperty)
	{
		const FDelegateProperty* ValueSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, ValueSignalName);
		const FDelegateProperty* InSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, InSignalName);
		const FDelegateProperty* OutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, OutSignalName);
		const FDelegateProperty* InoutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, InoutSignalName);
		const FDelegateProperty* ReturnSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, ReturnSignalName);

		bool bPassed = true;
		bPassed &= ExpectDelegateArrayParameter<ElementPropertyType>(
			Test,
			ValueSignalProperty,
			*FString::Printf(TEXT("%s value"), ContextLabel),
			CPF_None,
			OutValueParameter);
		bPassed &= ExpectDelegateArrayParameter<ElementPropertyType>(
			Test,
			InSignalProperty,
			*FString::Printf(TEXT("%s const-ref"), ContextLabel),
			CPF_ConstParm | CPF_OutParm,
			OutInParameter);
		bPassed &= ExpectDelegateArrayParameter<ElementPropertyType>(
			Test,
			OutSignalProperty,
			*FString::Printf(TEXT("%s out"), ContextLabel),
			CPF_OutParm,
			OutOutParameter);
		bPassed &= ExpectDelegateArrayParameter<ElementPropertyType>(
			Test,
			InoutSignalProperty,
			*FString::Printf(TEXT("%s inout"), ContextLabel),
			CPF_ReferenceParm | CPF_OutParm,
			OutInoutParameter);
		bPassed &= ExpectDelegateArrayReturn<ElementPropertyType>(
			Test,
			ReturnSignalProperty,
			*FString::Printf(TEXT("%s return"), ContextLabel),
			OutReturnProperty);
		return bPassed;
	}

	template <typename ElementPropertyType>
	static bool ExpectDelegateSetParameter(
		FAutomationTestBase& Test,
		const FDelegateProperty* DelegateProperty,
		const TCHAR* ContextLabel,
		EPropertyFlags RequiredFlags,
		FSetProperty*& OutSetProperty)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		OutSetProperty = nullptr;

		bPassed &= LocalAssert.IsNotNull(
			DelegateProperty,
			*FString::Printf(TEXT("%s delegate property should reflect"), ContextLabel));
		if (DelegateProperty == nullptr)
		{
			return false;
		}

		UFunction* SignatureFunction = DelegateProperty->SignatureFunction.Get();
		bPassed &= LocalAssert.IsNotNull(
			SignatureFunction,
			*FString::Printf(TEXT("%s delegate should keep its signature function"), ContextLabel));
		if (SignatureFunction == nullptr)
		{
			return false;
		}

		OutSetProperty = FindFProperty<FSetProperty>(SignatureFunction, TEXT("Items"));
		bPassed &= LocalAssert.IsNotNull(
			OutSetProperty,
			*FString::Printf(TEXT("%s delegate parameter should reflect as FSetProperty"), ContextLabel));
		if (OutSetProperty == nullptr)
		{
			return false;
		}

		bPassed &= ExpectDelegateParameterFlags(Test, OutSetProperty, ContextLabel, RequiredFlags);
		bPassed &= LocalAssert.IsNotNull(
			CastField<ElementPropertyType>(OutSetProperty->ElementProp),
			*FString::Printf(TEXT("%s delegate element should reflect as the expected property type"), ContextLabel));

		return bPassed;
	}

	template <typename ElementPropertyType>
	static bool ExpectDelegateSetReturn(
		FAutomationTestBase& Test,
		const FDelegateProperty* DelegateProperty,
		const TCHAR* ContextLabel,
		FSetProperty*& OutSetProperty)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;
		OutSetProperty = nullptr;

		bPassed &= LocalAssert.IsNotNull(
			DelegateProperty,
			*FString::Printf(TEXT("%s delegate property should reflect"), ContextLabel));
		if (DelegateProperty == nullptr)
		{
			return false;
		}

		UFunction* SignatureFunction = DelegateProperty->SignatureFunction.Get();
		bPassed &= LocalAssert.IsNotNull(
			SignatureFunction,
			*FString::Printf(TEXT("%s delegate should keep its signature function"), ContextLabel));
		if (SignatureFunction == nullptr)
		{
			return false;
		}

		OutSetProperty = CastField<FSetProperty>(SignatureFunction->GetReturnProperty());
		bPassed &= LocalAssert.IsNotNull(
			OutSetProperty,
			*FString::Printf(TEXT("%s delegate return should reflect as FSetProperty"), ContextLabel));
		if (OutSetProperty == nullptr)
		{
			return false;
		}

		bPassed &= LocalAssert.IsTrue(
			OutSetProperty->HasAnyPropertyFlags(CPF_ReturnParm),
			*FString::Printf(TEXT("%s delegate return should carry CPF_ReturnParm"), ContextLabel));
		bPassed &= LocalAssert.IsNotNull(
			CastField<ElementPropertyType>(OutSetProperty->ElementProp),
			*FString::Printf(TEXT("%s delegate return element should reflect as the expected property type"), ContextLabel));

		return bPassed;
	}

	template <typename ElementPropertyType>
	static bool ExpectDelegateSetPermutation(
		FAutomationTestBase& Test,
		UClass* ScriptClass,
		const TCHAR* ValueSignalName,
		const TCHAR* InSignalName,
		const TCHAR* OutSignalName,
		const TCHAR* InoutSignalName,
		const TCHAR* ReturnSignalName,
		const TCHAR* ContextLabel,
		FSetProperty*& OutValueParameter,
		FSetProperty*& OutInParameter,
		FSetProperty*& OutOutParameter,
		FSetProperty*& OutInoutParameter,
		FSetProperty*& OutReturnProperty)
	{
		const FDelegateProperty* ValueSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, ValueSignalName);
		const FDelegateProperty* InSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, InSignalName);
		const FDelegateProperty* OutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, OutSignalName);
		const FDelegateProperty* InoutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, InoutSignalName);
		const FDelegateProperty* ReturnSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, ReturnSignalName);

		bool bPassed = true;
		bPassed &= ExpectDelegateSetParameter<ElementPropertyType>(
			Test,
			ValueSignalProperty,
			*FString::Printf(TEXT("%s value"), ContextLabel),
			CPF_None,
			OutValueParameter);
		bPassed &= ExpectDelegateSetParameter<ElementPropertyType>(
			Test,
			InSignalProperty,
			*FString::Printf(TEXT("%s const-ref"), ContextLabel),
			CPF_ConstParm | CPF_OutParm,
			OutInParameter);
		bPassed &= ExpectDelegateSetParameter<ElementPropertyType>(
			Test,
			OutSignalProperty,
			*FString::Printf(TEXT("%s out"), ContextLabel),
			CPF_OutParm,
			OutOutParameter);
		bPassed &= ExpectDelegateSetParameter<ElementPropertyType>(
			Test,
			InoutSignalProperty,
			*FString::Printf(TEXT("%s inout"), ContextLabel),
			CPF_ReferenceParm | CPF_OutParm,
			OutInoutParameter);
		bPassed &= ExpectDelegateSetReturn<ElementPropertyType>(
			Test,
			ReturnSignalProperty,
			*FString::Printf(TEXT("%s return"), ContextLabel),
			OutReturnProperty);
		return bPassed;
	}

	static void SetStructItemFields(
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		void* ItemAddress,
		int32 ID,
		FName Tag)
	{
		IDProperty.SetPropertyValue_InContainer(ItemAddress, ID);
		TagProperty.SetPropertyValue_InContainer(ItemAddress, Tag);
	}

	static void SetStructScoreLabelFields(
		const FIntProperty& ScoreProperty,
		const FStrProperty& LabelProperty,
		void* ItemAddress,
		int32 Score,
		const FString& Label)
	{
		ScoreProperty.SetPropertyValue_InContainer(ItemAddress, Score);
		LabelProperty.SetPropertyValue_InContainer(ItemAddress, Label);
	}

	template <typename PointerType>
	static bool ExpectNotNull(FAutomationTestBase& Test, const TCHAR* Message, PointerType* Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Value, Message);
	}

	template <typename PointerType>
	static bool ExpectNotNull(FAutomationTestBase& Test, const TCHAR* Message, const TObjectPtr<PointerType>& Value)
	{
		return ExpectNotNull(Test, Message, Value.Get());
	}

	static bool ExpectTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bValue)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bValue, Message);
	}

	template <typename ExpectedType, typename ActualType>
	static bool ExpectEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreEqual(Expected, Actual, Message);
	}

	static bool FailTest(FAutomationTestBase& Test, const FString& Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(false, *Message);
	}

	static bool FailTest(FAutomationTestBase& Test, const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(false, Message);
	}

	static bool AddStructItemToArray(
		FAutomationTestBase& Test,
		const FArrayProperty& ArrayProperty,
		void* ArrayAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag)
	{
		FScriptArrayHelper Helper(&ArrayProperty, ArrayAddress);
		const int32 Index = Helper.AddValue();
		void* ItemAddress = Helper.GetRawPtr(Index);
		if (!ExpectNotNull(Test, TEXT("TArray<FStruct> item should expose writable memory"), ItemAddress))
		{
			return false;
		}

		SetStructItemFields(IDProperty, TagProperty, ItemAddress, ID, Tag);
		return true;
	}

	static bool AddStructItemToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 Key,
		int32 ID,
		FName Tag)
	{
		const FStructProperty* ValueProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<int,FStruct> value should be a struct property"), ValueProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<int,FStruct> value struct type should be available"), ValueProperty->Struct))
		{
			return false;
		}

		FStructOnScope ValueScope(ValueProperty->Struct);
		void* ValueStorage = ValueScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<int,FStruct> temporary value should allocate struct memory"), ValueStorage))
		{
			return false;
		}

		SetStructItemFields(IDProperty, TagProperty, ValueStorage, ID, Tag);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(&Key, ValueStorage);
		return true;
	}

	static bool AddStructKeyToIntMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag,
		int32 Value)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> key should be a struct property"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FIntProperty* ValueProperty = CastField<const FIntProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> value should be an int property"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> temporary key should allocate struct memory"), KeyStorage))
		{
			return false;
		}

		SetStructItemFields(IDProperty, TagProperty, KeyStorage, ID, Tag);
		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(KeyStorage, &Value);
		return true;
	}

	static bool AddStructKeyStructValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& KeyIDProperty,
		const FNameProperty& KeyTagProperty,
		int32 KeyID,
		FName KeyTag,
		const FIntProperty& ValueScoreProperty,
		const FStrProperty& ValueLabelProperty,
		int32 ValueScore,
		const FString& ValueLabel)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> key should be a struct property"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FStructProperty* ValueProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> value should be a struct property"), ValueProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> value struct type should be available"), ValueProperty->Struct))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> temporary key should allocate struct memory"), KeyStorage))
		{
			return false;
		}

		FStructOnScope ValueScope(ValueProperty->Struct);
		void* ValueStorage = ValueScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> temporary value should allocate struct memory"), ValueStorage))
		{
			return false;
		}

		SetStructItemFields(KeyIDProperty, KeyTagProperty, KeyStorage, KeyID, KeyTag);
		SetStructScoreLabelFields(ValueScoreProperty, ValueLabelProperty, ValueStorage, ValueScore, ValueLabel);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(KeyStorage, ValueStorage);
		return true;
	}

	template <typename KeyPropertyType, typename KeyValueType>
	static bool AddSimpleKeyStructValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const KeyValueType& Key,
		const FIntProperty& ValueScoreProperty,
		const FStrProperty& ValueLabelProperty,
		int32 ValueScore,
		const FString& ValueLabel)
	{
		const KeyPropertyType* KeyProperty = CastField<const KeyPropertyType>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<Simple,FStruct> key should match expected FProperty type"), KeyProperty))
		{
			return false;
		}

		const FStructProperty* ValueProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<Simple,FStruct> value should be a struct property"), ValueProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<Simple,FStruct> value struct type should be available"), ValueProperty->Struct))
		{
			return false;
		}

		FStructOnScope ValueScope(ValueProperty->Struct);
		void* ValueStorage = ValueScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<Simple,FStruct> temporary value should allocate struct memory"), ValueStorage))
		{
			return false;
		}

		SetStructScoreLabelFields(ValueScoreProperty, ValueLabelProperty, ValueStorage, ValueScore, ValueLabel);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(&Key, ValueStorage);
		return true;
	}

	static bool AddObjectKeyStructValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		UObject* Key,
		const FIntProperty& ValueScoreProperty,
		const FStrProperty& ValueLabelProperty,
		int32 ValueScore,
		const FString& ValueLabel)
	{
		const FObjectProperty* KeyProperty = CastField<const FObjectProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<UObject,FStruct> key should be an object property"), KeyProperty))
		{
			return false;
		}

		const FStructProperty* ValueProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<UObject,FStruct> value should be a struct property"), ValueProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<UObject,FStruct> value struct type should be available"), ValueProperty->Struct))
		{
			return false;
		}

		FStructOnScope ValueScope(ValueProperty->Struct);
		void* ValueStorage = ValueScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<UObject,FStruct> temporary value should allocate struct memory"), ValueStorage))
		{
			return false;
		}

		SetStructScoreLabelFields(ValueScoreProperty, ValueLabelProperty, ValueStorage, ValueScore, ValueLabel);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(&Key, ValueStorage);
		return true;
	}

	static bool AddStructKeyStringValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& KeyIDProperty,
		const FNameProperty& KeyTagProperty,
		int32 KeyID,
		FName KeyTag,
		const FString& Value)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FString> key should be a struct property"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FString> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FStrProperty* ValueProperty = CastField<const FStrProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FString> value should be a string property"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FString> temporary key should allocate struct memory"), KeyStorage))
		{
			return false;
		}

		SetStructItemFields(KeyIDProperty, KeyTagProperty, KeyStorage, KeyID, KeyTag);
		void* ValueStorage = FMemory_Alloca(ValueProperty->GetSize());
		ValueProperty->InitializeValue(ValueStorage);
		ON_SCOPE_EXIT
		{
			ValueProperty->DestroyValue(ValueStorage);
		};
		ValueProperty->SetPropertyValue(ValueStorage, Value);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(KeyStorage, ValueStorage);
		return true;
	}

	static bool AddStructKeyNameValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& KeyIDProperty,
		const FNameProperty& KeyTagProperty,
		int32 KeyID,
		FName KeyTag,
		FName Value)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FName> key should be a struct property"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FName> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FNameProperty* ValueProperty = CastField<const FNameProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FName> value should be a name property"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FName> temporary key should allocate struct memory"), KeyStorage))
		{
			return false;
		}

		SetStructItemFields(KeyIDProperty, KeyTagProperty, KeyStorage, KeyID, KeyTag);
		void* ValueStorage = FMemory_Alloca(ValueProperty->GetSize());
		ValueProperty->InitializeValue(ValueStorage);
		ON_SCOPE_EXIT
		{
			ValueProperty->DestroyValue(ValueStorage);
		};
		ValueProperty->SetPropertyValue(ValueStorage, Value);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(KeyStorage, ValueStorage);
		return true;
	}

	static bool AddStructKeyObjectValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& KeyIDProperty,
		const FNameProperty& KeyTagProperty,
		int32 KeyID,
		FName KeyTag,
		UObject* Value)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> key should be a struct property"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FObjectProperty* ValueProperty = CastField<const FObjectProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> value should be an object property"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> temporary key should allocate struct memory"), KeyStorage))
		{
			return false;
		}

		SetStructItemFields(KeyIDProperty, KeyTagProperty, KeyStorage, KeyID, KeyTag);
		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(KeyStorage, &Value);
		return true;
	}

	template <typename ValuePropertyType, typename ValueType>
	static bool AddStructKeySimpleValueToMap(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		void* MapAddress,
		const FIntProperty& KeyIDProperty,
		const FNameProperty& KeyTagProperty,
		int32 KeyID,
		FName KeyTag,
		const ValueType& Value)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> key should be a struct property"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const ValuePropertyType* ValueProperty = CastField<const ValuePropertyType>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> value should match expected FProperty type"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> temporary key should allocate struct memory"), KeyStorage))
		{
			return false;
		}

		SetStructItemFields(KeyIDProperty, KeyTagProperty, KeyStorage, KeyID, KeyTag);
		void* ValueStorage = FMemory_Alloca(ValueProperty->GetSize());
		ValueProperty->InitializeValue(ValueStorage);
		ON_SCOPE_EXIT
		{
			ValueProperty->DestroyValue(ValueStorage);
		};
		ValueProperty->SetPropertyValue(ValueStorage, Value);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		Helper.AddPair(KeyStorage, ValueStorage);
		return true;
	}

	static bool AddStructItemToSet(
		FAutomationTestBase& Test,
		const FSetProperty& SetProperty,
		void* SetAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag)
	{
		FScriptSetHelper Helper(&SetProperty, SetAddress);
		const int32 Index = Helper.AddDefaultValue_Invalid_NeedsRehash();
		if (!ExpectTrue(Test, TEXT("TSet<FStruct> added element index should be valid"), Helper.IsValidIndex(Index)))
		{
			return false;
		}
		void* ItemAddress = Helper.GetElementPtr(Index);
		if (!ExpectNotNull(Test, TEXT("TSet<FStruct> item should expose writable memory"), ItemAddress))
		{
			return false;
		}

		SetStructItemFields(IDProperty, TagProperty, ItemAddress, ID, Tag);
		Helper.Rehash();
		return true;
	}

	static bool GetArrayStructItem(
		FAutomationTestBase& Test,
		const FArrayProperty& ArrayProperty,
		const void* ArrayAddress,
		int32 Index,
		const void*& OutItemAddress)
	{
		FScriptArrayHelper Helper(&ArrayProperty, ArrayAddress);
		if (!ExpectTrue(Test, TEXT("TArray<FStruct> index should be valid"), Helper.IsValidIndex(Index)))
		{
			return false;
		}

		OutItemAddress = Helper.GetRawPtr(Index);
		return ExpectNotNull(Test, TEXT("TArray<FStruct> item should expose readable memory"), OutItemAddress);
	}

	static bool ExpectStructItemFields(
		FAutomationTestBase& Test,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		const void* ItemAddress,
		int32 ExpectedID,
		FName ExpectedTag,
		const TCHAR* ContextLabel)
	{
		bool bPassed = true;
		bPassed &= ExpectEqual(Test,
			*FString::Printf(TEXT("%s should preserve ID"), ContextLabel),
			IDProperty.GetPropertyValue_InContainer(ItemAddress),
			ExpectedID);
		bPassed &= ExpectEqual(Test,
			*FString::Printf(TEXT("%s should preserve Tag"), ContextLabel),
			TagProperty.GetPropertyValue_InContainer(ItemAddress),
			ExpectedTag);
		return bPassed;
	}

	static bool ExpectStructScoreLabelFields(
		FAutomationTestBase& Test,
		const FIntProperty& ScoreProperty,
		const FStrProperty& LabelProperty,
		const void* ItemAddress,
		int32 ExpectedScore,
		const FString& ExpectedLabel,
		const TCHAR* ContextLabel)
	{
		bool bPassed = true;
		bPassed &= ExpectEqual(Test,
			*FString::Printf(TEXT("%s should preserve Score"), ContextLabel),
			ScoreProperty.GetPropertyValue_InContainer(ItemAddress),
			ExpectedScore);
		bPassed &= ExpectEqual(Test,
			*FString::Printf(TEXT("%s should preserve Label"), ContextLabel),
			LabelProperty.GetPropertyValue_InContainer(ItemAddress),
			ExpectedLabel);
		return bPassed;
	}

	static bool GetMapStructValue(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		int32 Key,
		const FStructProperty*& OutStructProperty,
		const void*& OutValueAddress)
	{
		const FIntProperty* KeyProperty = CastField<const FIntProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<int,FStruct> key should reflect as FIntProperty"), KeyProperty))
		{
			return false;
		}

		OutStructProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<int,FStruct> value should reflect as FStructProperty"), OutStructProperty))
		{
			return false;
		}

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const int32 ActualKey = KeyProperty->GetPropertyValue(Helper.GetKeyPtr(SparseIndex));
			if (ActualKey == Key)
			{
				OutValueAddress = Helper.GetValuePtr(SparseIndex);
				return true;
			}
		}

		FailTest(Test, FString::Printf(TEXT("TMap<int,FStruct> should contain key %d"), Key));
		return false;
	}

	template <typename KeyPropertyType, typename KeyValueType>
	static bool GetSimpleKeyStructMapValue(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		const KeyValueType& Key,
		const FStructProperty*& OutStructProperty,
		const void*& OutValueAddress)
	{
		const KeyPropertyType* KeyProperty = CastField<const KeyPropertyType>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<Simple,FStruct> key should match expected FProperty type"), KeyProperty))
		{
			return false;
		}

		OutStructProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<Simple,FStruct> value should reflect as FStructProperty"), OutStructProperty))
		{
			return false;
		}

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const KeyValueType ActualKey = KeyProperty->GetPropertyValue(Helper.GetKeyPtr(SparseIndex));
			if (ActualKey == Key)
			{
				OutValueAddress = Helper.GetValuePtr(SparseIndex);
				return true;
			}
		}

		FailTest(Test, TEXT("TMap<Simple,FStruct> should contain the expected key"));
		return false;
	}

	static bool GetObjectKeyStructMapValue(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		UObject* Key,
		const FStructProperty*& OutStructProperty,
		const void*& OutValueAddress)
	{
		const FObjectProperty* KeyProperty = CastField<const FObjectProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<UObject,FStruct> key should reflect as FObjectProperty"), KeyProperty))
		{
			return false;
		}

		OutStructProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<UObject,FStruct> value should reflect as FStructProperty"), OutStructProperty))
		{
			return false;
		}

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->GetObjectPropertyValue(Helper.GetKeyPtr(SparseIndex)) == Key)
			{
				OutValueAddress = Helper.GetValuePtr(SparseIndex);
				return true;
			}
		}

		FailTest(Test, TEXT("TMap<UObject,FStruct> should contain the expected key"));
		return false;
	}

	static bool GetIntMapValueByStructKey(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag,
		int32& OutValue)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> key should reflect as FStructProperty"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FIntProperty* ValueProperty = CastField<const FIntProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> value should reflect as FIntProperty"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,int> lookup key should allocate struct memory"), KeyStorage))
		{
			return false;
		}
		SetStructItemFields(IDProperty, TagProperty, KeyStorage, ID, Tag);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->Identical(Helper.GetKeyPtr(SparseIndex), KeyStorage, PPF_None))
			{
				OutValue = ValueProperty->GetPropertyValue(Helper.GetValuePtr(SparseIndex));
				return true;
			}
		}

		FailTest(Test, FString::Printf(TEXT("TMap<FStruct,int> should contain key (%d, %s)"), ID, *Tag.ToString()));
		return false;
	}

	template <typename ValuePropertyType, typename ValueType>
	static bool GetSimpleMapValueByStructKey(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag,
		ValueType& OutValue)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> key should reflect as FStructProperty"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const ValuePropertyType* ValueProperty = CastField<const ValuePropertyType>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> value should match expected FProperty type"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,Simple> lookup key should allocate struct memory"), KeyStorage))
		{
			return false;
		}
		SetStructItemFields(IDProperty, TagProperty, KeyStorage, ID, Tag);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->Identical(Helper.GetKeyPtr(SparseIndex), KeyStorage, PPF_None))
			{
				OutValue = ValueProperty->GetPropertyValue(Helper.GetValuePtr(SparseIndex));
				return true;
			}
		}

		FailTest(Test, FString::Printf(TEXT("TMap<FStruct,Simple> should contain key (%d, %s)"), ID, *Tag.ToString()));
		return false;
	}

	static bool GetObjectMapValueByStructKey(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag,
		UObject*& OutValue)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> key should reflect as FStructProperty"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		const FObjectProperty* ValueProperty = CastField<const FObjectProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> value should reflect as FObjectProperty"), ValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,UObject> lookup key should allocate struct memory"), KeyStorage))
		{
			return false;
		}
		SetStructItemFields(IDProperty, TagProperty, KeyStorage, ID, Tag);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->Identical(Helper.GetKeyPtr(SparseIndex), KeyStorage, PPF_None))
			{
				OutValue = ValueProperty->GetObjectPropertyValue(Helper.GetValuePtr(SparseIndex));
				return true;
			}
		}

		FailTest(Test, FString::Printf(TEXT("TMap<FStruct,UObject> should contain key (%d, %s)"), ID, *Tag.ToString()));
		return false;
	}

	static bool GetStructMapValueByStructKey(
		FAutomationTestBase& Test,
		const FMapProperty& MapProperty,
		const void* MapAddress,
		const FIntProperty& KeyIDProperty,
		const FNameProperty& KeyTagProperty,
		int32 KeyID,
		FName KeyTag,
		const FStructProperty*& OutValueProperty,
		const void*& OutValueAddress)
	{
		const FStructProperty* KeyProperty = CastField<const FStructProperty>(MapProperty.KeyProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> key should reflect as FStructProperty"), KeyProperty))
		{
			return false;
		}
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> key struct type should be available"), KeyProperty->Struct))
		{
			return false;
		}

		OutValueProperty = CastField<const FStructProperty>(MapProperty.ValueProp);
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> value should reflect as FStructProperty"), OutValueProperty))
		{
			return false;
		}

		FStructOnScope KeyScope(KeyProperty->Struct);
		void* KeyStorage = KeyScope.GetStructMemory();
		if (!ExpectNotNull(Test, TEXT("TMap<FStruct,FStruct> lookup key should allocate struct memory"), KeyStorage))
		{
			return false;
		}
		SetStructItemFields(KeyIDProperty, KeyTagProperty, KeyStorage, KeyID, KeyTag);

		FScriptMapHelper Helper(&MapProperty, MapAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			if (KeyProperty->Identical(Helper.GetKeyPtr(SparseIndex), KeyStorage, PPF_None))
			{
				OutValueAddress = Helper.GetValuePtr(SparseIndex);
				return true;
			}
		}

		FailTest(Test, FString::Printf(TEXT("TMap<FStruct,FStruct> should contain key (%d, %s)"), KeyID, *KeyTag.ToString()));
		return false;
	}

	static bool SetContainsStructItem(
		const FSetProperty& SetProperty,
		const void* SetAddress,
		const FIntProperty& IDProperty,
		const FNameProperty& TagProperty,
		int32 ID,
		FName Tag)
	{
		FScriptSetHelper Helper(&SetProperty, SetAddress);
		for (int32 SparseIndex = 0; SparseIndex < Helper.GetMaxIndex(); ++SparseIndex)
		{
			if (!Helper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			const void* ItemAddress = Helper.GetElementPtr(SparseIndex);
			if (IDProperty.GetPropertyValue_InContainer(ItemAddress) == ID
				&& TagProperty.GetPropertyValue_InContainer(ItemAddress) == Tag)
			{
				return true;
			}
		}

		return false;
	}

	static bool ExpectCompileFailureWithDiagnostic(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName,
		const FString& Source,
		const TCHAR* Label,
		const TCHAR* Diagnostic)
	{
		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(Diagnostic);
		return CompileAndExpectFailure(
			Test,
			Engine,
			ModuleName,
			Source,
			Label,
			MakeArrayView(ExpectedDiagnostics));
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// -------------------------------------------------------------------------
	// Basic struct declarations: USTRUCT(), plain struct, nested struct
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructBasicDeclaration)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_BasicDecl"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructBasicDecl.as"),
			ASTEST_AS(R"AS(
			// USTRUCT() - minimal declaration
			USTRUCT()
			struct FSimpleStruct
			{
				UPROPERTY()
				int Value = 42;
			}

			// Plain struct without USTRUCT (script-only)
			struct FPlainStruct
			{
				int X = 10;
				int Y = 20;
			}

			// Nested struct
			USTRUCT()
			struct FNestedOuter
			{
				UPROPERTY()
				int OuterValue = 100;

				UPROPERTY()
				FSimpleStruct InnerStruct;
			}

			UCLASS()
			class ACoverageStructBasicActor : AActor
			{
				UPROPERTY()
				FSimpleStruct SimpleData;

				UPROPERTY()
				FNestedOuter NestedData;

				FPlainStruct PlainData;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					SimpleData.Value = 99;
					NestedData.OuterValue = 200;
					NestedData.InnerStruct.Value = 300;
					PlainData.X = 50;
					PlainData.Y = 75;
				}
			}
			)AS"),
			TEXT("ACoverageStructBasicActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct basic declaration actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct basic declaration actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify USTRUCT members
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SimpleData.Value"), 99, TEXT("FSimpleStruct.Value should be set"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.OuterValue"), 200, TEXT("FNestedOuter.OuterValue should be set"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.InnerStruct.Value"), 300, TEXT("Nested FSimpleStruct.Value should be set"));
	}

	// -------------------------------------------------------------------------
	// USTRUCT declaration and construction edges: empty USTRUCT, script-only struct,
	// explicit constructors, and true copy initialization.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructDeclarationAndConstructionEdgeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_DeclarationAndConstructionEdgeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructDeclarationAndConstructionEdgeMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FEmptyCoverageStruct
			{
			}

			USTRUCT(BlueprintType)
			struct FConstructedStruct
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				FString Label = "Default";

				FConstructedStruct()
				{
					Value = 7;
					Label = "DefaultCtor";
				}

				FConstructedStruct(int InValue, FString InLabel)
				{
					Value = InValue;
					Label = InLabel;
				}

				FConstructedStruct(const FConstructedStruct& Other)
				{
					Value = Other.Value + 100;
					Label = Other.Label + "_CopyCtor";
				}
			}

			struct FPlainConstructionStruct
			{
				int X = 2;
				int Y = 3;

				FPlainConstructionStruct()
				{
					X = 5;
					Y = 6;
				}

				FPlainConstructionStruct(int InX, int InY)
				{
					X = InX;
					Y = InY;
				}
			}

			delegate int FEmptyStructSignal(FEmptyCoverageStruct Payload);

			UCLASS()
			class ACoverageStructConstructionEdgeActor : AActor
			{
				UPROPERTY()
				FEmptyCoverageStruct EmptyProperty;

				UPROPERTY()
				FConstructedStruct DefaultConstructed;

				UPROPERTY()
				FConstructedStruct ExplicitConstructed;

				UPROPERTY()
				FConstructedStruct CopyInitialized;

				UPROPERTY()
				FConstructedStruct Assigned;

				UPROPERTY()
				int PlainDefaultSum = 0;

				UPROPERTY()
				int PlainExplicitSum = 0;

				UPROPERTY()
				int EmptyValueCallCount = 0;

				UPROPERTY()
				int EmptyDelegateCallCount = 0;

				UPROPERTY()
				int EmptyDelegateResult = 0;

				UPROPERTY()
				FEmptyStructSignal EmptySignal;

				UFUNCTION(BlueprintCallable)
				int AcceptEmptyValue(FEmptyCoverageStruct Payload)
				{
					EmptyValueCallCount += 1;
					return EmptyValueCallCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillEmptyOut(FEmptyCoverageStruct&out Payload)
				{
					EmptyValueCallCount += 10;
				}

				UFUNCTION(BlueprintCallable)
				FEmptyCoverageStruct ReturnEmpty()
				{
					EmptyValueCallCount += 100;
					FEmptyCoverageStruct Payload;
					return Payload;
				}

				UFUNCTION()
				int HandleEmpty(FEmptyCoverageStruct Payload)
				{
					EmptyDelegateCallCount += 1;
					return 77;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FConstructedStruct LocalDefault;
					DefaultConstructed = LocalDefault;

					ExplicitConstructed = FConstructedStruct(21, "Explicit");

					FConstructedStruct Source(31, "Source");
					FConstructedStruct Copy(Source);
					CopyInitialized = Copy;

					Assigned = Source;

					FPlainConstructionStruct PlainDefault;
					PlainDefaultSum = PlainDefault.X + PlainDefault.Y;

					FPlainConstructionStruct PlainExplicit(9, 10);
					PlainExplicitSum = PlainExplicit.X + PlainExplicit.Y;

					EmptySignal.BindUFunction(this, n"HandleEmpty");
					EmptyDelegateResult = EmptySignal.Execute(EmptyProperty);
				}
			}
			)AS"),
			TEXT("ACoverageStructConstructionEdgeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct declaration/construction edge actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* EmptyProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("EmptyProperty"));
		ASSERT_THAT(IsNotNull(EmptyProperty, TEXT("Empty USTRUCT property should reflect as FStructProperty")));
		if (EmptyProperty == nullptr || EmptyProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(EmptyProperty->Struct->GetCppStructOps(), TEXT("Empty USTRUCT should expose cpp struct ops")));
		ASSERT_THAT(IsTrue(EmptyProperty->Struct->GetStructureSize() > 0, TEXT("Empty USTRUCT should still have a non-zero reflected structure size")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct declaration/construction edge actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DefaultConstructed.Value"), 7,
			TEXT("Explicit default constructor should initialize int fields before assignment to UPROPERTY"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("DefaultConstructed.Label"), FString(TEXT("DefaultCtor")),
			TEXT("Explicit default constructor should initialize string fields before assignment to UPROPERTY"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ExplicitConstructed.Value"), 21,
			TEXT("Parameterized constructor should initialize int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ExplicitConstructed.Label"), FString(TEXT("Explicit")),
			TEXT("Parameterized constructor should initialize string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopyInitialized.Value"), 131,
			TEXT("Copy constructor should run for explicit copy initialization"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("CopyInitialized.Label"), FString(TEXT("Source_CopyCtor")),
			TEXT("Copy constructor should preserve and transform string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Assigned.Value"), 31,
			TEXT("Assignment should copy the source int without invoking the custom copy constructor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Assigned.Label"), FString(TEXT("Source")),
			TEXT("Assignment should copy the source string without invoking the custom copy constructor"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PlainDefaultSum"), 11,
			TEXT("Plain script-only struct default constructor should be externally observable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("PlainExplicitSum"), 19,
			TEXT("Plain script-only struct parameterized constructor should be externally observable"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EmptyDelegateCallCount"), 1,
			TEXT("Empty USTRUCT delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EmptyDelegateResult"), 77,
			TEXT("Empty USTRUCT delegate value parameter should return receiver result"))));

		FFunctionInvoker EmptyValueInvoker(*TestRunner, Actor, TEXT("AcceptEmptyValue"));
		ASSERT_THAT(IsTrue(EmptyValueInvoker.IsValid(), TEXT("AcceptEmptyValue should be invokable")));
		if (!EmptyValueInvoker.IsValid())
		{
			return;
		}
		FProperty* EmptyParamProperty = nullptr;
		void* EmptyParamSlot = nullptr;
		ASSERT_THAT(IsTrue(EmptyValueInvoker.AddParamSlot(EmptyParamProperty, EmptyParamSlot),
			TEXT("AcceptEmptyValue should expose an empty USTRUCT parameter slot")));
		FStructProperty* EmptyParamStructProperty = CastField<FStructProperty>(EmptyParamProperty);
		ASSERT_THAT(IsNotNull(EmptyParamStructProperty, TEXT("Empty USTRUCT value parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyParamSlot, TEXT("Empty USTRUCT value parameter slot should be writable")));
		if (EmptyParamStructProperty == nullptr || EmptyParamSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(EmptyProperty->Struct, EmptyParamStructProperty->Struct,
			TEXT("Empty USTRUCT property and value parameter should share the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(1, EmptyValueInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT value parameter should execute through reflected caller buffer")));

		FFunctionInvoker EmptyOutInvoker(*TestRunner, Actor, TEXT("FillEmptyOut"));
		ASSERT_THAT(IsTrue(EmptyOutInvoker.IsValid(), TEXT("FillEmptyOut should be invokable")));
		if (!EmptyOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(EmptyOutInvoker.AddParamSlot(EmptyParamProperty, EmptyParamSlot),
			TEXT("FillEmptyOut should expose an empty USTRUCT out parameter slot")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(EmptyParamProperty), TEXT("Empty USTRUCT out parameter should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyParamSlot, TEXT("Empty USTRUCT out parameter slot should be writable")));
		if (EmptyParamSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(EmptyOutInvoker.Call(), TEXT("Empty USTRUCT out parameter should execute through reflected caller buffer")));

		FFunctionInvoker EmptyReturnInvoker(*TestRunner, Actor, TEXT("ReturnEmpty"));
		ASSERT_THAT(IsTrue(EmptyReturnInvoker.IsValid(), TEXT("ReturnEmpty should be invokable")));
		if (!EmptyReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(EmptyReturnInvoker.Call(), TEXT("Empty USTRUCT return should execute through reflection")));
		UFunction* ReturnEmptyFunction = Actor->FindFunction(TEXT("ReturnEmpty"));
		ASSERT_THAT(IsNotNull(ReturnEmptyFunction, TEXT("ReturnEmpty should reflect as a UFunction")));
		if (ReturnEmptyFunction == nullptr)
		{
			return;
		}
		FStructProperty* ReturnEmptyProperty = CastField<FStructProperty>(ReturnEmptyFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ReturnEmptyProperty, TEXT("Empty USTRUCT return should reflect as FStructProperty")));
		if (ReturnEmptyProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(EmptyProperty->Struct, ReturnEmptyProperty->Struct,
			TEXT("Empty USTRUCT property and return value should share the same generated UScriptStruct")));
		void* ReturnSlot = ReturnEmptyProperty->ContainerPtrToValuePtr<void>(EmptyReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("Empty USTRUCT return slot should be readable")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("EmptyValueCallCount"), 111,
			TEXT("Empty USTRUCT value/out/return reflected calls should update script-side state"))));
	}

	// -------------------------------------------------------------------------
	// Empty USTRUCT containers: array/map/set member and reflected call shapes.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructEmptyContainerShapeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_EmptyContainerShapeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FEmptyContainerStruct
			{
				bool opEquals(const FEmptyContainerStruct& Other) const
				{
					return true;
				}

				uint32 Hash() const
				{
					return 17;
				}
			}

			UCLASS()
			class ACoverageEmptyStructContainerActor : AActor
			{
				UPROPERTY()
				TArray<FEmptyContainerStruct> EmptyArray;

				UPROPERTY()
				TMap<int, FEmptyContainerStruct> IntToEmpty;

				UPROPERTY()
				TMap<FEmptyContainerStruct, int> EmptyToInt;

				UPROPERTY()
				TMap<FEmptyContainerStruct, FEmptyContainerStruct> EmptyToEmpty;

				UPROPERTY()
				TSet<FEmptyContainerStruct> EmptySet;

				UPROPERTY()
				int ArrayValueCount = 0;

				UPROPERTY()
				int ArrayInCount = 0;

				UPROPERTY()
				TArray<FEmptyContainerStruct> ArrayInout;

				UPROPERTY()
				int MapValueCount = 0;

				UPROPERTY()
				int MapInCount = 0;

				UPROPERTY()
				TMap<int, FEmptyContainerStruct> MapInout;

				UPROPERTY()
				int StructKeyMapValueCount = 0;

				UPROPERTY()
				int StructKeyMapInCount = 0;

				UPROPERTY()
				TMap<FEmptyContainerStruct, int> StructKeyMapInout;

				UPROPERTY()
				TMap<FEmptyContainerStruct, int> StructKeyMapOut;

				UPROPERTY()
				int StructStructMapValueCount = 0;

				UPROPERTY()
				int StructStructMapInCount = 0;

				UPROPERTY()
				TMap<FEmptyContainerStruct, FEmptyContainerStruct> StructStructMapInout;

				UPROPERTY()
				TMap<FEmptyContainerStruct, FEmptyContainerStruct> StructStructMapOut;

				UPROPERTY()
				int SetValueCount = 0;

				UPROPERTY()
				int SetInCount = 0;

				UPROPERTY()
				TSet<FEmptyContainerStruct> SetInout;

				UPROPERTY()
				bool EmptySetDeduplicated = false;

				UPROPERTY()
				bool EmptyKeyMapOverwrote = false;

				UPROPERTY()
				bool EmptyKeyMapInFound = false;

				UPROPERTY()
				bool EmptyStructStructMapFound = false;

				UPROPERTY()
				bool EmptyStructStructMapInFound = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				FEmptyContainerStruct MakeEmpty()
				{
					FEmptyContainerStruct Item;
					return Item;
				}

				UFUNCTION(BlueprintCallable)
				int CountArrayValue(TArray<FEmptyContainerStruct> Items)
				{
					ArrayValueCount = Items.Num();
					return ArrayValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountArrayIn(const TArray<FEmptyContainerStruct>&in Items)
				{
					ArrayInCount = Items.Num();
					return ArrayInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillArrayOut(TArray<FEmptyContainerStruct>&out Items)
				{
					Items.Add(MakeEmpty());
					Items.Add(MakeEmpty());
				}

				UFUNCTION(BlueprintCallable)
				void MutateArrayInout(TArray<FEmptyContainerStruct>&inout Items)
				{
					Items.Add(MakeEmpty());
					ArrayInout = Items;
				}

				UFUNCTION(BlueprintCallable)
				TArray<FEmptyContainerStruct> ReturnArray()
				{
					TArray<FEmptyContainerStruct> Items;
					Items.Add(MakeEmpty());
					Items.Add(MakeEmpty());
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountMapValue(TMap<int, FEmptyContainerStruct> Items)
				{
					MapValueCount = Items.Num();
					return MapValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountMapIn(const TMap<int, FEmptyContainerStruct>&in Items)
				{
					MapInCount = Items.Num();
					return MapInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillMapOut(TMap<int, FEmptyContainerStruct>&out Items)
				{
					Items.Add(10, MakeEmpty());
					Items.Add(11, MakeEmpty());
				}

				UFUNCTION(BlueprintCallable)
				void MutateMapInout(TMap<int, FEmptyContainerStruct>&inout Items)
				{
					Items.Add(12, MakeEmpty());
					MapInout = Items;
				}

				UFUNCTION(BlueprintCallable)
				TMap<int, FEmptyContainerStruct> ReturnMap()
				{
					TMap<int, FEmptyContainerStruct> Items;
					Items.Add(20, MakeEmpty());
					Items.Add(21, MakeEmpty());
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountStructKeyMapValue(TMap<FEmptyContainerStruct, int> Items)
				{
					StructKeyMapValueCount = Items.Num();
					int Found = 0;
					EmptyKeyMapOverwrote = Items.Find(MakeEmpty(), Found) && Found == 200;
					return StructKeyMapValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructKeyMapIn(const TMap<FEmptyContainerStruct, int>&in Items)
				{
					StructKeyMapInCount = Items.Num();
					int Found = 0;
					EmptyKeyMapInFound = Items.Find(MakeEmpty(), Found) && Found == 220;
					return StructKeyMapInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructKeyMapOut(TMap<FEmptyContainerStruct, int>&out Items)
				{
					Items.Add(MakeEmpty(), 500);
					StructKeyMapOut = Items;
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructKeyMapInout(TMap<FEmptyContainerStruct, int>&inout Items)
				{
					Items.Add(MakeEmpty(), 300);
					StructKeyMapInout = Items;
				}

				UFUNCTION(BlueprintCallable)
				TMap<FEmptyContainerStruct, int> ReturnStructKeyMap()
				{
					TMap<FEmptyContainerStruct, int> Items;
					Items.Add(MakeEmpty(), 400);
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructStructMapValue(TMap<FEmptyContainerStruct, FEmptyContainerStruct> Items)
				{
					StructStructMapValueCount = Items.Num();
					FEmptyContainerStruct Found;
					EmptyStructStructMapFound = Items.Find(MakeEmpty(), Found);
					return StructStructMapValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructStructMapIn(const TMap<FEmptyContainerStruct, FEmptyContainerStruct>&in Items)
				{
					StructStructMapInCount = Items.Num();
					FEmptyContainerStruct Found;
					EmptyStructStructMapInFound = Items.Find(MakeEmpty(), Found);
					return StructStructMapInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructStructMapOut(TMap<FEmptyContainerStruct, FEmptyContainerStruct>&out Items)
				{
					Items.Add(MakeEmpty(), MakeEmpty());
					StructStructMapOut = Items;
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructStructMapInout(TMap<FEmptyContainerStruct, FEmptyContainerStruct>&inout Items)
				{
					Items.Add(MakeEmpty(), MakeEmpty());
					StructStructMapInout = Items;
				}

				UFUNCTION(BlueprintCallable)
				TMap<FEmptyContainerStruct, FEmptyContainerStruct> ReturnStructStructMap()
				{
					TMap<FEmptyContainerStruct, FEmptyContainerStruct> Items;
					Items.Add(MakeEmpty(), MakeEmpty());
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountSetValue(TSet<FEmptyContainerStruct> Items)
				{
					SetValueCount = Items.Num();
					EmptySetDeduplicated = Items.Num() == 1 && Items.Contains(MakeEmpty());
					return SetValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountSetIn(const TSet<FEmptyContainerStruct>&in Items)
				{
					SetInCount = Items.Num();
					return SetInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillSetOut(TSet<FEmptyContainerStruct>&out Items)
				{
					Items.Add(MakeEmpty());
				}

				UFUNCTION(BlueprintCallable)
				void MutateSetInout(TSet<FEmptyContainerStruct>&inout Items)
				{
					Items.Add(MakeEmpty());
					SetInout = Items;
				}

				UFUNCTION(BlueprintCallable)
				TSet<FEmptyContainerStruct> ReturnSet()
				{
					TSet<FEmptyContainerStruct> Items;
					Items.Add(MakeEmpty());
					return Items;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					EmptyArray.Add(MakeEmpty());
					EmptyArray.Add(MakeEmpty());

					IntToEmpty.Add(1, MakeEmpty());
					IntToEmpty.Add(2, MakeEmpty());

					EmptyToInt.Add(MakeEmpty(), 100);
					EmptyToInt.Add(MakeEmpty(), 200);

					EmptyToEmpty.Add(MakeEmpty(), MakeEmpty());
					EmptyToEmpty.Add(MakeEmpty(), MakeEmpty());

					EmptySet.Add(MakeEmpty());
					EmptySet.Add(MakeEmpty());
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructEmptyContainerShapeMatrix.as"),
			ScriptSource,
			TEXT("ACoverageEmptyStructContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("Empty USTRUCT container actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FArrayProperty* EmptyArrayProperty = FindFProperty<FArrayProperty>(ScriptClass, TEXT("EmptyArray"));
		FMapProperty* IntToEmptyProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("IntToEmpty"));
		FMapProperty* EmptyToIntProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("EmptyToInt"));
		FMapProperty* EmptyToEmptyProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("EmptyToEmpty"));
		FSetProperty* EmptySetProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("EmptySet"));
		ASSERT_THAT(IsNotNull(EmptyArrayProperty, TEXT("Empty USTRUCT TArray member should reflect")));
		ASSERT_THAT(IsNotNull(IntToEmptyProperty, TEXT("Empty USTRUCT TMap<int,FStruct> member should reflect")));
		ASSERT_THAT(IsNotNull(EmptyToIntProperty, TEXT("Empty USTRUCT TMap<FStruct,int> member should reflect")));
		ASSERT_THAT(IsNotNull(EmptyToEmptyProperty, TEXT("Empty USTRUCT TMap<FStruct,FStruct> member should reflect")));
		ASSERT_THAT(IsNotNull(EmptySetProperty, TEXT("Empty USTRUCT TSet member should reflect")));
		if (EmptyArrayProperty == nullptr || IntToEmptyProperty == nullptr
			|| EmptyToIntProperty == nullptr || EmptyToEmptyProperty == nullptr || EmptySetProperty == nullptr)
		{
			return;
		}

		FStructProperty* EmptyArrayInnerProperty = CastField<FStructProperty>(EmptyArrayProperty->Inner);
		FStructProperty* EmptyMapValueProperty = CastField<FStructProperty>(IntToEmptyProperty->ValueProp);
		FStructProperty* EmptyMapKeyProperty = CastField<FStructProperty>(EmptyToIntProperty->KeyProp);
		FStructProperty* EmptyStructMapKeyProperty = CastField<FStructProperty>(EmptyToEmptyProperty->KeyProp);
		FStructProperty* EmptyStructMapValueProperty = CastField<FStructProperty>(EmptyToEmptyProperty->ValueProp);
		FStructProperty* EmptySetElementProperty = CastField<FStructProperty>(EmptySetProperty->ElementProp);
		ASSERT_THAT(IsNotNull(EmptyArrayInnerProperty, TEXT("Empty USTRUCT TArray inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyMapValueProperty, TEXT("Empty USTRUCT map value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyMapKeyProperty, TEXT("Empty USTRUCT map key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyStructMapKeyProperty, TEXT("Empty USTRUCT struct-map key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyStructMapValueProperty, TEXT("Empty USTRUCT struct-map value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptySetElementProperty, TEXT("Empty USTRUCT set element should be FStructProperty")));
		if (EmptyArrayInnerProperty == nullptr || EmptyMapValueProperty == nullptr
			|| EmptyMapKeyProperty == nullptr || EmptyStructMapKeyProperty == nullptr
			|| EmptyStructMapValueProperty == nullptr || EmptySetElementProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(EmptyArrayInnerProperty->Struct, EmptyMapValueProperty->Struct,
			TEXT("Empty USTRUCT array and map value should share generated UScriptStruct")));
		ASSERT_THAT(AreEqual(EmptyArrayInnerProperty->Struct, EmptyMapKeyProperty->Struct,
			TEXT("Empty USTRUCT array element and map key should share generated UScriptStruct")));
		ASSERT_THAT(AreEqual(EmptyArrayInnerProperty->Struct, EmptyStructMapKeyProperty->Struct,
			TEXT("Empty USTRUCT array element and struct-map key should share generated UScriptStruct")));
		ASSERT_THAT(AreEqual(EmptyArrayInnerProperty->Struct, EmptyStructMapValueProperty->Struct,
			TEXT("Empty USTRUCT array element and struct-map value should share generated UScriptStruct")));
		ASSERT_THAT(AreEqual(EmptyArrayInnerProperty->Struct, EmptySetElementProperty->Struct,
			TEXT("Empty USTRUCT array element and set element should share generated UScriptStruct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("Empty USTRUCT container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("EmptyArray"), Count),
			TEXT("Empty USTRUCT array member should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("Empty USTRUCT array member should preserve duplicate value elements")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IntToEmpty"), Count),
			TEXT("Empty USTRUCT map value member should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("Empty USTRUCT map value member should preserve entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("EmptyToInt"), Count),
			TEXT("Empty USTRUCT map key member should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT map key member should overwrite equivalent empty keys")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("EmptyToEmpty"), Count),
			TEXT("Empty USTRUCT struct-to-struct map member should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT struct-to-struct map member should overwrite equivalent empty keys")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("EmptySet"), Count),
			TEXT("Empty USTRUCT set member should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT set member should deduplicate equivalent empty elements")));

		FFunctionInvoker ArrayValueInvoker(*TestRunner, Actor, TEXT("CountArrayValue"));
		ASSERT_THAT(IsTrue(ArrayValueInvoker.IsValid(), TEXT("CountArrayValue should be invokable")));
		if (!ArrayValueInvoker.IsValid())
		{
			return;
		}

		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(ArrayValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountArrayValue should expose empty USTRUCT array parameter slot")));
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("Empty USTRUCT TArray parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		FScriptArrayHelper ArrayValueHelper(ArrayProperty, ParamSlot);
		ArrayValueHelper.AddValue();
		ArrayValueHelper.AddValue();
		ASSERT_THAT(AreEqual(2, ArrayValueInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT array value parameter should count caller entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayValueCount"), 2,
			TEXT("Empty USTRUCT array value parameter should update script-side state"))));

		FFunctionInvoker ArrayInInvoker(*TestRunner, Actor, TEXT("CountArrayIn"));
		ASSERT_THAT(IsTrue(ArrayInInvoker.IsValid(), TEXT("CountArrayIn should be invokable")));
		if (!ArrayInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountArrayIn should expose empty USTRUCT array const-ref parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("Empty USTRUCT TArray const-ref parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		FScriptArrayHelper ArrayInHelper(ArrayProperty, ParamSlot);
		ArrayInHelper.AddValue();
		ASSERT_THAT(AreEqual(1, ArrayInInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT array const-ref parameter should count caller entries")));

		FFunctionInvoker ArrayOutInvoker(*TestRunner, Actor, TEXT("FillArrayOut"));
		ASSERT_THAT(IsTrue(ArrayOutInvoker.IsValid(), TEXT("FillArrayOut should be invokable")));
		if (!ArrayOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillArrayOut should expose empty USTRUCT array out parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("Empty USTRUCT TArray out parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayOutInvoker.Call(), TEXT("Empty USTRUCT array out parameter should execute")));
		FScriptArrayHelper ArrayOutHelper(ArrayProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, ArrayOutHelper.Num(), TEXT("Empty USTRUCT array out parameter should write entries")));

		FFunctionInvoker ArrayInoutInvoker(*TestRunner, Actor, TEXT("MutateArrayInout"));
		ASSERT_THAT(IsTrue(ArrayInoutInvoker.IsValid(), TEXT("MutateArrayInout should be invokable")));
		if (!ArrayInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateArrayInout should expose empty USTRUCT array inout parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("Empty USTRUCT TArray inout parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		FScriptArrayHelper ArrayInoutHelper(ArrayProperty, ParamSlot);
		ArrayInoutHelper.AddValue();
		ASSERT_THAT(IsTrue(ArrayInoutInvoker.Call(), TEXT("Empty USTRUCT array inout parameter should execute")));
		ASSERT_THAT(AreEqual(2, ArrayInoutHelper.Num(), TEXT("Empty USTRUCT array inout parameter should append one entry")));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayInout"), Count),
			TEXT("Empty USTRUCT array inout script storage should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("Empty USTRUCT array inout script storage should preserve entries")));

		FFunctionInvoker ArrayReturnInvoker(*TestRunner, Actor, TEXT("ReturnArray"));
		ASSERT_THAT(IsTrue(ArrayReturnInvoker.IsValid(), TEXT("ReturnArray should be invokable")));
		if (!ArrayReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayReturnInvoker.Call(), TEXT("Empty USTRUCT array return should execute")));
		UFunction* ReturnArrayFunction = Actor->FindFunction(TEXT("ReturnArray"));
		ASSERT_THAT(IsNotNull(ReturnArrayFunction, TEXT("ReturnArray should reflect as a UFunction")));
		if (ReturnArrayFunction == nullptr)
		{
			return;
		}
		FArrayProperty* ArrayReturnProperty = CastField<FArrayProperty>(ReturnArrayFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ArrayReturnProperty, TEXT("Empty USTRUCT array return should reflect as FArrayProperty")));
		if (ArrayReturnProperty == nullptr)
		{
			return;
		}
		void* ReturnSlot = ArrayReturnProperty->ContainerPtrToValuePtr<void>(ArrayReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("Empty USTRUCT array return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptArrayHelper ArrayReturnHelper(ArrayReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ArrayReturnHelper.Num(), TEXT("Empty USTRUCT array return should contain entries")));

		FFunctionInvoker MapValueInvoker(*TestRunner, Actor, TEXT("CountMapValue"));
		ASSERT_THAT(IsTrue(MapValueInvoker.IsValid(), TEXT("CountMapValue should be invokable")));
		if (!MapValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountMapValue should expose empty USTRUCT map value parameter slot")));
		FMapProperty* MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<int,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper MapValueHelper(MapProperty, ParamSlot);
		FStructOnScope EmptyValueScope(EmptyMapValueProperty->Struct);
		void* EmptyValueMemory = EmptyValueScope.GetStructMemory();
		ASSERT_THAT(IsNotNull(EmptyValueMemory, TEXT("Empty USTRUCT map value temporary should allocate memory")));
		if (EmptyValueMemory == nullptr)
		{
			return;
		}
		int32 MapKey = 1;
		MapValueHelper.AddPair(&MapKey, EmptyValueMemory);
		MapKey = 2;
		MapValueHelper.AddPair(&MapKey, EmptyValueMemory);
		ASSERT_THAT(AreEqual(2, MapValueInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT map value parameter should count caller entries")));

		FFunctionInvoker MapInInvoker(*TestRunner, Actor, TEXT("CountMapIn"));
		ASSERT_THAT(IsTrue(MapInInvoker.IsValid(), TEXT("CountMapIn should be invokable")));
		if (!MapInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountMapIn should expose empty USTRUCT map const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<int,FStruct> const-ref parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper MapInHelper(MapProperty, ParamSlot);
		MapKey = 3;
		MapInHelper.AddPair(&MapKey, EmptyValueMemory);
		ASSERT_THAT(AreEqual(1, MapInInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT map const-ref parameter should count caller entries")));

		FFunctionInvoker MapOutInvoker(*TestRunner, Actor, TEXT("FillMapOut"));
		ASSERT_THAT(IsTrue(MapOutInvoker.IsValid(), TEXT("FillMapOut should be invokable")));
		if (!MapOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillMapOut should expose empty USTRUCT map out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<int,FStruct> out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.Call(), TEXT("Empty USTRUCT map out parameter should execute")));
		FScriptMapHelper MapOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapOutHelper.Num(), TEXT("Empty USTRUCT map out parameter should write entries")));

		FFunctionInvoker MapInoutInvoker(*TestRunner, Actor, TEXT("MutateMapInout"));
		ASSERT_THAT(IsTrue(MapInoutInvoker.IsValid(), TEXT("MutateMapInout should be invokable")));
		if (!MapInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateMapInout should expose empty USTRUCT map inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<int,FStruct> inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper MapInoutHelper(MapProperty, ParamSlot);
		MapKey = 4;
		MapInoutHelper.AddPair(&MapKey, EmptyValueMemory);
		ASSERT_THAT(IsTrue(MapInoutInvoker.Call(), TEXT("Empty USTRUCT map inout parameter should execute")));
		ASSERT_THAT(AreEqual(2, MapInoutHelper.Num(), TEXT("Empty USTRUCT map inout parameter should add one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapInout"), Count),
			TEXT("Empty USTRUCT map inout script storage should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("Empty USTRUCT map inout script storage should preserve entries")));

		FFunctionInvoker MapReturnInvoker(*TestRunner, Actor, TEXT("ReturnMap"));
		ASSERT_THAT(IsTrue(MapReturnInvoker.IsValid(), TEXT("ReturnMap should be invokable")));
		if (!MapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapReturnInvoker.Call(), TEXT("Empty USTRUCT map return should execute")));
		UFunction* ReturnMapFunction = Actor->FindFunction(TEXT("ReturnMap"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnMap should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("Empty USTRUCT map return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(MapReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("Empty USTRUCT map return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper MapReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, MapReturnHelper.Num(), TEXT("Empty USTRUCT map return should contain entries")));

		FFunctionInvoker StructKeyMapValueInvoker(*TestRunner, Actor, TEXT("CountStructKeyMapValue"));
		ASSERT_THAT(IsTrue(StructKeyMapValueInvoker.IsValid(), TEXT("CountStructKeyMapValue should be invokable")));
		if (!StructKeyMapValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructKeyMapValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructKeyMapValue should expose empty USTRUCT map key parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,int> parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper StructKeyMapValueHelper(MapProperty, ParamSlot);
		FStructOnScope EmptyKeyScope(EmptyMapKeyProperty->Struct);
		void* EmptyKeyMemory = EmptyKeyScope.GetStructMemory();
		ASSERT_THAT(IsNotNull(EmptyKeyMemory, TEXT("Empty USTRUCT map key temporary should allocate memory")));
		if (EmptyKeyMemory == nullptr)
		{
			return;
		}
		int32 StructKeyMapValue = 100;
		StructKeyMapValueHelper.AddPair(EmptyKeyMemory, &StructKeyMapValue);
		StructKeyMapValue = 200;
		StructKeyMapValueHelper.AddPair(EmptyKeyMemory, &StructKeyMapValue);
		ASSERT_THAT(AreEqual(1, StructKeyMapValueInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT map key value parameter should deduplicate equivalent keys")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EmptyKeyMapOverwrote"), true,
			TEXT("Empty USTRUCT map key value parameter should overwrite equivalent keys"))));

		FFunctionInvoker StructKeyMapInInvoker(*TestRunner, Actor, TEXT("CountStructKeyMapIn"));
		ASSERT_THAT(IsTrue(StructKeyMapInInvoker.IsValid(), TEXT("CountStructKeyMapIn should be invokable")));
		if (!StructKeyMapInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructKeyMapInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructKeyMapIn should expose empty USTRUCT map key const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,int> const-ref parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper StructKeyMapInHelper(MapProperty, ParamSlot);
		StructKeyMapValue = 220;
		StructKeyMapInHelper.AddPair(EmptyKeyMemory, &StructKeyMapValue);
		ASSERT_THAT(AreEqual(1, StructKeyMapInInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT map key const-ref parameter should count caller entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructKeyMapInCount"), 1,
			TEXT("Empty USTRUCT map key const-ref parameter should update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EmptyKeyMapInFound"), true,
			TEXT("Empty USTRUCT map key const-ref parameter should preserve equivalent-key lookup"))));

		FFunctionInvoker StructKeyMapOutInvoker(*TestRunner, Actor, TEXT("FillStructKeyMapOut"));
		ASSERT_THAT(IsTrue(StructKeyMapOutInvoker.IsValid(), TEXT("FillStructKeyMapOut should be invokable")));
		if (!StructKeyMapOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructKeyMapOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructKeyMapOut should expose empty USTRUCT map key out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,int> out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructKeyMapOutInvoker.Call(), TEXT("Empty USTRUCT map key out parameter should execute")));
		FScriptMapHelper StructKeyMapOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(1, StructKeyMapOutHelper.Num(), TEXT("Empty USTRUCT map key out parameter should write one equivalent key")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructKeyMapOut"), Count),
			TEXT("Empty USTRUCT map key out script storage should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT map key out script storage should preserve one entry")));

		FFunctionInvoker StructKeyMapInoutInvoker(*TestRunner, Actor, TEXT("MutateStructKeyMapInout"));
		ASSERT_THAT(IsTrue(StructKeyMapInoutInvoker.IsValid(), TEXT("MutateStructKeyMapInout should be invokable")));
		if (!StructKeyMapInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructKeyMapInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructKeyMapInout should expose empty USTRUCT map key inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,int> inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper StructKeyMapInoutHelper(MapProperty, ParamSlot);
		StructKeyMapValue = 250;
		StructKeyMapInoutHelper.AddPair(EmptyKeyMemory, &StructKeyMapValue);
		ASSERT_THAT(IsTrue(StructKeyMapInoutInvoker.Call(), TEXT("Empty USTRUCT map key inout parameter should execute")));
		ASSERT_THAT(AreEqual(1, StructKeyMapInoutHelper.Num(), TEXT("Empty USTRUCT map key inout parameter should retain one equivalent key")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructKeyMapInout"), Count),
			TEXT("Empty USTRUCT map key inout script storage should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT map key inout script storage should preserve one entry")));

		FFunctionInvoker StructKeyMapReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructKeyMap"));
		ASSERT_THAT(IsTrue(StructKeyMapReturnInvoker.IsValid(), TEXT("ReturnStructKeyMap should be invokable")));
		if (!StructKeyMapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructKeyMapReturnInvoker.Call(), TEXT("Empty USTRUCT map key return should execute")));
		UFunction* ReturnStructKeyMapFunction = Actor->FindFunction(TEXT("ReturnStructKeyMap"));
		ASSERT_THAT(IsNotNull(ReturnStructKeyMapFunction, TEXT("ReturnStructKeyMap should reflect as a UFunction")));
		if (ReturnStructKeyMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* StructKeyMapReturnProperty = CastField<FMapProperty>(ReturnStructKeyMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(StructKeyMapReturnProperty, TEXT("Empty USTRUCT map key return should reflect as FMapProperty")));
		if (StructKeyMapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = StructKeyMapReturnProperty->ContainerPtrToValuePtr<void>(StructKeyMapReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("Empty USTRUCT map key return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructKeyMapReturnHelper(StructKeyMapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(1, StructKeyMapReturnHelper.Num(), TEXT("Empty USTRUCT map key return should contain one equivalent key")));

		FFunctionInvoker StructStructMapValueInvoker(*TestRunner, Actor, TEXT("CountStructStructMapValue"));
		ASSERT_THAT(IsTrue(StructStructMapValueInvoker.IsValid(), TEXT("CountStructStructMapValue should be invokable")));
		if (!StructStructMapValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStructMapValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructStructMapValue should expose empty USTRUCT struct-to-struct map parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper StructStructMapValueHelper(MapProperty, ParamSlot);
		StructStructMapValueHelper.AddPair(EmptyKeyMemory, EmptyValueMemory);
		ASSERT_THAT(AreEqual(1, StructStructMapValueInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT struct-to-struct map value parameter should count caller entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EmptyStructStructMapFound"), true,
			TEXT("Empty USTRUCT struct-to-struct map value parameter should find equivalent keys"))));

		FFunctionInvoker StructStructMapInInvoker(*TestRunner, Actor, TEXT("CountStructStructMapIn"));
		ASSERT_THAT(IsTrue(StructStructMapInInvoker.IsValid(), TEXT("CountStructStructMapIn should be invokable")));
		if (!StructStructMapInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStructMapInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructStructMapIn should expose empty USTRUCT struct-to-struct map const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,FStruct> const-ref parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper StructStructMapInHelper(MapProperty, ParamSlot);
		StructStructMapInHelper.AddPair(EmptyKeyMemory, EmptyValueMemory);
		ASSERT_THAT(AreEqual(1, StructStructMapInInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT struct-to-struct map const-ref parameter should count caller entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStructMapInCount"), 1,
			TEXT("Empty USTRUCT struct-to-struct map const-ref parameter should update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EmptyStructStructMapInFound"), true,
			TEXT("Empty USTRUCT struct-to-struct map const-ref parameter should preserve equivalent-key lookup"))));

		FFunctionInvoker StructStructMapOutInvoker(*TestRunner, Actor, TEXT("FillStructStructMapOut"));
		ASSERT_THAT(IsTrue(StructStructMapOutInvoker.IsValid(), TEXT("FillStructStructMapOut should be invokable")));
		if (!StructStructMapOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStructMapOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructStructMapOut should expose empty USTRUCT struct-to-struct map out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,FStruct> out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStructMapOutInvoker.Call(), TEXT("Empty USTRUCT struct-to-struct map out parameter should execute")));
		FScriptMapHelper StructStructMapOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(1, StructStructMapOutHelper.Num(), TEXT("Empty USTRUCT struct-to-struct map out parameter should write one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructStructMapOut"), Count),
			TEXT("Empty USTRUCT struct-to-struct map out script storage should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT struct-to-struct map out script storage should preserve one entry")));

		FFunctionInvoker StructStructMapInoutInvoker(*TestRunner, Actor, TEXT("MutateStructStructMapInout"));
		ASSERT_THAT(IsTrue(StructStructMapInoutInvoker.IsValid(), TEXT("MutateStructStructMapInout should be invokable")));
		if (!StructStructMapInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStructMapInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructStructMapInout should expose empty USTRUCT struct-to-struct map inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("Empty USTRUCT TMap<FStruct,FStruct> inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		FScriptMapHelper StructStructMapInoutHelper(MapProperty, ParamSlot);
		StructStructMapInoutHelper.AddPair(EmptyKeyMemory, EmptyValueMemory);
		ASSERT_THAT(IsTrue(StructStructMapInoutInvoker.Call(), TEXT("Empty USTRUCT struct-to-struct map inout parameter should execute")));
		ASSERT_THAT(AreEqual(1, StructStructMapInoutHelper.Num(), TEXT("Empty USTRUCT struct-to-struct map inout parameter should retain one equivalent key")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructStructMapInout"), Count),
			TEXT("Empty USTRUCT struct-to-struct map inout script storage should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT struct-to-struct map inout script storage should preserve one entry")));

		FFunctionInvoker StructStructMapReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructStructMap"));
		ASSERT_THAT(IsTrue(StructStructMapReturnInvoker.IsValid(), TEXT("ReturnStructStructMap should be invokable")));
		if (!StructStructMapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStructMapReturnInvoker.Call(), TEXT("Empty USTRUCT struct-to-struct map return should execute")));
		UFunction* ReturnStructStructMapFunction = Actor->FindFunction(TEXT("ReturnStructStructMap"));
		ASSERT_THAT(IsNotNull(ReturnStructStructMapFunction, TEXT("ReturnStructStructMap should reflect as a UFunction")));
		if (ReturnStructStructMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* StructStructMapReturnProperty = CastField<FMapProperty>(ReturnStructStructMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(StructStructMapReturnProperty, TEXT("Empty USTRUCT struct-to-struct map return should reflect as FMapProperty")));
		if (StructStructMapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = StructStructMapReturnProperty->ContainerPtrToValuePtr<void>(StructStructMapReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("Empty USTRUCT struct-to-struct map return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructStructMapReturnHelper(StructStructMapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(1, StructStructMapReturnHelper.Num(), TEXT("Empty USTRUCT struct-to-struct map return should contain one equivalent key")));

		FFunctionInvoker SetValueInvoker(*TestRunner, Actor, TEXT("CountSetValue"));
		ASSERT_THAT(IsTrue(SetValueInvoker.IsValid(), TEXT("CountSetValue should be invokable")));
		if (!SetValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountSetValue should expose empty USTRUCT set parameter slot")));
		FSetProperty* SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("Empty USTRUCT TSet value parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		FScriptSetHelper SetValueHelper(SetProperty, ParamSlot);
		SetValueHelper.AddDefaultValue_Invalid_NeedsRehash();
		SetValueHelper.Rehash();
		ASSERT_THAT(AreEqual(1, SetValueInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT set value parameter should count caller entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("EmptySetDeduplicated"), true,
			TEXT("Empty USTRUCT set value parameter should support Contains on equivalent elements"))));

		FFunctionInvoker SetInInvoker(*TestRunner, Actor, TEXT("CountSetIn"));
		ASSERT_THAT(IsTrue(SetInInvoker.IsValid(), TEXT("CountSetIn should be invokable")));
		if (!SetInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountSetIn should expose empty USTRUCT set const-ref parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("Empty USTRUCT TSet const-ref parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		FScriptSetHelper SetInHelper(SetProperty, ParamSlot);
		SetInHelper.AddDefaultValue_Invalid_NeedsRehash();
		SetInHelper.Rehash();
		ASSERT_THAT(AreEqual(1, SetInInvoker.CallAndReturn<int32>(0),
			TEXT("Empty USTRUCT set const-ref parameter should count caller entries")));

		FFunctionInvoker SetOutInvoker(*TestRunner, Actor, TEXT("FillSetOut"));
		ASSERT_THAT(IsTrue(SetOutInvoker.IsValid(), TEXT("FillSetOut should be invokable")));
		if (!SetOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillSetOut should expose empty USTRUCT set out parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("Empty USTRUCT TSet out parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetOutInvoker.Call(), TEXT("Empty USTRUCT set out parameter should execute")));
		FScriptSetHelper SetOutHelper(SetProperty, ParamSlot);
		ASSERT_THAT(AreEqual(1, SetOutHelper.Num(), TEXT("Empty USTRUCT set out parameter should write one element")));

		FFunctionInvoker SetInoutInvoker(*TestRunner, Actor, TEXT("MutateSetInout"));
		ASSERT_THAT(IsTrue(SetInoutInvoker.IsValid(), TEXT("MutateSetInout should be invokable")));
		if (!SetInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateSetInout should expose empty USTRUCT set inout parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("Empty USTRUCT TSet inout parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		FScriptSetHelper SetInoutHelper(SetProperty, ParamSlot);
		SetInoutHelper.AddDefaultValue_Invalid_NeedsRehash();
		SetInoutHelper.Rehash();
		ASSERT_THAT(IsTrue(SetInoutInvoker.Call(), TEXT("Empty USTRUCT set inout parameter should execute")));
		ASSERT_THAT(AreEqual(1, SetInoutHelper.Num(), TEXT("Empty USTRUCT set inout parameter should retain one equivalent element")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetInout"), Count),
			TEXT("Empty USTRUCT set inout script storage should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("Empty USTRUCT set inout script storage should preserve one element")));

		FFunctionInvoker SetReturnInvoker(*TestRunner, Actor, TEXT("ReturnSet"));
		ASSERT_THAT(IsTrue(SetReturnInvoker.IsValid(), TEXT("ReturnSet should be invokable")));
		if (!SetReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetReturnInvoker.Call(), TEXT("Empty USTRUCT set return should execute")));
		UFunction* ReturnSetFunction = Actor->FindFunction(TEXT("ReturnSet"));
		ASSERT_THAT(IsNotNull(ReturnSetFunction, TEXT("ReturnSet should reflect as a UFunction")));
		if (ReturnSetFunction == nullptr)
		{
			return;
		}
		FSetProperty* SetReturnProperty = CastField<FSetProperty>(ReturnSetFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(SetReturnProperty, TEXT("Empty USTRUCT set return should reflect as FSetProperty")));
		if (SetReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = SetReturnProperty->ContainerPtrToValuePtr<void>(SetReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("Empty USTRUCT set return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptSetHelper SetReturnHelper(SetReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(1, SetReturnHelper.Num(), TEXT("Empty USTRUCT set return should contain one element")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT namespace handling: namespaced struct reflected through properties
	// and function parameter/return slots.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructNamespacedDeclarationAndReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_NamespacedDeclarationAndReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructNamespacedDeclarationAndReflection.as"),
			ASTEST_AS(R"AS(
			namespace CoverageStructNS
			{
				USTRUCT(BlueprintType)
				struct FNamespacedStruct
				{
					UPROPERTY()
					int Count = 0;

					UPROPERTY()
					FString Label;
				}
			}

			UCLASS()
			class ACoverageStructNamespacedActor : AActor
			{
				UPROPERTY()
				CoverageStructNS::FNamespacedStruct Data;

				UPROPERTY()
				CoverageStructNS::FNamespacedStruct LastAccepted;

				UFUNCTION(BlueprintCallable)
				void Accept(CoverageStructNS::FNamespacedStruct Payload)
				{
					LastAccepted = Payload;
				}

				UFUNCTION(BlueprintCallable)
				CoverageStructNS::FNamespacedStruct MakePayload(int InCount, const FString&in InLabel)
				{
					CoverageStructNS::FNamespacedStruct Result;
					Result.Count = InCount;
					Result.Label = InLabel;
					return Result;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data = MakePayload(31, "Namespaced");
					Accept(Data);
				}
			}
			)AS"),
			TEXT("ACoverageStructNamespacedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct namespaced actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		FStructProperty* LastAcceptedProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("LastAccepted"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Namespaced USTRUCT property should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(LastAcceptedProperty, TEXT("Second namespaced USTRUCT property should reflect as FStructProperty")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr || LastAcceptedProperty == nullptr || LastAcceptedProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(DataProperty->Struct, LastAcceptedProperty->Struct,
			TEXT("Namespaced USTRUCT properties should share the same generated UScriptStruct")));

		UFunction* AcceptFunction = ScriptClass->FindFunctionByName(TEXT("Accept"));
		UFunction* MakePayloadFunction = ScriptClass->FindFunctionByName(TEXT("MakePayload"));
		ASSERT_THAT(IsNotNull(AcceptFunction, TEXT("Namespaced USTRUCT parameter function should reflect")));
		ASSERT_THAT(IsNotNull(MakePayloadFunction, TEXT("Namespaced USTRUCT return function should reflect")));
		if (AcceptFunction == nullptr || MakePayloadFunction == nullptr)
		{
			return;
		}

		FStructProperty* AcceptPayloadProperty = FindFProperty<FStructProperty>(AcceptFunction, TEXT("Payload"));
		FStructProperty* ReturnPayloadProperty = CastField<FStructProperty>(MakePayloadFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(AcceptPayloadProperty, TEXT("Namespaced USTRUCT UFUNCTION parameter should reflect")));
		ASSERT_THAT(IsNotNull(ReturnPayloadProperty, TEXT("Namespaced USTRUCT UFUNCTION return should reflect")));
		if (AcceptPayloadProperty == nullptr || AcceptPayloadProperty->Struct == nullptr
			|| ReturnPayloadProperty == nullptr || ReturnPayloadProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(DataProperty->Struct, AcceptPayloadProperty->Struct,
			TEXT("Namespaced USTRUCT property and parameter should share generated type identity")));
		ASSERT_THAT(AreEqual(DataProperty->Struct, ReturnPayloadProperty->Struct,
			TEXT("Namespaced USTRUCT property and return value should share generated type identity")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct namespaced actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.Count"), 31,
			TEXT("Namespaced USTRUCT return should initialize int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.Label"), FString(TEXT("Namespaced")),
			TEXT("Namespaced USTRUCT return should initialize string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastAccepted.Count"), 31,
			TEXT("Namespaced USTRUCT parameter should copy int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastAccepted.Label"), FString(TEXT("Namespaced")),
			TEXT("Namespaced USTRUCT parameter should copy string fields"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT specifiers: BlueprintType
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Specifiers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructSpecifiers.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FBlueprintTypeStruct
			{
				UPROPERTY()
				int Value = 10;
			}

			UCLASS()
			class ACoverageStructSpecifierActor : AActor
			{
				UPROPERTY()
				FBlueprintTypeStruct BPData;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BPData.Value = 100;
				}
			}
			)AS"),
			TEXT("ACoverageStructSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* BPDataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("BPData"));
		ASSERT_THAT(IsNotNull(BPDataProperty, TEXT("BlueprintType struct property should reflect as FStructProperty")));
		if (BPDataProperty == nullptr || BPDataProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("true")), BPDataProperty->Struct->GetMetaData(TEXT("BlueprintType")),
			TEXT("USTRUCT(BlueprintType) should persist BlueprintType metadata on the generated UScriptStruct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BPData.Value"), 100, TEXT("BlueprintType struct should work"));
	}

	// -------------------------------------------------------------------------
	// USTRUCT Blueprint boundary: inherited BlueprintType struct property on a
	// transient Blueprint child keeps CDO defaults and runtime values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructBlueprintGeneratedClassBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_BlueprintGeneratedClassBoundary"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructBlueprintGeneratedClassBoundary.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FBlueprintBoundaryStruct
			{
				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				int Count = 23;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FString Label = "StructDefault";
			}

			UCLASS()
			class ACoverageStructBlueprintBoundaryActor : AActor
			{
				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FBlueprintBoundaryStruct Payload;

				UPROPERTY()
				FBlueprintBoundaryStruct RuntimeCopy;

				UPROPERTY()
				int RuntimeCount = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					RuntimeCopy = Payload;
					RuntimeCopy.Count += 5;
					RuntimeCopy.Label = Payload.Label + "_Runtime";
					RuntimeCount = RuntimeCopy.Count;
				}
			}
			)AS"),
			TEXT("ACoverageStructBlueprintBoundaryActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct Blueprint boundary actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UASClass* ParentASClass = Cast<UASClass>(ScriptClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("UStruct Blueprint boundary parent should be an AS class")));
		if (ParentASClass == nullptr)
		{
			return;
		}

		FStructProperty* ParentPayloadProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Payload"));
		ASSERT_THAT(IsNotNull(ParentPayloadProperty, TEXT("Script parent should expose the BlueprintType struct property")));
		if (ParentPayloadProperty == nullptr || ParentPayloadProperty->Struct == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("true")), ParentPayloadProperty->Struct->GetMetaData(TEXT("BlueprintType")),
			TEXT("Blueprint boundary struct should preserve BlueprintType metadata")));
		ASSERT_THAT(IsTrue(ParentPayloadProperty->HasAnyPropertyFlags(CPF_BlueprintVisible),
			TEXT("Struct property should be Blueprint-visible on the script parent")));

		FScopedTransientBlueprint Blueprint;
		ASSERT_THAT(IsTrue(Blueprint.CreateAndCompile(*TestRunner, ScriptClass, TEXT("BlueprintStructBoundary"))));
		UClass* BlueprintClass = Blueprint.GetGeneratedClass();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("UStruct Blueprint boundary should produce a Blueprint generated class")));
		if (BlueprintClass == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(ScriptClass), TEXT("Blueprint generated class should inherit from the script parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(BlueprintClass), TEXT("Blueprint generated class should remain a regular Blueprint class")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(BlueprintClass),
			TEXT("Blueprint generated class should retain the AS parent in its ancestry")));

		FStructProperty* BlueprintPayloadProperty = FindFProperty<FStructProperty>(BlueprintClass, TEXT("Payload"));
		ASSERT_THAT(IsNotNull(BlueprintPayloadProperty, TEXT("Blueprint generated class should inherit the struct property")));
		if (BlueprintPayloadProperty == nullptr || BlueprintPayloadProperty->Struct == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ParentPayloadProperty->Struct, BlueprintPayloadProperty->Struct,
			TEXT("Blueprint child should reference the same generated script UScriptStruct")));
		ASSERT_THAT(IsTrue(BlueprintPayloadProperty->HasAnyPropertyFlags(CPF_BlueprintVisible),
			TEXT("Inherited struct property should stay Blueprint-visible on the Blueprint generated class")));

		UObject* BlueprintDefaultObject = BlueprintClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(BlueprintDefaultObject, TEXT("Blueprint generated class should expose a CDO")));
		if (BlueprintDefaultObject == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, BlueprintDefaultObject, TEXT("Payload.Count"), 23,
			TEXT("Blueprint CDO should preserve inherited struct int default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, BlueprintDefaultObject, TEXT("Payload.Label"), FString(TEXT("StructDefault")),
			TEXT("Blueprint CDO should preserve inherited struct string default"))));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("UStruct Blueprint boundary actor should spawn from the Blueprint generated class")));
		if (BlueprintActor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *BlueprintActor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, BlueprintActor, TEXT("Payload.Count"), 23,
			TEXT("Blueprint actor should keep inherited struct int default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, BlueprintActor, TEXT("Payload.Label"), FString(TEXT("StructDefault")),
			TEXT("Blueprint actor should keep inherited struct string default"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, BlueprintActor, TEXT("RuntimeCopy.Count"), 28,
			TEXT("Blueprint actor should copy and mutate struct int at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, BlueprintActor, TEXT("RuntimeCopy.Label"), FString(TEXT("StructDefault_Runtime")),
			TEXT("Blueprint actor should copy and mutate struct string at runtime"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, BlueprintActor, TEXT("RuntimeCount"), 28,
			TEXT("Blueprint actor should observe runtime struct field values after BeginPlay"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT unsupported specifiers: Atomic and Immutable are not script-side specifiers.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructUnsupportedSpecifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		TArray<FString> AtomicDiagnostics;
		AtomicDiagnostics.Add(TEXT("Unknown class specifier Atomic"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_AtomicUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(Atomic)
			struct FAtomicStruct
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS"),
			TEXT("USTRUCT(Atomic) should remain an explicit unsupported script-side boundary"),
			MakeArrayView(AtomicDiagnostics))));

		TArray<FString> ImmutableDiagnostics;
		ImmutableDiagnostics.Add(TEXT("Unknown class specifier Immutable"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_ImmutableUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(Immutable)
			struct FImmutableStruct
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS"),
			TEXT("USTRUCT(Immutable) should remain an explicit unsupported script-side boundary"),
			MakeArrayView(ImmutableDiagnostics))));

		TArray<FString> NoExportDiagnostics;
		NoExportDiagnostics.Add(TEXT("Unknown class specifier NoExport"));
		ASSERT_THAT(IsTrue(CompileAndExpectFailure(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_NoExportUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(NoExport)
			struct FNoExportStruct
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS"),
			TEXT("USTRUCT(NoExport) should remain an explicit unsupported script-side boundary"),
			MakeArrayView(NoExportDiagnostics))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT member property specifiers: edit, visibility, serialization, config, instanced flags.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructPropertySpecifierFlagMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_PropertySpecifierFlagMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructPropertySpecifierFlagMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageStructInstancedObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			USTRUCT(BlueprintType)
			struct FStructPropertySpecifierMatrix
			{
				UPROPERTY(VisibleAnywhere)
				int VisibleValue = 1;

				UPROPERTY(VisibleDefaultsOnly)
				int VisibleDefaultValue = 2;

				UPROPERTY(VisibleInstanceOnly)
				int VisibleInstanceValue = 3;

				UPROPERTY(EditAnywhere)
				int EditAnywhereValue = 4;

				UPROPERTY(EditDefaultsOnly)
				int EditDefaultValue = 5;

				UPROPERTY(EditInstanceOnly)
				int EditInstanceValue = 6;

				UPROPERTY(NotEditable)
				int NotEditableValue = 7;

				UPROPERTY(EditConst)
				int EditConstValue = 8;

				UPROPERTY(AdvancedDisplay)
				int AdvancedValue = 9;

				UPROPERTY(Config)
				int ConfigValue = 10;

				UPROPERTY(AssetRegistrySearchable)
				int SearchableValue = 11;

				UPROPERTY(SkipSerialization)
				int SkipSerializedValue = 12;

				UPROPERTY(NoClear)
				AActor NoClearActor;

				UPROPERTY(Transient)
				int TransientValue = 13;

				UPROPERTY(SaveGame)
				int SaveGameValue = 14;

				UPROPERTY(EditFixedSize)
				TArray<int> FixedArray;

				UPROPERTY(Instanced)
				UCoverageStructInstancedObject InlineObject;
			}

			UCLASS()
			class ACoverageStructPropertySpecifierActor : AActor
			{
				UPROPERTY()
				FStructPropertySpecifierMatrix Data;
			}
			)AS"),
			TEXT("ACoverageStructPropertySpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct property-specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data should reflect as a generated struct property")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		UScriptStruct* MatrixStruct = DataProperty->Struct;
		FIntProperty* VisibleValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("VisibleValue"));
		FIntProperty* VisibleDefaultValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("VisibleDefaultValue"));
		FIntProperty* VisibleInstanceValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("VisibleInstanceValue"));
		FIntProperty* EditAnywhereValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("EditAnywhereValue"));
		FIntProperty* EditDefaultValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("EditDefaultValue"));
		FIntProperty* EditInstanceValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("EditInstanceValue"));
		FIntProperty* NotEditableValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("NotEditableValue"));
		FIntProperty* EditConstValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("EditConstValue"));
		FIntProperty* AdvancedValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("AdvancedValue"));
		FIntProperty* ConfigValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("ConfigValue"));
		FIntProperty* SearchableValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("SearchableValue"));
		FIntProperty* SkipSerializedValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("SkipSerializedValue"));
		FObjectProperty* NoClearActor = FindFProperty<FObjectProperty>(MatrixStruct, TEXT("NoClearActor"));
		FIntProperty* TransientValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("TransientValue"));
		FIntProperty* SaveGameValue = FindFProperty<FIntProperty>(MatrixStruct, TEXT("SaveGameValue"));
		FArrayProperty* FixedArray = FindFProperty<FArrayProperty>(MatrixStruct, TEXT("FixedArray"));
		FObjectProperty* InlineObject = FindFProperty<FObjectProperty>(MatrixStruct, TEXT("InlineObject"));

		ASSERT_THAT(IsNotNull(VisibleValue, TEXT("VisibleAnywhere member should reflect")));
		ASSERT_THAT(IsNotNull(VisibleDefaultValue, TEXT("VisibleDefaultsOnly member should reflect")));
		ASSERT_THAT(IsNotNull(VisibleInstanceValue, TEXT("VisibleInstanceOnly member should reflect")));
		ASSERT_THAT(IsNotNull(EditAnywhereValue, TEXT("EditAnywhere member should reflect")));
		ASSERT_THAT(IsNotNull(EditDefaultValue, TEXT("EditDefaultsOnly member should reflect")));
		ASSERT_THAT(IsNotNull(EditInstanceValue, TEXT("EditInstanceOnly member should reflect")));
		ASSERT_THAT(IsNotNull(NotEditableValue, TEXT("NotEditable member should reflect")));
		ASSERT_THAT(IsNotNull(EditConstValue, TEXT("EditConst member should reflect")));
		ASSERT_THAT(IsNotNull(AdvancedValue, TEXT("AdvancedDisplay member should reflect")));
		ASSERT_THAT(IsNotNull(ConfigValue, TEXT("Config member should reflect")));
		ASSERT_THAT(IsNotNull(SearchableValue, TEXT("AssetRegistrySearchable member should reflect")));
		ASSERT_THAT(IsNotNull(SkipSerializedValue, TEXT("SkipSerialization member should reflect")));
		ASSERT_THAT(IsNotNull(NoClearActor, TEXT("NoClear object member should reflect")));
		ASSERT_THAT(IsNotNull(TransientValue, TEXT("Transient member should reflect")));
		ASSERT_THAT(IsNotNull(SaveGameValue, TEXT("SaveGame member should reflect")));
		ASSERT_THAT(IsNotNull(FixedArray, TEXT("EditFixedSize array member should reflect")));
		ASSERT_THAT(IsNotNull(InlineObject, TEXT("Instanced object member should reflect")));
		if (VisibleValue == nullptr || VisibleDefaultValue == nullptr || VisibleInstanceValue == nullptr
			|| EditAnywhereValue == nullptr || EditDefaultValue == nullptr || EditInstanceValue == nullptr
			|| NotEditableValue == nullptr || EditConstValue == nullptr || AdvancedValue == nullptr
			|| ConfigValue == nullptr || SearchableValue == nullptr || SkipSerializedValue == nullptr
			|| NoClearActor == nullptr || TransientValue == nullptr || SaveGameValue == nullptr
			|| FixedArray == nullptr || InlineObject == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectPropertyFlags(VisibleValue, CPF_Edit | CPF_EditConst),
			TEXT("VisibleAnywhere should set edit visibility plus EditConst")));
		ASSERT_THAT(IsTrue(ExpectPropertyFlags(VisibleDefaultValue, CPF_Edit | CPF_EditConst | CPF_DisableEditOnInstance),
			TEXT("VisibleDefaultsOnly should disable instance editing")));
		ASSERT_THAT(IsTrue(ExpectPropertyFlags(VisibleInstanceValue, CPF_Edit | CPF_EditConst | CPF_DisableEditOnTemplate),
			TEXT("VisibleInstanceOnly should disable template editing")));
		ASSERT_THAT(IsTrue(ExpectPropertyFlags(EditAnywhereValue, CPF_Edit),
			TEXT("EditAnywhere should set CPF_Edit")));
		ASSERT_THAT(IsFalse(EditAnywhereValue->HasAnyPropertyFlags(CPF_DisableEditOnInstance | CPF_DisableEditOnTemplate),
			TEXT("EditAnywhere should not disable instance or template editing")));
		ASSERT_THAT(IsTrue(ExpectPropertyFlags(EditDefaultValue, CPF_Edit | CPF_DisableEditOnInstance),
			TEXT("EditDefaultsOnly should disable instance editing")));
		ASSERT_THAT(IsTrue(ExpectPropertyFlags(EditInstanceValue, CPF_Edit | CPF_DisableEditOnTemplate),
			TEXT("EditInstanceOnly should disable template editing")));
		ASSERT_THAT(IsFalse(NotEditableValue->HasAnyPropertyFlags(CPF_Edit),
			TEXT("NotEditable should suppress CPF_Edit")));
		ASSERT_THAT(IsFalse(EditConstValue->HasAnyPropertyFlags(CPF_Edit),
			TEXT("EditConst without an edit-visible specifier should not force CPF_Edit")));
		ASSERT_THAT(IsTrue(AdvancedValue->HasAnyPropertyFlags(CPF_AdvancedDisplay),
			TEXT("AdvancedDisplay should set CPF_AdvancedDisplay")));
		ASSERT_THAT(IsTrue(ConfigValue->HasAnyPropertyFlags(CPF_Config),
			TEXT("Config should set CPF_Config")));
		ASSERT_THAT(IsTrue(SearchableValue->HasAnyPropertyFlags(CPF_AssetRegistrySearchable),
			TEXT("AssetRegistrySearchable should set CPF_AssetRegistrySearchable")));
		ASSERT_THAT(IsTrue(SkipSerializedValue->HasAnyPropertyFlags(CPF_SkipSerialization),
			TEXT("SkipSerialization should set CPF_SkipSerialization")));
		ASSERT_THAT(IsTrue(NoClearActor->HasAnyPropertyFlags(CPF_NoClear),
			TEXT("NoClear should set CPF_NoClear")));
		ASSERT_THAT(IsTrue(TransientValue->HasAnyPropertyFlags(CPF_Transient),
			TEXT("Transient should set CPF_Transient")));
		ASSERT_THAT(IsTrue(SaveGameValue->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("SaveGame should set CPF_SaveGame")));
		ASSERT_THAT(IsTrue(FixedArray->HasAnyPropertyFlags(CPF_EditFixedSize),
			TEXT("EditFixedSize should set CPF_EditFixedSize")));
		ASSERT_THAT(IsTrue(InlineObject->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ExportObject | CPF_PersistentInstance),
			TEXT("Instanced should mark object properties as instanced/exported persistent references")));
		ASSERT_THAT(IsTrue((MatrixStruct->StructFlags & STRUCT_HasInstancedReference) != 0,
			TEXT("Instanced member inside USTRUCT should mark the generated UScriptStruct as containing instanced references")));
	}
	// -------------------------------------------------------------------------
	// USTRUCT members: various types (int, float, bool, FString, FVector, AActor, TArray)
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMembers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Members"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMembers.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FComplexStruct
			{
				UPROPERTY(EditAnywhere)
				int IntValue = 0;

				UPROPERTY(EditAnywhere)
				float FloatValue = 0.0f;

				UPROPERTY(EditAnywhere)
				bool BoolValue = false;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FString StringValue;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FName NameValue;

				UPROPERTY(EditAnywhere)
				FVector VectorValue;

				UPROPERTY()
				AActor ActorRef;

				UPROPERTY()
				TArray<int> IntArray;

				UPROPERTY()
				TArray<FString> StringArray;
			}

			UCLASS()
			class ACoverageStructMemberActor : AActor
			{
				UPROPERTY()
				FComplexStruct Data;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.IntValue = 42;
					Data.FloatValue = 3.14f;
					Data.BoolValue = true;
					Data.StringValue = "Hello";
					Data.NameValue = n"TestName";
					Data.VectorValue = FVector(1.0f, 2.0f, 3.0f);
					Data.ActorRef = this;
					Data.IntArray.Add(10);
					Data.IntArray.Add(20);
					Data.StringArray.Add("First");
					Data.StringArray.Add("Second");
				}
			}
			)AS"),
			TEXT("ACoverageStructMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct members actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct members actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.IntValue"), 42, TEXT("Struct int member"));
		VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("Data.FloatValue"), 3.14, TEXT("Struct float member"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("Data.BoolValue"), true, TEXT("Struct bool member"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.StringValue"), FString(TEXT("Hello")), TEXT("Struct FString member"));
		VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("Data.NameValue"), FName(TEXT("TestName")), TEXT("Struct FName member"));

		// Verify FVector
		FVector VectorResult(0.0f);
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("Data.VectorValue"), VectorResult), TEXT("Get FVector from struct")));
		ASSERT_THAT(IsTrue(VectorResult.Equals(FVector(1.0f, 2.0f, 3.0f)), TEXT("Struct FVector member should match")));

		// Verify AActor reference
		UObject* ActorRefObj = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Data.ActorRef"), ActorRefObj), TEXT("Get AActor ref from struct")));
		AActor* ActorRef = Cast<AActor>(ActorRefObj);
		ASSERT_THAT(AreEqual(Actor, ActorRef, TEXT("Struct AActor reference should point to self")));

		// Verify TArray<int>
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.IntArray[0]"), 10, TEXT("Struct TArray<int>[0]"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.IntArray[1]"), 20, TEXT("Struct TArray<int>[1]"));

		// Verify TArray<FString>
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.StringArray[0]"), FString(TEXT("First")), TEXT("Struct TArray<FString>[0]"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.StringArray[1]"), FString(TEXT("Second")), TEXT("Struct TArray<FString>[1]"));
	}

	// -------------------------------------------------------------------------
	// USTRUCT extended member matrix: text, math structs, object refs, soft/weak refs.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructExtendedMemberTypeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ExtendedMemberTypeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructExtendedMemberTypeMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageStructMemberObject : UObject
			{
				UPROPERTY()
				int Value = 17;
			}

			USTRUCT(BlueprintType)
			struct FStructExtendedMemberData
			{
				UPROPERTY()
				double DoubleValue = 0.0;

				UPROPERTY()
				FText TextValue;

				UPROPERTY()
				FRotator RotatorValue;

				UPROPERTY()
				FQuat QuatValue;

				UPROPERTY()
				FTransform TransformValue;

				UPROPERTY()
				UCoverageStructMemberObject ObjectRef;

				UPROPERTY()
				TSubclassOf<AActor> ActorClass;

				UPROPERTY()
				TWeakObjectPtr<AActor> WeakActor;

				UPROPERTY()
				TSoftObjectPtr<AActor> SoftActor;

				UPROPERTY()
				TSoftClassPtr<AActor> SoftActorClass;
			}

			UCLASS()
			class ACoverageStructExtendedMemberActor : AActor
			{
				UPROPERTY()
				FStructExtendedMemberData Data;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.DoubleValue = 6.25;
					Data.TextValue = FText::FromString("Struct extended text");
					Data.RotatorValue = FRotator(10, 20, 30);
					Data.QuatValue = FQuat(FRotator(0, 90, 0));
					Data.TransformValue = FTransform(FRotator(0, 45, 0), FVector(3, 4, 5), FVector(2, 2, 2));
					Data.ObjectRef = Cast<UCoverageStructMemberObject>(NewObject(this, UCoverageStructMemberObject::StaticClass()));
					Data.ActorClass = ACoverageStructExtendedMemberActor::StaticClass();
					Data.WeakActor = this;
					Data.SoftActor = this;
					Data.SoftActorClass = ACoverageStructExtendedMemberActor::StaticClass();
				}
			}
			)AS"),
			TEXT("ACoverageStructExtendedMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct extended member actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data should reflect as FStructProperty")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		UScriptStruct* DataStruct = DataProperty->Struct;
		ASSERT_THAT(IsNotNull(FindFProperty<FDoubleProperty>(DataStruct, TEXT("DoubleValue")), TEXT("double member should reflect as FDoubleProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FTextProperty>(DataStruct, TEXT("TextValue")), TEXT("FText member should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(DataStruct, TEXT("RotatorValue")), TEXT("FRotator member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(DataStruct, TEXT("QuatValue")), TEXT("FQuat member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FStructProperty>(DataStruct, TEXT("TransformValue")), TEXT("FTransform member should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FObjectProperty>(DataStruct, TEXT("ObjectRef")), TEXT("UObject member should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FClassProperty>(DataStruct, TEXT("ActorClass")), TEXT("TSubclassOf member should reflect as FClassProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FWeakObjectProperty>(DataStruct, TEXT("WeakActor")), TEXT("TWeakObjectPtr member should reflect as FWeakObjectProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FSoftObjectProperty>(DataStruct, TEXT("SoftActor")), TEXT("TSoftObjectPtr member should reflect as FSoftObjectProperty")));
		ASSERT_THAT(IsNotNull(FindFProperty<FSoftClassProperty>(DataStruct, TEXT("SoftActorClass")), TEXT("TSoftClassPtr member should reflect as FSoftClassProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct extended member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FDoubleProperty, double>(*TestRunner, Actor, TEXT("Data.DoubleValue"), 6.25, TEXT("double member should round-trip"))));
		FText TextValue;
		ASSERT_THAT(IsTrue(GetTextByPath(*TestRunner, Actor, TEXT("Data.TextValue"), TextValue), TEXT("FText member should be readable")));
		ASSERT_THAT(AreEqual(FString(TEXT("Struct extended text")), TextValue.ToString(), TEXT("FText member should round-trip")));

		FRotator RotatorValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FRotator>(*TestRunner, Actor, TEXT("Data.RotatorValue"), RotatorValue), TEXT("FRotator member should be readable")));
		ASSERT_THAT(IsTrue(RotatorValue.Equals(FRotator(10, 20, 30), 0.001), TEXT("FRotator member should round-trip")));

		FQuat QuatValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FQuat>(*TestRunner, Actor, TEXT("Data.QuatValue"), QuatValue), TEXT("FQuat member should be readable")));
		ASSERT_THAT(IsTrue(QuatValue.Equals(FQuat(FRotator(0, 90, 0)), 0.001), TEXT("FQuat member should round-trip")));

		FTransform TransformValue;
		ASSERT_THAT(IsTrue(GetStructByPath<FTransform>(*TestRunner, Actor, TEXT("Data.TransformValue"), TransformValue), TEXT("FTransform member should be readable")));
		ASSERT_THAT(IsTrue(TransformValue.Equals(FTransform(FRotator(0, 45, 0), FVector(3, 4, 5), FVector(2, 2, 2)), 0.001), TEXT("FTransform member should round-trip")));

		UObject* ObjectRef = nullptr;
		ASSERT_THAT(IsTrue(GetObjectByPath(*TestRunner, Actor, TEXT("Data.ObjectRef"), ObjectRef), TEXT("UObject member should be readable")));
		ASSERT_THAT(IsNotNull(ObjectRef, TEXT("UObject member should store NewObject result")));

		UObject* WeakObject = nullptr;
		ASSERT_THAT(IsTrue(GetWeakObjectByPath(*TestRunner, Actor, TEXT("Data.WeakActor"), WeakObject), TEXT("TWeakObjectPtr member should be readable")));
		ASSERT_THAT(AreEqual(static_cast<UObject*>(Actor), WeakObject, TEXT("TWeakObjectPtr member should resolve to the actor")));

		FSoftObjectPath SoftActorPath;
		ASSERT_THAT(IsTrue(GetSoftObjectPathByPath(*TestRunner, Actor, TEXT("Data.SoftActor"), SoftActorPath), TEXT("TSoftObjectPtr member should expose a path")));
		ASSERT_THAT(IsFalse(SoftActorPath.IsNull(), TEXT("TSoftObjectPtr member should not be null after assignment")));

		FSoftObjectPath SoftActorClassPath;
		ASSERT_THAT(IsTrue(GetSoftClassPathByPath(*TestRunner, Actor, TEXT("Data.SoftActorClass"), SoftActorClassPath), TEXT("TSoftClassPtr member should expose a path")));
		ASSERT_THAT(IsFalse(SoftActorClassPath.IsNull(), TEXT("TSoftClassPtr member should not be null after assignment")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT member reflection: enum/FText fields and member property flags.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructEnumTextAndPropertyFlags)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_EnumTextAndFlags"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructEnumTextAndFlags.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EStructMemberState
			{
				Idle,
				Running,
				Finished
			}

			USTRUCT(BlueprintType)
			struct FStructMemberReflectionData
			{
				UPROPERTY(EditAnywhere)
				EStructMemberState State = EStructMemberState::Idle;

				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				FText Description;

				UPROPERTY(SaveGame)
				int SavedScore = 7;

				UPROPERTY(Transient)
				int RuntimeScratch = 9;
			}

			UCLASS()
			class ACoverageStructMemberReflectionActor : AActor
			{
				UPROPERTY()
				FStructMemberReflectionData Data;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.State = EStructMemberState::Finished;
					Data.Description = FText::FromString("Struct text value");
					Data.SavedScore = 42;
					Data.RuntimeScratch = 88;
				}
			}
			)AS"),
			TEXT("ACoverageStructMemberReflectionActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct enum/text member actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data should reflect as a struct property")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		FEnumProperty* StateProperty = FindFProperty<FEnumProperty>(DataProperty->Struct, TEXT("State"));
		FTextProperty* DescriptionProperty = FindFProperty<FTextProperty>(DataProperty->Struct, TEXT("Description"));
		FIntProperty* SavedScoreProperty = FindFProperty<FIntProperty>(DataProperty->Struct, TEXT("SavedScore"));
		FIntProperty* RuntimeScratchProperty = FindFProperty<FIntProperty>(DataProperty->Struct, TEXT("RuntimeScratch"));
		ASSERT_THAT(IsNotNull(StateProperty, TEXT("Struct enum member should reflect as FEnumProperty")));
		ASSERT_THAT(IsNotNull(DescriptionProperty, TEXT("Struct FText member should reflect as FTextProperty")));
		ASSERT_THAT(IsNotNull(SavedScoreProperty, TEXT("Struct SaveGame int member should reflect")));
		ASSERT_THAT(IsNotNull(RuntimeScratchProperty, TEXT("Struct Transient int member should reflect")));
		if (StateProperty == nullptr || DescriptionProperty == nullptr
			|| SavedScoreProperty == nullptr || RuntimeScratchProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(SavedScoreProperty->HasAnyPropertyFlags(CPF_SaveGame),
			TEXT("SaveGame on a USTRUCT member should set CPF_SaveGame")));
		ASSERT_THAT(IsTrue(RuntimeScratchProperty->HasAnyPropertyFlags(CPF_Transient),
			TEXT("Transient on a USTRUCT member should set CPF_Transient")));
		ASSERT_THAT(IsTrue(DescriptionProperty->HasAnyPropertyFlags(CPF_BlueprintVisible),
			TEXT("BlueprintReadWrite on a USTRUCT FText member should set CPF_BlueprintVisible")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct enum/text member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		void* DataAddress = DataProperty->ContainerPtrToValuePtr<void>(Actor);
		ASSERT_THAT(IsNotNull(DataAddress, TEXT("Actor should store Data struct memory")));
		if (DataAddress == nullptr)
		{
			return;
		}

		const void* StateAddress = StateProperty->ContainerPtrToValuePtr<void>(DataAddress);
		const int64 StateValue = StateProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(StateAddress);
		ASSERT_THAT(AreEqual(2LL, StateValue, TEXT("Struct enum member should round-trip script assignment")));

		const FText DescriptionValue = DescriptionProperty->GetPropertyValue_InContainer(DataAddress);
		ASSERT_THAT(AreEqual(FString(TEXT("Struct text value")), DescriptionValue.ToString(),
			TEXT("Struct FText member should round-trip script assignment")));
		ASSERT_THAT(AreEqual(42, SavedScoreProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("SaveGame struct member value should round-trip script assignment")));
		ASSERT_THAT(AreEqual(88, RuntimeScratchProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("Transient struct member value should round-trip script assignment")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT default value matrix: scalar, name/string, enum, math, null refs, empty containers.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructDefaultValueTypeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_DefaultValueTypeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructDefaultValueTypeMatrix.as"),
			ASTEST_AS(R"AS(
			UENUM(BlueprintType)
			enum EStructDefaultState
			{
				None,
				Ready,
				Complete
			}

			USTRUCT(BlueprintType)
			struct FStructDefaultValueMatrix
			{
				UPROPERTY()
				bool bEnabled = true;

				UPROPERTY()
				int Count = 17;

				UPROPERTY()
				double Weight = 2.5;

				UPROPERTY()
				FString Label = "DefaultLabel";

				UPROPERTY()
				FName Tag = n"DefaultTag";

				UPROPERTY()
				EStructDefaultState State = EStructDefaultState::Ready;

				UPROPERTY()
				FVector Location = FVector(1, 2, 3);

				UPROPERTY()
				FRotator Rotation = FRotator(10, 20, 30);

				UPROPERTY()
				FVector2D Screen = FVector2D(4, 5);

				UPROPERTY()
				FColor Color = FColor(10, 20, 30, 40);

				UPROPERTY()
				FLinearColor LinearColor = FLinearColor(0.1, 0.2, 0.3, 0.4);

				UPROPERTY()
				AActor ActorRef;

				UPROPERTY()
				TArray<int> Numbers;

				UPROPERTY()
				TMap<FName, int> Scores;

				UPROPERTY()
				TSet<FName> Tags;
			}

			UCLASS()
			class ACoverageStructDefaultValueActor : AActor
			{
				UPROPERTY()
				FStructDefaultValueMatrix Data;

				UPROPERTY()
				int RuntimeEmptyContainerMask = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					if (Data.Numbers.Num() == 0)
					{
						RuntimeEmptyContainerMask |= 1;
					}

					if (Data.Scores.Num() == 0)
					{
						RuntimeEmptyContainerMask |= 2;
					}

					if (Data.Tags.Num() == 0)
					{
						RuntimeEmptyContainerMask |= 4;
					}

					if (Data.ActorRef == nullptr)
					{
						RuntimeEmptyContainerMask |= 8;
					}
				}
			}
			)AS"),
			TEXT("ACoverageStructDefaultValueActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct default-value matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* ClassDefaultObject = ScriptClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ClassDefaultObject, TEXT("UStruct default-value actor should expose a CDO")));
		if (ClassDefaultObject == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data should reflect as a default-value struct property")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		void* DataAddress = DataProperty->ContainerPtrToValuePtr<void>(ClassDefaultObject);
		ASSERT_THAT(IsNotNull(DataAddress, TEXT("CDO should store Data struct memory")));
		if (DataAddress == nullptr)
		{
			return;
		}

		UScriptStruct* DefaultStruct = DataProperty->Struct;
		FBoolProperty* EnabledProperty = FindFProperty<FBoolProperty>(DefaultStruct, TEXT("bEnabled"));
		FIntProperty* CountProperty = FindFProperty<FIntProperty>(DefaultStruct, TEXT("Count"));
		FDoubleProperty* WeightProperty = FindFProperty<FDoubleProperty>(DefaultStruct, TEXT("Weight"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(DefaultStruct, TEXT("Label"));
		FNameProperty* TagProperty = FindFProperty<FNameProperty>(DefaultStruct, TEXT("Tag"));
		FEnumProperty* StateProperty = FindFProperty<FEnumProperty>(DefaultStruct, TEXT("State"));
		FStructProperty* LocationProperty = FindFProperty<FStructProperty>(DefaultStruct, TEXT("Location"));
		FStructProperty* RotationProperty = FindFProperty<FStructProperty>(DefaultStruct, TEXT("Rotation"));
		FStructProperty* ScreenProperty = FindFProperty<FStructProperty>(DefaultStruct, TEXT("Screen"));
		FStructProperty* ColorProperty = FindFProperty<FStructProperty>(DefaultStruct, TEXT("Color"));
		FStructProperty* LinearColorProperty = FindFProperty<FStructProperty>(DefaultStruct, TEXT("LinearColor"));
		FObjectProperty* ActorRefProperty = FindFProperty<FObjectProperty>(DefaultStruct, TEXT("ActorRef"));
		FArrayProperty* NumbersProperty = FindFProperty<FArrayProperty>(DefaultStruct, TEXT("Numbers"));
		FMapProperty* ScoresProperty = FindFProperty<FMapProperty>(DefaultStruct, TEXT("Scores"));
		FSetProperty* TagsProperty = FindFProperty<FSetProperty>(DefaultStruct, TEXT("Tags"));

		ASSERT_THAT(IsNotNull(EnabledProperty, TEXT("bool default member should reflect")));
		ASSERT_THAT(IsNotNull(CountProperty, TEXT("int default member should reflect")));
		ASSERT_THAT(IsNotNull(WeightProperty, TEXT("double default member should reflect")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("FString default member should reflect")));
		ASSERT_THAT(IsNotNull(TagProperty, TEXT("FName default member should reflect")));
		ASSERT_THAT(IsNotNull(StateProperty, TEXT("enum default member should reflect")));
		ASSERT_THAT(IsNotNull(LocationProperty, TEXT("FVector default member should reflect")));
		ASSERT_THAT(IsNotNull(RotationProperty, TEXT("FRotator default member should reflect")));
		ASSERT_THAT(IsNotNull(ScreenProperty, TEXT("FVector2D default member should reflect")));
		ASSERT_THAT(IsNotNull(ColorProperty, TEXT("FColor default member should reflect")));
		ASSERT_THAT(IsNotNull(LinearColorProperty, TEXT("FLinearColor default member should reflect")));
		ASSERT_THAT(IsNotNull(ActorRefProperty, TEXT("AActor null default member should reflect")));
		ASSERT_THAT(IsNotNull(NumbersProperty, TEXT("TArray default member should reflect")));
		ASSERT_THAT(IsNotNull(ScoresProperty, TEXT("TMap default member should reflect")));
		ASSERT_THAT(IsNotNull(TagsProperty, TEXT("TSet default member should reflect")));
		if (EnabledProperty == nullptr || CountProperty == nullptr || WeightProperty == nullptr
			|| LabelProperty == nullptr || TagProperty == nullptr || StateProperty == nullptr
			|| LocationProperty == nullptr || RotationProperty == nullptr || ScreenProperty == nullptr
			|| ColorProperty == nullptr || LinearColorProperty == nullptr || ActorRefProperty == nullptr
			|| NumbersProperty == nullptr || ScoresProperty == nullptr || TagsProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(EnabledProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("bool default should propagate to the CDO")));
		ASSERT_THAT(AreEqual(17, CountProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("int default should propagate to the CDO")));
		ASSERT_THAT(IsNear(2.5, WeightProperty->GetPropertyValue_InContainer(DataAddress), 0.0001,
			TEXT("double default should propagate to the CDO")));
		ASSERT_THAT(AreEqual(FString(TEXT("DefaultLabel")), LabelProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("FString default should propagate to the CDO")));
		ASSERT_THAT(AreEqual(FName(TEXT("DefaultTag")), TagProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("FName default should propagate to the CDO")));

		const void* StateAddress = StateProperty->ContainerPtrToValuePtr<void>(DataAddress);
		ASSERT_THAT(AreEqual(1LL, StateProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(StateAddress),
			TEXT("enum default should propagate to the CDO")));

		const FVector& LocationValue = *LocationProperty->ContainerPtrToValuePtr<FVector>(DataAddress);
		ASSERT_THAT(IsTrue(LocationValue.Equals(FVector(1, 2, 3), 0.001),
			TEXT("FVector default should propagate to the CDO")));
		const FRotator& RotationValue = *RotationProperty->ContainerPtrToValuePtr<FRotator>(DataAddress);
		ASSERT_THAT(IsTrue(RotationValue.Equals(FRotator(10, 20, 30), 0.001),
			TEXT("FRotator default should propagate to the CDO")));
		const FVector2D& ScreenValue = *ScreenProperty->ContainerPtrToValuePtr<FVector2D>(DataAddress);
		ASSERT_THAT(IsTrue(ScreenValue.Equals(FVector2D(4, 5), 0.001),
			TEXT("FVector2D default should propagate to the CDO")));
		const FColor& ColorValue = *ColorProperty->ContainerPtrToValuePtr<FColor>(DataAddress);
		ASSERT_THAT(AreEqual(FColor(10, 20, 30, 40), ColorValue,
			TEXT("FColor default should propagate to the CDO")));
		const FLinearColor& LinearColorValue = *LinearColorProperty->ContainerPtrToValuePtr<FLinearColor>(DataAddress);
		ASSERT_THAT(IsTrue(LinearColorValue.Equals(FLinearColor(0.1f, 0.2f, 0.3f, 0.4f), 0.001f),
			TEXT("FLinearColor default should propagate to the CDO")));
		ASSERT_THAT(IsNull(ActorRefProperty->GetObjectPropertyValue_InContainer(DataAddress),
			TEXT("UObject reference should default to null inside USTRUCT")));

		FScriptArrayHelper NumbersHelper(NumbersProperty, NumbersProperty->ContainerPtrToValuePtr<void>(DataAddress));
		FScriptMapHelper ScoresHelper(ScoresProperty, ScoresProperty->ContainerPtrToValuePtr<void>(DataAddress));
		FScriptSetHelper TagsHelper(TagsProperty, TagsProperty->ContainerPtrToValuePtr<void>(DataAddress));
		ASSERT_THAT(AreEqual(0, NumbersHelper.Num(), TEXT("TArray member should default to empty on the CDO")));
		ASSERT_THAT(AreEqual(0, ScoresHelper.Num(), TEXT("TMap member should default to empty on the CDO")));
		ASSERT_THAT(AreEqual(0, TagsHelper.Num(), TEXT("TSet member should default to empty on the CDO")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct default-value actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("RuntimeEmptyContainerMask"), 15,
			TEXT("runtime default checks should see empty containers and null references"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT member specifier matrix: read-only/edit flags and optional struct fields.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructOptionalAndSpecifierCombinations)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_OptionalAndSpecifierCombinations"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructOptionalAndSpecifierCombinations.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructOptionalPayload
			{
				UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coverage|OptionalStruct")
				int ReadOnlyEditable = 5;

				UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Coverage|OptionalStruct")
				FString EditableLabel = "DefaultLabel";
			}

			USTRUCT(BlueprintType)
			struct FStructOptionalOwner
			{
				UPROPERTY(EditAnywhere, BlueprintReadWrite)
				TOptional<FStructOptionalPayload> SetPayload;

				UPROPERTY(EditAnywhere, BlueprintReadOnly)
				TOptional<FStructOptionalPayload> EmptyPayload;
			}

			UCLASS()
			class ACoverageStructOptionalSpecifierActor : AActor
			{
				UPROPERTY()
				FStructOptionalOwner Data;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FStructOptionalPayload Payload;
					Payload.ReadOnlyEditable = 42;
					Payload.EditableLabel = "OptionalLabel";
					Data.SetPayload = Payload;
				}
			}
			)AS"),
			TEXT("ACoverageStructOptionalSpecifierActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct optional/specifier actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data should reflect as a struct property")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		FOptionalProperty* SetPayloadProperty = FindFProperty<FOptionalProperty>(DataProperty->Struct, TEXT("SetPayload"));
		FOptionalProperty* EmptyPayloadProperty = FindFProperty<FOptionalProperty>(DataProperty->Struct, TEXT("EmptyPayload"));
		ASSERT_THAT(IsNotNull(SetPayloadProperty, TEXT("TOptional<FStruct> set field should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(EmptyPayloadProperty, TEXT("TOptional<FStruct> empty field should reflect as FOptionalProperty")));
		if (SetPayloadProperty == nullptr || EmptyPayloadProperty == nullptr)
		{
			return;
		}

		FStructProperty* SetPayloadInner = CastField<FStructProperty>(SetPayloadProperty->GetValueProperty());
		ASSERT_THAT(IsNotNull(SetPayloadInner, TEXT("TOptional<FStruct> inner value should reflect as FStructProperty")));
		if (SetPayloadInner == nullptr || SetPayloadInner->Struct == nullptr)
		{
			return;
		}

		FIntProperty* ReadOnlyEditableProperty = FindFProperty<FIntProperty>(SetPayloadInner->Struct, TEXT("ReadOnlyEditable"));
		FStrProperty* EditableLabelProperty = FindFProperty<FStrProperty>(SetPayloadInner->Struct, TEXT("EditableLabel"));
		ASSERT_THAT(IsNotNull(ReadOnlyEditableProperty, TEXT("Optional payload int member should reflect")));
		ASSERT_THAT(IsNotNull(EditableLabelProperty, TEXT("Optional payload string member should reflect")));
		if (ReadOnlyEditableProperty == nullptr || EditableLabelProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ReadOnlyEditableProperty->HasAnyPropertyFlags(CPF_Edit),
			TEXT("EditAnywhere inside optional USTRUCT payload should set CPF_Edit")));
		ASSERT_THAT(IsTrue(ReadOnlyEditableProperty->HasAnyPropertyFlags(CPF_BlueprintVisible),
			TEXT("BlueprintReadOnly inside optional USTRUCT payload should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsTrue(ReadOnlyEditableProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly),
			TEXT("BlueprintReadOnly inside optional USTRUCT payload should set CPF_BlueprintReadOnly")));
		ASSERT_THAT(IsTrue(EditableLabelProperty->HasAnyPropertyFlags(CPF_Edit),
			TEXT("EditAnywhere+BlueprintReadWrite inside optional USTRUCT payload should set CPF_Edit")));
		ASSERT_THAT(IsTrue(EditableLabelProperty->HasAnyPropertyFlags(CPF_BlueprintVisible),
			TEXT("BlueprintReadWrite inside optional USTRUCT payload should set CPF_BlueprintVisible")));
		ASSERT_THAT(IsFalse(EditableLabelProperty->HasAnyPropertyFlags(CPF_BlueprintReadOnly),
			TEXT("BlueprintReadWrite inside optional USTRUCT payload should not set CPF_BlueprintReadOnly")));
		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|OptionalStruct")), ReadOnlyEditableProperty->GetMetaData(TEXT("Category")),
			TEXT("Category metadata should round-trip inside optional USTRUCT payload")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct optional/specifier actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		bool bIsSet = false;
		ASSERT_THAT(IsTrue(GetOptionalIsSetByPath(*TestRunner, Actor, TEXT("Data.SetPayload"), bIsSet),
			TEXT("Set TOptional<FStruct> should be inspectable")));
		ASSERT_THAT(IsTrue(bIsSet, TEXT("Assigned TOptional<FStruct> should report IsSet=true")));

		ASSERT_THAT(IsTrue(GetOptionalIsSetByPath(*TestRunner, Actor, TEXT("Data.EmptyPayload"), bIsSet),
			TEXT("Empty TOptional<FStruct> should be inspectable")));
		ASSERT_THAT(IsFalse(bIsSet, TEXT("Unassigned TOptional<FStruct> should report IsSet=false")));

		FPropertyBindingPathIndirection Leaf;
		ASSERT_THAT(IsTrue(ResolvePathOnObject(*TestRunner, Actor, TEXT("Data.SetPayload"), Leaf),
			TEXT("Set TOptional<FStruct> path should resolve")));
		if (Leaf.GetPropertyAddress() == nullptr)
		{
			return;
		}

		const void* PayloadAddress = SetPayloadProperty->GetValuePointerForRead(Leaf.GetPropertyAddress());
		ASSERT_THAT(IsNotNull(PayloadAddress, TEXT("Set TOptional<FStruct> should expose inner value memory")));
		if (PayloadAddress == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, ReadOnlyEditableProperty->GetPropertyValue_InContainer(PayloadAddress),
			TEXT("TOptional<FStruct> should preserve nested int member values")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalLabel")), EditableLabelProperty->GetPropertyValue_InContainer(PayloadAddress),
			TEXT("TOptional<FStruct> should preserve nested string member values")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT value semantics: copy construction, assignment, comparison, defaults
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructValueSemantics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ValueSemantics"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructValueSemantics.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FValueStruct
			{
				UPROPERTY()
				int X = 10;

				UPROPERTY()
				int Y = 20;

				UPROPERTY()
				FString Name = "Default";

				// Script USTRUCTs do not auto-generate ==; define value equality explicitly.
				bool opEquals(const FValueStruct&in Other) const
				{
					return X == Other.X && Y == Other.Y && Name == Other.Name;
				}
			}

			UCLASS()
			class ACoverageStructValueActor : AActor
			{
				UPROPERTY()
				FValueStruct Original;

				UPROPERTY()
				FValueStruct CopyConstructed;

				UPROPERTY()
				FValueStruct Assigned;

				UPROPERTY()
				bool AreEqual = false;

				UPROPERTY()
				bool AreNotEqual = false;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test default values
					Original.X = 100;
					Original.Y = 200;
					Original.Name = "Original";

					// Copy construction
					CopyConstructed = Original;

					// Assignment
					Assigned.X = 0;
					Assigned.Y = 0;
					Assigned.Name = "Temp";
					Assigned = Original;

					// Comparison
					AreEqual = (CopyConstructed == Original);

					FValueStruct Different;
					Different.X = 999;
					AreNotEqual = (Different != Original);
				}
			}
			)AS"),
			TEXT("ACoverageStructValueActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct value semantics actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct value semantics actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify original
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Original.X"), 100, TEXT("Original.X"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Original.Y"), 200, TEXT("Original.Y"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Original.Name"), FString(TEXT("Original")), TEXT("Original.Name"));

		// Verify copy construction
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopyConstructed.X"), 100, TEXT("CopyConstructed.X should match original"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopyConstructed.Y"), 200, TEXT("CopyConstructed.Y should match original"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("CopyConstructed.Name"), FString(TEXT("Original")), TEXT("CopyConstructed.Name should match original"));

		// Verify assignment
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Assigned.X"), 100, TEXT("Assigned.X should match original after assignment"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Assigned.Y"), 200, TEXT("Assigned.Y should match original after assignment"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Assigned.Name"), FString(TEXT("Original")), TEXT("Assigned.Name should match original after assignment"));

		// Verify comparison operators
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AreEqual"), true, TEXT("Structs with same values should be equal"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AreNotEqual"), true, TEXT("Structs with different values should not be equal"));
	}
	// -------------------------------------------------------------------------
	// USTRUCT operator overloads: opEquals, opAdd, opCmp
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructOperators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Operators"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructOperators.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FOperatorStruct
			{
				UPROPERTY()
				int X = 0;

				UPROPERTY()
				int Y = 0;

				bool opEquals(const FOperatorStruct& Other) const
				{
					return X == Other.X && Y == Other.Y;
				}

				FOperatorStruct opAdd(const FOperatorStruct& Other) const
				{
					FOperatorStruct Result;
					Result.X = X + Other.X;
					Result.Y = Y + Other.Y;
					return Result;
				}

				FOperatorStruct& opAssign(const FOperatorStruct& Other)
				{
					X = Other.X + 1;
					Y = Other.Y + 1;
					return this;
				}

				int opCmp(const FOperatorStruct& Other) const
				{
					if (X < Other.X) return -1;
					if (X > Other.X) return 1;
					if (Y < Other.Y) return -1;
					if (Y > Other.Y) return 1;
					return 0;
				}

				int opIndex(int Index) const
				{
					if (Index == 0) return X;
					if (Index == 1) return Y;
					return -1;
				}
			}

			UCLASS()
			class ACoverageStructOperatorActor : AActor
			{
				UPROPERTY()
				FOperatorStruct A;

				UPROPERTY()
				FOperatorStruct B;

				UPROPERTY()
				FOperatorStruct Sum;

				UPROPERTY()
				FOperatorStruct AssignedViaOperator;

				UPROPERTY()
				bool AreEqual = false;

				UPROPERTY()
				bool ALessThanB = false;

				UPROPERTY()
				bool AGreaterThanB = false;

				UPROPERTY()
				int IndexedSum = 0;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					A.X = 10;
					A.Y = 20;

					B.X = 5;
					B.Y = 15;

					// opAdd
					Sum = A + B;

					// opEquals
					FOperatorStruct ACopy;
					ACopy.X = 10;
					ACopy.Y = 20;
					AreEqual = (A == ACopy);

					AssignedViaOperator = B;

					// opCmp
					ALessThanB = (B < A);
					AGreaterThanB = (A > B);

					// opIndex
					IndexedSum = A[0] + A[1];
				}
			}
			)AS"),
			TEXT("ACoverageStructOperatorActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct operators actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct operators actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify opAdd result
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Sum.X"), 15, TEXT("opAdd should sum X values"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Sum.Y"), 35, TEXT("opAdd should sum Y values"));

		// Verify opEquals
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AreEqual"), true, TEXT("opEquals should return true for equal structs"));

		// Verify opAssign
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AssignedViaOperator.X"), 6, TEXT("opAssign should customize assigned X value"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("AssignedViaOperator.Y"), 16, TEXT("opAssign should customize assigned Y value"));

		// Verify opCmp
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ALessThanB"), true, TEXT("opCmp should support less-than comparison"));
		VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("AGreaterThanB"), true, TEXT("opCmp should support greater-than comparison"));

		// Verify opIndex
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("IndexedSum"), 30, TEXT("opIndex should support struct indexing"));
	}

	// -------------------------------------------------------------------------
	// USTRUCT member methods: const/non-const calls, mutation, and struct return.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMemberMethodInvocationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_MemberMethodInvocationMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMemberMethodInvocationMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructMethodPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;

				int Score() const
				{
					return Count * 10 + Label.Len();
				}

				FString Describe() const
				{
					return Label + ":" + Count;
				}

				void Add(int Delta)
				{
					Count += Delta;
				}

				void Rename(const FString&in NewLabel)
				{
					Label = NewLabel;
				}

				FStructMethodPayload WithBonus(int Bonus) const
				{
					FStructMethodPayload Result;
					Result.Count = Count + Bonus;
					Result.Label = Label + "_Bonus";
					return Result;
				}

				void CopyFrom(const FStructMethodPayload& Other)
				{
					Count = Other.Count;
					Label = Other.Label;
				}
			}

			UCLASS()
			class ACoverageStructMethodActor : AActor
			{
				UPROPERTY()
				FStructMethodPayload Data;

				UPROPERTY()
				FStructMethodPayload BonusData;

				UPROPERTY()
				FStructMethodPayload CopiedData;

				UPROPERTY()
				int InitialScore = 0;

				UPROPERTY()
				int MutatedScore = 0;

				UPROPERTY()
				FString InitialDescription;

				UPROPERTY()
				FString MutatedDescription;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.Count = 4;
					Data.Label = "Base";

					InitialScore = Data.Score();
					InitialDescription = Data.Describe();

					Data.Add(3);
					Data.Rename("Renamed");

					MutatedScore = Data.Score();
					MutatedDescription = Data.Describe();

					BonusData = Data.WithBonus(5);
					CopiedData.CopyFrom(BonusData);
				}
			}
			)AS"),
			TEXT("ACoverageStructMethodActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct member-method actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct member-method actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InitialScore"), 44,
			TEXT("Const USTRUCT member method should read fields and return int values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("InitialDescription"), FString(TEXT("Base:4")),
			TEXT("Const USTRUCT member method should return FString values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.Count"), 7,
			TEXT("Non-const USTRUCT member method should mutate int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.Label"), FString(TEXT("Renamed")),
			TEXT("Non-const USTRUCT member method should mutate string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MutatedScore"), 77,
			TEXT("Const USTRUCT member method should observe post-mutation state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("MutatedDescription"), FString(TEXT("Renamed:7")),
			TEXT("Const USTRUCT member method should return strings after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BonusData.Count"), 12,
			TEXT("USTRUCT member method should return a new struct value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("BonusData.Label"), FString(TEXT("Renamed_Bonus")),
			TEXT("Returned USTRUCT value should preserve string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("CopiedData.Count"), 12,
			TEXT("Non-const USTRUCT member method should copy fields from const-ref struct parameters"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("CopiedData.Label"), FString(TEXT("Renamed_Bonus")),
			TEXT("Non-const USTRUCT member method should copy FString fields from const-ref struct parameters"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT as parameter: value, &in, &out, &inout
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAsParameter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Parameter"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructParameter.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FParamStruct
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				FString Name;
			}

			UCLASS()
			class ACoverageStructParamActor : AActor
			{
				UPROPERTY()
				FParamStruct ValueParam;

				UPROPERTY()
				FParamStruct InParam;

				UPROPERTY()
				FParamStruct OutParam;

				UPROPERTY()
				FParamStruct InoutParam;

				void ModifyByValue(FParamStruct Param)
				{
					// By-value UStruct params are immutable in this fork; mutating a local
					// copy still demonstrates that the caller's struct is unaffected.
					FParamStruct Local = Param;
					Local.Value = 999;
				}

				void ReadByConstRef(const FParamStruct&in Param)
				{
					InParam.Value = Param.Value;
					InParam.Name = Param.Name;
				}

				void WriteByRef(FParamStruct&out Param)
				{
					Param.Value = 777;
					Param.Name = "Modified";
				}

				void MutateInout(FParamStruct&inout Param)
				{
					Param.Value += 333;
					Param.Name += "_Inout";
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Test value parameter (copy)
					ValueParam.Value = 100;
					ValueParam.Name = "Original";
					ModifyByValue(ValueParam);
					// ValueParam should remain 100

					// Test &in parameter (read-only reference)
					FParamStruct Source;
					Source.Value = 200;
					Source.Name = "Source";
					ReadByConstRef(Source);

					// Test &out parameter (write reference)
					WriteByRef(OutParam);

					// Test &inout parameter (read and write reference)
					InoutParam.Value = 300;
					InoutParam.Name = "Mutable";
					MutateInout(InoutParam);
				}
			}
			)AS"),
			TEXT("ACoverageStructParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ValueParam.Value"), 100,
			TEXT("Value parameter should not be modified"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ValueParam.Name"), FString(TEXT("Original")),
			TEXT("Value parameter name should not be modified"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InParam.Value"), 200,
			TEXT("&in parameter should read value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("InParam.Name"), FString(TEXT("Source")),
			TEXT("&in parameter should read name"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OutParam.Value"), 777,
			TEXT("&out parameter should write value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("OutParam.Name"), FString(TEXT("Modified")),
			TEXT("&out parameter should write name"))));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InoutParam.Value"), 633,
			TEXT("&inout parameter should read and mutate value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("InoutParam.Name"), FString(TEXT("Mutable_Inout")),
			TEXT("&inout parameter should read and mutate string fields"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT as reflected UFUNCTION parameters: real runtime invocation path.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructUFunctionParameterInvocation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_UFunctionParameterInvocation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructUFunctionParameterInvocation.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FInvokedStructParam
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructUFunctionParamActor : AActor
			{
				UPROPERTY()
				FInvokedStructParam LastValue;

				UPROPERTY()
				FInvokedStructParam LastConstRef;

				UPROPERTY()
				FInvokedStructParam LastOut;

				UPROPERTY()
				FInvokedStructParam LastInout;

				UFUNCTION(BlueprintCallable)
				void AcceptStructValue(FInvokedStructParam Param)
				{
					LastValue.Count = Param.Count + 1;
					LastValue.Label = Param.Label + "_Value";
				}

				UFUNCTION(BlueprintCallable)
				void AcceptStructConstRef(const FInvokedStructParam&in Param)
				{
					LastConstRef.Count = Param.Count + 2;
					LastConstRef.Label = Param.Label + "_Ref";
				}

				UFUNCTION(BlueprintCallable)
				void FillStructOut(FInvokedStructParam&out Param)
				{
					Param.Count = 55;
					Param.Label = "OutValue";
					LastOut = Param;
				}

				UFUNCTION(BlueprintCallable)
				int MutateStructInout(FInvokedStructParam&inout Param)
				{
					Param.Count += 3;
					Param.Label += "_Inout";
					LastInout = Param;
					return Param.Count;
				}
			}
			)AS"),
			TEXT("ACoverageStructUFunctionParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct UFUNCTION parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct UFUNCTION parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FStructProperty* LastValueProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("LastValue"));
		ASSERT_THAT(IsNotNull(LastValueProperty, TEXT("LastValue should reflect the AS struct type")));
		if (LastValueProperty == nullptr || LastValueProperty->Struct == nullptr)
		{
			return;
		}

		FIntProperty* CountProperty = FindFProperty<FIntProperty>(LastValueProperty->Struct, TEXT("Count"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(LastValueProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(CountProperty, TEXT("FInvokedStructParam.Count should reflect")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("FInvokedStructParam.Label should reflect")));
		if (CountProperty == nullptr || LabelProperty == nullptr)
		{
			return;
		}

		FFunctionInvoker ValueInvoker(*TestRunner, Actor, TEXT("AcceptStructValue"));
		ASSERT_THAT(IsTrue(ValueInvoker.IsValid(), TEXT("AcceptStructValue should be invokable")));
		if (!ValueInvoker.IsValid())
		{
			return;
		}

		FProperty* ValueParamProperty = nullptr;
		void* ValueParamSlot = nullptr;
		ASSERT_THAT(IsTrue(ValueInvoker.AddParamSlot(ValueParamProperty, ValueParamSlot),
			TEXT("AcceptStructValue should expose a mutable struct parameter slot")));
		FStructProperty* ValueStructParamProperty = CastField<FStructProperty>(ValueParamProperty);
		ASSERT_THAT(IsNotNull(ValueStructParamProperty,
			TEXT("AcceptStructValue parameter should reflect as FStructProperty")));
		if (ValueParamSlot == nullptr || ValueStructParamProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(LastValueProperty->Struct, ValueStructParamProperty->Struct,
			TEXT("AcceptStructValue should use the generated AS struct type")));
		if (ValueStructParamProperty->Struct != LastValueProperty->Struct)
		{
			return;
		}

		CountProperty->SetPropertyValue_InContainer(ValueParamSlot, 10);
		LabelProperty->SetPropertyValue_InContainer(ValueParamSlot, FString(TEXT("Incoming")));
		ASSERT_THAT(IsTrue(ValueInvoker.Call(), TEXT("AcceptStructValue should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastValue.Count"), 11,
			TEXT("Value UFUNCTION struct parameter should cross the runtime call boundary"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastValue.Label"), FString(TEXT("Incoming_Value")),
			TEXT("Value UFUNCTION struct string field should cross the runtime call boundary"))));

		FFunctionInvoker ConstRefInvoker(*TestRunner, Actor, TEXT("AcceptStructConstRef"));
		ASSERT_THAT(IsTrue(ConstRefInvoker.IsValid(), TEXT("AcceptStructConstRef should be invokable")));
		if (!ConstRefInvoker.IsValid())
		{
			return;
		}

		FProperty* ConstRefParamProperty = nullptr;
		void* ConstRefParamSlot = nullptr;
		ASSERT_THAT(IsTrue(ConstRefInvoker.AddParamSlot(ConstRefParamProperty, ConstRefParamSlot),
			TEXT("AcceptStructConstRef should expose a mutable struct parameter slot")));
		FStructProperty* ConstRefStructParamProperty = CastField<FStructProperty>(ConstRefParamProperty);
		ASSERT_THAT(IsNotNull(ConstRefStructParamProperty,
			TEXT("AcceptStructConstRef parameter should reflect as FStructProperty")));
		if (ConstRefParamSlot == nullptr || ConstRefStructParamProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(LastValueProperty->Struct, ConstRefStructParamProperty->Struct,
			TEXT("AcceptStructConstRef should use the generated AS struct type")));
		if (ConstRefStructParamProperty->Struct != LastValueProperty->Struct)
		{
			return;
		}

		CountProperty->SetPropertyValue_InContainer(ConstRefParamSlot, 20);
		LabelProperty->SetPropertyValue_InContainer(ConstRefParamSlot, FString(TEXT("Borrowed")));
		ASSERT_THAT(IsTrue(ConstRefInvoker.Call(), TEXT("AcceptStructConstRef should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastConstRef.Count"), 22,
			TEXT("const ref UFUNCTION struct parameter should cross the runtime call boundary"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastConstRef.Label"), FString(TEXT("Borrowed_Ref")),
			TEXT("const ref UFUNCTION struct string field should cross the runtime call boundary"))));

		FFunctionInvoker OutInvoker(*TestRunner, Actor, TEXT("FillStructOut"));
		ASSERT_THAT(IsTrue(OutInvoker.IsValid(), TEXT("FillStructOut should be invokable")));
		if (!OutInvoker.IsValid())
		{
			return;
		}

		FProperty* OutParamProperty = nullptr;
		void* OutParamSlot = nullptr;
		ASSERT_THAT(IsTrue(OutInvoker.AddParamSlot(OutParamProperty, OutParamSlot),
			TEXT("FillStructOut should expose a mutable output struct parameter slot")));
		FStructProperty* OutStructParamProperty = CastField<FStructProperty>(OutParamProperty);
		ASSERT_THAT(IsNotNull(OutStructParamProperty,
			TEXT("FillStructOut parameter should reflect as FStructProperty")));
		if (OutParamSlot == nullptr || OutStructParamProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(LastValueProperty->Struct, OutStructParamProperty->Struct,
			TEXT("FillStructOut should use the generated AS struct type")));
		if (OutStructParamProperty->Struct != LastValueProperty->Struct)
		{
			return;
		}
		ASSERT_THAT(IsTrue(OutInvoker.Call(), TEXT("FillStructOut should execute through reflection")));

		ASSERT_THAT(AreEqual(55, CountProperty->GetPropertyValue_InContainer(OutParamSlot),
			TEXT("UFUNCTION &out struct parameter should write back into the caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("OutValue")), LabelProperty->GetPropertyValue_InContainer(OutParamSlot),
			TEXT("UFUNCTION &out struct FString field should write back into the caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastOut.Count"), 55,
			TEXT("Out parameter should also update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastOut.Label"), FString(TEXT("OutValue")),
			TEXT("Out parameter string should also update script-side state"))));

		FFunctionInvoker InoutInvoker(*TestRunner, Actor, TEXT("MutateStructInout"));
		ASSERT_THAT(IsTrue(InoutInvoker.IsValid(), TEXT("MutateStructInout should be invokable")));
		if (!InoutInvoker.IsValid())
		{
			return;
		}

		FProperty* InoutParamProperty = nullptr;
		void* InoutParamSlot = nullptr;
		ASSERT_THAT(IsTrue(InoutInvoker.AddParamSlot(InoutParamProperty, InoutParamSlot),
			TEXT("MutateStructInout should expose a mutable inout struct parameter slot")));
		FStructProperty* InoutStructParamProperty = CastField<FStructProperty>(InoutParamProperty);
		ASSERT_THAT(IsNotNull(InoutStructParamProperty,
			TEXT("MutateStructInout parameter should reflect as FStructProperty")));
		if (InoutParamSlot == nullptr || InoutStructParamProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(LastValueProperty->Struct, InoutStructParamProperty->Struct,
			TEXT("MutateStructInout should use the generated AS struct type")));
		if (InoutStructParamProperty->Struct != LastValueProperty->Struct)
		{
			return;
		}

		CountProperty->SetPropertyValue_InContainer(InoutParamSlot, 70);
		LabelProperty->SetPropertyValue_InContainer(InoutParamSlot, FString(TEXT("Mutable")));
		const int32 InoutReturn = InoutInvoker.CallAndReturn<int32>(0);
		ASSERT_THAT(AreEqual(73, InoutReturn, TEXT("UFUNCTION &inout struct return should see mutated value")));
		ASSERT_THAT(AreEqual(73, CountProperty->GetPropertyValue_InContainer(InoutParamSlot),
			TEXT("UFUNCTION &inout struct parameter should write Count back into the caller buffer")));
		ASSERT_THAT(AreEqual(FString(TEXT("Mutable_Inout")), LabelProperty->GetPropertyValue_InContainer(InoutParamSlot),
			TEXT("UFUNCTION &inout struct parameter should write Label back into the caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastInout.Count"), 73,
			TEXT("Inout parameter should update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastInout.Label"), FString(TEXT("Mutable_Inout")),
			TEXT("Inout parameter string should update script-side state"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT as reflected UFUNCTION return: read AS USTRUCT from the return slot.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructUFunctionReturnInvocation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_UFunctionReturnInvocation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructUFunctionReturnInvocation.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FReturnedStructPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;

				UPROPERTY()
				FVector Location;
			}

			UCLASS()
			class ACoverageStructUFunctionReturnActor : AActor
			{
				UPROPERTY()
				FReturnedStructPayload LastReturned;

				UFUNCTION(BlueprintCallable)
				FReturnedStructPayload MakePayload(int BaseValue, const FString& Label)
				{
					FReturnedStructPayload Payload;
					Payload.Count = BaseValue + 7;
					Payload.Label = Label + "_Returned";
					Payload.Location = FVector(BaseValue, BaseValue + 1, BaseValue + 2);
					LastReturned = Payload;
					return Payload;
				}
			}
			)AS"),
			TEXT("ACoverageStructUFunctionReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct UFUNCTION return actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct UFUNCTION return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UFunction* MakePayloadFunction = ScriptClass->FindFunctionByName(TEXT("MakePayload"));
		ASSERT_THAT(IsNotNull(MakePayloadFunction, TEXT("MakePayload should reflect as a UFunction")));
		if (MakePayloadFunction == nullptr)
		{
			return;
		}

		FStructProperty* ReturnProperty = CastField<FStructProperty>(MakePayloadFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ReturnProperty, TEXT("MakePayload return value should reflect as FStructProperty")));
		if (ReturnProperty == nullptr || ReturnProperty->Struct == nullptr)
		{
			return;
		}

		FIntProperty* CountProperty = FindFProperty<FIntProperty>(ReturnProperty->Struct, TEXT("Count"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(ReturnProperty->Struct, TEXT("Label"));
		FStructProperty* LocationProperty = FindFProperty<FStructProperty>(ReturnProperty->Struct, TEXT("Location"));
		ASSERT_THAT(IsNotNull(CountProperty, TEXT("Returned struct should expose Count")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("Returned struct should expose Label")));
		ASSERT_THAT(IsNotNull(LocationProperty, TEXT("Returned struct should expose Location")));
		if (CountProperty == nullptr || LabelProperty == nullptr || LocationProperty == nullptr)
		{
			return;
		}

		FFunctionInvoker Invoker(*TestRunner, Actor, TEXT("MakePayload"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("MakePayload should be invokable through reflection")));
		if (!Invoker.IsValid())
		{
			return;
		}

		Invoker.AddParam<int32>(35);
		Invoker.AddParam<FString>(FString(TEXT("Payload")));
		ASSERT_THAT(IsTrue(Invoker.Call(), TEXT("MakePayload should execute through FFunctionInvoker")));

		void* ReturnAddress = ReturnProperty->ContainerPtrToValuePtr<void>(Invoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnAddress, TEXT("MakePayload return slot should expose struct memory")));
		if (ReturnAddress == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(42, CountProperty->GetPropertyValue_InContainer(ReturnAddress),
			TEXT("Reflected UFUNCTION AS USTRUCT return should preserve int fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("Payload_Returned")), LabelProperty->GetPropertyValue_InContainer(ReturnAddress),
			TEXT("Reflected UFUNCTION AS USTRUCT return should preserve string fields")));
		const FVector& LocationValue = *LocationProperty->ContainerPtrToValuePtr<FVector>(ReturnAddress);
		ASSERT_THAT(IsTrue(LocationValue.Equals(FVector(35, 36, 37), 0.001),
			TEXT("Reflected UFUNCTION AS USTRUCT return should preserve native struct fields")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastReturned.Count"), 42,
			TEXT("UFUNCTION return path should also update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastReturned.Label"), FString(TEXT("Payload_Returned")),
			TEXT("UFUNCTION return path string state should round-trip"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT delegate parameter: real event argument buffer path.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructDelegateParameterRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_DelegateParameterRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructDelegateParameterRoundTrip.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FDelegateStructPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			delegate int FStructPayloadSignal(FDelegateStructPayload Payload);
			delegate int FStructPayloadConstRefSignal(const FDelegateStructPayload&in Payload);
			delegate void FStructPayloadOutSignal(FDelegateStructPayload&out Payload);
			delegate int FStructPayloadInoutSignal(FDelegateStructPayload&inout Payload);
			delegate FDelegateStructPayload FStructPayloadFactorySignal(int BaseValue);

			UCLASS()
			class ACoverageStructDelegateActor : AActor
			{
				UPROPERTY()
				FStructPayloadSignal Signal;

				UPROPERTY()
				FStructPayloadConstRefSignal ConstRefSignal;

				UPROPERTY()
				FStructPayloadOutSignal OutSignal;

				UPROPERTY()
				FStructPayloadInoutSignal InoutSignal;

				UPROPERTY()
				FStructPayloadFactorySignal FactorySignal;

				UPROPERTY()
				FDelegateStructPayload LastPayload;

				UPROPERTY()
				FDelegateStructPayload LastConstRefPayload;

				UPROPERTY()
				FDelegateStructPayload LastOutPayload;

				UPROPERTY()
				FDelegateStructPayload LastInoutPayload;

				UPROPERTY()
				FDelegateStructPayload ReturnedPayload;

				UPROPERTY()
				int DelegateResult = 0;

				UPROPERTY()
				int ConstRefDelegateResult = 0;

				UPROPERTY()
				int InoutDelegateResult = 0;

				UPROPERTY()
				bool bReturnPayloadPreserved = false;

				UFUNCTION()
				int HandlePayload(FDelegateStructPayload Payload)
				{
					LastPayload = Payload;
					return Payload.Count + Payload.Label.Len();
				}

				UFUNCTION()
				int HandleConstRefPayload(const FDelegateStructPayload&in Payload)
				{
					LastConstRefPayload = Payload;
					return Payload.Count + Payload.Label.Len() + 100;
				}

				UFUNCTION()
				void FillOutPayload(FDelegateStructPayload&out Payload)
				{
					Payload.Count = 41;
					Payload.Label = "OutSignal";
					LastOutPayload = Payload;
				}

				UFUNCTION()
				int MutateInoutPayload(FDelegateStructPayload&inout Payload)
				{
					Payload.Count += 5;
					Payload.Label += "_InoutSignal";
					LastInoutPayload = Payload;
					return Payload.Count + Payload.Label.Len();
				}

				UFUNCTION()
				FDelegateStructPayload MakePayload(int BaseValue)
				{
					FDelegateStructPayload Payload;
					Payload.Count = BaseValue + 7;
					Payload.Label = "Factory";
					return Payload;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FDelegateStructPayload Payload;
					Payload.Count = 31;
					Payload.Label = "Signal";

					Signal.BindUFunction(this, n"HandlePayload");
					DelegateResult = Signal.Execute(Payload);

					ConstRefSignal.BindUFunction(this, n"HandleConstRefPayload");
					ConstRefDelegateResult = ConstRefSignal.Execute(Payload);

					OutSignal.BindUFunction(this, n"FillOutPayload");
					FDelegateStructPayload OutPayload;
					OutSignal.Execute(OutPayload);

					InoutSignal.BindUFunction(this, n"MutateInoutPayload");
					FDelegateStructPayload InoutPayload;
					InoutPayload.Count = 45;
					InoutPayload.Label = "Signal";
					InoutDelegateResult = InoutSignal.Execute(InoutPayload);

					FactorySignal.BindUFunction(this, n"MakePayload");
					ReturnedPayload = FactorySignal.Execute(50);
					bReturnPayloadPreserved =
						ReturnedPayload.Count == 57
						&& ReturnedPayload.Label == "Factory";
				}
			}
			)AS"),
			TEXT("ACoverageStructDelegateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct delegate parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FDelegateProperty* SignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("Signal"));
		FDelegateProperty* ConstRefSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("ConstRefSignal"));
		FDelegateProperty* OutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("OutSignal"));
		FDelegateProperty* InoutSignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("InoutSignal"));
		FDelegateProperty* FactorySignalProperty = FindFProperty<FDelegateProperty>(ScriptClass, TEXT("FactorySignal"));
		ASSERT_THAT(IsNotNull(SignalProperty, TEXT("AS delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(ConstRefSignalProperty, TEXT("AS const-ref delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(OutSignalProperty, TEXT("AS out delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(InoutSignalProperty, TEXT("AS inout delegate member should reflect as FDelegateProperty")));
		ASSERT_THAT(IsNotNull(FactorySignalProperty, TEXT("AS struct-return delegate member should reflect as FDelegateProperty")));
		if (SignalProperty == nullptr || SignalProperty->SignatureFunction == nullptr
			|| ConstRefSignalProperty == nullptr || ConstRefSignalProperty->SignatureFunction == nullptr
			|| OutSignalProperty == nullptr || OutSignalProperty->SignatureFunction == nullptr
			|| InoutSignalProperty == nullptr || InoutSignalProperty->SignatureFunction == nullptr
			|| FactorySignalProperty == nullptr || FactorySignalProperty->SignatureFunction == nullptr)
		{
			return;
		}

		FStructProperty* PayloadParameter = FindFProperty<FStructProperty>(SignalProperty->SignatureFunction, TEXT("Payload"));
		FStructProperty* ConstRefPayloadParameter = FindFProperty<FStructProperty>(ConstRefSignalProperty->SignatureFunction, TEXT("Payload"));
		FStructProperty* OutPayloadParameter = FindFProperty<FStructProperty>(OutSignalProperty->SignatureFunction, TEXT("Payload"));
		FStructProperty* InoutPayloadParameter = FindFProperty<FStructProperty>(InoutSignalProperty->SignatureFunction, TEXT("Payload"));
		FStructProperty* FactoryReturnProperty = CastField<FStructProperty>(FactorySignalProperty->SignatureFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(PayloadParameter, TEXT("Delegate signature should expose AS USTRUCT payload parameter")));
		ASSERT_THAT(IsNotNull(ConstRefPayloadParameter, TEXT("Const-ref delegate signature should expose AS USTRUCT payload parameter")));
		ASSERT_THAT(IsNotNull(OutPayloadParameter, TEXT("Out delegate signature should expose AS USTRUCT payload parameter")));
		ASSERT_THAT(IsNotNull(InoutPayloadParameter, TEXT("Inout delegate signature should expose AS USTRUCT payload parameter")));
		ASSERT_THAT(IsNotNull(FactoryReturnProperty, TEXT("Struct-return delegate signature should expose AS USTRUCT return value")));
		if (PayloadParameter == nullptr || PayloadParameter->Struct == nullptr
			|| ConstRefPayloadParameter == nullptr || ConstRefPayloadParameter->Struct == nullptr
			|| OutPayloadParameter == nullptr || OutPayloadParameter->Struct == nullptr
			|| InoutPayloadParameter == nullptr || InoutPayloadParameter->Struct == nullptr
			|| FactoryReturnProperty == nullptr || FactoryReturnProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(PayloadParameter->HasAnyPropertyFlags(CPF_Parm),
			TEXT("Delegate AS USTRUCT payload should be a reflected parameter")));
		ASSERT_THAT(IsTrue(ConstRefPayloadParameter->HasAllPropertyFlags(CPF_ConstParm | CPF_OutParm),
			TEXT("Delegate AS USTRUCT const-ref payload should reflect as const out parameter metadata")));
		ASSERT_THAT(IsTrue(OutPayloadParameter->HasAnyPropertyFlags(CPF_OutParm),
			TEXT("Delegate AS USTRUCT out payload should reflect as CPF_OutParm")));
		ASSERT_THAT(IsTrue(InoutPayloadParameter->HasAllPropertyFlags(CPF_ReferenceParm | CPF_OutParm),
			TEXT("Delegate AS USTRUCT inout payload should reflect as reference out parameter")));
		ASSERT_THAT(IsTrue(FactoryReturnProperty->HasAnyPropertyFlags(CPF_ReturnParm),
			TEXT("Delegate AS USTRUCT return payload should carry CPF_ReturnParm")));
		ASSERT_THAT(AreEqual(PayloadParameter->Struct, ConstRefPayloadParameter->Struct,
			TEXT("Delegate value and const-ref payloads should share the same generated AS USTRUCT")));
		ASSERT_THAT(AreEqual(PayloadParameter->Struct, OutPayloadParameter->Struct,
			TEXT("Delegate value and out payloads should share the same generated AS USTRUCT")));
		ASSERT_THAT(AreEqual(PayloadParameter->Struct, InoutPayloadParameter->Struct,
			TEXT("Delegate value and inout payloads should share the same generated AS USTRUCT")));
		ASSERT_THAT(AreEqual(PayloadParameter->Struct, FactoryReturnProperty->Struct,
			TEXT("Delegate payload parameter and return should share the same generated AS USTRUCT")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct delegate parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("DelegateResult"), 37,
			TEXT("Delegate execution should return a value computed from AS USTRUCT fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ConstRefDelegateResult"), 137,
			TEXT("Const-ref delegate execution should return a value computed from AS USTRUCT fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastPayload.Count"), 31,
			TEXT("Delegate AS USTRUCT payload should cross the event argument buffer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastPayload.Label"), FString(TEXT("Signal")),
			TEXT("Delegate AS USTRUCT FString field should cross the event argument buffer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastConstRefPayload.Count"), 31,
			TEXT("Const-ref delegate AS USTRUCT payload should cross the event argument buffer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastConstRefPayload.Label"), FString(TEXT("Signal")),
			TEXT("Const-ref delegate AS USTRUCT FString field should cross the event argument buffer"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastOutPayload.Count"), 41,
			TEXT("Out delegate AS USTRUCT payload should write int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastOutPayload.Label"), FString(TEXT("OutSignal")),
			TEXT("Out delegate AS USTRUCT payload should write string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastInoutPayload.Count"), 50,
			TEXT("Inout delegate AS USTRUCT payload should mutate int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastInoutPayload.Label"), FString(TEXT("Signal_InoutSignal")),
			TEXT("Inout delegate AS USTRUCT payload should mutate string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InoutDelegateResult"), 68,
			TEXT("Inout delegate execution should return a value computed after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReturnedPayload.Count"), 57,
			TEXT("Delegate AS USTRUCT return should preserve int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReturnedPayload.Label"), FString(TEXT("Factory")),
			TEXT("Delegate AS USTRUCT return should preserve string fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bReturnPayloadPreserved"), true,
			TEXT("Delegate AS USTRUCT return should be visible to the AS caller"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT container delegates: array/map/set through value, in, out, inout, return paths.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructDelegateContainerRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_DelegateContainerRoundTrip"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FDelegateContainerStruct
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FDelegateContainerStruct& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 977) + Tag.GetHash();
				}
			}

			delegate int FStructArrayValueSignal(TArray<FDelegateContainerStruct> Items);
			delegate int FStructArrayInSignal(const TArray<FDelegateContainerStruct>&in Items);
			delegate void FStructArrayOutSignal(TArray<FDelegateContainerStruct>&out Items);
			delegate int FStructArrayInoutSignal(TArray<FDelegateContainerStruct>&inout Items);
			delegate TArray<FDelegateContainerStruct> FStructArrayReturnSignal();

			delegate int FStructMapValueSignal(TMap<int, FDelegateContainerStruct> Items);
			delegate int FStructMapInSignal(const TMap<int, FDelegateContainerStruct>&in Items);
			delegate void FStructMapOutSignal(TMap<int, FDelegateContainerStruct>&out Items);
			delegate int FStructMapInoutSignal(TMap<int, FDelegateContainerStruct>&inout Items);
			delegate TMap<int, FDelegateContainerStruct> FStructMapReturnSignal();
			)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

			delegate int FStructKeyMapValueSignal(TMap<FDelegateContainerStruct, int> Items);
			delegate int FStructKeyMapInSignal(const TMap<FDelegateContainerStruct, int>&in Items);
			delegate void FStructKeyMapOutSignal(TMap<FDelegateContainerStruct, int>&out Items);
			delegate int FStructKeyMapInoutSignal(TMap<FDelegateContainerStruct, int>&inout Items);
			delegate TMap<FDelegateContainerStruct, int> FStructKeyMapReturnSignal();

			delegate int FStructStructMapValueSignal(TMap<FDelegateContainerStruct, FDelegateContainerStruct> Items);
			delegate int FStructStructMapInSignal(const TMap<FDelegateContainerStruct, FDelegateContainerStruct>&in Items);
			delegate void FStructStructMapOutSignal(TMap<FDelegateContainerStruct, FDelegateContainerStruct>&out Items);
			delegate int FStructStructMapInoutSignal(TMap<FDelegateContainerStruct, FDelegateContainerStruct>&inout Items);
			delegate TMap<FDelegateContainerStruct, FDelegateContainerStruct> FStructStructMapReturnSignal();

			delegate int FStructSetValueSignal(TSet<FDelegateContainerStruct> Items);
			delegate int FStructSetInSignal(const TSet<FDelegateContainerStruct>&in Items);
			delegate void FStructSetOutSignal(TSet<FDelegateContainerStruct>&out Items);
			delegate int FStructSetInoutSignal(TSet<FDelegateContainerStruct>&inout Items);
			delegate TSet<FDelegateContainerStruct> FStructSetReturnSignal();

			UCLASS()
			class ACoverageStructDelegateContainerActor : AActor
			{
				UPROPERTY()
				FStructArrayValueSignal ArrayValueSignal;

				UPROPERTY()
				FStructArrayInSignal ArrayInSignal;

				UPROPERTY()
				FStructArrayOutSignal ArrayOutSignal;

				UPROPERTY()
				FStructArrayInoutSignal ArrayInoutSignal;

				UPROPERTY()
				FStructArrayReturnSignal ArrayReturnSignal;

				UPROPERTY()
				FStructMapValueSignal MapValueSignal;

				UPROPERTY()
				FStructMapInSignal MapInSignal;

				UPROPERTY()
				FStructMapOutSignal MapOutSignal;

				UPROPERTY()
				FStructMapInoutSignal MapInoutSignal;

				UPROPERTY()
				FStructMapReturnSignal MapReturnSignal;

				UPROPERTY()
				FStructKeyMapValueSignal KeyMapValueSignal;

				UPROPERTY()
				FStructKeyMapInSignal KeyMapInSignal;

				UPROPERTY()
				FStructKeyMapOutSignal KeyMapOutSignal;

				UPROPERTY()
				FStructKeyMapInoutSignal KeyMapInoutSignal;

				UPROPERTY()
				FStructKeyMapReturnSignal KeyMapReturnSignal;

				UPROPERTY()
				FStructStructMapValueSignal StructMapValueSignal;

				UPROPERTY()
				FStructStructMapInSignal StructMapInSignal;

				UPROPERTY()
				FStructStructMapOutSignal StructMapOutSignal;

				UPROPERTY()
				FStructStructMapInoutSignal StructMapInoutSignal;

				UPROPERTY()
				FStructStructMapReturnSignal StructMapReturnSignal;

				UPROPERTY()
				FStructSetValueSignal SetValueSignal;

				UPROPERTY()
				FStructSetInSignal SetInSignal;

				UPROPERTY()
				FStructSetOutSignal SetOutSignal;

				UPROPERTY()
				FStructSetInoutSignal SetInoutSignal;

				UPROPERTY()
				FStructSetReturnSignal SetReturnSignal;

				UPROPERTY()
				int ArrayValueResult = 0;

				UPROPERTY()
				int ArrayInResult = 0;

				UPROPERTY()
				int ArrayInoutResult = 0;

				UPROPERTY()
				TArray<FDelegateContainerStruct> ArrayOutResult;

				UPROPERTY()
				TArray<FDelegateContainerStruct> ArrayInoutResultItems;

				UPROPERTY()
				TArray<FDelegateContainerStruct> ArrayReturnResult;

				UPROPERTY()
				int MapValueResult = 0;

				UPROPERTY()
				int MapInResult = 0;

				UPROPERTY()
				int MapInoutResult = 0;

				UPROPERTY()
				TMap<int, FDelegateContainerStruct> MapOutResult;

				UPROPERTY()
				TMap<int, FDelegateContainerStruct> MapInoutResultItems;

				UPROPERTY()
				TMap<int, FDelegateContainerStruct> MapReturnResult;

				UPROPERTY()
				int KeyMapValueResult = 0;

				UPROPERTY()
				int KeyMapInResult = 0;

				UPROPERTY()
				int KeyMapInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateContainerStruct, int> KeyMapOutResult;

				UPROPERTY()
				TMap<FDelegateContainerStruct, int> KeyMapInoutResultItems;

				UPROPERTY()
				TMap<FDelegateContainerStruct, int> KeyMapReturnResult;

				UPROPERTY()
				int StructMapValueResult = 0;

				UPROPERTY()
				int StructMapInResult = 0;

				UPROPERTY()
				int StructMapInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateContainerStruct, FDelegateContainerStruct> StructMapOutResult;

				UPROPERTY()
				TMap<FDelegateContainerStruct, FDelegateContainerStruct> StructMapInoutResultItems;

				UPROPERTY()
				TMap<FDelegateContainerStruct, FDelegateContainerStruct> StructMapReturnResult;

				UPROPERTY()
				int SetValueResult = 0;

				UPROPERTY()
				int SetInResult = 0;

				UPROPERTY()
				int SetInoutResult = 0;

				UPROPERTY()
				TSet<FDelegateContainerStruct> SetOutResult;

				UPROPERTY()
				TSet<FDelegateContainerStruct> SetInoutResultItems;

				UPROPERTY()
				TSet<FDelegateContainerStruct> SetReturnResult;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UPROPERTY()
				bool bArrayValuePreserved = false;

				UPROPERTY()
				bool bArrayInPreserved = false;

				UPROPERTY()
				bool bMapValuePreserved = false;

				UPROPERTY()
				bool bMapInPreserved = false;

				UPROPERTY()
				bool bKeyMapValuePreserved = false;

				UPROPERTY()
				bool bKeyMapInPreserved = false;

				UPROPERTY()
				bool bKeyMapOutPreserved = false;

				UPROPERTY()
				bool bKeyMapInoutPreserved = false;

				UPROPERTY()
				bool bKeyMapReturnPreserved = false;

				UPROPERTY()
				bool bStructMapValuePreserved = false;

				UPROPERTY()
				bool bStructMapInPreserved = false;

				UPROPERTY()
				bool bStructMapOutPreserved = false;

				UPROPERTY()
				bool bStructMapInoutPreserved = false;

				UPROPERTY()
				bool bStructMapReturnPreserved = false;

				UPROPERTY()
				bool bSetValuePreserved = false;

				UPROPERTY()
				bool bSetInPreserved = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				FDelegateContainerStruct MakeItem(int ID, FName Tag)
				{
					FDelegateContainerStruct Item;
					Item.ID = ID;
					Item.Tag = Tag;
					return Item;
				}

				UFUNCTION()
				int HandleArrayValue(TArray<FDelegateContainerStruct> Items)
				{
					bArrayValuePreserved = Items.Num() == 2 && Items[1].ID == 11 && Items[1].Tag == n"ArrayValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleArrayIn(const TArray<FDelegateContainerStruct>&in Items)
				{
					bArrayInPreserved = Items.Num() == 2 && Items[0].ID == 12 && Items[0].Tag == n"ArrayInA";
					return Items.Num() + 10;
				}

				UFUNCTION()
				void HandleArrayOut(TArray<FDelegateContainerStruct>&out Items)
				{
					Items.Add(MakeItem(20, n"ArrayOutA"));
					Items.Add(MakeItem(21, n"ArrayOutB"));
				}

				UFUNCTION()
				int HandleArrayInout(TArray<FDelegateContainerStruct>&inout Items)
				{
					FDelegateContainerStruct First = Items[0];
					First.ID += 100;
					First.Tag = n"ArrayInoutMutated";
					Items[0] = First;
					Items.Add(MakeItem(22, n"ArrayInoutAdded"));
					return Items.Num() + Items[0].ID;
				}

				UFUNCTION()
				TArray<FDelegateContainerStruct> HandleArrayReturn()
				{
					TArray<FDelegateContainerStruct> Items;
					Items.Add(MakeItem(30, n"ArrayReturnA"));
					Items.Add(MakeItem(31, n"ArrayReturnB"));
					return Items;
				}

				UFUNCTION()
				int HandleMapValue(TMap<int, FDelegateContainerStruct> Items)
				{
					FDelegateContainerStruct Found;
					bMapValuePreserved = Items.Find(11, Found) && Found.ID == 11 && Found.Tag == n"MapValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleMapIn(const TMap<int, FDelegateContainerStruct>&in Items)
				{
					FDelegateContainerStruct Found;
					bMapInPreserved = Items.Find(12, Found) && Found.ID == 12 && Found.Tag == n"MapInA";
					return Items.Num() + 20;
				}

				UFUNCTION()
				void HandleMapOut(TMap<int, FDelegateContainerStruct>&out Items)
				{
					Items.Add(20, MakeItem(20, n"MapOutA"));
					Items.Add(21, MakeItem(21, n"MapOutB"));
				}

				UFUNCTION()
				int HandleMapInout(TMap<int, FDelegateContainerStruct>&inout Items)
				{
					Items[10] = MakeItem(110, n"MapInoutMutated");
					Items.Add(22, MakeItem(22, n"MapInoutAdded"));
					return Items.Num() + Items[10].ID;
				}

				UFUNCTION()
				TMap<int, FDelegateContainerStruct> HandleMapReturn()
				{
					TMap<int, FDelegateContainerStruct> Items;
					Items.Add(30, MakeItem(30, n"MapReturnA"));
					Items.Add(31, MakeItem(31, n"MapReturnB"));
					return Items;
				}

				UFUNCTION()
				int HandleKeyMapValue(TMap<FDelegateContainerStruct, int> Items)
				{
					int Found = 0;
					bKeyMapValuePreserved = Items.Find(MakeItem(41, n"KeyMapValueB"), Found) && Found == 141;
					return Items.Num();
				}

				UFUNCTION()
				int HandleKeyMapIn(const TMap<FDelegateContainerStruct, int>&in Items)
				{
					int Found = 0;
					bKeyMapInPreserved = Items.Find(MakeItem(42, n"KeyMapInA"), Found) && Found == 142;
					return Items.Num() + 40;
				}

				UFUNCTION()
				void HandleKeyMapOut(TMap<FDelegateContainerStruct, int>&out Items)
				{
					Items.Add(MakeItem(50, n"KeyMapOutA"), 150);
					Items.Add(MakeItem(51, n"KeyMapOutB"), 151);
				}

				UFUNCTION()
				int HandleKeyMapInout(TMap<FDelegateContainerStruct, int>&inout Items)
				{
					FDelegateContainerStruct Existing = MakeItem(52, n"KeyMapInoutA");
					Items.Remove(Existing);
					Items.Add(Existing, 252);
					Items.Add(MakeItem(53, n"KeyMapInoutB"), 153);

					int Found = 0;
					bKeyMapInoutPreserved = Items.Find(Existing, Found) && Found == 252;
					return Items.Num() + Found;
				}

				UFUNCTION()
				TMap<FDelegateContainerStruct, int> HandleKeyMapReturn()
				{
					TMap<FDelegateContainerStruct, int> Items;
					Items.Add(MakeItem(54, n"KeyMapReturnA"), 154);
					Items.Add(MakeItem(55, n"KeyMapReturnB"), 155);
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION()
				int HandleStructMapValue(TMap<FDelegateContainerStruct, FDelegateContainerStruct> Items)
				{
					FDelegateContainerStruct Found;
					bStructMapValuePreserved =
						Items.Find(MakeItem(61, n"StructMapValueKeyB"), Found)
						&& Found.ID == 161
						&& Found.Tag == n"StructMapValueValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleStructMapIn(const TMap<FDelegateContainerStruct, FDelegateContainerStruct>&in Items)
				{
					FDelegateContainerStruct Found;
					bStructMapInPreserved =
						Items.Find(MakeItem(62, n"StructMapInKeyA"), Found)
						&& Found.ID == 162
						&& Found.Tag == n"StructMapInValueA";
					return Items.Num() + 60;
				}

				UFUNCTION()
				void HandleStructMapOut(TMap<FDelegateContainerStruct, FDelegateContainerStruct>&out Items)
				{
					Items.Add(MakeItem(70, n"StructMapOutKeyA"), MakeItem(170, n"StructMapOutValueA"));
					Items.Add(MakeItem(71, n"StructMapOutKeyB"), MakeItem(171, n"StructMapOutValueB"));
				}

				UFUNCTION()
				int HandleStructMapInout(TMap<FDelegateContainerStruct, FDelegateContainerStruct>&inout Items)
				{
					FDelegateContainerStruct Existing = MakeItem(72, n"StructMapInoutKeyA");
					Items.Remove(Existing);
					Items.Add(Existing, MakeItem(272, n"StructMapInoutMutated"));
					Items.Add(MakeItem(73, n"StructMapInoutKeyB"), MakeItem(173, n"StructMapInoutAdded"));

					FDelegateContainerStruct Found;
					bStructMapInoutPreserved =
						Items.Find(Existing, Found)
						&& Found.ID == 272
						&& Found.Tag == n"StructMapInoutMutated";
					return Items.Num() + Found.ID;
				}

				UFUNCTION()
				TMap<FDelegateContainerStruct, FDelegateContainerStruct> HandleStructMapReturn()
				{
					TMap<FDelegateContainerStruct, FDelegateContainerStruct> Items;
					Items.Add(MakeItem(74, n"StructMapReturnKeyA"), MakeItem(174, n"StructMapReturnValueA"));
					Items.Add(MakeItem(75, n"StructMapReturnKeyB"), MakeItem(175, n"StructMapReturnValueB"));
					return Items;
				}

				UFUNCTION()
				int HandleSetValue(TSet<FDelegateContainerStruct> Items)
				{
					bSetValuePreserved = Items.Contains(MakeItem(11, n"SetValueB"));
					return Items.Num();
				}

				UFUNCTION()
				int HandleSetIn(const TSet<FDelegateContainerStruct>&in Items)
				{
					bSetInPreserved = Items.Contains(MakeItem(12, n"SetInA"));
					return Items.Num() + 30;
				}

				UFUNCTION()
				void HandleSetOut(TSet<FDelegateContainerStruct>&out Items)
				{
					Items.Add(MakeItem(20, n"SetOutA"));
					Items.Add(MakeItem(21, n"SetOutB"));
				}

				UFUNCTION()
				int HandleSetInout(TSet<FDelegateContainerStruct>&inout Items)
				{
					Items.Remove(MakeItem(10, n"SetInoutA"));
					Items.Add(MakeItem(22, n"SetInoutAdded"));
					return Items.Num() + 40;
				}

				UFUNCTION()
				TSet<FDelegateContainerStruct> HandleSetReturn()
				{
					TSet<FDelegateContainerStruct> Items;
					Items.Add(MakeItem(30, n"SetReturnA"));
					Items.Add(MakeItem(31, n"SetReturnB"));
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					ArrayValueSignal.BindUFunction(this, n"HandleArrayValue");
					ArrayInSignal.BindUFunction(this, n"HandleArrayIn");
					ArrayOutSignal.BindUFunction(this, n"HandleArrayOut");
					ArrayInoutSignal.BindUFunction(this, n"HandleArrayInout");
					ArrayReturnSignal.BindUFunction(this, n"HandleArrayReturn");

					MapValueSignal.BindUFunction(this, n"HandleMapValue");
					MapInSignal.BindUFunction(this, n"HandleMapIn");
					MapOutSignal.BindUFunction(this, n"HandleMapOut");
					MapInoutSignal.BindUFunction(this, n"HandleMapInout");
					MapReturnSignal.BindUFunction(this, n"HandleMapReturn");

					KeyMapValueSignal.BindUFunction(this, n"HandleKeyMapValue");
					KeyMapInSignal.BindUFunction(this, n"HandleKeyMapIn");
					KeyMapOutSignal.BindUFunction(this, n"HandleKeyMapOut");
					KeyMapInoutSignal.BindUFunction(this, n"HandleKeyMapInout");
					KeyMapReturnSignal.BindUFunction(this, n"HandleKeyMapReturn");

					StructMapValueSignal.BindUFunction(this, n"HandleStructMapValue");
					StructMapInSignal.BindUFunction(this, n"HandleStructMapIn");
					StructMapOutSignal.BindUFunction(this, n"HandleStructMapOut");
					StructMapInoutSignal.BindUFunction(this, n"HandleStructMapInout");
					StructMapReturnSignal.BindUFunction(this, n"HandleStructMapReturn");

					SetValueSignal.BindUFunction(this, n"HandleSetValue");
					SetInSignal.BindUFunction(this, n"HandleSetIn");
					SetOutSignal.BindUFunction(this, n"HandleSetOut");
					SetInoutSignal.BindUFunction(this, n"HandleSetInout");
					SetReturnSignal.BindUFunction(this, n"HandleSetReturn");

					TArray<FDelegateContainerStruct> ArrayValueItems;
					ArrayValueItems.Add(MakeItem(10, n"ArrayValueA"));
					ArrayValueItems.Add(MakeItem(11, n"ArrayValueB"));
					ArrayValueResult = ArrayValueSignal.Execute(ArrayValueItems);

					TArray<FDelegateContainerStruct> ArrayInItems;
					ArrayInItems.Add(MakeItem(12, n"ArrayInA"));
					ArrayInItems.Add(MakeItem(13, n"ArrayInB"));
					ArrayInResult = ArrayInSignal.Execute(ArrayInItems);

					ArrayOutSignal.Execute(ArrayOutResult);

					ArrayInoutResultItems.Add(MakeItem(10, n"ArrayInoutA"));
					ArrayInoutResult = ArrayInoutSignal.Execute(ArrayInoutResultItems);

					ArrayReturnResult = ArrayReturnSignal.Execute();

					TMap<int, FDelegateContainerStruct> MapValueItems;
					MapValueItems.Add(10, MakeItem(10, n"MapValueA"));
					MapValueItems.Add(11, MakeItem(11, n"MapValueB"));
					MapValueResult = MapValueSignal.Execute(MapValueItems);

					TMap<int, FDelegateContainerStruct> MapInItems;
					MapInItems.Add(12, MakeItem(12, n"MapInA"));
					MapInItems.Add(13, MakeItem(13, n"MapInB"));
					MapInResult = MapInSignal.Execute(MapInItems);

					MapOutSignal.Execute(MapOutResult);

					MapInoutResultItems.Add(10, MakeItem(10, n"MapInoutA"));
					MapInoutResult = MapInoutSignal.Execute(MapInoutResultItems);

					MapReturnResult = MapReturnSignal.Execute();

					TMap<FDelegateContainerStruct, int> KeyMapValueItems;
					KeyMapValueItems.Add(MakeItem(40, n"KeyMapValueA"), 140);
					KeyMapValueItems.Add(MakeItem(41, n"KeyMapValueB"), 141);
					KeyMapValueResult = KeyMapValueSignal.Execute(KeyMapValueItems);

					TMap<FDelegateContainerStruct, int> KeyMapInItems;
					KeyMapInItems.Add(MakeItem(42, n"KeyMapInA"), 142);
					KeyMapInItems.Add(MakeItem(43, n"KeyMapInB"), 143);
					KeyMapInResult = KeyMapInSignal.Execute(KeyMapInItems);

					KeyMapOutSignal.Execute(KeyMapOutResult);
					int KeyMapOutFound = 0;
					bKeyMapOutPreserved =
						KeyMapOutResult.Find(MakeItem(51, n"KeyMapOutB"), KeyMapOutFound)
						&& KeyMapOutFound == 151;

					KeyMapInoutResultItems.Add(MakeItem(52, n"KeyMapInoutA"), 152);
					KeyMapInoutResult = KeyMapInoutSignal.Execute(KeyMapInoutResultItems);

					KeyMapReturnResult = KeyMapReturnSignal.Execute();
					int KeyMapReturnFound = 0;
					bKeyMapReturnPreserved =
						KeyMapReturnResult.Find(MakeItem(55, n"KeyMapReturnB"), KeyMapReturnFound)
						&& KeyMapReturnFound == 155;
					)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

					TMap<FDelegateContainerStruct, FDelegateContainerStruct> StructMapValueItems;
					StructMapValueItems.Add(MakeItem(60, n"StructMapValueKeyA"), MakeItem(160, n"StructMapValueValueA"));
					StructMapValueItems.Add(MakeItem(61, n"StructMapValueKeyB"), MakeItem(161, n"StructMapValueValueB"));
					StructMapValueResult = StructMapValueSignal.Execute(StructMapValueItems);

					TMap<FDelegateContainerStruct, FDelegateContainerStruct> StructMapInItems;
					StructMapInItems.Add(MakeItem(62, n"StructMapInKeyA"), MakeItem(162, n"StructMapInValueA"));
					StructMapInItems.Add(MakeItem(63, n"StructMapInKeyB"), MakeItem(163, n"StructMapInValueB"));
					StructMapInResult = StructMapInSignal.Execute(StructMapInItems);

					StructMapOutSignal.Execute(StructMapOutResult);
					FDelegateContainerStruct StructMapOutFound;
					bStructMapOutPreserved =
						StructMapOutResult.Find(MakeItem(71, n"StructMapOutKeyB"), StructMapOutFound)
						&& StructMapOutFound.ID == 171
						&& StructMapOutFound.Tag == n"StructMapOutValueB";

					StructMapInoutResultItems.Add(MakeItem(72, n"StructMapInoutKeyA"), MakeItem(172, n"StructMapInoutOriginal"));
					StructMapInoutResult = StructMapInoutSignal.Execute(StructMapInoutResultItems);

					StructMapReturnResult = StructMapReturnSignal.Execute();
					FDelegateContainerStruct StructMapReturnFound;
					bStructMapReturnPreserved =
						StructMapReturnResult.Find(MakeItem(75, n"StructMapReturnKeyB"), StructMapReturnFound)
						&& StructMapReturnFound.ID == 175
						&& StructMapReturnFound.Tag == n"StructMapReturnValueB";

					TSet<FDelegateContainerStruct> SetValueItems;
					SetValueItems.Add(MakeItem(10, n"SetValueA"));
					SetValueItems.Add(MakeItem(11, n"SetValueB"));
					SetValueResult = SetValueSignal.Execute(SetValueItems);

					TSet<FDelegateContainerStruct> SetInItems;
					SetInItems.Add(MakeItem(12, n"SetInA"));
					SetInItems.Add(MakeItem(13, n"SetInB"));
					SetInResult = SetInSignal.Execute(SetInItems);

					SetOutSignal.Execute(SetOutResult);

					SetInoutResultItems.Add(MakeItem(10, n"SetInoutA"));
					SetInoutResult = SetInoutSignal.Execute(SetInoutResultItems);

					SetReturnResult = SetReturnSignal.Execute();
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructDelegateContainerRoundTrip.as"),
			ScriptSource,
			TEXT("ACoverageStructDelegateContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct delegate container actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FArrayProperty* ArrayValueParameter = nullptr;
		FArrayProperty* ArrayInParameter = nullptr;
		FArrayProperty* ArrayOutParameter = nullptr;
		FArrayProperty* ArrayInoutParameter = nullptr;
		FArrayProperty* ArrayReturnProperty = nullptr;
		FMapProperty* MapValueParameter = nullptr;
		FMapProperty* MapInParameter = nullptr;
		FMapProperty* MapOutParameter = nullptr;
		FMapProperty* MapInoutParameter = nullptr;
		FMapProperty* MapReturnProperty = nullptr;
		FMapProperty* KeyMapValueParameter = nullptr;
		FMapProperty* KeyMapInParameter = nullptr;
		FMapProperty* KeyMapOutParameter = nullptr;
		FMapProperty* KeyMapInoutParameter = nullptr;
		FMapProperty* KeyMapReturnProperty = nullptr;
		FMapProperty* StructMapValueParameter = nullptr;
		FMapProperty* StructMapInParameter = nullptr;
		FMapProperty* StructMapOutParameter = nullptr;
		FMapProperty* StructMapInoutParameter = nullptr;
		FMapProperty* StructMapReturnProperty = nullptr;
		FSetProperty* SetValueParameter = nullptr;
		FSetProperty* SetInParameter = nullptr;
		FSetProperty* SetOutParameter = nullptr;
		FSetProperty* SetInoutParameter = nullptr;
		FSetProperty* SetReturnProperty = nullptr;
		ASSERT_THAT(IsTrue((ExpectDelegateArrayPermutation<FStructProperty>(
			*TestRunner, ScriptClass, TEXT("ArrayValueSignal"), TEXT("ArrayInSignal"), TEXT("ArrayOutSignal"),
			TEXT("ArrayInoutSignal"), TEXT("ArrayReturnSignal"), TEXT("TArray<FStruct>"),
			ArrayValueParameter, ArrayInParameter, ArrayOutParameter, ArrayInoutParameter, ArrayReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FIntProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("MapValueSignal"), TEXT("MapInSignal"), TEXT("MapOutSignal"),
			TEXT("MapInoutSignal"), TEXT("MapReturnSignal"), TEXT("TMap<int,FStruct>"),
			MapValueParameter, MapInParameter, MapOutParameter, MapInoutParameter, MapReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FIntProperty>(
			*TestRunner, ScriptClass, TEXT("KeyMapValueSignal"), TEXT("KeyMapInSignal"), TEXT("KeyMapOutSignal"),
			TEXT("KeyMapInoutSignal"), TEXT("KeyMapReturnSignal"), TEXT("TMap<FStruct,int>"),
			KeyMapValueParameter, KeyMapInParameter, KeyMapOutParameter, KeyMapInoutParameter, KeyMapReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("StructMapValueSignal"), TEXT("StructMapInSignal"), TEXT("StructMapOutSignal"),
			TEXT("StructMapInoutSignal"), TEXT("StructMapReturnSignal"), TEXT("TMap<FStruct,FStruct>"),
			StructMapValueParameter, StructMapInParameter, StructMapOutParameter, StructMapInoutParameter, StructMapReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateSetPermutation<FStructProperty>(
			*TestRunner, ScriptClass, TEXT("SetValueSignal"), TEXT("SetInSignal"), TEXT("SetOutSignal"),
			TEXT("SetInoutSignal"), TEXT("SetReturnSignal"), TEXT("TSet<FStruct>"),
			SetValueParameter, SetInParameter, SetOutParameter, SetInoutParameter, SetReturnProperty))));
		if (ArrayValueParameter == nullptr || ArrayInParameter == nullptr || ArrayOutParameter == nullptr
			|| ArrayInoutParameter == nullptr || ArrayReturnProperty == nullptr
			|| MapValueParameter == nullptr || MapInParameter == nullptr || MapOutParameter == nullptr
			|| MapInoutParameter == nullptr || MapReturnProperty == nullptr
			|| KeyMapValueParameter == nullptr || KeyMapInParameter == nullptr || KeyMapOutParameter == nullptr
			|| KeyMapInoutParameter == nullptr || KeyMapReturnProperty == nullptr
			|| StructMapValueParameter == nullptr || StructMapInParameter == nullptr || StructMapOutParameter == nullptr
			|| StructMapInoutParameter == nullptr || StructMapReturnProperty == nullptr
			|| SetValueParameter == nullptr || SetInParameter == nullptr || SetOutParameter == nullptr
			|| SetInoutParameter == nullptr || SetReturnProperty == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct delegate container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayValueResult"), 2,
			TEXT("TArray<FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayInResult"), 12,
			TEXT("TArray<FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayInoutResult"), 112,
			TEXT("TArray<FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bArrayValuePreserved"), true,
			TEXT("TArray<FStruct> delegate value parameter should preserve struct fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bArrayInPreserved"), true,
			TEXT("TArray<FStruct> delegate const-ref parameter should preserve struct fields"))));
		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayOutResult"), Count),
			TEXT("TArray<FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FStruct> delegate out should write two items")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayOutResult[1].ID"), 21,
			TEXT("TArray<FStruct> delegate out should preserve struct fields"))));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayInoutResultItems"), Count),
			TEXT("TArray<FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FStruct> delegate inout should append one item")));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("ArrayInoutResultItems[0].Tag"), FName(TEXT("ArrayInoutMutated")),
			TEXT("TArray<FStruct> delegate inout should mutate existing item"))));
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayReturnResult"), Count),
			TEXT("TArray<FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FStruct> delegate return should contain two items")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapValueResult"), 2,
			TEXT("TMap<int,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapInResult"), 22,
			TEXT("TMap<int,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapInoutResult"), 112,
			TEXT("TMap<int,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMapValuePreserved"), true,
			TEXT("TMap<int,FStruct> delegate value parameter should preserve struct fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMapInPreserved"), true,
			TEXT("TMap<int,FStruct> delegate const-ref parameter should preserve struct fields"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapOutResult"), Count),
			TEXT("TMap<int,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,FStruct> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapInoutResultItems"), Count),
			TEXT("TMap<int,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,FStruct> delegate inout should add one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapReturnResult"), Count),
			TEXT("TMap<int,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,FStruct> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeyMapValueResult"), 2,
			TEXT("TMap<FStruct,int> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeyMapInResult"), 42,
			TEXT("TMap<FStruct,int> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeyMapInoutResult"), 254,
			TEXT("TMap<FStruct,int> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bKeyMapValuePreserved"), true,
			TEXT("TMap<FStruct,int> delegate value parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bKeyMapInPreserved"), true,
			TEXT("TMap<FStruct,int> delegate const-ref parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bKeyMapOutPreserved"), true,
			TEXT("TMap<FStruct,int> delegate out parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bKeyMapInoutPreserved"), true,
			TEXT("TMap<FStruct,int> delegate inout parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bKeyMapReturnPreserved"), true,
			TEXT("TMap<FStruct,int> delegate return should preserve struct keys"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("KeyMapOutResult"), Count),
			TEXT("TMap<FStruct,int> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,int> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("KeyMapInoutResultItems"), Count),
			TEXT("TMap<FStruct,int> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,int> delegate inout should add one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("KeyMapReturnResult"), Count),
			TEXT("TMap<FStruct,int> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,int> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructMapValueResult"), 2,
			TEXT("TMap<FStruct,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructMapInResult"), 62,
			TEXT("TMap<FStruct,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructMapInoutResult"), 274,
			TEXT("TMap<FStruct,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStructMapValuePreserved"), true,
			TEXT("TMap<FStruct,FStruct> delegate value parameter should preserve struct keys and values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStructMapInPreserved"), true,
			TEXT("TMap<FStruct,FStruct> delegate const-ref parameter should preserve struct keys and values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStructMapOutPreserved"), true,
			TEXT("TMap<FStruct,FStruct> delegate out parameter should preserve struct keys and values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStructMapInoutPreserved"), true,
			TEXT("TMap<FStruct,FStruct> delegate inout parameter should preserve struct keys and values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bStructMapReturnPreserved"), true,
			TEXT("TMap<FStruct,FStruct> delegate return should preserve struct keys and values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructMapOutResult"), Count),
			TEXT("TMap<FStruct,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FStruct> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructMapInoutResultItems"), Count),
			TEXT("TMap<FStruct,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FStruct> delegate inout should add one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructMapReturnResult"), Count),
			TEXT("TMap<FStruct,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FStruct> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetValueResult"), 2,
			TEXT("TSet<FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetInResult"), 32,
			TEXT("TSet<FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetInoutResult"), 41,
			TEXT("TSet<FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetValuePreserved"), true,
			TEXT("TSet<FStruct> delegate value parameter should preserve struct fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetInPreserved"), true,
			TEXT("TSet<FStruct> delegate const-ref parameter should preserve struct fields"))));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetOutResult"), Count),
			TEXT("TSet<FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FStruct> delegate out should write two elements")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetInoutResultItems"), Count),
			TEXT("TSet<FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TSet<FStruct> delegate inout should remove one and add one element")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetReturnResult"), Count),
			TEXT("TSet<FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FStruct> delegate return should contain two elements")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT extended map delegates: bool keys, bool values, and float values
	// through value, in, out, inout, and return paths.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructExtendedMapDelegatePermutationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ExtendedMapDelegatePermutationMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FDelegateExtendedMapKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FDelegateExtendedMapKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 887) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FDelegateExtendedMapValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			delegate int FBoolStructMapValueSignal(TMap<bool, FDelegateExtendedMapValue> Items);
			delegate int FBoolStructMapInSignal(const TMap<bool, FDelegateExtendedMapValue>&in Items);
			delegate void FBoolStructMapOutSignal(TMap<bool, FDelegateExtendedMapValue>&out Items);
			delegate int FBoolStructMapInoutSignal(TMap<bool, FDelegateExtendedMapValue>&inout Items);
			delegate TMap<bool, FDelegateExtendedMapValue> FBoolStructMapReturnSignal();

			delegate int FStructBoolMapValueSignal(TMap<FDelegateExtendedMapKey, bool> Items);
			delegate int FStructBoolMapInSignal(const TMap<FDelegateExtendedMapKey, bool>&in Items);
			delegate void FStructBoolMapOutSignal(TMap<FDelegateExtendedMapKey, bool>&out Items);
			delegate int FStructBoolMapInoutSignal(TMap<FDelegateExtendedMapKey, bool>&inout Items);
			delegate TMap<FDelegateExtendedMapKey, bool> FStructBoolMapReturnSignal();

			delegate int FStructFloatMapValueSignal(TMap<FDelegateExtendedMapKey, float> Items);
			delegate int FStructFloatMapInSignal(const TMap<FDelegateExtendedMapKey, float>&in Items);
			delegate void FStructFloatMapOutSignal(TMap<FDelegateExtendedMapKey, float>&out Items);
			delegate int FStructFloatMapInoutSignal(TMap<FDelegateExtendedMapKey, float>&inout Items);
			delegate TMap<FDelegateExtendedMapKey, float> FStructFloatMapReturnSignal();

			UCLASS()
			class ACoverageStructExtendedMapDelegateActor : AActor
			{
				UPROPERTY()
				FBoolStructMapValueSignal BoolStructValueSignal;

				UPROPERTY()
				FBoolStructMapInSignal BoolStructInSignal;

				UPROPERTY()
				FBoolStructMapOutSignal BoolStructOutSignal;

				UPROPERTY()
				FBoolStructMapInoutSignal BoolStructInoutSignal;

				UPROPERTY()
				FBoolStructMapReturnSignal BoolStructReturnSignal;

				UPROPERTY()
				FStructBoolMapValueSignal StructBoolValueSignal;

				UPROPERTY()
				FStructBoolMapInSignal StructBoolInSignal;

				UPROPERTY()
				FStructBoolMapOutSignal StructBoolOutSignal;

				UPROPERTY()
				FStructBoolMapInoutSignal StructBoolInoutSignal;

				UPROPERTY()
				FStructBoolMapReturnSignal StructBoolReturnSignal;

				UPROPERTY()
				FStructFloatMapValueSignal StructFloatValueSignal;

				UPROPERTY()
				FStructFloatMapInSignal StructFloatInSignal;

				UPROPERTY()
				FStructFloatMapOutSignal StructFloatOutSignal;

				UPROPERTY()
				FStructFloatMapInoutSignal StructFloatInoutSignal;

				UPROPERTY()
				FStructFloatMapReturnSignal StructFloatReturnSignal;

				UPROPERTY()
				int BoolStructValueResult = 0;

				UPROPERTY()
				int BoolStructInResult = 0;

				UPROPERTY()
				int BoolStructInoutResult = 0;

				UPROPERTY()
				TMap<bool, FDelegateExtendedMapValue> BoolStructOutResult;

				UPROPERTY()
				TMap<bool, FDelegateExtendedMapValue> BoolStructInoutResultItems;

				UPROPERTY()
				TMap<bool, FDelegateExtendedMapValue> BoolStructReturnResult;

				UPROPERTY()
				bool BoolStructValuePreserved = false;

				UPROPERTY()
				bool BoolStructInPreserved = false;

				UPROPERTY()
				bool BoolStructOutPreserved = false;

				UPROPERTY()
				bool BoolStructInoutPreserved = false;

				UPROPERTY()
				bool BoolStructReturnPreserved = false;

				UPROPERTY()
				int StructBoolValueResult = 0;

				UPROPERTY()
				int StructBoolInResult = 0;

				UPROPERTY()
				int StructBoolInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateExtendedMapKey, bool> StructBoolOutResult;

				UPROPERTY()
				TMap<FDelegateExtendedMapKey, bool> StructBoolInoutResultItems;

				UPROPERTY()
				TMap<FDelegateExtendedMapKey, bool> StructBoolReturnResult;

				UPROPERTY()
				bool StructBoolValuePreserved = false;

				UPROPERTY()
				bool StructBoolInPreserved = false;

				UPROPERTY()
				bool StructBoolOutPreserved = false;

				UPROPERTY()
				bool StructBoolInoutPreserved = false;

				UPROPERTY()
				bool StructBoolReturnPreserved = false;

				UPROPERTY()
				int StructFloatValueResult = 0;

				UPROPERTY()
				int StructFloatInResult = 0;

				UPROPERTY()
				int StructFloatInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateExtendedMapKey, float> StructFloatOutResult;

				UPROPERTY()
				TMap<FDelegateExtendedMapKey, float> StructFloatInoutResultItems;

				UPROPERTY()
				TMap<FDelegateExtendedMapKey, float> StructFloatReturnResult;

				UPROPERTY()
				bool StructFloatValuePreserved = false;

				UPROPERTY()
				bool StructFloatInPreserved = false;

				UPROPERTY()
				bool StructFloatOutPreserved = false;

				UPROPERTY()
				bool StructFloatInoutPreserved = false;

				UPROPERTY()
				bool StructFloatReturnPreserved = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				FDelegateExtendedMapKey MakeKey(int ID, FName Tag)
				{
					FDelegateExtendedMapKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FDelegateExtendedMapValue MakeValue(int Score, FString Label)
				{
					FDelegateExtendedMapValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UFUNCTION()
				int HandleBoolStructValue(TMap<bool, FDelegateExtendedMapValue> Items)
				{
					FDelegateExtendedMapValue Found;
					BoolStructValuePreserved =
						Items.Find(false, Found)
						&& Found.Score == 102
						&& Found.Label == "BoolValueFalse";
					return Items.Num();
				}

				UFUNCTION()
				int HandleBoolStructIn(const TMap<bool, FDelegateExtendedMapValue>&in Items)
				{
					FDelegateExtendedMapValue Found;
					BoolStructInPreserved =
						Items.Find(true, Found)
						&& Found.Score == 111
						&& Found.Label == "BoolInTrue";
					return Items.Num() + 10;
				}

				UFUNCTION()
				void HandleBoolStructOut(TMap<bool, FDelegateExtendedMapValue>&out Items)
				{
					Items.Add(true, MakeValue(121, "BoolOutTrue"));
					Items.Add(false, MakeValue(122, "BoolOutFalse"));
				}

				UFUNCTION()
				int HandleBoolStructInout(TMap<bool, FDelegateExtendedMapValue>&inout Items)
				{
					FDelegateExtendedMapValue Found;
					if (Items.Find(true, Found))
					{
						Found.Score += 100;
						Found.Label = "BoolInoutMutated";
						Items.Add(true, Found);
					}
					Items.Add(false, MakeValue(132, "BoolInoutAdded"));
					BoolStructInoutResultItems = Items;
					FDelegateExtendedMapValue Mutated;
					BoolStructInoutPreserved =
						Items.Find(true, Mutated)
						&& Mutated.Score == 231
						&& Mutated.Label == "BoolInoutMutated";
					return Items.Num() + Mutated.Score;
				}

				UFUNCTION()
				TMap<bool, FDelegateExtendedMapValue> HandleBoolStructReturn()
				{
					TMap<bool, FDelegateExtendedMapValue> Items;
					Items.Add(true, MakeValue(141, "BoolReturnTrue"));
					Items.Add(false, MakeValue(142, "BoolReturnFalse"));
					return Items;
				}

				UFUNCTION()
				int HandleStructBoolValue(TMap<FDelegateExtendedMapKey, bool> Items)
				{
					bool Found = true;
					StructBoolValuePreserved =
						Items.Find(MakeKey(201, n"StructBoolValueB"), Found)
						&& !Found;
					return Items.Num();
				}

				UFUNCTION()
				int HandleStructBoolIn(const TMap<FDelegateExtendedMapKey, bool>&in Items)
				{
					bool Found = false;
					StructBoolInPreserved =
						Items.Find(MakeKey(211, n"StructBoolInB"), Found)
						&& Found;
					return Items.Num() + 20;
				}

				UFUNCTION()
				void HandleStructBoolOut(TMap<FDelegateExtendedMapKey, bool>&out Items)
				{
					Items.Add(MakeKey(220, n"StructBoolOutA"), true);
					Items.Add(MakeKey(221, n"StructBoolOutB"), false);
				}

				UFUNCTION()
				int HandleStructBoolInout(TMap<FDelegateExtendedMapKey, bool>&inout Items)
				{
					FDelegateExtendedMapKey Existing = MakeKey(230, n"StructBoolInoutA");
					bool Found = false;
					if (Items.Find(Existing, Found))
					{
						Items.Add(Existing, !Found);
					}
					Items.Add(MakeKey(231, n"StructBoolInoutB"), true);
					StructBoolInoutResultItems = Items;
					bool Mutated = false;
					StructBoolInoutPreserved =
						Items.Find(Existing, Mutated)
						&& !Mutated;
					return Items.Num() + (Mutated ? 40 : 50);
				}

				UFUNCTION()
				TMap<FDelegateExtendedMapKey, bool> HandleStructBoolReturn()
				{
					TMap<FDelegateExtendedMapKey, bool> Items;
					Items.Add(MakeKey(240, n"StructBoolReturnA"), true);
					Items.Add(MakeKey(241, n"StructBoolReturnB"), false);
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION()
				int HandleStructFloatValue(TMap<FDelegateExtendedMapKey, float> Items)
				{
					float Found = 0.0f;
					StructFloatValuePreserved =
						Items.Find(MakeKey(301, n"StructFloatValueB"), Found)
						&& Found == 302.5f;
					return Items.Num();
				}

				UFUNCTION()
				int HandleStructFloatIn(const TMap<FDelegateExtendedMapKey, float>&in Items)
				{
					float Found = 0.0f;
					StructFloatInPreserved =
						Items.Find(MakeKey(311, n"StructFloatInB"), Found)
						&& Found == 312.5f;
					return Items.Num() + 30;
				}

				UFUNCTION()
				void HandleStructFloatOut(TMap<FDelegateExtendedMapKey, float>&out Items)
				{
					Items.Add(MakeKey(320, n"StructFloatOutA"), 321.5f);
					Items.Add(MakeKey(321, n"StructFloatOutB"), 322.5f);
				}

				UFUNCTION()
				int HandleStructFloatInout(TMap<FDelegateExtendedMapKey, float>&inout Items)
				{
					FDelegateExtendedMapKey Existing = MakeKey(330, n"StructFloatInoutA");
					float Found = 0.0f;
					if (Items.Find(Existing, Found))
					{
						Items.Add(Existing, Found + 100.0f);
					}
					Items.Add(MakeKey(331, n"StructFloatInoutB"), 332.5f);
					StructFloatInoutResultItems = Items;
					float Mutated = 0.0f;
					StructFloatInoutPreserved =
						Items.Find(Existing, Mutated)
						&& Mutated == 431.5f;
					return Items.Num() + int(Mutated);
				}

				UFUNCTION()
				TMap<FDelegateExtendedMapKey, float> HandleStructFloatReturn()
				{
					TMap<FDelegateExtendedMapKey, float> Items;
					Items.Add(MakeKey(340, n"StructFloatReturnA"), 341.5f);
					Items.Add(MakeKey(341, n"StructFloatReturnB"), 342.5f);
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					BoolStructValueSignal.BindUFunction(this, n"HandleBoolStructValue");
					BoolStructInSignal.BindUFunction(this, n"HandleBoolStructIn");
					BoolStructOutSignal.BindUFunction(this, n"HandleBoolStructOut");
					BoolStructInoutSignal.BindUFunction(this, n"HandleBoolStructInout");
					BoolStructReturnSignal.BindUFunction(this, n"HandleBoolStructReturn");

					TMap<bool, FDelegateExtendedMapValue> BoolStructValueItems;
					BoolStructValueItems.Add(true, MakeValue(101, "BoolValueTrue"));
					BoolStructValueItems.Add(false, MakeValue(102, "BoolValueFalse"));
					BoolStructValueResult = BoolStructValueSignal.Execute(BoolStructValueItems);

					TMap<bool, FDelegateExtendedMapValue> BoolStructInItems;
					BoolStructInItems.Add(true, MakeValue(111, "BoolInTrue"));
					BoolStructInItems.Add(false, MakeValue(112, "BoolInFalse"));
					BoolStructInResult = BoolStructInSignal.Execute(BoolStructInItems);

					BoolStructOutSignal.Execute(BoolStructOutResult);
					FDelegateExtendedMapValue BoolStructOutFound;
					BoolStructOutPreserved =
						BoolStructOutResult.Find(false, BoolStructOutFound)
						&& BoolStructOutFound.Score == 122
						&& BoolStructOutFound.Label == "BoolOutFalse";

					BoolStructInoutResultItems.Add(true, MakeValue(131, "BoolInoutOriginal"));
					BoolStructInoutResult = BoolStructInoutSignal.Execute(BoolStructInoutResultItems);

					BoolStructReturnResult = BoolStructReturnSignal.Execute();
					FDelegateExtendedMapValue BoolStructReturnFound;
					BoolStructReturnPreserved =
						BoolStructReturnResult.Find(false, BoolStructReturnFound)
						&& BoolStructReturnFound.Score == 142
						&& BoolStructReturnFound.Label == "BoolReturnFalse";

					StructBoolValueSignal.BindUFunction(this, n"HandleStructBoolValue");
					StructBoolInSignal.BindUFunction(this, n"HandleStructBoolIn");
					StructBoolOutSignal.BindUFunction(this, n"HandleStructBoolOut");
					StructBoolInoutSignal.BindUFunction(this, n"HandleStructBoolInout");
					StructBoolReturnSignal.BindUFunction(this, n"HandleStructBoolReturn");

					TMap<FDelegateExtendedMapKey, bool> StructBoolValueItems;
					StructBoolValueItems.Add(MakeKey(200, n"StructBoolValueA"), true);
					StructBoolValueItems.Add(MakeKey(201, n"StructBoolValueB"), false);
					StructBoolValueResult = StructBoolValueSignal.Execute(StructBoolValueItems);

					TMap<FDelegateExtendedMapKey, bool> StructBoolInItems;
					StructBoolInItems.Add(MakeKey(210, n"StructBoolInA"), false);
					StructBoolInItems.Add(MakeKey(211, n"StructBoolInB"), true);
					StructBoolInResult = StructBoolInSignal.Execute(StructBoolInItems);

					StructBoolOutSignal.Execute(StructBoolOutResult);
					bool StructBoolOutFound = true;
					StructBoolOutPreserved =
						StructBoolOutResult.Find(MakeKey(221, n"StructBoolOutB"), StructBoolOutFound)
						&& !StructBoolOutFound;

					StructBoolInoutResultItems.Add(MakeKey(230, n"StructBoolInoutA"), true);
					StructBoolInoutResult = StructBoolInoutSignal.Execute(StructBoolInoutResultItems);

					StructBoolReturnResult = StructBoolReturnSignal.Execute();
					bool StructBoolReturnFound = true;
					StructBoolReturnPreserved =
						StructBoolReturnResult.Find(MakeKey(241, n"StructBoolReturnB"), StructBoolReturnFound)
						&& !StructBoolReturnFound;

					StructFloatValueSignal.BindUFunction(this, n"HandleStructFloatValue");
					StructFloatInSignal.BindUFunction(this, n"HandleStructFloatIn");
					StructFloatOutSignal.BindUFunction(this, n"HandleStructFloatOut");
					StructFloatInoutSignal.BindUFunction(this, n"HandleStructFloatInout");
					StructFloatReturnSignal.BindUFunction(this, n"HandleStructFloatReturn");

					TMap<FDelegateExtendedMapKey, float> StructFloatValueItems;
					StructFloatValueItems.Add(MakeKey(300, n"StructFloatValueA"), 301.5f);
					StructFloatValueItems.Add(MakeKey(301, n"StructFloatValueB"), 302.5f);
					StructFloatValueResult = StructFloatValueSignal.Execute(StructFloatValueItems);

					TMap<FDelegateExtendedMapKey, float> StructFloatInItems;
					StructFloatInItems.Add(MakeKey(310, n"StructFloatInA"), 311.5f);
					StructFloatInItems.Add(MakeKey(311, n"StructFloatInB"), 312.5f);
					StructFloatInResult = StructFloatInSignal.Execute(StructFloatInItems);

					StructFloatOutSignal.Execute(StructFloatOutResult);
					float StructFloatOutFound = 0.0f;
					StructFloatOutPreserved =
						StructFloatOutResult.Find(MakeKey(321, n"StructFloatOutB"), StructFloatOutFound)
						&& StructFloatOutFound == 322.5f;

					StructFloatInoutResultItems.Add(MakeKey(330, n"StructFloatInoutA"), 331.5f);
					StructFloatInoutResult = StructFloatInoutSignal.Execute(StructFloatInoutResultItems);

					StructFloatReturnResult = StructFloatReturnSignal.Execute();
					float StructFloatReturnFound = 0.0f;
					StructFloatReturnPreserved =
						StructFloatReturnResult.Find(MakeKey(341, n"StructFloatReturnB"), StructFloatReturnFound)
						&& StructFloatReturnFound == 342.5f;
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructExtendedMapDelegatePermutationMatrix.as"),
			ScriptSource,
			TEXT("ACoverageStructExtendedMapDelegateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct extended map-delegate actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FMapProperty* BoolStructValueParameter = nullptr;
		FMapProperty* BoolStructInParameter = nullptr;
		FMapProperty* BoolStructOutParameter = nullptr;
		FMapProperty* BoolStructInoutParameter = nullptr;
		FMapProperty* BoolStructReturnProperty = nullptr;
		FMapProperty* StructBoolValueParameter = nullptr;
		FMapProperty* StructBoolInParameter = nullptr;
		FMapProperty* StructBoolOutParameter = nullptr;
		FMapProperty* StructBoolInoutParameter = nullptr;
		FMapProperty* StructBoolReturnProperty = nullptr;
		FMapProperty* StructFloatValueParameter = nullptr;
		FMapProperty* StructFloatInParameter = nullptr;
		FMapProperty* StructFloatOutParameter = nullptr;
		FMapProperty* StructFloatInoutParameter = nullptr;
		FMapProperty* StructFloatReturnProperty = nullptr;
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FBoolProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("BoolStructValueSignal"), TEXT("BoolStructInSignal"), TEXT("BoolStructOutSignal"),
			TEXT("BoolStructInoutSignal"), TEXT("BoolStructReturnSignal"), TEXT("TMap<bool,FStruct>"),
			BoolStructValueParameter, BoolStructInParameter, BoolStructOutParameter, BoolStructInoutParameter, BoolStructReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FBoolProperty>(
			*TestRunner, ScriptClass, TEXT("StructBoolValueSignal"), TEXT("StructBoolInSignal"), TEXT("StructBoolOutSignal"),
			TEXT("StructBoolInoutSignal"), TEXT("StructBoolReturnSignal"), TEXT("TMap<FStruct,bool>"),
			StructBoolValueParameter, StructBoolInParameter, StructBoolOutParameter, StructBoolInoutParameter, StructBoolReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FScriptFloatProperty>(
			*TestRunner, ScriptClass, TEXT("StructFloatValueSignal"), TEXT("StructFloatInSignal"), TEXT("StructFloatOutSignal"),
			TEXT("StructFloatInoutSignal"), TEXT("StructFloatReturnSignal"), TEXT("TMap<FStruct,float>"),
			StructFloatValueParameter, StructFloatInParameter, StructFloatOutParameter, StructFloatInoutParameter, StructFloatReturnProperty))));
		if (BoolStructValueParameter == nullptr || BoolStructInParameter == nullptr || BoolStructOutParameter == nullptr
			|| BoolStructInoutParameter == nullptr || BoolStructReturnProperty == nullptr
			|| StructBoolValueParameter == nullptr || StructBoolInParameter == nullptr || StructBoolOutParameter == nullptr
			|| StructBoolInoutParameter == nullptr || StructBoolReturnProperty == nullptr
			|| StructFloatValueParameter == nullptr || StructFloatInParameter == nullptr || StructFloatOutParameter == nullptr
			|| StructFloatInoutParameter == nullptr || StructFloatReturnProperty == nullptr)
		{
			return;
		}

		FStructProperty* BoolStructValueProperty = CastField<FStructProperty>(BoolStructValueParameter->ValueProp);
		FStructProperty* StructBoolKeyProperty = CastField<FStructProperty>(StructBoolValueParameter->KeyProp);
		FStructProperty* StructFloatKeyProperty = CastField<FStructProperty>(StructFloatValueParameter->KeyProp);
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(BoolStructValueParameter->KeyProp),
			TEXT("TMap<bool,FStruct> delegate key should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(BoolStructValueProperty,
			TEXT("TMap<bool,FStruct> delegate value should expose struct values")));
		ASSERT_THAT(IsNotNull(StructBoolKeyProperty,
			TEXT("TMap<FStruct,bool> delegate key should expose struct keys")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(StructBoolValueParameter->ValueProp),
			TEXT("TMap<FStruct,bool> delegate value should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(StructFloatKeyProperty,
			TEXT("TMap<FStruct,float> delegate key should expose struct keys")));
		ASSERT_THAT(IsNotNull(CastField<FScriptFloatProperty>(StructFloatValueParameter->ValueProp),
			TEXT("TMap<FStruct,float> delegate value should use script float storage")));
		if (BoolStructValueProperty == nullptr || BoolStructValueProperty->Struct == nullptr
			|| StructBoolKeyProperty == nullptr || StructBoolKeyProperty->Struct == nullptr
			|| StructFloatKeyProperty == nullptr || StructFloatKeyProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(StructBoolKeyProperty->Struct, StructFloatKeyProperty->Struct,
			TEXT("TMap<FStruct,bool> and TMap<FStruct,float> delegates should reuse the key UScriptStruct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct extended map-delegate actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolStructValueResult"), 2,
			TEXT("TMap<bool,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolStructInResult"), 12,
			TEXT("TMap<bool,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolStructInoutResult"), 233,
			TEXT("TMap<bool,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructValuePreserved"), true,
			TEXT("TMap<bool,FStruct> delegate value parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructInPreserved"), true,
			TEXT("TMap<bool,FStruct> delegate const-ref parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructOutPreserved"), true,
			TEXT("TMap<bool,FStruct> delegate out parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructInoutPreserved"), true,
			TEXT("TMap<bool,FStruct> delegate inout parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructReturnPreserved"), true,
			TEXT("TMap<bool,FStruct> delegate return should preserve struct values"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoolStructOutResult"), Count),
			TEXT("TMap<bool,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<bool,FStruct> delegate out should write true/false entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoolStructInoutResultItems"), Count),
			TEXT("TMap<bool,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<bool,FStruct> delegate inout should contain true/false entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoolStructReturnResult"), Count),
			TEXT("TMap<bool,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<bool,FStruct> delegate return should contain true/false entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructBoolValueResult"), 2,
			TEXT("TMap<FStruct,bool> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructBoolInResult"), 22,
			TEXT("TMap<FStruct,bool> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructBoolInoutResult"), 52,
			TEXT("TMap<FStruct,bool> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolValuePreserved"), true,
			TEXT("TMap<FStruct,bool> delegate value parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolInPreserved"), true,
			TEXT("TMap<FStruct,bool> delegate const-ref parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolOutPreserved"), true,
			TEXT("TMap<FStruct,bool> delegate out should preserve bool values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolInoutPreserved"), true,
			TEXT("TMap<FStruct,bool> delegate inout should preserve bool values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolReturnPreserved"), true,
			TEXT("TMap<FStruct,bool> delegate return should preserve bool values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructBoolOutResult"), Count),
			TEXT("TMap<FStruct,bool> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,bool> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructBoolInoutResultItems"), Count),
			TEXT("TMap<FStruct,bool> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,bool> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructBoolReturnResult"), Count),
			TEXT("TMap<FStruct,bool> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,bool> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructFloatValueResult"), 2,
			TEXT("TMap<FStruct,float> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructFloatInResult"), 32,
			TEXT("TMap<FStruct,float> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructFloatInoutResult"), 433,
			TEXT("TMap<FStruct,float> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatValuePreserved"), true,
			TEXT("TMap<FStruct,float> delegate value parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatInPreserved"), true,
			TEXT("TMap<FStruct,float> delegate const-ref parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatOutPreserved"), true,
			TEXT("TMap<FStruct,float> delegate out should preserve float values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatInoutPreserved"), true,
			TEXT("TMap<FStruct,float> delegate inout should preserve float values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatReturnPreserved"), true,
			TEXT("TMap<FStruct,float> delegate return should preserve float values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructFloatOutResult"), Count),
			TEXT("TMap<FStruct,float> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,float> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructFloatInoutResultItems"), Count),
			TEXT("TMap<FStruct,float> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,float> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructFloatReturnResult"), Count),
			TEXT("TMap<FStruct,float> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,float> delegate return should contain two entries")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT map key/value delegates: FName/FString simple keys and struct keys
	// with string/name/object values through every delegate call shape.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMapKeyValueDelegatePermutationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_MapKeyValueDelegatePermutationMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageStructDelegateMapKeyObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class UCoverageStructDelegateMapValueObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			USTRUCT(BlueprintType)
			struct FDelegateKeyValueMapKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FDelegateKeyValueMapKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 929) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FDelegateKeyValueMapValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}
			)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

			delegate int FNameStructMapValueSignal(TMap<FName, FDelegateKeyValueMapValue> Items);
			delegate int FNameStructMapInSignal(const TMap<FName, FDelegateKeyValueMapValue>&in Items);
			delegate void FNameStructMapOutSignal(TMap<FName, FDelegateKeyValueMapValue>&out Items);
			delegate int FNameStructMapInoutSignal(TMap<FName, FDelegateKeyValueMapValue>&inout Items);
			delegate TMap<FName, FDelegateKeyValueMapValue> FNameStructMapReturnSignal();

			delegate int FStringStructMapValueSignal(TMap<FString, FDelegateKeyValueMapValue> Items);
			delegate int FStringStructMapInSignal(const TMap<FString, FDelegateKeyValueMapValue>&in Items);
			delegate void FStringStructMapOutSignal(TMap<FString, FDelegateKeyValueMapValue>&out Items);
			delegate int FStringStructMapInoutSignal(TMap<FString, FDelegateKeyValueMapValue>&inout Items);
			delegate TMap<FString, FDelegateKeyValueMapValue> FStringStructMapReturnSignal();

			delegate int FFloatStructMapValueSignal(TMap<float, FDelegateKeyValueMapValue> Items);
			delegate int FFloatStructMapInSignal(const TMap<float, FDelegateKeyValueMapValue>&in Items);
			delegate void FFloatStructMapOutSignal(TMap<float, FDelegateKeyValueMapValue>&out Items);
			delegate int FFloatStructMapInoutSignal(TMap<float, FDelegateKeyValueMapValue>&inout Items);
			delegate TMap<float, FDelegateKeyValueMapValue> FFloatStructMapReturnSignal();

			delegate int FObjectStructMapValueSignal(TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> Items);
			delegate int FObjectStructMapInSignal(const TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue>&in Items);
			delegate void FObjectStructMapOutSignal(TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue>&out Items);
			delegate int FObjectStructMapInoutSignal(TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue>&inout Items);
			delegate TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> FObjectStructMapReturnSignal();

			delegate int FStructStringMapValueSignal(TMap<FDelegateKeyValueMapKey, FString> Items);
			delegate int FStructStringMapInSignal(const TMap<FDelegateKeyValueMapKey, FString>&in Items);
			delegate void FStructStringMapOutSignal(TMap<FDelegateKeyValueMapKey, FString>&out Items);
			delegate int FStructStringMapInoutSignal(TMap<FDelegateKeyValueMapKey, FString>&inout Items);
			delegate TMap<FDelegateKeyValueMapKey, FString> FStructStringMapReturnSignal();

			delegate int FStructNameMapValueSignal(TMap<FDelegateKeyValueMapKey, FName> Items);
			delegate int FStructNameMapInSignal(const TMap<FDelegateKeyValueMapKey, FName>&in Items);
			delegate void FStructNameMapOutSignal(TMap<FDelegateKeyValueMapKey, FName>&out Items);
			delegate int FStructNameMapInoutSignal(TMap<FDelegateKeyValueMapKey, FName>&inout Items);
			delegate TMap<FDelegateKeyValueMapKey, FName> FStructNameMapReturnSignal();

			delegate int FStructObjectMapValueSignal(TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> Items);
			delegate int FStructObjectMapInSignal(const TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject>&in Items);
			delegate void FStructObjectMapOutSignal(TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject>&out Items);
			delegate int FStructObjectMapInoutSignal(TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject>&inout Items);
			delegate TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> FStructObjectMapReturnSignal();

			UCLASS()
			class ACoverageStructMapKeyValueDelegateActor : AActor
			{
				UPROPERTY()
				FNameStructMapValueSignal NameStructValueSignal;

				UPROPERTY()
				FNameStructMapInSignal NameStructInSignal;

				UPROPERTY()
				FNameStructMapOutSignal NameStructOutSignal;

				UPROPERTY()
				FNameStructMapInoutSignal NameStructInoutSignal;

				UPROPERTY()
				FNameStructMapReturnSignal NameStructReturnSignal;

				UPROPERTY()
				FStringStructMapValueSignal StringStructValueSignal;

				UPROPERTY()
				FStringStructMapInSignal StringStructInSignal;

				UPROPERTY()
				FStringStructMapOutSignal StringStructOutSignal;

				UPROPERTY()
				FStringStructMapInoutSignal StringStructInoutSignal;

				UPROPERTY()
				FStringStructMapReturnSignal StringStructReturnSignal;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UPROPERTY()
				FFloatStructMapValueSignal FloatStructValueSignal;

				UPROPERTY()
				FFloatStructMapInSignal FloatStructInSignal;

				UPROPERTY()
				FFloatStructMapOutSignal FloatStructOutSignal;

				UPROPERTY()
				FFloatStructMapInoutSignal FloatStructInoutSignal;

				UPROPERTY()
				FFloatStructMapReturnSignal FloatStructReturnSignal;

				UPROPERTY()
				FObjectStructMapValueSignal ObjectStructValueSignal;

				UPROPERTY()
				FObjectStructMapInSignal ObjectStructInSignal;

				UPROPERTY()
				FObjectStructMapOutSignal ObjectStructOutSignal;

				UPROPERTY()
				FObjectStructMapInoutSignal ObjectStructInoutSignal;

				UPROPERTY()
				FObjectStructMapReturnSignal ObjectStructReturnSignal;

				UPROPERTY()
				FStructStringMapValueSignal StructStringValueSignal;

				UPROPERTY()
				FStructStringMapInSignal StructStringInSignal;

				UPROPERTY()
				FStructStringMapOutSignal StructStringOutSignal;

				UPROPERTY()
				FStructStringMapInoutSignal StructStringInoutSignal;

				UPROPERTY()
				FStructStringMapReturnSignal StructStringReturnSignal;

				UPROPERTY()
				FStructNameMapValueSignal StructNameValueSignal;

				UPROPERTY()
				FStructNameMapInSignal StructNameInSignal;

				UPROPERTY()
				FStructNameMapOutSignal StructNameOutSignal;

				UPROPERTY()
				FStructNameMapInoutSignal StructNameInoutSignal;

				UPROPERTY()
				FStructNameMapReturnSignal StructNameReturnSignal;

				UPROPERTY()
				FStructObjectMapValueSignal StructObjectValueSignal;

				UPROPERTY()
				FStructObjectMapInSignal StructObjectInSignal;

				UPROPERTY()
				FStructObjectMapOutSignal StructObjectOutSignal;

				UPROPERTY()
				FStructObjectMapInoutSignal StructObjectInoutSignal;

				UPROPERTY()
				FStructObjectMapReturnSignal StructObjectReturnSignal;

				UPROPERTY()
				int NameStructValueResult = 0;

				UPROPERTY()
				int NameStructInResult = 0;

				UPROPERTY()
				int NameStructInoutResult = 0;

				UPROPERTY()
				TMap<FName, FDelegateKeyValueMapValue> NameStructOutResult;

				UPROPERTY()
				TMap<FName, FDelegateKeyValueMapValue> NameStructInoutResultItems;

				UPROPERTY()
				TMap<FName, FDelegateKeyValueMapValue> NameStructReturnResult;

				UPROPERTY()
				bool NameStructValuePreserved = false;

				UPROPERTY()
				bool NameStructInPreserved = false;

				UPROPERTY()
				bool NameStructOutPreserved = false;

				UPROPERTY()
				bool NameStructInoutPreserved = false;

				UPROPERTY()
				bool NameStructReturnPreserved = false;

				UPROPERTY()
				int StringStructValueResult = 0;

				UPROPERTY()
				int StringStructInResult = 0;

				UPROPERTY()
				int StringStructInoutResult = 0;

				UPROPERTY()
				TMap<FString, FDelegateKeyValueMapValue> StringStructOutResult;

				UPROPERTY()
				TMap<FString, FDelegateKeyValueMapValue> StringStructInoutResultItems;

				UPROPERTY()
				TMap<FString, FDelegateKeyValueMapValue> StringStructReturnResult;

				UPROPERTY()
				bool StringStructValuePreserved = false;

				UPROPERTY()
				bool StringStructInPreserved = false;

				UPROPERTY()
				bool StringStructOutPreserved = false;

				UPROPERTY()
				bool StringStructInoutPreserved = false;

				UPROPERTY()
				bool StringStructReturnPreserved = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UPROPERTY()
				int FloatStructValueResult = 0;

				UPROPERTY()
				int FloatStructInResult = 0;

				UPROPERTY()
				int FloatStructInoutResult = 0;

				UPROPERTY()
				TMap<float, FDelegateKeyValueMapValue> FloatStructOutResult;

				UPROPERTY()
				TMap<float, FDelegateKeyValueMapValue> FloatStructInoutResultItems;

				UPROPERTY()
				TMap<float, FDelegateKeyValueMapValue> FloatStructReturnResult;

				UPROPERTY()
				bool FloatStructValuePreserved = false;

				UPROPERTY()
				bool FloatStructInPreserved = false;

				UPROPERTY()
				bool FloatStructOutPreserved = false;

				UPROPERTY()
				bool FloatStructInoutPreserved = false;

				UPROPERTY()
				bool FloatStructReturnPreserved = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UPROPERTY()
				int ObjectStructValueResult = 0;

				UPROPERTY()
				int ObjectStructInResult = 0;

				UPROPERTY()
				int ObjectStructInoutResult = 0;

				UPROPERTY()
				TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> ObjectStructOutResult;

				UPROPERTY()
				TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> ObjectStructInoutResultItems;

				UPROPERTY()
				TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> ObjectStructReturnResult;

				UPROPERTY()
				bool ObjectStructValuePreserved = false;

				UPROPERTY()
				bool ObjectStructInPreserved = false;

				UPROPERTY()
				bool ObjectStructOutPreserved = false;

				UPROPERTY()
				bool ObjectStructInoutPreserved = false;

				UPROPERTY()
				bool ObjectStructReturnPreserved = false;

				UPROPERTY()
				int StructStringValueResult = 0;

				UPROPERTY()
				int StructStringInResult = 0;

				UPROPERTY()
				int StructStringInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, FString> StructStringOutResult;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, FString> StructStringInoutResultItems;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, FString> StructStringReturnResult;

				UPROPERTY()
				bool StructStringValuePreserved = false;

				UPROPERTY()
				bool StructStringInPreserved = false;

				UPROPERTY()
				bool StructStringOutPreserved = false;

				UPROPERTY()
				bool StructStringInoutPreserved = false;

				UPROPERTY()
				bool StructStringReturnPreserved = false;

				UPROPERTY()
				int StructNameValueResult = 0;

				UPROPERTY()
				int StructNameInResult = 0;

				UPROPERTY()
				int StructNameInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, FName> StructNameOutResult;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, FName> StructNameInoutResultItems;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, FName> StructNameReturnResult;

				UPROPERTY()
				bool StructNameValuePreserved = false;

				UPROPERTY()
				bool StructNameInPreserved = false;

				UPROPERTY()
				bool StructNameOutPreserved = false;

				UPROPERTY()
				bool StructNameInoutPreserved = false;

				UPROPERTY()
				bool StructNameReturnPreserved = false;

				UPROPERTY()
				int StructObjectValueResult = 0;

				UPROPERTY()
				int StructObjectInResult = 0;

				UPROPERTY()
				int StructObjectInoutResult = 0;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> StructObjectOutResult;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> StructObjectInoutResultItems;

				UPROPERTY()
				TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> StructObjectReturnResult;

				UPROPERTY()
				bool StructObjectValuePreserved = false;

				UPROPERTY()
				bool StructObjectInPreserved = false;

				UPROPERTY()
				bool StructObjectOutPreserved = false;

				UPROPERTY()
				bool StructObjectInoutPreserved = false;

				UPROPERTY()
				bool StructObjectReturnPreserved = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				FDelegateKeyValueMapKey MakeKey(int ID, FName Tag)
				{
					FDelegateKeyValueMapKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FDelegateKeyValueMapValue MakeValue(int Score, FString Label)
				{
					FDelegateKeyValueMapValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UCoverageStructDelegateMapKeyObject MakeKeyObject(int Value)
				{
					UCoverageStructDelegateMapKeyObject Object = Cast<UCoverageStructDelegateMapKeyObject>(NewObject(this, UCoverageStructDelegateMapKeyObject::StaticClass()));
					Object.Value = Value;
					return Object;
				}

				UCoverageStructDelegateMapValueObject MakeObject(int Value)
				{
					UCoverageStructDelegateMapValueObject Object = Cast<UCoverageStructDelegateMapValueObject>(NewObject(this, UCoverageStructDelegateMapValueObject::StaticClass()));
					Object.Value = Value;
					return Object;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION()
				int HandleNameStructValue(TMap<FName, FDelegateKeyValueMapValue> Items)
				{
					FDelegateKeyValueMapValue Found;
					NameStructValuePreserved =
						Items.Find(n"NameValueB", Found)
						&& Found.Score == 102
						&& Found.Label == "NameValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleNameStructIn(const TMap<FName, FDelegateKeyValueMapValue>&in Items)
				{
					FDelegateKeyValueMapValue Found;
					NameStructInPreserved =
						Items.Find(n"NameInB", Found)
						&& Found.Score == 112
						&& Found.Label == "NameInB";
					return Items.Num() + 10;
				}

				UFUNCTION()
				void HandleNameStructOut(TMap<FName, FDelegateKeyValueMapValue>&out Items)
				{
					Items.Add(n"NameOutA", MakeValue(121, "NameOutA"));
					Items.Add(n"NameOutB", MakeValue(122, "NameOutB"));
				}

				UFUNCTION()
				int HandleNameStructInout(TMap<FName, FDelegateKeyValueMapValue>&inout Items)
				{
					FDelegateKeyValueMapValue Found;
					if (Items.Find(n"NameInoutA", Found))
					{
						Found.Score += 100;
						Found.Label = "NameInoutMutated";
						Items.Add(n"NameInoutA", Found);
					}
					Items.Add(n"NameInoutB", MakeValue(132, "NameInoutAdded"));
					NameStructInoutResultItems = Items;
					FDelegateKeyValueMapValue Mutated;
					NameStructInoutPreserved =
						Items.Find(n"NameInoutA", Mutated)
						&& Mutated.Score == 231
						&& Mutated.Label == "NameInoutMutated";
					return Items.Num() + Mutated.Score;
				}

				UFUNCTION()
				TMap<FName, FDelegateKeyValueMapValue> HandleNameStructReturn()
				{
					TMap<FName, FDelegateKeyValueMapValue> Items;
					Items.Add(n"NameReturnA", MakeValue(141, "NameReturnA"));
					Items.Add(n"NameReturnB", MakeValue(142, "NameReturnB"));
					return Items;
				}

				UFUNCTION()
				int HandleStringStructValue(TMap<FString, FDelegateKeyValueMapValue> Items)
				{
					FDelegateKeyValueMapValue Found;
					StringStructValuePreserved =
						Items.Find("StringValueB", Found)
						&& Found.Score == 202
						&& Found.Label == "StringValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleStringStructIn(const TMap<FString, FDelegateKeyValueMapValue>&in Items)
				{
					FDelegateKeyValueMapValue Found;
					StringStructInPreserved =
						Items.Find("StringInB", Found)
						&& Found.Score == 212
						&& Found.Label == "StringInB";
					return Items.Num() + 20;
				}

				UFUNCTION()
				void HandleStringStructOut(TMap<FString, FDelegateKeyValueMapValue>&out Items)
				{
					Items.Add("StringOutA", MakeValue(221, "StringOutA"));
					Items.Add("StringOutB", MakeValue(222, "StringOutB"));
				}

				UFUNCTION()
				int HandleStringStructInout(TMap<FString, FDelegateKeyValueMapValue>&inout Items)
				{
					FDelegateKeyValueMapValue Found;
					if (Items.Find("StringInoutA", Found))
					{
						Found.Score += 100;
						Found.Label = "StringInoutMutated";
						Items.Add("StringInoutA", Found);
					}
					Items.Add("StringInoutB", MakeValue(232, "StringInoutAdded"));
					StringStructInoutResultItems = Items;
					FDelegateKeyValueMapValue Mutated;
					StringStructInoutPreserved =
						Items.Find("StringInoutA", Mutated)
						&& Mutated.Score == 331
						&& Mutated.Label == "StringInoutMutated";
					return Items.Num() + Mutated.Score;
				}

				UFUNCTION()
				TMap<FString, FDelegateKeyValueMapValue> HandleStringStructReturn()
				{
					TMap<FString, FDelegateKeyValueMapValue> Items;
					Items.Add("StringReturnA", MakeValue(241, "StringReturnA"));
					Items.Add("StringReturnB", MakeValue(242, "StringReturnB"));
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION()
				int HandleFloatStructValue(TMap<float, FDelegateKeyValueMapValue> Items)
				{
					FDelegateKeyValueMapValue Found;
					FloatStructValuePreserved =
						Items.Find(602.5f, Found)
						&& Found.Score == 602
						&& Found.Label == "FloatValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleFloatStructIn(const TMap<float, FDelegateKeyValueMapValue>&in Items)
				{
					FDelegateKeyValueMapValue Found;
					FloatStructInPreserved =
						Items.Find(612.5f, Found)
						&& Found.Score == 612
						&& Found.Label == "FloatInB";
					return Items.Num() + 30;
				}

				UFUNCTION()
				void HandleFloatStructOut(TMap<float, FDelegateKeyValueMapValue>&out Items)
				{
					Items.Add(621.5f, MakeValue(621, "FloatOutA"));
					Items.Add(622.5f, MakeValue(622, "FloatOutB"));
				}

				UFUNCTION()
				int HandleFloatStructInout(TMap<float, FDelegateKeyValueMapValue>&inout Items)
				{
					FDelegateKeyValueMapValue Found;
					if (Items.Find(631.5f, Found))
					{
						Found.Score += 100;
						Found.Label = "FloatInoutMutated";
						Items.Add(631.5f, Found);
					}
					Items.Add(632.5f, MakeValue(632, "FloatInoutAdded"));
					FloatStructInoutResultItems = Items;
					FDelegateKeyValueMapValue Mutated;
					FloatStructInoutPreserved =
						Items.Find(631.5f, Mutated)
						&& Mutated.Score == 731
						&& Mutated.Label == "FloatInoutMutated";
					return Items.Num() + Mutated.Score;
				}

				UFUNCTION()
				TMap<float, FDelegateKeyValueMapValue> HandleFloatStructReturn()
				{
					TMap<float, FDelegateKeyValueMapValue> Items;
					Items.Add(641.5f, MakeValue(641, "FloatReturnA"));
					Items.Add(642.5f, MakeValue(642, "FloatReturnB"));
					return Items;
				}

				UFUNCTION()
				int HandleObjectStructValue(TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> Items)
				{
					ObjectStructValuePreserved = false;
					for (auto Element : Items)
					{
						UCoverageStructDelegateMapKeyObject Key = Element.GetKey();
						FDelegateKeyValueMapValue Value = Element.GetValue();
						if (Key != nullptr
							&& Key.Value == 702
							&& Value.Score == 702
							&& Value.Label == "ObjectValueB")
						{
							ObjectStructValuePreserved = true;
						}
					}
					return Items.Num();
				}

				UFUNCTION()
				int HandleObjectStructIn(const TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue>&in Items)
				{
					ObjectStructInPreserved = false;
					for (auto Element : Items)
					{
						UCoverageStructDelegateMapKeyObject Key = Element.GetKey();
						FDelegateKeyValueMapValue Value = Element.GetValue();
						if (Key != nullptr
							&& Key.Value == 712
							&& Value.Score == 712
							&& Value.Label == "ObjectInB")
						{
							ObjectStructInPreserved = true;
						}
					}
					return Items.Num() + 90;
				}

				UFUNCTION()
				void HandleObjectStructOut(TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue>&out Items)
				{
					Items.Add(MakeKeyObject(721), MakeValue(721, "ObjectOutA"));
					Items.Add(MakeKeyObject(722), MakeValue(722, "ObjectOutB"));
				}

				UFUNCTION()
				int HandleObjectStructInout(TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue>&inout Items)
				{
					for (auto Element : Items)
					{
						UCoverageStructDelegateMapKeyObject Key = Element.GetKey();
						FDelegateKeyValueMapValue Value = Element.GetValue();
						if (Key != nullptr && Key.Value == 731)
						{
							Value.Score += 100;
							Value.Label = "ObjectInoutMutated";
							Element.SetValue(Value);
						}
					}
					Items.Add(MakeKeyObject(732), MakeValue(732, "ObjectInoutAdded"));
					ObjectStructInoutResultItems = Items;
					ObjectStructInoutPreserved = false;
					for (auto Element : ObjectStructInoutResultItems)
					{
						UCoverageStructDelegateMapKeyObject Key = Element.GetKey();
						FDelegateKeyValueMapValue Value = Element.GetValue();
						if (Key != nullptr
							&& Key.Value == 731
							&& Value.Score == 831
							&& Value.Label == "ObjectInoutMutated")
						{
							ObjectStructInoutPreserved = true;
						}
					}
					return Items.Num() + 100;
				}

				UFUNCTION()
				TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> HandleObjectStructReturn()
				{
					TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> Items;
					Items.Add(MakeKeyObject(741), MakeValue(741, "ObjectReturnA"));
					Items.Add(MakeKeyObject(742), MakeValue(742, "ObjectReturnB"));
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION()
				int HandleStructStringValue(TMap<FDelegateKeyValueMapKey, FString> Items)
				{
					FString Found;
					StructStringValuePreserved =
						Items.Find(MakeKey(301, n"StructStringValueB"), Found)
						&& Found == "StructStringValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleStructStringIn(const TMap<FDelegateKeyValueMapKey, FString>&in Items)
				{
					FString Found;
					StructStringInPreserved =
						Items.Find(MakeKey(311, n"StructStringInB"), Found)
						&& Found == "StructStringInB";
					return Items.Num() + 30;
				}

				UFUNCTION()
				void HandleStructStringOut(TMap<FDelegateKeyValueMapKey, FString>&out Items)
				{
					Items.Add(MakeKey(320, n"StructStringOutA"), "StructStringOutA");
					Items.Add(MakeKey(321, n"StructStringOutB"), "StructStringOutB");
				}

				UFUNCTION()
				int HandleStructStringInout(TMap<FDelegateKeyValueMapKey, FString>&inout Items)
				{
					FDelegateKeyValueMapKey Existing = MakeKey(330, n"StructStringInoutA");
					FString Found;
					if (Items.Find(Existing, Found))
					{
						Items.Add(Existing, "StructStringInoutMutated");
					}
					Items.Add(MakeKey(331, n"StructStringInoutB"), "StructStringInoutAdded");
					StructStringInoutResultItems = Items;
					FString Mutated;
					StructStringInoutPreserved =
						Items.Find(Existing, Mutated)
						&& Mutated == "StructStringInoutMutated";
					return Items.Num() + 40;
				}

				UFUNCTION()
				TMap<FDelegateKeyValueMapKey, FString> HandleStructStringReturn()
				{
					TMap<FDelegateKeyValueMapKey, FString> Items;
					Items.Add(MakeKey(340, n"StructStringReturnA"), "StructStringReturnA");
					Items.Add(MakeKey(341, n"StructStringReturnB"), "StructStringReturnB");
					return Items;
				}

				UFUNCTION()
				int HandleStructNameValue(TMap<FDelegateKeyValueMapKey, FName> Items)
				{
					FName Found;
					StructNameValuePreserved =
						Items.Find(MakeKey(401, n"StructNameValueB"), Found)
						&& Found == n"StructNameValueB";
					return Items.Num();
				}

				UFUNCTION()
				int HandleStructNameIn(const TMap<FDelegateKeyValueMapKey, FName>&in Items)
				{
					FName Found;
					StructNameInPreserved =
						Items.Find(MakeKey(411, n"StructNameInB"), Found)
						&& Found == n"StructNameInB";
					return Items.Num() + 50;
				}

				UFUNCTION()
				void HandleStructNameOut(TMap<FDelegateKeyValueMapKey, FName>&out Items)
				{
					Items.Add(MakeKey(420, n"StructNameOutA"), n"StructNameOutA");
					Items.Add(MakeKey(421, n"StructNameOutB"), n"StructNameOutB");
				}

				UFUNCTION()
				int HandleStructNameInout(TMap<FDelegateKeyValueMapKey, FName>&inout Items)
				{
					FDelegateKeyValueMapKey Existing = MakeKey(430, n"StructNameInoutA");
					FName Found;
					if (Items.Find(Existing, Found))
					{
						Items.Add(Existing, n"StructNameInoutMutated");
					}
					Items.Add(MakeKey(431, n"StructNameInoutB"), n"StructNameInoutAdded");
					StructNameInoutResultItems = Items;
					FName Mutated;
					StructNameInoutPreserved =
						Items.Find(Existing, Mutated)
						&& Mutated == n"StructNameInoutMutated";
					return Items.Num() + 60;
				}

				UFUNCTION()
				TMap<FDelegateKeyValueMapKey, FName> HandleStructNameReturn()
				{
					TMap<FDelegateKeyValueMapKey, FName> Items;
					Items.Add(MakeKey(440, n"StructNameReturnA"), n"StructNameReturnA");
					Items.Add(MakeKey(441, n"StructNameReturnB"), n"StructNameReturnB");
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION()
				int HandleStructObjectValue(TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> Items)
				{
					UCoverageStructDelegateMapValueObject Found = nullptr;
					StructObjectValuePreserved =
						Items.Find(MakeKey(501, n"StructObjectValueB"), Found)
						&& Found != nullptr
						&& Found.Value == 502;
					return Items.Num();
				}

				UFUNCTION()
				int HandleStructObjectIn(const TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject>&in Items)
				{
					UCoverageStructDelegateMapValueObject Found = nullptr;
					StructObjectInPreserved =
						Items.Find(MakeKey(511, n"StructObjectInB"), Found)
						&& Found != nullptr
						&& Found.Value == 512;
					return Items.Num() + 70;
				}

				UFUNCTION()
				void HandleStructObjectOut(TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject>&out Items)
				{
					Items.Add(MakeKey(520, n"StructObjectOutA"), MakeObject(521));
					Items.Add(MakeKey(521, n"StructObjectOutB"), MakeObject(522));
				}

				UFUNCTION()
				int HandleStructObjectInout(TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject>&inout Items)
				{
					FDelegateKeyValueMapKey Existing = MakeKey(530, n"StructObjectInoutA");
					UCoverageStructDelegateMapValueObject Found = nullptr;
					if (Items.Find(Existing, Found) && Found != nullptr)
					{
						Items.Add(Existing, MakeObject(Found.Value + 100));
					}
					Items.Add(MakeKey(531, n"StructObjectInoutB"), MakeObject(532));
					StructObjectInoutResultItems = Items;
					UCoverageStructDelegateMapValueObject Mutated = nullptr;
					StructObjectInoutPreserved =
						Items.Find(Existing, Mutated)
						&& Mutated != nullptr
						&& Mutated.Value == 631;
					return Items.Num() + 80;
				}

				UFUNCTION()
				TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> HandleStructObjectReturn()
				{
					TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> Items;
					Items.Add(MakeKey(540, n"StructObjectReturnA"), MakeObject(541));
					Items.Add(MakeKey(541, n"StructObjectReturnB"), MakeObject(542));
					return Items;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					NameStructValueSignal.BindUFunction(this, n"HandleNameStructValue");
					NameStructInSignal.BindUFunction(this, n"HandleNameStructIn");
					NameStructOutSignal.BindUFunction(this, n"HandleNameStructOut");
					NameStructInoutSignal.BindUFunction(this, n"HandleNameStructInout");
					NameStructReturnSignal.BindUFunction(this, n"HandleNameStructReturn");

					TMap<FName, FDelegateKeyValueMapValue> NameStructValueItems;
					NameStructValueItems.Add(n"NameValueA", MakeValue(101, "NameValueA"));
					NameStructValueItems.Add(n"NameValueB", MakeValue(102, "NameValueB"));
					NameStructValueResult = NameStructValueSignal.Execute(NameStructValueItems);

					TMap<FName, FDelegateKeyValueMapValue> NameStructInItems;
					NameStructInItems.Add(n"NameInA", MakeValue(111, "NameInA"));
					NameStructInItems.Add(n"NameInB", MakeValue(112, "NameInB"));
					NameStructInResult = NameStructInSignal.Execute(NameStructInItems);

					NameStructOutSignal.Execute(NameStructOutResult);
					FDelegateKeyValueMapValue NameStructOutFound;
					NameStructOutPreserved =
						NameStructOutResult.Find(n"NameOutB", NameStructOutFound)
						&& NameStructOutFound.Score == 122
						&& NameStructOutFound.Label == "NameOutB";

					NameStructInoutResultItems.Add(n"NameInoutA", MakeValue(131, "NameInoutA"));
					NameStructInoutResult = NameStructInoutSignal.Execute(NameStructInoutResultItems);

					NameStructReturnResult = NameStructReturnSignal.Execute();
					FDelegateKeyValueMapValue NameStructReturnFound;
					NameStructReturnPreserved =
						NameStructReturnResult.Find(n"NameReturnB", NameStructReturnFound)
						&& NameStructReturnFound.Score == 142
						&& NameStructReturnFound.Label == "NameReturnB";

					StringStructValueSignal.BindUFunction(this, n"HandleStringStructValue");
					StringStructInSignal.BindUFunction(this, n"HandleStringStructIn");
					StringStructOutSignal.BindUFunction(this, n"HandleStringStructOut");
					StringStructInoutSignal.BindUFunction(this, n"HandleStringStructInout");
					StringStructReturnSignal.BindUFunction(this, n"HandleStringStructReturn");

					TMap<FString, FDelegateKeyValueMapValue> StringStructValueItems;
					StringStructValueItems.Add("StringValueA", MakeValue(201, "StringValueA"));
					StringStructValueItems.Add("StringValueB", MakeValue(202, "StringValueB"));
					StringStructValueResult = StringStructValueSignal.Execute(StringStructValueItems);

					TMap<FString, FDelegateKeyValueMapValue> StringStructInItems;
					StringStructInItems.Add("StringInA", MakeValue(211, "StringInA"));
					StringStructInItems.Add("StringInB", MakeValue(212, "StringInB"));
					StringStructInResult = StringStructInSignal.Execute(StringStructInItems);

					StringStructOutSignal.Execute(StringStructOutResult);
					FDelegateKeyValueMapValue StringStructOutFound;
					StringStructOutPreserved =
						StringStructOutResult.Find("StringOutB", StringStructOutFound)
						&& StringStructOutFound.Score == 222
						&& StringStructOutFound.Label == "StringOutB";

					StringStructInoutResultItems.Add("StringInoutA", MakeValue(231, "StringInoutA"));
					StringStructInoutResult = StringStructInoutSignal.Execute(StringStructInoutResultItems);

					StringStructReturnResult = StringStructReturnSignal.Execute();
					FDelegateKeyValueMapValue StringStructReturnFound;
					StringStructReturnPreserved =
						StringStructReturnResult.Find("StringReturnB", StringStructReturnFound)
						&& StringStructReturnFound.Score == 242
						&& StringStructReturnFound.Label == "StringReturnB";
					)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

					FloatStructValueSignal.BindUFunction(this, n"HandleFloatStructValue");
					FloatStructInSignal.BindUFunction(this, n"HandleFloatStructIn");
					FloatStructOutSignal.BindUFunction(this, n"HandleFloatStructOut");
					FloatStructInoutSignal.BindUFunction(this, n"HandleFloatStructInout");
					FloatStructReturnSignal.BindUFunction(this, n"HandleFloatStructReturn");

					TMap<float, FDelegateKeyValueMapValue> FloatStructValueItems;
					FloatStructValueItems.Add(601.5f, MakeValue(601, "FloatValueA"));
					FloatStructValueItems.Add(602.5f, MakeValue(602, "FloatValueB"));
					FloatStructValueResult = FloatStructValueSignal.Execute(FloatStructValueItems);

					TMap<float, FDelegateKeyValueMapValue> FloatStructInItems;
					FloatStructInItems.Add(611.5f, MakeValue(611, "FloatInA"));
					FloatStructInItems.Add(612.5f, MakeValue(612, "FloatInB"));
					FloatStructInResult = FloatStructInSignal.Execute(FloatStructInItems);

					FloatStructOutSignal.Execute(FloatStructOutResult);
					FDelegateKeyValueMapValue FloatStructOutFound;
					FloatStructOutPreserved =
						FloatStructOutResult.Find(622.5f, FloatStructOutFound)
						&& FloatStructOutFound.Score == 622
						&& FloatStructOutFound.Label == "FloatOutB";

					FloatStructInoutResultItems.Add(631.5f, MakeValue(631, "FloatInoutA"));
					FloatStructInoutResult = FloatStructInoutSignal.Execute(FloatStructInoutResultItems);

					FloatStructReturnResult = FloatStructReturnSignal.Execute();
					FDelegateKeyValueMapValue FloatStructReturnFound;
					FloatStructReturnPreserved =
						FloatStructReturnResult.Find(642.5f, FloatStructReturnFound)
						&& FloatStructReturnFound.Score == 642
						&& FloatStructReturnFound.Label == "FloatReturnB";

					ObjectStructValueSignal.BindUFunction(this, n"HandleObjectStructValue");
					ObjectStructInSignal.BindUFunction(this, n"HandleObjectStructIn");
					ObjectStructOutSignal.BindUFunction(this, n"HandleObjectStructOut");
					ObjectStructInoutSignal.BindUFunction(this, n"HandleObjectStructInout");
					ObjectStructReturnSignal.BindUFunction(this, n"HandleObjectStructReturn");

					TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> ObjectStructValueItems;
					ObjectStructValueItems.Add(MakeKeyObject(701), MakeValue(701, "ObjectValueA"));
					ObjectStructValueItems.Add(MakeKeyObject(702), MakeValue(702, "ObjectValueB"));
					ObjectStructValueResult = ObjectStructValueSignal.Execute(ObjectStructValueItems);

					TMap<UCoverageStructDelegateMapKeyObject, FDelegateKeyValueMapValue> ObjectStructInItems;
					ObjectStructInItems.Add(MakeKeyObject(711), MakeValue(711, "ObjectInA"));
					ObjectStructInItems.Add(MakeKeyObject(712), MakeValue(712, "ObjectInB"));
					ObjectStructInResult = ObjectStructInSignal.Execute(ObjectStructInItems);

					ObjectStructOutSignal.Execute(ObjectStructOutResult);
					ObjectStructOutPreserved = false;
					for (auto Element : ObjectStructOutResult)
					{
						UCoverageStructDelegateMapKeyObject Key = Element.GetKey();
						FDelegateKeyValueMapValue Value = Element.GetValue();
						if (Key != nullptr
							&& Key.Value == 722
							&& Value.Score == 722
							&& Value.Label == "ObjectOutB")
						{
							ObjectStructOutPreserved = true;
						}
					}

					ObjectStructInoutResultItems.Add(MakeKeyObject(731), MakeValue(731, "ObjectInoutA"));
					ObjectStructInoutResult = ObjectStructInoutSignal.Execute(ObjectStructInoutResultItems);

					ObjectStructReturnResult = ObjectStructReturnSignal.Execute();
					ObjectStructReturnPreserved = false;
					for (auto Element : ObjectStructReturnResult)
					{
						UCoverageStructDelegateMapKeyObject Key = Element.GetKey();
						FDelegateKeyValueMapValue Value = Element.GetValue();
						if (Key != nullptr
							&& Key.Value == 742
							&& Value.Score == 742
							&& Value.Label == "ObjectReturnB")
						{
							ObjectStructReturnPreserved = true;
						}
					}
					)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

					StructStringValueSignal.BindUFunction(this, n"HandleStructStringValue");
					StructStringInSignal.BindUFunction(this, n"HandleStructStringIn");
					StructStringOutSignal.BindUFunction(this, n"HandleStructStringOut");
					StructStringInoutSignal.BindUFunction(this, n"HandleStructStringInout");
					StructStringReturnSignal.BindUFunction(this, n"HandleStructStringReturn");

					TMap<FDelegateKeyValueMapKey, FString> StructStringValueItems;
					StructStringValueItems.Add(MakeKey(300, n"StructStringValueA"), "StructStringValueA");
					StructStringValueItems.Add(MakeKey(301, n"StructStringValueB"), "StructStringValueB");
					StructStringValueResult = StructStringValueSignal.Execute(StructStringValueItems);

					TMap<FDelegateKeyValueMapKey, FString> StructStringInItems;
					StructStringInItems.Add(MakeKey(310, n"StructStringInA"), "StructStringInA");
					StructStringInItems.Add(MakeKey(311, n"StructStringInB"), "StructStringInB");
					StructStringInResult = StructStringInSignal.Execute(StructStringInItems);

					StructStringOutSignal.Execute(StructStringOutResult);
					FString StructStringOutFound;
					StructStringOutPreserved =
						StructStringOutResult.Find(MakeKey(321, n"StructStringOutB"), StructStringOutFound)
						&& StructStringOutFound == "StructStringOutB";

					StructStringInoutResultItems.Add(MakeKey(330, n"StructStringInoutA"), "StructStringInoutA");
					StructStringInoutResult = StructStringInoutSignal.Execute(StructStringInoutResultItems);

					StructStringReturnResult = StructStringReturnSignal.Execute();
					FString StructStringReturnFound;
					StructStringReturnPreserved =
						StructStringReturnResult.Find(MakeKey(341, n"StructStringReturnB"), StructStringReturnFound)
						&& StructStringReturnFound == "StructStringReturnB";
					)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

					StructNameValueSignal.BindUFunction(this, n"HandleStructNameValue");
					StructNameInSignal.BindUFunction(this, n"HandleStructNameIn");
					StructNameOutSignal.BindUFunction(this, n"HandleStructNameOut");
					StructNameInoutSignal.BindUFunction(this, n"HandleStructNameInout");
					StructNameReturnSignal.BindUFunction(this, n"HandleStructNameReturn");

					TMap<FDelegateKeyValueMapKey, FName> StructNameValueItems;
					StructNameValueItems.Add(MakeKey(400, n"StructNameValueA"), n"StructNameValueA");
					StructNameValueItems.Add(MakeKey(401, n"StructNameValueB"), n"StructNameValueB");
					StructNameValueResult = StructNameValueSignal.Execute(StructNameValueItems);

					TMap<FDelegateKeyValueMapKey, FName> StructNameInItems;
					StructNameInItems.Add(MakeKey(410, n"StructNameInA"), n"StructNameInA");
					StructNameInItems.Add(MakeKey(411, n"StructNameInB"), n"StructNameInB");
					StructNameInResult = StructNameInSignal.Execute(StructNameInItems);

					StructNameOutSignal.Execute(StructNameOutResult);
					FName StructNameOutFound;
					StructNameOutPreserved =
						StructNameOutResult.Find(MakeKey(421, n"StructNameOutB"), StructNameOutFound)
						&& StructNameOutFound == n"StructNameOutB";

					StructNameInoutResultItems.Add(MakeKey(430, n"StructNameInoutA"), n"StructNameInoutA");
					StructNameInoutResult = StructNameInoutSignal.Execute(StructNameInoutResultItems);

					StructNameReturnResult = StructNameReturnSignal.Execute();
					FName StructNameReturnFound;
					StructNameReturnPreserved =
						StructNameReturnResult.Find(MakeKey(441, n"StructNameReturnB"), StructNameReturnFound)
						&& StructNameReturnFound == n"StructNameReturnB";

					StructObjectValueSignal.BindUFunction(this, n"HandleStructObjectValue");
					StructObjectInSignal.BindUFunction(this, n"HandleStructObjectIn");
					StructObjectOutSignal.BindUFunction(this, n"HandleStructObjectOut");
					StructObjectInoutSignal.BindUFunction(this, n"HandleStructObjectInout");
					StructObjectReturnSignal.BindUFunction(this, n"HandleStructObjectReturn");

					TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> StructObjectValueItems;
					StructObjectValueItems.Add(MakeKey(500, n"StructObjectValueA"), MakeObject(501));
					StructObjectValueItems.Add(MakeKey(501, n"StructObjectValueB"), MakeObject(502));
					StructObjectValueResult = StructObjectValueSignal.Execute(StructObjectValueItems);

					TMap<FDelegateKeyValueMapKey, UCoverageStructDelegateMapValueObject> StructObjectInItems;
					StructObjectInItems.Add(MakeKey(510, n"StructObjectInA"), MakeObject(511));
					StructObjectInItems.Add(MakeKey(511, n"StructObjectInB"), MakeObject(512));
					StructObjectInResult = StructObjectInSignal.Execute(StructObjectInItems);

					StructObjectOutSignal.Execute(StructObjectOutResult);
					UCoverageStructDelegateMapValueObject StructObjectOutFound = nullptr;
					StructObjectOutPreserved =
						StructObjectOutResult.Find(MakeKey(521, n"StructObjectOutB"), StructObjectOutFound)
						&& StructObjectOutFound != nullptr
						&& StructObjectOutFound.Value == 522;

					StructObjectInoutResultItems.Add(MakeKey(530, n"StructObjectInoutA"), MakeObject(531));
					StructObjectInoutResult = StructObjectInoutSignal.Execute(StructObjectInoutResultItems);

					StructObjectReturnResult = StructObjectReturnSignal.Execute();
					UCoverageStructDelegateMapValueObject StructObjectReturnFound = nullptr;
					StructObjectReturnPreserved =
						StructObjectReturnResult.Find(MakeKey(541, n"StructObjectReturnB"), StructObjectReturnFound)
						&& StructObjectReturnFound != nullptr
						&& StructObjectReturnFound.Value == 542;
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMapKeyValueDelegatePermutationMatrix.as"),
			ScriptSource,
			TEXT("ACoverageStructMapKeyValueDelegateActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct map key/value delegate actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FMapProperty* NameStructValueParameter = nullptr;
		FMapProperty* NameStructInParameter = nullptr;
		FMapProperty* NameStructOutParameter = nullptr;
		FMapProperty* NameStructInoutParameter = nullptr;
		FMapProperty* NameStructReturnProperty = nullptr;
		FMapProperty* StringStructValueParameter = nullptr;
		FMapProperty* StringStructInParameter = nullptr;
		FMapProperty* StringStructOutParameter = nullptr;
		FMapProperty* StringStructInoutParameter = nullptr;
		FMapProperty* StringStructReturnProperty = nullptr;
		FMapProperty* FloatStructValueParameter = nullptr;
		FMapProperty* FloatStructInParameter = nullptr;
		FMapProperty* FloatStructOutParameter = nullptr;
		FMapProperty* FloatStructInoutParameter = nullptr;
		FMapProperty* FloatStructReturnProperty = nullptr;
		FMapProperty* ObjectStructValueParameter = nullptr;
		FMapProperty* ObjectStructInParameter = nullptr;
		FMapProperty* ObjectStructOutParameter = nullptr;
		FMapProperty* ObjectStructInoutParameter = nullptr;
		FMapProperty* ObjectStructReturnProperty = nullptr;
		FMapProperty* StructStringValueParameter = nullptr;
		FMapProperty* StructStringInParameter = nullptr;
		FMapProperty* StructStringOutParameter = nullptr;
		FMapProperty* StructStringInoutParameter = nullptr;
		FMapProperty* StructStringReturnProperty = nullptr;
		FMapProperty* StructNameValueParameter = nullptr;
		FMapProperty* StructNameInParameter = nullptr;
		FMapProperty* StructNameOutParameter = nullptr;
		FMapProperty* StructNameInoutParameter = nullptr;
		FMapProperty* StructNameReturnProperty = nullptr;
		FMapProperty* StructObjectValueParameter = nullptr;
		FMapProperty* StructObjectInParameter = nullptr;
		FMapProperty* StructObjectOutParameter = nullptr;
		FMapProperty* StructObjectInoutParameter = nullptr;
		FMapProperty* StructObjectReturnProperty = nullptr;
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FNameProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("NameStructValueSignal"), TEXT("NameStructInSignal"), TEXT("NameStructOutSignal"),
			TEXT("NameStructInoutSignal"), TEXT("NameStructReturnSignal"), TEXT("TMap<FName,FStruct>"),
			NameStructValueParameter, NameStructInParameter, NameStructOutParameter, NameStructInoutParameter, NameStructReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStrProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("StringStructValueSignal"), TEXT("StringStructInSignal"), TEXT("StringStructOutSignal"),
			TEXT("StringStructInoutSignal"), TEXT("StringStructReturnSignal"), TEXT("TMap<FString,FStruct>"),
			StringStructValueParameter, StringStructInParameter, StringStructOutParameter, StringStructInoutParameter, StringStructReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FScriptFloatProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("FloatStructValueSignal"), TEXT("FloatStructInSignal"), TEXT("FloatStructOutSignal"),
			TEXT("FloatStructInoutSignal"), TEXT("FloatStructReturnSignal"), TEXT("TMap<float,FStruct>"),
			FloatStructValueParameter, FloatStructInParameter, FloatStructOutParameter, FloatStructInoutParameter, FloatStructReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FObjectProperty, FStructProperty>(
			*TestRunner, ScriptClass, TEXT("ObjectStructValueSignal"), TEXT("ObjectStructInSignal"), TEXT("ObjectStructOutSignal"),
			TEXT("ObjectStructInoutSignal"), TEXT("ObjectStructReturnSignal"), TEXT("TMap<UObject,FStruct>"),
			ObjectStructValueParameter, ObjectStructInParameter, ObjectStructOutParameter, ObjectStructInoutParameter, ObjectStructReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FStrProperty>(
			*TestRunner, ScriptClass, TEXT("StructStringValueSignal"), TEXT("StructStringInSignal"), TEXT("StructStringOutSignal"),
			TEXT("StructStringInoutSignal"), TEXT("StructStringReturnSignal"), TEXT("TMap<FStruct,FString>"),
			StructStringValueParameter, StructStringInParameter, StructStringOutParameter, StructStringInoutParameter, StructStringReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FNameProperty>(
			*TestRunner, ScriptClass, TEXT("StructNameValueSignal"), TEXT("StructNameInSignal"), TEXT("StructNameOutSignal"),
			TEXT("StructNameInoutSignal"), TEXT("StructNameReturnSignal"), TEXT("TMap<FStruct,FName>"),
			StructNameValueParameter, StructNameInParameter, StructNameOutParameter, StructNameInoutParameter, StructNameReturnProperty))));
		ASSERT_THAT(IsTrue((ExpectDelegateMapPermutation<FStructProperty, FObjectProperty>(
			*TestRunner, ScriptClass, TEXT("StructObjectValueSignal"), TEXT("StructObjectInSignal"), TEXT("StructObjectOutSignal"),
			TEXT("StructObjectInoutSignal"), TEXT("StructObjectReturnSignal"), TEXT("TMap<FStruct,UObject>"),
			StructObjectValueParameter, StructObjectInParameter, StructObjectOutParameter, StructObjectInoutParameter, StructObjectReturnProperty))));
		if (NameStructValueParameter == nullptr || NameStructInParameter == nullptr || NameStructOutParameter == nullptr
			|| NameStructInoutParameter == nullptr || NameStructReturnProperty == nullptr
			|| StringStructValueParameter == nullptr || StringStructInParameter == nullptr || StringStructOutParameter == nullptr
			|| StringStructInoutParameter == nullptr || StringStructReturnProperty == nullptr
			|| FloatStructValueParameter == nullptr || FloatStructInParameter == nullptr || FloatStructOutParameter == nullptr
			|| FloatStructInoutParameter == nullptr || FloatStructReturnProperty == nullptr
			|| ObjectStructValueParameter == nullptr || ObjectStructInParameter == nullptr || ObjectStructOutParameter == nullptr
			|| ObjectStructInoutParameter == nullptr || ObjectStructReturnProperty == nullptr
			|| StructStringValueParameter == nullptr || StructStringInParameter == nullptr || StructStringOutParameter == nullptr
			|| StructStringInoutParameter == nullptr || StructStringReturnProperty == nullptr
			|| StructNameValueParameter == nullptr || StructNameInParameter == nullptr || StructNameOutParameter == nullptr
			|| StructNameInoutParameter == nullptr || StructNameReturnProperty == nullptr
			|| StructObjectValueParameter == nullptr || StructObjectInParameter == nullptr || StructObjectOutParameter == nullptr
			|| StructObjectInoutParameter == nullptr || StructObjectReturnProperty == nullptr)
		{
			return;
		}

		FStructProperty* NameStructValueProperty = CastField<FStructProperty>(NameStructValueParameter->ValueProp);
		FStructProperty* StringStructValueProperty = CastField<FStructProperty>(StringStructValueParameter->ValueProp);
		FStructProperty* FloatStructValueProperty = CastField<FStructProperty>(FloatStructValueParameter->ValueProp);
		FStructProperty* ObjectStructValueProperty = CastField<FStructProperty>(ObjectStructValueParameter->ValueProp);
		FStructProperty* StructStringKeyProperty = CastField<FStructProperty>(StructStringValueParameter->KeyProp);
		FStructProperty* StructNameKeyProperty = CastField<FStructProperty>(StructNameValueParameter->KeyProp);
		FStructProperty* StructObjectKeyProperty = CastField<FStructProperty>(StructObjectValueParameter->KeyProp);
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameStructValueParameter->KeyProp),
			TEXT("TMap<FName,FStruct> delegate key should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(NameStructValueProperty,
			TEXT("TMap<FName,FStruct> delegate value should expose struct values")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringStructValueParameter->KeyProp),
			TEXT("TMap<FString,FStruct> delegate key should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(StringStructValueProperty,
			TEXT("TMap<FString,FStruct> delegate value should expose struct values")));
		ASSERT_THAT(IsNotNull(CastField<FScriptFloatProperty>(FloatStructValueParameter->KeyProp),
			TEXT("TMap<float,FStruct> delegate key should use script float storage")));
		ASSERT_THAT(IsNotNull(FloatStructValueProperty,
			TEXT("TMap<float,FStruct> delegate value should expose struct values")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(ObjectStructValueParameter->KeyProp),
			TEXT("TMap<UObject,FStruct> delegate key should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(ObjectStructValueProperty,
			TEXT("TMap<UObject,FStruct> delegate value should expose struct values")));
		ASSERT_THAT(IsNotNull(StructStringKeyProperty,
			TEXT("TMap<FStruct,FString> delegate key should expose struct keys")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StructStringValueParameter->ValueProp),
			TEXT("TMap<FStruct,FString> delegate value should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(StructNameKeyProperty,
			TEXT("TMap<FStruct,FName> delegate key should expose struct keys")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(StructNameValueParameter->ValueProp),
			TEXT("TMap<FStruct,FName> delegate value should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(StructObjectKeyProperty,
			TEXT("TMap<FStruct,UObject> delegate key should expose struct keys")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(StructObjectValueParameter->ValueProp),
			TEXT("TMap<FStruct,UObject> delegate value should reflect as FObjectProperty")));
		if (NameStructValueProperty == nullptr || NameStructValueProperty->Struct == nullptr
			|| StringStructValueProperty == nullptr || StringStructValueProperty->Struct == nullptr
			|| FloatStructValueProperty == nullptr || FloatStructValueProperty->Struct == nullptr
			|| ObjectStructValueProperty == nullptr || ObjectStructValueProperty->Struct == nullptr
			|| StructStringKeyProperty == nullptr || StructStringKeyProperty->Struct == nullptr
			|| StructNameKeyProperty == nullptr || StructNameKeyProperty->Struct == nullptr
			|| StructObjectKeyProperty == nullptr || StructObjectKeyProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(NameStructValueProperty->Struct, StringStructValueProperty->Struct,
			TEXT("TMap<FName,FStruct> and TMap<FString,FStruct> delegates should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(NameStructValueProperty->Struct, FloatStructValueProperty->Struct,
			TEXT("TMap<FName,FStruct> and TMap<float,FStruct> delegates should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(NameStructValueProperty->Struct, ObjectStructValueProperty->Struct,
			TEXT("TMap<FName,FStruct> and TMap<UObject,FStruct> delegates should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(StructStringKeyProperty->Struct, StructNameKeyProperty->Struct,
			TEXT("TMap<FStruct,FString> and TMap<FStruct,FName> delegates should reuse the key UScriptStruct")));
		ASSERT_THAT(AreEqual(StructStringKeyProperty->Struct, StructObjectKeyProperty->Struct,
			TEXT("TMap<FStruct,FString> and TMap<FStruct,UObject> delegates should reuse the key UScriptStruct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct map key/value delegate actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NameStructValueResult"), 2,
			TEXT("TMap<FName,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NameStructInResult"), 12,
			TEXT("TMap<FName,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NameStructInoutResult"), 233,
			TEXT("TMap<FName,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructValuePreserved"), true,
			TEXT("TMap<FName,FStruct> delegate value parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructInPreserved"), true,
			TEXT("TMap<FName,FStruct> delegate const-ref parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructOutPreserved"), true,
			TEXT("TMap<FName,FStruct> delegate out should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructInoutPreserved"), true,
			TEXT("TMap<FName,FStruct> delegate inout should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructReturnPreserved"), true,
			TEXT("TMap<FName,FStruct> delegate return should preserve struct values"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameStructOutResult"), Count),
			TEXT("TMap<FName,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName,FStruct> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameStructInoutResultItems"), Count),
			TEXT("TMap<FName,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName,FStruct> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameStructReturnResult"), Count),
			TEXT("TMap<FName,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName,FStruct> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringStructValueResult"), 2,
			TEXT("TMap<FString,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringStructInResult"), 22,
			TEXT("TMap<FString,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringStructInoutResult"), 333,
			TEXT("TMap<FString,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructValuePreserved"), true,
			TEXT("TMap<FString,FStruct> delegate value parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructInPreserved"), true,
			TEXT("TMap<FString,FStruct> delegate const-ref parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructOutPreserved"), true,
			TEXT("TMap<FString,FStruct> delegate out should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructInoutPreserved"), true,
			TEXT("TMap<FString,FStruct> delegate inout should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructReturnPreserved"), true,
			TEXT("TMap<FString,FStruct> delegate return should preserve struct values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringStructOutResult"), Count),
			TEXT("TMap<FString,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,FStruct> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringStructInoutResultItems"), Count),
			TEXT("TMap<FString,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,FStruct> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringStructReturnResult"), Count),
			TEXT("TMap<FString,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,FStruct> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FloatStructValueResult"), 2,
			TEXT("TMap<float,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FloatStructInResult"), 32,
			TEXT("TMap<float,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FloatStructInoutResult"), 733,
			TEXT("TMap<float,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructValuePreserved"), true,
			TEXT("TMap<float,FStruct> delegate value parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructInPreserved"), true,
			TEXT("TMap<float,FStruct> delegate const-ref parameter should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructOutPreserved"), true,
			TEXT("TMap<float,FStruct> delegate out should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructInoutPreserved"), true,
			TEXT("TMap<float,FStruct> delegate inout should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructReturnPreserved"), true,
			TEXT("TMap<float,FStruct> delegate return should preserve struct values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("FloatStructOutResult"), Count),
			TEXT("TMap<float,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<float,FStruct> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("FloatStructInoutResultItems"), Count),
			TEXT("TMap<float,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<float,FStruct> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("FloatStructReturnResult"), Count),
			TEXT("TMap<float,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<float,FStruct> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectStructValueResult"), 2,
			TEXT("TMap<UObject,FStruct> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectStructInResult"), 92,
			TEXT("TMap<UObject,FStruct> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectStructInoutResult"), 102,
			TEXT("TMap<UObject,FStruct> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructValuePreserved"), true,
			TEXT("TMap<UObject,FStruct> delegate value parameter should preserve object keys and struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructInPreserved"), true,
			TEXT("TMap<UObject,FStruct> delegate const-ref parameter should preserve object keys and struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructOutPreserved"), true,
			TEXT("TMap<UObject,FStruct> delegate out should preserve object keys and struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructInoutPreserved"), true,
			TEXT("TMap<UObject,FStruct> delegate inout should preserve object keys and struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructReturnPreserved"), true,
			TEXT("TMap<UObject,FStruct> delegate return should preserve object keys and struct values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ObjectStructOutResult"), Count),
			TEXT("TMap<UObject,FStruct> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<UObject,FStruct> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ObjectStructInoutResultItems"), Count),
			TEXT("TMap<UObject,FStruct> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<UObject,FStruct> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ObjectStructReturnResult"), Count),
			TEXT("TMap<UObject,FStruct> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<UObject,FStruct> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStringValueResult"), 2,
			TEXT("TMap<FStruct,FString> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStringInResult"), 32,
			TEXT("TMap<FStruct,FString> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStringInoutResult"), 42,
			TEXT("TMap<FStruct,FString> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringValuePreserved"), true,
			TEXT("TMap<FStruct,FString> delegate value parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringInPreserved"), true,
			TEXT("TMap<FStruct,FString> delegate const-ref parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringOutPreserved"), true,
			TEXT("TMap<FStruct,FString> delegate out should preserve string values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringInoutPreserved"), true,
			TEXT("TMap<FStruct,FString> delegate inout should preserve string values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringReturnPreserved"), true,
			TEXT("TMap<FStruct,FString> delegate return should preserve string values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructStringOutResult"), Count),
			TEXT("TMap<FStruct,FString> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FString> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructStringInoutResultItems"), Count),
			TEXT("TMap<FStruct,FString> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FString> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructStringReturnResult"), Count),
			TEXT("TMap<FStruct,FString> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FString> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructNameValueResult"), 2,
			TEXT("TMap<FStruct,FName> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructNameInResult"), 52,
			TEXT("TMap<FStruct,FName> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructNameInoutResult"), 62,
			TEXT("TMap<FStruct,FName> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameValuePreserved"), true,
			TEXT("TMap<FStruct,FName> delegate value parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameInPreserved"), true,
			TEXT("TMap<FStruct,FName> delegate const-ref parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameOutPreserved"), true,
			TEXT("TMap<FStruct,FName> delegate out should preserve name values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameInoutPreserved"), true,
			TEXT("TMap<FStruct,FName> delegate inout should preserve name values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameReturnPreserved"), true,
			TEXT("TMap<FStruct,FName> delegate return should preserve name values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructNameOutResult"), Count),
			TEXT("TMap<FStruct,FName> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FName> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructNameInoutResultItems"), Count),
			TEXT("TMap<FStruct,FName> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FName> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructNameReturnResult"), Count),
			TEXT("TMap<FStruct,FName> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FName> delegate return should contain two entries")));

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructObjectValueResult"), 2,
			TEXT("TMap<FStruct,UObject> delegate value parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructObjectInResult"), 72,
			TEXT("TMap<FStruct,UObject> delegate const-ref parameter should execute"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructObjectInoutResult"), 82,
			TEXT("TMap<FStruct,UObject> delegate inout parameter should execute after mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectValuePreserved"), true,
			TEXT("TMap<FStruct,UObject> delegate value parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectInPreserved"), true,
			TEXT("TMap<FStruct,UObject> delegate const-ref parameter should preserve struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectOutPreserved"), true,
			TEXT("TMap<FStruct,UObject> delegate out should preserve object values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectInoutPreserved"), true,
			TEXT("TMap<FStruct,UObject> delegate inout should preserve object values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectReturnPreserved"), true,
			TEXT("TMap<FStruct,UObject> delegate return should preserve object values"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructObjectOutResult"), Count),
			TEXT("TMap<FStruct,UObject> delegate out result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,UObject> delegate out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructObjectInoutResultItems"), Count),
			TEXT("TMap<FStruct,UObject> delegate inout result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,UObject> delegate inout should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructObjectReturnResult"), Count),
			TEXT("TMap<FStruct,UObject> delegate return result should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,UObject> delegate return should contain two entries")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT as return value
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAsReturn)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Return"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructReturn.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FReturnStruct
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FString Description;

				UPROPERTY()
				FVector Position;
			}

			UCLASS()
			class ACoverageStructReturnActor : AActor
			{
				UPROPERTY()
				FReturnStruct Result;

				FReturnStruct CreateStruct(int InID, FString InDesc)
				{
					FReturnStruct New;
					New.ID = InID;
					New.Description = InDesc;
					New.Position = FVector(InID * 10.0f, InID * 20.0f, InID * 30.0f);
					return New;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Result = CreateStruct(42, "Test Result");
				}
			}
			)AS"),
			TEXT("ACoverageStructReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct return actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify returned struct values
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Result.ID"), 42, TEXT("Returned struct ID should be set"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Result.Description"), FString(TEXT("Test Result")), TEXT("Returned struct Description should be set"));

		FVector VectorResult(0.0f);
		ASSERT_THAT(IsTrue(GetStructByPath<FVector>(*TestRunner, Actor, TEXT("Result.Position"), VectorResult), TEXT("Get returned struct Position")));
		ASSERT_THAT(IsTrue(VectorResult.Equals(FVector(420.0f, 840.0f, 1260.0f)), TEXT("Returned struct Position should be calculated correctly")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT function shape matrix: local, value, in, out, inout, return, containers.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructFunctionShapeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_FunctionShapeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructFunctionShapeMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FShapeStruct
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructFunctionShapeActor : AActor
			{
				UPROPERTY()
				FShapeStruct LocalResult;

				UPROPERTY()
				FShapeStruct ValueResult;

				UPROPERTY()
				FShapeStruct InResult;

				UPROPERTY()
				FShapeStruct OutResult;

				UPROPERTY()
				FShapeStruct InoutResult;

				UPROPERTY()
				FShapeStruct ReturnResult;

				UPROPERTY()
				TArray<FShapeStruct> ArrayResult;

				UPROPERTY()
				TMap<int, FShapeStruct> MapResult;

				FShapeStruct MakeStruct(int Value, FString Label)
				{
					FShapeStruct Result;
					Result.Value = Value;
					Result.Label = Label;
					return Result;
				}

				FShapeStruct AcceptValue(FShapeStruct Param)
				{
					// By-value UStruct params are immutable in this fork; mutate a local copy.
					FShapeStruct Result = Param;
					Result.Value += 10;
					Result.Label += "_Value";
					return Result;
				}

				FShapeStruct AcceptConstRef(const FShapeStruct&in Param)
				{
					FShapeStruct Result;
					Result.Value = Param.Value + 20;
					Result.Label = Param.Label + "_In";
					return Result;
				}

				void FillOut(FShapeStruct&out Param)
				{
					Param.Value = 30;
					Param.Label = "Out";
				}

				int MutateInout(FShapeStruct&inout Param)
				{
					Param.Value += 40;
					Param.Label += "_Inout";
					return Param.Value;
				}

				TArray<FShapeStruct> MakeArray(FShapeStruct First, const FShapeStruct&in Second)
				{
					TArray<FShapeStruct> Result;
					Result.Add(First);
					Result.Add(Second);
					return Result;
				}

				TMap<int, FShapeStruct> MakeMap(const FShapeStruct&in First, const FShapeStruct&in Second)
				{
					TMap<int, FShapeStruct> Result;
					Result.Add(1, First);
					Result.Add(2, Second);
					return Result;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FShapeStruct Local;
					Local.Value = 1;
					Local.Label = "Local";
					LocalResult = Local;

					ValueResult = AcceptValue(Local);
					InResult = AcceptConstRef(Local);
					FillOut(OutResult);

					InoutResult = Local;
					MutateInout(InoutResult);

					ReturnResult = MakeStruct(50, "Return");
					ArrayResult = MakeArray(ValueResult, InResult);
					MapResult = MakeMap(OutResult, InoutResult);
				}
			}
			)AS"),
			TEXT("ACoverageStructFunctionShapeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct function-shape actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct function-shape actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LocalResult.Value"), 1, TEXT("local struct variable should assign to UPROPERTY"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LocalResult.Label"), FString(TEXT("Local")), TEXT("local struct string field should assign to UPROPERTY"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ValueResult.Value"), 11, TEXT("value parameter should copy and return"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ValueResult.Label"), FString(TEXT("Local_Value")), TEXT("value parameter string field should copy and return"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InResult.Value"), 21, TEXT("const &in parameter should read source fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("InResult.Label"), FString(TEXT("Local_In")), TEXT("const &in parameter string field should read source fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("OutResult.Value"), 30, TEXT("&out parameter should write target fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("OutResult.Label"), FString(TEXT("Out")), TEXT("&out parameter string field should write target fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("InoutResult.Value"), 41, TEXT("&inout parameter should read and mutate target fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("InoutResult.Label"), FString(TEXT("Local_Inout")), TEXT("&inout parameter string field should mutate target fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ReturnResult.Value"), 50, TEXT("return value should preserve struct int field"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ReturnResult.Label"), FString(TEXT("Return")), TEXT("return value should preserve struct string field"))));

		int32 ArrayCount = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayResult"), ArrayCount), TEXT("TArray<FStruct> return should be readable")));
		ASSERT_THAT(AreEqual(2, ArrayCount, TEXT("TArray<FStruct> return should contain two entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayResult[0].Value"), 11, TEXT("TArray<FStruct> return first element should preserve fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ArrayResult[1].Label"), FString(TEXT("Local_In")), TEXT("TArray<FStruct> return second element should preserve fields"))));

		int32 MapCount = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapResult"), MapCount), TEXT("TMap<int,FStruct> return should be readable")));
		ASSERT_THAT(AreEqual(2, MapCount, TEXT("TMap<int,FStruct> return should contain two entries")));

		const FStructProperty* MapValueStructProperty = nullptr;
		const void* FirstMapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath(*TestRunner, Actor, TEXT("MapResult"), 1, MapValueStructProperty, FirstMapValueAddress),
			TEXT("TMap<int,FStruct> first value should be readable")));
		if (MapValueStructProperty == nullptr || MapValueStructProperty->Struct == nullptr || FirstMapValueAddress == nullptr)
		{
			return;
		}

		FIntProperty* MapValueProperty = FindFProperty<FIntProperty>(MapValueStructProperty->Struct, TEXT("Value"));
		FStrProperty* MapLabelProperty = FindFProperty<FStrProperty>(MapValueStructProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(MapValueProperty, TEXT("TMap<int,FStruct> value struct should expose Value")));
		ASSERT_THAT(IsNotNull(MapLabelProperty, TEXT("TMap<int,FStruct> value struct should expose Label")));
		if (MapValueProperty == nullptr || MapLabelProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(30, MapValueProperty->GetPropertyValue_InContainer(FirstMapValueAddress),
			TEXT("TMap<int,FStruct> first value should preserve int fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("Out")), MapLabelProperty->GetPropertyValue_InContainer(FirstMapValueAddress),
			TEXT("TMap<int,FStruct> first value should preserve string fields")));

		const void* SecondMapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath(*TestRunner, Actor, TEXT("MapResult"), 2, MapValueStructProperty, SecondMapValueAddress),
			TEXT("TMap<int,FStruct> second value should be readable")));
		if (SecondMapValueAddress == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(41, MapValueProperty->GetPropertyValue_InContainer(SecondMapValueAddress),
			TEXT("TMap<int,FStruct> second value should preserve inout-mutated int fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("Local_Inout")), MapLabelProperty->GetPropertyValue_InContainer(SecondMapValueAddress),
			TEXT("TMap<int,FStruct> second value should preserve inout-mutated string fields")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT container parameter matrix: struct elements through every parameter mode.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructContainerParameterShapeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ContainerParameterShapeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructContainerParameterShapeMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructContainerParamItem
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FStructContainerParamItem& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 131) + Tag.GetHash();
				}
			}

			UCLASS()
			class ACoverageStructContainerParamActor : AActor
			{
				UPROPERTY()
				TArray<FStructContainerParamItem> ArrayOut;

				UPROPERTY()
				TArray<FStructContainerParamItem> ArrayInout;

				UPROPERTY()
				TArray<FStructContainerParamItem> ArrayReturn;

				UPROPERTY()
				TMap<int, FStructContainerParamItem> MapOut;

				UPROPERTY()
				TMap<int, FStructContainerParamItem> MapInout;

				UPROPERTY()
				TMap<int, FStructContainerParamItem> MapReturn;

				UPROPERTY()
				TSet<FStructContainerParamItem> SetOut;

				UPROPERTY()
				TSet<FStructContainerParamItem> SetInout;

				UPROPERTY()
				TSet<FStructContainerParamItem> SetReturn;

				UPROPERTY()
				int ArrayValueCount = 0;

				UPROPERTY()
				int ArrayInCount = 0;

				UPROPERTY()
				int MapValueCount = 0;

				UPROPERTY()
				int MapInCount = 0;

				UPROPERTY()
				int SetValueCount = 0;

				UPROPERTY()
				int SetInCount = 0;

				UPROPERTY()
				bool SetReturnContains = false;

				FStructContainerParamItem MakeItem(int ID, FName Tag)
				{
					FStructContainerParamItem Item;
					Item.ID = ID;
					Item.Tag = Tag;
					return Item;
				}

				int CountArrayValue(TArray<FStructContainerParamItem> Items)
				{
					return Items.Num();
				}

				int CountArrayIn(const TArray<FStructContainerParamItem>&in Items)
				{
					return Items.Num();
				}

				void FillArrayOut(TArray<FStructContainerParamItem>&out Items)
				{
					Items.Add(MakeItem(10, n"ArrayOutA"));
					Items.Add(MakeItem(11, n"ArrayOutB"));
				}

				void MutateArrayInout(TArray<FStructContainerParamItem>&inout Items)
				{
					Items.Add(MakeItem(12, n"ArrayInoutAdded"));
					FStructContainerParamItem First = Items[0];
					First.ID = 13;
					Items[0] = First;
				}

				TArray<FStructContainerParamItem> ReturnArray()
				{
					TArray<FStructContainerParamItem> Items;
					Items.Add(MakeItem(14, n"ArrayReturnA"));
					Items.Add(MakeItem(15, n"ArrayReturnB"));
					return Items;
				}

				int CountMapValue(TMap<int, FStructContainerParamItem> Items)
				{
					return Items.Num();
				}

				int CountMapIn(const TMap<int, FStructContainerParamItem>&in Items)
				{
					return Items.Num();
				}

				void FillMapOut(TMap<int, FStructContainerParamItem>&out Items)
				{
					Items.Add(20, MakeItem(20, n"MapOutA"));
					Items.Add(21, MakeItem(21, n"MapOutB"));
				}

				void MutateMapInout(TMap<int, FStructContainerParamItem>&inout Items)
				{
					Items.Add(22, MakeItem(22, n"MapInoutAdded"));
					Items[1] = MakeItem(23, n"MapInoutReplaced");
				}

				TMap<int, FStructContainerParamItem> ReturnMap()
				{
					TMap<int, FStructContainerParamItem> Items;
					Items.Add(24, MakeItem(24, n"MapReturnA"));
					Items.Add(25, MakeItem(25, n"MapReturnB"));
					return Items;
				}

				int CountSetValue(TSet<FStructContainerParamItem> Items)
				{
					return Items.Num();
				}

				int CountSetIn(const TSet<FStructContainerParamItem>&in Items)
				{
					return Items.Num();
				}

				void FillSetOut(TSet<FStructContainerParamItem>&out Items)
				{
					Items.Add(MakeItem(30, n"SetOutA"));
					Items.Add(MakeItem(31, n"SetOutB"));
				}

				void MutateSetInout(TSet<FStructContainerParamItem>&inout Items)
				{
					Items.Add(MakeItem(32, n"SetInoutAdded"));
					Items.Remove(MakeItem(2, n"SetInitialB"));
				}

				TSet<FStructContainerParamItem> ReturnSet()
				{
					TSet<FStructContainerParamItem> Items;
					Items.Add(MakeItem(33, n"SetReturnA"));
					Items.Add(MakeItem(34, n"SetReturnB"));
					return Items;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					TArray<FStructContainerParamItem> LocalArray;
					LocalArray.Add(MakeItem(1, n"ArrayInitialA"));
					LocalArray.Add(MakeItem(2, n"ArrayInitialB"));
					ArrayValueCount = CountArrayValue(LocalArray);
					ArrayInCount = CountArrayIn(LocalArray);
					FillArrayOut(ArrayOut);
					ArrayInout = LocalArray;
					MutateArrayInout(ArrayInout);
					ArrayReturn = ReturnArray();

					TMap<int, FStructContainerParamItem> LocalMap;
					LocalMap.Add(1, MakeItem(1, n"MapInitialA"));
					LocalMap.Add(2, MakeItem(2, n"MapInitialB"));
					MapValueCount = CountMapValue(LocalMap);
					MapInCount = CountMapIn(LocalMap);
					FillMapOut(MapOut);
					MapInout = LocalMap;
					MutateMapInout(MapInout);
					MapReturn = ReturnMap();

					TSet<FStructContainerParamItem> LocalSet;
					LocalSet.Add(MakeItem(1, n"SetInitialA"));
					LocalSet.Add(MakeItem(2, n"SetInitialB"));
					SetValueCount = CountSetValue(LocalSet);
					SetInCount = CountSetIn(LocalSet);
					FillSetOut(SetOut);
					SetInout = LocalSet;
					MutateSetInout(SetInout);
					SetReturn = ReturnSet();
					SetReturnContains = SetReturn.Contains(MakeItem(34, n"SetReturnB"));
				}
			}
			)AS"),
			TEXT("ACoverageStructContainerParamActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct container-parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FMapProperty* MapOutProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("MapOut"));
		FSetProperty* SetOutProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("SetOut"));
		ASSERT_THAT(IsNotNull(MapOutProperty, TEXT("TMap<int,FStruct> output property should reflect")));
		ASSERT_THAT(IsNotNull(SetOutProperty, TEXT("TSet<FStruct> output property should reflect")));
		if (MapOutProperty == nullptr || SetOutProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(MapOutProperty->ValueProp), TEXT("TMap<int,FStruct> value should be an AS struct")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(SetOutProperty->ElementProp), TEXT("TSet<FStruct> element should be an AS struct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct container-parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayValueCount"), 2, TEXT("TArray<FStruct> value parameter should count elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayInCount"), 2, TEXT("TArray<FStruct> &in parameter should count elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapValueCount"), 2, TEXT("TMap<int,FStruct> value parameter should count entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapInCount"), 2, TEXT("TMap<int,FStruct> &in parameter should count entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetValueCount"), 2, TEXT("TSet<FStruct> value parameter should count elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("SetInCount"), 2, TEXT("TSet<FStruct> &in parameter should count elements"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayOut"), Count), TEXT("TArray<FStruct> &out should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FStruct> &out should write two elements")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayOut[1].ID"), 11, TEXT("TArray<FStruct> &out should preserve second element fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("ArrayOut[1].Tag"), FName(TEXT("ArrayOutB")), TEXT("TArray<FStruct> &out should preserve FName fields"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayInout"), Count), TEXT("TArray<FStruct> &inout should be readable")));
		ASSERT_THAT(AreEqual(3, Count, TEXT("TArray<FStruct> &inout should add one element")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayInout[0].ID"), 13, TEXT("TArray<FStruct> &inout should mutate existing element fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("ArrayInout[2].Tag"), FName(TEXT("ArrayInoutAdded")), TEXT("TArray<FStruct> &inout should append struct elements"))));

		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ArrayReturn"), Count), TEXT("TArray<FStruct> return should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TArray<FStruct> return should contain two elements")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ArrayReturn[0].ID"), 14, TEXT("TArray<FStruct> return should preserve first element fields"))));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapOut"), Count), TEXT("TMap<int,FStruct> &out should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,FStruct> &out should write two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapInout"), Count), TEXT("TMap<int,FStruct> &inout should be readable")));
		ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int,FStruct> &inout should add one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("MapReturn"), Count), TEXT("TMap<int,FStruct> return should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<int,FStruct> return should contain two entries")));

		const FStructProperty* MapValueStructProperty = nullptr;
		const void* MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath(*TestRunner, Actor, TEXT("MapInout"), 1, MapValueStructProperty, MapValueAddress),
			TEXT("TMap<int,FStruct> &inout should expose replaced value")));
		if (MapValueStructProperty == nullptr || MapValueStructProperty->Struct == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		FIntProperty* IDProperty = FindFProperty<FIntProperty>(MapValueStructProperty->Struct, TEXT("ID"));
		FNameProperty* TagProperty = FindFProperty<FNameProperty>(MapValueStructProperty->Struct, TEXT("Tag"));
		ASSERT_THAT(IsNotNull(IDProperty, TEXT("Struct map value should expose ID")));
		ASSERT_THAT(IsNotNull(TagProperty, TEXT("Struct map value should expose Tag")));
		if (IDProperty == nullptr || TagProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(23, IDProperty->GetPropertyValue_InContainer(MapValueAddress),
			TEXT("TMap<int,FStruct> &inout should replace existing struct values")));
		ASSERT_THAT(AreEqual(FName(TEXT("MapInoutReplaced")), TagProperty->GetPropertyValue_InContainer(MapValueAddress),
			TEXT("TMap<int,FStruct> &inout should replace existing FName values")));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetOut"), Count), TEXT("TSet<FStruct> &out should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FStruct> &out should write two elements")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetInout"), Count), TEXT("TSet<FStruct> &inout should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FStruct> &inout should remove one and add one element")));
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("SetReturn"), Count), TEXT("TSet<FStruct> return should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TSet<FStruct> return should contain two elements")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SetReturnContains"), true, TEXT("TSet<FStruct> return should support Contains on hashable structs"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT container members: struct-owned arrays, maps, and sets with struct
	// values, keys, and elements.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructContainerMemberShapeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ContainerMemberShapeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructContainerMemberShapeMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructMemberContainerKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FStructMemberContainerKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 719) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FStructMemberContainerValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			USTRUCT(BlueprintType)
			struct FStructContainerOwner
			{
				UPROPERTY()
				TArray<FStructMemberContainerValue> Values;

				UPROPERTY()
				TMap<int, FStructMemberContainerValue> IntToValue;

				UPROPERTY()
				TMap<FStructMemberContainerKey, int> KeyToScore;

				UPROPERTY()
				TMap<FStructMemberContainerKey, FStructMemberContainerValue> KeyToValue;

				UPROPERTY()
				TSet<FStructMemberContainerKey> KeySet;
			}

			UCLASS()
			class ACoverageStructContainerMemberActor : AActor
			{
				UPROPERTY()
				FStructContainerOwner Data;

				UPROPERTY()
				bool KeyToScoreFindWorked = false;

				UPROPERTY()
				int KeyToScoreFound = 0;

				UPROPERTY()
				bool KeyToValueFindWorked = false;

				UPROPERTY()
				int KeyToValueFoundScore = 0;

				UPROPERTY()
				bool KeySetContainsWorked = false;

				UPROPERTY()
				bool KeySetRemoveWorked = false;

				FStructMemberContainerKey MakeKey(int ID, FName Tag)
				{
					FStructMemberContainerKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FStructMemberContainerValue MakeValue(int Score, FString Label)
				{
					FStructMemberContainerValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.Values.Add(MakeValue(10, "ArrayA"));
					Data.Values.Add(MakeValue(11, "ArrayB"));

					Data.IntToValue.Add(20, MakeValue(20, "MapValueA"));
					Data.IntToValue.Add(21, MakeValue(21, "MapValueB"));

					FStructMemberContainerKey Alpha = MakeKey(30, n"Alpha");
					FStructMemberContainerKey AlphaDuplicate = MakeKey(30, n"Alpha");
					FStructMemberContainerKey Beta = MakeKey(31, n"Beta");

					Data.KeyToScore.Add(Alpha, 300);
					Data.KeyToScore.Add(Beta, 310);
					KeyToScoreFindWorked = Data.KeyToScore.Find(AlphaDuplicate, KeyToScoreFound);

					Data.KeyToValue.Add(Alpha, MakeValue(400, "StructMapA"));
					Data.KeyToValue.Add(Beta, MakeValue(410, "StructMapB"));
					FStructMemberContainerValue FoundValue;
					KeyToValueFindWorked = Data.KeyToValue.Find(AlphaDuplicate, FoundValue);
					KeyToValueFoundScore = FoundValue.Score;

					Data.KeySet.Add(Alpha);
					Data.KeySet.Add(AlphaDuplicate);
					Data.KeySet.Add(Beta);
					KeySetContainsWorked = Data.KeySet.Contains(AlphaDuplicate) && Data.KeySet.Num() == 2;
					KeySetRemoveWorked = Data.KeySet.Remove(AlphaDuplicate) && !Data.KeySet.Contains(Alpha) && Data.KeySet.Contains(Beta);
				}
			}
			)AS"),
			TEXT("ACoverageStructContainerMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct container-member actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Container-owner USTRUCT property should reflect")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		FArrayProperty* ValuesProperty = FindFProperty<FArrayProperty>(DataProperty->Struct, TEXT("Values"));
		FMapProperty* IntToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("IntToValue"));
		FMapProperty* KeyToScoreProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToScore"));
		FMapProperty* KeyToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToValue"));
		FSetProperty* KeySetProperty = FindFProperty<FSetProperty>(DataProperty->Struct, TEXT("KeySet"));
		ASSERT_THAT(IsNotNull(ValuesProperty, TEXT("USTRUCT member TArray<FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(IntToValueProperty, TEXT("USTRUCT member TMap<int,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToScoreProperty, TEXT("USTRUCT member TMap<FStruct,int> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToValueProperty, TEXT("USTRUCT member TMap<FStruct,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(KeySetProperty, TEXT("USTRUCT member TSet<FStruct> should reflect")));
		if (ValuesProperty == nullptr || IntToValueProperty == nullptr || KeyToScoreProperty == nullptr
			|| KeyToValueProperty == nullptr || KeySetProperty == nullptr)
		{
			return;
		}

		FStructProperty* ArrayInnerProperty = CastField<FStructProperty>(ValuesProperty->Inner);
		FStructProperty* IntMapValueProperty = CastField<FStructProperty>(IntToValueProperty->ValueProp);
		FStructProperty* StructMapKeyProperty = CastField<FStructProperty>(KeyToScoreProperty->KeyProp);
		FStructProperty* StructMapValueKeyProperty = CastField<FStructProperty>(KeyToValueProperty->KeyProp);
		FStructProperty* StructMapValueProperty = CastField<FStructProperty>(KeyToValueProperty->ValueProp);
		FStructProperty* SetElementProperty = CastField<FStructProperty>(KeySetProperty->ElementProp);
		ASSERT_THAT(IsNotNull(ArrayInnerProperty, TEXT("USTRUCT member TArray inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(IntMapValueProperty, TEXT("USTRUCT member TMap<int,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(StructMapKeyProperty, TEXT("USTRUCT member TMap<FStruct,int> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(StructMapValueKeyProperty, TEXT("USTRUCT member TMap<FStruct,FStruct> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(StructMapValueProperty, TEXT("USTRUCT member TMap<FStruct,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(SetElementProperty, TEXT("USTRUCT member TSet<FStruct> element should be FStructProperty")));
		if (ArrayInnerProperty == nullptr || IntMapValueProperty == nullptr || StructMapKeyProperty == nullptr
			|| StructMapValueKeyProperty == nullptr || StructMapValueProperty == nullptr || SetElementProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(ArrayInnerProperty->Struct, IntMapValueProperty->Struct,
			TEXT("USTRUCT member array and int-map values should share the same value struct type")));
		ASSERT_THAT(AreEqual(StructMapKeyProperty->Struct, StructMapValueKeyProperty->Struct,
			TEXT("USTRUCT member struct-key maps should share the same key struct type")));
		ASSERT_THAT(AreEqual(StructMapKeyProperty->Struct, SetElementProperty->Struct,
			TEXT("USTRUCT member struct-key map and set should share the same key struct type")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct container-member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("Data.Values"), Count),
			TEXT("USTRUCT member TArray<FStruct> should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TArray<FStruct> should contain two entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Data.Values[1].Score"), 11,
			TEXT("USTRUCT member TArray<FStruct> should preserve struct fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("Data.Values[1].Label"), FString(TEXT("ArrayB")),
			TEXT("USTRUCT member TArray<FStruct> should preserve string fields"))));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.IntToValue"), Count),
			TEXT("USTRUCT member TMap<int,FStruct> should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<int,FStruct> should contain two entries")));
		const FStructProperty* IntMapValueStructProperty = nullptr;
		const void* IntMapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath(*TestRunner, Actor, TEXT("Data.IntToValue"), 21, IntMapValueStructProperty, IntMapValueAddress),
			TEXT("USTRUCT member TMap<int,FStruct> value should be readable by key")));
		if (IntMapValueStructProperty == nullptr || IntMapValueStructProperty->Struct == nullptr || IntMapValueAddress == nullptr)
		{
			return;
		}
		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(IntMapValueStructProperty->Struct, TEXT("Score"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(IntMapValueStructProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(ScoreProperty, TEXT("USTRUCT member map value should expose Score")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("USTRUCT member map value should expose Label")));
		if (ScoreProperty == nullptr || LabelProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(21, ScoreProperty->GetPropertyValue_InContainer(IntMapValueAddress),
			TEXT("USTRUCT member TMap<int,FStruct> should preserve int fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("MapValueB")), LabelProperty->GetPropertyValue_InContainer(IntMapValueAddress),
			TEXT("USTRUCT member TMap<int,FStruct> should preserve string fields")));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.KeyToScore"), Count),
			TEXT("USTRUCT member TMap<FStruct,int> should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<FStruct,int> should contain two entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToScoreFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,int> should support Find with equivalent struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeyToScoreFound"), 300,
			TEXT("USTRUCT member TMap<FStruct,int> should preserve integer values"))));

		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.KeyToValue"), Count),
			TEXT("USTRUCT member TMap<FStruct,FStruct> should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<FStruct,FStruct> should contain two entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToValueFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,FStruct> should support Find with equivalent struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeyToValueFoundScore"), 400,
			TEXT("USTRUCT member TMap<FStruct,FStruct> should preserve struct value fields"))));

		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("Data.KeySet"), Count),
			TEXT("USTRUCT member TSet<FStruct> should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("USTRUCT member TSet<FStruct> should contain only Beta after remove")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeySetContainsWorked"), true,
			TEXT("USTRUCT member TSet<FStruct> should deduplicate equivalent struct elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeySetRemoveWorked"), true,
			TEXT("USTRUCT member TSet<FStruct> should remove equivalent struct elements"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT extended map members: simple keys to struct values and struct keys
	// to simple/object values.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructExtendedMapMemberPermutationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ExtendedMapMemberPermutationMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructExtendedMapMemberPermutationMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageStructExtendedMemberMapObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			USTRUCT(BlueprintType)
			struct FStructMemberExtendedKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FStructMemberExtendedKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 811) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FStructMemberExtendedValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			USTRUCT(BlueprintType)
			struct FStructExtendedMapOwner
			{
				UPROPERTY()
				TMap<FName, FStructMemberExtendedValue> NameToValue;

				UPROPERTY()
				TMap<FString, FStructMemberExtendedValue> StringToValue;

				UPROPERTY()
				TMap<bool, FStructMemberExtendedValue> BoolToValue;

				UPROPERTY()
				TMap<float, FStructMemberExtendedValue> FloatToValue;

				UPROPERTY()
				TMap<UCoverageStructExtendedMemberMapObject, FStructMemberExtendedValue> ObjectToValue;

				UPROPERTY()
				TMap<FStructMemberExtendedKey, FString> KeyToString;

				UPROPERTY()
				TMap<FStructMemberExtendedKey, FName> KeyToName;

				UPROPERTY()
				TMap<FStructMemberExtendedKey, bool> KeyToBool;

				UPROPERTY()
				TMap<FStructMemberExtendedKey, float> KeyToFloat;

				UPROPERTY()
				TMap<FStructMemberExtendedKey, UCoverageStructExtendedMemberMapObject> KeyToObject;
			}

			UCLASS()
			class ACoverageStructExtendedMapMemberActor : AActor
			{
				UPROPERTY()
				FStructExtendedMapOwner Data;

				UPROPERTY()
				bool NameToValueFindWorked = false;

				UPROPERTY()
				int NameToValueFoundScore = 0;

				UPROPERTY()
				bool StringToValueFindWorked = false;

				UPROPERTY()
				int StringToValueFoundScore = 0;

				UPROPERTY()
				bool BoolToValueFindWorked = false;

				UPROPERTY()
				int BoolToValueFoundScore = 0;

				UPROPERTY()
				bool FloatToValueFindWorked = false;

				UPROPERTY()
				int FloatToValueFoundScore = 0;

				UPROPERTY()
				bool ObjectToValueFindWorked = false;

				UPROPERTY()
				int ObjectToValueFoundScore = 0;

				UPROPERTY()
				bool KeyToStringFindWorked = false;

				UPROPERTY()
				FString KeyToStringFound;

				UPROPERTY()
				bool KeyToNameFindWorked = false;

				UPROPERTY()
				FName KeyToNameFound;

				UPROPERTY()
				bool KeyToBoolFindWorked = false;

				UPROPERTY()
				bool KeyToBoolFound = false;

				UPROPERTY()
				bool KeyToFloatFindWorked = false;

				UPROPERTY()
				float KeyToFloatFound = 0.0f;

				UPROPERTY()
				bool KeyToObjectFindWorked = false;

				UPROPERTY()
				int KeyToObjectFoundValue = 0;

				FStructMemberExtendedKey MakeKey(int ID, FName Tag)
				{
					FStructMemberExtendedKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FStructMemberExtendedValue MakeValue(int Score, FString Label)
				{
					FStructMemberExtendedValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UCoverageStructExtendedMemberMapObject MakeObject(int Value)
				{
					UCoverageStructExtendedMemberMapObject Object = Cast<UCoverageStructExtendedMemberMapObject>(NewObject(this, UCoverageStructExtendedMemberMapObject::StaticClass()));
					Object.Value = Value;
					return Object;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					Data.NameToValue.Add(n"NameA", MakeValue(101, "NameA"));
					Data.NameToValue.Add(n"NameB", MakeValue(102, "NameB"));
					FStructMemberExtendedValue FoundValue;
					NameToValueFindWorked = Data.NameToValue.Find(n"NameB", FoundValue);
					NameToValueFoundScore = FoundValue.Score;

					Data.StringToValue.Add("StringA", MakeValue(201, "StringA"));
					Data.StringToValue.Add("StringB", MakeValue(202, "StringB"));
					StringToValueFindWorked = Data.StringToValue.Find("StringB", FoundValue);
					StringToValueFoundScore = FoundValue.Score;

					Data.BoolToValue.Add(true, MakeValue(301, "BoolTrue"));
					Data.BoolToValue.Add(false, MakeValue(302, "BoolFalse"));
					BoolToValueFindWorked = Data.BoolToValue.Find(false, FoundValue);
					BoolToValueFoundScore = FoundValue.Score;

					Data.FloatToValue.Add(401.5f, MakeValue(401, "FloatA"));
					Data.FloatToValue.Add(402.5f, MakeValue(402, "FloatB"));
					FloatToValueFindWorked = Data.FloatToValue.Find(402.5f, FoundValue);
					FloatToValueFoundScore = FoundValue.Score;

					UCoverageStructExtendedMemberMapObject ObjectA = MakeObject(410);
					UCoverageStructExtendedMemberMapObject ObjectB = MakeObject(420);
					Data.ObjectToValue.Add(ObjectA, MakeValue(411, "ObjectA"));
					Data.ObjectToValue.Add(ObjectB, MakeValue(422, "ObjectB"));
					ObjectToValueFindWorked = Data.ObjectToValue.Find(ObjectB, FoundValue);
					ObjectToValueFoundScore = FoundValue.Score;

					FStructMemberExtendedKey Alpha = MakeKey(401, n"Alpha");
					FStructMemberExtendedKey AlphaDuplicate = MakeKey(401, n"Alpha");
					FStructMemberExtendedKey Beta = MakeKey(402, n"Beta");

					Data.KeyToString.Add(Alpha, "StringValue");
					Data.KeyToString.Add(Beta, "StringOther");
					KeyToStringFindWorked = Data.KeyToString.Find(AlphaDuplicate, KeyToStringFound);

					Data.KeyToName.Add(Alpha, n"NameValue");
					Data.KeyToName.Add(Beta, n"NameOther");
					KeyToNameFindWorked = Data.KeyToName.Find(AlphaDuplicate, KeyToNameFound);

					Data.KeyToBool.Add(Alpha, true);
					Data.KeyToBool.Add(Beta, false);
					KeyToBoolFindWorked = Data.KeyToBool.Find(AlphaDuplicate, KeyToBoolFound);

					Data.KeyToFloat.Add(Alpha, 72.5f);
					Data.KeyToFloat.Add(Beta, 73.5f);
					KeyToFloatFindWorked = Data.KeyToFloat.Find(AlphaDuplicate, KeyToFloatFound);

					Data.KeyToObject.Add(Alpha, MakeObject(801));
					Data.KeyToObject.Add(Beta, MakeObject(802));
					UCoverageStructExtendedMemberMapObject FoundObject = nullptr;
					KeyToObjectFindWorked = Data.KeyToObject.Find(Beta, FoundObject);
					KeyToObjectFoundValue = FoundObject != nullptr ? FoundObject.Value : -1;
				}
			}
			)AS"),
			TEXT("ACoverageStructExtendedMapMemberActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct extended map-member actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Extended map owner USTRUCT property should reflect")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		FMapProperty* NameToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("NameToValue"));
		FMapProperty* StringToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("StringToValue"));
		FMapProperty* BoolToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("BoolToValue"));
		FMapProperty* FloatToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("FloatToValue"));
		FMapProperty* ObjectToValueProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("ObjectToValue"));
		FMapProperty* KeyToStringProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToString"));
		FMapProperty* KeyToNameProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToName"));
		FMapProperty* KeyToBoolProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToBool"));
		FMapProperty* KeyToFloatProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToFloat"));
		FMapProperty* KeyToObjectProperty = FindFProperty<FMapProperty>(DataProperty->Struct, TEXT("KeyToObject"));
		ASSERT_THAT(IsNotNull(NameToValueProperty, TEXT("USTRUCT member TMap<FName,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(StringToValueProperty, TEXT("USTRUCT member TMap<FString,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(BoolToValueProperty, TEXT("USTRUCT member TMap<bool,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(FloatToValueProperty, TEXT("USTRUCT member TMap<float,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(ObjectToValueProperty, TEXT("USTRUCT member TMap<UObject,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToStringProperty, TEXT("USTRUCT member TMap<FStruct,FString> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToNameProperty, TEXT("USTRUCT member TMap<FStruct,FName> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToBoolProperty, TEXT("USTRUCT member TMap<FStruct,bool> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToFloatProperty, TEXT("USTRUCT member TMap<FStruct,float> should reflect")));
		ASSERT_THAT(IsNotNull(KeyToObjectProperty, TEXT("USTRUCT member TMap<FStruct,UObject> should reflect")));
		if (NameToValueProperty == nullptr || StringToValueProperty == nullptr || BoolToValueProperty == nullptr
			|| FloatToValueProperty == nullptr || ObjectToValueProperty == nullptr || KeyToStringProperty == nullptr
			|| KeyToNameProperty == nullptr || KeyToBoolProperty == nullptr || KeyToFloatProperty == nullptr
			|| KeyToObjectProperty == nullptr)
		{
			return;
		}

		FStructProperty* NameValueProperty = CastField<FStructProperty>(NameToValueProperty->ValueProp);
		FStructProperty* StringValueProperty = CastField<FStructProperty>(StringToValueProperty->ValueProp);
		FStructProperty* BoolValueProperty = CastField<FStructProperty>(BoolToValueProperty->ValueProp);
		FStructProperty* FloatValueProperty = CastField<FStructProperty>(FloatToValueProperty->ValueProp);
		FStructProperty* ObjectValueProperty = CastField<FStructProperty>(ObjectToValueProperty->ValueProp);
		FStructProperty* StringKeyProperty = CastField<FStructProperty>(KeyToStringProperty->KeyProp);
		FStructProperty* NameKeyProperty = CastField<FStructProperty>(KeyToNameProperty->KeyProp);
		FStructProperty* BoolKeyProperty = CastField<FStructProperty>(KeyToBoolProperty->KeyProp);
		FStructProperty* FloatKeyProperty = CastField<FStructProperty>(KeyToFloatProperty->KeyProp);
		FStructProperty* ObjectKeyProperty = CastField<FStructProperty>(KeyToObjectProperty->KeyProp);
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToValueProperty->KeyProp),
			TEXT("USTRUCT member TMap<FName,FStruct> key should be FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringToValueProperty->KeyProp),
			TEXT("USTRUCT member TMap<FString,FStruct> key should be FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(BoolToValueProperty->KeyProp),
			TEXT("USTRUCT member TMap<bool,FStruct> key should be FBoolProperty")));
		ASSERT_THAT(IsNotNull(CastField<FScriptFloatProperty>(FloatToValueProperty->KeyProp),
			TEXT("USTRUCT member TMap<float,FStruct> key should use script float storage")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(ObjectToValueProperty->KeyProp),
			TEXT("USTRUCT member TMap<UObject,FStruct> key should be FObjectProperty")));
		ASSERT_THAT(IsNotNull(NameValueProperty, TEXT("USTRUCT member TMap<FName,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(StringValueProperty, TEXT("USTRUCT member TMap<FString,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(BoolValueProperty, TEXT("USTRUCT member TMap<bool,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(FloatValueProperty, TEXT("USTRUCT member TMap<float,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(ObjectValueProperty, TEXT("USTRUCT member TMap<UObject,FStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(StringKeyProperty, TEXT("USTRUCT member TMap<FStruct,FString> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(NameKeyProperty, TEXT("USTRUCT member TMap<FStruct,FName> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(BoolKeyProperty, TEXT("USTRUCT member TMap<FStruct,bool> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(FloatKeyProperty, TEXT("USTRUCT member TMap<FStruct,float> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(ObjectKeyProperty, TEXT("USTRUCT member TMap<FStruct,UObject> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(KeyToStringProperty->ValueProp),
			TEXT("USTRUCT member TMap<FStruct,FString> value should be FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(KeyToNameProperty->ValueProp),
			TEXT("USTRUCT member TMap<FStruct,FName> value should be FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(KeyToBoolProperty->ValueProp),
			TEXT("USTRUCT member TMap<FStruct,bool> value should be FBoolProperty")));
		ASSERT_THAT(IsNotNull(CastField<FScriptFloatProperty>(KeyToFloatProperty->ValueProp),
			TEXT("USTRUCT member TMap<FStruct,float> value should use script float storage")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(KeyToObjectProperty->ValueProp),
			TEXT("USTRUCT member TMap<FStruct,UObject> value should be FObjectProperty")));
		if (NameValueProperty == nullptr || NameValueProperty->Struct == nullptr
			|| StringValueProperty == nullptr || StringValueProperty->Struct == nullptr
			|| BoolValueProperty == nullptr || BoolValueProperty->Struct == nullptr
			|| FloatValueProperty == nullptr || FloatValueProperty->Struct == nullptr
			|| ObjectValueProperty == nullptr || ObjectValueProperty->Struct == nullptr
			|| StringKeyProperty == nullptr || StringKeyProperty->Struct == nullptr
			|| NameKeyProperty == nullptr || NameKeyProperty->Struct == nullptr
			|| BoolKeyProperty == nullptr || BoolKeyProperty->Struct == nullptr
			|| FloatKeyProperty == nullptr || FloatKeyProperty->Struct == nullptr
			|| ObjectKeyProperty == nullptr || ObjectKeyProperty->Struct == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(NameValueProperty->Struct, StringValueProperty->Struct,
			TEXT("USTRUCT member simple-key maps should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(NameValueProperty->Struct, BoolValueProperty->Struct,
			TEXT("USTRUCT member bool-key map should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(NameValueProperty->Struct, FloatValueProperty->Struct,
			TEXT("USTRUCT member float-key map should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(NameValueProperty->Struct, ObjectValueProperty->Struct,
			TEXT("USTRUCT member object-key map should reuse the value UScriptStruct")));
		ASSERT_THAT(AreEqual(StringKeyProperty->Struct, NameKeyProperty->Struct,
			TEXT("USTRUCT member string/name value maps should reuse the key UScriptStruct")));
		ASSERT_THAT(AreEqual(StringKeyProperty->Struct, BoolKeyProperty->Struct,
			TEXT("USTRUCT member bool value map should reuse the key UScriptStruct")));
		ASSERT_THAT(AreEqual(StringKeyProperty->Struct, FloatKeyProperty->Struct,
			TEXT("USTRUCT member float value map should reuse the key UScriptStruct")));
		ASSERT_THAT(AreEqual(StringKeyProperty->Struct, ObjectKeyProperty->Struct,
			TEXT("USTRUCT member object value map should reuse the key UScriptStruct")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct extended map-member actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameToValueFindWorked"), true,
			TEXT("USTRUCT member TMap<FName,FStruct> should support Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NameToValueFoundScore"), 102,
			TEXT("USTRUCT member TMap<FName,FStruct> should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringToValueFindWorked"), true,
			TEXT("USTRUCT member TMap<FString,FStruct> should support Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringToValueFoundScore"), 202,
			TEXT("USTRUCT member TMap<FString,FStruct> should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolToValueFindWorked"), true,
			TEXT("USTRUCT member TMap<bool,FStruct> should support Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolToValueFoundScore"), 302,
			TEXT("USTRUCT member TMap<bool,FStruct> should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatToValueFindWorked"), true,
			TEXT("USTRUCT member TMap<float,FStruct> should support Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FloatToValueFoundScore"), 402,
			TEXT("USTRUCT member TMap<float,FStruct> should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectToValueFindWorked"), true,
			TEXT("USTRUCT member TMap<UObject,FStruct> should support Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectToValueFoundScore"), 422,
			TEXT("USTRUCT member TMap<UObject,FStruct> should preserve struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToStringFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,FString> should support equivalent struct-key Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("KeyToStringFound"), FString(TEXT("StringValue")),
			TEXT("USTRUCT member TMap<FStruct,FString> should preserve string values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToNameFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,FName> should support equivalent struct-key Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("KeyToNameFound"), FName(TEXT("NameValue")),
			TEXT("USTRUCT member TMap<FStruct,FName> should preserve name values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToBoolFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,bool> should support equivalent struct-key Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToBoolFound"), true,
			TEXT("USTRUCT member TMap<FStruct,bool> should preserve bool values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToFloatFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,float> should support equivalent struct-key Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("KeyToFloatFound"), 72.5,
			TEXT("USTRUCT member TMap<FStruct,float> should preserve float values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("KeyToObjectFindWorked"), true,
			TEXT("USTRUCT member TMap<FStruct,UObject> should support equivalent struct-key Find"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("KeyToObjectFoundValue"), 802,
			TEXT("USTRUCT member TMap<FStruct,UObject> should preserve object references"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.NameToValue"), Count),
			TEXT("USTRUCT member TMap<FName,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<FName,FStruct> should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.StringToValue"), Count),
			TEXT("USTRUCT member TMap<FString,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<FString,FStruct> should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.BoolToValue"), Count),
			TEXT("USTRUCT member TMap<bool,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<bool,FStruct> should contain true/false entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.FloatToValue"), Count),
			TEXT("USTRUCT member TMap<float,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<float,FStruct> should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.ObjectToValue"), Count),
			TEXT("USTRUCT member TMap<UObject,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<UObject,FStruct> should contain two entries")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("Data.KeyToFloat"), Count),
			TEXT("USTRUCT member TMap<FStruct,float> count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("USTRUCT member TMap<FStruct,float> should contain two entries")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT reflected container parameters: C++ caller buffer through UFUNCTION.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructReflectedContainerParameterInvocation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_ReflectedContainerParameterInvocation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructReflectedContainerParameterInvocation.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FReflectedContainerItem
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FReflectedContainerItem& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 257) + Tag.GetHash();
				}
			}

			UCLASS()
			class ACoverageStructReflectedContainerActor : AActor
			{
				UPROPERTY()
				int LastArrayValueCount = 0;

				UPROPERTY()
				int LastArrayInCount = 0;

				UPROPERTY()
				int LastMapValueCount = 0;

				UPROPERTY()
				int LastMapInCount = 0;

				UPROPERTY()
				int LastSetValueCount = 0;

				UPROPERTY()
				int LastSetInCount = 0;

				UPROPERTY()
				bool bArrayValuePreserved = false;

				UPROPERTY()
				bool bMapValuePreserved = false;

				UPROPERTY()
				bool bSetValuePreserved = false;

				FReflectedContainerItem MakeItem(int ID, FName Tag)
				{
					FReflectedContainerItem Item;
					Item.ID = ID;
					Item.Tag = Tag;
					return Item;
				}

				UFUNCTION(BlueprintCallable)
				int CountArrayValue(TArray<FReflectedContainerItem> Items)
				{
					LastArrayValueCount = Items.Num();
					bArrayValuePreserved = Items.Num() == 2 && Items[1].ID == 11 && Items[1].Tag == n"ArrayValueB";
					return LastArrayValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountArrayIn(const TArray<FReflectedContainerItem>&in Items)
				{
					LastArrayInCount = Items.Num();
					return LastArrayInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillArrayOut(TArray<FReflectedContainerItem>&out Items)
				{
					Items.Add(MakeItem(20, n"ArrayOutA"));
					Items.Add(MakeItem(21, n"ArrayOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateArrayInout(TArray<FReflectedContainerItem>&inout Items)
				{
					FReflectedContainerItem First = Items[0];
					First.ID += 100;
					First.Tag = n"ArrayInoutMutated";
					Items[0] = First;
					Items.Add(MakeItem(22, n"ArrayInoutAdded"));
				}

				UFUNCTION(BlueprintCallable)
				TArray<FReflectedContainerItem> ReturnArray()
				{
					TArray<FReflectedContainerItem> Items;
					Items.Add(MakeItem(23, n"ArrayReturnA"));
					Items.Add(MakeItem(24, n"ArrayReturnB"));
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountMapValue(TMap<int, FReflectedContainerItem> Items)
				{
					LastMapValueCount = Items.Num();
					FReflectedContainerItem Found;
					bMapValuePreserved = Items.Find(12, Found) && Found.ID == 12 && Found.Tag == n"MapValueB";
					return LastMapValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountMapIn(const TMap<int, FReflectedContainerItem>&in Items)
				{
					LastMapInCount = Items.Num();
					return LastMapInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillMapOut(TMap<int, FReflectedContainerItem>&out Items)
				{
					Items.Add(30, MakeItem(30, n"MapOutA"));
					Items.Add(31, MakeItem(31, n"MapOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateMapInout(TMap<int, FReflectedContainerItem>&inout Items)
				{
					Items[10] = MakeItem(110, n"MapInoutMutated");
					Items.Add(32, MakeItem(32, n"MapInoutAdded"));
				}

				UFUNCTION(BlueprintCallable)
				TMap<int, FReflectedContainerItem> ReturnMap()
				{
					TMap<int, FReflectedContainerItem> Items;
					Items.Add(33, MakeItem(33, n"MapReturnA"));
					Items.Add(34, MakeItem(34, n"MapReturnB"));
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountSetValue(TSet<FReflectedContainerItem> Items)
				{
					LastSetValueCount = Items.Num();
					bSetValuePreserved = Items.Contains(MakeItem(11, n"SetValueB"));
					return LastSetValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountSetIn(const TSet<FReflectedContainerItem>&in Items)
				{
					LastSetInCount = Items.Num();
					return LastSetInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillSetOut(TSet<FReflectedContainerItem>&out Items)
				{
					Items.Add(MakeItem(40, n"SetOutA"));
					Items.Add(MakeItem(41, n"SetOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateSetInout(TSet<FReflectedContainerItem>&inout Items)
				{
					Items.Remove(MakeItem(10, n"SetValueA"));
					Items.Add(MakeItem(42, n"SetInoutAdded"));
				}

				UFUNCTION(BlueprintCallable)
				TSet<FReflectedContainerItem> ReturnSet()
				{
					TSet<FReflectedContainerItem> Items;
					Items.Add(MakeItem(43, n"SetReturnA"));
					Items.Add(MakeItem(44, n"SetReturnB"));
					return Items;
				}
			}
			)AS"),
			TEXT("ACoverageStructReflectedContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct reflected container parameter actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct reflected container parameter actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker ArrayValueInvoker(*TestRunner, Actor, TEXT("CountArrayValue"));
		ASSERT_THAT(IsTrue(ArrayValueInvoker.IsValid(), TEXT("CountArrayValue should be invokable")));
		if (!ArrayValueInvoker.IsValid())
		{
			return;
		}

		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(ArrayValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountArrayValue should expose TArray<FStruct> parameter slot")));
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("TArray<FStruct> value parameter should reflect as FArrayProperty")));
		FStructProperty* ItemStructProperty = ArrayProperty != nullptr ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
		ASSERT_THAT(IsNotNull(ItemStructProperty, TEXT("TArray<FStruct> inner should reflect as FStructProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr || ItemStructProperty == nullptr || ItemStructProperty->Struct == nullptr)
		{
			return;
		}

		FIntProperty* IDProperty = FindFProperty<FIntProperty>(ItemStructProperty->Struct, TEXT("ID"));
		FNameProperty* TagProperty = FindFProperty<FNameProperty>(ItemStructProperty->Struct, TEXT("Tag"));
		ASSERT_THAT(IsNotNull(IDProperty, TEXT("Reflected container item should expose ID")));
		ASSERT_THAT(IsNotNull(TagProperty, TEXT("Reflected container item should expose Tag")));
		if (IDProperty == nullptr || TagProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(AddStructItemToArray(*TestRunner, *ArrayProperty, ParamSlot, *IDProperty, *TagProperty, 10, FName(TEXT("ArrayValueA")))));
		ASSERT_THAT(IsTrue(AddStructItemToArray(*TestRunner, *ArrayProperty, ParamSlot, *IDProperty, *TagProperty, 11, FName(TEXT("ArrayValueB")))));
		ASSERT_THAT(AreEqual(2, ArrayValueInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TArray<FStruct> by-value parameter should count caller-provided items")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastArrayValueCount"), 2,
			TEXT("Reflected TArray<FStruct> by-value call should update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bArrayValuePreserved"), true,
			TEXT("Reflected TArray<FStruct> by-value call should preserve struct fields"))));

		FFunctionInvoker ArrayInInvoker(*TestRunner, Actor, TEXT("CountArrayIn"));
		ASSERT_THAT(IsTrue(ArrayInInvoker.IsValid(), TEXT("CountArrayIn should be invokable")));
		if (!ArrayInInvoker.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ArrayInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountArrayIn should expose TArray<FStruct> const ref parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("TArray<FStruct> &in parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToArray(*TestRunner, *ArrayProperty, ParamSlot, *IDProperty, *TagProperty, 12, FName(TEXT("ArrayInA")))));
		ASSERT_THAT(IsTrue(AddStructItemToArray(*TestRunner, *ArrayProperty, ParamSlot, *IDProperty, *TagProperty, 13, FName(TEXT("ArrayInB")))));
		ASSERT_THAT(AreEqual(2, ArrayInInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TArray<FStruct> &in parameter should count caller-provided items")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastArrayInCount"), 2,
			TEXT("Reflected TArray<FStruct> &in call should update script-side state"))));

		FFunctionInvoker ArrayOutInvoker(*TestRunner, Actor, TEXT("FillArrayOut"));
		ASSERT_THAT(IsTrue(ArrayOutInvoker.IsValid(), TEXT("FillArrayOut should be invokable")));
		if (!ArrayOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillArrayOut should expose TArray<FStruct> out parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("TArray<FStruct> &out parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayOutInvoker.Call(), TEXT("FillArrayOut should execute through reflection")));
		FScriptArrayHelper ArrayOutHelper(ArrayProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, ArrayOutHelper.Num(), TEXT("Reflected TArray<FStruct> &out parameter should write two items")));
		const void* ArrayItemAddress = nullptr;
		ASSERT_THAT(IsTrue(GetArrayStructItem(*TestRunner, *ArrayProperty, ParamSlot, 1, ArrayItemAddress)));
		if (ArrayItemAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructItemFields(*TestRunner, *IDProperty, *TagProperty, ArrayItemAddress, 21, FName(TEXT("ArrayOutB")),
			TEXT("Reflected TArray<FStruct> &out second item"))));

		FFunctionInvoker ArrayInoutInvoker(*TestRunner, Actor, TEXT("MutateArrayInout"));
		ASSERT_THAT(IsTrue(ArrayInoutInvoker.IsValid(), TEXT("MutateArrayInout should be invokable")));
		if (!ArrayInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateArrayInout should expose TArray<FStruct> inout parameter slot")));
		ArrayProperty = CastField<FArrayProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("TArray<FStruct> &inout parameter should reflect as FArrayProperty")));
		if (ParamSlot == nullptr || ArrayProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToArray(*TestRunner, *ArrayProperty, ParamSlot, *IDProperty, *TagProperty, 14, FName(TEXT("ArrayInoutA")))));
		ASSERT_THAT(IsTrue(ArrayInoutInvoker.Call(), TEXT("MutateArrayInout should execute through reflection")));
		FScriptArrayHelper ArrayInoutHelper(ArrayProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, ArrayInoutHelper.Num(), TEXT("Reflected TArray<FStruct> &inout parameter should append one item")));
		ArrayItemAddress = nullptr;
		ASSERT_THAT(IsTrue(GetArrayStructItem(*TestRunner, *ArrayProperty, ParamSlot, 0, ArrayItemAddress)));
		if (ArrayItemAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructItemFields(*TestRunner, *IDProperty, *TagProperty, ArrayItemAddress, 114, FName(TEXT("ArrayInoutMutated")),
			TEXT("Reflected TArray<FStruct> &inout first item"))));

		FFunctionInvoker ArrayReturnInvoker(*TestRunner, Actor, TEXT("ReturnArray"));
		ASSERT_THAT(IsTrue(ArrayReturnInvoker.IsValid(), TEXT("ReturnArray should be invokable")));
		if (!ArrayReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ArrayReturnInvoker.Call(), TEXT("ReturnArray should execute through reflection")));
		UFunction* ReturnArrayFunction = Actor->FindFunction(TEXT("ReturnArray"));
		ASSERT_THAT(IsNotNull(ReturnArrayFunction, TEXT("ReturnArray should reflect as a UFunction")));
		if (ReturnArrayFunction == nullptr)
		{
			return;
		}
		FArrayProperty* ArrayReturnProperty = CastField<FArrayProperty>(ReturnArrayFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ArrayReturnProperty, TEXT("TArray<FStruct> return should reflect as FArrayProperty")));
		if (ArrayReturnProperty == nullptr)
		{
			return;
		}
		void* ReturnSlot = ArrayReturnProperty->ContainerPtrToValuePtr<void>(ArrayReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TArray<FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptArrayHelper ArrayReturnHelper(ArrayReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ArrayReturnHelper.Num(), TEXT("Reflected TArray<FStruct> return should contain two items")));
		ArrayItemAddress = nullptr;
		ASSERT_THAT(IsTrue(GetArrayStructItem(*TestRunner, *ArrayReturnProperty, ReturnSlot, 1, ArrayItemAddress)));
		if (ArrayItemAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructItemFields(*TestRunner, *IDProperty, *TagProperty, ArrayItemAddress, 24, FName(TEXT("ArrayReturnB")),
			TEXT("Reflected TArray<FStruct> return second item"))));

		FFunctionInvoker MapValueInvoker(*TestRunner, Actor, TEXT("CountMapValue"));
		ASSERT_THAT(IsTrue(MapValueInvoker.IsValid(), TEXT("CountMapValue should be invokable")));
		if (!MapValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountMapValue should expose TMap<int,FStruct> parameter slot")));
		FMapProperty* MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<int,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 10, 10, FName(TEXT("MapValueA")))));
		ASSERT_THAT(IsTrue(AddStructItemToMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 12, 12, FName(TEXT("MapValueB")))));
		ASSERT_THAT(AreEqual(2, MapValueInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TMap<int,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastMapValueCount"), 2,
			TEXT("Reflected TMap<int,FStruct> by-value call should update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bMapValuePreserved"), true,
			TEXT("Reflected TMap<int,FStruct> by-value call should preserve struct fields"))));

		FFunctionInvoker MapInInvoker(*TestRunner, Actor, TEXT("CountMapIn"));
		ASSERT_THAT(IsTrue(MapInInvoker.IsValid(), TEXT("CountMapIn should be invokable")));
		if (!MapInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountMapIn should expose TMap<int,FStruct> const ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<int,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 13, 13, FName(TEXT("MapInA")))));
		ASSERT_THAT(IsTrue(AddStructItemToMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 14, 14, FName(TEXT("MapInB")))));
		ASSERT_THAT(AreEqual(2, MapInInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TMap<int,FStruct> &in parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastMapInCount"), 2,
			TEXT("Reflected TMap<int,FStruct> &in call should update script-side state"))));

		FFunctionInvoker MapOutInvoker(*TestRunner, Actor, TEXT("FillMapOut"));
		ASSERT_THAT(IsTrue(MapOutInvoker.IsValid(), TEXT("FillMapOut should be invokable")));
		if (!MapOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillMapOut should expose TMap<int,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<int,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.Call(), TEXT("FillMapOut should execute through reflection")));
		FScriptMapHelper MapOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapOutHelper.Num(), TEXT("Reflected TMap<int,FStruct> &out parameter should write two entries")));
		const FStructProperty* MapValueStructProperty = nullptr;
		const void* MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValue(*TestRunner, *MapProperty, ParamSlot, 31, MapValueStructProperty, MapValueAddress)));
		if (MapValueStructProperty == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructItemFields(*TestRunner, *IDProperty, *TagProperty, MapValueAddress, 31, FName(TEXT("MapOutB")),
			TEXT("Reflected TMap<int,FStruct> &out second value"))));

		FFunctionInvoker MapInoutInvoker(*TestRunner, Actor, TEXT("MutateMapInout"));
		ASSERT_THAT(IsTrue(MapInoutInvoker.IsValid(), TEXT("MutateMapInout should be invokable")));
		if (!MapInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateMapInout should expose TMap<int,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<int,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 10, 10, FName(TEXT("MapInoutA")))));
		ASSERT_THAT(IsTrue(MapInoutInvoker.Call(), TEXT("MutateMapInout should execute through reflection")));
		FScriptMapHelper MapInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapInoutHelper.Num(), TEXT("Reflected TMap<int,FStruct> &inout parameter should contain two entries")));
		MapValueStructProperty = nullptr;
		MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValue(*TestRunner, *MapProperty, ParamSlot, 10, MapValueStructProperty, MapValueAddress)));
		if (MapValueStructProperty == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructItemFields(*TestRunner, *IDProperty, *TagProperty, MapValueAddress, 110, FName(TEXT("MapInoutMutated")),
			TEXT("Reflected TMap<int,FStruct> &inout replaced value"))));

		FFunctionInvoker MapReturnInvoker(*TestRunner, Actor, TEXT("ReturnMap"));
		ASSERT_THAT(IsTrue(MapReturnInvoker.IsValid(), TEXT("ReturnMap should be invokable")));
		if (!MapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapReturnInvoker.Call(), TEXT("ReturnMap should execute through reflection")));
		UFunction* ReturnMapFunction = Actor->FindFunction(TEXT("ReturnMap"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnMap should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<int,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(MapReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<int,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper MapReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, MapReturnHelper.Num(), TEXT("Reflected TMap<int,FStruct> return should contain two entries")));
		MapValueStructProperty = nullptr;
		MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValue(*TestRunner, *MapReturnProperty, ReturnSlot, 34, MapValueStructProperty, MapValueAddress)));
		if (MapValueStructProperty == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructItemFields(*TestRunner, *IDProperty, *TagProperty, MapValueAddress, 34, FName(TEXT("MapReturnB")),
			TEXT("Reflected TMap<int,FStruct> return second value"))));

		FFunctionInvoker SetValueInvoker(*TestRunner, Actor, TEXT("CountSetValue"));
		ASSERT_THAT(IsTrue(SetValueInvoker.IsValid(), TEXT("CountSetValue should be invokable")));
		if (!SetValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountSetValue should expose TSet<FStruct> parameter slot")));
		FSetProperty* SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> value parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 10, FName(TEXT("SetValueA")))));
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 11, FName(TEXT("SetValueB")))));
		ASSERT_THAT(AreEqual(2, SetValueInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TSet<FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastSetValueCount"), 2,
			TEXT("Reflected TSet<FStruct> by-value call should update script-side state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetValuePreserved"), true,
			TEXT("Reflected TSet<FStruct> by-value call should preserve struct fields"))));

		FFunctionInvoker SetInInvoker(*TestRunner, Actor, TEXT("CountSetIn"));
		ASSERT_THAT(IsTrue(SetInInvoker.IsValid(), TEXT("CountSetIn should be invokable")));
		if (!SetInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountSetIn should expose TSet<FStruct> const ref parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> &in parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 12, FName(TEXT("SetInA")))));
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 13, FName(TEXT("SetInB")))));
		ASSERT_THAT(AreEqual(2, SetInInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TSet<FStruct> &in parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastSetInCount"), 2,
			TEXT("Reflected TSet<FStruct> &in call should update script-side state"))));

		FFunctionInvoker SetOutInvoker(*TestRunner, Actor, TEXT("FillSetOut"));
		ASSERT_THAT(IsTrue(SetOutInvoker.IsValid(), TEXT("FillSetOut should be invokable")));
		if (!SetOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillSetOut should expose TSet<FStruct> out parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> &out parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetOutInvoker.Call(), TEXT("FillSetOut should execute through reflection")));
		FScriptSetHelper SetOutHelper(SetProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, SetOutHelper.Num(), TEXT("Reflected TSet<FStruct> &out parameter should write two entries")));
		ASSERT_THAT(IsTrue(SetContainsStructItem(*SetProperty, ParamSlot, *IDProperty, *TagProperty, 41, FName(TEXT("SetOutB"))),
			TEXT("Reflected TSet<FStruct> &out should preserve struct fields")));

		FFunctionInvoker SetInoutInvoker(*TestRunner, Actor, TEXT("MutateSetInout"));
		ASSERT_THAT(IsTrue(SetInoutInvoker.IsValid(), TEXT("MutateSetInout should be invokable")));
		if (!SetInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateSetInout should expose TSet<FStruct> inout parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> &inout parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 10, FName(TEXT("SetValueA")))));
		ASSERT_THAT(IsTrue(SetInoutInvoker.Call(), TEXT("MutateSetInout should execute through reflection")));
		FScriptSetHelper SetInoutHelper(SetProperty, ParamSlot);
		ASSERT_THAT(AreEqual(1, SetInoutHelper.Num(), TEXT("Reflected TSet<FStruct> &inout parameter should remove one and add one entry")));
		ASSERT_THAT(IsTrue(SetContainsStructItem(*SetProperty, ParamSlot, *IDProperty, *TagProperty, 42, FName(TEXT("SetInoutAdded"))),
			TEXT("Reflected TSet<FStruct> &inout should preserve added struct fields")));

		FFunctionInvoker SetReturnInvoker(*TestRunner, Actor, TEXT("ReturnSet"));
		ASSERT_THAT(IsTrue(SetReturnInvoker.IsValid(), TEXT("ReturnSet should be invokable")));
		if (!SetReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetReturnInvoker.Call(), TEXT("ReturnSet should execute through reflection")));
		UFunction* ReturnSetFunction = Actor->FindFunction(TEXT("ReturnSet"));
		ASSERT_THAT(IsNotNull(ReturnSetFunction, TEXT("ReturnSet should reflect as a UFunction")));
		if (ReturnSetFunction == nullptr)
		{
			return;
		}
		FSetProperty* SetReturnProperty = CastField<FSetProperty>(ReturnSetFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(SetReturnProperty, TEXT("TSet<FStruct> return should reflect as FSetProperty")));
		if (SetReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = SetReturnProperty->ContainerPtrToValuePtr<void>(SetReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TSet<FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptSetHelper SetReturnHelper(SetReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, SetReturnHelper.Num(), TEXT("Reflected TSet<FStruct> return should contain two entries")));
		ASSERT_THAT(IsTrue(SetContainsStructItem(*SetReturnProperty, ReturnSlot, *IDProperty, *TagProperty, 44, FName(TEXT("SetReturnB"))),
			TEXT("Reflected TSet<FStruct> return should preserve struct fields")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT in containers: TArray<FStruct>, TMap with struct keys/values
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructInContainers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Containers"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructContainers.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FItemStruct
			{
				UPROPERTY()
				int ItemID = 0;

				UPROPERTY()
				FString ItemName;

				UPROPERTY()
				float Weight = 0.0f;

				bool opEquals(const FItemStruct& Other) const
				{
					return ItemID == Other.ItemID;
				}

				int opCmp(const FItemStruct& Other) const
				{
					if (ItemID < Other.ItemID) return -1;
					if (ItemID > Other.ItemID) return 1;
					return 0;
				}
			}

			UCLASS()
			class ACoverageStructContainerActor : AActor
			{
				UPROPERTY()
				TArray<FItemStruct> ItemArray;

				UPROPERTY()
				TMap<int, FItemStruct> IDToItemMap;

				UPROPERTY()
				TMap<FString, FItemStruct> NameToItemMap;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Populate TArray<FStruct>
					FItemStruct Item1;
					Item1.ItemID = 1;
					Item1.ItemName = "Sword";
					Item1.Weight = 5.0f;
					ItemArray.Add(Item1);

					FItemStruct Item2;
					Item2.ItemID = 2;
					Item2.ItemName = "Shield";
					Item2.Weight = 10.0f;
					ItemArray.Add(Item2);

					FItemStruct Item3;
					Item3.ItemID = 3;
					Item3.ItemName = "Potion";
					Item3.Weight = 0.5f;
					ItemArray.Add(Item3);

					// Populate TMap<int, FStruct>
					IDToItemMap.Add(1, Item1);
					IDToItemMap.Add(2, Item2);
					IDToItemMap.Add(3, Item3);

					// Populate TMap<FString, FStruct>
					NameToItemMap.Add("Sword", Item1);
					NameToItemMap.Add("Shield", Item2);
					NameToItemMap.Add("Potion", Item3);
				}
			}
			)AS"),
			TEXT("ACoverageStructContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct container actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify TArray<FStruct>
		{
			int32 Length = 0;
			ASSERT_THAT(IsTrue(GetArrayNumByPath(*TestRunner, Actor, TEXT("ItemArray"), Length), TEXT("TArray<FStruct> length should resolve")));
			ASSERT_THAT(AreEqual(3, Length, TEXT("TArray<FStruct> should have 3 elements")));
		}
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ItemArray[0].ItemID"), 1, TEXT("TArray<FStruct>[0].ItemID"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ItemArray[0].ItemName"), FString(TEXT("Sword")), TEXT("TArray<FStruct>[0].ItemName"));
		VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("ItemArray[0].Weight"), 5.0, TEXT("TArray<FStruct>[0].Weight"));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ItemArray[1].ItemID"), 2, TEXT("TArray<FStruct>[1].ItemID"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("ItemArray[1].ItemName"), FString(TEXT("Shield")), TEXT("TArray<FStruct>[1].ItemName"));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ItemArray[2].ItemID"), 3, TEXT("TArray<FStruct>[2].ItemID"));
		VerifyByPath<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, Actor, TEXT("ItemArray[2].Weight"), 0.5, TEXT("TArray<FStruct>[2].Weight"));

		// Verify TMap<int, FStruct>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("IDToItemMap"), Count), TEXT("TMap<int, FStruct> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<int, FStruct> should have 3 entries")));
		}

		const FStructProperty* ItemMapValueStructProperty = nullptr;
		const void* ShieldValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath(*TestRunner, Actor, TEXT("IDToItemMap"), 2, ItemMapValueStructProperty, ShieldValueAddress),
			TEXT("TMap<int,FStruct> value should be readable by key")));
		if (ItemMapValueStructProperty == nullptr || ItemMapValueStructProperty->Struct == nullptr || ShieldValueAddress == nullptr)
		{
			return;
		}

		FIntProperty* ItemIDProperty = FindFProperty<FIntProperty>(ItemMapValueStructProperty->Struct, TEXT("ItemID"));
		FStrProperty* ItemNameProperty = FindFProperty<FStrProperty>(ItemMapValueStructProperty->Struct, TEXT("ItemName"));
		FScriptFloatProperty* WeightProperty = FindFProperty<FScriptFloatProperty>(ItemMapValueStructProperty->Struct, TEXT("Weight"));
		ASSERT_THAT(IsNotNull(ItemIDProperty, TEXT("TMap<int,FStruct> value should expose ItemID")));
		ASSERT_THAT(IsNotNull(ItemNameProperty, TEXT("TMap<int,FStruct> value should expose ItemName")));
		ASSERT_THAT(IsNotNull(WeightProperty, TEXT("TMap<int,FStruct> value should expose Weight")));
		if (ItemIDProperty == nullptr || ItemNameProperty == nullptr || WeightProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(2, ItemIDProperty->GetPropertyValue_InContainer(ShieldValueAddress),
			TEXT("TMap<int,FStruct> should preserve struct int value fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("Shield")), ItemNameProperty->GetPropertyValue_InContainer(ShieldValueAddress),
			TEXT("TMap<int,FStruct> should preserve struct string value fields")));
		ASSERT_THAT(IsNear(10.0, WeightProperty->GetPropertyValue_InContainer(ShieldValueAddress), 0.0001,
			TEXT("TMap<int,FStruct> should preserve struct float value fields")));

		// Verify TMap<FString, FStruct>
		{
			int32 Count = 0;
			ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToItemMap"), Count), TEXT("TMap<FString, FStruct> length should resolve")));
			ASSERT_THAT(AreEqual(3, Count, TEXT("TMap<FString, FStruct> should have 3 entries")));
		}

		const FStructProperty* StringMapValueStructProperty = nullptr;
		const void* PotionValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath<FStrProperty, FString>(
			*TestRunner,
			Actor,
			TEXT("NameToItemMap"),
			FString(TEXT("Potion")),
			StringMapValueStructProperty,
			PotionValueAddress),
			TEXT("TMap<FString,FStruct> value should be readable by key")));
		if (StringMapValueStructProperty == nullptr || StringMapValueStructProperty->Struct == nullptr || PotionValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(ItemMapValueStructProperty->Struct, StringMapValueStructProperty->Struct,
			TEXT("TMap<int,FStruct> and TMap<FString,FStruct> values should share the same generated UScriptStruct")));
		if (StringMapValueStructProperty->Struct != ItemMapValueStructProperty->Struct)
		{
			return;
		}
		ASSERT_THAT(AreEqual(3, ItemIDProperty->GetPropertyValue_InContainer(PotionValueAddress),
			TEXT("TMap<FString,FStruct> should preserve struct int value fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("Potion")), ItemNameProperty->GetPropertyValue_InContainer(PotionValueAddress),
			TEXT("TMap<FString,FStruct> should preserve struct string value fields")));
		ASSERT_THAT(IsNear(0.5, WeightProperty->GetPropertyValue_InContainer(PotionValueAddress), 0.0001,
			TEXT("TMap<FString,FStruct> should preserve struct float value fields")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT hashable containers: script structs as TMap keys and TSet elements.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructHashableMapKeyAndSetElement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_HashableMapKeyAndSetElement"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructHashableMapKeyAndSetElement.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FHashableStructKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FHashableStructKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 31) + Tag.GetHash();
				}
			}

			UCLASS()
			class ACoverageStructHashableContainerActor : AActor
			{
				UPROPERTY()
				TMap<FHashableStructKey, int> StructToIntMap;

				UPROPERTY()
				TSet<FHashableStructKey> StructSet;

				UPROPERTY()
				bool MapContainsOriginal = false;

				UPROPERTY()
				bool MapFindOriginal = false;

				UPROPERTY()
				int MapFoundValue = 0;

				UPROPERTY()
				bool MapOverwriteWorked = false;

				UPROPERTY()
				bool SetContainsOriginal = false;

				UPROPERTY()
				bool SetDedupWorked = false;

				UPROPERTY()
				bool SetRemoveWorked = false;

				FHashableStructKey MakeKey(int ID, FName Tag)
				{
					FHashableStructKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FHashableStructKey Alpha = MakeKey(1, n"Alpha");
					FHashableStructKey AlphaDuplicate = MakeKey(1, n"Alpha");
					FHashableStructKey Beta = MakeKey(2, n"Beta");

					StructToIntMap.Add(Alpha, 10);
					StructToIntMap.Add(Beta, 20);
					MapContainsOriginal = StructToIntMap.Contains(AlphaDuplicate);
					MapFindOriginal = StructToIntMap.Find(AlphaDuplicate, MapFoundValue);
					StructToIntMap.Add(AlphaDuplicate, 15);
					MapOverwriteWorked = StructToIntMap[Alpha] == 15;

					StructSet.Add(Alpha);
					StructSet.Add(AlphaDuplicate);
					StructSet.Add(Beta);
					SetContainsOriginal = StructSet.Contains(AlphaDuplicate);
					SetDedupWorked = StructSet.Num() == 2;
					SetRemoveWorked = StructSet.Remove(AlphaDuplicate) && !StructSet.Contains(Alpha);
				}
			}
			)AS"),
			TEXT("ACoverageStructHashableContainerActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct hashable container actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* StructToIntMapKey = nullptr;
		if (FMapProperty* StructToIntMapProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructToIntMap")))
		{
			StructToIntMapKey = CastField<FStructProperty>(StructToIntMapProperty->KeyProp);
			ASSERT_THAT(IsNotNull(StructToIntMapKey, TEXT("TMap<FStruct,int> key should reflect as FStructProperty")));
			ASSERT_THAT(IsNotNull(CastField<FIntProperty>(StructToIntMapProperty->ValueProp), TEXT("TMap<FStruct,int> value should reflect as FIntProperty")));
		}
		else
		{
			ASSERT_THAT(IsNotNull(StructToIntMapProperty, TEXT("StructToIntMap should reflect as FMapProperty")));
			return;
		}

		FSetProperty* StructSetProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("StructSet"));
		ASSERT_THAT(IsNotNull(StructSetProperty, TEXT("StructSet should reflect as FSetProperty")));
		if (StructSetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructSetProperty->ElementProp), TEXT("TSet<FStruct> element should reflect as FStructProperty")));
		if (StructToIntMapKey != nullptr)
		{
			UScriptStruct::ICppStructOps* CppStructOps = StructToIntMapKey->Struct != nullptr ? StructToIntMapKey->Struct->GetCppStructOps() : nullptr;
			ASSERT_THAT(IsNotNull(CppStructOps, TEXT("hashable AS struct key should expose cpp struct ops")));
			if (CppStructOps == nullptr)
			{
				return;
			}
			ASSERT_THAT(IsTrue(CppStructOps->HasGetTypeHash(), TEXT("hashable AS struct key should expose GetTypeHash")));
			ASSERT_THAT(IsTrue(EnumHasAnyFlags(CppStructOps->GetComputedPropertyFlags(), CPF_HasGetValueTypeHash),
				TEXT("hashable AS struct key should carry CPF_HasGetValueTypeHash as a computed property flag")));
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct hashable container actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		int32 MapCount = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructToIntMap"), MapCount), TEXT("TMap<FStruct,int> count should resolve")));
		ASSERT_THAT(AreEqual(2, MapCount, TEXT("TMap<FStruct,int> should deduplicate equal struct keys")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapContainsOriginal"), true, TEXT("TMap<FStruct,int>.Contains should use opEquals and Hash"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapFindOriginal"), true, TEXT("TMap<FStruct,int>.Find should use opEquals and Hash"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("MapFoundValue"), 10, TEXT("TMap<FStruct,int>.Find should copy the original value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("MapOverwriteWorked"), true, TEXT("TMap<FStruct,int>.Add should overwrite equal struct keys"))));

		int32 SetCount = 0;
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StructSet"), SetCount), TEXT("TSet<FStruct> count should resolve")));
		ASSERT_THAT(AreEqual(1, SetCount, TEXT("TSet<FStruct> should contain only Beta after removing Alpha")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SetContainsOriginal"), true, TEXT("TSet<FStruct>.Contains should use opEquals and Hash"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SetDedupWorked"), true, TEXT("TSet<FStruct> should deduplicate equal struct elements"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("SetRemoveWorked"), true, TEXT("TSet<FStruct>.Remove should remove matching struct element"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT hashable key containers: TMap/TSet parameter and return combinations.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructKeyContainerParameterAndReturnMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_KeyContainerParameterAndReturnMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructKeyContainerParameterAndReturnMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructContainerKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FStructContainerKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 313) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FStructContainerValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructKeyContainerMatrixActor : AActor
			{
				UPROPERTY()
				int StructKeyMapValueCount = 0;

				UPROPERTY()
				int StructKeyMapInCount = 0;

				UPROPERTY()
				TMap<FStructContainerKey, int> StructKeyMapInout;

				UPROPERTY()
				TMap<FStructContainerKey, int> StructKeyMapOut;

				UPROPERTY()
				TMap<FStructContainerKey, int> StructKeyMapReturn;

				UPROPERTY()
				bool StructKeyMapValueContains = false;

				UPROPERTY()
				bool StructKeyMapInContains = false;

				UPROPERTY()
				bool StructKeyMapInoutMutated = false;

				UPROPERTY()
				bool StructValueMapReturnContains = false;

				UPROPERTY()
				int StructValueMapReturnScore = 0;

				UPROPERTY()
				int StructSetValueCount = 0;

				UPROPERTY()
				int StructSetInCount = 0;

				UPROPERTY()
				TSet<FStructContainerKey> StructSetInout;

				UPROPERTY()
				TSet<FStructContainerKey> StructSetOut;

				UPROPERTY()
				TSet<FStructContainerKey> StructSetReturn;

				UPROPERTY()
				bool StructSetValueContains = false;

				UPROPERTY()
				bool StructSetInContains = false;

				UPROPERTY()
				bool StructSetInoutMutated = false;

				FStructContainerKey MakeKey(int ID, FName Tag)
				{
					FStructContainerKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FStructContainerValue MakeValue(int Score, FString Label)
				{
					FStructContainerValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructKeyMapValue(TMap<FStructContainerKey, int> Items)
				{
					StructKeyMapValueCount = Items.Num();
					StructKeyMapValueContains = Items.Contains(MakeKey(11, n"MapValueB"));
					return StructKeyMapValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructKeyMapIn(const TMap<FStructContainerKey, int>&in Items)
				{
					StructKeyMapInCount = Items.Num();
					StructKeyMapInContains = Items.Contains(MakeKey(13, n"MapInB"));
					return StructKeyMapInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructKeyMapOut(TMap<FStructContainerKey, int>&out Items)
				{
					Items.Add(MakeKey(14, n"MapOutA"), 114);
					Items.Add(MakeKey(15, n"MapOutB"), 115);
					StructKeyMapOut = Items;
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructKeyMapInout(TMap<FStructContainerKey, int>&inout Items)
				{
					FStructContainerKey Existing = MakeKey(20, n"MapInoutA");
					Items.Remove(Existing);
					Items.Add(Existing, 220);
					Items.Add(MakeKey(21, n"MapInoutB"), 221);
					StructKeyMapInout = Items;
					int MutatedValue = 0;
					StructKeyMapInoutMutated = Items.Find(Existing, MutatedValue) && MutatedValue == 220;
				}

				UFUNCTION(BlueprintCallable)
				TMap<FStructContainerKey, int> ReturnStructKeyMap()
				{
					TMap<FStructContainerKey, int> Items;
					Items.Add(MakeKey(30, n"MapReturnA"), 330);
					Items.Add(MakeKey(31, n"MapReturnB"), 331);
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				TMap<int, FStructContainerValue> ReturnStructValueMap()
				{
					TMap<int, FStructContainerValue> Items;
					Items.Add(40, MakeValue(440, "MapValueReturnA"));
					Items.Add(41, MakeValue(441, "MapValueReturnB"));

					FStructContainerValue Found;
					StructValueMapReturnContains = Items.Find(41, Found);
					StructValueMapReturnScore = Found.Score;
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructSetValue(TSet<FStructContainerKey> Items)
				{
					StructSetValueCount = Items.Num();
					StructSetValueContains = Items.Contains(MakeKey(51, n"SetValueB"));
					return StructSetValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructSetIn(const TSet<FStructContainerKey>&in Items)
				{
					StructSetInCount = Items.Num();
					StructSetInContains = Items.Contains(MakeKey(53, n"SetInB"));
					return StructSetInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructSetOut(TSet<FStructContainerKey>&out Items)
				{
					Items.Add(MakeKey(54, n"SetOutA"));
					Items.Add(MakeKey(55, n"SetOutB"));
					StructSetOut = Items;
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructSetInout(TSet<FStructContainerKey>&inout Items)
				{
					Items.Remove(MakeKey(60, n"SetInoutA"));
					Items.Add(MakeKey(61, n"SetInoutB"));
					StructSetInout = Items;
					StructSetInoutMutated = !Items.Contains(MakeKey(60, n"SetInoutA")) && Items.Contains(MakeKey(61, n"SetInoutB"));
				}

				UFUNCTION(BlueprintCallable)
				TSet<FStructContainerKey> ReturnStructSet()
				{
					TSet<FStructContainerKey> Items;
					Items.Add(MakeKey(70, n"SetReturnA"));
					Items.Add(MakeKey(71, n"SetReturnB"));
					return Items;
				}
			}
			)AS"),
			TEXT("ACoverageStructKeyContainerMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct key-container matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct key-container matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FMapProperty* StructKeyMapInoutProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructKeyMapInout"));
		FSetProperty* StructSetInoutProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("StructSetInout"));
		ASSERT_THAT(IsNotNull(StructKeyMapInoutProperty, TEXT("TMap<FStruct,int> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StructSetInoutProperty, TEXT("TSet<FStruct> inout storage should reflect")));
		if (StructKeyMapInoutProperty == nullptr || StructSetInoutProperty == nullptr)
		{
			return;
		}

		const FStructProperty* StructKeyProperty = CastField<FStructProperty>(StructKeyMapInoutProperty->KeyProp);
		const FStructProperty* StructSetElementProperty = CastField<FStructProperty>(StructSetInoutProperty->ElementProp);
		ASSERT_THAT(IsNotNull(StructKeyProperty, TEXT("TMap<FStruct,int> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(StructSetElementProperty, TEXT("TSet<FStruct> element should expose the AS key struct")));
		if (StructKeyProperty == nullptr || StructKeyProperty->Struct == nullptr
			|| StructSetElementProperty == nullptr || StructSetElementProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(StructKeyProperty->Struct, StructSetElementProperty->Struct,
			TEXT("TMap key and TSet element should share the same generated AS UScriptStruct")));
		if (StructKeyProperty->Struct != StructSetElementProperty->Struct)
		{
			return;
		}

		FIntProperty* IDProperty = FindFProperty<FIntProperty>(StructKeyProperty->Struct, TEXT("ID"));
		FNameProperty* TagProperty = FindFProperty<FNameProperty>(StructKeyProperty->Struct, TEXT("Tag"));
		ASSERT_THAT(IsNotNull(IDProperty, TEXT("Hashable key struct should expose ID")));
		ASSERT_THAT(IsNotNull(TagProperty, TEXT("Hashable key struct should expose Tag")));
		if (IDProperty == nullptr || TagProperty == nullptr)
		{
			return;
		}

		FFunctionInvoker MapValueInvoker(*TestRunner, Actor, TEXT("CountStructKeyMapValue"));
		ASSERT_THAT(IsTrue(MapValueInvoker.IsValid(), TEXT("CountStructKeyMapValue should be invokable")));
		if (!MapValueInvoker.IsValid())
		{
			return;
		}
		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(MapValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructKeyMapValue should expose TMap<FStruct,int> parameter slot")));
		FMapProperty* MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,int> value parameter should reflect as FMapProperty")));
		FStructProperty* MapKeyParameterProperty = MapProperty != nullptr ? CastField<FStructProperty>(MapProperty->KeyProp) : nullptr;
		ASSERT_THAT(IsNotNull(MapKeyParameterProperty, TEXT("TMap<FStruct,int> value parameter key should be FStructProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr || MapKeyParameterProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(StructKeyProperty->Struct, MapKeyParameterProperty->Struct,
			TEXT("TMap<FStruct,int> value parameter key should use the same generated AS struct")));
		if (MapKeyParameterProperty == nullptr || MapKeyParameterProperty->Struct != StructKeyProperty->Struct)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyToIntMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 10, FName(TEXT("MapValueA")), 110)));
		ASSERT_THAT(IsTrue(AddStructKeyToIntMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 11, FName(TEXT("MapValueB")), 111)));
		ASSERT_THAT(AreEqual(2, MapValueInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TMap<FStruct,int> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructKeyMapValueCount"), 2,
			TEXT("TMap<FStruct,int> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructKeyMapValueContains"), true,
			TEXT("TMap<FStruct,int> by-value call should preserve hashable struct keys"))));

		FFunctionInvoker MapInInvoker(*TestRunner, Actor, TEXT("CountStructKeyMapIn"));
		ASSERT_THAT(IsTrue(MapInInvoker.IsValid(), TEXT("CountStructKeyMapIn should be invokable")));
		if (!MapInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructKeyMapIn should expose TMap<FStruct,int> const ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,int> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyToIntMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 12, FName(TEXT("MapInA")), 112)));
		ASSERT_THAT(IsTrue(AddStructKeyToIntMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 13, FName(TEXT("MapInB")), 113)));
		ASSERT_THAT(AreEqual(2, MapInInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TMap<FStruct,int> &in parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructKeyMapInContains"), true,
			TEXT("TMap<FStruct,int> &in call should preserve hashable struct keys"))));

		FFunctionInvoker MapOutInvoker(*TestRunner, Actor, TEXT("FillStructKeyMapOut"));
		ASSERT_THAT(IsTrue(MapOutInvoker.IsValid(), TEXT("FillStructKeyMapOut should be invokable")));
		if (!MapOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructKeyMapOut should expose TMap<FStruct,int> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,int> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.Call(), TEXT("FillStructKeyMapOut should execute through reflection")));
		FScriptMapHelper MapOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapOutHelper.Num(), TEXT("TMap<FStruct,int> &out should write two entries")));
		int32 OutFoundIntValue = 0;
		ASSERT_THAT(IsTrue(GetIntMapValueByStructKey(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 15, FName(TEXT("MapOutB")), OutFoundIntValue)));
		ASSERT_THAT(AreEqual(115, OutFoundIntValue, TEXT("TMap<FStruct,int> &out should preserve hashable struct keys")));
		int32 OutCount = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructKeyMapOut"), OutCount),
			TEXT("TMap<FStruct,int> &out should be assignable to script-side storage")));
		ASSERT_THAT(AreEqual(2, OutCount, TEXT("TMap<FStruct,int> &out storage should contain two entries")));

		FFunctionInvoker MapInoutInvoker(*TestRunner, Actor, TEXT("MutateStructKeyMapInout"));
		ASSERT_THAT(IsTrue(MapInoutInvoker.IsValid(), TEXT("MutateStructKeyMapInout should be invokable")));
		if (!MapInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructKeyMapInout should expose TMap<FStruct,int> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,int> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyToIntMap(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 20, FName(TEXT("MapInoutA")), 120)));
		ASSERT_THAT(IsTrue(MapInoutInvoker.Call(), TEXT("MutateStructKeyMapInout should execute through reflection")));
		FScriptMapHelper MapInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapInoutHelper.Num(), TEXT("TMap<FStruct,int> &inout should add one entry")));
		int32 FoundIntValue = 0;
		ASSERT_THAT(IsTrue(GetIntMapValueByStructKey(*TestRunner, *MapProperty, ParamSlot, *IDProperty, *TagProperty, 20, FName(TEXT("MapInoutA")), FoundIntValue)));
		ASSERT_THAT(AreEqual(220, FoundIntValue, TEXT("TMap<FStruct,int> &inout should mutate existing struct-key value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructKeyMapInoutMutated"), true,
			TEXT("TMap<FStruct,int> &inout call should update script-side state"))));

		FFunctionInvoker MapReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructKeyMap"));
		ASSERT_THAT(IsTrue(MapReturnInvoker.IsValid(), TEXT("ReturnStructKeyMap should be invokable")));
		if (!MapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapReturnInvoker.Call(), TEXT("ReturnStructKeyMap should execute through reflection")));
		UFunction* ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructKeyMap"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructKeyMap should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,int> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		void* ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(MapReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,int> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper MapReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, MapReturnHelper.Num(), TEXT("TMap<FStruct,int> return should contain two entries")));
		FoundIntValue = 0;
		ASSERT_THAT(IsTrue(GetIntMapValueByStructKey(*TestRunner, *MapReturnProperty, ReturnSlot, *IDProperty, *TagProperty, 31, FName(TEXT("MapReturnB")), FoundIntValue)));
		ASSERT_THAT(AreEqual(331, FoundIntValue, TEXT("TMap<FStruct,int> return should preserve hashable struct keys")));

		FFunctionInvoker StructValueMapReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructValueMap"));
		ASSERT_THAT(IsTrue(StructValueMapReturnInvoker.IsValid(), TEXT("ReturnStructValueMap should be invokable")));
		if (!StructValueMapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructValueMapReturnInvoker.Call(), TEXT("ReturnStructValueMap should execute through reflection")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructValueMapReturnContains"), true,
			TEXT("TMap<int,FStruct> return function should preserve script-side Find behavior"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructValueMapReturnScore"), 441,
			TEXT("TMap<int,FStruct> return function should preserve struct value fields"))));

		FFunctionInvoker SetValueInvoker(*TestRunner, Actor, TEXT("CountStructSetValue"));
		ASSERT_THAT(IsTrue(SetValueInvoker.IsValid(), TEXT("CountStructSetValue should be invokable")));
		if (!SetValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructSetValue should expose TSet<FStruct> parameter slot")));
		FSetProperty* SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> value parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 50, FName(TEXT("SetValueA")))));
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 51, FName(TEXT("SetValueB")))));
		ASSERT_THAT(AreEqual(2, SetValueInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TSet<FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructSetValueContains"), true,
			TEXT("TSet<FStruct> by-value call should preserve hashable struct elements"))));

		FFunctionInvoker SetInInvoker(*TestRunner, Actor, TEXT("CountStructSetIn"));
		ASSERT_THAT(IsTrue(SetInInvoker.IsValid(), TEXT("CountStructSetIn should be invokable")));
		if (!SetInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructSetIn should expose TSet<FStruct> const ref parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> &in parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 52, FName(TEXT("SetInA")))));
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 53, FName(TEXT("SetInB")))));
		ASSERT_THAT(AreEqual(2, SetInInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TSet<FStruct> &in parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructSetInContains"), true,
			TEXT("TSet<FStruct> &in call should preserve hashable struct elements"))));

		FFunctionInvoker SetOutInvoker(*TestRunner, Actor, TEXT("FillStructSetOut"));
		ASSERT_THAT(IsTrue(SetOutInvoker.IsValid(), TEXT("FillStructSetOut should be invokable")));
		if (!SetOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructSetOut should expose TSet<FStruct> out parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> &out parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetOutInvoker.Call(), TEXT("FillStructSetOut should execute through reflection")));
		FScriptSetHelper SetOutHelper(SetProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, SetOutHelper.Num(), TEXT("TSet<FStruct> &out should write two entries")));
		ASSERT_THAT(IsTrue(SetContainsStructItem(*SetProperty, ParamSlot, *IDProperty, *TagProperty, 55, FName(TEXT("SetOutB"))),
			TEXT("TSet<FStruct> &out should preserve hashable struct elements")));
		int32 SetOutCount = 0;
		ASSERT_THAT(IsTrue(GetSetNumByPath(*TestRunner, Actor, TEXT("StructSetOut"), SetOutCount),
			TEXT("TSet<FStruct> &out should be assignable to script-side storage")));
		ASSERT_THAT(AreEqual(2, SetOutCount, TEXT("TSet<FStruct> &out storage should contain two entries")));

		FFunctionInvoker SetInoutInvoker(*TestRunner, Actor, TEXT("MutateStructSetInout"));
		ASSERT_THAT(IsTrue(SetInoutInvoker.IsValid(), TEXT("MutateStructSetInout should be invokable")));
		if (!SetInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructSetInout should expose TSet<FStruct> inout parameter slot")));
		SetProperty = CastField<FSetProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(SetProperty, TEXT("TSet<FStruct> &inout parameter should reflect as FSetProperty")));
		if (ParamSlot == nullptr || SetProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructItemToSet(*TestRunner, *SetProperty, ParamSlot, *IDProperty, *TagProperty, 60, FName(TEXT("SetInoutA")))));
		ASSERT_THAT(IsTrue(SetInoutInvoker.Call(), TEXT("MutateStructSetInout should execute through reflection")));
		FScriptSetHelper SetInoutHelper(SetProperty, ParamSlot);
		ASSERT_THAT(AreEqual(1, SetInoutHelper.Num(), TEXT("TSet<FStruct> &inout should remove one and add one entry")));
		ASSERT_THAT(IsTrue(SetContainsStructItem(*SetProperty, ParamSlot, *IDProperty, *TagProperty, 61, FName(TEXT("SetInoutB"))),
			TEXT("TSet<FStruct> &inout should preserve added struct element")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructSetInoutMutated"), true,
			TEXT("TSet<FStruct> &inout call should update script-side state"))));

		FFunctionInvoker SetReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructSet"));
		ASSERT_THAT(IsTrue(SetReturnInvoker.IsValid(), TEXT("ReturnStructSet should be invokable")));
		if (!SetReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetReturnInvoker.Call(), TEXT("ReturnStructSet should execute through reflection")));
		UFunction* ReturnSetFunction = Actor->FindFunction(TEXT("ReturnStructSet"));
		ASSERT_THAT(IsNotNull(ReturnSetFunction, TEXT("ReturnStructSet should reflect as a UFunction")));
		if (ReturnSetFunction == nullptr)
		{
			return;
		}
		FSetProperty* SetReturnProperty = CastField<FSetProperty>(ReturnSetFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(SetReturnProperty, TEXT("TSet<FStruct> return should reflect as FSetProperty")));
		if (SetReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = SetReturnProperty->ContainerPtrToValuePtr<void>(SetReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TSet<FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptSetHelper SetReturnHelper(SetReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, SetReturnHelper.Num(), TEXT("TSet<FStruct> return should contain two entries")));
		ASSERT_THAT(IsTrue(SetContainsStructItem(*SetReturnProperty, ReturnSlot, *IDProperty, *TagProperty, 71, FName(TEXT("SetReturnB"))),
			TEXT("TSet<FStruct> return should preserve hashable struct elements")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT struct-to-struct maps: reflected TMap<FStruct,FStruct> UFUNCTION paths.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructStructToStructMapParameterAndReturnMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_StructToStructMapParameterAndReturnMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructStructToStructMapParameterAndReturnMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructToStructMapKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FStructToStructMapKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 941) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FStructToStructMapValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructToStructMapMatrixActor : AActor
			{
				UPROPERTY()
				int StructStructMapValueCount = 0;

				UPROPERTY()
				int StructStructMapInCount = 0;

				UPROPERTY()
				TMap<FStructToStructMapKey, FStructToStructMapValue> StructStructMapInout;

				UPROPERTY()
				bool StructStructMapValuePreserved = false;

				UPROPERTY()
				bool StructStructMapInPreserved = false;

				UPROPERTY()
				bool StructStructMapInoutSawOriginal = false;

				UPROPERTY()
				bool StructStructMapInoutMutated = false;

				UPROPERTY()
				bool StructStructMapReturnPreserved = false;

				FStructToStructMapKey MakeKey(int ID, FName Tag)
				{
					FStructToStructMapKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FStructToStructMapValue MakeValue(int Score, FString Label)
				{
					FStructToStructMapValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructStructMapValue(TMap<FStructToStructMapKey, FStructToStructMapValue> Items)
				{
					StructStructMapValueCount = Items.Num();
					FStructToStructMapValue Found;
					StructStructMapValuePreserved =
						Items.Find(MakeKey(11, n"MapValueB"), Found)
						&& Found.Score == 111
						&& Found.Label == "ValueB";
					return StructStructMapValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructStructMapIn(const TMap<FStructToStructMapKey, FStructToStructMapValue>&in Items)
				{
					StructStructMapInCount = Items.Num();
					FStructToStructMapValue Found;
					StructStructMapInPreserved =
						Items.Find(MakeKey(13, n"MapInB"), Found)
						&& Found.Score == 113
						&& Found.Label == "InB";
					return StructStructMapInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructStructMapOut(TMap<FStructToStructMapKey, FStructToStructMapValue>&out Items)
				{
					Items.Add(MakeKey(20, n"MapOutA"), MakeValue(220, "OutA"));
					Items.Add(MakeKey(21, n"MapOutB"), MakeValue(221, "OutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructStructMapInout(TMap<FStructToStructMapKey, FStructToStructMapValue>&inout Items)
				{
					FStructToStructMapKey Existing = MakeKey(30, n"MapInoutA");
					FStructToStructMapValue Found;
					StructStructMapInoutSawOriginal =
						Items.Find(Existing, Found)
						&& Found.Score == 330
						&& Found.Label == "InoutA";

					Items.Remove(Existing);
					Items.Add(Existing, MakeValue(430, "InoutMutated"));
					Items.Add(MakeKey(31, n"MapInoutB"), MakeValue(431, "InoutAdded"));
					StructStructMapInout = Items;

					FStructToStructMapValue Mutated;
					StructStructMapInoutMutated =
						Items.Find(Existing, Mutated)
						&& Mutated.Score == 430
						&& Mutated.Label == "InoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<FStructToStructMapKey, FStructToStructMapValue> ReturnStructStructMap()
				{
					TMap<FStructToStructMapKey, FStructToStructMapValue> Items;
					Items.Add(MakeKey(40, n"MapReturnA"), MakeValue(440, "ReturnA"));
					Items.Add(MakeKey(41, n"MapReturnB"), MakeValue(441, "ReturnB"));

					FStructToStructMapValue Found;
					StructStructMapReturnPreserved =
						Items.Find(MakeKey(41, n"MapReturnB"), Found)
						&& Found.Score == 441
						&& Found.Label == "ReturnB";
					return Items;
				}
			}
			)AS"),
			TEXT("ACoverageStructToStructMapMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct struct-to-struct map matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct struct-to-struct map matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FMapProperty* StructStructMapInoutProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructStructMapInout"));
		ASSERT_THAT(IsNotNull(StructStructMapInoutProperty, TEXT("TMap<FStruct,FStruct> inout storage should reflect")));
		if (StructStructMapInoutProperty == nullptr)
		{
			return;
		}

		FStructProperty* StructMapKeyProperty = CastField<FStructProperty>(StructStructMapInoutProperty->KeyProp);
		FStructProperty* StructMapValueProperty = CastField<FStructProperty>(StructStructMapInoutProperty->ValueProp);
		ASSERT_THAT(IsNotNull(StructMapKeyProperty, TEXT("TMap<FStruct,FStruct> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(StructMapValueProperty, TEXT("TMap<FStruct,FStruct> value should expose the AS value struct")));
		if (StructMapKeyProperty == nullptr || StructMapKeyProperty->Struct == nullptr
			|| StructMapValueProperty == nullptr || StructMapValueProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructMapKeyProperty->Struct != StructMapValueProperty->Struct,
			TEXT("TMap<FStruct,FStruct> should support distinct generated key and value UScriptStructs")));

		FIntProperty* KeyIDProperty = FindFProperty<FIntProperty>(StructMapKeyProperty->Struct, TEXT("ID"));
		FNameProperty* KeyTagProperty = FindFProperty<FNameProperty>(StructMapKeyProperty->Struct, TEXT("Tag"));
		FIntProperty* ValueScoreProperty = FindFProperty<FIntProperty>(StructMapValueProperty->Struct, TEXT("Score"));
		FStrProperty* ValueLabelProperty = FindFProperty<FStrProperty>(StructMapValueProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(KeyIDProperty, TEXT("Struct map key should expose ID")));
		ASSERT_THAT(IsNotNull(KeyTagProperty, TEXT("Struct map key should expose Tag")));
		ASSERT_THAT(IsNotNull(ValueScoreProperty, TEXT("Struct map value should expose Score")));
		ASSERT_THAT(IsNotNull(ValueLabelProperty, TEXT("Struct map value should expose Label")));
		if (KeyIDProperty == nullptr || KeyTagProperty == nullptr
			|| ValueScoreProperty == nullptr || ValueLabelProperty == nullptr)
		{
			return;
		}

		FFunctionInvoker MapValueInvoker(*TestRunner, Actor, TEXT("CountStructStructMapValue"));
		ASSERT_THAT(IsTrue(MapValueInvoker.IsValid(), TEXT("CountStructStructMapValue should be invokable")));
		if (!MapValueInvoker.IsValid())
		{
			return;
		}
		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(MapValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructStructMapValue should expose TMap<FStruct,FStruct> parameter slot")));
		FMapProperty* MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 10, FName(TEXT("MapValueA")),
			*ValueScoreProperty, *ValueLabelProperty, 110, FString(TEXT("ValueA")))));
		ASSERT_THAT(IsTrue(AddStructKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 11, FName(TEXT("MapValueB")),
			*ValueScoreProperty, *ValueLabelProperty, 111, FString(TEXT("ValueB")))));
		ASSERT_THAT(AreEqual(2, MapValueInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TMap<FStruct,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStructMapValueCount"), 2,
			TEXT("TMap<FStruct,FStruct> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructMapValuePreserved"), true,
			TEXT("TMap<FStruct,FStruct> by-value call should preserve key lookup and value fields"))));

		FFunctionInvoker MapInInvoker(*TestRunner, Actor, TEXT("CountStructStructMapIn"));
		ASSERT_THAT(IsTrue(MapInInvoker.IsValid(), TEXT("CountStructStructMapIn should be invokable")));
		if (!MapInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructStructMapIn should expose TMap<FStruct,FStruct> const ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 12, FName(TEXT("MapInA")),
			*ValueScoreProperty, *ValueLabelProperty, 112, FString(TEXT("InA")))));
		ASSERT_THAT(IsTrue(AddStructKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 13, FName(TEXT("MapInB")),
			*ValueScoreProperty, *ValueLabelProperty, 113, FString(TEXT("InB")))));
		ASSERT_THAT(AreEqual(2, MapInInvoker.CallAndReturn<int32>(0),
			TEXT("Reflected TMap<FStruct,FStruct> &in parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructMapInPreserved"), true,
			TEXT("TMap<FStruct,FStruct> &in call should preserve key lookup and value fields"))));

		FFunctionInvoker MapOutInvoker(*TestRunner, Actor, TEXT("FillStructStructMapOut"));
		ASSERT_THAT(IsTrue(MapOutInvoker.IsValid(), TEXT("FillStructStructMapOut should be invokable")));
		if (!MapOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructStructMapOut should expose TMap<FStruct,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapOutInvoker.Call(), TEXT("FillStructStructMapOut should execute through reflection")));
		FScriptMapHelper MapOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapOutHelper.Num(), TEXT("TMap<FStruct,FStruct> &out should write two entries")));
		const FStructProperty* MapValueStructProperty = nullptr;
		const void* MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetStructMapValueByStructKey(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 21, FName(TEXT("MapOutB")),
			MapValueStructProperty, MapValueAddress)));
		if (MapValueStructProperty == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(StructMapValueProperty->Struct, MapValueStructProperty->Struct,
			TEXT("TMap<FStruct,FStruct> &out value should use the generated value UScriptStruct")));
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, MapValueAddress,
			221, FString(TEXT("OutB")), TEXT("TMap<FStruct,FStruct> &out value"))));

		FFunctionInvoker MapInoutInvoker(*TestRunner, Actor, TEXT("MutateStructStructMapInout"));
		ASSERT_THAT(IsTrue(MapInoutInvoker.IsValid(), TEXT("MutateStructStructMapInout should be invokable")));
		if (!MapInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructStructMapInout should expose TMap<FStruct,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 30, FName(TEXT("MapInoutA")),
			*ValueScoreProperty, *ValueLabelProperty, 330, FString(TEXT("InoutA")))));
		ASSERT_THAT(IsTrue(MapInoutInvoker.Call(), TEXT("MutateStructStructMapInout should execute through reflection")));
		FScriptMapHelper MapInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, MapInoutHelper.Num(), TEXT("TMap<FStruct,FStruct> &inout should add one entry")));
		MapValueStructProperty = nullptr;
		MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetStructMapValueByStructKey(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 30, FName(TEXT("MapInoutA")),
			MapValueStructProperty, MapValueAddress)));
		if (MapValueStructProperty == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, MapValueAddress,
			430, FString(TEXT("InoutMutated")), TEXT("TMap<FStruct,FStruct> &inout mutated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructMapInoutSawOriginal"), true,
			TEXT("TMap<FStruct,FStruct> &inout call should read caller-provided entries before mutation"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructMapInoutMutated"), true,
			TEXT("TMap<FStruct,FStruct> &inout call should update script-side state"))));

		FFunctionInvoker MapReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructStructMap"));
		ASSERT_THAT(IsTrue(MapReturnInvoker.IsValid(), TEXT("ReturnStructStructMap should be invokable")));
		if (!MapReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(MapReturnInvoker.Call(), TEXT("ReturnStructStructMap should execute through reflection")));
		UFunction* ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructStructMap"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructStructMap should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		void* ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(MapReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper MapReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, MapReturnHelper.Num(), TEXT("TMap<FStruct,FStruct> return should contain two entries")));
		MapValueStructProperty = nullptr;
		MapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetStructMapValueByStructKey(*TestRunner, *MapReturnProperty, ReturnSlot,
			*KeyIDProperty, *KeyTagProperty, 41, FName(TEXT("MapReturnB")),
			MapValueStructProperty, MapValueAddress)));
		if (MapValueStructProperty == nullptr || MapValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, MapValueAddress,
			441, FString(TEXT("ReturnB")), TEXT("TMap<FStruct,FStruct> return value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructMapReturnPreserved"), true,
			TEXT("TMap<FStruct,FStruct> return function should preserve script-side Find behavior"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT map key/value shape matrix: struct and name/string/object value combinations.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMapKeyValueShapeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_MapKeyValueShapeMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMapKeyValueShapeMatrix.as"),
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageStructMapValueObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			USTRUCT(BlueprintType)
			struct FStructMapKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FStructMapKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 101) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FStructMapValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructMapShapeActor : AActor
			{
				UPROPERTY()
				TMap<FName, FStructMapValue> NameToStruct;

				UPROPERTY()
				TMap<FStructMapKey, FStructMapValue> StructToStruct;

				UPROPERTY()
				TMap<FStructMapKey, FString> StructToString;

				UPROPERTY()
				TMap<FStructMapKey, FName> StructToName;

				UPROPERTY()
				TMap<FStructMapKey, UCoverageStructMapValueObject> StructToObject;

				UPROPERTY()
				bool NameFindWorked = false;

				UPROPERTY()
				int NameFoundScore = 0;

				UPROPERTY()
				bool StructStructContains = false;

				UPROPERTY()
				bool StructStructFindWorked = false;

				UPROPERTY()
				int StructStructFoundScore = 0;

				UPROPERTY()
				bool StructStructOverwriteWorked = false;

				UPROPERTY()
				bool StructStringFindWorked = false;

				UPROPERTY()
				FString StructStringFound;

				UPROPERTY()
				bool StructNameFindWorked = false;

				UPROPERTY()
				FName StructNameFound;

				UPROPERTY()
				bool StructObjectFindWorked = false;

				UPROPERTY()
				int StructObjectFoundValue = 0;

				UPROPERTY()
				bool StructRemoveWorked = false;

				FStructMapKey MakeKey(int ID, FName Tag)
				{
					FStructMapKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FStructMapValue MakeValue(int Score, FString Label)
				{
					FStructMapValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					FStructMapKey Alpha = MakeKey(1, n"Alpha");
					FStructMapKey AlphaDuplicate = MakeKey(1, n"Alpha");
					FStructMapKey Beta = MakeKey(2, n"Beta");

					NameToStruct.Add(n"Primary", MakeValue(11, "NamePrimary"));
					FStructMapValue NameValue;
					NameFindWorked = NameToStruct.Find(n"Primary", NameValue);
					NameFoundScore = NameValue.Score;

					StructToStruct.Add(Alpha, MakeValue(21, "AlphaValue"));
					StructToStruct.Add(Beta, MakeValue(22, "BetaValue"));
					StructStructContains = StructToStruct.Contains(AlphaDuplicate);

					FStructMapValue StructValue;
					StructStructFindWorked = StructToStruct.Find(AlphaDuplicate, StructValue);
					StructStructFoundScore = StructValue.Score;
					StructToStruct.Add(AlphaDuplicate, MakeValue(31, "AlphaOverwrite"));
					StructStructOverwriteWorked = StructToStruct[Alpha].Score == 31;

					StructToString.Add(Alpha, "StringValue");
					StructStringFindWorked = StructToString.Find(AlphaDuplicate, StructStringFound);

					StructToName.Add(Alpha, n"NameValue");
					StructNameFindWorked = StructToName.Find(AlphaDuplicate, StructNameFound);

					UCoverageStructMapValueObject Obj = Cast<UCoverageStructMapValueObject>(NewObject(this, UCoverageStructMapValueObject::StaticClass()));
					Obj.Value = 77;
					StructToObject.Add(Alpha, Obj);
					UCoverageStructMapValueObject FoundObj = nullptr;
					StructObjectFindWorked = StructToObject.Find(AlphaDuplicate, FoundObj);
					StructObjectFoundValue = FoundObj != nullptr ? FoundObj.Value : -1;

					StructRemoveWorked = StructToStruct.Remove(Beta) && !StructToStruct.Contains(Beta);
				}
			}
			)AS"),
			TEXT("ACoverageStructMapShapeActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct map key/value shape actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FMapProperty* NameToStructProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("NameToStruct"));
		FMapProperty* StructToStructProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructToStruct"));
		FMapProperty* StructToStringProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructToString"));
		FMapProperty* StructToNameProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructToName"));
		FMapProperty* StructToObjectProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructToObject"));
		ASSERT_THAT(IsNotNull(NameToStructProperty, TEXT("TMap<FName,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(StructToStructProperty, TEXT("TMap<FStruct,FStruct> should reflect")));
		ASSERT_THAT(IsNotNull(StructToStringProperty, TEXT("TMap<FStruct,FString> should reflect")));
		ASSERT_THAT(IsNotNull(StructToNameProperty, TEXT("TMap<FStruct,FName> should reflect")));
		ASSERT_THAT(IsNotNull(StructToObjectProperty, TEXT("TMap<FStruct,UObject> should reflect")));
		if (NameToStructProperty == nullptr || StructToStructProperty == nullptr || StructToStringProperty == nullptr
			|| StructToNameProperty == nullptr || StructToObjectProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameToStructProperty->KeyProp),
			TEXT("TMap<FName,FStruct> key should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(NameToStructProperty->ValueProp),
			TEXT("TMap<FName,FStruct> value should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructToStructProperty->KeyProp),
			TEXT("TMap<FStruct,FStruct> key should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructToStructProperty->ValueProp),
			TEXT("TMap<FStruct,FStruct> value should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructToStringProperty->KeyProp),
			TEXT("TMap<FStruct,FString> key should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StructToStringProperty->ValueProp),
			TEXT("TMap<FStruct,FString> value should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructToNameProperty->KeyProp),
			TEXT("TMap<FStruct,FName> key should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(StructToNameProperty->ValueProp),
			TEXT("TMap<FStruct,FName> value should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(CastField<FStructProperty>(StructToObjectProperty->KeyProp),
			TEXT("TMap<FStruct,UObject> key should reflect as FStructProperty")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(StructToObjectProperty->ValueProp),
			TEXT("TMap<FStruct,UObject> value should reflect as FObjectProperty")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct map key/value shape actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameFindWorked"), true,
			TEXT("TMap<FName,FStruct>.Find should copy struct values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NameFoundScore"), 11,
			TEXT("TMap<FName,FStruct>.Find should preserve int fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructContains"), true,
			TEXT("TMap<FStruct,FStruct>.Contains should use hashable struct key equality"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructFindWorked"), true,
			TEXT("TMap<FStruct,FStruct>.Find should work with equivalent struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStructFoundScore"), 21,
			TEXT("TMap<FStruct,FStruct>.Find should preserve struct value fields"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStructOverwriteWorked"), true,
			TEXT("TMap<FStruct,FStruct>.Add should overwrite equivalent struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringFindWorked"), true,
			TEXT("TMap<FStruct,FString>.Find should work with struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("StructStringFound"), FString(TEXT("StringValue")),
			TEXT("TMap<FStruct,FString>.Find should copy string values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameFindWorked"), true,
			TEXT("TMap<FStruct,FName>.Find should work with struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("StructNameFound"), FName(TEXT("NameValue")),
			TEXT("TMap<FStruct,FName>.Find should copy name values"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectFindWorked"), true,
			TEXT("TMap<FStruct,UObject>.Find should work with struct keys"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructObjectFoundValue"), 77,
			TEXT("TMap<FStruct,UObject>.Find should copy object references"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructRemoveWorked"), true,
			TEXT("TMap<FStruct,FStruct>.Remove should remove equivalent struct keys"))));

		int32 Count = 0;
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameToStruct"), Count),
			TEXT("TMap<FName,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FName,FStruct> should contain one entry")));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructToStruct"), Count),
			TEXT("TMap<FStruct,FStruct> count should be readable")));
		ASSERT_THAT(AreEqual(1, Count, TEXT("TMap<FStruct,FStruct> should contain one entry after remove")));

		const FStructProperty* NameMapStructValueProperty = nullptr;
		const void* NameMapValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetMapStructValueByPath<FNameProperty, FName>(
			*TestRunner,
			Actor,
			TEXT("NameToStruct"),
			FName(TEXT("Primary")),
			NameMapStructValueProperty,
			NameMapValueAddress),
			TEXT("TMap<FName,FStruct> value should be readable by key")));
		if (NameMapStructValueProperty == nullptr || NameMapStructValueProperty->Struct == nullptr || NameMapValueAddress == nullptr)
		{
			return;
		}

		FIntProperty* ScoreProperty = FindFProperty<FIntProperty>(NameMapStructValueProperty->Struct, TEXT("Score"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(NameMapStructValueProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(ScoreProperty, TEXT("TMap<FName,FStruct> value should expose Score")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("TMap<FName,FStruct> value should expose Label")));
		if (ScoreProperty == nullptr || LabelProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(11, ScoreProperty->GetPropertyValue_InContainer(NameMapValueAddress),
			TEXT("TMap<FName,FStruct> reflected value should preserve int fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("NamePrimary")), LabelProperty->GetPropertyValue_InContainer(NameMapValueAddress),
			TEXT("TMap<FName,FStruct> reflected value should preserve string fields")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT map key/value parameter and return matrix: simple-key and non-struct value combinations.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMapKeyValueParameterAndReturnMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_MapKeyValueParameterAndReturnMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageStructMapParamValueObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			USTRUCT(BlueprintType)
			struct FMapParamKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FMapParamKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 977) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FMapParamValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructMapParamMatrixActor : AActor
			{
				UPROPERTY()
				int NameStructValueCount = 0;

				UPROPERTY()
				int NameStructInCount = 0;

				UPROPERTY()
				TMap<FName, FMapParamValue> NameStructInout;

				UPROPERTY()
				bool NameStructValuePreserved = false;

				UPROPERTY()
				bool NameStructInPreserved = false;

				UPROPERTY()
				bool NameStructInoutSawOriginal = false;

				UPROPERTY()
				bool NameStructInoutMutated = false;

				UPROPERTY()
				bool NameStructReturnPreserved = false;

				UPROPERTY()
				int StringStructValueCount = 0;

				UPROPERTY()
				int StringStructInCount = 0;

				UPROPERTY()
				TMap<FString, FMapParamValue> StringStructInout;

				UPROPERTY()
				bool StringStructValuePreserved = false;

				UPROPERTY()
				bool StringStructInPreserved = false;

				UPROPERTY()
				bool StringStructInoutSawOriginal = false;

				UPROPERTY()
				bool StringStructInoutMutated = false;

				UPROPERTY()
				bool StringStructReturnPreserved = false;

				UPROPERTY()
				int StructStringValueCount = 0;

				UPROPERTY()
				int StructStringInCount = 0;

				UPROPERTY()
				TMap<FMapParamKey, FString> StructStringInout;

				UPROPERTY()
				bool StructStringValuePreserved = false;

				UPROPERTY()
				bool StructStringInPreserved = false;

				UPROPERTY()
				bool StructStringInoutSawOriginal = false;

				UPROPERTY()
				bool StructStringInoutMutated = false;

				UPROPERTY()
				bool StructStringReturnPreserved = false;

				UPROPERTY()
				int StructNameValueCount = 0;

				UPROPERTY()
				int StructNameInCount = 0;

				UPROPERTY()
				TMap<FMapParamKey, FName> StructNameInout;

				UPROPERTY()
				bool StructNameValuePreserved = false;

				UPROPERTY()
				bool StructNameInPreserved = false;

				UPROPERTY()
				bool StructNameInoutSawOriginal = false;

				UPROPERTY()
				bool StructNameInoutMutated = false;

				UPROPERTY()
				bool StructNameReturnPreserved = false;

				UPROPERTY()
				int StructObjectValueCount = 0;

				UPROPERTY()
				int StructObjectInCount = 0;

				UPROPERTY()
				TMap<FMapParamKey, UCoverageStructMapParamValueObject> StructObjectInout;

				UPROPERTY()
				bool StructObjectValuePreserved = false;

				UPROPERTY()
				bool StructObjectInPreserved = false;

				UPROPERTY()
				bool StructObjectInoutSawOriginal = false;

				UPROPERTY()
				bool StructObjectInoutMutated = false;

				UPROPERTY()
				bool StructObjectReturnPreserved = false;
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				FMapParamKey MakeKey(int ID, FName Tag)
				{
					FMapParamKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FMapParamValue MakeValue(int Score, FString Label)
				{
					FMapParamValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UCoverageStructMapParamValueObject MakeObject(int Value)
				{
					UCoverageStructMapParamValueObject Object = Cast<UCoverageStructMapParamValueObject>(NewObject(this, UCoverageStructMapParamValueObject::StaticClass()));
					Object.Value = Value;
					return Object;
				}

				UFUNCTION(BlueprintCallable)
				int CountNameStructValue(TMap<FName, FMapParamValue> Items)
				{
					NameStructValueCount = Items.Num();
					FMapParamValue Found;
					NameStructValuePreserved =
						Items.Find(n"ValueB", Found)
						&& Found.Score == 102
						&& Found.Label == "NameValueB";
					return NameStructValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountNameStructIn(const TMap<FName, FMapParamValue>&in Items)
				{
					NameStructInCount = Items.Num();
					FMapParamValue Found;
					NameStructInPreserved =
						Items.Find(n"InB", Found)
						&& Found.Score == 112
						&& Found.Label == "NameInB";
					return NameStructInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillNameStructOut(TMap<FName, FMapParamValue>&out Items)
				{
					Items.Add(n"OutA", MakeValue(121, "NameOutA"));
					Items.Add(n"OutB", MakeValue(122, "NameOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateNameStructInout(TMap<FName, FMapParamValue>&inout Items)
				{
					FMapParamValue Found;
					NameStructInoutSawOriginal =
						Items.Find(n"InoutA", Found)
						&& Found.Score == 131
						&& Found.Label == "NameInoutA";
					Items.Add(n"InoutA", MakeValue(231, "NameInoutMutated"));
					Items.Add(n"InoutB", MakeValue(232, "NameInoutAdded"));
					NameStructInout = Items;

					FMapParamValue Mutated;
					NameStructInoutMutated =
						Items.Find(n"InoutA", Mutated)
						&& Mutated.Score == 231
						&& Mutated.Label == "NameInoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<FName, FMapParamValue> ReturnNameStruct()
				{
					TMap<FName, FMapParamValue> Items;
					Items.Add(n"ReturnA", MakeValue(141, "NameReturnA"));
					Items.Add(n"ReturnB", MakeValue(142, "NameReturnB"));

					FMapParamValue Found;
					NameStructReturnPreserved =
						Items.Find(n"ReturnB", Found)
						&& Found.Score == 142
						&& Found.Label == "NameReturnB";
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountStringStructValue(TMap<FString, FMapParamValue> Items)
				{
					StringStructValueCount = Items.Num();
					FMapParamValue Found;
					StringStructValuePreserved =
						Items.Find("ValueB", Found)
						&& Found.Score == 202
						&& Found.Label == "StringValueB";
					return StringStructValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStringStructIn(const TMap<FString, FMapParamValue>&in Items)
				{
					StringStructInCount = Items.Num();
					FMapParamValue Found;
					StringStructInPreserved =
						Items.Find("InB", Found)
						&& Found.Score == 212
						&& Found.Label == "StringInB";
					return StringStructInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStringStructOut(TMap<FString, FMapParamValue>&out Items)
				{
					Items.Add("OutA", MakeValue(221, "StringOutA"));
					Items.Add("OutB", MakeValue(222, "StringOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateStringStructInout(TMap<FString, FMapParamValue>&inout Items)
				{
					FMapParamValue Found;
					StringStructInoutSawOriginal =
						Items.Find("InoutA", Found)
						&& Found.Score == 231
						&& Found.Label == "StringInoutA";
					Items.Add("InoutA", MakeValue(331, "StringInoutMutated"));
					Items.Add("InoutB", MakeValue(332, "StringInoutAdded"));
					StringStructInout = Items;

					FMapParamValue Mutated;
					StringStructInoutMutated =
						Items.Find("InoutA", Mutated)
						&& Mutated.Score == 331
						&& Mutated.Label == "StringInoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<FString, FMapParamValue> ReturnStringStruct()
				{
					TMap<FString, FMapParamValue> Items;
					Items.Add("ReturnA", MakeValue(241, "StringReturnA"));
					Items.Add("ReturnB", MakeValue(242, "StringReturnB"));

					FMapParamValue Found;
					StringStructReturnPreserved =
						Items.Find("ReturnB", Found)
						&& Found.Score == 242
						&& Found.Label == "StringReturnB";
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructStringValue(TMap<FMapParamKey, FString> Items)
				{
					StructStringValueCount = Items.Num();
					FString Found;
					StructStringValuePreserved =
						Items.Find(MakeKey(301, n"ValueB"), Found)
						&& Found == "StructStringValueB";
					return StructStringValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructStringIn(const TMap<FMapParamKey, FString>&in Items)
				{
					StructStringInCount = Items.Num();
					FString Found;
					StructStringInPreserved =
						Items.Find(MakeKey(311, n"InB"), Found)
						&& Found == "StructStringInB";
					return StructStringInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructStringOut(TMap<FMapParamKey, FString>&out Items)
				{
					Items.Add(MakeKey(320, n"OutA"), "StructStringOutA");
					Items.Add(MakeKey(321, n"OutB"), "StructStringOutB");
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructStringInout(TMap<FMapParamKey, FString>&inout Items)
				{
					FMapParamKey Existing = MakeKey(330, n"InoutA");
					FString Found;
					StructStringInoutSawOriginal =
						Items.Find(Existing, Found)
						&& Found == "StructStringInoutA";
					Items.Add(Existing, "StructStringInoutMutated");
					Items.Add(MakeKey(331, n"InoutB"), "StructStringInoutAdded");
					StructStringInout = Items;

					FString Mutated;
					StructStringInoutMutated =
						Items.Find(Existing, Mutated)
						&& Mutated == "StructStringInoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<FMapParamKey, FString> ReturnStructString()
				{
					TMap<FMapParamKey, FString> Items;
					Items.Add(MakeKey(340, n"ReturnA"), "StructStringReturnA");
					Items.Add(MakeKey(341, n"ReturnB"), "StructStringReturnB");

					FString Found;
					StructStringReturnPreserved =
						Items.Find(MakeKey(341, n"ReturnB"), Found)
						&& Found == "StructStringReturnB";
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountStructNameValue(TMap<FMapParamKey, FName> Items)
				{
					StructNameValueCount = Items.Num();
					FName Found;
					StructNameValuePreserved =
						Items.Find(MakeKey(401, n"ValueB"), Found)
						&& Found == n"StructNameValueB";
					return StructNameValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructNameIn(const TMap<FMapParamKey, FName>&in Items)
				{
					StructNameInCount = Items.Num();
					FName Found;
					StructNameInPreserved =
						Items.Find(MakeKey(411, n"InB"), Found)
						&& Found == n"StructNameInB";
					return StructNameInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructNameOut(TMap<FMapParamKey, FName>&out Items)
				{
					Items.Add(MakeKey(420, n"OutA"), n"StructNameOutA");
					Items.Add(MakeKey(421, n"OutB"), n"StructNameOutB");
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructNameInout(TMap<FMapParamKey, FName>&inout Items)
				{
					FMapParamKey Existing = MakeKey(430, n"InoutA");
					FName Found;
					StructNameInoutSawOriginal =
						Items.Find(Existing, Found)
						&& Found == n"StructNameInoutA";
					Items.Add(Existing, n"StructNameInoutMutated");
					Items.Add(MakeKey(431, n"InoutB"), n"StructNameInoutAdded");
					StructNameInout = Items;

					FName Mutated;
					StructNameInoutMutated =
						Items.Find(Existing, Mutated)
						&& Mutated == n"StructNameInoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<FMapParamKey, FName> ReturnStructName()
				{
					TMap<FMapParamKey, FName> Items;
					Items.Add(MakeKey(440, n"ReturnA"), n"StructNameReturnA");
					Items.Add(MakeKey(441, n"ReturnB"), n"StructNameReturnB");

					FName Found;
					StructNameReturnPreserved =
						Items.Find(MakeKey(441, n"ReturnB"), Found)
						&& Found == n"StructNameReturnB";
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountStructObjectValue(TMap<FMapParamKey, UCoverageStructMapParamValueObject> Items)
				{
					StructObjectValueCount = Items.Num();
					UCoverageStructMapParamValueObject Found = nullptr;
					StructObjectValuePreserved =
						Items.Find(MakeKey(501, n"ValueB"), Found)
						&& Found != nullptr
						&& Found.Value == 502;
					return StructObjectValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructObjectIn(const TMap<FMapParamKey, UCoverageStructMapParamValueObject>&in Items)
				{
					StructObjectInCount = Items.Num();
					UCoverageStructMapParamValueObject Found = nullptr;
					StructObjectInPreserved =
						Items.Find(MakeKey(511, n"InB"), Found)
						&& Found != nullptr
						&& Found.Value == 512;
					return StructObjectInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructObjectOut(TMap<FMapParamKey, UCoverageStructMapParamValueObject>&out Items)
				{
					Items.Add(MakeKey(520, n"OutA"), MakeObject(521));
					Items.Add(MakeKey(521, n"OutB"), MakeObject(522));
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructObjectInout(TMap<FMapParamKey, UCoverageStructMapParamValueObject>&inout Items)
				{
					FMapParamKey Existing = MakeKey(530, n"InoutA");
					UCoverageStructMapParamValueObject Found = nullptr;
					StructObjectInoutSawOriginal =
						Items.Find(Existing, Found)
						&& Found != nullptr
						&& Found.Value == 531;
					Items.Add(Existing, MakeObject(631));
					Items.Add(MakeKey(531, n"InoutB"), MakeObject(632));
					StructObjectInout = Items;

					UCoverageStructMapParamValueObject Mutated = nullptr;
					StructObjectInoutMutated =
						Items.Find(Existing, Mutated)
						&& Mutated != nullptr
						&& Mutated.Value == 631;
				}

				UFUNCTION(BlueprintCallable)
				TMap<FMapParamKey, UCoverageStructMapParamValueObject> ReturnStructObject()
				{
					TMap<FMapParamKey, UCoverageStructMapParamValueObject> Items;
					Items.Add(MakeKey(540, n"ReturnA"), MakeObject(541));
					Items.Add(MakeKey(541, n"ReturnB"), MakeObject(542));

					UCoverageStructMapParamValueObject Found = nullptr;
					StructObjectReturnPreserved =
						Items.Find(MakeKey(541, n"ReturnB"), Found)
						&& Found != nullptr
						&& Found.Value == 542;
					return Items;
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMapKeyValueParameterAndReturnMatrix.as"),
			ScriptSource,
			TEXT("ACoverageStructMapParamMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct map key/value parameter matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UClass* ValueObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageStructMapParamValueObject"));
		ASSERT_THAT(IsNotNull(ValueObjectClass, TEXT("UStruct map value object class should be generated")));
		if (ValueObjectClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct map key/value parameter matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FMapProperty* NameStructStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("NameStructInout"));
		FMapProperty* StringStructStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StringStructInout"));
		FMapProperty* StructStringStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructStringInout"));
		FMapProperty* StructNameStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructNameInout"));
		FMapProperty* StructObjectStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructObjectInout"));
		ASSERT_THAT(IsNotNull(NameStructStorageProperty, TEXT("TMap<FName,FStruct> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StringStructStorageProperty, TEXT("TMap<FString,FStruct> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StructStringStorageProperty, TEXT("TMap<FStruct,FString> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StructNameStorageProperty, TEXT("TMap<FStruct,FName> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StructObjectStorageProperty, TEXT("TMap<FStruct,UObject> inout storage should reflect")));
		if (NameStructStorageProperty == nullptr || StringStructStorageProperty == nullptr || StructStringStorageProperty == nullptr
			|| StructNameStorageProperty == nullptr || StructObjectStorageProperty == nullptr)
		{
			return;
		}

		FStructProperty* NameStructValueProperty = CastField<FStructProperty>(NameStructStorageProperty->ValueProp);
		FStructProperty* StringStructValueProperty = CastField<FStructProperty>(StringStructStorageProperty->ValueProp);
		FStructProperty* StructStringKeyProperty = CastField<FStructProperty>(StructStringStorageProperty->KeyProp);
		FStructProperty* StructNameKeyProperty = CastField<FStructProperty>(StructNameStorageProperty->KeyProp);
		FStructProperty* StructObjectKeyProperty = CastField<FStructProperty>(StructObjectStorageProperty->KeyProp);
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(NameStructStorageProperty->KeyProp),
			TEXT("TMap<FName,FStruct> key should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(NameStructValueProperty, TEXT("TMap<FName,FStruct> value should expose the AS value struct")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StringStructStorageProperty->KeyProp),
			TEXT("TMap<FString,FStruct> key should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(StringStructValueProperty, TEXT("TMap<FString,FStruct> value should expose the AS value struct")));
		ASSERT_THAT(IsNotNull(StructStringKeyProperty, TEXT("TMap<FStruct,FString> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(CastField<FStrProperty>(StructStringStorageProperty->ValueProp),
			TEXT("TMap<FStruct,FString> value should reflect as FStrProperty")));
		ASSERT_THAT(IsNotNull(StructNameKeyProperty, TEXT("TMap<FStruct,FName> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(CastField<FNameProperty>(StructNameStorageProperty->ValueProp),
			TEXT("TMap<FStruct,FName> value should reflect as FNameProperty")));
		ASSERT_THAT(IsNotNull(StructObjectKeyProperty, TEXT("TMap<FStruct,UObject> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(StructObjectStorageProperty->ValueProp),
			TEXT("TMap<FStruct,UObject> value should reflect as FObjectProperty")));
		if (NameStructValueProperty == nullptr || NameStructValueProperty->Struct == nullptr
			|| StringStructValueProperty == nullptr || StringStructValueProperty->Struct == nullptr
			|| StructStringKeyProperty == nullptr || StructStringKeyProperty->Struct == nullptr
			|| StructNameKeyProperty == nullptr || StructNameKeyProperty->Struct == nullptr
			|| StructObjectKeyProperty == nullptr || StructObjectKeyProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(NameStructValueProperty->Struct, StringStructValueProperty->Struct,
			TEXT("TMap<FName,FStruct> and TMap<FString,FStruct> should reuse the same value UScriptStruct")));
		ASSERT_THAT(AreEqual(StructStringKeyProperty->Struct, StructNameKeyProperty->Struct,
			TEXT("TMap<FStruct,FString> and TMap<FStruct,FName> should reuse the same key UScriptStruct")));
		ASSERT_THAT(AreEqual(StructStringKeyProperty->Struct, StructObjectKeyProperty->Struct,
			TEXT("TMap<FStruct,FString> and TMap<FStruct,UObject> should reuse the same key UScriptStruct")));

		FIntProperty* ValueScoreProperty = FindFProperty<FIntProperty>(NameStructValueProperty->Struct, TEXT("Score"));
		FStrProperty* ValueLabelProperty = FindFProperty<FStrProperty>(NameStructValueProperty->Struct, TEXT("Label"));
		FIntProperty* KeyIDProperty = FindFProperty<FIntProperty>(StructStringKeyProperty->Struct, TEXT("ID"));
		FNameProperty* KeyTagProperty = FindFProperty<FNameProperty>(StructStringKeyProperty->Struct, TEXT("Tag"));
		ASSERT_THAT(IsNotNull(ValueScoreProperty, TEXT("Map value struct should expose Score")));
		ASSERT_THAT(IsNotNull(ValueLabelProperty, TEXT("Map value struct should expose Label")));
		ASSERT_THAT(IsNotNull(KeyIDProperty, TEXT("Map key struct should expose ID")));
		ASSERT_THAT(IsNotNull(KeyTagProperty, TEXT("Map key struct should expose Tag")));
		if (ValueScoreProperty == nullptr || ValueLabelProperty == nullptr || KeyIDProperty == nullptr || KeyTagProperty == nullptr)
		{
			return;
		}

		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;

		FFunctionInvoker NameValueInvoker(*TestRunner, Actor, TEXT("CountNameStructValue"));
		ASSERT_THAT(IsTrue(NameValueInvoker.IsValid(), TEXT("CountNameStructValue should be invokable")));
		if (!NameValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(NameValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountNameStructValue should expose TMap<FName,FStruct> parameter slot")));
		FMapProperty* MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FName,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			FName(TEXT("ValueA")), *ValueScoreProperty, *ValueLabelProperty, 101, FString(TEXT("NameValueA")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			FName(TEXT("ValueB")), *ValueScoreProperty, *ValueLabelProperty, 102, FString(TEXT("NameValueB")))));
		ASSERT_THAT(AreEqual(2, NameValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FName,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructValuePreserved"), true,
			TEXT("TMap<FName,FStruct> by-value call should preserve simple key and struct value"))));

		FFunctionInvoker NameInInvoker(*TestRunner, Actor, TEXT("CountNameStructIn"));
		ASSERT_THAT(IsTrue(NameInInvoker.IsValid(), TEXT("CountNameStructIn should be invokable")));
		if (!NameInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(NameInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountNameStructIn should expose TMap<FName,FStruct> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FName,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			FName(TEXT("InA")), *ValueScoreProperty, *ValueLabelProperty, 111, FString(TEXT("NameInA")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			FName(TEXT("InB")), *ValueScoreProperty, *ValueLabelProperty, 112, FString(TEXT("NameInB")))));
		ASSERT_THAT(AreEqual(2, NameInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FName,FStruct> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructInPreserved"), true,
			TEXT("TMap<FName,FStruct> const-ref call should preserve simple key and struct value"))));

		FIntProperty* ObjectValueProperty = FindFProperty<FIntProperty>(ValueObjectClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ObjectValueProperty, TEXT("Map value object should expose Value")));
		if (ObjectValueProperty == nullptr)
		{
			return;
		}

		auto MakeObjectValue = [ValueObjectClass, ObjectValueProperty](int32 Value) -> UObject*
		{
			UObject* Object = NewObject<UObject>(GetTransientPackage(), ValueObjectClass, NAME_None, RF_Transient);
			if (Object != nullptr)
			{
				ObjectValueProperty->SetPropertyValue_InContainer(Object, Value);
			}
			return Object;
		};

		int32 Count = 0;
		const FStructProperty* StructValueProperty = nullptr;
		const void* StructValueAddress = nullptr;
		FString StringValue;
		FName NameValue;
		UObject* ObjectValue = nullptr;

		FFunctionInvoker NameOutInvoker(*TestRunner, Actor, TEXT("FillNameStructOut"));
		ASSERT_THAT(IsTrue(NameOutInvoker.IsValid(), TEXT("FillNameStructOut should be invokable")));
		if (!NameOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(NameOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillNameStructOut should expose TMap<FName,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FName,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(NameOutInvoker.Call(), TEXT("FillNameStructOut should execute through reflection")));
		FScriptMapHelper NameOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, NameOutHelper.Num(), TEXT("TMap<FName,FStruct> &out should write two entries")));
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FNameProperty, FName>(
			*TestRunner, *MapProperty, ParamSlot, FName(TEXT("OutB")), StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			122, FString(TEXT("NameOutB")), TEXT("TMap<FName,FStruct> &out value"))));

		FFunctionInvoker NameInoutInvoker(*TestRunner, Actor, TEXT("MutateNameStructInout"));
		ASSERT_THAT(IsTrue(NameInoutInvoker.IsValid(), TEXT("MutateNameStructInout should be invokable")));
		if (!NameInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(NameInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateNameStructInout should expose TMap<FName,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FName,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			FName(TEXT("InoutA")), *ValueScoreProperty, *ValueLabelProperty, 131, FString(TEXT("NameInoutA")))));
		ASSERT_THAT(IsTrue(NameInoutInvoker.Call(), TEXT("MutateNameStructInout should execute through reflection")));
		FScriptMapHelper NameInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, NameInoutHelper.Num(), TEXT("TMap<FName,FStruct> &inout should add one entry")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FNameProperty, FName>(
			*TestRunner, *MapProperty, ParamSlot, FName(TEXT("InoutA")), StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			231, FString(TEXT("NameInoutMutated")), TEXT("TMap<FName,FStruct> &inout mutated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructInoutSawOriginal"), true,
			TEXT("TMap<FName,FStruct> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructInoutMutated"), true,
			TEXT("TMap<FName,FStruct> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("NameStructInout"), Count),
			TEXT("TMap<FName,FStruct> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FName,FStruct> &inout storage should contain two entries")));

		FFunctionInvoker NameReturnInvoker(*TestRunner, Actor, TEXT("ReturnNameStruct"));
		ASSERT_THAT(IsTrue(NameReturnInvoker.IsValid(), TEXT("ReturnNameStruct should be invokable")));
		if (!NameReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(NameReturnInvoker.Call(), TEXT("ReturnNameStruct should execute through reflection")));
		UFunction* ReturnMapFunction = Actor->FindFunction(TEXT("ReturnNameStruct"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnNameStruct should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		FMapProperty* MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FName,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		void* ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(NameReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FName,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper NameReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, NameReturnHelper.Num(), TEXT("TMap<FName,FStruct> return should contain two entries")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FNameProperty, FName>(
			*TestRunner, *MapReturnProperty, ReturnSlot, FName(TEXT("ReturnB")), StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			142, FString(TEXT("NameReturnB")), TEXT("TMap<FName,FStruct> return value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("NameStructReturnPreserved"), true,
			TEXT("TMap<FName,FStruct> return should preserve script-side Find behavior"))));

		FFunctionInvoker StringValueInvoker(*TestRunner, Actor, TEXT("CountStringStructValue"));
		ASSERT_THAT(IsTrue(StringValueInvoker.IsValid(), TEXT("CountStringStructValue should be invokable")));
		if (!StringValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStringStructValue should expose TMap<FString,FStruct> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FString,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			FString(TEXT("ValueA")), *ValueScoreProperty, *ValueLabelProperty, 201, FString(TEXT("StringValueA")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			FString(TEXT("ValueB")), *ValueScoreProperty, *ValueLabelProperty, 202, FString(TEXT("StringValueB")))));
		ASSERT_THAT(AreEqual(2, StringValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FString,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringStructValueCount"), 2,
			TEXT("TMap<FString,FStruct> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructValuePreserved"), true,
			TEXT("TMap<FString,FStruct> by-value call should preserve simple key and struct value"))));

		FFunctionInvoker StringInInvoker(*TestRunner, Actor, TEXT("CountStringStructIn"));
		ASSERT_THAT(IsTrue(StringInInvoker.IsValid(), TEXT("CountStringStructIn should be invokable")));
		if (!StringInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStringStructIn should expose TMap<FString,FStruct> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FString,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			FString(TEXT("InA")), *ValueScoreProperty, *ValueLabelProperty, 211, FString(TEXT("StringInA")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			FString(TEXT("InB")), *ValueScoreProperty, *ValueLabelProperty, 212, FString(TEXT("StringInB")))));
		ASSERT_THAT(AreEqual(2, StringInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FString,FStruct> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StringStructInCount"), 2,
			TEXT("TMap<FString,FStruct> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructInPreserved"), true,
			TEXT("TMap<FString,FStruct> const-ref call should preserve simple key and struct value"))));

		FFunctionInvoker StringOutInvoker(*TestRunner, Actor, TEXT("FillStringStructOut"));
		ASSERT_THAT(IsTrue(StringOutInvoker.IsValid(), TEXT("FillStringStructOut should be invokable")));
		if (!StringOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStringStructOut should expose TMap<FString,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FString,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringOutInvoker.Call(), TEXT("FillStringStructOut should execute through reflection")));
		FScriptMapHelper StringOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StringOutHelper.Num(), TEXT("TMap<FString,FStruct> &out should write two entries")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FStrProperty, FString>(
			*TestRunner, *MapProperty, ParamSlot, FString(TEXT("OutB")), StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			222, FString(TEXT("StringOutB")), TEXT("TMap<FString,FStruct> &out value"))));

		FFunctionInvoker StringInoutInvoker(*TestRunner, Actor, TEXT("MutateStringStructInout"));
		ASSERT_THAT(IsTrue(StringInoutInvoker.IsValid(), TEXT("MutateStringStructInout should be invokable")));
		if (!StringInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStringStructInout should expose TMap<FString,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FString,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			FString(TEXT("InoutA")), *ValueScoreProperty, *ValueLabelProperty, 231, FString(TEXT("StringInoutA")))));
		ASSERT_THAT(IsTrue(StringInoutInvoker.Call(), TEXT("MutateStringStructInout should execute through reflection")));
		FScriptMapHelper StringInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StringInoutHelper.Num(), TEXT("TMap<FString,FStruct> &inout should add one entry")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FStrProperty, FString>(
			*TestRunner, *MapProperty, ParamSlot, FString(TEXT("InoutA")), StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			331, FString(TEXT("StringInoutMutated")), TEXT("TMap<FString,FStruct> &inout mutated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructInoutSawOriginal"), true,
			TEXT("TMap<FString,FStruct> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructInoutMutated"), true,
			TEXT("TMap<FString,FStruct> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StringStructInout"), Count),
			TEXT("TMap<FString,FStruct> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FString,FStruct> &inout storage should contain two entries")));

		FFunctionInvoker StringReturnInvoker(*TestRunner, Actor, TEXT("ReturnStringStruct"));
		ASSERT_THAT(IsTrue(StringReturnInvoker.IsValid(), TEXT("ReturnStringStruct should be invokable")));
		if (!StringReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StringReturnInvoker.Call(), TEXT("ReturnStringStruct should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStringStruct"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStringStruct should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FString,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(StringReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FString,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StringReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, StringReturnHelper.Num(), TEXT("TMap<FString,FStruct> return should contain two entries")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FStrProperty, FString>(
			*TestRunner, *MapReturnProperty, ReturnSlot, FString(TEXT("ReturnB")), StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			242, FString(TEXT("StringReturnB")), TEXT("TMap<FString,FStruct> return value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StringStructReturnPreserved"), true,
			TEXT("TMap<FString,FStruct> return should preserve script-side Find behavior"))));

		FFunctionInvoker StructStringValueInvoker(*TestRunner, Actor, TEXT("CountStructStringValue"));
		ASSERT_THAT(IsTrue(StructStringValueInvoker.IsValid(), TEXT("CountStructStringValue should be invokable")));
		if (!StructStringValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStringValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructStringValue should expose TMap<FStruct,FString> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FString> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyStringValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 300, FName(TEXT("ValueA")), FString(TEXT("StructStringValueA")))));
		ASSERT_THAT(IsTrue(AddStructKeyStringValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 301, FName(TEXT("ValueB")), FString(TEXT("StructStringValueB")))));
		ASSERT_THAT(AreEqual(2, StructStringValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,FString> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStringValueCount"), 2,
			TEXT("TMap<FStruct,FString> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringValuePreserved"), true,
			TEXT("TMap<FStruct,FString> by-value call should preserve struct key and string value"))));

		FFunctionInvoker StructStringInInvoker(*TestRunner, Actor, TEXT("CountStructStringIn"));
		ASSERT_THAT(IsTrue(StructStringInInvoker.IsValid(), TEXT("CountStructStringIn should be invokable")));
		if (!StructStringInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStringInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructStringIn should expose TMap<FStruct,FString> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FString> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyStringValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 310, FName(TEXT("InA")), FString(TEXT("StructStringInA")))));
		ASSERT_THAT(IsTrue(AddStructKeyStringValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 311, FName(TEXT("InB")), FString(TEXT("StructStringInB")))));
		ASSERT_THAT(AreEqual(2, StructStringInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,FString> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructStringInCount"), 2,
			TEXT("TMap<FStruct,FString> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringInPreserved"), true,
			TEXT("TMap<FStruct,FString> const-ref call should preserve struct key and string value"))));

		FFunctionInvoker StructStringOutInvoker(*TestRunner, Actor, TEXT("FillStructStringOut"));
		ASSERT_THAT(IsTrue(StructStringOutInvoker.IsValid(), TEXT("FillStructStringOut should be invokable")));
		if (!StructStringOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStringOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructStringOut should expose TMap<FStruct,FString> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FString> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStringOutInvoker.Call(), TEXT("FillStructStringOut should execute through reflection")));
		FScriptMapHelper StructStringOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructStringOutHelper.Num(), TEXT("TMap<FStruct,FString> &out should write two entries")));
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 321, FName(TEXT("OutB")), StringValue)));
		ASSERT_THAT(AreEqual(FString(TEXT("StructStringOutB")), StringValue,
			TEXT("TMap<FStruct,FString> &out should preserve string values")));

		FFunctionInvoker StructStringInoutInvoker(*TestRunner, Actor, TEXT("MutateStructStringInout"));
		ASSERT_THAT(IsTrue(StructStringInoutInvoker.IsValid(), TEXT("MutateStructStringInout should be invokable")));
		if (!StructStringInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStringInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructStringInout should expose TMap<FStruct,FString> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FString> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyStringValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 330, FName(TEXT("InoutA")), FString(TEXT("StructStringInoutA")))));
		ASSERT_THAT(IsTrue(StructStringInoutInvoker.Call(), TEXT("MutateStructStringInout should execute through reflection")));
		FScriptMapHelper StructStringInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructStringInoutHelper.Num(), TEXT("TMap<FStruct,FString> &inout should add one entry")));
		StringValue.Reset();
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FStrProperty, FString>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 330, FName(TEXT("InoutA")), StringValue)));
		ASSERT_THAT(AreEqual(FString(TEXT("StructStringInoutMutated")), StringValue,
			TEXT("TMap<FStruct,FString> &inout should mutate existing value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringInoutSawOriginal"), true,
			TEXT("TMap<FStruct,FString> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringInoutMutated"), true,
			TEXT("TMap<FStruct,FString> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructStringInout"), Count),
			TEXT("TMap<FStruct,FString> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FString> &inout storage should contain two entries")));

		FFunctionInvoker StructStringReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructString"));
		ASSERT_THAT(IsTrue(StructStringReturnInvoker.IsValid(), TEXT("ReturnStructString should be invokable")));
		if (!StructStringReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructStringReturnInvoker.Call(), TEXT("ReturnStructString should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructString"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructString should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,FString> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(StructStringReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,FString> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructStringReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, StructStringReturnHelper.Num(), TEXT("TMap<FStruct,FString> return should contain two entries")));
		StringValue.Reset();
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FStrProperty, FString>(*TestRunner, *MapReturnProperty, ReturnSlot,
			*KeyIDProperty, *KeyTagProperty, 341, FName(TEXT("ReturnB")), StringValue)));
		ASSERT_THAT(AreEqual(FString(TEXT("StructStringReturnB")), StringValue,
			TEXT("TMap<FStruct,FString> return should preserve string values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructStringReturnPreserved"), true,
			TEXT("TMap<FStruct,FString> return should preserve script-side Find behavior"))));

		FFunctionInvoker StructNameValueInvoker(*TestRunner, Actor, TEXT("CountStructNameValue"));
		ASSERT_THAT(IsTrue(StructNameValueInvoker.IsValid(), TEXT("CountStructNameValue should be invokable")));
		if (!StructNameValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructNameValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructNameValue should expose TMap<FStruct,FName> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FName> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyNameValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 400, FName(TEXT("ValueA")), FName(TEXT("StructNameValueA")))));
		ASSERT_THAT(IsTrue(AddStructKeyNameValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 401, FName(TEXT("ValueB")), FName(TEXT("StructNameValueB")))));
		ASSERT_THAT(AreEqual(2, StructNameValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,FName> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructNameValueCount"), 2,
			TEXT("TMap<FStruct,FName> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameValuePreserved"), true,
			TEXT("TMap<FStruct,FName> by-value call should preserve struct key and name value"))));

		FFunctionInvoker StructNameInInvoker(*TestRunner, Actor, TEXT("CountStructNameIn"));
		ASSERT_THAT(IsTrue(StructNameInInvoker.IsValid(), TEXT("CountStructNameIn should be invokable")));
		if (!StructNameInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructNameInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructNameIn should expose TMap<FStruct,FName> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FName> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyNameValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 410, FName(TEXT("InA")), FName(TEXT("StructNameInA")))));
		ASSERT_THAT(IsTrue(AddStructKeyNameValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 411, FName(TEXT("InB")), FName(TEXT("StructNameInB")))));
		ASSERT_THAT(AreEqual(2, StructNameInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,FName> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructNameInCount"), 2,
			TEXT("TMap<FStruct,FName> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameInPreserved"), true,
			TEXT("TMap<FStruct,FName> const-ref call should preserve struct key and name value"))));

		FFunctionInvoker StructNameOutInvoker(*TestRunner, Actor, TEXT("FillStructNameOut"));
		ASSERT_THAT(IsTrue(StructNameOutInvoker.IsValid(), TEXT("FillStructNameOut should be invokable")));
		if (!StructNameOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructNameOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructNameOut should expose TMap<FStruct,FName> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FName> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructNameOutInvoker.Call(), TEXT("FillStructNameOut should execute through reflection")));
		FScriptMapHelper StructNameOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructNameOutHelper.Num(), TEXT("TMap<FStruct,FName> &out should write two entries")));
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 421, FName(TEXT("OutB")), NameValue)));
		ASSERT_THAT(AreEqual(FName(TEXT("StructNameOutB")), NameValue,
			TEXT("TMap<FStruct,FName> &out should preserve name values")));

		FFunctionInvoker StructNameInoutInvoker(*TestRunner, Actor, TEXT("MutateStructNameInout"));
		ASSERT_THAT(IsTrue(StructNameInoutInvoker.IsValid(), TEXT("MutateStructNameInout should be invokable")));
		if (!StructNameInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructNameInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructNameInout should expose TMap<FStruct,FName> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,FName> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyNameValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 430, FName(TEXT("InoutA")), FName(TEXT("StructNameInoutA")))));
		ASSERT_THAT(IsTrue(StructNameInoutInvoker.Call(), TEXT("MutateStructNameInout should execute through reflection")));
		FScriptMapHelper StructNameInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructNameInoutHelper.Num(), TEXT("TMap<FStruct,FName> &inout should add one entry")));
		NameValue = NAME_None;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FNameProperty, FName>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 430, FName(TEXT("InoutA")), NameValue)));
		ASSERT_THAT(AreEqual(FName(TEXT("StructNameInoutMutated")), NameValue,
			TEXT("TMap<FStruct,FName> &inout should mutate existing value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameInoutSawOriginal"), true,
			TEXT("TMap<FStruct,FName> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameInoutMutated"), true,
			TEXT("TMap<FStruct,FName> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructNameInout"), Count),
			TEXT("TMap<FStruct,FName> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,FName> &inout storage should contain two entries")));

		FFunctionInvoker StructNameReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructName"));
		ASSERT_THAT(IsTrue(StructNameReturnInvoker.IsValid(), TEXT("ReturnStructName should be invokable")));
		if (!StructNameReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructNameReturnInvoker.Call(), TEXT("ReturnStructName should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructName"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructName should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,FName> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(StructNameReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,FName> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructNameReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, StructNameReturnHelper.Num(), TEXT("TMap<FStruct,FName> return should contain two entries")));
		NameValue = NAME_None;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FNameProperty, FName>(*TestRunner, *MapReturnProperty, ReturnSlot,
			*KeyIDProperty, *KeyTagProperty, 441, FName(TEXT("ReturnB")), NameValue)));
		ASSERT_THAT(AreEqual(FName(TEXT("StructNameReturnB")), NameValue,
			TEXT("TMap<FStruct,FName> return should preserve name values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructNameReturnPreserved"), true,
			TEXT("TMap<FStruct,FName> return should preserve script-side Find behavior"))));

		UObject* ObjectValueA = MakeObjectValue(501);
		UObject* ObjectValueB = MakeObjectValue(502);
		UObject* ObjectInA = MakeObjectValue(511);
		UObject* ObjectInB = MakeObjectValue(512);
		UObject* ObjectInoutA = MakeObjectValue(531);
		ASSERT_THAT(IsNotNull(ObjectValueA, TEXT("Object map value A should be created")));
		ASSERT_THAT(IsNotNull(ObjectValueB, TEXT("Object map value B should be created")));
		ASSERT_THAT(IsNotNull(ObjectInA, TEXT("Object map in A should be created")));
		ASSERT_THAT(IsNotNull(ObjectInB, TEXT("Object map in B should be created")));
		ASSERT_THAT(IsNotNull(ObjectInoutA, TEXT("Object map inout A should be created")));
		if (ObjectValueA == nullptr || ObjectValueB == nullptr || ObjectInA == nullptr || ObjectInB == nullptr || ObjectInoutA == nullptr)
		{
			return;
		}

		FFunctionInvoker StructObjectValueInvoker(*TestRunner, Actor, TEXT("CountStructObjectValue"));
		ASSERT_THAT(IsTrue(StructObjectValueInvoker.IsValid(), TEXT("CountStructObjectValue should be invokable")));
		if (!StructObjectValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructObjectValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructObjectValue should expose TMap<FStruct,UObject> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,UObject> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyObjectValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 500, FName(TEXT("ValueA")), ObjectValueA)));
		ASSERT_THAT(IsTrue(AddStructKeyObjectValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 501, FName(TEXT("ValueB")), ObjectValueB)));
		ASSERT_THAT(AreEqual(2, StructObjectValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,UObject> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructObjectValueCount"), 2,
			TEXT("TMap<FStruct,UObject> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectValuePreserved"), true,
			TEXT("TMap<FStruct,UObject> by-value call should preserve struct key and object value"))));

		FFunctionInvoker StructObjectInInvoker(*TestRunner, Actor, TEXT("CountStructObjectIn"));
		ASSERT_THAT(IsTrue(StructObjectInInvoker.IsValid(), TEXT("CountStructObjectIn should be invokable")));
		if (!StructObjectInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructObjectInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructObjectIn should expose TMap<FStruct,UObject> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,UObject> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyObjectValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 510, FName(TEXT("InA")), ObjectInA)));
		ASSERT_THAT(IsTrue(AddStructKeyObjectValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 511, FName(TEXT("InB")), ObjectInB)));
		ASSERT_THAT(AreEqual(2, StructObjectInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,UObject> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructObjectInCount"), 2,
			TEXT("TMap<FStruct,UObject> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectInPreserved"), true,
			TEXT("TMap<FStruct,UObject> const-ref call should preserve struct key and object value"))));

		FFunctionInvoker StructObjectOutInvoker(*TestRunner, Actor, TEXT("FillStructObjectOut"));
		ASSERT_THAT(IsTrue(StructObjectOutInvoker.IsValid(), TEXT("FillStructObjectOut should be invokable")));
		if (!StructObjectOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructObjectOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructObjectOut should expose TMap<FStruct,UObject> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,UObject> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructObjectOutInvoker.Call(), TEXT("FillStructObjectOut should execute through reflection")));
		FScriptMapHelper StructObjectOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructObjectOutHelper.Num(), TEXT("TMap<FStruct,UObject> &out should write two entries")));
		ObjectValue = nullptr;
		ASSERT_THAT(IsTrue(GetObjectMapValueByStructKey(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 521, FName(TEXT("OutB")), ObjectValue)));
		ASSERT_THAT(IsNotNull(ObjectValue, TEXT("TMap<FStruct,UObject> &out should preserve object references")));
		if (ObjectValue == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(522, ObjectValueProperty->GetPropertyValue_InContainer(ObjectValue),
			TEXT("TMap<FStruct,UObject> &out should preserve object fields")));

		FFunctionInvoker StructObjectInoutInvoker(*TestRunner, Actor, TEXT("MutateStructObjectInout"));
		ASSERT_THAT(IsTrue(StructObjectInoutInvoker.IsValid(), TEXT("MutateStructObjectInout should be invokable")));
		if (!StructObjectInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructObjectInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructObjectInout should expose TMap<FStruct,UObject> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,UObject> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeyObjectValueToMap(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 530, FName(TEXT("InoutA")), ObjectInoutA)));
		ASSERT_THAT(IsTrue(StructObjectInoutInvoker.Call(), TEXT("MutateStructObjectInout should execute through reflection")));
		FScriptMapHelper StructObjectInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructObjectInoutHelper.Num(), TEXT("TMap<FStruct,UObject> &inout should add one entry")));
		ObjectValue = nullptr;
		ASSERT_THAT(IsTrue(GetObjectMapValueByStructKey(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 530, FName(TEXT("InoutA")), ObjectValue)));
		ASSERT_THAT(IsNotNull(ObjectValue, TEXT("TMap<FStruct,UObject> &inout should preserve object references")));
		if (ObjectValue == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(631, ObjectValueProperty->GetPropertyValue_InContainer(ObjectValue),
			TEXT("TMap<FStruct,UObject> &inout should mutate existing object value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectInoutSawOriginal"), true,
			TEXT("TMap<FStruct,UObject> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectInoutMutated"), true,
			TEXT("TMap<FStruct,UObject> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructObjectInout"), Count),
			TEXT("TMap<FStruct,UObject> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,UObject> &inout storage should contain two entries")));

		FFunctionInvoker StructObjectReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructObject"));
		ASSERT_THAT(IsTrue(StructObjectReturnInvoker.IsValid(), TEXT("ReturnStructObject should be invokable")));
		if (!StructObjectReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructObjectReturnInvoker.Call(), TEXT("ReturnStructObject should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructObject"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructObject should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,UObject> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(StructObjectReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,UObject> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructObjectReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, StructObjectReturnHelper.Num(), TEXT("TMap<FStruct,UObject> return should contain two entries")));
		ObjectValue = nullptr;
		ASSERT_THAT(IsTrue(GetObjectMapValueByStructKey(*TestRunner, *MapReturnProperty, ReturnSlot,
			*KeyIDProperty, *KeyTagProperty, 541, FName(TEXT("ReturnB")), ObjectValue)));
		ASSERT_THAT(IsNotNull(ObjectValue, TEXT("TMap<FStruct,UObject> return should preserve object references")));
		if (ObjectValue == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(542, ObjectValueProperty->GetPropertyValue_InContainer(ObjectValue),
			TEXT("TMap<FStruct,UObject> return should preserve object fields")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructObjectReturnPreserved"), true,
			TEXT("TMap<FStruct,UObject> return should preserve script-side Find behavior"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT map primitive permutations: bool key, bool value, and float value paths.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMapPrimitiveKeyValueParameterAndReturnMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_MapPrimitiveKeyValueParameterAndReturnMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		const FString ScriptSource =
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FMapPrimitiveKey
			{
				UPROPERTY()
				int ID = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FMapPrimitiveKey& Other) const
				{
					return ID == Other.ID && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(ID * 977) + Tag.GetHash();
				}
			}

			USTRUCT(BlueprintType)
			struct FMapPrimitiveValue
			{
				UPROPERTY()
				int Score = 0;

				UPROPERTY()
				FString Label;
			}
			)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

			UCLASS()
			class UCoverageStructMapPrimitiveKeyObject : UObject
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructMapPrimitiveMatrixActor : AActor
			{
				UPROPERTY()
				int BoolStructValueCount = 0;

				UPROPERTY()
				int BoolStructInCount = 0;

				UPROPERTY()
				TMap<bool, FMapPrimitiveValue> BoolStructInout;

				UPROPERTY()
				bool BoolStructValuePreserved = false;

				UPROPERTY()
				bool BoolStructInPreserved = false;

				UPROPERTY()
				bool BoolStructInoutSawOriginal = false;

				UPROPERTY()
				bool BoolStructInoutMutated = false;

				UPROPERTY()
				bool BoolStructReturnPreserved = false;

				UPROPERTY()
				int FloatStructValueCount = 0;

				UPROPERTY()
				int FloatStructInCount = 0;

				UPROPERTY()
				TMap<float, FMapPrimitiveValue> FloatStructInout;

				UPROPERTY()
				bool FloatStructValuePreserved = false;

				UPROPERTY()
				bool FloatStructInPreserved = false;

				UPROPERTY()
				bool FloatStructInoutSawOriginal = false;

				UPROPERTY()
				bool FloatStructInoutMutated = false;

				UPROPERTY()
				bool FloatStructReturnPreserved = false;

				UPROPERTY()
				int ObjectStructValueCount = 0;

				UPROPERTY()
				int ObjectStructInCount = 0;

				UPROPERTY()
				TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue> ObjectStructInout;

				UPROPERTY()
				bool ObjectStructValuePreserved = false;

				UPROPERTY()
				bool ObjectStructInPreserved = false;

				UPROPERTY()
				bool ObjectStructInoutSawOriginal = false;

				UPROPERTY()
				bool ObjectStructInoutMutated = false;

				UPROPERTY()
				bool ObjectStructReturnPreserved = false;

				UPROPERTY()
				int StructBoolValueCount = 0;

				UPROPERTY()
				int StructBoolInCount = 0;

				UPROPERTY()
				TMap<FMapPrimitiveKey, bool> StructBoolInout;

				UPROPERTY()
				bool StructBoolValuePreserved = false;

				UPROPERTY()
				bool StructBoolInPreserved = false;

				UPROPERTY()
				bool StructBoolInoutSawOriginal = false;

				UPROPERTY()
				bool StructBoolInoutMutated = false;

				UPROPERTY()
				bool StructBoolReturnPreserved = false;

				UPROPERTY()
				int StructFloatValueCount = 0;

				UPROPERTY()
				int StructFloatInCount = 0;

				UPROPERTY()
				TMap<FMapPrimitiveKey, float> StructFloatInout;

				UPROPERTY()
				bool StructFloatValuePreserved = false;

				UPROPERTY()
				bool StructFloatInPreserved = false;

				UPROPERTY()
				bool StructFloatInoutSawOriginal = false;

				UPROPERTY()
				bool StructFloatInoutMutated = false;

				UPROPERTY()
				bool StructFloatReturnPreserved = false;

				FMapPrimitiveKey MakeKey(int ID, FName Tag)
				{
					FMapPrimitiveKey Key;
					Key.ID = ID;
					Key.Tag = Tag;
					return Key;
				}

				FMapPrimitiveValue MakeValue(int Score, FString Label)
				{
					FMapPrimitiveValue Value;
					Value.Score = Score;
					Value.Label = Label;
					return Value;
				}

				UCoverageStructMapPrimitiveKeyObject MakeObjectKey(int Value)
				{
					UCoverageStructMapPrimitiveKeyObject Object = Cast<UCoverageStructMapPrimitiveKeyObject>(NewObject(this, UCoverageStructMapPrimitiveKeyObject::StaticClass()));
					Object.Value = Value;
					return Object;
				}

				UFUNCTION(BlueprintCallable)
				int CountBoolStructValue(TMap<bool, FMapPrimitiveValue> Items)
				{
					BoolStructValueCount = Items.Num();
					FMapPrimitiveValue Found;
					BoolStructValuePreserved =
						Items.Find(false, Found)
						&& Found.Score == 102
						&& Found.Label == "BoolValueFalse";
					return BoolStructValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountBoolStructIn(const TMap<bool, FMapPrimitiveValue>&in Items)
				{
					BoolStructInCount = Items.Num();
					FMapPrimitiveValue Found;
					BoolStructInPreserved =
						Items.Find(true, Found)
						&& Found.Score == 111
						&& Found.Label == "BoolInTrue";
					return BoolStructInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillBoolStructOut(TMap<bool, FMapPrimitiveValue>&out Items)
				{
					Items.Add(true, MakeValue(121, "BoolOutTrue"));
					Items.Add(false, MakeValue(122, "BoolOutFalse"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateBoolStructInout(TMap<bool, FMapPrimitiveValue>&inout Items)
				{
					FMapPrimitiveValue Found;
					BoolStructInoutSawOriginal =
						Items.Find(true, Found)
						&& Found.Score == 131
						&& Found.Label == "BoolInoutTrue";
					Items.Add(true, MakeValue(231, "BoolInoutMutated"));
					Items.Add(false, MakeValue(232, "BoolInoutAdded"));
					BoolStructInout = Items;

					FMapPrimitiveValue Mutated;
					BoolStructInoutMutated =
						Items.Find(true, Mutated)
						&& Mutated.Score == 231
						&& Mutated.Label == "BoolInoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<bool, FMapPrimitiveValue> ReturnBoolStruct()
				{
					TMap<bool, FMapPrimitiveValue> Items;
					Items.Add(true, MakeValue(141, "BoolReturnTrue"));
					Items.Add(false, MakeValue(142, "BoolReturnFalse"));

					FMapPrimitiveValue Found;
					BoolStructReturnPreserved =
						Items.Find(false, Found)
						&& Found.Score == 142
						&& Found.Label == "BoolReturnFalse";
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountFloatStructValue(TMap<float, FMapPrimitiveValue> Items)
				{
					FloatStructValueCount = Items.Num();
					FMapPrimitiveValue Found;
					FloatStructValuePreserved =
						Items.Find(102.5f, Found)
						&& Found.Score == 102
						&& Found.Label == "FloatValueB";
					return FloatStructValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountFloatStructIn(const TMap<float, FMapPrimitiveValue>&in Items)
				{
					FloatStructInCount = Items.Num();
					FMapPrimitiveValue Found;
					FloatStructInPreserved =
						Items.Find(112.5f, Found)
						&& Found.Score == 112
						&& Found.Label == "FloatInB";
					return FloatStructInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillFloatStructOut(TMap<float, FMapPrimitiveValue>&out Items)
				{
					Items.Add(121.5f, MakeValue(121, "FloatOutA"));
					Items.Add(122.5f, MakeValue(122, "FloatOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateFloatStructInout(TMap<float, FMapPrimitiveValue>&inout Items)
				{
					FMapPrimitiveValue Found;
					FloatStructInoutSawOriginal =
						Items.Find(131.5f, Found)
						&& Found.Score == 131
						&& Found.Label == "FloatInoutA";
					Items.Add(131.5f, MakeValue(231, "FloatInoutMutated"));
					Items.Add(132.5f, MakeValue(232, "FloatInoutAdded"));
					FloatStructInout = Items;

					FMapPrimitiveValue Mutated;
					FloatStructInoutMutated =
						Items.Find(131.5f, Mutated)
						&& Mutated.Score == 231
						&& Mutated.Label == "FloatInoutMutated";
				}

				UFUNCTION(BlueprintCallable)
				TMap<float, FMapPrimitiveValue> ReturnFloatStruct()
				{
					TMap<float, FMapPrimitiveValue> Items;
					Items.Add(141.5f, MakeValue(141, "FloatReturnA"));
					Items.Add(142.5f, MakeValue(142, "FloatReturnB"));

					FMapPrimitiveValue Found;
					FloatStructReturnPreserved =
						Items.Find(142.5f, Found)
						&& Found.Score == 142
						&& Found.Label == "FloatReturnB";
					return Items;
				}

				UFUNCTION(BlueprintCallable)
				int CountObjectStructValue(TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue> Items)
				{
					ObjectStructValueCount = Items.Num();
					for (auto Element : Items)
					{
						UCoverageStructMapPrimitiveKeyObject Key = Element.GetKey();
						FMapPrimitiveValue Value = Element.GetValue();
						if (Key != nullptr && Key.Value == 202 && Value.Score == 202 && Value.Label == "ObjectValueB")
						{
							ObjectStructValuePreserved = true;
						}
					}
					return ObjectStructValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountObjectStructIn(const TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue>&in Items)
				{
					ObjectStructInCount = Items.Num();
					for (auto Element : Items)
					{
						UCoverageStructMapPrimitiveKeyObject Key = Element.GetKey();
						FMapPrimitiveValue Value = Element.GetValue();
						if (Key != nullptr && Key.Value == 212 && Value.Score == 212 && Value.Label == "ObjectInB")
						{
							ObjectStructInPreserved = true;
						}
					}
					return ObjectStructInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillObjectStructOut(TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue>&out Items)
				{
					Items.Add(MakeObjectKey(221), MakeValue(221, "ObjectOutA"));
					Items.Add(MakeObjectKey(222), MakeValue(222, "ObjectOutB"));
				}

				UFUNCTION(BlueprintCallable)
				void MutateObjectStructInout(TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue>&inout Items)
				{
					for (auto Element : Items)
					{
						UCoverageStructMapPrimitiveKeyObject Key = Element.GetKey();
						FMapPrimitiveValue Value = Element.GetValue();
						if (Key != nullptr && Key.Value == 231 && Value.Score == 231 && Value.Label == "ObjectInoutA")
						{
							ObjectStructInoutSawOriginal = true;
							Element.SetValue(MakeValue(331, "ObjectInoutMutated"));
						}
					}
					Items.Add(MakeObjectKey(232), MakeValue(332, "ObjectInoutAdded"));
					ObjectStructInout = Items;

					for (auto Element : ObjectStructInout)
					{
						UCoverageStructMapPrimitiveKeyObject Key = Element.GetKey();
						FMapPrimitiveValue Value = Element.GetValue();
						if (Key != nullptr && Key.Value == 231 && Value.Score == 331 && Value.Label == "ObjectInoutMutated")
						{
							ObjectStructInoutMutated = true;
						}
					}
				}

				UFUNCTION(BlueprintCallable)
				TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue> ReturnObjectStruct()
				{
					TMap<UCoverageStructMapPrimitiveKeyObject, FMapPrimitiveValue> Items;
					Items.Add(MakeObjectKey(241), MakeValue(241, "ObjectReturnA"));
					Items.Add(MakeObjectKey(242), MakeValue(242, "ObjectReturnB"));

					for (auto Element : Items)
					{
						UCoverageStructMapPrimitiveKeyObject Key = Element.GetKey();
						FMapPrimitiveValue Value = Element.GetValue();
						if (Key != nullptr && Key.Value == 242 && Value.Score == 242 && Value.Label == "ObjectReturnB")
						{
					ObjectStructReturnPreserved = true;
						}
					}
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountStructBoolValue(TMap<FMapPrimitiveKey, bool> Items)
				{
					StructBoolValueCount = Items.Num();
					bool Found = true;
					StructBoolValuePreserved =
						Items.Find(MakeKey(301, n"ValueFalse"), Found)
						&& !Found;
					return StructBoolValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructBoolIn(const TMap<FMapPrimitiveKey, bool>&in Items)
				{
					StructBoolInCount = Items.Num();
					bool Found = false;
					StructBoolInPreserved =
						Items.Find(MakeKey(310, n"InTrue"), Found)
						&& Found;
					return StructBoolInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructBoolOut(TMap<FMapPrimitiveKey, bool>&out Items)
				{
					Items.Add(MakeKey(320, n"OutTrue"), true);
					Items.Add(MakeKey(321, n"OutFalse"), false);
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructBoolInout(TMap<FMapPrimitiveKey, bool>&inout Items)
				{
					FMapPrimitiveKey Existing = MakeKey(330, n"InoutTrue");
					bool Found = false;
					StructBoolInoutSawOriginal =
						Items.Find(Existing, Found)
						&& Found;
					Items.Add(Existing, false);
					Items.Add(MakeKey(331, n"InoutAdded"), true);
					StructBoolInout = Items;

					bool Mutated = true;
					StructBoolInoutMutated =
						Items.Find(Existing, Mutated)
						&& !Mutated;
				}

				UFUNCTION(BlueprintCallable)
				TMap<FMapPrimitiveKey, bool> ReturnStructBool()
				{
					TMap<FMapPrimitiveKey, bool> Items;
					Items.Add(MakeKey(340, n"ReturnTrue"), true);
					Items.Add(MakeKey(341, n"ReturnFalse"), false);

					bool Found = true;
					StructBoolReturnPreserved =
						Items.Find(MakeKey(341, n"ReturnFalse"), Found)
						&& !Found;
					return Items;
				}
				)AS") + TEXT("\n") +
			ASTEST_AS(R"AS(

				UFUNCTION(BlueprintCallable)
				int CountStructFloatValue(TMap<FMapPrimitiveKey, float> Items)
				{
					StructFloatValueCount = Items.Num();
					float Found = 0.0f;
					StructFloatValuePreserved =
						Items.Find(MakeKey(501, n"ValueFloatB"), Found)
						&& Found == 502.5f;
					return StructFloatValueCount;
				}

				UFUNCTION(BlueprintCallable)
				int CountStructFloatIn(const TMap<FMapPrimitiveKey, float>&in Items)
				{
					StructFloatInCount = Items.Num();
					float Found = 0.0f;
					StructFloatInPreserved =
						Items.Find(MakeKey(511, n"InFloatB"), Found)
						&& Found == 512.5f;
					return StructFloatInCount;
				}

				UFUNCTION(BlueprintCallable)
				void FillStructFloatOut(TMap<FMapPrimitiveKey, float>&out Items)
				{
					Items.Add(MakeKey(520, n"OutFloatA"), 521.5f);
					Items.Add(MakeKey(521, n"OutFloatB"), 522.5f);
				}

				UFUNCTION(BlueprintCallable)
				void MutateStructFloatInout(TMap<FMapPrimitiveKey, float>&inout Items)
				{
					FMapPrimitiveKey Existing = MakeKey(530, n"InoutFloatA");
					float Found = 0.0f;
					StructFloatInoutSawOriginal =
						Items.Find(Existing, Found)
						&& Found == 531.5f;
					Items.Add(Existing, 631.5f);
					Items.Add(MakeKey(531, n"InoutFloatB"), 632.5f);
					StructFloatInout = Items;

					float Mutated = 0.0f;
					StructFloatInoutMutated =
						Items.Find(Existing, Mutated)
						&& Mutated == 631.5f;
				}

				UFUNCTION(BlueprintCallable)
				TMap<FMapPrimitiveKey, float> ReturnStructFloat()
				{
					TMap<FMapPrimitiveKey, float> Items;
					Items.Add(MakeKey(540, n"ReturnFloatA"), 541.5f);
					Items.Add(MakeKey(541, n"ReturnFloatB"), 542.5f);

					float Found = 0.0f;
					StructFloatReturnPreserved =
						Items.Find(MakeKey(541, n"ReturnFloatB"), Found)
						&& Found == 542.5f;
					return Items;
				}
			}
			)AS");

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMapPrimitiveKeyValueParameterAndReturnMatrix.as"),
			ScriptSource,
			TEXT("ACoverageStructMapPrimitiveMatrixActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct primitive map matrix actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct primitive map matrix actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FMapProperty* BoolStructStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("BoolStructInout"));
		FMapProperty* FloatStructStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("FloatStructInout"));
		FMapProperty* ObjectStructStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("ObjectStructInout"));
		FMapProperty* StructBoolStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructBoolInout"));
		FMapProperty* StructFloatStorageProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructFloatInout"));
		ASSERT_THAT(IsNotNull(BoolStructStorageProperty, TEXT("TMap<bool,FStruct> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(FloatStructStorageProperty, TEXT("TMap<float,FStruct> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(ObjectStructStorageProperty, TEXT("TMap<UObject,FStruct> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StructBoolStorageProperty, TEXT("TMap<FStruct,bool> inout storage should reflect")));
		ASSERT_THAT(IsNotNull(StructFloatStorageProperty, TEXT("TMap<FStruct,float> inout storage should reflect")));
		if (BoolStructStorageProperty == nullptr || FloatStructStorageProperty == nullptr || ObjectStructStorageProperty == nullptr
			|| StructBoolStorageProperty == nullptr || StructFloatStorageProperty == nullptr)
		{
			return;
		}

		FStructProperty* BoolStructValueProperty = CastField<FStructProperty>(BoolStructStorageProperty->ValueProp);
		FStructProperty* FloatStructValueProperty = CastField<FStructProperty>(FloatStructStorageProperty->ValueProp);
		FStructProperty* ObjectStructValueProperty = CastField<FStructProperty>(ObjectStructStorageProperty->ValueProp);
		FStructProperty* StructBoolKeyProperty = CastField<FStructProperty>(StructBoolStorageProperty->KeyProp);
		FStructProperty* StructFloatKeyProperty = CastField<FStructProperty>(StructFloatStorageProperty->KeyProp);
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(BoolStructStorageProperty->KeyProp),
			TEXT("TMap<bool,FStruct> key should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(BoolStructValueProperty, TEXT("TMap<bool,FStruct> value should expose the AS value struct")));
		ASSERT_THAT(IsNotNull(CastField<FScriptFloatProperty>(FloatStructStorageProperty->KeyProp),
			TEXT("TMap<float,FStruct> key should reflect as script float storage")));
		ASSERT_THAT(IsNotNull(FloatStructValueProperty, TEXT("TMap<float,FStruct> value should expose the AS value struct")));
		ASSERT_THAT(IsNotNull(CastField<FObjectProperty>(ObjectStructStorageProperty->KeyProp),
			TEXT("TMap<UObject,FStruct> key should reflect as FObjectProperty")));
		ASSERT_THAT(IsNotNull(ObjectStructValueProperty, TEXT("TMap<UObject,FStruct> value should expose the AS value struct")));
		ASSERT_THAT(IsNotNull(StructBoolKeyProperty, TEXT("TMap<FStruct,bool> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(CastField<FBoolProperty>(StructBoolStorageProperty->ValueProp),
			TEXT("TMap<FStruct,bool> value should reflect as FBoolProperty")));
		ASSERT_THAT(IsNotNull(StructFloatKeyProperty, TEXT("TMap<FStruct,float> key should expose the AS key struct")));
		ASSERT_THAT(IsNotNull(CastField<FScriptFloatProperty>(StructFloatStorageProperty->ValueProp),
			TEXT("TMap<FStruct,float> value should reflect as script float storage")));
		if (BoolStructValueProperty == nullptr || BoolStructValueProperty->Struct == nullptr
			|| FloatStructValueProperty == nullptr || FloatStructValueProperty->Struct == nullptr
			|| ObjectStructValueProperty == nullptr || ObjectStructValueProperty->Struct == nullptr
			|| StructBoolKeyProperty == nullptr || StructBoolKeyProperty->Struct == nullptr
			|| StructFloatKeyProperty == nullptr || StructFloatKeyProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(BoolStructValueProperty->Struct, FloatStructValueProperty->Struct,
			TEXT("TMap<bool,FStruct> and TMap<float,FStruct> should reuse the same value UScriptStruct")));
		ASSERT_THAT(AreEqual(BoolStructValueProperty->Struct, ObjectStructValueProperty->Struct,
			TEXT("TMap<bool,FStruct> and TMap<UObject,FStruct> should reuse the same value UScriptStruct")));
		ASSERT_THAT(AreEqual(StructBoolKeyProperty->Struct, StructFloatKeyProperty->Struct,
			TEXT("TMap<FStruct,bool> and TMap<FStruct,float> should reuse the same key UScriptStruct")));

		FIntProperty* ValueScoreProperty = FindFProperty<FIntProperty>(BoolStructValueProperty->Struct, TEXT("Score"));
		FStrProperty* ValueLabelProperty = FindFProperty<FStrProperty>(BoolStructValueProperty->Struct, TEXT("Label"));
		FIntProperty* KeyIDProperty = FindFProperty<FIntProperty>(StructBoolKeyProperty->Struct, TEXT("ID"));
		FNameProperty* KeyTagProperty = FindFProperty<FNameProperty>(StructBoolKeyProperty->Struct, TEXT("Tag"));
		ASSERT_THAT(IsNotNull(ValueScoreProperty, TEXT("Map primitive value struct should expose Score")));
		ASSERT_THAT(IsNotNull(ValueLabelProperty, TEXT("Map primitive value struct should expose Label")));
		ASSERT_THAT(IsNotNull(KeyIDProperty, TEXT("Map primitive key struct should expose ID")));
		ASSERT_THAT(IsNotNull(KeyTagProperty, TEXT("Map primitive key struct should expose Tag")));
		if (ValueScoreProperty == nullptr || ValueLabelProperty == nullptr || KeyIDProperty == nullptr || KeyTagProperty == nullptr)
		{
			return;
		}

		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		FMapProperty* MapProperty = nullptr;
		UFunction* ReturnMapFunction = nullptr;
		FMapProperty* MapReturnProperty = nullptr;
		void* ReturnSlot = nullptr;
		int32 Count = 0;
		const FStructProperty* StructValueProperty = nullptr;
		const void* StructValueAddress = nullptr;

		FFunctionInvoker BoolStructValueInvoker(*TestRunner, Actor, TEXT("CountBoolStructValue"));
		ASSERT_THAT(IsTrue(BoolStructValueInvoker.IsValid(), TEXT("CountBoolStructValue should be invokable")));
		if (!BoolStructValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolStructValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountBoolStructValue should expose TMap<bool,FStruct> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<bool,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			true, *ValueScoreProperty, *ValueLabelProperty, 101, FString(TEXT("BoolValueTrue")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			false, *ValueScoreProperty, *ValueLabelProperty, 102, FString(TEXT("BoolValueFalse")))));
		ASSERT_THAT(AreEqual(2, BoolStructValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<bool,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolStructValueCount"), 2,
			TEXT("TMap<bool,FStruct> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructValuePreserved"), true,
			TEXT("TMap<bool,FStruct> by-value call should preserve bool key and struct value"))));

		FFunctionInvoker BoolStructInInvoker(*TestRunner, Actor, TEXT("CountBoolStructIn"));
		ASSERT_THAT(IsTrue(BoolStructInInvoker.IsValid(), TEXT("CountBoolStructIn should be invokable")));
		if (!BoolStructInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolStructInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountBoolStructIn should expose TMap<bool,FStruct> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<bool,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			true, *ValueScoreProperty, *ValueLabelProperty, 111, FString(TEXT("BoolInTrue")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			false, *ValueScoreProperty, *ValueLabelProperty, 112, FString(TEXT("BoolInFalse")))));
		ASSERT_THAT(AreEqual(2, BoolStructInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<bool,FStruct> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("BoolStructInCount"), 2,
			TEXT("TMap<bool,FStruct> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructInPreserved"), true,
			TEXT("TMap<bool,FStruct> const-ref call should preserve bool key and struct value"))));

		FFunctionInvoker BoolStructOutInvoker(*TestRunner, Actor, TEXT("FillBoolStructOut"));
		ASSERT_THAT(IsTrue(BoolStructOutInvoker.IsValid(), TEXT("FillBoolStructOut should be invokable")));
		if (!BoolStructOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolStructOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillBoolStructOut should expose TMap<bool,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<bool,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolStructOutInvoker.Call(), TEXT("FillBoolStructOut should execute through reflection")));
		FScriptMapHelper BoolStructOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, BoolStructOutHelper.Num(), TEXT("TMap<bool,FStruct> &out should write two entries")));
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FBoolProperty, bool>(
			*TestRunner, *MapProperty, ParamSlot, false, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			122, FString(TEXT("BoolOutFalse")), TEXT("TMap<bool,FStruct> &out value"))));

		FFunctionInvoker BoolStructInoutInvoker(*TestRunner, Actor, TEXT("MutateBoolStructInout"));
		ASSERT_THAT(IsTrue(BoolStructInoutInvoker.IsValid(), TEXT("MutateBoolStructInout should be invokable")));
		if (!BoolStructInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolStructInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateBoolStructInout should expose TMap<bool,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<bool,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			true, *ValueScoreProperty, *ValueLabelProperty, 131, FString(TEXT("BoolInoutTrue")))));
		ASSERT_THAT(IsTrue(BoolStructInoutInvoker.Call(), TEXT("MutateBoolStructInout should execute through reflection")));
		FScriptMapHelper BoolStructInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, BoolStructInoutHelper.Num(), TEXT("TMap<bool,FStruct> &inout should add one entry")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FBoolProperty, bool>(
			*TestRunner, *MapProperty, ParamSlot, true, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			231, FString(TEXT("BoolInoutMutated")), TEXT("TMap<bool,FStruct> &inout mutated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructInoutSawOriginal"), true,
			TEXT("TMap<bool,FStruct> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructInoutMutated"), true,
			TEXT("TMap<bool,FStruct> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("BoolStructInout"), Count),
			TEXT("TMap<bool,FStruct> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<bool,FStruct> &inout storage should contain two entries")));

		FFunctionInvoker BoolStructReturnInvoker(*TestRunner, Actor, TEXT("ReturnBoolStruct"));
		ASSERT_THAT(IsTrue(BoolStructReturnInvoker.IsValid(), TEXT("ReturnBoolStruct should be invokable")));
		if (!BoolStructReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(BoolStructReturnInvoker.Call(), TEXT("ReturnBoolStruct should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnBoolStruct"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnBoolStruct should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<bool,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(BoolStructReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<bool,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper BoolStructReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, BoolStructReturnHelper.Num(), TEXT("TMap<bool,FStruct> return should contain two entries")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FBoolProperty, bool>(
			*TestRunner, *MapReturnProperty, ReturnSlot, false, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			142, FString(TEXT("BoolReturnFalse")), TEXT("TMap<bool,FStruct> return value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("BoolStructReturnPreserved"), true,
			TEXT("TMap<bool,FStruct> return should preserve script-side Find behavior"))));

		FFunctionInvoker FloatStructValueInvoker(*TestRunner, Actor, TEXT("CountFloatStructValue"));
		ASSERT_THAT(IsTrue(FloatStructValueInvoker.IsValid(), TEXT("CountFloatStructValue should be invokable")));
		if (!FloatStructValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FloatStructValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountFloatStructValue should expose TMap<float,FStruct> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<float,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			101.5, *ValueScoreProperty, *ValueLabelProperty, 101, FString(TEXT("FloatValueA")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			102.5, *ValueScoreProperty, *ValueLabelProperty, 102, FString(TEXT("FloatValueB")))));
		ASSERT_THAT(AreEqual(2, FloatStructValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<float,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FloatStructValueCount"), 2,
			TEXT("TMap<float,FStruct> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructValuePreserved"), true,
			TEXT("TMap<float,FStruct> by-value call should preserve float key and struct value"))));

		FFunctionInvoker FloatStructInInvoker(*TestRunner, Actor, TEXT("CountFloatStructIn"));
		ASSERT_THAT(IsTrue(FloatStructInInvoker.IsValid(), TEXT("CountFloatStructIn should be invokable")));
		if (!FloatStructInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FloatStructInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountFloatStructIn should expose TMap<float,FStruct> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<float,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			111.5, *ValueScoreProperty, *ValueLabelProperty, 111, FString(TEXT("FloatInA")))));
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			112.5, *ValueScoreProperty, *ValueLabelProperty, 112, FString(TEXT("FloatInB")))));
		ASSERT_THAT(AreEqual(2, FloatStructInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<float,FStruct> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("FloatStructInCount"), 2,
			TEXT("TMap<float,FStruct> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructInPreserved"), true,
			TEXT("TMap<float,FStruct> const-ref call should preserve float key and struct value"))));

		FFunctionInvoker FloatStructOutInvoker(*TestRunner, Actor, TEXT("FillFloatStructOut"));
		ASSERT_THAT(IsTrue(FloatStructOutInvoker.IsValid(), TEXT("FillFloatStructOut should be invokable")));
		if (!FloatStructOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FloatStructOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillFloatStructOut should expose TMap<float,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<float,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(FloatStructOutInvoker.Call(), TEXT("FillFloatStructOut should execute through reflection")));
		FScriptMapHelper FloatStructOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, FloatStructOutHelper.Num(), TEXT("TMap<float,FStruct> &out should write two entries")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FScriptFloatProperty, FScriptFloatValue>(
			*TestRunner, *MapProperty, ParamSlot, 122.5, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			122, FString(TEXT("FloatOutB")), TEXT("TMap<float,FStruct> &out value"))));

		FFunctionInvoker FloatStructInoutInvoker(*TestRunner, Actor, TEXT("MutateFloatStructInout"));
		ASSERT_THAT(IsTrue(FloatStructInoutInvoker.IsValid(), TEXT("MutateFloatStructInout should be invokable")));
		if (!FloatStructInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FloatStructInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateFloatStructInout should expose TMap<float,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<float,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddSimpleKeyStructValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			131.5, *ValueScoreProperty, *ValueLabelProperty, 131, FString(TEXT("FloatInoutA")))));
		ASSERT_THAT(IsTrue(FloatStructInoutInvoker.Call(), TEXT("MutateFloatStructInout should execute through reflection")));
		FScriptMapHelper FloatStructInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, FloatStructInoutHelper.Num(), TEXT("TMap<float,FStruct> &inout should add one entry")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FScriptFloatProperty, FScriptFloatValue>(
			*TestRunner, *MapProperty, ParamSlot, 131.5, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			231, FString(TEXT("FloatInoutMutated")), TEXT("TMap<float,FStruct> &inout mutated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructInoutSawOriginal"), true,
			TEXT("TMap<float,FStruct> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructInoutMutated"), true,
			TEXT("TMap<float,FStruct> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("FloatStructInout"), Count),
			TEXT("TMap<float,FStruct> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<float,FStruct> &inout storage should contain two entries")));

		FFunctionInvoker FloatStructReturnInvoker(*TestRunner, Actor, TEXT("ReturnFloatStruct"));
		ASSERT_THAT(IsTrue(FloatStructReturnInvoker.IsValid(), TEXT("ReturnFloatStruct should be invokable")));
		if (!FloatStructReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(FloatStructReturnInvoker.Call(), TEXT("ReturnFloatStruct should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnFloatStruct"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnFloatStruct should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<float,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(FloatStructReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<float,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper FloatStructReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, FloatStructReturnHelper.Num(), TEXT("TMap<float,FStruct> return should contain two entries")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetSimpleKeyStructMapValue<FScriptFloatProperty, FScriptFloatValue>(
			*TestRunner, *MapReturnProperty, ReturnSlot, 142.5, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			142, FString(TEXT("FloatReturnB")), TEXT("TMap<float,FStruct> return value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("FloatStructReturnPreserved"), true,
			TEXT("TMap<float,FStruct> return should preserve script-side Find behavior"))));

		UClass* KeyObjectClass = FindGeneratedClass(&Engine, TEXT("UCoverageStructMapPrimitiveKeyObject"));
		ASSERT_THAT(IsNotNull(KeyObjectClass, TEXT("UStruct primitive map key object class should be generated")));
		if (KeyObjectClass == nullptr)
		{
			return;
		}
		FIntProperty* ObjectKeyValueProperty = FindFProperty<FIntProperty>(KeyObjectClass, TEXT("Value"));
		ASSERT_THAT(IsNotNull(ObjectKeyValueProperty, TEXT("Object map key should expose Value")));
		if (ObjectKeyValueProperty == nullptr)
		{
			return;
		}
		auto MakeObjectKeyValue = [KeyObjectClass, ObjectKeyValueProperty](int32 Value) -> UObject*
		{
			UObject* Object = NewObject<UObject>(GetTransientPackage(), KeyObjectClass, NAME_None, RF_Transient);
			if (Object != nullptr)
			{
				ObjectKeyValueProperty->SetPropertyValue_InContainer(Object, Value);
			}
			return Object;
		};
		UObject* ObjectValueA = MakeObjectKeyValue(201);
		UObject* ObjectValueB = MakeObjectKeyValue(202);
		UObject* ObjectInA = MakeObjectKeyValue(211);
		UObject* ObjectInB = MakeObjectKeyValue(212);
		UObject* ObjectInoutA = MakeObjectKeyValue(231);
		ASSERT_THAT(IsNotNull(ObjectValueA, TEXT("Object struct key value A should be created")));
		ASSERT_THAT(IsNotNull(ObjectValueB, TEXT("Object struct key value B should be created")));
		ASSERT_THAT(IsNotNull(ObjectInA, TEXT("Object struct key in A should be created")));
		ASSERT_THAT(IsNotNull(ObjectInB, TEXT("Object struct key in B should be created")));
		ASSERT_THAT(IsNotNull(ObjectInoutA, TEXT("Object struct key inout A should be created")));
		if (ObjectValueA == nullptr || ObjectValueB == nullptr || ObjectInA == nullptr || ObjectInB == nullptr || ObjectInoutA == nullptr)
		{
			return;
		}

		FFunctionInvoker ObjectStructValueInvoker(*TestRunner, Actor, TEXT("CountObjectStructValue"));
		ASSERT_THAT(IsTrue(ObjectStructValueInvoker.IsValid(), TEXT("CountObjectStructValue should be invokable")));
		if (!ObjectStructValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectStructValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountObjectStructValue should expose TMap<UObject,FStruct> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<UObject,FStruct> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddObjectKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			ObjectValueA, *ValueScoreProperty, *ValueLabelProperty, 201, FString(TEXT("ObjectValueA")))));
		ASSERT_THAT(IsTrue(AddObjectKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			ObjectValueB, *ValueScoreProperty, *ValueLabelProperty, 202, FString(TEXT("ObjectValueB")))));
		ASSERT_THAT(AreEqual(2, ObjectStructValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<UObject,FStruct> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectStructValueCount"), 2,
			TEXT("TMap<UObject,FStruct> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructValuePreserved"), true,
			TEXT("TMap<UObject,FStruct> by-value call should preserve object key and struct value"))));

		FFunctionInvoker ObjectStructInInvoker(*TestRunner, Actor, TEXT("CountObjectStructIn"));
		ASSERT_THAT(IsTrue(ObjectStructInInvoker.IsValid(), TEXT("CountObjectStructIn should be invokable")));
		if (!ObjectStructInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectStructInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountObjectStructIn should expose TMap<UObject,FStruct> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<UObject,FStruct> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddObjectKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			ObjectInA, *ValueScoreProperty, *ValueLabelProperty, 211, FString(TEXT("ObjectInA")))));
		ASSERT_THAT(IsTrue(AddObjectKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			ObjectInB, *ValueScoreProperty, *ValueLabelProperty, 212, FString(TEXT("ObjectInB")))));
		ASSERT_THAT(AreEqual(2, ObjectStructInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<UObject,FStruct> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("ObjectStructInCount"), 2,
			TEXT("TMap<UObject,FStruct> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructInPreserved"), true,
			TEXT("TMap<UObject,FStruct> const-ref call should preserve object key and struct value"))));

		FFunctionInvoker ObjectStructOutInvoker(*TestRunner, Actor, TEXT("FillObjectStructOut"));
		ASSERT_THAT(IsTrue(ObjectStructOutInvoker.IsValid(), TEXT("FillObjectStructOut should be invokable")));
		if (!ObjectStructOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectStructOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillObjectStructOut should expose TMap<UObject,FStruct> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<UObject,FStruct> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectStructOutInvoker.Call(), TEXT("FillObjectStructOut should execute through reflection")));
		FScriptMapHelper ObjectStructOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, ObjectStructOutHelper.Num(), TEXT("TMap<UObject,FStruct> &out should write two entries")));
		bool bFoundObjectStructOut = false;
		FObjectProperty* ObjectStructOutKeyProperty = CastField<FObjectProperty>(MapProperty->KeyProp);
		ASSERT_THAT(IsNotNull(ObjectStructOutKeyProperty, TEXT("TMap<UObject,FStruct> &out key should reflect as FObjectProperty")));
		if (ObjectStructOutKeyProperty == nullptr)
		{
			return;
		}
		for (int32 SparseIndex = 0; SparseIndex < ObjectStructOutHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!ObjectStructOutHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}
			UObject* KeyObject = ObjectStructOutKeyProperty->GetObjectPropertyValue(ObjectStructOutHelper.GetKeyPtr(SparseIndex));
			const int32 KeyValue = KeyObject != nullptr ? ObjectKeyValueProperty->GetPropertyValue_InContainer(KeyObject) : INDEX_NONE;
			if (KeyValue == 222)
			{
				StructValueAddress = ObjectStructOutHelper.GetValuePtr(SparseIndex);
				bFoundObjectStructOut = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundObjectStructOut, TEXT("TMap<UObject,FStruct> &out should contain object key 222")));
		if (!bFoundObjectStructOut || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			222, FString(TEXT("ObjectOutB")), TEXT("TMap<UObject,FStruct> &out value"))));

		FFunctionInvoker ObjectStructInoutInvoker(*TestRunner, Actor, TEXT("MutateObjectStructInout"));
		ASSERT_THAT(IsTrue(ObjectStructInoutInvoker.IsValid(), TEXT("MutateObjectStructInout should be invokable")));
		if (!ObjectStructInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectStructInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateObjectStructInout should expose TMap<UObject,FStruct> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<UObject,FStruct> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddObjectKeyStructValueToMap(*TestRunner, *MapProperty, ParamSlot,
			ObjectInoutA, *ValueScoreProperty, *ValueLabelProperty, 231, FString(TEXT("ObjectInoutA")))));
		ASSERT_THAT(IsTrue(ObjectStructInoutInvoker.Call(), TEXT("MutateObjectStructInout should execute through reflection")));
		FScriptMapHelper ObjectStructInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, ObjectStructInoutHelper.Num(), TEXT("TMap<UObject,FStruct> &inout should add one entry")));
		StructValueProperty = nullptr;
		StructValueAddress = nullptr;
		ASSERT_THAT(IsTrue(GetObjectKeyStructMapValue(
			*TestRunner, *MapProperty, ParamSlot, ObjectInoutA, StructValueProperty, StructValueAddress)));
		if (StructValueProperty == nullptr || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			331, FString(TEXT("ObjectInoutMutated")), TEXT("TMap<UObject,FStruct> &inout mutated value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructInoutSawOriginal"), true,
			TEXT("TMap<UObject,FStruct> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructInoutMutated"), true,
			TEXT("TMap<UObject,FStruct> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("ObjectStructInout"), Count),
			TEXT("TMap<UObject,FStruct> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<UObject,FStruct> &inout storage should contain two entries")));

		FFunctionInvoker ObjectStructReturnInvoker(*TestRunner, Actor, TEXT("ReturnObjectStruct"));
		ASSERT_THAT(IsTrue(ObjectStructReturnInvoker.IsValid(), TEXT("ReturnObjectStruct should be invokable")));
		if (!ObjectStructReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ObjectStructReturnInvoker.Call(), TEXT("ReturnObjectStruct should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnObjectStruct"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnObjectStruct should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<UObject,FStruct> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(ObjectStructReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<UObject,FStruct> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper ObjectStructReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, ObjectStructReturnHelper.Num(), TEXT("TMap<UObject,FStruct> return should contain two entries")));
		FObjectProperty* ObjectStructReturnKeyProperty = CastField<FObjectProperty>(MapReturnProperty->KeyProp);
		ASSERT_THAT(IsNotNull(ObjectStructReturnKeyProperty, TEXT("TMap<UObject,FStruct> return key should reflect as FObjectProperty")));
		if (ObjectStructReturnKeyProperty == nullptr)
		{
			return;
		}
		bool bFoundObjectStructReturn = false;
		StructValueAddress = nullptr;
		for (int32 SparseIndex = 0; SparseIndex < ObjectStructReturnHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!ObjectStructReturnHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}
			UObject* KeyObject = ObjectStructReturnKeyProperty->GetObjectPropertyValue(ObjectStructReturnHelper.GetKeyPtr(SparseIndex));
			const int32 KeyValue = KeyObject != nullptr ? ObjectKeyValueProperty->GetPropertyValue_InContainer(KeyObject) : INDEX_NONE;
			if (KeyValue == 242)
			{
				StructValueAddress = ObjectStructReturnHelper.GetValuePtr(SparseIndex);
				bFoundObjectStructReturn = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundObjectStructReturn, TEXT("TMap<UObject,FStruct> return should contain object key 242")));
		if (!bFoundObjectStructReturn || StructValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectStructScoreLabelFields(*TestRunner, *ValueScoreProperty, *ValueLabelProperty, StructValueAddress,
			242, FString(TEXT("ObjectReturnB")), TEXT("TMap<UObject,FStruct> return value"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ObjectStructReturnPreserved"), true,
			TEXT("TMap<UObject,FStruct> return should preserve script-side iteration behavior"))));

		bool BoolValue = false;
		FFunctionInvoker StructBoolValueInvoker(*TestRunner, Actor, TEXT("CountStructBoolValue"));
		ASSERT_THAT(IsTrue(StructBoolValueInvoker.IsValid(), TEXT("CountStructBoolValue should be invokable")));
		if (!StructBoolValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructBoolValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructBoolValue should expose TMap<FStruct,bool> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,bool> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 300, FName(TEXT("ValueTrue")), true)));
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 301, FName(TEXT("ValueFalse")), false)));
		ASSERT_THAT(AreEqual(2, StructBoolValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,bool> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructBoolValueCount"), 2,
			TEXT("TMap<FStruct,bool> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolValuePreserved"), true,
			TEXT("TMap<FStruct,bool> by-value call should preserve struct key and bool value"))));

		FFunctionInvoker StructBoolInInvoker(*TestRunner, Actor, TEXT("CountStructBoolIn"));
		ASSERT_THAT(IsTrue(StructBoolInInvoker.IsValid(), TEXT("CountStructBoolIn should be invokable")));
		if (!StructBoolInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructBoolInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructBoolIn should expose TMap<FStruct,bool> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,bool> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 310, FName(TEXT("InTrue")), true)));
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 311, FName(TEXT("InFalse")), false)));
		ASSERT_THAT(AreEqual(2, StructBoolInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,bool> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructBoolInCount"), 2,
			TEXT("TMap<FStruct,bool> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolInPreserved"), true,
			TEXT("TMap<FStruct,bool> const-ref call should preserve struct key and bool value"))));

		FFunctionInvoker StructBoolOutInvoker(*TestRunner, Actor, TEXT("FillStructBoolOut"));
		ASSERT_THAT(IsTrue(StructBoolOutInvoker.IsValid(), TEXT("FillStructBoolOut should be invokable")));
		if (!StructBoolOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructBoolOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructBoolOut should expose TMap<FStruct,bool> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,bool> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructBoolOutInvoker.Call(), TEXT("FillStructBoolOut should execute through reflection")));
		FScriptMapHelper StructBoolOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructBoolOutHelper.Num(), TEXT("TMap<FStruct,bool> &out should write two entries")));
		BoolValue = true;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 321, FName(TEXT("OutFalse")), BoolValue)));
		ASSERT_THAT(AreEqual(false, BoolValue, TEXT("TMap<FStruct,bool> &out should preserve bool values")));

		FFunctionInvoker StructBoolInoutInvoker(*TestRunner, Actor, TEXT("MutateStructBoolInout"));
		ASSERT_THAT(IsTrue(StructBoolInoutInvoker.IsValid(), TEXT("MutateStructBoolInout should be invokable")));
		if (!StructBoolInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructBoolInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructBoolInout should expose TMap<FStruct,bool> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,bool> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 330, FName(TEXT("InoutTrue")), true)));
		ASSERT_THAT(IsTrue(StructBoolInoutInvoker.Call(), TEXT("MutateStructBoolInout should execute through reflection")));
		FScriptMapHelper StructBoolInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructBoolInoutHelper.Num(), TEXT("TMap<FStruct,bool> &inout should add one entry")));
		BoolValue = true;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FBoolProperty, bool>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 330, FName(TEXT("InoutTrue")), BoolValue)));
		ASSERT_THAT(AreEqual(false, BoolValue, TEXT("TMap<FStruct,bool> &inout should mutate existing value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolInoutSawOriginal"), true,
			TEXT("TMap<FStruct,bool> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolInoutMutated"), true,
			TEXT("TMap<FStruct,bool> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructBoolInout"), Count),
			TEXT("TMap<FStruct,bool> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,bool> &inout storage should contain two entries")));

		FFunctionInvoker StructBoolReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructBool"));
		ASSERT_THAT(IsTrue(StructBoolReturnInvoker.IsValid(), TEXT("ReturnStructBool should be invokable")));
		if (!StructBoolReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructBoolReturnInvoker.Call(), TEXT("ReturnStructBool should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructBool"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructBool should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,bool> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(StructBoolReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,bool> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructBoolReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, StructBoolReturnHelper.Num(), TEXT("TMap<FStruct,bool> return should contain two entries")));
		BoolValue = true;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FBoolProperty, bool>(*TestRunner, *MapReturnProperty, ReturnSlot,
			*KeyIDProperty, *KeyTagProperty, 341, FName(TEXT("ReturnFalse")), BoolValue)));
		ASSERT_THAT(AreEqual(false, BoolValue, TEXT("TMap<FStruct,bool> return should preserve bool values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructBoolReturnPreserved"), true,
			TEXT("TMap<FStruct,bool> return should preserve script-side Find behavior"))));

		FScriptFloatValue FloatValue = 0.0;
		FFunctionInvoker StructFloatValueInvoker(*TestRunner, Actor, TEXT("CountStructFloatValue"));
		ASSERT_THAT(IsTrue(StructFloatValueInvoker.IsValid(), TEXT("CountStructFloatValue should be invokable")));
		if (!StructFloatValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructFloatValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructFloatValue should expose TMap<FStruct,float> parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,float> value parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 500, FName(TEXT("ValueFloatA")), 501.5)));
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 501, FName(TEXT("ValueFloatB")), 502.5)));
		ASSERT_THAT(AreEqual(2, StructFloatValueInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,float> by-value parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructFloatValueCount"), 2,
			TEXT("TMap<FStruct,float> by-value call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatValuePreserved"), true,
			TEXT("TMap<FStruct,float> by-value call should preserve struct key and float value"))));

		FFunctionInvoker StructFloatInInvoker(*TestRunner, Actor, TEXT("CountStructFloatIn"));
		ASSERT_THAT(IsTrue(StructFloatInInvoker.IsValid(), TEXT("CountStructFloatIn should be invokable")));
		if (!StructFloatInInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructFloatInInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("CountStructFloatIn should expose TMap<FStruct,float> const-ref parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,float> &in parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 510, FName(TEXT("InFloatA")), 511.5)));
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 511, FName(TEXT("InFloatB")), 512.5)));
		ASSERT_THAT(AreEqual(2, StructFloatInInvoker.CallAndReturn<int32>(0),
			TEXT("TMap<FStruct,float> const-ref parameter should count caller-provided entries")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("StructFloatInCount"), 2,
			TEXT("TMap<FStruct,float> const-ref call should update script-side count"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatInPreserved"), true,
			TEXT("TMap<FStruct,float> const-ref call should preserve struct key and float value"))));

		FFunctionInvoker StructFloatOutInvoker(*TestRunner, Actor, TEXT("FillStructFloatOut"));
		ASSERT_THAT(IsTrue(StructFloatOutInvoker.IsValid(), TEXT("FillStructFloatOut should be invokable")));
		if (!StructFloatOutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructFloatOutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("FillStructFloatOut should expose TMap<FStruct,float> out parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,float> &out parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructFloatOutInvoker.Call(), TEXT("FillStructFloatOut should execute through reflection")));
		FScriptMapHelper StructFloatOutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructFloatOutHelper.Num(), TEXT("TMap<FStruct,float> &out should write two entries")));
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 521, FName(TEXT("OutFloatB")), FloatValue)));
		ASSERT_THAT(IsNear(522.5, FloatValue, 0.0001, TEXT("TMap<FStruct,float> &out should preserve float values")));

		FFunctionInvoker StructFloatInoutInvoker(*TestRunner, Actor, TEXT("MutateStructFloatInout"));
		ASSERT_THAT(IsTrue(StructFloatInoutInvoker.IsValid(), TEXT("MutateStructFloatInout should be invokable")));
		if (!StructFloatInoutInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructFloatInoutInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("MutateStructFloatInout should expose TMap<FStruct,float> inout parameter slot")));
		MapProperty = CastField<FMapProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(MapProperty, TEXT("TMap<FStruct,float> &inout parameter should reflect as FMapProperty")));
		if (ParamSlot == nullptr || MapProperty == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddStructKeySimpleValueToMap<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 530, FName(TEXT("InoutFloatA")), 531.5)));
		ASSERT_THAT(IsTrue(StructFloatInoutInvoker.Call(), TEXT("MutateStructFloatInout should execute through reflection")));
		FScriptMapHelper StructFloatInoutHelper(MapProperty, ParamSlot);
		ASSERT_THAT(AreEqual(2, StructFloatInoutHelper.Num(), TEXT("TMap<FStruct,float> &inout should add one entry")));
		FloatValue = 0.0;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapProperty, ParamSlot,
			*KeyIDProperty, *KeyTagProperty, 530, FName(TEXT("InoutFloatA")), FloatValue)));
		ASSERT_THAT(IsNear(631.5, FloatValue, 0.0001, TEXT("TMap<FStruct,float> &inout should mutate existing value")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatInoutSawOriginal"), true,
			TEXT("TMap<FStruct,float> &inout should read caller-provided entries"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatInoutMutated"), true,
			TEXT("TMap<FStruct,float> &inout should update script-side state"))));
		ASSERT_THAT(IsTrue(GetMapNumByPath(*TestRunner, Actor, TEXT("StructFloatInout"), Count),
			TEXT("TMap<FStruct,float> &inout storage count should be readable")));
		ASSERT_THAT(AreEqual(2, Count, TEXT("TMap<FStruct,float> &inout storage should contain two entries")));

		FFunctionInvoker StructFloatReturnInvoker(*TestRunner, Actor, TEXT("ReturnStructFloat"));
		ASSERT_THAT(IsTrue(StructFloatReturnInvoker.IsValid(), TEXT("ReturnStructFloat should be invokable")));
		if (!StructFloatReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(StructFloatReturnInvoker.Call(), TEXT("ReturnStructFloat should execute through reflection")));
		ReturnMapFunction = Actor->FindFunction(TEXT("ReturnStructFloat"));
		ASSERT_THAT(IsNotNull(ReturnMapFunction, TEXT("ReturnStructFloat should reflect as a UFunction")));
		if (ReturnMapFunction == nullptr)
		{
			return;
		}
		MapReturnProperty = CastField<FMapProperty>(ReturnMapFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(MapReturnProperty, TEXT("TMap<FStruct,float> return should reflect as FMapProperty")));
		if (MapReturnProperty == nullptr)
		{
			return;
		}
		ReturnSlot = MapReturnProperty->ContainerPtrToValuePtr<void>(StructFloatReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("TMap<FStruct,float> return slot should be readable")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		FScriptMapHelper StructFloatReturnHelper(MapReturnProperty, ReturnSlot);
		ASSERT_THAT(AreEqual(2, StructFloatReturnHelper.Num(), TEXT("TMap<FStruct,float> return should contain two entries")));
		FloatValue = 0.0;
		ASSERT_THAT(IsTrue(GetSimpleMapValueByStructKey<FScriptFloatProperty, FScriptFloatValue>(*TestRunner, *MapReturnProperty, ReturnSlot,
			*KeyIDProperty, *KeyTagProperty, 541, FName(TEXT("ReturnFloatB")), FloatValue)));
		ASSERT_THAT(IsNear(542.5, FloatValue, 0.0001, TEXT("TMap<FStruct,float> return should preserve float values")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("StructFloatReturnPreserved"), true,
			TEXT("TMap<FStruct,float> return should preserve script-side Find behavior"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT optional returns: TOptional<FStruct> can return but cannot be a parameter.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructOptionalReturnMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_OptionalReturnMatrix"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructOptionalReturnMatrix.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalReturnPayload
			{
				UPROPERTY()
				int Count = 0;

				UPROPERTY()
				FString Label;
			}

			UCLASS()
			class ACoverageStructOptionalReturnActor : AActor
			{
				UPROPERTY()
				bool bSetReturnObserved = false;

				UPROPERTY()
				bool bEmptyReturnObserved = false;

				UPROPERTY()
				int LastSetCount = 0;

				UPROPERTY()
				FString LastSetLabel;

				UFUNCTION(BlueprintCallable)
				TOptional<FOptionalReturnPayload> ReturnSetPayload()
				{
					FOptionalReturnPayload Payload;
					Payload.Count = 64;
					Payload.Label = "OptionalReturn";

					TOptional<FOptionalReturnPayload> Result;
					Result.Set(Payload);

					TOptional<FOptionalReturnPayload> Observed = Result;
					bSetReturnObserved = Observed.IsSet();
					LastSetCount = Observed.GetValue().Count;
					LastSetLabel = Observed.GetValue().Label;
					return Result;
				}

				UFUNCTION(BlueprintCallable)
				TOptional<FOptionalReturnPayload> ReturnEmptyPayload()
				{
					TOptional<FOptionalReturnPayload> Result;
					bEmptyReturnObserved = !Result.IsSet();
					return Result;
				}
			}
			)AS"),
			TEXT("ACoverageStructOptionalReturnActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct optional return actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct optional return actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		UFunction* ReturnSetFunction = ScriptClass->FindFunctionByName(TEXT("ReturnSetPayload"));
		UFunction* ReturnEmptyFunction = ScriptClass->FindFunctionByName(TEXT("ReturnEmptyPayload"));
		ASSERT_THAT(IsNotNull(ReturnSetFunction, TEXT("ReturnSetPayload should reflect as a UFunction")));
		ASSERT_THAT(IsNotNull(ReturnEmptyFunction, TEXT("ReturnEmptyPayload should reflect as a UFunction")));
		if (ReturnSetFunction == nullptr || ReturnEmptyFunction == nullptr)
		{
			return;
		}

		FOptionalProperty* SetReturnProperty = CastField<FOptionalProperty>(ReturnSetFunction->GetReturnProperty());
		FOptionalProperty* EmptyReturnProperty = CastField<FOptionalProperty>(ReturnEmptyFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(SetReturnProperty, TEXT("TOptional<FStruct> set return should reflect as FOptionalProperty")));
		ASSERT_THAT(IsNotNull(EmptyReturnProperty, TEXT("TOptional<FStruct> empty return should reflect as FOptionalProperty")));
		if (SetReturnProperty == nullptr || EmptyReturnProperty == nullptr)
		{
			return;
		}

		FStructProperty* SetInnerProperty = CastField<FStructProperty>(SetReturnProperty->GetValueProperty());
		FStructProperty* EmptyInnerProperty = CastField<FStructProperty>(EmptyReturnProperty->GetValueProperty());
		ASSERT_THAT(IsNotNull(SetInnerProperty, TEXT("TOptional<FStruct> set return inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(EmptyInnerProperty, TEXT("TOptional<FStruct> empty return inner should be FStructProperty")));
		if (SetInnerProperty == nullptr || SetInnerProperty->Struct == nullptr
			|| EmptyInnerProperty == nullptr || EmptyInnerProperty->Struct == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(SetInnerProperty->Struct, EmptyInnerProperty->Struct,
			TEXT("Set and empty TOptional<FStruct> returns should use the same generated AS USTRUCT")));

		FIntProperty* CountProperty = FindFProperty<FIntProperty>(SetInnerProperty->Struct, TEXT("Count"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(SetInnerProperty->Struct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(CountProperty, TEXT("Optional return payload should expose Count")));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("Optional return payload should expose Label")));
		if (CountProperty == nullptr || LabelProperty == nullptr)
		{
			return;
		}

		FFunctionInvoker SetReturnInvoker(*TestRunner, Actor, TEXT("ReturnSetPayload"));
		ASSERT_THAT(IsTrue(SetReturnInvoker.IsValid(), TEXT("ReturnSetPayload should be invokable")));
		if (!SetReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetReturnInvoker.Call(), TEXT("ReturnSetPayload should execute through reflection")));
		void* SetReturnSlot = SetReturnProperty->ContainerPtrToValuePtr<void>(SetReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(SetReturnSlot, TEXT("TOptional<FStruct> set return slot should be readable")));
		if (SetReturnSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(SetReturnProperty->IsSet(SetReturnSlot), TEXT("TOptional<FStruct> set return should report IsSet=true")));
		const void* InnerValueAddress = SetReturnProperty->GetValuePointerForRead(SetReturnSlot);
		ASSERT_THAT(IsNotNull(InnerValueAddress, TEXT("TOptional<FStruct> set return should expose inner value memory")));
		if (InnerValueAddress == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(64, CountProperty->GetPropertyValue_InContainer(InnerValueAddress),
			TEXT("TOptional<FStruct> set return should preserve int fields")));
		ASSERT_THAT(AreEqual(FString(TEXT("OptionalReturn")), LabelProperty->GetPropertyValue_InContainer(InnerValueAddress),
			TEXT("TOptional<FStruct> set return should preserve string fields")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bSetReturnObserved"), true,
			TEXT("TOptional<FStruct> set return should be observable in AS before returning"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastSetCount"), 64,
			TEXT("TOptional<FStruct> set return should preserve AS-side Count reads"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("LastSetLabel"), FString(TEXT("OptionalReturn")),
			TEXT("TOptional<FStruct> set return should preserve AS-side Label reads"))));

		FFunctionInvoker EmptyReturnInvoker(*TestRunner, Actor, TEXT("ReturnEmptyPayload"));
		ASSERT_THAT(IsTrue(EmptyReturnInvoker.IsValid(), TEXT("ReturnEmptyPayload should be invokable")));
		if (!EmptyReturnInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(EmptyReturnInvoker.Call(), TEXT("ReturnEmptyPayload should execute through reflection")));
		void* EmptyReturnSlot = EmptyReturnProperty->ContainerPtrToValuePtr<void>(EmptyReturnInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(EmptyReturnSlot, TEXT("TOptional<FStruct> empty return slot should be readable")));
		if (EmptyReturnSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsFalse(EmptyReturnProperty->IsSet(EmptyReturnSlot), TEXT("TOptional<FStruct> empty return should report IsSet=false")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("bEmptyReturnObserved"), true,
			TEXT("TOptional<FStruct> empty return should be observable in AS before returning"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT type identity: one generated UScriptStruct across all reflection sites.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructTypeIdentityAcrossReflectionSites)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_TypeIdentityAcrossReflectionSites"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructTypeIdentityAcrossReflectionSites.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FIdentityStruct
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				FName Tag;

				bool opEquals(const FIdentityStruct& Other) const
				{
					return Value == Other.Value && Tag == Other.Tag;
				}

				uint32 Hash() const
				{
					return uint32(Value * 17) + Tag.GetHash();
				}
			}

			UCLASS()
			class ACoverageStructIdentityActor : AActor
			{
				UPROPERTY()
				FIdentityStruct Direct;

				UPROPERTY()
				TArray<FIdentityStruct> ArrayValues;

				UPROPERTY()
				TMap<int, FIdentityStruct> IntToStruct;

				UPROPERTY()
				TMap<FIdentityStruct, int> StructToInt;

				UPROPERTY()
				TSet<FIdentityStruct> StructSet;

				UPROPERTY()
				int LastAcceptValueScore = 0;

				UPROPERTY()
				int LastAcceptConstRefScore = 0;

				UPROPERTY()
				bool ReturnValueMatchesDirect = false;

				UFUNCTION(BlueprintCallable)
				void AcceptValue(FIdentityStruct Item)
				{
					Direct = Item;
					LastAcceptValueScore = Item.Value + (Item.Tag == n"ValueCall" ? 1000 : 0);
				}

				UFUNCTION(BlueprintCallable)
				void AcceptConstRef(const FIdentityStruct&in Item)
				{
					Direct = Item;
					LastAcceptConstRefScore = Item.Value + (Item.Tag == n"ConstRefCall" ? 2000 : 0);
				}

				UFUNCTION(BlueprintCallable)
				FIdentityStruct ReturnValue()
				{
					FIdentityStruct Returned = Direct;
					ReturnValueMatchesDirect = Returned.Value == Direct.Value && Returned.Tag == Direct.Tag;
					return Returned;
				}
			}
			)AS"),
			TEXT("ACoverageStructIdentityActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct type identity actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DirectProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Direct"));
		FArrayProperty* ArrayProperty = FindFProperty<FArrayProperty>(ScriptClass, TEXT("ArrayValues"));
		FMapProperty* IntToStructProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("IntToStruct"));
		FMapProperty* StructToIntProperty = FindFProperty<FMapProperty>(ScriptClass, TEXT("StructToInt"));
		FSetProperty* StructSetProperty = FindFProperty<FSetProperty>(ScriptClass, TEXT("StructSet"));
		ASSERT_THAT(IsNotNull(DirectProperty, TEXT("Direct FIdentityStruct property should reflect")));
		ASSERT_THAT(IsNotNull(ArrayProperty, TEXT("TArray<FIdentityStruct> property should reflect")));
		ASSERT_THAT(IsNotNull(IntToStructProperty, TEXT("TMap<int,FIdentityStruct> property should reflect")));
		ASSERT_THAT(IsNotNull(StructToIntProperty, TEXT("TMap<FIdentityStruct,int> property should reflect")));
		ASSERT_THAT(IsNotNull(StructSetProperty, TEXT("TSet<FIdentityStruct> property should reflect")));
		if (DirectProperty == nullptr || DirectProperty->Struct == nullptr || ArrayProperty == nullptr
			|| IntToStructProperty == nullptr || StructToIntProperty == nullptr || StructSetProperty == nullptr)
		{
			return;
		}

		const UScriptStruct* IdentityStruct = DirectProperty->Struct;
		FStructProperty* ArrayInnerProperty = CastField<FStructProperty>(ArrayProperty->Inner);
		FStructProperty* MapValueProperty = CastField<FStructProperty>(IntToStructProperty->ValueProp);
		FStructProperty* MapKeyProperty = CastField<FStructProperty>(StructToIntProperty->KeyProp);
		FStructProperty* SetElementProperty = CastField<FStructProperty>(StructSetProperty->ElementProp);
		ASSERT_THAT(IsNotNull(ArrayInnerProperty, TEXT("TArray<FIdentityStruct> inner should be FStructProperty")));
		ASSERT_THAT(IsNotNull(MapValueProperty, TEXT("TMap<int,FIdentityStruct> value should be FStructProperty")));
		ASSERT_THAT(IsNotNull(MapKeyProperty, TEXT("TMap<FIdentityStruct,int> key should be FStructProperty")));
		ASSERT_THAT(IsNotNull(SetElementProperty, TEXT("TSet<FIdentityStruct> element should be FStructProperty")));
		if (ArrayInnerProperty == nullptr || MapValueProperty == nullptr || MapKeyProperty == nullptr || SetElementProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(IdentityStruct, ArrayInnerProperty->Struct,
			TEXT("UPROPERTY direct and TArray inner should reference the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(IdentityStruct, MapValueProperty->Struct,
			TEXT("UPROPERTY direct and TMap value should reference the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(IdentityStruct, MapKeyProperty->Struct,
			TEXT("UPROPERTY direct and TMap key should reference the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(IdentityStruct, SetElementProperty->Struct,
			TEXT("UPROPERTY direct and TSet element should reference the same generated UScriptStruct")));

		UFunction* AcceptValueFunction = ScriptClass->FindFunctionByName(TEXT("AcceptValue"));
		UFunction* AcceptConstRefFunction = ScriptClass->FindFunctionByName(TEXT("AcceptConstRef"));
		UFunction* ReturnValueFunction = ScriptClass->FindFunctionByName(TEXT("ReturnValue"));
		ASSERT_THAT(IsNotNull(AcceptValueFunction, TEXT("AcceptValue should reflect as UFunction")));
		ASSERT_THAT(IsNotNull(AcceptConstRefFunction, TEXT("AcceptConstRef should reflect as UFunction")));
		ASSERT_THAT(IsNotNull(ReturnValueFunction, TEXT("ReturnValue should reflect as UFunction")));
		if (AcceptValueFunction == nullptr || AcceptConstRefFunction == nullptr || ReturnValueFunction == nullptr)
		{
			return;
		}

		FStructProperty* ValueParameter = FindFProperty<FStructProperty>(AcceptValueFunction, TEXT("Item"));
		FStructProperty* ConstRefParameter = FindFProperty<FStructProperty>(AcceptConstRefFunction, TEXT("Item"));
		FStructProperty* ReturnProperty = CastField<FStructProperty>(ReturnValueFunction->GetReturnProperty());
		ASSERT_THAT(IsNotNull(ValueParameter, TEXT("Value UFUNCTION parameter should be FStructProperty")));
		ASSERT_THAT(IsNotNull(ConstRefParameter, TEXT("const ref UFUNCTION parameter should be FStructProperty")));
		ASSERT_THAT(IsNotNull(ReturnProperty, TEXT("UFUNCTION return should be FStructProperty")));
		if (ValueParameter == nullptr || ConstRefParameter == nullptr || ReturnProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(IdentityStruct, ValueParameter->Struct,
			TEXT("UFUNCTION value parameter should reference the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(IdentityStruct, ConstRefParameter->Struct,
			TEXT("UFUNCTION const ref parameter should reference the same generated UScriptStruct")));
		ASSERT_THAT(AreEqual(IdentityStruct, ReturnProperty->Struct,
			TEXT("UFUNCTION return should reference the same generated UScriptStruct")));

		FIntProperty* ValueProperty = FindFProperty<FIntProperty>(DirectProperty->Struct, TEXT("Value"));
		FNameProperty* TagProperty = FindFProperty<FNameProperty>(DirectProperty->Struct, TEXT("Tag"));
		ASSERT_THAT(IsNotNull(ValueProperty, TEXT("Identity USTRUCT should expose Value for reflected invocation")));
		ASSERT_THAT(IsNotNull(TagProperty, TEXT("Identity USTRUCT should expose Tag for reflected invocation")));
		if (ValueProperty == nullptr || TagProperty == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct type identity actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}

		FFunctionInvoker AcceptValueInvoker(*TestRunner, Actor, TEXT("AcceptValue"));
		ASSERT_THAT(IsTrue(AcceptValueInvoker.IsValid(), TEXT("AcceptValue should be invokable")));
		if (!AcceptValueInvoker.IsValid())
		{
			return;
		}
		FProperty* ParamProperty = nullptr;
		void* ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(AcceptValueInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("AcceptValue should expose an AS USTRUCT parameter slot")));
		FStructProperty* RuntimeValueParameter = CastField<FStructProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeValueParameter, TEXT("AcceptValue runtime slot should be FStructProperty")));
		ASSERT_THAT(IsNotNull(ParamSlot, TEXT("AcceptValue runtime slot should expose writable memory")));
		if (RuntimeValueParameter == nullptr || ParamSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(IdentityStruct, RuntimeValueParameter->Struct,
			TEXT("AcceptValue runtime slot should use the same generated UScriptStruct")));
		SetStructItemFields(*ValueProperty, *TagProperty, ParamSlot, 91, FName(TEXT("ValueCall")));
		ASSERT_THAT(IsTrue(AcceptValueInvoker.Call(), TEXT("AcceptValue should execute through reflected caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Direct.Value"), 91,
			TEXT("AcceptValue should copy reflected caller-buffer fields into script state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("Direct.Tag"), FName(TEXT("ValueCall")),
			TEXT("AcceptValue should copy reflected caller-buffer FName fields into script state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastAcceptValueScore"), 1091,
			TEXT("AcceptValue should execute AS logic after receiving the shared struct type"))));

		FFunctionInvoker AcceptConstRefInvoker(*TestRunner, Actor, TEXT("AcceptConstRef"));
		ASSERT_THAT(IsTrue(AcceptConstRefInvoker.IsValid(), TEXT("AcceptConstRef should be invokable")));
		if (!AcceptConstRefInvoker.IsValid())
		{
			return;
		}
		ParamProperty = nullptr;
		ParamSlot = nullptr;
		ASSERT_THAT(IsTrue(AcceptConstRefInvoker.AddParamSlot(ParamProperty, ParamSlot),
			TEXT("AcceptConstRef should expose an AS USTRUCT parameter slot")));
		FStructProperty* RuntimeConstRefParameter = CastField<FStructProperty>(ParamProperty);
		ASSERT_THAT(IsNotNull(RuntimeConstRefParameter, TEXT("AcceptConstRef runtime slot should be FStructProperty")));
		ASSERT_THAT(IsNotNull(ParamSlot, TEXT("AcceptConstRef runtime slot should expose writable memory")));
		if (RuntimeConstRefParameter == nullptr || ParamSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(IdentityStruct, RuntimeConstRefParameter->Struct,
			TEXT("AcceptConstRef runtime slot should use the same generated UScriptStruct")));
		SetStructItemFields(*ValueProperty, *TagProperty, ParamSlot, 92, FName(TEXT("ConstRefCall")));
		ASSERT_THAT(IsTrue(AcceptConstRefInvoker.Call(), TEXT("AcceptConstRef should execute through reflected caller buffer")));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("Direct.Value"), 92,
			TEXT("AcceptConstRef should copy reflected caller-buffer fields into script state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FNameProperty, FName>(*TestRunner, Actor, TEXT("Direct.Tag"), FName(TEXT("ConstRefCall")),
			TEXT("AcceptConstRef should copy reflected caller-buffer FName fields into script state"))));
		ASSERT_THAT(IsTrue(VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("LastAcceptConstRefScore"), 2092,
			TEXT("AcceptConstRef should execute AS logic after receiving the shared struct type"))));

		FFunctionInvoker ReturnValueInvoker(*TestRunner, Actor, TEXT("ReturnValue"));
		ASSERT_THAT(IsTrue(ReturnValueInvoker.IsValid(), TEXT("ReturnValue should be invokable")));
		if (!ReturnValueInvoker.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ReturnValueInvoker.Call(), TEXT("ReturnValue should execute through reflected caller buffer")));
		void* ReturnSlot = ReturnProperty->ContainerPtrToValuePtr<void>(ReturnValueInvoker.GetParamsMemory());
		ASSERT_THAT(IsNotNull(ReturnSlot, TEXT("ReturnValue slot should expose readable struct memory")));
		if (ReturnSlot == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(92, ValueProperty->GetPropertyValue_InContainer(ReturnSlot),
			TEXT("ReturnValue should preserve the shared struct int field")));
		ASSERT_THAT(AreEqual(FName(TEXT("ConstRefCall")), TagProperty->GetPropertyValue_InContainer(ReturnSlot),
			TEXT("ReturnValue should preserve the shared struct FName field")));
		ASSERT_THAT(IsTrue(VerifyByPath<FBoolProperty, bool>(*TestRunner, Actor, TEXT("ReturnValueMatchesDirect"), true,
			TEXT("ReturnValue should execute script-side identity check before returning"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT unsupported combinations: nested containers and invalid hash keys.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructUnsupportedCombinationBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalStructParameterUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalParameterStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalParameterActor : AActor
			{
				UFUNCTION(BlueprintCallable)
				void AcceptOptional(TOptional<FOptionalParameterStruct> Payload)
				{
				}
			}
			)AS"),
			TEXT("TOptional<FStruct> UFUNCTION parameter should remain an explicit unsupported boundary"),
			TEXT("Unknown or invalid parameter type for parameter Payload"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalStructConstRefParameterUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalConstRefParameterStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalConstRefParameterActor : AActor
			{
				UFUNCTION(BlueprintCallable)
				void AcceptOptionalConstRef(const TOptional<FOptionalConstRefParameterStruct>&in Payload)
				{
				}
			}
			)AS"),
			TEXT("TOptional<FStruct> const-ref UFUNCTION parameter should remain unsupported"),
			TEXT("Unknown or invalid parameter type for parameter Payload"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalStructOutParameterUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalOutParameterStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalOutParameterActor : AActor
			{
				UFUNCTION(BlueprintCallable)
				void FillOptional(TOptional<FOptionalOutParameterStruct>&out Payload)
				{
				}
			}
			)AS"),
			TEXT("TOptional<FStruct> out UFUNCTION parameter should remain unsupported"),
			TEXT("Unknown or invalid parameter type for parameter Payload"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalStructInoutParameterUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalInoutParameterStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalInoutParameterActor : AActor
			{
				UFUNCTION(BlueprintCallable)
				void MutateOptional(TOptional<FOptionalInoutParameterStruct>&inout Payload)
				{
				}
			}
			)AS"),
			TEXT("TOptional<FStruct> inout UFUNCTION parameter should remain unsupported"),
			TEXT("Unknown or invalid parameter type for parameter Payload"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalStructDelegateParameterUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalDelegateParameterStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			delegate void FOptionalStructSignal(TOptional<FOptionalDelegateParameterStruct> Payload);

			UCLASS()
			class ACoverageStructOptionalDelegateParameterActor : AActor
			{
				UPROPERTY()
				FOptionalStructSignal Signal;
			}
			)AS"),
			TEXT("TOptional<FStruct> delegate parameter should remain unsupported"),
			TEXT("Unknown or invalid parameter type for parameter Payload to delegate FOptionalStructSignal"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalStructEventParameterUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalEventParameterStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			event void FOptionalStructEvent(TOptional<FOptionalEventParameterStruct> Payload);

			UCLASS()
			class ACoverageStructOptionalEventParameterActor : AActor
			{
				UPROPERTY()
				FOptionalStructEvent Signal;
			}
			)AS"),
			TEXT("TOptional<FStruct> multicast event parameter should remain unsupported"),
			TEXT("Unknown or invalid parameter type for parameter Payload to delegate FOptionalStructEvent"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_ArrayOfOptionalStructsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalArrayElementStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructArrayOfOptionalActor : AActor
			{
				UPROPERTY()
				TArray<TOptional<FOptionalArrayElementStruct>> Values;
			}
			)AS"),
			TEXT("TArray<TOptional<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_MapValueOptionalStructUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalMapValueStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructMapValueOptionalActor : AActor
			{
				UPROPERTY()
				TMap<int, TOptional<FOptionalMapValueStruct>> Values;
			}
			)AS"),
			TEXT("TMap<int,TOptional<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_MapKeyOptionalStructUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalMapKeyStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructMapKeyOptionalActor : AActor
			{
				UPROPERTY()
				TMap<TOptional<FOptionalMapKeyStruct>, int> Values;
			}
			)AS"),
			TEXT("TMap<TOptional<FStruct>,int> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_SetOfOptionalStructsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalSetElementStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructSetOfOptionalActor : AActor
			{
				UPROPERTY()
				TSet<TOptional<FOptionalSetElementStruct>> Values;
			}
			)AS"),
			TEXT("TSet<TOptional<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalArrayOfStructsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalArrayPayloadStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalArrayActor : AActor
			{
				UPROPERTY()
				TOptional<TArray<FOptionalArrayPayloadStruct>> Values;
			}
			)AS"),
			TEXT("TOptional<TArray<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalMapOfStructsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalMapPayloadStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalMapActor : AActor
			{
				UPROPERTY()
				TOptional<TMap<int, FOptionalMapPayloadStruct>> Values;
			}
			)AS"),
			TEXT("TOptional<TMap<int,FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_OptionalSetOfStructsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOptionalSetPayloadStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructOptionalSetActor : AActor
			{
				UPROPERTY()
				TOptional<TSet<FOptionalSetPayloadStruct>> Values;
			}
			)AS"),
			TEXT("TOptional<TSet<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_ArrayOfStructArraysUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedContainerStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructArrayOfArraysActor : AActor
			{
				UPROPERTY()
				TArray<TArray<FNestedContainerStruct>> Matrix;
			}
			)AS"),
			TEXT("TArray<TArray<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_MapOfStructArraysUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedMapStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructMapOfArraysActor : AActor
			{
				UPROPERTY()
				TMap<int, TArray<FNestedMapStruct>> Groups;
			}
			)AS"),
			TEXT("TMap<int,TArray<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_ArrayOfStructSetsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedSetStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructArrayOfSetsActor : AActor
			{
				UPROPERTY()
				TArray<TSet<FNestedSetStruct>> Sets;
			}
			)AS"),
			TEXT("TArray<TSet<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_ArrayOfStructMapsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedArrayMapStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructArrayOfMapsActor : AActor
			{
				UPROPERTY()
				TArray<TMap<int, FNestedArrayMapStruct>> Maps;
			}
			)AS"),
			TEXT("TArray<TMap<int,FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_MapOfStructSetsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedMapSetStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructMapOfSetsActor : AActor
			{
				UPROPERTY()
				TMap<int, TSet<FNestedMapSetStruct>> Groups;
			}
			)AS"),
			TEXT("TMap<int,TSet<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_MapOfStructMapsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedMapMapStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructMapOfMapsActor : AActor
			{
				UPROPERTY()
				TMap<int, TMap<int, FNestedMapMapStruct>> Groups;
			}
			)AS"),
			TEXT("TMap<int,TMap<int,FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_SetOfStructArraysUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedSetArrayStruct
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructSetOfArraysActor : AActor
			{
				UPROPERTY()
				TSet<TArray<FNestedSetArrayStruct>> Groups;
			}
			)AS"),
			TEXT("TSet<TArray<FStruct>> should remain an explicit unsupported boundary"),
			TEXT("Containers cannot be nested in other containers"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_UnhashableMapKeyUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUnhashableStructKey
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructUnhashableMapKeyActor : AActor
			{
				UPROPERTY()
				TMap<FUnhashableStructKey, int> Values;
			}
			)AS"),
			TEXT("TMap<FStruct,int> without Hash/opEquals should remain an explicit unsupported boundary"),
			TEXT("Key type does not have a hash function defined"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_UnhashableSetElementUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FUnhashableStructElement
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class ACoverageStructUnhashableSetActor : AActor
			{
				UPROPERTY()
				TSet<FUnhashableStructElement> Values;
			}
			)AS"),
			TEXT("TSet<FStruct> without Hash/opEquals should remain an explicit unsupported boundary"),
			TEXT("Key type does not have a hash function defined"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_MapKeyOnlyEqualsUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FOnlyEqualsStructKey
			{
				UPROPERTY()
				int Value = 0;

				bool opEquals(const FOnlyEqualsStructKey& Other) const
				{
					return Value == Other.Value;
				}
			}

			UCLASS()
			class ACoverageStructOnlyEqualsMapKeyActor : AActor
			{
				UPROPERTY()
				TMap<FOnlyEqualsStructKey, int> Values;
			}
			)AS"),
			TEXT("TMap<FStruct,int> with opEquals but no Hash should remain unsupported"),
			TEXT("Key type does not have a hash function defined"))));

		ASSERT_THAT(IsTrue(ExpectCompileFailureWithDiagnostic(
			*TestRunner,
			Engine,
			TEXT("ASCoverageUStruct_SetElementBadHashSignatureUnsupported"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FBadHashStructElement
			{
				UPROPERTY()
				int Value = 0;

				bool opEquals(const FBadHashStructElement& Other) const
				{
					return Value == Other.Value;
				}

				int Hash() const
				{
					return Value;
				}
			}

			UCLASS()
			class ACoverageStructBadHashSetActor : AActor
			{
				UPROPERTY()
				TSet<FBadHashStructElement> Values;
			}
			)AS"),
			TEXT("TSet<FStruct> with non-uint32 Hash should remain unsupported"),
			TEXT("Key type does not have a hash function defined"))));
	}

	// -------------------------------------------------------------------------
	// USTRUCT nested: struct within struct
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructNested)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_Nested"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructNested.as"),
			ASTEST_AS(R"AS(
			USTRUCT()
			struct FInnerStruct
			{
				UPROPERTY()
				int InnerValue = 0;

				UPROPERTY()
				FString InnerName;
			}

			USTRUCT()
			struct FMiddleStruct
			{
				UPROPERTY()
				int MiddleValue = 0;

				UPROPERTY()
				FInnerStruct InnerData;

				UPROPERTY()
				TArray<FInnerStruct> InnerArray;
			}

			USTRUCT()
			struct FOuterStruct
			{
				UPROPERTY()
				int OuterValue = 0;

				UPROPERTY()
				FMiddleStruct MiddleData;

				UPROPERTY()
				FInnerStruct DirectInner;

				UPROPERTY()
				TArray<FMiddleStruct> MiddleArray;
			}

			UCLASS()
			class ACoverageStructNestedActor : AActor
			{
				UPROPERTY()
				FOuterStruct NestedData;

				UFUNCTION(BlueprintOverride)
				void BeginPlay()
				{
					// Set outer level
					NestedData.OuterValue = 100;

					// Set middle level
					NestedData.MiddleData.MiddleValue = 200;

					// Set inner level (through middle)
					NestedData.MiddleData.InnerData.InnerValue = 300;
					NestedData.MiddleData.InnerData.InnerName = "DeepInner";

					// Set direct inner
					NestedData.DirectInner.InnerValue = 400;
					NestedData.DirectInner.InnerName = "DirectInner";

					// Set array of inner structs in middle
					FInnerStruct Inner1;
					Inner1.InnerValue = 301;
					Inner1.InnerName = "Inner1";
					NestedData.MiddleData.InnerArray.Add(Inner1);

					FInnerStruct Inner2;
					Inner2.InnerValue = 302;
					Inner2.InnerName = "Inner2";
					NestedData.MiddleData.InnerArray.Add(Inner2);

					// Set array of middle structs
					FMiddleStruct Middle1;
					Middle1.MiddleValue = 201;
					Middle1.InnerData.InnerValue = 311;
					Middle1.InnerData.InnerName = "MiddleArray1Inner";
					NestedData.MiddleArray.Add(Middle1);

					FMiddleStruct Middle2;
					Middle2.MiddleValue = 202;
					Middle2.InnerData.InnerValue = 312;
					Middle2.InnerData.InnerName = "MiddleArray2Inner";
					NestedData.MiddleArray.Add(Middle2);
				}
			}
			)AS"),
			TEXT("ACoverageStructNestedActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct nested actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ScriptClass);
		ASSERT_THAT(IsNotNull(Actor, TEXT("UStruct nested actor should spawn")));
		if (Actor == nullptr)
		{
			return;
		}
		BeginPlayActor(Engine, *Actor);

		// Verify outer level
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.OuterValue"), 100, TEXT("Outer level value"));

		// Verify middle level
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.MiddleValue"), 200, TEXT("Middle level value"));

		// Verify inner level (3 levels deep)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerData.InnerValue"), 300, TEXT("Inner level value (3 deep)"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerData.InnerName"), FString(TEXT("DeepInner")), TEXT("Inner level name (3 deep)"));

		// Verify direct inner (2 levels deep)
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.DirectInner.InnerValue"), 400, TEXT("Direct inner value"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.DirectInner.InnerName"), FString(TEXT("DirectInner")), TEXT("Direct inner name"));

		// Verify array of inner structs in middle
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[0].InnerValue"), 301, TEXT("InnerArray[0].InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[0].InnerName"), FString(TEXT("Inner1")), TEXT("InnerArray[0].InnerName"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[1].InnerValue"), 302, TEXT("InnerArray[1].InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleData.InnerArray[1].InnerName"), FString(TEXT("Inner2")), TEXT("InnerArray[1].InnerName"));

		// Verify array of middle structs with nested inner structs
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[0].MiddleValue"), 201, TEXT("MiddleArray[0].MiddleValue"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[0].InnerData.InnerValue"), 311, TEXT("MiddleArray[0].InnerData.InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[0].InnerData.InnerName"), FString(TEXT("MiddleArray1Inner")), TEXT("MiddleArray[0].InnerData.InnerName"));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[1].MiddleValue"), 202, TEXT("MiddleArray[1].MiddleValue"));
		VerifyByPath<FIntProperty, int32>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[1].InnerData.InnerValue"), 312, TEXT("MiddleArray[1].InnerData.InnerValue"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Actor, TEXT("NestedData.MiddleArray[1].InnerData.InnerName"), FString(TEXT("MiddleArray2Inner")), TEXT("MiddleArray[1].InnerData.InnerName"));
	}

	// -------------------------------------------------------------------------
	// USTRUCT nested member defaults: reflected CDO defaults without runtime mutation.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructNestedDefaultsReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_NestedDefaultsReflection"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructNestedDefaultsReflection.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FNestedDefaultLeaf
			{
				UPROPERTY(EditAnywhere)
				int Count = 17;

				UPROPERTY(BlueprintReadOnly)
				FString Label = "LeafDefault";
			}

			USTRUCT(BlueprintType)
			struct FNestedDefaultBranch
			{
				UPROPERTY(EditAnywhere)
				int Weight = 29;

				UPROPERTY(EditAnywhere)
				FNestedDefaultLeaf Leaf;
			}

			UCLASS()
			class ACoverageStructNestedDefaultsActor : AActor
			{
				UPROPERTY(EditAnywhere)
				FNestedDefaultBranch Defaults;
			}
			)AS"),
			TEXT("ACoverageStructNestedDefaultsActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct nested-defaults actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* ClassDefaultObject = ScriptClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ClassDefaultObject, TEXT("UStruct nested-defaults actor should expose a CDO")));
		if (ClassDefaultObject == nullptr)
		{
			return;
		}

		FStructProperty* DefaultsProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Defaults"));
		ASSERT_THAT(IsNotNull(DefaultsProperty, TEXT("Defaults should be reflected as an outer struct property")));
		if (DefaultsProperty == nullptr)
		{
			return;
		}

		UScriptStruct* BranchStruct = DefaultsProperty->Struct;
		ASSERT_THAT(IsNotNull(BranchStruct, TEXT("Defaults should have a generated branch UScriptStruct")));
		if (BranchStruct == nullptr)
		{
			return;
		}

		FIntProperty* WeightProperty = FindFProperty<FIntProperty>(BranchStruct, TEXT("Weight"));
		FStructProperty* LeafProperty = FindFProperty<FStructProperty>(BranchStruct, TEXT("Leaf"));
		ASSERT_THAT(IsNotNull(WeightProperty, TEXT("Nested branch should reflect Weight")));
		if (WeightProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(LeafProperty, TEXT("Nested branch should reflect Leaf")));
		if (LeafProperty == nullptr)
		{
			return;
		}

		UScriptStruct* LeafStruct = LeafProperty->Struct;
		ASSERT_THAT(IsNotNull(LeafStruct, TEXT("Leaf should have a generated leaf UScriptStruct")));
		if (LeafStruct == nullptr)
		{
			return;
		}

		FIntProperty* CountProperty = FindFProperty<FIntProperty>(LeafStruct, TEXT("Count"));
		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(LeafStruct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(CountProperty, TEXT("Nested leaf should reflect Count")));
		if (CountProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("Nested leaf should reflect Label")));
		if (LabelProperty == nullptr)
		{
			return;
		}

		void* DefaultsAddress = DefaultsProperty->ContainerPtrToValuePtr<void>(ClassDefaultObject);
		ASSERT_THAT(IsNotNull(DefaultsAddress, TEXT("CDO should store Defaults struct memory")));
		if (DefaultsAddress == nullptr)
		{
			return;
		}

		void* LeafAddress = LeafProperty->ContainerPtrToValuePtr<void>(DefaultsAddress);
		ASSERT_THAT(IsNotNull(LeafAddress, TEXT("CDO should store nested Leaf struct memory")));
		if (LeafAddress == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(29, WeightProperty->GetPropertyValue_InContainer(DefaultsAddress),
			TEXT("Nested branch int default should propagate to the CDO")));
		ASSERT_THAT(AreEqual(17, CountProperty->GetPropertyValue_InContainer(LeafAddress),
			TEXT("Nested leaf int default should propagate to the CDO")));
		ASSERT_THAT(AreEqual(FString(TEXT("LeafDefault")), LabelProperty->GetPropertyValue_InContainer(LeafAddress),
			TEXT("Nested leaf FString default should propagate to the CDO")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT alias/deprecation metadata: member ScriptName and deprecation keys.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructMetadataAliasAndDeprecationMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_MetadataAliasAndDeprecation"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructMetadataAliasAndDeprecation.as"),
			ASTEST_AS(R"AS(
			USTRUCT(BlueprintType)
			struct FStructMetadataAliasCarrier
			{
				UPROPERTY(EditAnywhere, meta=(ScriptName="AliasCount"))
				int NativeCount = 4;

				UPROPERTY(EditAnywhere, meta=(DeprecatedProperty, DeprecationMessage="Use NativeCount instead"))
				int DeprecatedCount = 9;

				UPROPERTY(EditAnywhere, meta=(ScriptName="AliasLabel", DeprecatedProperty, DeprecationMessage="Use AliasLabel instead"))
				FString NativeLabel = "AliasDefault";
			}

			UCLASS()
			class ACoverageStructMetadataAliasActor : AActor
			{
				UPROPERTY()
				FStructMetadataAliasCarrier Data;
			}
			)AS"),
			TEXT("ACoverageStructMetadataAliasActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct metadata alias/deprecation actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data property should expose the alias/deprecation metadata struct")));
		if (DataProperty == nullptr || DataProperty->Struct == nullptr)
		{
			return;
		}

		UScriptStruct* MetadataStruct = DataProperty->Struct;
		FIntProperty* NativeCountProperty = FindFProperty<FIntProperty>(MetadataStruct, TEXT("NativeCount"));
		FIntProperty* DeprecatedCountProperty = FindFProperty<FIntProperty>(MetadataStruct, TEXT("DeprecatedCount"));
		FStrProperty* NativeLabelProperty = FindFProperty<FStrProperty>(MetadataStruct, TEXT("NativeLabel"));
		ASSERT_THAT(IsNotNull(NativeCountProperty, TEXT("ScriptName metadata member should keep its reflected native property name")));
		ASSERT_THAT(IsNotNull(DeprecatedCountProperty, TEXT("DeprecatedProperty metadata member should keep its reflected native property name")));
		ASSERT_THAT(IsNotNull(NativeLabelProperty, TEXT("Combined alias/deprecated member should keep its reflected native property name")));
		if (NativeCountProperty == nullptr || DeprecatedCountProperty == nullptr || NativeLabelProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("AliasCount")), NativeCountProperty->GetMetaData(TEXT("ScriptName")),
			TEXT("USTRUCT int member ScriptName metadata should round-trip")));
		ASSERT_THAT(IsFalse(NativeCountProperty->HasMetaData(TEXT("DeprecatedProperty")),
			TEXT("USTRUCT ScriptName-only member should not gain DeprecatedProperty metadata")));
		ASSERT_THAT(IsTrue(DeprecatedCountProperty->HasMetaData(TEXT("DeprecatedProperty")),
			TEXT("USTRUCT int member DeprecatedProperty metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use NativeCount instead")), DeprecatedCountProperty->GetMetaData(TEXT("DeprecationMessage")),
			TEXT("USTRUCT int member DeprecationMessage metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("AliasLabel")), NativeLabelProperty->GetMetaData(TEXT("ScriptName")),
			TEXT("USTRUCT FString member ScriptName metadata should round-trip")));
		ASSERT_THAT(IsTrue(NativeLabelProperty->HasMetaData(TEXT("DeprecatedProperty")),
			TEXT("USTRUCT member should support ScriptName and DeprecatedProperty metadata together")));
		ASSERT_THAT(AreEqual(FString(TEXT("Use AliasLabel instead")), NativeLabelProperty->GetMetaData(TEXT("DeprecationMessage")),
			TEXT("USTRUCT combined member DeprecationMessage metadata should round-trip")));

		UObject* DefaultObject = ScriptClass->GetDefaultObject();
		ASSERT_THAT(IsNotNull(DefaultObject, TEXT("Alias/deprecation metadata actor CDO should exist")));
		if (DefaultObject == nullptr)
		{
			return;
		}

		void* DataAddress = DataProperty->ContainerPtrToValuePtr<void>(DefaultObject);
		ASSERT_THAT(IsNotNull(DataAddress, TEXT("Alias/deprecation metadata CDO should store nested struct memory")));
		if (DataAddress == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(4, NativeCountProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("ScriptName metadata should not disturb USTRUCT int member defaults")));
		ASSERT_THAT(AreEqual(9, DeprecatedCountProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("DeprecatedProperty metadata should not disturb USTRUCT int member defaults")));
		ASSERT_THAT(AreEqual(FString(TEXT("AliasDefault")), NativeLabelProperty->GetPropertyValue_InContainer(DataAddress),
			TEXT("Combined alias/deprecation metadata should not disturb USTRUCT FString member defaults")));
	}

	// -------------------------------------------------------------------------
	// USTRUCT advanced metadata: struct/member display, tooltip, custom, and numeric UI/unit metadata.
	// -------------------------------------------------------------------------
	TEST_METHOD(UStructAdvancedMetadata)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		static const FName ModuleName(TEXT("ASCoverageUStruct_AdvancedMetadata"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASCoverageUStructAdvancedMetadata.as"),
			ASTEST_AS(R"AS(
			USTRUCT(meta=(DisplayName="Coverage Metadata Struct", ToolTip="Struct tooltip text", ShortToolTip="Struct short tooltip", CoverageStructKey="StructValue"))
			struct FStructMetadataCarrier
			{
				UPROPERTY(EditAnywhere, Category="Coverage|StructMeta", meta=(DisplayName="Count Value", ToolTip="Count tooltip text", ShortToolTip="Count short tooltip", CoveragePropertyKey="CountValue", ClampMin="1", ClampMax="9"))
				int Count = 3;

				UPROPERTY(EditAnywhere, Category="Coverage|StructMeta", meta=(DisplayName="Ratio Value", UIMin="0.0", UIMax="1.0", Units="Percent"))
				float Ratio = 0.5;

				UPROPERTY(EditAnywhere, Category="Coverage|StructMeta", meta=(ClampMin="-180.0", ClampMax="180.0", UIMin="-90.0", UIMax="90.0", Units="Degrees"))
				float Angle = 15.0;

				UPROPERTY(BlueprintReadOnly, meta=(DisplayName="Label Value", ToolTip="Label tooltip text"))
				FString Label = "Initial";
			}

			UCLASS()
			class ACoverageStructMetadataActor : AActor
			{
				UPROPERTY()
				FStructMetadataCarrier Data;
			}
			)AS"),
			TEXT("ACoverageStructMetadataActor"));
		ASSERT_THAT(IsNotNull(ScriptClass, TEXT("UStruct advanced metadata actor should compile")));
		if (ScriptClass == nullptr)
		{
			return;
		}

		FStructProperty* DataProperty = FindFProperty<FStructProperty>(ScriptClass, TEXT("Data"));
		ASSERT_THAT(IsNotNull(DataProperty, TEXT("Data property should expose the generated metadata struct")));
		if (DataProperty == nullptr)
		{
			return;
		}

		UScriptStruct* MetadataStruct = DataProperty->Struct;
		ASSERT_THAT(IsNotNull(MetadataStruct, TEXT("Generated metadata struct should have a backing UScriptStruct")));
		if (MetadataStruct == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Coverage Metadata Struct")), MetadataStruct->GetMetaData(TEXT("DisplayName")),
			TEXT("USTRUCT DisplayName metadata should round-trip to UScriptStruct")));
		ASSERT_THAT(AreEqual(FString(TEXT("Struct tooltip text")), MetadataStruct->GetMetaData(TEXT("ToolTip")),
			TEXT("USTRUCT ToolTip metadata should round-trip to UScriptStruct")));
		ASSERT_THAT(AreEqual(FString(TEXT("Struct short tooltip")), MetadataStruct->GetMetaData(TEXT("ShortToolTip")),
			TEXT("USTRUCT ShortToolTip metadata should round-trip to UScriptStruct")));
		ASSERT_THAT(AreEqual(FString(TEXT("StructValue")), MetadataStruct->GetMetaData(TEXT("CoverageStructKey")),
			TEXT("USTRUCT custom metadata should round-trip to UScriptStruct")));

		FIntProperty* CountProperty = FindFProperty<FIntProperty>(MetadataStruct, TEXT("Count"));
		ASSERT_THAT(IsNotNull(CountProperty, TEXT("Count member should exist on generated UScriptStruct")));
		if (CountProperty == nullptr)
		{
			return;
		}

		FStrProperty* LabelProperty = FindFProperty<FStrProperty>(MetadataStruct, TEXT("Label"));
		ASSERT_THAT(IsNotNull(LabelProperty, TEXT("Label member should exist on generated UScriptStruct")));
		if (LabelProperty == nullptr)
		{
			return;
		}

		FScriptFloatProperty* RatioProperty = FindFProperty<FScriptFloatProperty>(MetadataStruct, TEXT("Ratio"));
		ASSERT_THAT(IsNotNull(RatioProperty, TEXT("Ratio member should exist on generated UScriptStruct")));
		if (RatioProperty == nullptr)
		{
			return;
		}

		FScriptFloatProperty* AngleProperty = FindFProperty<FScriptFloatProperty>(MetadataStruct, TEXT("Angle"));
		ASSERT_THAT(IsNotNull(AngleProperty, TEXT("Angle member should exist on generated UScriptStruct")));
		if (AngleProperty == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|StructMeta")), CountProperty->GetMetaData(TEXT("Category")),
			TEXT("USTRUCT member Category metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Count Value")), CountProperty->GetMetaData(TEXT("DisplayName")),
			TEXT("USTRUCT int member DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Count tooltip text")), CountProperty->GetMetaData(TEXT("ToolTip")),
			TEXT("USTRUCT int member ToolTip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Count short tooltip")), CountProperty->GetMetaData(TEXT("ShortToolTip")),
			TEXT("USTRUCT int member ShortToolTip metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("CountValue")), CountProperty->GetMetaData(TEXT("CoveragePropertyKey")),
			TEXT("USTRUCT int member custom metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("1")), CountProperty->GetMetaData(TEXT("ClampMin")),
			TEXT("USTRUCT int member ClampMin metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("9")), CountProperty->GetMetaData(TEXT("ClampMax")),
			TEXT("USTRUCT int member ClampMax metadata should round-trip")));

		ASSERT_THAT(AreEqual(FString(TEXT("Coverage|StructMeta")), RatioProperty->GetMetaData(TEXT("Category")),
			TEXT("USTRUCT float member Category metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Ratio Value")), RatioProperty->GetMetaData(TEXT("DisplayName")),
			TEXT("USTRUCT float member DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("0.0")), RatioProperty->GetMetaData(TEXT("UIMin")),
			TEXT("USTRUCT float member UIMin metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("1.0")), RatioProperty->GetMetaData(TEXT("UIMax")),
			TEXT("USTRUCT float member UIMax metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Percent")), RatioProperty->GetMetaData(TEXT("Units")),
			TEXT("USTRUCT float member Units metadata should round-trip")));

		ASSERT_THAT(AreEqual(FString(TEXT("-180.0")), AngleProperty->GetMetaData(TEXT("ClampMin")),
			TEXT("USTRUCT float member ClampMin metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("180.0")), AngleProperty->GetMetaData(TEXT("ClampMax")),
			TEXT("USTRUCT float member ClampMax metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("-90.0")), AngleProperty->GetMetaData(TEXT("UIMin")),
			TEXT("USTRUCT float member combined UIMin metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("90.0")), AngleProperty->GetMetaData(TEXT("UIMax")),
			TEXT("USTRUCT float member combined UIMax metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Degrees")), AngleProperty->GetMetaData(TEXT("Units")),
			TEXT("USTRUCT float member combined Units metadata should round-trip")));

		ASSERT_THAT(AreEqual(FString(TEXT("Label Value")), LabelProperty->GetMetaData(TEXT("DisplayName")),
			TEXT("USTRUCT FString member DisplayName metadata should round-trip")));
		ASSERT_THAT(AreEqual(FString(TEXT("Label tooltip text")), LabelProperty->GetMetaData(TEXT("ToolTip")),
			TEXT("USTRUCT FString member ToolTip metadata should round-trip")));
	}
};

#endif
