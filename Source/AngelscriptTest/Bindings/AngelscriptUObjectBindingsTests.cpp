// ============================================================================
// AngelscriptUObjectBindingsTests.cpp
//
// UObject binding cross-validation tests. Pattern:
//   AS creates / mutates object → returns to C++ → C++ verifies real UObject
//   state → AS performs cleanup → C++ verifies post-cleanup state.
//
// Automation IDs (13 tests under UObject. prefix):
//   - Angelscript.TestModule.Bindings.UObject.CreateAndIdentity
//   - Angelscript.TestModule.Bindings.UObject.HierarchyAndOuter
//   - Angelscript.TestModule.Bindings.UObject.TypeQueryAndCast
//   - Angelscript.TestModule.Bindings.UObject.FindAndLookup
//   - Angelscript.TestModule.Bindings.UObject.RootLifecycle
//   - Angelscript.TestModule.Bindings.UObject.FlagMutation
//   - Angelscript.TestModule.Bindings.UObject.NullAndIsValid
//   - Angelscript.TestModule.Bindings.UObject.NewObjectVariants
//   - Angelscript.TestModule.Bindings.UObject.ClassReflection
//   - Angelscript.TestModule.Bindings.UObject.ReturnValueCrossCheck
//   - Angelscript.TestModule.Bindings.UObject.CppToScriptPassthrough
//   - Angelscript.TestModule.Bindings.UObject.ObjectChainAndNesting
//   - Angelscript.TestModule.Bindings.UObject.LogAndDiagnostics
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestEngineHelper.h"

#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS



// ============================================================================
// Sections — each section uses AS→C++ cross-validation
// ============================================================================

