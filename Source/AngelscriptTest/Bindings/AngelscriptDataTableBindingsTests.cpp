// ============================================================================
// AngelscriptDataTableBindingsTests.cpp
//
// DataTable row/handle/category binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.DataTable.FAngelscriptDataTableBindingsTest.*
//
// Sections:
//   RowHandleCompat  — AddRow, FindRow, GetAllRows, FDataTableRowHandle, FDataTableCategoryHandle
//   ErrorPaths       — wrong-struct, null-handle, wrong-array exception paths
//
// CQTest adaptation notes:
//   Two legacy automation tests merged into one TEST_CLASS.
//   Uses $TOKEN$ → ReplaceInline for DataTable path injection.
//   ErrorPaths section preserves AddExpectedError + manual context execution.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestUtilities.h"

#include "Bindings/AngelscriptDataTableBindingTestTypes.h"

#include "Engine/DataTable.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptDataTableBindingsTest,
	"Angelscript.TestModule.Bindings.DataTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static const FAngelscriptBindingDataTableRow* FindBindingRow(
		FAutomationTestBase& Test,
		const UDataTable& DataTable,
		const TCHAR* RowName,
		const TCHAR* ContextLabel)
	{
		const FAngelscriptBindingDataTableRow* Row = DataTable.FindRow<FAngelscriptBindingDataTableRow>(FName(RowName), ContextLabel);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Row, *FString::Printf(TEXT("%s should resolve row '%s'"), ContextLabel, RowName)))
		{
			return nullptr;
		}
		return Row;
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

	// ====================================================================
	// Section: RowHandleCompat
	// ====================================================================

	TEST_METHOD(RowHandleAndCategoryRoundTrip)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASDataTableRowHandleCompat"));
		};

		const FName TableName = MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("BindingDataTableCompat"));
		UDataTable* DataTable = NewObject<UDataTable>(GetTransientPackage(), TableName);
		if (!this->Assert.IsNotNull(DataTable, TEXT("Data table binding test should create a transient UDataTable")))
		{
			return;
		}

		DataTable->RowStruct = FAngelscriptBindingDataTableRow::StaticStruct();

		FString RowHandleScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				UObject TableObject = FindObject("$TABLE_PATH$");
				UDataTable Table = Cast<UDataTable>(TableObject);
				if (!IsValid(Table))
				{
					return 10;
				}

				FAngelscriptBindingDataTableRow Alpha;
				Alpha.Category = n"Enemy";
				Alpha.Count = 2;
				Alpha.Label = "Alpha";
				Table.AddRow(n"Alpha", Alpha);

				FAngelscriptBindingDataTableRow Beta;
				Beta.Category = n"Item";
				Beta.Count = 7;
				Beta.Label = "Beta";
				Table.AddRow(n"Beta", Beta);

				FAngelscriptBindingDataTableRow Gamma;
				Gamma.Category = n"Enemy";
				Gamma.Count = 5;
				Gamma.Label = "Gamma";
				Table.AddRow(n"Gamma", Gamma);

				TArray<FName> RowNames = Table.GetRowNames();
				if (RowNames.Num() != 3)
				{
					return 20;
				}
				if (!RowNames.Contains(n"Alpha") || !RowNames.Contains(n"Beta") || !RowNames.Contains(n"Gamma"))
				{
					return 30;
				}

				FAngelscriptBindingDataTableRow FoundAlpha;
				if (!Table.FindRow(n"Alpha", FoundAlpha))
				{
					return 40;
				}
				if (FoundAlpha.Category != n"Enemy" || FoundAlpha.Count != 2 || FoundAlpha.Label != "Alpha")
				{
					return 50;
				}

				TArray<FAngelscriptBindingDataTableRow> AllRows;
				FAngelscriptBindingDataTableRow Sentinel;
				Sentinel.Category = n"Sentinel";
				Sentinel.Count = -99;
				Sentinel.Label = "Sentinel";
				AllRows.Add(Sentinel);
				Table.GetAllRows(AllRows);
				if (AllRows.Num() != 4)
				{
					return 60;
				}
				if (AllRows[0].Category != n"Sentinel" || AllRows[0].Count != -99 || AllRows[0].Label != "Sentinel")
				{
					return 70;
				}

				bool bSawAlpha = false;
				bool bSawBeta = false;
				bool bSawGamma = false;
				for (int Index = 1; Index < AllRows.Num(); ++Index)
				{
					if (AllRows[Index].Label == "Alpha" && AllRows[Index].Category == n"Enemy" && AllRows[Index].Count == 2)
					{
						bSawAlpha = true;
					}
					else if (AllRows[Index].Label == "Beta" && AllRows[Index].Category == n"Item" && AllRows[Index].Count == 7)
					{
						bSawBeta = true;
					}
					else if (AllRows[Index].Label == "Gamma" && AllRows[Index].Category == n"Enemy" && AllRows[Index].Count == 5)
					{
						bSawGamma = true;
					}
					else
					{
						return 80;
					}
				}
				if (!bSawAlpha || !bSawBeta || !bSawGamma)
				{
					return 90;
				}

				FDataTableRowHandle BetaHandle;
				BetaHandle.DataTable = Table;
				BetaHandle.RowName = n"Beta";
				FAngelscriptBindingDataTableRow BetaRow;
				if (!BetaHandle.GetRow(BetaRow))
				{
					return 100;
				}
				if (BetaRow.Category != n"Item" || BetaRow.Count != 7 || BetaRow.Label != "Beta")
				{
					return 110;
				}

				FDataTableCategoryHandle EnemyHandle;
				EnemyHandle.DataTable = Table;
				EnemyHandle.ColumnName = n"Category";
				EnemyHandle.RowContents = n"Enemy";

				TArray<FName> EnemyNames = EnemyHandle.GetRowNames();
				if (EnemyNames.Num() != 2)
				{
					return 120;
				}
				if (!EnemyNames.Contains(n"Alpha") || !EnemyNames.Contains(n"Gamma"))
				{
					return 130;
				}

				TArray<FAngelscriptBindingDataTableRow> EnemyRows;
				EnemyRows.Add(Sentinel);
				EnemyHandle.GetRows(EnemyRows);
				if (EnemyRows.Num() != 3)
				{
					return 140;
				}
				if (EnemyRows[0].Category != n"Sentinel" || EnemyRows[0].Count != -99 || EnemyRows[0].Label != "Sentinel")
				{
					return 150;
				}

				int EnemyRowCount = 0;
				bool bSawEnemyAlpha = false;
				bool bSawEnemyGamma = false;
				for (int Index = 1; Index < EnemyRows.Num(); ++Index)
				{
					if (EnemyRows[Index].Category != n"Enemy")
					{
						return 160;
					}
					if (EnemyRows[Index].Label == "Alpha" && EnemyRows[Index].Count == 2)
					{
						bSawEnemyAlpha = true;
					}
					else if (EnemyRows[Index].Label == "Gamma" && EnemyRows[Index].Count == 5)
					{
						bSawEnemyGamma = true;
					}
					else
					{
						return 170;
					}
					EnemyRowCount += 1;
				}
				if (EnemyRowCount != 2 || !bSawEnemyAlpha || !bSawEnemyGamma)
				{
					return 180;
				}

				return 1;
			}
			)AS");

		RowHandleScriptSource.ReplaceInline(TEXT("$TABLE_PATH$"), *DataTable->GetPathName().ReplaceCharWithEscapedChar());

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASDataTableRowHandleCompat", RowHandleScriptSource);
		if (Module == nullptr)
		{
			return;
		}

		asIScriptFunction* EntryFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		if (EntryFunction == nullptr)
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *EntryFunction, Result))
		{
			return;
		}

		if (!this->Assert.AreEqual(
			1,
			Result,
			TEXT("Data table row, handle and category bindings should preserve row copy, append and category-filter semantics")))
		{
			return;
		}

		if (!this->Assert.AreEqual(
			3,
			DataTable->GetRowNames().Num(),
			TEXT("Native data table should contain three rows after the script add-row round-trip")))
		{
			return;
		}

		const FAngelscriptBindingDataTableRow* AlphaRow = FindBindingRow(*TestRunner, *DataTable, TEXT("Alpha"), TEXT("Data table row handle compat"));
		const FAngelscriptBindingDataTableRow* BetaRow = FindBindingRow(*TestRunner, *DataTable, TEXT("Beta"), TEXT("Data table row handle compat"));
		const FAngelscriptBindingDataTableRow* GammaRow = FindBindingRow(*TestRunner, *DataTable, TEXT("Gamma"), TEXT("Data table row handle compat"));
		if (AlphaRow == nullptr || BetaRow == nullptr || GammaRow == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(FName(TEXT("Enemy")), AlphaRow->Category, TEXT("Alpha row category should match the script-authored value")));
		ASSERT_THAT(AreEqual(2, AlphaRow->Count, TEXT("Alpha row count should match the script-authored value")));
		ASSERT_THAT(AreEqual(FString(TEXT("Alpha")), AlphaRow->Label, TEXT("Alpha row label should match the script-authored value")));
		ASSERT_THAT(AreEqual(FName(TEXT("Item")), BetaRow->Category, TEXT("Beta row category should match the script-authored value")));
		ASSERT_THAT(AreEqual(7, BetaRow->Count, TEXT("Beta row count should match the script-authored value")));
		ASSERT_THAT(AreEqual(FString(TEXT("Beta")), BetaRow->Label, TEXT("Beta row label should match the script-authored value")));
		ASSERT_THAT(AreEqual(FName(TEXT("Enemy")), GammaRow->Category, TEXT("Gamma row category should match the script-authored value")));
		ASSERT_THAT(AreEqual(5, GammaRow->Count, TEXT("Gamma row count should match the script-authored value")));
		ASSERT_THAT(AreEqual(FString(TEXT("Gamma")), GammaRow->Label, TEXT("Gamma row label should match the script-authored value")));
	}

	// ====================================================================
	// Section: ErrorPaths
	// ====================================================================

	TEST_METHOD(ErrorPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASDataTableErrorPathsState"));
			Engine.DiscardModule(TEXT("ASDataTableErrorPathsWrongArray"));
		};

		const FName TableName = MakeUniqueObjectName(GetTransientPackage(), UDataTable::StaticClass(), TEXT("BindingDataTableErrorPaths"));
		UDataTable* DataTable = NewObject<UDataTable>(GetTransientPackage(), TableName);
		if (!this->Assert.IsNotNull(DataTable, TEXT("Data table error-path test should create a transient UDataTable")))
		{
			return;
		}

		DataTable->RowStruct = FAngelscriptBindingDataTableRow::StaticStruct();

		FAngelscriptBindingDataTableRow AlphaRow;
		AlphaRow.Category = TEXT("Enemy");
		AlphaRow.Count = 2;
		AlphaRow.Label = TEXT("Alpha");
		DataTable->AddRow(TEXT("Alpha"), AlphaRow);

		FString StateScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				UObject TableObject = FindObject("$TABLE_PATH$");
				UDataTable Table = Cast<UDataTable>(TableObject);
				if (!IsValid(Table))
				{
					return 10;
				}

				FVector WrongRow;
				WrongRow.X = 11;
				WrongRow.Y = 22;
				WrongRow.Z = 33;
				if (Table.FindRow(n"Alpha", WrongRow))
				{
					return 20;
				}
				if (WrongRow.X != 11 || WrongRow.Y != 22 || WrongRow.Z != 33)
				{
					return 30;
				}

				int InitialRowCount = Table.GetRowNames().Num();
				Table.AddRow(n"Bad", WrongRow);
				if (Table.GetRowNames().Num() != InitialRowCount)
				{
					return 40;
				}

				FDataTableRowHandle NullRowHandle;
				FAngelscriptBindingDataTableRow NullHandleOut;
				NullHandleOut.Category = n"Sentinel";
				NullHandleOut.Count = -99;
				NullHandleOut.Label = "Sentinel";
				if (NullRowHandle.GetRow(NullHandleOut))
				{
					return 50;
				}
				if (NullHandleOut.Category != n"Sentinel" || NullHandleOut.Count != -99 || NullHandleOut.Label != "Sentinel")
				{
					return 60;
				}

				FDataTableCategoryHandle NullCategoryHandle;
				TArray<FName> NullRowNames = NullCategoryHandle.GetRowNames();
				if (NullRowNames.Num() != 0)
				{
					return 70;
				}

				TArray<FAngelscriptBindingDataTableRow> NullRows;
				FAngelscriptBindingDataTableRow Sentinel;
				Sentinel.Category = n"Sentinel";
				Sentinel.Count = -99;
				Sentinel.Label = "Sentinel";
				NullRows.Add(Sentinel);
				NullCategoryHandle.GetRows(NullRows);
				if (NullRows.Num() != 1)
				{
					return 80;
				}
				if (NullRows[0].Category != n"Sentinel" || NullRows[0].Count != -99 || NullRows[0].Label != "Sentinel")
				{
					return 90;
				}

				return 1;
			}
			)AS");

		FString WrongArrayScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				UObject TableObject = FindObject("$TABLE_PATH$");
				UDataTable Table = Cast<UDataTable>(TableObject);
				if (!IsValid(Table))
				{
					return 10;
				}

				TArray<FVector> WrongRows;
				FVector Seed;
				Seed.X = 1;
				Seed.Y = 2;
				Seed.Z = 3;
				WrongRows.Add(Seed);
				Table.GetAllRows(WrongRows);
				return WrongRows.Num();
			}
			)AS");

		const FString EscapedTablePath = DataTable->GetPathName().ReplaceCharWithEscapedChar();
		StateScriptSource.ReplaceInline(TEXT("$TABLE_PATH$"), *EscapedTablePath);
		WrongArrayScriptSource.ReplaceInline(TEXT("$TABLE_PATH$"), *EscapedTablePath);

		asIScriptModule* StateModule = BuildModule(*TestRunner, Engine, "ASDataTableErrorPathsState", StateScriptSource);
		if (StateModule == nullptr)
		{
			return;
		}

		asIScriptFunction* StateEntryFunction = GetFunctionByDecl(*TestRunner, *StateModule, TEXT("int Entry()"));
		if (StateEntryFunction == nullptr)
		{
			return;
		}

		int32 StateResult = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *StateEntryFunction, StateResult))
		{
			return;
		}

		if (!this->Assert.AreEqual(
			1,
			StateResult,
			TEXT("Data table error paths should keep wrong-struct, null-handle and invalid-category operations fail-closed")))
		{
			return;
		}
		if (!this->Assert.AreEqual(
			1,
			DataTable->GetRowNames().Num(),
			TEXT("Data table error paths should keep the native table row count unchanged after wrong-struct AddRow")))
		{
			return;
		}
		if (!this->Assert.IsFalse(
			DataTable->GetRowNames().Contains(TEXT("Bad")),
			TEXT("Data table error paths should not create a new row when AddRow receives the wrong struct type")))
		{
			return;
		}

		asIScriptModule* WrongArrayModule = BuildModule(*TestRunner, Engine, "ASDataTableErrorPathsWrongArray", WrongArrayScriptSource);
		if (WrongArrayModule == nullptr)
		{
			return;
		}

		asIScriptFunction* WrongArrayEntryFunction = GetFunctionByDecl(*TestRunner, *WrongArrayModule, TEXT("int Entry()"));
		if (WrongArrayEntryFunction == nullptr)
		{
			return;
		}

		TestRunner->AddExpectedError(TEXT("ASDataTableErrorPathsWrongArray"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("int Entry() | Line 16 | Col 2"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestRunner->AddExpectedError(TEXT("OutArray must be a TArray of structs."), EAutomationExpectedErrorFlags::Contains, 1);

		asIScriptContext* WrongArrayContext = Engine.CreateContext();
		if (!this->Assert.IsNotNull(WrongArrayContext, TEXT("Data table error paths should create a context for the wrong-array test case")))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			WrongArrayContext->Release();
		};

		const int WrongArrayPrepareResult = WrongArrayContext->Prepare(WrongArrayEntryFunction);
		if (!this->Assert.AreEqual(
				static_cast<int32>(asSUCCESS),
				WrongArrayPrepareResult,
				TEXT("Data table error paths should prepare the wrong-array test case successfully")))
		{
			return;
		}

		const int WrongArrayExecuteResult = WrongArrayContext->Execute();
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			WrongArrayExecuteResult,
			TEXT("Data table error paths should raise a script exception when GetAllRows receives a TArray with the wrong subtype")));
		const FString WrongArrayException = WrongArrayContext->GetExceptionString() != nullptr
			? UTF8_TO_TCHAR(WrongArrayContext->GetExceptionString())
			: TEXT("");
		ASSERT_THAT(AreEqual(
			FString(TEXT("OutArray must be a TArray of structs.")),
			WrongArrayException,
			TEXT("Data table error paths should preserve the thrown wrong-array exception text")));
		ASSERT_THAT(AreEqual(
			1,
			DataTable->GetRowNames().Num(),
			TEXT("Data table error paths should keep the native table unchanged after wrong-array execution fails")));
	}
};

#endif
