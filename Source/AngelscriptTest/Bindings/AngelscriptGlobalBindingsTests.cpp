// ============================================================================
// AngelscriptGlobalBindingsTests.cpp
//
// Global utility function binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Global.FAngelscriptGlobalBindingsTest.*
//
// Sections:
//   GlobalVariables     — CollisionProfile, FComponentQueryParams globals
//   PrimitiveAliasesAndImplicitStringConversion — primitive aliases and ToString conversions
//   PrimitiveConstants — integer limits and floating-point constants
//   CommandletGlobals   — IsRunningCommandlet, IsRunningCookCommandlet, GetRunningCommandletClass
//
// CQTest adaptation notes:
//   CommandletGlobals requires runtime template substitution for expected values.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptGlobalBindingsTest,
	"Angelscript.TestModule.Bindings.Global",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: GlobalVariables
	// ====================================================================

	TEST_METHOD(GlobalVariables)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGlobal_GlobalVar"), ASTEST_AS(R"AS(
			int GlobalVar_CollisionProfileBlockAllDynamic()
			{
				return (CollisionProfile::BlockAllDynamic.Compare(FName("BlockAllDynamic")) == 0) ? 1 : 0;
			}

			int GlobalVar_DefaultComponentQueryParams()
			{
				FComponentQueryParams FreshParams;
				return (FComponentQueryParams::DefaultComponentQueryParams.ShapeCollisionMask.Bits == FreshParams.ShapeCollisionMask.Bits) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GlobalVar_CollisionProfileBlockAllDynamic()"), TEXT("CollisionProfile::BlockAllDynamic should match FName"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int GlobalVar_DefaultComponentQueryParams()"), TEXT("DefaultComponentQueryParams should match fresh default"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(PrimitiveAliasesAndImplicitStringConversion)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyPrimitiveAliasesAndImplicitStringConversion()
			{
				int32 SignedAlias = int8(-8);
				uint32 UnsignedAlias = uint16(16);
				float32 NarrowFloat = 1.25f;
				float64 WideFloat = NarrowFloat;
				double DoubleAlias = WideFloat;

				FString Rendered = "i8=" + int8(-8)
					+ ",i16=" + int16(-16)
					+ ",i32=" + SignedAlias
					+ ",i64=" + int64(-64)
					+ ",u8=" + uint8(8)
					+ ",u16=" + UnsignedAlias
					+ ",u32=" + uint32(32)
					+ ",u64=" + uint64(64)
					+ ",f32=" + NarrowFloat
					+ ",f64=" + DoubleAlias
					+ ",bool=" + true;

				return Rendered == "i8=-8,i16=-16,i32=-8,i64=-64,u8=8,u16=16,u32=32,u64=64,f32=1.25,f64=1.25,bool=true" ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASGlobal_PrimitiveAliasesAndImplicitStringConversion"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Primitive alias and implicit string conversion module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyPrimitiveAliasesAndImplicitStringConversion()"),
			TEXT("Primitive aliases and implicit string conversions should preserve every registered primitive form"),
			1), TEXT("Primitive aliases and implicit string conversions should execute")));
	}

	TEST_METHOD(PrimitiveConstants)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyPrimitiveIntegerConstants()
			{
				if (MIN_uint8 != 0 || MAX_uint8 != 255
					|| MIN_uint16 != 0 || MAX_uint16 != 65535
					|| MIN_uint32 != 0 || MAX_uint32 <= uint32(MAX_uint16)
					|| MIN_uint64 != 0 || MAX_uint64 <= uint64(MAX_uint32)
					|| MIN_int8 != -128 || MAX_int8 != 127
					|| MIN_int16 != -32768 || MAX_int16 != 32767
					|| MIN_int32 >= int32(MIN_int16) || MAX_int32 <= int32(MAX_int16)
					|| MIN_int64 >= int64(MIN_int32) || MAX_int64 <= int64(MAX_int32))
				{
					return 0;
				}

				return 1;
			}

			int VerifyPrimitiveMinFloat()
			{
				return MIN_flt > 0 ? 1 : 0;
			}

			int VerifyPrimitiveMaxFloat()
			{
				return MAX_flt > MIN_flt ? 1 : 0;
			}

			int VerifyPrimitiveNaN()
			{
				return Math::IsNaN(NAN_flt) ? 1 : 0;
			}

			int VerifyPrimitiveMathConstants()
			{
				return PI > 3.14
					&& HALF_PI > 1.57
					&& TWO_PI > 6.28
					&& EULERS_NUMBER > 2.71
					&& SMALL_NUMBER < KINDA_SMALL_NUMBER
					&& KINDA_SMALL_NUMBER < BIG_NUMBER
					&& THRESH_VECTOR_NORMALIZED > 0
					&& THRESH_NORMALS_ARE_PARALLEL > 0
					&& THRESH_NORMALS_ARE_ORTHOGONAL > 0
					&& __PI_flt > 3.14f ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASGlobal_PrimitiveConstants"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Primitive constant module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyPrimitiveIntegerConstants()"),
			TEXT("Primitive integer limits should retain their registered values"),
			1), TEXT("Primitive integer limit verification should execute")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyPrimitiveMinFloat()"),
			TEXT("MIN_flt should remain positive"),
			1), TEXT("MIN_flt verification should execute")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyPrimitiveMaxFloat()"),
			TEXT("MAX_flt should remain greater than MIN_flt"),
			1), TEXT("MAX_flt verification should execute")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyPrimitiveNaN()"),
			TEXT("NAN_flt should retain its NaN bit pattern through a native call"),
			1), TEXT("NAN_flt verification should execute")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyPrimitiveMathConstants()"),
			TEXT("Primitive mathematical constants should retain their registered values"),
			1), TEXT("Primitive mathematical constant verification should execute")));
	}

	// ====================================================================
	// Section: CommandletGlobals
	// ====================================================================

	TEST_METHOD(CommandletGlobals)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bExpectedRunningCommandlet = ::IsRunningCommandlet();
		const bool bExpectedRunningCookCommandlet = ::IsRunningCookCommandlet();
		const bool bExpectedRunningDLCCookCommandlet = ::IsRunningDLCCookCommandlet();
		UClass* ExpectedCommandletClass = ::GetRunningCommandletClass();

		FString CommandletGlobalsSource = ASTEST_AS(R"AS(
			int CommandletGlobals_IsRunningCommandlet()
			{
				return (IsRunningCommandlet() == $IS_RUNNING_COMMANDLET$) ? 1 : 0;
			}

			int CommandletGlobals_IsRunningCookCommandlet()
			{
				return (IsRunningCookCommandlet() == $IS_RUNNING_COOK_COMMANDLET$) ? 1 : 0;
			}

			int CommandletGlobals_IsRunningDLCCookCommandlet()
			{
				return (IsRunningDLCCookCommandlet() == $IS_RUNNING_DLC_COOK_COMMANDLET$) ? 1 : 0;
			}

			int CommandletGlobals_GetRunningCommandletClass()
			{
				UClass RunningCommandletClass = GetRunningCommandletClass();
				if ($EXPECTS_NULL_COMMANDLET_CLASS$)
				{
					return (RunningCommandletClass == null) ? 1 : 0;
				}
				else
				{
					if (!IsValid(RunningCommandletClass))
					{
						return 0;
					}
					return (RunningCommandletClass.GetName() == "$RUNNING_COMMANDLET_CLASS_NAME$") ? 1 : 0;
				}
			}
			)AS");
		CommandletGlobalsSource.ReplaceInline(TEXT("$IS_RUNNING_COMMANDLET$"), bExpectedRunningCommandlet ? TEXT("true") : TEXT("false"));
		CommandletGlobalsSource.ReplaceInline(TEXT("$IS_RUNNING_COOK_COMMANDLET$"), bExpectedRunningCookCommandlet ? TEXT("true") : TEXT("false"));
		CommandletGlobalsSource.ReplaceInline(TEXT("$IS_RUNNING_DLC_COOK_COMMANDLET$"), bExpectedRunningDLCCookCommandlet ? TEXT("true") : TEXT("false"));
		CommandletGlobalsSource.ReplaceInline(TEXT("$EXPECTS_NULL_COMMANDLET_CLASS$"), ExpectedCommandletClass == nullptr ? TEXT("true") : TEXT("false"));
		CommandletGlobalsSource.ReplaceInline(
			TEXT("$RUNNING_COMMANDLET_CLASS_NAME$"),
			ExpectedCommandletClass != nullptr
				? *ExpectedCommandletClass->GetName().ReplaceCharWithEscapedChar()
				: TEXT(""));

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASGlobal_Commandlet"), CommandletGlobalsSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_IsRunningCommandlet()"), TEXT("IsRunningCommandlet should match native value"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_IsRunningCookCommandlet()"), TEXT("IsRunningCookCommandlet should match native value"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_IsRunningDLCCookCommandlet()"), TEXT("IsRunningDLCCookCommandlet should match native value"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int CommandletGlobals_GetRunningCommandletClass()"), TEXT("GetRunningCommandletClass should match native value"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
