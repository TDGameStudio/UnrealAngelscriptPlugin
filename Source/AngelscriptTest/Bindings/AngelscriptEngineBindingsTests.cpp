// ============================================================================
// AngelscriptEngineBindingsTests.cpp
//
// Core engine value-type binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Engine.FAngelscriptEngineBindingsTest.*
//
// Sections:
//   ValueTypes        — int32, double, FString, FName, FVector, FRotator, FTransform, FText
//   FNameArrayCompat  — FName[] / TArray<FName> alias, add, copy, contains
//   FNameArrayIndexWriteBack — opIndex write-back without aliasing copies
//   ForeachCompat     — range-for over int[], TArray<FName>, TArray<FVector>, mutation
//
// CQTest adaptation notes:
//   Four legacy automation tests merged into one TEST_CLASS.
//   Each `int Entry()` split into named `int FuncName()` returning 1/0.
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

TEST_CLASS_WITH_FLAGS(FAngelscriptEngineBindingsTest,
	"Angelscript.TestModule.Bindings.Engine",
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
	// Section: ValueTypes
	// ====================================================================

	TEST_METHOD(ValueTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEngine_ValueTypes"), ASTEST_AS(R"AS(
			int ValueTypes_IntDouble()
			{
				int32 Count = 5;
				double Precise = 1.5;
				return (Count == 5 && Precise > 1.49 && Precise < 1.51) ? 1 : 0;
			}

			int ValueTypes_FName()
			{
				FString Text = "Alpha";
				FName Name(Text);
				return (Name == FName("Alpha")) ? 1 : 0;
			}

			int ValueTypes_FVector()
			{
				FVector Vector = FVector(1.0, 2.0, 3.0) + FVector::OneVector;
				return Vector.Equals(FVector(2.0, 3.0, 4.0)) ? 1 : 0;
			}

			int ValueTypes_FRotator()
			{
				FRotator FacingRotation = FVector::RightVector.Rotation();
				return FacingRotation.Equals(FRotator(0.0, 90.0, 0.0), 0.01) ? 1 : 0;
			}

			int ValueTypes_FTransform()
			{
				FTransform Transform = FTransform(FRotator::ZeroRotator, FVector(10.0, 0.0, 0.0), FVector::OneVector);
				FVector Transformed = Transform.TransformPosition(FVector::ForwardVector);
				return Transformed.Equals(FVector(11.0, 0.0, 0.0)) ? 1 : 0;
			}

			int ValueTypes_FText()
			{
				FText Label = FText::FromString("Alpha");
				return (!Label.IsEmpty() && Label.ToString() == "Alpha") ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValueTypes_IntDouble()"), TEXT("int32 and double should hold expected values"), 1), TEXT("int32 and double should hold expected values")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValueTypes_FName()"), TEXT("FName constructed from FString should match"), 1), TEXT("FName constructed from FString should match")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValueTypes_FVector()"), TEXT("FVector addition with OneVector should compute correctly"), 1), TEXT("FVector addition with OneVector should compute correctly")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValueTypes_FRotator()"), TEXT("FVector::RightVector.Rotation() should yield (0,90,0)"), 1), TEXT("FVector::RightVector.Rotation() should yield (0,90,0)")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValueTypes_FTransform()"), TEXT("FTransform::TransformPosition should offset correctly"), 1), TEXT("FTransform::TransformPosition should offset correctly")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ValueTypes_FText()"), TEXT("FText::FromString should round-trip correctly"), 1), TEXT("FText::FromString should round-trip correctly")));
	}

	// ====================================================================
	// Section: FNameArrayCompat
	// ====================================================================

	TEST_METHOD(FNameArrayPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEngine_FNameArrayCompat"), ASTEST_AS(R"AS(
			struct FFNameMemberHolder
			{
				UObject NativeSelf;
				FName Value;
			}

			int FNameArray_AliasAdd()
			{
				FName[] AliasValues;
				AliasValues.Add(n"Alpha");
				AliasValues.Add(n"Gamma");
				return (AliasValues.Num() == 2 && AliasValues[1] == n"Gamma") ? 1 : 0;
			}

			int FNameArray_ExplicitAdd()
			{
				TArray<FName> ExplicitValues;
				ExplicitValues.Add(n"Beta");
				FName Copy = n"Alpha";
				ExplicitValues.Add(Copy);
				return (ExplicitValues.Num() == 2 && ExplicitValues[0] == n"Beta" && ExplicitValues[1] == n"Alpha") ? 1 : 0;
			}

			int FNameArray_Copy()
			{
				FName[] AliasValues;
				AliasValues.Add(n"Alpha");
				FName Copy = AliasValues[0];
				return (Copy == n"Alpha") ? 1 : 0;
			}

			int FNameArray_MemberHolder()
			{
				FFNameMemberHolder Holder;
				Holder.Value = n"Delta";
				return (Holder.Value == n"Delta") ? 1 : 0;
			}

			int FNameArray_Contains()
			{
				FName[] AliasValues;
				AliasValues.Add(n"Alpha");
				TArray<FName> ExplicitValues;
				ExplicitValues.Add(n"Beta");
				ExplicitValues.Add(n"Alpha");
				return (AliasValues.Contains(n"Alpha") && ExplicitValues.Contains(n"Alpha")) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameArray_AliasAdd()"), TEXT("FName[] alias should support Add and indexing"), 1), TEXT("FName[] alias should support Add and indexing")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameArray_ExplicitAdd()"), TEXT("TArray<FName> explicit should support Add and copy"), 1), TEXT("TArray<FName> explicit should support Add and copy")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameArray_Copy()"), TEXT("FName copy from array index should preserve value"), 1), TEXT("FName copy from array index should preserve value")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameArray_MemberHolder()"), TEXT("FName struct member should support assignment"), 1), TEXT("FName struct member should support assignment")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameArray_Contains()"), TEXT("FName arrays should support Contains"), 1), TEXT("FName arrays should support Contains")));
	}

	// ====================================================================
	// Section: FNameArrayIndexWriteBack
	// ====================================================================

	TEST_METHOD(FNameArrayIndexWriteBack)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEngine_FNameArrayIdxWB"), ASTEST_AS(R"AS(
			int FNameIdxWB_AliasWriteBack()
			{
				FName[] AliasValues;
				AliasValues.Add(n"Alpha");
				AliasValues.Add(n"Beta");
				FName Copy = AliasValues[0];
				AliasValues[0] = n"Omega";
				return (Copy == n"Alpha" && AliasValues[0] == n"Omega" && AliasValues[1] == n"Beta") ? 1 : 0;
			}

			int FNameIdxWB_ExplicitWriteBack()
			{
				TArray<FName> ExplicitValues;
				ExplicitValues.Add(n"Gamma");
				ExplicitValues[0] = n"Sigma";
				return (ExplicitValues[0] == n"Sigma" && ExplicitValues.Num() == 1) ? 1 : 0;
			}

			int FNameIdxWB_ContainsAfterMutation()
			{
				FName[] AliasValues;
				AliasValues.Add(n"Alpha");
				AliasValues[0] = n"Omega";
				TArray<FName> ExplicitValues;
				ExplicitValues.Add(n"Gamma");
				ExplicitValues[0] = n"Sigma";
				return (AliasValues.Contains(n"Omega") && !AliasValues.Contains(n"Alpha")
				&& ExplicitValues.Contains(n"Sigma") && !ExplicitValues.Contains(n"Gamma")) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameIdxWB_AliasWriteBack()"), TEXT("FName[] opIndex write-back should not alias the copy"), 1), TEXT("FName[] opIndex write-back should not alias the copy")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameIdxWB_ExplicitWriteBack()"), TEXT("TArray<FName> opIndex write-back should replace in-place"), 1), TEXT("TArray<FName> opIndex write-back should replace in-place")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int FNameIdxWB_ContainsAfterMutation()"), TEXT("Contains should reflect mutations after opIndex write-back"), 1), TEXT("Contains should reflect mutations after opIndex write-back")));
	}

	// ====================================================================
	// Section: ForeachCompat
	// ====================================================================

	TEST_METHOD(ForeachPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASEngine_ForeachCompat"), ASTEST_AS(R"AS(
			int Foreach_IntAlias()
			{
				int[] AliasValues;
				AliasValues.Add(1);
				AliasValues.Add(2);
				AliasValues.Add(3);
				int Sum = 0;
				for (int Value : AliasValues)
				{
					Sum += Value;
				}
				return (Sum == 6) ? 1 : 0;
			}

			int Foreach_FNameExplicit()
			{
				TArray<FName> ExplicitNames;
				ExplicitNames.Add(n"Alpha");
				ExplicitNames.Add(n"Beta");
				int NameCount = 0;
				for (FName Name : ExplicitNames)
				{
					if (Name == n"Alpha" || Name == n"Beta")
					{
						NameCount += 1;
					}
				}
				return (NameCount == 2) ? 1 : 0;
			}

			int Foreach_FVectorExplicit()
			{
				TArray<FVector> ExplicitVectors;
				ExplicitVectors.Add(FVector(1.0, 2.0, 3.0));
				ExplicitVectors.Add(FVector(4.0, 5.0, 6.0));
				double VectorXSum = 0.0;
				for (const FVector& VectorValue : ExplicitVectors)
				{
					VectorXSum += VectorValue.X;
				}
				return (VectorXSum > 4.99 && VectorXSum < 5.01) ? 1 : 0;
			}

			int Foreach_MutatedArray()
			{
				int[] AliasValues;
				AliasValues.Add(1);
				AliasValues.Add(2);
				AliasValues.Add(3);
				AliasValues[1] = 5;
				int MutatedTotal = 0;
				for (int Value : AliasValues)
				{
					MutatedTotal += Value;
				}
				return (MutatedTotal == 9) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Foreach_IntAlias()"), TEXT("range-for over int[] should sum correctly"), 1), TEXT("range-for over int[] should sum correctly")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Foreach_FNameExplicit()"), TEXT("range-for over TArray<FName> should iterate all elements"), 1), TEXT("range-for over TArray<FName> should iterate all elements")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Foreach_FVectorExplicit()"), TEXT("range-for over TArray<FVector> const-ref should read X components"), 1), TEXT("range-for over TArray<FVector> const-ref should read X components")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Foreach_MutatedArray()"), TEXT("range-for after opIndex mutation should reflect new values"), 1), TEXT("range-for after opIndex mutation should reflect new values")));
	}
};

#endif