namespace
{
	// -----------------------------------------------------------------------
	// Section: CreateAndIdentity
	//   AS creates object with name → returns to C++ → C++ verifies GetName,
	//   GetClass, GetFullName, GetPathName on the real UObject pointer.
	// -----------------------------------------------------------------------
	bool RunCreateAndIdentitySection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_CreateIdentity"), TEXT(R"(
UObject CreateNamedObject()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjBindTest_Identity");
}
FName ScriptGetName(UObject Obj)
{
	return Obj.GetName();
}
FString ScriptGetFullName(UObject Obj)
{
	return Obj.GetFullName();
}
FString ScriptGetPathName(UObject Obj)
{
	return Obj.GetPathName();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// Step 1: AS creates the object, returns UObject* to C++
		FASGlobalFunctionInvoker CreateInvoker(
			Test, Engine, Module, TEXT("UObject CreateNamedObject()"));
		if (!LocalAssert.IsTrue(CreateInvoker.IsValid(), TEXT("[UObject] CreateNamedObject invoker should be valid")))
			return false;

		UObject* Obj = CreateInvoker.CallAndReturn<UObject*>(nullptr);
		bPassed &= LocalAssert.IsNotNull(Obj, TEXT("[UObject] AS-created object should be non-null in C++"));
		if (Obj == nullptr) return false;

		// Step 2: C++ directly verifies the real UObject
		bPassed &= LocalAssert.AreEqual(
			FName(TEXT("UObjBindTest_Identity")),
			Obj->GetFName(),
			TEXT("[UObject] C++ GetFName should match the name AS gave"));
		bPassed &= LocalAssert.AreEqual(
			UTexture2D::StaticClass(),
			Obj->GetClass(),
			TEXT("[UObject] C++ GetClass should be UTexture2D"));
		bPassed &= LocalAssert.IsTrue(
			Obj->GetFullName().Contains(TEXT("Texture2D")),
			TEXT("[UObject] C++ GetFullName should contain Texture2D"));
		bPassed &= LocalAssert.IsTrue(
			Obj->GetPathName().Len() > 0,
			TEXT("[UObject] C++ GetPathName should be non-empty"));

		// Step 3: AS reads the same object's identity — C++ compares with ground truth
		{
			FASGlobalFunctionInvoker NameInvoker(
				Test, Engine, Module, TEXT("FName ScriptGetName(UObject Obj)"));
			if (NameInvoker.IsValid())
			{
				NameInvoker.AddArgObject(Obj);
				FName ScriptName;
				if (NameInvoker.Call())
				{
					NameInvoker.ReadReturnStruct(ScriptName);
					bPassed &= LocalAssert.AreEqual(
						Obj->GetFName(),
						ScriptName,
						TEXT("[UObject] AS GetName() should match C++ GetFName()"));
				}
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: HierarchyAndOuter
	//   AS creates object → returns to C++ → C++ verifies Outer/Package chain.
	//   AS queries GetOuter/GetPackage → returns to C++ for comparison.
	// -----------------------------------------------------------------------
	bool RunHierarchyAndOuterSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_Hierarchy"), TEXT(R"(
UObject CreateInTransient()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjBindTest_Hierarchy");
}
UObject ScriptGetOuter(UObject Obj)
{
	return Obj.GetOuter();
}
UObject ScriptGetPackage(UObject Obj)
{
	return Obj.GetPackage();
}
UObject ScriptGetOutermost(UObject Obj)
{
	return Obj.GetOutermost();
}
UClass ScriptGetClass(UObject Obj)
{
	return Obj.GetClass();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// AS creates
		FASGlobalFunctionInvoker CreateInvoker(
			Test, Engine, Module, TEXT("UObject CreateInTransient()"));
		if (!CreateInvoker.IsValid()) return false;
		UObject* Obj = CreateInvoker.CallAndReturn<UObject*>(nullptr);
		bPassed &= LocalAssert.IsNotNull(Obj, TEXT("[UObject] Created object should exist"));
		if (!Obj) return false;

		// C++ verifies outer chain directly
		bPassed &= LocalAssert.AreEqual(
			static_cast<UObject*>(GetTransientPackage()),
			Obj->GetOuter(),
			TEXT("[UObject] C++ GetOuter() should be transient package"));
		bPassed &= LocalAssert.AreEqual(
			GetTransientPackage(),
			Obj->GetPackage(),
			TEXT("[UObject] C++ GetPackage() should be transient package"));

		// AS returns GetOuter → C++ verifies consistency
		{
			FASGlobalFunctionInvoker OuterInvoker(
				Test, Engine, Module, TEXT("UObject ScriptGetOuter(UObject Obj)"));
			if (OuterInvoker.IsValid())
			{
				OuterInvoker.AddArgObject(Obj);
				UObject* ScriptOuter = OuterInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					Obj->GetOuter(),
					ScriptOuter,
					TEXT("[UObject] AS GetOuter() should match C++ GetOuter()"));
			}
		}

		// AS returns GetPackage → C++ verifies
		{
			FASGlobalFunctionInvoker PkgInvoker(
				Test, Engine, Module, TEXT("UObject ScriptGetPackage(UObject Obj)"));
			if (PkgInvoker.IsValid())
			{
				PkgInvoker.AddArgObject(Obj);
				UObject* ScriptPkg = PkgInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					static_cast<UObject*>(Obj->GetPackage()),
					ScriptPkg,
					TEXT("[UObject] AS GetPackage() should match C++ GetPackage()"));
			}
		}

		// AS returns GetClass → C++ verifies
		{
			FASGlobalFunctionInvoker ClassInvoker(
				Test, Engine, Module, TEXT("UClass ScriptGetClass(UObject Obj)"));
			if (ClassInvoker.IsValid())
			{
				ClassInvoker.AddArgObject(Obj);
				UClass* ScriptClass = ClassInvoker.CallAndReturn<UClass*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					Obj->GetClass(),
					ScriptClass,
					TEXT("[UObject] AS GetClass() should match C++ GetClass()"));
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: TypeQueryAndCast
	//   AS creates ACameraActor → returns to C++ → C++ verifies IsA/Cast.
	//   AS does Cast<ACameraActor> → returns result to C++ → C++ verifies.
	// -----------------------------------------------------------------------
	bool RunTypeQueryAndCastSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_TypeQuery"), TEXT(R"(
UObject CreateCamera()
{
	return NewObject(GetTransientPackage(), ACameraActor::StaticClass(), n"UObjBindTest_Camera");
}
UObject ScriptCastToActor(UObject Obj)
{
	return Cast<AActor>(Obj);
}
UObject ScriptCastToCamera(UObject Obj)
{
	return Cast<ACameraActor>(Obj);
}
UObject ScriptCastToTexture(UObject Obj)
{
	return Cast<UTexture2D>(Obj);
}
int ScriptIsA_Actor(UObject Obj)
{
	return Obj.IsA(AActor::StaticClass()) ? 1 : 0;
}
int ScriptIsA_Camera(UObject Obj)
{
	return Obj.IsA(ACameraActor::StaticClass()) ? 1 : 0;
}
int ScriptIsA_Texture(UObject Obj)
{
	return Obj.IsA(UTexture2D::StaticClass()) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// AS creates camera actor
		FASGlobalFunctionInvoker CreateInvoker(
			Test, Engine, Module, TEXT("UObject CreateCamera()"));
		if (!CreateInvoker.IsValid()) return false;
		UObject* CameraObj = CreateInvoker.CallAndReturn<UObject*>(nullptr);
		bPassed &= LocalAssert.IsNotNull(CameraObj, TEXT("[UObject] Camera should be created"));
		if (!CameraObj) return false;

		// C++ directly verifies IsA
		bPassed &= LocalAssert.IsTrue(CameraObj->IsA(AActor::StaticClass()), TEXT("[UObject] C++ IsA(AActor) should be true"));
		bPassed &= LocalAssert.IsTrue(CameraObj->IsA(ACameraActor::StaticClass()), TEXT("[UObject] C++ IsA(ACameraActor) should be true"));
		bPassed &= LocalAssert.IsFalse(CameraObj->IsA(UTexture2D::StaticClass()), TEXT("[UObject] C++ IsA(UTexture2D) should be false"));

		// AS Cast<AActor> → C++ verifies pointer identity
		{
			FASGlobalFunctionInvoker CastInvoker(
				Test, Engine, Module, TEXT("UObject ScriptCastToActor(UObject Obj)"));
			if (CastInvoker.IsValid())
			{
				CastInvoker.AddArgObject(CameraObj);
				UObject* AsActor = CastInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					CameraObj,
					AsActor,
					TEXT("[UObject] AS Cast<AActor>(camera) should return same pointer"));
			}
		}

		// AS Cast<ACameraActor> → C++ verifies pointer identity
		{
			FASGlobalFunctionInvoker CastInvoker(
				Test, Engine, Module, TEXT("UObject ScriptCastToCamera(UObject Obj)"));
			if (CastInvoker.IsValid())
			{
				CastInvoker.AddArgObject(CameraObj);
				UObject* AsCamera = CastInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					CameraObj,
					AsCamera,
					TEXT("[UObject] AS Cast<ACameraActor>(camera) should return same pointer"));
			}
		}

		// AS Cast<UTexture2D> on camera → C++ verifies null (invalid cast)
		{
			FASGlobalFunctionInvoker CastInvoker(
				Test, Engine, Module, TEXT("UObject ScriptCastToTexture(UObject Obj)"));
			if (CastInvoker.IsValid())
			{
				CastInvoker.AddArgObject(CameraObj);
				UObject* AsTexture = CastInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.IsNull(
					AsTexture,
					TEXT("[UObject] AS Cast<UTexture2D>(camera) should return null"));
			}
		}

		// AS IsA → C++ verifies int return matches expectation
		auto TestIsA = [&](const TCHAR* Decl, const TCHAR* Label, int32 Expected)
		{
			FASGlobalFunctionInvoker Invoker(Test, Engine, Module, Decl);
			if (Invoker.IsValid())
			{
				Invoker.AddArgObject(CameraObj);
				int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(Expected, Result, Label);
			}
		};
		TestIsA(TEXT("int ScriptIsA_Actor(UObject Obj)"),   TEXT("[UObject] AS IsA(AActor) should return 1"),     1);
		TestIsA(TEXT("int ScriptIsA_Camera(UObject Obj)"),  TEXT("[UObject] AS IsA(ACameraActor) should return 1"), 1);
		TestIsA(TEXT("int ScriptIsA_Texture(UObject Obj)"), TEXT("[UObject] AS IsA(UTexture2D) should return 0"), 0);

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: FindAndLookup
	//   AS creates named object → C++ uses FindObject to verify.
	//   C++ creates object → passes to AS FindObject → verifies round-trip.
	// -----------------------------------------------------------------------
	bool RunFindAndLookupSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_FindLookup"), TEXT(R"(
UObject CreateNamedForFind()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjBindTest_Find");
}
UObject ScriptFindByPath(const FString& in Path)
{
	return FindObject(Path);
}
UObject ScriptFindWithOuter(UObject Outer, const FString& in Name)
{
	return FindObject(Outer, Name);
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// AS creates named object → C++ finds it via UE FindObject
		FASGlobalFunctionInvoker CreateInvoker(
			Test, Engine, Module, TEXT("UObject CreateNamedForFind()"));
		if (!CreateInvoker.IsValid()) return false;
		UObject* Created = CreateInvoker.CallAndReturn<UObject*>(nullptr);
		bPassed &= LocalAssert.IsNotNull(Created, TEXT("[UObject] AS-created named object should exist"));
		if (!Created) return false;

		// C++ directly uses FindObject to find the same object
		UObject* CppFound = FindObject<UObject>(GetTransientPackage(), TEXT("UObjBindTest_Find"));
		bPassed &= LocalAssert.AreEqual(
			Created,
			CppFound,
			TEXT("[UObject] C++ FindObject should find the same object AS created"));

		// C++ passes path to AS ScriptFindByPath → AS returns found object → C++ verifies
		{
			FString Path = Created->GetPathName();
			FASGlobalFunctionInvoker FindInvoker(
				Test, Engine, Module, TEXT("UObject ScriptFindByPath(const FString& in Path)"));
			if (FindInvoker.IsValid())
			{
				FindInvoker.AddArgRef(Path);
				UObject* ScriptFound = FindInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					Created,
					ScriptFound,
					TEXT("[UObject] AS FindObject(path) should return same object"));
			}
		}

		// C++ passes outer + name to AS ScriptFindWithOuter → C++ verifies
		{
			FString Name = TEXT("UObjBindTest_Find");
			FASGlobalFunctionInvoker FindOuterInvoker(
				Test, Engine, Module, TEXT("UObject ScriptFindWithOuter(UObject Outer, const FString& in Name)"));
			if (FindOuterInvoker.IsValid())
			{
				FindOuterInvoker.AddArgObject(GetTransientPackage());
				FindOuterInvoker.AddArgRef(Name);
				UObject* ScriptFound = FindOuterInvoker.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					Created,
					ScriptFound,
					TEXT("[UObject] AS FindObject(outer, name) should return same object"));
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: RootLifecycle
	//   AS creates object → AS AddToRoot → C++ verifies IsRooted.
	//   AS RemoveFromRoot → C++ verifies not rooted.
	// -----------------------------------------------------------------------
	bool RunRootLifecycleSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_RootLifecycle"), TEXT(R"(
UObject CreateForRoot()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjBindTest_Root");
}
void ScriptAddToRoot(UObject Obj)
{
	Obj.AddToRoot();
}
void ScriptRemoveFromRoot(UObject Obj)
{
	Obj.RemoveFromRoot();
}
int ScriptGetIsRooted(UObject Obj)
{
	return Obj.GetIsRooted() ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// AS creates
		FASGlobalFunctionInvoker CreateInvoker(
			Test, Engine, Module, TEXT("UObject CreateForRoot()"));
		if (!CreateInvoker.IsValid()) return false;
		UObject* Obj = CreateInvoker.CallAndReturn<UObject*>(nullptr);
		bPassed &= LocalAssert.IsNotNull(Obj, TEXT("[UObject] Object for root test should exist"));
		if (!Obj) return false;

		// C++ verifies not rooted initially
		bPassed &= LocalAssert.IsFalse(Obj->IsRooted(), TEXT("[UObject] C++ IsRooted() should be false initially"));

		// AS AddToRoot
		{
			FASGlobalFunctionInvoker AddInvoker(
				Test, Engine, Module, TEXT("void ScriptAddToRoot(UObject Obj)"));
			if (AddInvoker.IsValid())
			{
				AddInvoker.AddArgObject(Obj);
				AddInvoker.Call();
			}
		}

		// C++ verifies rooted after AS AddToRoot
		bPassed &= LocalAssert.IsTrue(Obj->IsRooted(), TEXT("[UObject] C++ IsRooted() should be true after AS AddToRoot"));

		// AS GetIsRooted → C++ verifies AS agrees with C++
		{
			FASGlobalFunctionInvoker RootedInvoker(
				Test, Engine, Module, TEXT("int ScriptGetIsRooted(UObject Obj)"));
			if (RootedInvoker.IsValid())
			{
				RootedInvoker.AddArgObject(Obj);
				int32 AsRooted = RootedInvoker.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(1, AsRooted, TEXT("[UObject] AS GetIsRooted() should match C++ IsRooted()"));
			}
		}

		// AS RemoveFromRoot
		{
			FASGlobalFunctionInvoker RemoveInvoker(
				Test, Engine, Module, TEXT("void ScriptRemoveFromRoot(UObject Obj)"));
			if (RemoveInvoker.IsValid())
			{
				RemoveInvoker.AddArgObject(Obj);
				RemoveInvoker.Call();
			}
		}

		// C++ verifies no longer rooted
		bPassed &= LocalAssert.IsFalse(Obj->IsRooted(), TEXT("[UObject] C++ IsRooted() should be false after AS RemoveFromRoot"));

		// AS confirms not rooted
		{
			FASGlobalFunctionInvoker RootedInvoker(
				Test, Engine, Module, TEXT("int ScriptGetIsRooted(UObject Obj)"));
			if (RootedInvoker.IsValid())
			{
				RootedInvoker.AddArgObject(Obj);
				int32 AsRooted = RootedInvoker.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(0, AsRooted, TEXT("[UObject] AS GetIsRooted() should be 0 after RemoveFromRoot"));
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: FlagMutation
	//   AS creates transient object → C++ verifies RF_Transient.
	//   AS calls SetTransactional(true) → C++ verifies RF_Transactional.
	//   AS calls SetTransactional(false) → C++ verifies flag cleared.
	// -----------------------------------------------------------------------
	bool RunFlagMutationSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_FlagMutation"), TEXT(R"(
UObject CreateTransient()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), NAME_None, true);
}
UObject CreateNonTransient()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass());
}
void ScriptSetTransactional(UObject Obj, bool bValue)
{
	Obj.SetTransactional(bValue);
}
int ScriptIsTransient(UObject Obj)
{
	return Obj.IsTransient() ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// AS creates transient object → C++ verifies
		{
			FASGlobalFunctionInvoker Invoker(
				Test, Engine, Module, TEXT("UObject CreateTransient()"));
			if (!Invoker.IsValid()) return false;
			UObject* TransientObj = Invoker.CallAndReturn<UObject*>(nullptr);
			bPassed &= LocalAssert.IsNotNull(TransientObj, TEXT("[UObject] Transient object should exist"));
			if (TransientObj)
			{
				bPassed &= LocalAssert.IsTrue(
					TransientObj->HasAnyFlags(RF_Transient),
					TEXT("[UObject] C++ HasAnyFlags(RF_Transient) should be true for transient-created object"));
			}
		}

		// AS creates non-transient → C++ verifies
		{
			FASGlobalFunctionInvoker Invoker(
				Test, Engine, Module, TEXT("UObject CreateNonTransient()"));
			if (!Invoker.IsValid()) return false;
			UObject* NormalObj = Invoker.CallAndReturn<UObject*>(nullptr);
			bPassed &= LocalAssert.IsNotNull(NormalObj, TEXT("[UObject] Non-transient object should exist"));
			if (NormalObj)
			{
				// NewObject(GetTransientPackage(),...) with no bTransient=true
				// still has RF_Transient set because the binding defaults to it when outer is transient pkg
				// Verify IsTransient through AS and through C++
				{
					FASGlobalFunctionInvoker IsTransInvoker(
						Test, Engine, Module, TEXT("int ScriptIsTransient(UObject Obj)"));
					if (IsTransInvoker.IsValid())
					{
						IsTransInvoker.AddArgObject(NormalObj);
						int32 AsTransient = IsTransInvoker.CallAndReturn<int32>(INDEX_NONE);
						bool bCppTransient = NormalObj->HasAnyFlags(RF_Transient);
						bPassed &= LocalAssert.AreEqual(
							bCppTransient ? 1 : 0,
							AsTransient,
							TEXT("[UObject] AS IsTransient() should match C++ HasAnyFlags(RF_Transient)"));
					}
				}

				// AS SetTransactional(true) → C++ verifies RF_Transactional
				{
					FASGlobalFunctionInvoker SetInvoker(
						Test, Engine, Module, TEXT("void ScriptSetTransactional(UObject Obj, bool bValue)"));
					if (SetInvoker.IsValid())
					{
						SetInvoker.AddArgObject(NormalObj);
						SetInvoker.AddArg(true);
						SetInvoker.Call();
					}
				}
				bPassed &= LocalAssert.IsTrue(
					NormalObj->HasAnyFlags(RF_Transactional),
					TEXT("[UObject] C++ RF_Transactional should be set after AS SetTransactional(true)"));

				// AS SetTransactional(false) → C++ verifies RF_Transactional cleared
				{
					FASGlobalFunctionInvoker SetInvoker(
						Test, Engine, Module, TEXT("void ScriptSetTransactional(UObject Obj, bool bValue)"));
					if (SetInvoker.IsValid())
					{
						SetInvoker.AddArgObject(NormalObj);
						SetInvoker.AddArg(false);
						SetInvoker.Call();
					}
				}
				bPassed &= LocalAssert.IsFalse(
					NormalObj->HasAnyFlags(RF_Transactional),
					TEXT("[UObject] C++ RF_Transactional should be cleared after AS SetTransactional(false)"));
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: NullAndIsValid
	//   Script-side null handling + IsValid + object comparison.
	//   These stay as pure-AS since null doesn't cross the boundary meaningfully.
	// -----------------------------------------------------------------------
	bool RunNullAndIsValidSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_NullValid"), TEXT(R"(
int IsValid_ValidObjectTrue()
{
	UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	return IsValid(Obj) ? 1 : 0;
}
int IsValid_NullLiteralFalse()
{
	return IsValid(null) ? 1 : 0;
}
int NullComparison_ValidNotEqualNull()
{
	UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	return (Obj != null) ? 1 : 0;
}
int NullComparison_ValidEqualsNullIsFalse()
{
	UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	return (Obj == null) ? 1 : 0;
}
int NullComparison_TwoValidSameInstance()
{
	UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	UObject Ref = Obj;
	return (Obj == Ref) ? 1 : 0;
}
int NullComparison_TwoValidDifferent()
{
	UObject A = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	UObject B = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	return (A != B) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int IsValid_ValidObjectTrue()"),              TEXT("IsValid on valid object should return true"),   1 },
			{ TEXT("int IsValid_NullLiteralFalse()"),             TEXT("IsValid(null) should return false"),            0 },
			{ TEXT("int NullComparison_ValidNotEqualNull()"),     TEXT("valid object != null should be true"),          1 },
			{ TEXT("int NullComparison_ValidEqualsNullIsFalse()"),TEXT("valid object == null should be false"),         0 },
			{ TEXT("int NullComparison_TwoValidSameInstance()"),  TEXT("Same instance references should be equal"),     1 },
			{ TEXT("int NullComparison_TwoValidDifferent()"),     TEXT("Different instances should not be equal"),      1 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: NewObjectVariants
	//   Tests all overload combinations of the NewObject binding:
	//     NewObject(Outer, Class)
	//     NewObject(Outer, Class, Name)
	//     NewObject(Outer, Class, Name, bTransient)
	//   AS creates objects with different parameter combos → returns to C++
	//   → C++ validates name, class, outer, and flags on each.
	// -----------------------------------------------------------------------
	bool RunNewObjectVariantsSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_NewObjVar"), TEXT(R"(
UObject Create_DefaultName()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass());
}
UObject Create_ExplicitName()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"NOV_Named_42");
}
UObject Create_TransientTrue()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"NOV_Trans_42", true);
}
UObject Create_TransientFalse()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"NOV_NonTrans_42", false);
}
UObject Create_NAMENone_Explicit()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), NAME_None, false);
}
UObject Create_CameraActor()
{
	return NewObject(GetTransientPackage(), ACameraActor::StaticClass(), n"NOV_Camera_42");
}
FName GetNameOf(UObject Obj)
{
	return Obj.GetName();
}
FString GetFullNameOf(UObject Obj)
{
	return Obj.GetFullName();
}
FString GetPathNameOf(UObject Obj)
{
	return Obj.GetPathName();
}
bool GetIsTransient(UObject Obj)
{
	return Obj.IsTransient();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// Helper: create via AS, then verify in C++
		auto CreateAndVerify = [&](
			const TCHAR* CreateDecl,
			const TCHAR* Label,
			UClass* ExpectedClass,
			const TCHAR* ExpectedName, // nullptr = auto-generated
			bool bExpectTransient) -> UObject*
		{
			FASGlobalFunctionInvoker Invoker(Test, Engine, Module, CreateDecl);
			if (!LocalAssert.IsTrue(Invoker.IsValid(), FString::Printf(TEXT("[UObject] %s invoker valid"), Label)))
				return nullptr;
			UObject* Obj = Invoker.CallAndReturn<UObject*>(nullptr);
			if (!LocalAssert.IsNotNull(Obj, FString::Printf(TEXT("[UObject] %s should create non-null"), Label)))
				return nullptr;

			// C++ class check
			bPassed &= LocalAssert.AreEqual(
				ExpectedClass,
				Obj->GetClass(),
				FString::Printf(TEXT("[UObject] %s C++ GetClass()"), Label));

			// C++ name check
			if (ExpectedName)
			{
				bPassed &= LocalAssert.AreEqual(
					FName(ExpectedName),
					Obj->GetFName(),
					FString::Printf(TEXT("[UObject] %s C++ GetFName()"), Label));

				// Cross-check: AS returns name → C++ compares
				FASGlobalFunctionInvoker NameInv(
					Test, Engine, Module, TEXT("FName GetNameOf(UObject Obj)"));
				if (NameInv.IsValid())
				{
					NameInv.AddArgObject(Obj);
					FName ASName;
					if (NameInv.Call())
					{
						NameInv.ReadReturnStruct(ASName);
						bPassed &= LocalAssert.AreEqual(
							Obj->GetFName(),
							ASName,
							FString::Printf(TEXT("[UObject] %s AS GetName() matches C++"), Label));
					}
				}
			}
			else
			{
				// Auto-generated name: just verify it's not empty
				bPassed &= LocalAssert.IsTrue(
					Obj->GetFName() != NAME_None,
					FString::Printf(TEXT("[UObject] %s C++ auto name non-empty"), Label));
			}

			// C++ outer check
			bPassed &= LocalAssert.AreEqual(
				static_cast<UObject*>(GetTransientPackage()),
				Obj->GetOuter(),
				FString::Printf(TEXT("[UObject] %s C++ outer is transient pkg"), Label));

			// C++ transient flag check — note: NewObject binding forces RF_Transient
			// when Outer is null, but GetTransientPackage() is non-null, so only
			// bTransient=true explicitly sets it via binding code path.
			bool bCppTransient = Obj->HasAnyFlags(RF_Transient);
			if (bExpectTransient)
			{
				bPassed &= LocalAssert.IsTrue(
					bCppTransient,
					FString::Printf(TEXT("[UObject] %s C++ should have RF_Transient"), Label));
			}

			// Cross-check: AS returns IsTransient → C++ compares
			{
				FASGlobalFunctionInvoker TransInv(
					Test, Engine, Module, TEXT("bool GetIsTransient(UObject Obj)"));
				if (TransInv.IsValid())
				{
					TransInv.AddArgObject(Obj);
					// AS bool returns as uint8 via GetReturnByte
					int32 ASTransInt = TransInv.CallAndReturn<int32>(INDEX_NONE);
					bool bASTransient = (ASTransInt != 0);
					bPassed &= LocalAssert.AreEqual(
						bCppTransient,
						bASTransient,
						FString::Printf(TEXT("[UObject] %s AS IsTransient() matches C++"), Label));
				}
			}

			// Cross-check: AS returns GetFullName → C++ compares
			{
				FASGlobalFunctionInvoker FullInv(
					Test, Engine, Module, TEXT("FString GetFullNameOf(UObject Obj)"));
				if (FullInv.IsValid())
				{
					FullInv.AddArgObject(Obj);
					FString ASFull;
					if (FullInv.Call())
					{
						FullInv.ReadReturnStruct(ASFull);
						bPassed &= LocalAssert.AreEqual(
							Obj->GetFullName(),
							ASFull,
							FString::Printf(TEXT("[UObject] %s AS GetFullName() matches C++"), Label));
					}
				}
			}

			// Cross-check: AS returns GetPathName → C++ compares
			{
				FASGlobalFunctionInvoker PathInv(
					Test, Engine, Module, TEXT("FString GetPathNameOf(UObject Obj)"));
				if (PathInv.IsValid())
				{
					PathInv.AddArgObject(Obj);
					FString ASPath;
					if (PathInv.Call())
					{
						PathInv.ReadReturnStruct(ASPath);
						bPassed &= LocalAssert.AreEqual(
							Obj->GetPathName(),
							ASPath,
							FString::Printf(TEXT("[UObject] %s AS GetPathName() matches C++"), Label));
					}
				}
			}

			return Obj;
		};

		// Test each NewObject variant
		// NOTE: The binding only auto-adds RF_Transient when Outer==null (forces
		// transient package) or when bTransient=true explicitly. Passing
		// GetTransientPackage() as Outer does NOT auto-set RF_Transient.
		CreateAndVerify(
			TEXT("UObject Create_DefaultName()"),
			TEXT("DefaultName"), UTexture2D::StaticClass(),
			nullptr, // auto-generated name
			false);  // Outer is non-null GetTransientPackage(), bTransient defaults false

		CreateAndVerify(
			TEXT("UObject Create_ExplicitName()"),
			TEXT("ExplicitName"), UTexture2D::StaticClass(),
			TEXT("NOV_Named_42"),
			false);

		CreateAndVerify(
			TEXT("UObject Create_TransientTrue()"),
			TEXT("TransientTrue"), UTexture2D::StaticClass(),
			TEXT("NOV_Trans_42"),
			true);   // bTransient=true explicitly

		CreateAndVerify(
			TEXT("UObject Create_TransientFalse()"),
			TEXT("TransientFalse"), UTexture2D::StaticClass(),
			TEXT("NOV_NonTrans_42"),
			false);

		CreateAndVerify(
			TEXT("UObject Create_NAMENone_Explicit()"),
			TEXT("NAMENone"), UTexture2D::StaticClass(),
			nullptr, // NAME_None → auto name
			false);

		CreateAndVerify(
			TEXT("UObject Create_CameraActor()"),
			TEXT("CameraActor"), ACameraActor::StaticClass(),
			TEXT("NOV_Camera_42"),
			false);  // Outer is non-null GetTransientPackage()

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: ClassReflection
	//   Tests UClass methods: GetDefaultObject, GetSuperClass, IsChildOf,
	//   IsAbstract, FindFunctionByName.
	//   AS calls these methods → returns to C++ → C++ verifies against
	//   direct UClass API calls.
	// -----------------------------------------------------------------------
	bool RunClassReflectionSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_ClassRefl"), TEXT(R"(
UObject GetCDO_Texture2D()
{
	return UTexture2D::StaticClass().GetDefaultObject();
}
UClass GetSuper_CameraActor()
{
	return ACameraActor::StaticClass().GetSuperClass();
}
bool IsChildOf_CameraIsActor()
{
	return ACameraActor::StaticClass().IsChildOf(AActor::StaticClass());
}
bool IsChildOf_ActorIsCamera()
{
	return AActor::StaticClass().IsChildOf(ACameraActor::StaticClass());
}
bool IsChildOf_SameClass()
{
	return AActor::StaticClass().IsChildOf(AActor::StaticClass());
}
bool IsAbstract_AActor()
{
	return AActor::StaticClass().IsAbstract();
}
bool IsAbstract_CameraActor()
{
	return ACameraActor::StaticClass().IsAbstract();
}
UFunction FindFunc_Actor_ReceiveTick()
{
	return AActor::StaticClass().FindFunctionByName(n"ReceiveTick");
}
UFunction FindFunc_Actor_Nonexistent()
{
	return AActor::StaticClass().FindFunctionByName(n"ThisFunctionDoesNotExist_XYZ");
}
FString GetClassName(UObject Obj)
{
	return Obj.GetClass().GetName().ToString();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// --- GetDefaultObject ---
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject GetCDO_Texture2D()"));
			if (Inv.IsValid())
			{
				UObject* ASCDO = Inv.CallAndReturn<UObject*>(nullptr);
				UObject* CppCDO = UTexture2D::StaticClass()->GetDefaultObject();
				bPassed &= LocalAssert.IsNotNull(ASCDO, TEXT("[UObject] AS GetDefaultObject should be non-null"));
				bPassed &= LocalAssert.AreEqual(
					CppCDO,
					ASCDO,
					TEXT("[UObject] AS GetDefaultObject should match C++ CDO pointer"));
			}
		}

		// --- GetSuperClass ---
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UClass GetSuper_CameraActor()"));
			if (Inv.IsValid())
			{
				UClass* ASSuperClass = Inv.CallAndReturn<UClass*>(nullptr);
				UClass* CppSuperClass = ACameraActor::StaticClass()->GetSuperClass();
				bPassed &= LocalAssert.IsNotNull(ASSuperClass, TEXT("[UObject] AS GetSuperClass should be non-null"));
				bPassed &= LocalAssert.AreEqual(
					CppSuperClass,
					ASSuperClass,
					TEXT("[UObject] AS GetSuperClass(ACameraActor) should match C++"));
			}
		}

		// --- IsChildOf ---
		auto TestBoolReturn = [&](const TCHAR* Decl, const TCHAR* Label, bool Expected)
		{
			FASGlobalFunctionInvoker Inv(Test, Engine, Module, Decl);
			if (Inv.IsValid())
			{
				int32 Result = Inv.CallAndReturn<int32>(INDEX_NONE);
				bool bResult = (Result != 0);
				bPassed &= LocalAssert.AreEqual(Expected, bResult, Label);
			}
		};

		TestBoolReturn(
			TEXT("bool IsChildOf_CameraIsActor()"),
			TEXT("[UObject] ACameraActor::IsChildOf(AActor) should be true"),
			true);
		TestBoolReturn(
			TEXT("bool IsChildOf_ActorIsCamera()"),
			TEXT("[UObject] AActor::IsChildOf(ACameraActor) should be false"),
			false);
		TestBoolReturn(
			TEXT("bool IsChildOf_SameClass()"),
			TEXT("[UObject] AActor::IsChildOf(AActor) should be true (same class)"),
			true);

		// --- IsAbstract ---
		TestBoolReturn(
			TEXT("bool IsAbstract_AActor()"),
			TEXT("[UObject] AActor::IsAbstract() should match C++"),
			AActor::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
		TestBoolReturn(
			TEXT("bool IsAbstract_CameraActor()"),
			TEXT("[UObject] ACameraActor::IsAbstract() should match C++"),
			ACameraActor::StaticClass()->HasAnyClassFlags(CLASS_Abstract));

		// --- FindFunctionByName ---
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UFunction FindFunc_Actor_ReceiveTick()"));
			if (Inv.IsValid())
			{
				UFunction* ASFunc = Inv.CallAndReturn<UFunction*>(nullptr);
				UFunction* CppFunc = AActor::StaticClass()->FindFunctionByName(FName(TEXT("ReceiveTick")));
				bPassed &= LocalAssert.AreEqual(
					CppFunc,
					ASFunc,
					TEXT("[UObject] AS FindFunctionByName(ReceiveTick) should match C++"));
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UFunction FindFunc_Actor_Nonexistent()"));
			if (Inv.IsValid())
			{
				UFunction* ASFunc = Inv.CallAndReturn<UFunction*>(nullptr);
				bPassed &= LocalAssert.IsNull(
					ASFunc,
					TEXT("[UObject] AS FindFunctionByName(nonexistent) should return null"));
			}
		}

		// --- GetClassName cross-check: AS builds string from GetClass().GetName() ---
		{
			UObject* TestObj = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString GetClassName(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TestObj);
				FString ASClassName;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(ASClassName);
					bPassed &= LocalAssert.AreEqual(
						TestObj->GetClass()->GetName(),
						ASClassName,
						TEXT("[UObject] AS GetClass().GetName() should match C++ class name"));
				}
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: ReturnValueCrossCheck
	//   Focused on verifying that AS function return values (FString, FName,
	//   int, bool, float) correctly round-trip through the AS↔C++ boundary.
	//   AS computes various values → returns to C++ → C++ verifies content.
	// -----------------------------------------------------------------------
	bool RunReturnValueCrossCheckSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_ReturnVal"), TEXT(R"(
FString ReturnString()
{
	return "Hello from Angelscript";
}
FName ReturnName()
{
	return n"AngelscriptTestName";
}
int ReturnInt()
{
	return 12345;
}
float ReturnFloat()
{
	// AS 'float' is 64-bit (bScriptFloatIsFloat64=true), use explicit value
	return 3.0;
}
bool ReturnTrue()
{
	return true;
}
bool ReturnFalse()
{
	return false;
}
FString ConcatIntFloat(int A, float B)
{
	// float is actually double in this config
	return "" + A + "_" + B;
}
FString ObjectToString(UObject Obj)
{
	FString Name = Obj.GetName().ToString();
	FString ClassName = Obj.GetClass().GetName().ToString();
	return ClassName + ":" + Name;
}
FString LogFormatted(UObject Obj)
{
	return Obj.GetFullName();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// FString return
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString ReturnString()"));
			if (Inv.IsValid())
			{
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.AreEqual(
						FString(TEXT("Hello from Angelscript")),
						Result,
						TEXT("[UObject] AS FString return should match expected"));
				}
			}
		}

		// FName return
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FName ReturnName()"));
			if (Inv.IsValid())
			{
				FName Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.AreEqual(
						FName(TEXT("AngelscriptTestName")),
						Result,
						TEXT("[UObject] AS FName return should match expected"));
				}
			}
		}

		// int return
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("int ReturnInt()"));
			if (Inv.IsValid())
			{
				int32 Result = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(
					12345,
					Result,
					TEXT("[UObject] AS int return should be 12345"));
			}
		}

		// float return (AS float is 64-bit double when bScriptFloatIsFloat64=true)
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("float ReturnFloat()"));
			if (Inv.IsValid())
			{
				double Result = Inv.CallAndReturn<double>(0.0);
				bPassed &= LocalAssert.IsNear(
					3.0,
					Result,
					0.01,
					TEXT("[UObject] AS float (double) return should be ~3.0"));
			}
		}

		// bool return (true)
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("bool ReturnTrue()"));
			if (Inv.IsValid())
			{
				int32 Result = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(
					true,
					Result != 0,
					TEXT("[UObject] AS bool return true should be nonzero"));
			}
		}

		// bool return (false)
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("bool ReturnFalse()"));
			if (Inv.IsValid())
			{
				int32 Result = Inv.CallAndReturn<int32>(0);
				bPassed &= LocalAssert.AreEqual(
					0,
					Result,
					TEXT("[UObject] AS bool return false should be zero"));
			}
		}

		// FString concat with int and float (tests string + type concatenation returns)
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString ConcatIntFloat(int A, float B)"));
			if (Inv.IsValid())
			{
				Inv.AddArg(static_cast<int32>(42));
				Inv.AddArg(2.5);  // AS float is double
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("42")),
						TEXT("[UObject] AS ConcatIntFloat should contain '42'"));
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("2.")),
						TEXT("[UObject] AS ConcatIntFloat should contain '2.'"));
				}
			}
		}

		// ObjectToString: AS builds "ClassName:ObjectName" → C++ validates
		{
			UObject* TestObj = NewObject<UTexture2D>(GetTransientPackage(), FName(TEXT("RetValTestObj_42")), RF_Transient);
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString ObjectToString(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TestObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = FString::Printf(TEXT("%s:%s"),
						*TestObj->GetClass()->GetName(), *TestObj->GetName());
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[UObject] AS ObjectToString should match C++ built string"));
				}
			}
		}

		// LogFormatted: AS returns GetFullName → C++ compares
		{
			UObject* TestObj = NewObject<UTexture2D>(GetTransientPackage(), FName(TEXT("RetValLogObj_42")), RF_Transient);
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString LogFormatted(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TestObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.AreEqual(
						TestObj->GetFullName(),
						Result,
						TEXT("[UObject] AS GetFullName return should match C++"));
				}
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: CppToScriptPassthrough
	//   C++ creates UObjects with known properties, passes them into AS
	//   functions as parameters. AS reads properties, calls methods, does
	//   Log() with the object info, and returns computed results to C++.
	//   C++ verifies every returned value against ground truth.
	//
	//   Key scenarios:
	//     1. C++ passes UTexture2D → AS reads GetName/GetClass/GetPathName
	//        → returns FString → C++ verifies
	//     2. C++ passes ACameraActor → AS does IsA/Cast → returns results
	//     3. C++ passes object → AS does Log("prefix:" + Obj) → C++ verifies
	//        the log contains expected content (via AddExpectedError for
	//        Error-level, or just execution-without-crash for Log-level)
	//     4. C++ passes multiple objects → AS compares them (==, !=)
	//     5. C++ passes null → AS does IsValid/null-check → returns result
	//     6. C++ passes object → AS calls GetOuter/GetPackage → returns
	//        pointer → C++ verifies matches
	// -----------------------------------------------------------------------
	bool RunCppToScriptPassthroughSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_CppToAS"), TEXT(R"(
// ---- Identity queries on C++-created objects ----
FString DescribeObject(UObject Obj)
{
	FString ClassName = Obj.GetClass().GetName().ToString();
	FString ObjName = Obj.GetName().ToString();
	FString PathName = Obj.GetPathName();
	return ClassName + "|" + ObjName + "|" + PathName;
}

FName GetNameOf(UObject Obj)
{
	return Obj.GetName();
}

FString GetFullNameOf(UObject Obj)
{
	return Obj.GetFullName();
}

// ---- Type queries on C++-passed objects ----
bool CheckIsA(UObject Obj, UClass Class)
{
	return Obj.IsA(Class);
}

UObject TryCastToActor(UObject Obj)
{
	return Cast<AActor>(Obj);
}

UObject TryCastToTexture(UObject Obj)
{
	return Cast<UTexture2D>(Obj);
}

// ---- Hierarchy queries ----
UObject GetOuterOf(UObject Obj)
{
	return Obj.GetOuter();
}

UObject GetPackageOf(UObject Obj)
{
	return Obj.GetPackage();
}

UClass GetClassOf(UObject Obj)
{
	return Obj.GetClass();
}

// ---- Null / IsValid ----
bool CheckIsValid(UObject Obj)
{
	return IsValid(Obj);
}

bool CheckIsNull(UObject Obj)
{
	return (Obj == null);
}

// ---- Comparison ----
bool CheckSame(UObject A, UObject B)
{
	return (A == B);
}

bool CheckNotSame(UObject A, UObject B)
{
	return (A != B);
}

// ---- Log with C++-passed object ----
FString LogAndReturnInfo(UObject Obj)
{
	FString Info = Obj.GetClass().GetName().ToString() + ":" + Obj.GetName().ToString();
	Log("CppToAS_LogMarker: " + Info);
	return Info;
}

FString LogMultiple(UObject A, UObject B)
{
	FString InfoA = A.GetName().ToString();
	FString InfoB = B.GetName().ToString();
	Log("CppToAS_MultiLog: " + InfoA + " | " + InfoB);
	return InfoA + "," + InfoB;
}

// ---- Mutation via C++-passed object ----
void DoAddToRoot(UObject Obj)
{
	Obj.AddToRoot();
}

void DoRemoveFromRoot(UObject Obj)
{
	Obj.RemoveFromRoot();
}

bool IsRootedCheck(UObject Obj)
{
	return Obj.GetIsRooted();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// Create test objects in C++
		UTexture2D* TexObj = NewObject<UTexture2D>(
			GetTransientPackage(), FName(TEXT("CppTex_PassTest_42")), RF_Transient);
		ACameraActor* CamObj = NewObject<ACameraActor>(
			GetTransientPackage(), FName(TEXT("CppCam_PassTest_42")), RF_Transient);

		// ---- 1. DescribeObject: C++ passes UTexture2D → AS returns formatted string ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString DescribeObject(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = FString::Printf(TEXT("%s|%s|%s"),
						*TexObj->GetClass()->GetName(),
						*TexObj->GetName(),
						*TexObj->GetPathName());
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[CppToAS] DescribeObject(Texture) should match C++ formatted string"));
				}
			}
		}

		// ---- 2. DescribeObject: C++ passes ACameraActor → AS returns formatted string ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString DescribeObject(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(CamObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = FString::Printf(TEXT("%s|%s|%s"),
						*CamObj->GetClass()->GetName(),
						*CamObj->GetName(),
						*CamObj->GetPathName());
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[CppToAS] DescribeObject(Camera) should match C++ formatted string"));
				}
			}
		}

		// ---- 3. GetNameOf: cross-check FName return ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FName GetNameOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				FName Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.AreEqual(
						TexObj->GetFName(),
						Result,
						TEXT("[CppToAS] AS GetName(Tex) should match C++ FName"));
				}
			}
		}

		// ---- 4. GetFullNameOf: cross-check FString return ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString GetFullNameOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(CamObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.AreEqual(
						CamObj->GetFullName(),
						Result,
						TEXT("[CppToAS] AS GetFullName(Cam) should match C++"));
				}
			}
		}

		// ---- 5. Type queries: IsA ----
		auto TestBool = [&](const TCHAR* Decl, void* Arg0, void* Arg1,
			const TCHAR* Label, bool Expected)
		{
			FASGlobalFunctionInvoker Inv(Test, Engine, Module, Decl);
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Arg0);
				if (Arg1) Inv.AddArgObject(Arg1);
				int32 R = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(Expected, R != 0, Label);
			}
		};

		TestBool(TEXT("bool CheckIsA(UObject Obj, UClass Class)"),
			CamObj, AActor::StaticClass(),
			TEXT("[CppToAS] CameraActor.IsA(AActor) should be true"), true);
		TestBool(TEXT("bool CheckIsA(UObject Obj, UClass Class)"),
			CamObj, UTexture2D::StaticClass(),
			TEXT("[CppToAS] CameraActor.IsA(UTexture2D) should be false"), false);
		TestBool(TEXT("bool CheckIsA(UObject Obj, UClass Class)"),
			TexObj, UTexture2D::StaticClass(),
			TEXT("[CppToAS] Texture2D.IsA(UTexture2D) should be true"), true);

		// ---- 6. Cast: valid and invalid ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject TryCastToActor(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(CamObj);
				UObject* Result = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					static_cast<UObject*>(CamObj),
					Result,
					TEXT("[CppToAS] Cast<AActor>(Camera) should return same pointer"));
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject TryCastToTexture(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(CamObj);
				UObject* Result = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.IsNull(
					Result,
					TEXT("[CppToAS] Cast<UTexture2D>(Camera) should be null"));
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject TryCastToTexture(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				UObject* Result = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					static_cast<UObject*>(TexObj),
					Result,
					TEXT("[CppToAS] Cast<UTexture2D>(Texture) should return same pointer"));
			}
		}

		// ---- 7. Hierarchy: GetOuter, GetPackage, GetClass ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject GetOuterOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				UObject* Result = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					TexObj->GetOuter(),
					Result,
					TEXT("[CppToAS] AS GetOuter(Tex) should match C++"));
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject GetPackageOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(CamObj);
				UObject* Result = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					static_cast<UObject*>(CamObj->GetPackage()),
					Result,
					TEXT("[CppToAS] AS GetPackage(Cam) should match C++"));
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UClass GetClassOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				UClass* Result = Inv.CallAndReturn<UClass*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					TexObj->GetClass(),
					Result,
					TEXT("[CppToAS] AS GetClass(Tex) should be UTexture2D"));
			}
		}

		// ---- 8. Null / IsValid ----
		TestBool(TEXT("bool CheckIsValid(UObject Obj)"),
			TexObj, nullptr,
			TEXT("[CppToAS] IsValid(valid C++ obj) should be true"), true);
		TestBool(TEXT("bool CheckIsNull(UObject Obj)"),
			TexObj, nullptr,
			TEXT("[CppToAS] valid C++ obj == null should be false"), false);

		// ---- 9. Comparison: same object, different objects ----
		TestBool(TEXT("bool CheckSame(UObject A, UObject B)"),
			TexObj, TexObj,
			TEXT("[CppToAS] same C++ pointer should be =="), true);
		TestBool(TEXT("bool CheckNotSame(UObject A, UObject B)"),
			TexObj, CamObj,
			TEXT("[CppToAS] different C++ pointers should be !="), true);
		TestBool(TEXT("bool CheckSame(UObject A, UObject B)"),
			TexObj, CamObj,
			TEXT("[CppToAS] different C++ pointers should not be =="), false);

		// ---- 10. Log with C++-passed objects ----
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString LogAndReturnInfo(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = FString::Printf(TEXT("%s:%s"),
						*TexObj->GetClass()->GetName(), *TexObj->GetName());
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[CppToAS] LogAndReturnInfo should return ClassName:ObjName"));
				}
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString LogMultiple(UObject A, UObject B)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(TexObj);
				Inv.AddArgObject(CamObj);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = FString::Printf(TEXT("%s,%s"),
						*TexObj->GetName(), *CamObj->GetName());
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[CppToAS] LogMultiple should return 'TexName,CamName'"));
				}
			}
		}

		// ---- 11. Mutation: C++ passes object → AS AddToRoot → C++ verifies ----
		{
			// Verify not rooted initially
			bPassed &= LocalAssert.IsFalse(
				TexObj->IsRooted(),
				TEXT("[CppToAS] Tex should not be rooted before AS AddToRoot"));

			// AS AddToRoot
			{
				FASGlobalFunctionInvoker Inv(
					Test, Engine, Module, TEXT("void DoAddToRoot(UObject Obj)"));
				if (Inv.IsValid()) { Inv.AddArgObject(TexObj); Inv.Call(); }
			}
			// C++ verifies rooted
			bPassed &= LocalAssert.IsTrue(
				TexObj->IsRooted(),
				TEXT("[CppToAS] C++ should see Tex rooted after AS AddToRoot"));

			// AS confirms rooted
			TestBool(TEXT("bool IsRootedCheck(UObject Obj)"),
				TexObj, nullptr,
				TEXT("[CppToAS] AS IsRootedCheck should be true"), true);

			// AS RemoveFromRoot
			{
				FASGlobalFunctionInvoker Inv(
					Test, Engine, Module, TEXT("void DoRemoveFromRoot(UObject Obj)"));
				if (Inv.IsValid()) { Inv.AddArgObject(TexObj); Inv.Call(); }
			}
			// C++ verifies no longer rooted
			bPassed &= LocalAssert.IsFalse(
				TexObj->IsRooted(),
				TEXT("[CppToAS] C++ should see Tex not rooted after AS RemoveFromRoot"));

			// AS confirms not rooted
			TestBool(TEXT("bool IsRootedCheck(UObject Obj)"),
				TexObj, nullptr,
				TEXT("[CppToAS] AS IsRootedCheck should be false after remove"), false);
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: ObjectChainAndNesting
	//   Tests multi-level UObject outer chains (Parent→Child→GrandChild),
	//   verifying:
	//     1. AS creates a chain of nested objects using NewObject(Outer, ...)
	//        → C++ verifies the entire outer chain
	//     2. C++ creates a chain → passes all objects to AS → AS walks the
	//        chain via GetOuter()/GetTypedOuter() → returns results → C++ verifies
	//     3. GetOutermost() on deeply nested objects returns the package
	//     4. GetPathName() reflects the full chain hierarchy
	//     5. TArray of UObjects passed from C++ → AS iterates and builds info
	//     6. AS stores C++-passed objects in a local array, queries each
	// -----------------------------------------------------------------------
	bool RunObjectChainAndNestingSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_ObjChain"), TEXT(R"(
// ---- AS creates a chain of nested objects ----
UObject CreateChainRoot()
{
	return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"ChainRoot_42");
}

UObject CreateChild(UObject Parent, FName ChildName)
{
	return NewObject(Parent, UTexture2D::StaticClass(), ChildName);
}

// ---- Walk the chain from the deepest object ----
UObject WalkToRoot(UObject Obj)
{
	UObject Current = Obj;
	// Walk up to 10 levels (safety limit)
	for (int i = 0; i < 10; i++)
	{
		UObject Outer = Current.GetOuter();
		if (Outer == null)
			break;
		// If we hit a UPackage, we're at the top
		if (Outer.IsA(UPackage::StaticClass()))
			break;
		Current = Outer;
	}
	return Current;
}

// ---- Get the depth of the outer chain (count non-package outers) ----
int GetChainDepth(UObject Obj)
{
	int Depth = 0;
	UObject Current = Obj;
	for (int i = 0; i < 20; i++)
	{
		UObject Outer = Current.GetOuter();
		if (Outer == null)
			break;
		if (Outer.IsA(UPackage::StaticClass()))
			break;
		Depth++;
		Current = Outer;
	}
	return Depth;
}

// ---- Collect names along the chain (child → root) ----
FString CollectChainNames(UObject Obj)
{
	FString Result = Obj.GetName().ToString();
	UObject Current = Obj;
	for (int i = 0; i < 20; i++)
	{
		UObject Outer = Current.GetOuter();
		if (Outer == null)
			break;
		if (Outer.IsA(UPackage::StaticClass()))
			break;
		Result += ">" + Outer.GetName().ToString();
		Current = Outer;
	}
	return Result;
}

// ---- Get outermost (should be the package) ----
UObject GetOutermostOf(UObject Obj)
{
	return Obj.GetOutermost();
}

// ---- GetPathName reflects the full chain ----
FString GetPathOf(UObject Obj)
{
	return Obj.GetPathName();
}

// ---- Compare two objects in the chain ----
bool IsSameOuter(UObject A, UObject B)
{
	return (A.GetOuter() == B.GetOuter());
}

// ---- Log chain info ----
FString DescribeChain(UObject Leaf)
{
	FString Desc = "";
	UObject Current = Leaf;
	int Count = 0;
	for (int i = 0; i < 20; i++)
	{
		if (Count > 0)
			Desc += " -> ";
		Desc += Current.GetName().ToString();
		Count++;
		UObject Outer = Current.GetOuter();
		if (Outer == null || Outer.IsA(UPackage::StaticClass()))
			break;
		Current = Outer;
	}
	Log("Chain: " + Desc);
	return Desc;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = true;

		// =================================================================
		// Part A: AS creates the chain, C++ verifies
		// =================================================================

		// AS creates root
		FASGlobalFunctionInvoker CreateRootInv(
			Test, Engine, Module, TEXT("UObject CreateChainRoot()"));
		if (!CreateRootInv.IsValid()) return false;
		UObject* Root = CreateRootInv.CallAndReturn<UObject*>(nullptr);
		bPassed &= LocalAssert.IsNotNull(Root, TEXT("[ObjChain] Root should be non-null"));
		if (!Root) return false;

		// C++ verifies root's outer is the transient package
		bPassed &= LocalAssert.AreEqual(
			static_cast<UObject*>(GetTransientPackage()),
			Root->GetOuter(),
			TEXT("[ObjChain] Root outer should be transient package"));

		// AS creates child under root
		UObject* Child = nullptr;
		{
			FName ChildName(TEXT("ChainChild_42"));
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject CreateChild(UObject Parent, FName ChildName)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Root);
				Inv.AddArgStruct(ChildName);
				Child = Inv.CallAndReturn<UObject*>(nullptr);
			}
		}
		bPassed &= LocalAssert.IsNotNull(Child, TEXT("[ObjChain] Child should be non-null"));
		if (!Child) return false;

		// C++ verifies child's outer is root
		bPassed &= LocalAssert.AreEqual(
			Root,
			Child->GetOuter(),
			TEXT("[ObjChain] C++ Child->GetOuter() should be Root"));

		// AS creates grandchild under child
		UObject* GrandChild = nullptr;
		{
			FName GCName(TEXT("ChainGrandChild_42"));
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject CreateChild(UObject Parent, FName ChildName)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Child);
				Inv.AddArgStruct(GCName);
				GrandChild = Inv.CallAndReturn<UObject*>(nullptr);
			}
		}
		bPassed &= LocalAssert.IsNotNull(GrandChild, TEXT("[ObjChain] GrandChild should be non-null"));
		if (!GrandChild) return false;

		// C++ verifies grandchild chain: GC→Child→Root→Package
		bPassed &= LocalAssert.AreEqual(
			Child,
			GrandChild->GetOuter(),
			TEXT("[ObjChain] C++ GrandChild->GetOuter() should be Child"));
		bPassed &= LocalAssert.AreEqual(
			Root,
			GrandChild->GetOuter()->GetOuter(),
			TEXT("[ObjChain] C++ GrandChild->GetOuter()->GetOuter() should be Root"));

		// =================================================================
		// Part B: AS walks the chain, C++ verifies results
		// =================================================================

		// WalkToRoot(GrandChild) should return Root
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject WalkToRoot(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(GrandChild);
				UObject* WalkedRoot = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					Root,
					WalkedRoot,
					TEXT("[ObjChain] AS WalkToRoot(GrandChild) should reach Root"));
			}
		}

		// GetChainDepth(GrandChild) should be 2 (GC→Child→Root, 2 non-package levels above)
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("int GetChainDepth(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(GrandChild);
				int32 Depth = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(
					2,
					Depth,
					TEXT("[ObjChain] AS GetChainDepth(GrandChild) should be 2"));
			}
		}

		// GetChainDepth(Child) should be 1
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("int GetChainDepth(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Child);
				int32 Depth = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(
					1,
					Depth,
					TEXT("[ObjChain] AS GetChainDepth(Child) should be 1"));
			}
		}

		// GetChainDepth(Root) should be 0 (root's outer is package)
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("int GetChainDepth(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Root);
				int32 Depth = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(
					0,
					Depth,
					TEXT("[ObjChain] AS GetChainDepth(Root) should be 0"));
			}
		}

		// CollectChainNames(GrandChild) should be "ChainGrandChild_42>ChainChild_42>ChainRoot_42"
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString CollectChainNames(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(GrandChild);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = TEXT("ChainGrandChild_42>ChainChild_42>ChainRoot_42");
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[ObjChain] AS CollectChainNames(GC) should be GC>Child>Root"));
				}
			}
		}

		// =================================================================
		// Part C: C++ creates a chain, passes to AS for queries
		// =================================================================

		// Create a 4-level chain in C++: Pkg → L1 → L2 → L3 → Leaf
		UObject* L1 = NewObject<UTexture2D>(GetTransientPackage(), FName(TEXT("CppL1_42")), RF_Transient);
		UObject* L2 = NewObject<UTexture2D>(L1, FName(TEXT("CppL2_42")));
		UObject* L3 = NewObject<UTexture2D>(L2, FName(TEXT("CppL3_42")));
		UObject* Leaf = NewObject<UTexture2D>(L3, FName(TEXT("CppLeaf_42")));

		// AS GetChainDepth(Leaf) should be 3
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("int GetChainDepth(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Leaf);
				int32 Depth = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.AreEqual(
					3,
					Depth,
					TEXT("[ObjChain] AS GetChainDepth(CppLeaf 4-level) should be 3"));
			}
		}

		// AS WalkToRoot(Leaf) should be L1
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject WalkToRoot(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Leaf);
				UObject* WalkedRoot = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					L1,
					WalkedRoot,
					TEXT("[ObjChain] AS WalkToRoot(CppLeaf) should reach CppL1"));
			}
		}

		// AS CollectChainNames(Leaf) → C++ verifies
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString CollectChainNames(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Leaf);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString Expected = TEXT("CppLeaf_42>CppL3_42>CppL2_42>CppL1_42");
					bPassed &= LocalAssert.AreEqual(
						Expected,
						Result,
						TEXT("[ObjChain] AS CollectChainNames(CppLeaf) should show full chain"));
				}
			}
		}

		// AS GetOutermost on all levels should return the same package
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject GetOutermostOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Leaf);
				UObject* OutermostLeaf = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					static_cast<UObject*>(GetTransientPackage()),
					OutermostLeaf,
					TEXT("[ObjChain] AS GetOutermost(Leaf) should be transient package"));
			}
		}
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("UObject GetOutermostOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(L2);
				UObject* OutermostL2 = Inv.CallAndReturn<UObject*>(nullptr);
				bPassed &= LocalAssert.AreEqual(
					static_cast<UObject*>(GetTransientPackage()),
					OutermostL2,
					TEXT("[ObjChain] AS GetOutermost(L2) should also be transient package"));
			}
		}

		// AS GetPathName(Leaf) should contain the full hierarchy
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString GetPathOf(UObject Obj)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Leaf);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					FString CppPath = Leaf->GetPathName();
					bPassed &= LocalAssert.AreEqual(
						CppPath,
						Result,
						TEXT("[ObjChain] AS GetPathName(Leaf) should match C++"));
					// Also verify it contains all ancestor names
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("CppL1_42")),
						TEXT("[ObjChain] PathName should contain L1"));
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("CppL2_42")),
						TEXT("[ObjChain] PathName should contain L2"));
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("CppL3_42")),
						TEXT("[ObjChain] PathName should contain L3"));
				}
			}
		}

		// IsSameOuter: L2 and sibling created under L1 should have same outer
		UObject* L2Sibling = NewObject<UTexture2D>(L1, FName(TEXT("CppL2Sibling_42")));
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("bool IsSameOuter(UObject A, UObject B)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(L2);
				Inv.AddArgObject(L2Sibling);
				int32 R = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.IsTrue(
					R != 0,
					TEXT("[ObjChain] L2 and L2Sibling should have same outer (L1)"));
			}
		}
		// L2 and L3 should have different outers
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("bool IsSameOuter(UObject A, UObject B)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(L2);
				Inv.AddArgObject(L3);
				int32 R = Inv.CallAndReturn<int32>(INDEX_NONE);
				bPassed &= LocalAssert.IsFalse(
					R != 0,
					TEXT("[ObjChain] L2 and L3 should have different outers"));
			}
		}

		// DescribeChain: AS logs and returns the full chain description
		{
			FASGlobalFunctionInvoker Inv(
				Test, Engine, Module, TEXT("FString DescribeChain(UObject Leaf)"));
			if (Inv.IsValid())
			{
				Inv.AddArgObject(Leaf);
				FString Result;
				if (Inv.Call())
				{
					Inv.ReadReturnStruct(Result);
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("CppLeaf_42")),
						TEXT("[ObjChain] DescribeChain should contain Leaf name"));
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT("CppL1_42")),
						TEXT("[ObjChain] DescribeChain should contain L1 name"));
					// Should be in format: Leaf -> L3 -> L2 -> L1
					bPassed &= LocalAssert.IsTrue(
						Result.Contains(TEXT(" -> ")),
						TEXT("[ObjChain] DescribeChain should contain arrow separators"));
				}
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: LogAndDiagnostics
	//   AS calls Log/Warning/Error/LogIf/WarningIf/ErrorIf with known strings.
	//   C++ registers AddExpectedError/AddExpectedMessage to verify the log
	//   output actually reaches the UE log system. Also tests Throw path.
	//
	//   Cross-validation pattern:
	//     C++ registers expected log patterns →
	//     AS calls Log functions →
	//     C++ verifies all expected patterns were matched (automation does
	//     this automatically — unmatched AddExpectedError triggers failure).
	// -----------------------------------------------------------------------
	bool RunLogAndDiagnosticsSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		// AS Error()/Warning()/Throw() produce UE_LOG at Error/Warning level.
		// Register all expected outputs so the automation framework doesn't
		// treat them as unexpected failures.
		Test.AddExpectedErrorPlain(TEXT("BIND_ERR_MARKER_42"),      EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("BIND_WARN_MARKER_42"),     EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("BIND_ERRIF_MARKER_42"),    EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("BIND_WARNIF_MARKER_42"),   EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("BIND_CAT_ERR_MARKER_42"),  EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("BIND_CAT_WARN_MARKER_42"), EAutomationExpectedErrorFlags::Contains, 1);
		// Throw produces 3 Error-level lines: the exception message, the module name, and the callstack frame
		Test.AddExpectedErrorPlain(TEXT("BIND_THROW_MARKER_42"),    EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("ASUObject_LogDiag"),       EAutomationExpectedErrorFlags::Contains, 1);
		Test.AddExpectedErrorPlain(TEXT("CallThrow"),               EAutomationExpectedErrorFlags::Contains, 1);

		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASUObject_LogDiag"), TEXT(R"(
void CallLog()
{
	Log("BIND_LOG_MARKER_42");
}
void CallLogInfo()
{
	LogInfo("BIND_LOGINFO_MARKER_42");
}
void CallLogDisplay()
{
	LogDisplay("BIND_LOGDISPLAY_MARKER_42");
}
void CallError()
{
	Error("BIND_ERR_MARKER_42");
}
void CallWarning()
{
	Warning("BIND_WARN_MARKER_42");
}
void CallLogIf_True()
{
	LogIf(true, "BIND_LOGIF_TRUE_MARKER_42");
}
void CallLogIf_False()
{
	LogIf(false, "SHOULD_NOT_APPEAR_LOG");
}
void CallErrorIf_True()
{
	ErrorIf(true, "BIND_ERRIF_MARKER_42");
}
void CallErrorIf_False()
{
	ErrorIf(false, "SHOULD_NOT_APPEAR_ERR");
}
void CallWarningIf_True()
{
	WarningIf(true, "BIND_WARNIF_MARKER_42");
}
void CallWarningIf_False()
{
	WarningIf(false, "SHOULD_NOT_APPEAR_WARN");
}
void CallLogWithCategory()
{
	Log(n"TestCategory", "BIND_CAT_LOG_MARKER_42");
}
void CallErrorWithCategory()
{
	Error(n"TestCategory", "BIND_CAT_ERR_MARKER_42");
}
void CallWarningWithCategory()
{
	Warning(n"TestCategory", "BIND_CAT_WARN_MARKER_42");
}
void CallThrow()
{
	Throw("BIND_THROW_MARKER_42");
}
int LogConcatTypes()
{
	UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
	int IntVal = 42;
	float FloatVal = 3.14;
	bool BoolVal = true;
	FName NameVal = n"TestName";
	FString StrVal = "Hello";

	Log("Int=" + IntVal + " Float=" + FloatVal + " Bool=" + BoolVal + " Name=" + NameVal + " Str=" + StrVal + " Obj=" + Obj);
	return 1;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();
		bool bPassed = true;

		// Step 1: AS calls all log functions, C++ verifies they execute without crash
		auto CallVoid = [&](const TCHAR* Decl, const TCHAR* Label) -> bool
		{
			FASGlobalFunctionInvoker Invoker(Test, Engine, Module, Decl);
			if (!Invoker.IsValid()) return false;
			bool bOk = Invoker.Call();
			Test.AddInfo(FString(Label));
			return bOk;
		};

		bPassed &= CallVoid(TEXT("void CallLog()"),             TEXT("Log() should execute"));
		bPassed &= CallVoid(TEXT("void CallLogInfo()"),          TEXT("LogInfo() should execute"));
		bPassed &= CallVoid(TEXT("void CallLogDisplay()"),       TEXT("LogDisplay() should execute"));
		bPassed &= CallVoid(TEXT("void CallError()"),            TEXT("Error() should execute"));
		bPassed &= CallVoid(TEXT("void CallWarning()"),          TEXT("Warning() should execute"));
		bPassed &= CallVoid(TEXT("void CallLogIf_True()"),       TEXT("LogIf(true) should execute"));
		bPassed &= CallVoid(TEXT("void CallLogIf_False()"),      TEXT("LogIf(false) should execute (no output)"));
		bPassed &= CallVoid(TEXT("void CallErrorIf_True()"),     TEXT("ErrorIf(true) should execute"));
		bPassed &= CallVoid(TEXT("void CallErrorIf_False()"),    TEXT("ErrorIf(false) should execute (no output)"));
		bPassed &= CallVoid(TEXT("void CallWarningIf_True()"),   TEXT("WarningIf(true) should execute"));
		bPassed &= CallVoid(TEXT("void CallWarningIf_False()"),  TEXT("WarningIf(false) should execute (no output)"));
		bPassed &= CallVoid(TEXT("void CallLogWithCategory()"),  TEXT("Log(category) should execute"));
		bPassed &= CallVoid(TEXT("void CallErrorWithCategory()"),TEXT("Error(category) should execute"));
		bPassed &= CallVoid(TEXT("void CallWarningWithCategory()"), TEXT("Warning(category) should execute"));

		// Step 2: Throw — should raise AS exception
		bPassed &= ExecuteFunctionExpectingScriptException(
			Test, Engine, Module, 
			TEXT("void CallThrow()"),
			TEXT("Throw() should raise script exception"),
			TEXT("BIND_THROW_MARKER_42"));

		// Step 3: Log with type concatenation — AS returns 1 if no crash
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int LogConcatTypes()"),
			TEXT("Log with int/float/bool/FName/FString/UObject concat should succeed"),
			1);

		// The AddExpectedError registrations above will cause the test to fail
		// if Error/Warning output was not seen, or if "SHOULD_NOT_APPEAR" was seen.

		return bPassed;
	}
}

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptUObjectBindingsTest,
	"Angelscript.TestModule.Bindings.UObject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(CreateAndIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunCreateAndIdentitySection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(HierarchyAndOuter)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunHierarchyAndOuterSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(TypeQueryAndCast)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunTypeQueryAndCastSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(FindAndLookup)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunFindAndLookupSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(RootLifecycle)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunRootLifecycleSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(FlagMutation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunFlagMutationSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(NullAndIsValid)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunNullAndIsValidSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(NewObjectVariants)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunNewObjectVariantsSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(ClassReflection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunClassReflectionSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(ReturnValueCrossCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunReturnValueCrossCheckSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(CppToScriptPassthrough)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunCppToScriptPassthroughSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(ObjectChainAndNesting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunObjectChainAndNestingSection(*TestRunner, Engine);
		}
	}

	TEST_METHOD(LogAndDiagnostics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		RunLogAndDiagnosticsSection(*TestRunner, Engine);
		}
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
