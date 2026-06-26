// ============================================================================
// AngelscriptOptionalBindingsTests.cpp
//
// TOptional binding coverage. Automation prefix:
//   - Angelscript.TestModule.Bindings.Container.Optional
// ============================================================================

#include "CQTest.h"
#include "AngelscriptBinds.h"
#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

static UObject* GetOptionalNullNativeRefForTesting()
{
	return nullptr;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_AngelscriptOptionalNullNativeRefForTesting(
	TEXT("AngelscriptOptionalNullNativeRefForTesting"),
	(int32)FAngelscriptBinds::EOrder::Late + 101,
	[]
	{
		FAngelscriptBinds::BindGlobalFunction(
			"UObject& OptionalNullNativeRefForTesting()",
			FUNC_TRIVIAL(GetOptionalNullNativeRefForTesting));
	});

TEST_CLASS_WITH_FLAGS(FAngelscriptOptionalBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Optional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

	TEST_METHOD(OptionalCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int OptEmpty_IsSet()
			{
				TOptional<int> O;
				return O.IsSet() ? 1 : 0;
			}

			int OptEmpty_GetFallback()
			{
				TOptional<int> O;
				return O.Get(7);
			}

			int OptSet_IsSet()
			{
				TOptional<int> O;
				O.Set(42);
				return O.IsSet() ? 1 : 0;
			}

			int OptSet_GetValue()
			{
				TOptional<int> O;
				O.Set(42);
				return O.GetValue();
			}

			int OptCopy_Equals()
			{
				TOptional<int> O;
				O.Set(42);
				TOptional<int> Copy(O);
				return (Copy == O) ? 1 : 0;
			}

			int OptAssign_GetValue()
			{
				TOptional<int> O;
				O.Set(42);
				TOptional<int> Copy(O);
				Copy = 19;
				return Copy.GetValue();
			}

			int OptReset_IsSet()
			{
				TOptional<int> O;
				O.Set(42);
				TOptional<int> Copy(O);
				Copy.Reset();
				return Copy.IsSet() ? 1 : 0;
			}

			int OptFName_IsSet()
			{
				TOptional<FName> O(FName("Alpha"));
				return O.IsSet() ? 1 : 0;
			}

			int OptFName_GetValue()
			{
				TOptional<FName> O(FName("Alpha"));
				return (O.GetValue() == FName("Alpha")) ? 1 : 0;
			}

			int OptFName_GetWithValue()
			{
				TOptional<FName> O(FName("Alpha"));
				return (O.Get(FName("Fallback")) == FName("Alpha")) ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_Compat"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional compatibility module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptEmpty_IsSet()"), TEXT("Empty TOptional should not be set"), 0 },
			{ TEXT("int OptEmpty_GetFallback()"), TEXT("Empty TOptional Get should return fallback 7"), 7 },
			{ TEXT("int OptSet_IsSet()"), TEXT("TOptional after Set(42) should be set"), 1 },
			{ TEXT("int OptSet_GetValue()"), TEXT("TOptional GetValue should return 42"), 42 },
			{ TEXT("int OptCopy_Equals()"), TEXT("Copy-constructed TOptional should equal original"), 1 },
			{ TEXT("int OptAssign_GetValue()"), TEXT("Assigned TOptional GetValue should be 19"), 19 },
			{ TEXT("int OptReset_IsSet()"), TEXT("Reset TOptional should not be set"), 0 },
			{ TEXT("int OptFName_IsSet()"), TEXT("FName TOptional should be set"), 1 },
			{ TEXT("int OptFName_GetValue()"), TEXT("FName TOptional GetValue should match Alpha"), 1 },
			{ TEXT("int OptFName_GetWithValue()"), TEXT("FName TOptional Get with value returns value not fallback"), 1 },
		};
		ASSERT_THAT(IsTrue(ExpectGlobalInts(*TestRunner, Engine, Module, Cases), TEXT("Optional compatibility cases should pass")));
	}

	TEST_METHOD(OptionalTypeMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int OptBool_True_IsSet()
			{
				TOptional<bool> O(true);
				return O.IsSet() ? 1 : 0;
			}

			int OptBool_True_GetValue()
			{
				TOptional<bool> O(true);
				return O.GetValue() ? 1 : 0;
			}

			int OptBool_False_IsSet()
			{
				TOptional<bool> O(false);
				return O.IsSet() ? 1 : 0;
			}

			int OptBool_False_GetValue()
			{
				TOptional<bool> O(false);
				return O.GetValue() ? 1 : 0;
			}

			int OptDouble_GetValue()
			{
				TOptional<float> O(3.5f);
				return (O.GetValue() > 3.4f && O.GetValue() < 3.6f) ? 1 : 0;
			}

			int OptDouble_GetFallback()
			{
				TOptional<float> O;
				float V = O.Get(7.25f);
				return (V > 7.24f && V < 7.26f) ? 1 : 0;
			}

			int OptString_GetValue()
			{
				TOptional<FString> O(FString("Hello"));
				return (O.GetValue() == "Hello") ? 1 : 0;
			}

			int OptString_FallbackVsValue()
			{
				TOptional<FString> O(FString("Real"));
				return (O.Get("Fallback") == "Real") ? 1 : 0;
			}

			int OptString_EmptyFallback()
			{
				TOptional<FString> O;
				return (O.Get("Fallback") == "Fallback") ? 1 : 0;
			}

			int OptVector_GetValueX()
			{
				TOptional<FVector> O(FVector(1, 2, 3));
				float X = O.GetValue().X;
				return (X > 0.9f && X < 1.1f) ? 1 : 0;
			}

			int OptVector_GetValueY()
			{
				TOptional<FVector> O(FVector(1, 2, 3));
				float Y = O.GetValue().Y;
				return (Y > 1.9f && Y < 2.1f) ? 1 : 0;
			}

			int OptVector_GetValueZ()
			{
				TOptional<FVector> O(FVector(1, 2, 3));
				float Z = O.GetValue().Z;
				return (Z > 2.9f && Z < 3.1f) ? 1 : 0;
			}

			int OptVector_Reset_IsSet()
			{
				TOptional<FVector> O(FVector(1, 2, 3));
				O.Reset();
				return O.IsSet() ? 1 : 0;
			}

			int OptEnum_GetValue()
			{
				TOptional<ETickingGroup> O(ETickingGroup::TG_PrePhysics);
				return (O.GetValue() == ETickingGroup::TG_PrePhysics) ? 1 : 0;
			}

			int OptObject_NullSet_IsSet()
			{
				TOptional<UObject> O(nullptr);
				return O.IsSet() ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_TypeMatrix"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional type matrix module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptBool_True_IsSet()"), TEXT("TOptional<bool>(true) should be set"), 1 },
			{ TEXT("int OptBool_True_GetValue()"), TEXT("TOptional<bool>(true).GetValue should be true"), 1 },
			{ TEXT("int OptBool_False_IsSet()"), TEXT("TOptional<bool>(false) should still be set"), 1 },
			{ TEXT("int OptBool_False_GetValue()"), TEXT("TOptional<bool>(false).GetValue should be false"), 0 },
			{ TEXT("int OptDouble_GetValue()"), TEXT("TOptional<float>(3.5).GetValue should be ~3.5"), 1 },
			{ TEXT("int OptDouble_GetFallback()"), TEXT("Empty TOptional<float>.Get(7.25) should fall back to 7.25"), 1 },
			{ TEXT("int OptString_GetValue()"), TEXT("TOptional<FString>.GetValue should match"), 1 },
			{ TEXT("int OptString_FallbackVsValue()"), TEXT("TOptional<FString>.Get(default) should return real value"), 1 },
			{ TEXT("int OptString_EmptyFallback()"), TEXT("Empty TOptional<FString>.Get(default) should return default"), 1 },
			{ TEXT("int OptVector_GetValueX()"), TEXT("TOptional<FVector>.GetValue.X should be ~1"), 1 },
			{ TEXT("int OptVector_GetValueY()"), TEXT("TOptional<FVector>.GetValue.Y should be ~2"), 1 },
			{ TEXT("int OptVector_GetValueZ()"), TEXT("TOptional<FVector>.GetValue.Z should be ~3"), 1 },
			{ TEXT("int OptVector_Reset_IsSet()"), TEXT("TOptional<FVector>.Reset should clear IsSet"), 0 },
			{ TEXT("int OptEnum_GetValue()"), TEXT("TOptional<ETickingGroup>.GetValue should match TG_PrePhysics"), 1 },
			{ TEXT("int OptObject_NullSet_IsSet()"), TEXT("TOptional<UObject>(nullptr) construction still sets the slot"), 1 },
		};
		ASSERT_THAT(IsTrue(ExpectGlobalInts(*TestRunner, Engine, Module, Cases), TEXT("Optional type matrix cases should pass")));
	}

	TEST_METHOD(OptionalApiCoverage)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int OptApi_EmptyEqualsEmpty()
			{
				TOptional<int> A;
				TOptional<int> B;
				return (A == B) ? 1 : 0;
			}

			int OptApi_SetNotEqualsEmpty()
			{
				TOptional<int> A;
				A.Set(5);
				TOptional<int> B;
				return (A == B) ? 1 : 0;
			}

			int OptApi_SetEqualsSameValue()
			{
				TOptional<int> A;
				A.Set(5);
				TOptional<int> B;
				B.Set(5);
				return (A == B) ? 1 : 0;
			}

			int OptApi_SetNotEqualsDifferentValue()
			{
				TOptional<int> A;
				A.Set(5);
				TOptional<int> B;
				B.Set(6);
				return (A == B) ? 1 : 0;
			}

			int OptApi_AssignFromValue_IsSet()
			{
				TOptional<int> O;
				O = 42;
				return O.IsSet() ? 1 : 0;
			}

			int OptApi_AssignFromValue_GetValue()
			{
				TOptional<int> O;
				O = 42;
				return O.GetValue();
			}

			int OptApi_AssignFromValueOverwrites()
			{
				TOptional<int> O;
				O.Set(1);
				O = 9;
				return O.GetValue();
			}

			int OptApi_AssignOptionalFromSet_IsSet()
			{
				TOptional<int> Src;
				Src.Set(7);
				TOptional<int> Dst;
				Dst = Src;
				return Dst.IsSet() ? 1 : 0;
			}

			int OptApi_AssignOptionalFromSet_GetValue()
			{
				TOptional<int> Src;
				Src.Set(7);
				TOptional<int> Dst;
				Dst = Src;
				return Dst.GetValue();
			}

			int OptApi_AssignOptionalUnsetClears()
			{
				TOptional<int> Src;
				TOptional<int> Dst;
				Dst.Set(99);
				Dst = Src;
				return Dst.IsSet() ? 1 : 0;
			}

			int OptApi_ResetThenSetRoundtrip_IsSet()
			{
				TOptional<int> O;
				O.Set(1);
				O.Reset();
				O.Set(2);
				return O.IsSet() ? 1 : 0;
			}

			int OptApi_ResetThenSetRoundtrip_GetValue()
			{
				TOptional<int> O;
				O.Set(1);
				O.Reset();
				O.Set(2);
				return O.GetValue();
			}

			int OptApi_GetMutableViaRef()
			{
				TOptional<int> O;
				O.Set(10);
				int& Ref = O.GetValue();
				Ref = 20;
				return O.GetValue();
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_ApiCoverage"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional API coverage module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptApi_EmptyEqualsEmpty()"), TEXT("empty == empty should be true"), 1 },
			{ TEXT("int OptApi_SetNotEqualsEmpty()"), TEXT("set vs empty should be unequal"), 0 },
			{ TEXT("int OptApi_SetEqualsSameValue()"), TEXT("set(5) == set(5) should be equal"), 1 },
			{ TEXT("int OptApi_SetNotEqualsDifferentValue()"), TEXT("set(5) == set(6) should be unequal"), 0 },
			{ TEXT("int OptApi_AssignFromValue_IsSet()"), TEXT("opAssign(value) on empty should set the slot"), 1 },
			{ TEXT("int OptApi_AssignFromValue_GetValue()"), TEXT("opAssign(value) should store the assigned value"), 42 },
			{ TEXT("int OptApi_AssignFromValueOverwrites()"), TEXT("opAssign(value) should overwrite previous content"), 9 },
			{ TEXT("int OptApi_AssignOptionalFromSet_IsSet()"), TEXT("opAssign(optional) from set should leave Dst set"), 1 },
			{ TEXT("int OptApi_AssignOptionalFromSet_GetValue()"), TEXT("opAssign(optional) from set should propagate value"), 7 },
			{ TEXT("int OptApi_AssignOptionalUnsetClears()"), TEXT("opAssign(optional) from unset should clear Dst"), 0 },
			{ TEXT("int OptApi_ResetThenSetRoundtrip_IsSet()"), TEXT("Reset+Set should leave optional set"), 1 },
			{ TEXT("int OptApi_ResetThenSetRoundtrip_GetValue()"), TEXT("Reset+Set(2) should store value 2"), 2 },
			{ TEXT("int OptApi_GetMutableViaRef()"), TEXT("non-const GetValue should return mutable reference"), 20 },
		};
		ASSERT_THAT(IsTrue(ExpectGlobalInts(*TestRunner, Engine, Module, Cases), TEXT("Optional API coverage cases should pass")));
	}

	TEST_METHOD(OptionalNullHandleValues)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			class UOptionalNullHandleObject : UObject
			{
			}

			UOptionalNullHandleObject GetNullHandle()
			{
				return nullptr;
			}

			int OptNullHandle_ConstructFromFunction_IsSet()
			{
				TOptional<UOptionalNullHandleObject> O(GetNullHandle());
				return O.IsSet() ? 1 : 0;
			}

			int OptNullHandle_ConstructFromFunction_ValueIsNull()
			{
				TOptional<UOptionalNullHandleObject> O(GetNullHandle());
				return O.GetValue() == nullptr ? 1 : 0;
			}

			int OptNullHandle_SetFromFunction_IsSet()
			{
				TOptional<UOptionalNullHandleObject> O;
				O.Set(GetNullHandle());
				return O.IsSet() ? 1 : 0;
			}

			int OptNullHandle_SetFromFunction_ValueIsNull()
			{
				TOptional<UOptionalNullHandleObject> O;
				O.Set(GetNullHandle());
				return O.GetValue() == nullptr ? 1 : 0;
			}

			int OptNullHandle_AssignFromFunction_IsSet()
			{
				TOptional<UOptionalNullHandleObject> O;
				O = GetNullHandle();
				return O.IsSet() ? 1 : 0;
			}

			int OptNullHandle_AssignFromFunction_ValueIsNull()
			{
				TOptional<UOptionalNullHandleObject> O;
				O = GetNullHandle();
				return O.GetValue() == nullptr ? 1 : 0;
			}

			int OptNullHandle_ConstructFromNativeRef_IsSet()
			{
				TOptional<UObject> O(OptionalNullNativeRefForTesting());
				return O.IsSet() ? 1 : 0;
			}

			int OptNullHandle_ConstructFromNativeRef_ValueIsNull()
			{
				TOptional<UObject> O(OptionalNullNativeRefForTesting());
				return O.GetValue() == nullptr ? 1 : 0;
			}

			int OptNullHandle_SetFromNativeRef_IsSet()
			{
				TOptional<UObject> O;
				O.Set(OptionalNullNativeRefForTesting());
				return O.IsSet() ? 1 : 0;
			}

			int OptNullHandle_SetFromNativeRef_ValueIsNull()
			{
				TOptional<UObject> O;
				O.Set(OptionalNullNativeRefForTesting());
				return O.GetValue() == nullptr ? 1 : 0;
			}

			int OptNullHandle_AssignFromNativeRef_IsSet()
			{
				TOptional<UObject> O;
				O = OptionalNullNativeRefForTesting();
				return O.IsSet() ? 1 : 0;
			}

			int OptNullHandle_AssignFromNativeRef_ValueIsNull()
			{
				TOptional<UObject> O;
				O = OptionalNullNativeRefForTesting();
				return O.GetValue() == nullptr ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_NullHandle"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional null handle module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int OptNullHandle_ConstructFromFunction_IsSet()"), TEXT("TOptional<script object> constructed from returned null handle should be set"), 1 },
			{ TEXT("int OptNullHandle_ConstructFromFunction_ValueIsNull()"), TEXT("TOptional<script object> constructed from returned null handle should store null"), 1 },
			{ TEXT("int OptNullHandle_SetFromFunction_IsSet()"), TEXT("TOptional<script object>.Set(returned null handle) should be set"), 1 },
			{ TEXT("int OptNullHandle_SetFromFunction_ValueIsNull()"), TEXT("TOptional<script object>.Set(returned null handle) should store null"), 1 },
			{ TEXT("int OptNullHandle_AssignFromFunction_IsSet()"), TEXT("TOptional<script object> assigned returned null handle should be set"), 1 },
			{ TEXT("int OptNullHandle_AssignFromFunction_ValueIsNull()"), TEXT("TOptional<script object> assigned returned null handle should store null"), 1 },
			{ TEXT("int OptNullHandle_ConstructFromNativeRef_IsSet()"), TEXT("TOptional<UObject> constructed from null native ref should be set"), 1 },
			{ TEXT("int OptNullHandle_ConstructFromNativeRef_ValueIsNull()"), TEXT("TOptional<UObject> constructed from null native ref should store null"), 1 },
			{ TEXT("int OptNullHandle_SetFromNativeRef_IsSet()"), TEXT("TOptional<UObject>.Set(null native ref) should be set"), 1 },
			{ TEXT("int OptNullHandle_SetFromNativeRef_ValueIsNull()"), TEXT("TOptional<UObject>.Set(null native ref) should store null"), 1 },
			{ TEXT("int OptNullHandle_AssignFromNativeRef_IsSet()"), TEXT("TOptional<UObject> assigned null native ref should be set"), 1 },
			{ TEXT("int OptNullHandle_AssignFromNativeRef_ValueIsNull()"), TEXT("TOptional<UObject> assigned null native ref should store null"), 1 },
		};
		ASSERT_THAT(IsTrue(ExpectGlobalInts(*TestRunner, Engine, Module, Cases), TEXT("Optional null handle cases should pass")));
	}

	TEST_METHOD(OptionalReturnTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			bool OptRet_Bool_IsSet()
			{
				TOptional<int> O;
				O.Set(42);
				return O.IsSet();
			}

			bool OptRet_Bool_IsSetEmpty()
			{
				TOptional<int> O;
				return O.IsSet();
			}

			float OptRet_Float_GetValue()
			{
				TOptional<float> O(3.5f);
				return O.GetValue();
			}

			float OptRet_Float_GetFallback()
			{
				TOptional<float> O;
				return O.Get(7.25f);
			}

			FString OptRet_String_GetValue()
			{
				TOptional<FString> O(FString("Hello"));
				return O.GetValue();
			}

			FString OptRet_String_GetFallback()
			{
				TOptional<FString> O;
				return O.Get("Fallback");
			}

			int OptRet_VerifyString_GetValue()
			{
				TOptional<FString> O(FString("Hello"));
				FString V = O.GetValue();
				return (V == "Hello") ? 1 : 0;
			}

			int OptRet_VerifyString_GetFallback()
			{
				TOptional<FString> O;
				FString V = O.Get("Fallback");
				return (V == "Fallback") ? 1 : 0;
			}

			FVector OptRet_Vector_GetValue()
			{
				TOptional<FVector> O(FVector(1, 2, 3));
				return O.GetValue();
			}

			int OptRet_VerifyVector_X()
			{
				FVector V = OptRet_Vector_GetValue();
				return (V.X > 0.9f && V.X < 1.1f) ? 1 : 0;
			}

			int OptRet_VerifyVector_Y()
			{
				FVector V = OptRet_Vector_GetValue();
				return (V.Y > 1.9f && V.Y < 2.1f) ? 1 : 0;
			}

			int OptRet_VerifyVector_Z()
			{
				FVector V = OptRet_Vector_GetValue();
				return (V.Z > 2.9f && V.Z < 3.1f) ? 1 : 0;
			}

			TOptional<int> MakeOptionalInt()
			{
				TOptional<int> O;
				O.Set(42);
				return O;
			}

			int OptRet_VerifyOptionalInt_IsSet()
			{
				TOptional<int> O = MakeOptionalInt();
				return O.IsSet() ? 1 : 0;
			}

			int OptRet_VerifyOptionalInt_GetValue()
			{
				TOptional<int> O = MakeOptionalInt();
				return O.GetValue();
			}

			TOptional<int> MakeOptionalIntEmpty()
			{
				TOptional<int> O;
				return O;
			}

			int OptRet_VerifyOptionalIntEmpty_IsSet()
			{
				TOptional<int> O = MakeOptionalIntEmpty();
				return O.IsSet() ? 1 : 0;
			}

			TOptional<FString> MakeOptionalString()
			{
				TOptional<FString> O(FString("World"));
				return O;
			}

			int OptRet_VerifyOptionalString_IsSet()
			{
				TOptional<FString> O = MakeOptionalString();
				return O.IsSet() ? 1 : 0;
			}

			int OptRet_VerifyOptionalString_Value()
			{
				TOptional<FString> O = MakeOptionalString();
				return (O.GetValue() == "World") ? 1 : 0;
			}

			TOptional<FVector> MakeOptionalVector()
			{
				TOptional<FVector> O(FVector(10, 20, 30));
				return O;
			}

			int OptRet_VerifyOptionalVector_IsSet()
			{
				TOptional<FVector> O = MakeOptionalVector();
				return O.IsSet() ? 1 : 0;
			}

			int OptRet_VerifyOptionalVector_X()
			{
				TOptional<FVector> O = MakeOptionalVector();
				return (O.GetValue().X > 9.9f && O.GetValue().X < 10.1f) ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_ReturnType"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional return type module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalReturnBool(*TestRunner, Engine, Module,
			TEXT("bool OptRet_Bool_IsSet()"),
			TEXT("bool return: set Optional.IsSet() should be true"), true),
			TEXT("Optional bool set return case should pass")));

		ASSERT_THAT(IsTrue(ExpectGlobalReturnBool(*TestRunner, Engine, Module,
			TEXT("bool OptRet_Bool_IsSetEmpty()"),
			TEXT("bool return: empty Optional.IsSet() should be false"), false),
			TEXT("Optional bool empty return case should pass")));

		ASSERT_THAT(IsTrue(ExpectGlobalReturnFloat(*TestRunner, Engine, Module,
			TEXT("float OptRet_Float_GetValue()"),
			TEXT("float return: GetValue should be 3.5"), 3.5f),
			TEXT("Optional float value return case should pass")));

		ASSERT_THAT(IsTrue(ExpectGlobalReturnFloat(*TestRunner, Engine, Module,
			TEXT("float OptRet_Float_GetFallback()"),
			TEXT("float return: Get fallback should be 7.25"), 7.25f),
			TEXT("Optional float fallback return case should pass")));

		const FExpectedGlobalInt IntCases[] = {
			{ TEXT("int OptRet_VerifyString_GetValue()"), TEXT("FString return: GetValue should match 'Hello'"), 1 },
			{ TEXT("int OptRet_VerifyString_GetFallback()"), TEXT("FString return: Get fallback should match 'Fallback'"), 1 },
			{ TEXT("int OptRet_VerifyVector_X()"), TEXT("FVector return: X should be ~1"), 1 },
			{ TEXT("int OptRet_VerifyVector_Y()"), TEXT("FVector return: Y should be ~2"), 1 },
			{ TEXT("int OptRet_VerifyVector_Z()"), TEXT("FVector return: Z should be ~3"), 1 },
			{ TEXT("int OptRet_VerifyOptionalInt_IsSet()"), TEXT("TOptional<int> return: should be set"), 1 },
			{ TEXT("int OptRet_VerifyOptionalInt_GetValue()"), TEXT("TOptional<int> return: GetValue should be 42"), 42 },
			{ TEXT("int OptRet_VerifyOptionalIntEmpty_IsSet()"), TEXT("TOptional<int> empty return: should not be set"), 0 },
			{ TEXT("int OptRet_VerifyOptionalString_IsSet()"), TEXT("TOptional<FString> return: should be set"), 1 },
			{ TEXT("int OptRet_VerifyOptionalString_Value()"), TEXT("TOptional<FString> return: value should match 'World'"), 1 },
			{ TEXT("int OptRet_VerifyOptionalVector_IsSet()"), TEXT("TOptional<FVector> return: should be set"), 1 },
			{ TEXT("int OptRet_VerifyOptionalVector_X()"), TEXT("TOptional<FVector> return: X should be ~10"), 1 },
		};
		ASSERT_THAT(IsTrue(ExpectGlobalInts(*TestRunner, Engine, Module, IntCases), TEXT("Optional return type int wrapper cases should pass")));
	}

	TEST_METHOD(OptionalLogDiagnostic)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int OptLog_IntAndString()
			{
				TOptional<int> OInt;
				OInt.Set(42);
				Log("OptLog TOptional<int> set: " + OInt.GetValue());

				TOptional<int> OEmpty;
				Log("OptLog TOptional<int> empty IsSet: " + OEmpty.IsSet());

				TOptional<FString> OStr(FString("Hello"));
				Log("OptLog TOptional<FString>: " + OStr.GetValue());

				TOptional<FName> OName(FName("TestName"));
				Log("OptLog TOptional<FName>: " + OName.GetValue());

				TOptional<float> OFloat(3.14f);
				Log("OptLog TOptional<float>: " + OFloat.GetValue());

				TOptional<bool> OBool(true);
				Log("OptLog TOptional<bool>: " + OBool.GetValue());

				TOptional<FVector> OVec(FVector(1, 2, 3));
				Log("OptLog TOptional<FVector>: " + OVec.GetValue());

				return 1;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_LogDiag"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional log diagnostic module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, Module,
			TEXT("int OptLog_IntAndString()"),
			TEXT("Log diagnostic: TOptional types should compile and log without crash"), 1),
			TEXT("Optional log diagnostic case should pass")));
	}

	TEST_METHOD(OptionalGetValueUnsetError)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int TriggerGetValueUnset()
			{
				TOptional<int> Empty;
				return Empty.GetValue();
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASOptional_GetValueUnsetError"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Optional unset GetValue error module should compile")));
		asIScriptModule& Module = ModuleScope.GetModule();

		TestRunner->AddExpectedErrorPlain(
			FString(TEXT("ASOptional_GetValueUnsetError")),
			EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("GetValue() called on Optional when not set"),
			EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(
			TEXT("int TriggerGetValueUnset()"),
			EAutomationExpectedErrorFlags::Contains, 0);

		ASSERT_THAT(IsTrue(ExecuteFunctionExpectingScriptException(
			*TestRunner, Engine, Module,
			TEXT("int TriggerGetValueUnset()"),
			TEXT("Unset TOptional.GetValue should raise exception"),
			FString(TEXT("GetValue() called on Optional when not set"))),
			TEXT("Unset Optional.GetValue should raise the expected script exception")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
