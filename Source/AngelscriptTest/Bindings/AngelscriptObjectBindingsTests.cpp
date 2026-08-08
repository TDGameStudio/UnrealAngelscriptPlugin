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

TEST_CLASS_WITH_FLAGS(FAngelscriptObjectBindingsTest,
	"Angelscript.TestModule.Bindings.Object",
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
	// Section: ObjectPtrCompat
	// ====================================================================

	TEST_METHOD(ObjectPtrPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASObject_ObjectPtrCompat"), ASTEST_AS(R"AS(
			int ObjPtr_DefaultIsNull()
			{
				TObjectPtr<UTexture2D> Empty;
				return (Empty.Get() == null) ? 1 : 0;
			}

			int ObjPtr_Construct()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				if (!IsValid(Texture))
				{
					return 0;
				}
				TObjectPtr<UTexture2D> Constructed(Texture);
				if (!(Constructed == Texture))
				{
					return 0;
				}
				if (!(Constructed.Get() == Texture))
				{
					return 0;
				}
				return 1;
			}

			int ObjPtr_Assign()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				TObjectPtr<UTexture2D> Assigned;
				Assigned = Texture;
				if (!(Assigned == Texture))
				{
					return 0;
				}
				if (!(Assigned.Get() == Texture))
				{
					return 0;
				}
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_DefaultIsNull()"), TEXT("default TObjectPtr is null"), 1), TEXT("default TObjectPtr is null")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_Construct()"), TEXT("TObjectPtr construct from raw pointer"), 1), TEXT("TObjectPtr construct from raw pointer")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_Assign()"), TEXT("TObjectPtr assign from raw pointer"), 1), TEXT("TObjectPtr assign from raw pointer")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_ImplicitConvert()"), TEXT("TObjectPtr implicit conversion to raw pointer"), 1), TEXT("TObjectPtr implicit conversion to raw pointer")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int ObjPtr_Copy()"), TEXT("TObjectPtr copy equality"), 1), TEXT("TObjectPtr copy equality")));
	}

	// ====================================================================
	// Section: SoftObjectPtrCompat
	// ====================================================================

	TEST_METHOD(SoftObjectPtrPaths)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASObject_SoftObjectPtrCompat"), ASTEST_AS(R"AS(
			int SoftPtr_DefaultState()
			{
				TSoftObjectPtr<UTexture2D> Empty;
				if (!Empty.IsNull())
				{
					return 0;
				}
				if (!(Empty.Get() == null))
				{
					return 0;
				}
				if (Empty.IsValid())
				{
					return 0;
				}
				return 1;
			}

			int SoftPtr_Construct()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				if (!IsValid(Texture))
				{
					return 0;
				}
				TSoftObjectPtr<UTexture2D> Constructed(Texture);
				if (!(Constructed == Texture))
				{
					return 0;
				}
				if (!(Constructed.Get() == Texture))
				{
					return 0;
				}
				return 1;
			}

			int SoftPtr_PathAndValidity()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				TSoftObjectPtr<UTexture2D> Constructed(Texture);
				if (!(Constructed.ToSoftObjectPath().ToString() == Constructed.ToString()))
				{
					return 0;
				}
				if (Constructed.IsPending())
				{
					return 0;
				}
				if (!Constructed.IsValid())
				{
					return 0;
				}
				if (Constructed.GetAssetName().IsEmpty())
				{
					return 0;
				}
				if (Constructed.GetLongPackageName().IsEmpty())
				{
					return 0;
				}
				if (Constructed.ToString().IsEmpty())
				{
					return 0;
				}
				return 1;
			}

			int SoftPtr_ConstructFromPath()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				TSoftObjectPtr<UTexture2D> Constructed(Texture);
				FSoftObjectPath ConstructedPath = Constructed.ToSoftObjectPath();
				if (ConstructedPath.ToString().IsEmpty())
				{
					return 0;
				}

				TSoftObjectPtr<UTexture2D> FromPath(ConstructedPath);
				if (!(FromPath == Texture))
				{
					return 0;
				}

				TSoftObjectPtr<UTexture2D> AssignedFromPath;
				AssignedFromPath = ConstructedPath;
				if (!(AssignedFromPath == Texture))
				{
					return 0;
				}
				return 1;
			}

			int SoftPtr_AssignAndConvert()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				TSoftObjectPtr<UTexture2D> Assigned;
				Assigned = Texture;
				if (!(Assigned == Texture))
				{
					return 0;
				}
				if (!(Assigned.Get() == Texture))
				{
					return 0;
				}
				UTexture2D Converted = Assigned.Get();
				if (!(Converted == Texture))
				{
					return 0;
				}
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
				{
					return 0;
				}
				if (!(History[0] == Texture))
				{
					return 0;
				}
				if (!History.Contains(Copy))
				{
					return 0;
				}
				return 1;
			}

			int SoftPtr_Reset()
			{
				UTexture2D Texture = Cast<UTexture2D>(NewObject(GetTransientPackage(), UTexture2D::StaticClass()));
				TSoftObjectPtr<UTexture2D> Assigned;
				Assigned = Texture;
				Assigned.Reset();
				if (!Assigned.IsNull())
				{
					return 0;
				}
				if (!(Assigned.Get() == null))
				{
					return 0;
				}
				return 1;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_DefaultState()"), TEXT("default TSoftObjectPtr is null and invalid"), 1), TEXT("default TSoftObjectPtr is null and invalid")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_Construct()"), TEXT("TSoftObjectPtr construct from raw pointer"), 1), TEXT("TSoftObjectPtr construct from raw pointer")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_PathAndValidity()"), TEXT("TSoftObjectPtr path and validity accessors"), 1), TEXT("TSoftObjectPtr path and validity accessors")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_ConstructFromPath()"), TEXT("TSoftObjectPtr construct and assign from FSoftObjectPath"), 1), TEXT("TSoftObjectPtr construct and assign from FSoftObjectPath")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_AssignAndConvert()"), TEXT("TSoftObjectPtr assign and Get conversion"), 1), TEXT("TSoftObjectPtr assign and Get conversion")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_CopyEquality()"), TEXT("TSoftObjectPtr copy equality"), 1), TEXT("TSoftObjectPtr copy equality")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_ArrayOperations()"), TEXT("TSoftObjectPtr TArray Add/Contains"), 1), TEXT("TSoftObjectPtr TArray Add/Contains")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int SoftPtr_Reset()"), TEXT("TSoftObjectPtr Reset clears to null"), 1), TEXT("TSoftObjectPtr Reset clears to null")));
	}

	TEST_METHOD(SoftObjectPtrReportsPathAndResolvesLoadedAsset)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int SoftObjectPtr_ToString()
			{
				FSoftObjectPath Path("/Engine/EngineResources/DefaultTexture.DefaultTexture");
				TSoftObjectPtr<UTexture2D> TextureReference(Path);
				return TextureReference.ToString() == Path.ToString() ? 1 : 0;
			}

			int SoftObjectPtr_LongPackageName()
			{
				FSoftObjectPath Path("/Engine/EngineResources/DefaultTexture.DefaultTexture");
				TSoftObjectPtr<UTexture2D> TextureReference(Path);
				return TextureReference.GetLongPackageName() == "/Engine/EngineResources/DefaultTexture" ? 1 : 0;
			}

			int SoftObjectPtr_AssetName()
			{
				FSoftObjectPath Path("/Engine/EngineResources/DefaultTexture.DefaultTexture");
				TSoftObjectPtr<UTexture2D> TextureReference(Path);
				return TextureReference.GetAssetName() == "DefaultTexture" ? 1 : 0;
			}

			int SoftObjectPtr_ResolvesAfterPathLoad()
			{
				FSoftObjectPath Path("/Engine/EngineResources/DefaultTexture.DefaultTexture");
				TSoftObjectPtr<UTexture2D> TextureReference(Path);
				UTexture2D LoadedTexture = Cast<UTexture2D>(Path.TryLoad());
				return LoadedTexture != null
					&& TextureReference.IsValid()
					&& TextureReference.Get() == LoadedTexture ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASObject_SoftObjectPtrReportsPathAndResolvesLoadedAsset"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("TSoftObjectPtr path accessor and resolution module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(),
			TEXT("int SoftObjectPtr_ToString()"),
			TEXT("TSoftObjectPtr ToString should preserve the source object path"), 1),
			TEXT("TSoftObjectPtr ToString should dispatch through the manual binding")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(),
			TEXT("int SoftObjectPtr_LongPackageName()"),
			TEXT("TSoftObjectPtr GetLongPackageName should return the source package"), 1),
			TEXT("TSoftObjectPtr GetLongPackageName should dispatch through the manual binding")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(),
			TEXT("int SoftObjectPtr_AssetName()"),
			TEXT("TSoftObjectPtr GetAssetName should return the source asset name"), 1),
			TEXT("TSoftObjectPtr GetAssetName should dispatch through the manual binding")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(*TestRunner, Engine, ModuleScope.GetModule(),
			TEXT("int SoftObjectPtr_ResolvesAfterPathLoad()"),
			TEXT("TSoftObjectPtr IsValid and Get should resolve an asset loaded through its path"), 1),
			TEXT("TSoftObjectPtr loaded-asset resolution should dispatch through the manual binding")));
	}
};

#endif
