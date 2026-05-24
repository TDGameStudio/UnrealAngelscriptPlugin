// ============================================================================
// AngelscriptObjectBindingsTests.cpp
//
// TObjectPtr / TSoftObjectPtr binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Object.FAngelscriptObjectBindingsTest.*
//
// Sections:
//   ObjectPtrCompat     — TObjectPtr default, construct, assign, convert, copy
//   SoftObjectPtrCompat — TSoftObjectPtr default, construct, assign, convert,
//                         copy, path construction, array, Reset
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptObjectBindingsTest,
	"Angelscript.TestModule.Bindings.Object",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ObjectPtrCompat
	// ====================================================================

	TEST_METHOD(ObjectPtrCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASObject_ObjectPtrCompat"), TEXT(R"(
int ObjPtr_DefaultIsNull()
{
	TObjectPtr<UTexture2D> Empty;
	return (Empty.Get() == null) ? 1 : 0;
}

int ObjPtr_Construct()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	if (!IsValid(Texture))
		return 0;
	TObjectPtr<UTexture2D> Constructed(Texture);
	if (!(Constructed == Texture))
		return 0;
	if (!(Constructed.Get() == Texture))
		return 0;
	return 1;
}

int ObjPtr_Assign()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	if (!(Assigned == Texture))
		return 0;
	if (!(Assigned.Get() == Texture))
		return 0;
	return 1;
}

int ObjPtr_ImplicitConvert()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	UTexture2D Converted = Assigned;
	return (Converted == Texture) ? 1 : 0;
}

int ObjPtr_Copy()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	TObjectPtr<UTexture2D> Copy = Assigned;
	return (Copy == Assigned) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_DefaultIsNull()"), TEXT("default TObjectPtr is null"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_Construct()"), TEXT("TObjectPtr construct from raw pointer"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_Assign()"), TEXT("TObjectPtr assign from raw pointer"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_ImplicitConvert()"), TEXT("TObjectPtr implicit conversion to raw pointer"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_Copy()"), TEXT("TObjectPtr copy equality"), 1);
	}

	// ====================================================================
	// Section: SoftObjectPtrCompat
	// ====================================================================

	TEST_METHOD(SoftObjectPtrCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASObject_SoftObjectPtrCompat"), TEXT(R"(
int SoftPtr_DefaultState()
{
	TSoftObjectPtr<UTexture2D> Empty;
	if (!Empty.IsNull())
		return 0;
	if (!(Empty.Get() == null))
		return 0;
	if (Empty.IsValid())
		return 0;
	return 1;
}

int SoftPtr_Construct()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	if (!IsValid(Texture))
		return 0;
	TSoftObjectPtr<UTexture2D> Constructed(Texture);
	if (!(Constructed == Texture))
		return 0;
	if (!(Constructed.Get() == Texture))
		return 0;
	return 1;
}

int SoftPtr_PathAndValidity()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TSoftObjectPtr<UTexture2D> Constructed(Texture);
	if (!(Constructed.ToSoftObjectPath().ToString() == Constructed.ToString()))
		return 0;
	if (Constructed.IsPending())
		return 0;
	if (!Constructed.IsValid())
		return 0;
	if (Constructed.GetAssetName().IsEmpty())
		return 0;
	if (Constructed.GetLongPackageName().IsEmpty())
		return 0;
	if (Constructed.ToString().IsEmpty())
		return 0;
	return 1;
}

int SoftPtr_ConstructFromPath()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TSoftObjectPtr<UTexture2D> Constructed(Texture);
	FSoftObjectPath ConstructedPath = Constructed.ToSoftObjectPath();
	if (ConstructedPath.ToString().IsEmpty())
		return 0;

	TSoftObjectPtr<UTexture2D> FromPath(ConstructedPath);
	if (!(FromPath == Texture))
		return 0;

	TSoftObjectPtr<UTexture2D> AssignedFromPath;
	AssignedFromPath = ConstructedPath;
	if (!(AssignedFromPath == Texture))
		return 0;
	return 1;
}

int SoftPtr_AssignAndConvert()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TSoftObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	if (!(Assigned == Texture))
		return 0;
	if (!(Assigned.Get() == Texture))
		return 0;
	UTexture2D Converted = Assigned.Get();
	if (!(Converted == Texture))
		return 0;
	return 1;
}

int SoftPtr_CopyEquality()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TSoftObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	TSoftObjectPtr<UTexture2D> Copy = Assigned;
	return (Copy == Assigned) ? 1 : 0;
}

int SoftPtr_ArrayOperations()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TSoftObjectPtr<UTexture2D> Constructed(Texture);
	TSoftObjectPtr<UTexture2D> Copy = Constructed;

	TArray<TSoftObjectPtr<UTexture2D>> History;
	History.Add(Constructed);
	History.Add(Copy);
	if (History.Num() != 2)
		return 0;
	if (!(History[0] == Texture))
		return 0;
	if (!History.Contains(Copy))
		return 0;
	return 1;
}

int SoftPtr_Reset()
{
	UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
	TSoftObjectPtr<UTexture2D> Assigned;
	Assigned = Texture;
	Assigned.Reset();
	if (!Assigned.IsNull())
		return 0;
	if (!(Assigned.Get() == null))
		return 0;
	return 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_DefaultState()"), TEXT("default TSoftObjectPtr is null and invalid"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_Construct()"), TEXT("TSoftObjectPtr construct from raw pointer"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_PathAndValidity()"), TEXT("TSoftObjectPtr path and validity accessors"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_ConstructFromPath()"), TEXT("TSoftObjectPtr construct and assign from FSoftObjectPath"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_AssignAndConvert()"), TEXT("TSoftObjectPtr assign and Get conversion"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_CopyEquality()"), TEXT("TSoftObjectPtr copy equality"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_ArrayOperations()"), TEXT("TSoftObjectPtr TArray Add/Contains"), 1);
		ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_Reset()"), TEXT("TSoftObjectPtr Reset clears to null"), 1);
	}
};

#endif
