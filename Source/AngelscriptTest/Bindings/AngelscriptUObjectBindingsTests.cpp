#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Camera/CameraActor.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptUObjectBindingsTest,
	"Angelscript.TestModule.Bindings.UObject",
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

	TEST_METHOD(NewObjectAndIdentityContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_NewObjectIdentitySmoke"), ASTEST_AS(R"AS(
			int NewObjectAndIdentitySmoke()
			{
				UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjectBindingIdentitySmoke", true);
				if (!IsValid(Obj))
				{
					return 0;
				}
				if (Obj.GetName() != n"UObjectBindingIdentitySmoke")
				{
					return 0;
				}
				if (!Obj.IsA(UTexture2D::StaticClass()))
				{
					return 0;
				}
				if (!Obj.GetFullName().Contains("Texture2D"))
				{
					return 0;
				}
				return Obj.IsTransient() ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject NewObject identity smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int NewObjectAndIdentitySmoke()"),
			TEXT("NewObject, GetName, GetFullName, IsA, and IsTransient should dispatch"), 1)));
	}

	TEST_METHOD(OuterContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_OuterSmoke"), ASTEST_AS(R"AS(
			int GetOuterSmoke()
			{
				UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjectBindingOuterSmoke");
				if (Obj == nullptr)
				{
					return 0;
				}
				if (Obj.GetOuter() != GetTransientPackage())
				{
					return 0;
				}
				return Obj.GetPathName().Contains("UObjectBindingOuterSmoke") ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject outer smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int GetOuterSmoke()"),
			TEXT("GetOuter and GetPathName should be callable for AS-created UObject handles"), 1)));
	}

	TEST_METHOD(NewObjectAndGetTypedOuterPreserveConcreteTypes)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int ExactResultOutputTypeSmoke()
			{
				UTexture2D ParentTexture = NewObject(
					GetTransientPackage(),
					UTexture2D,
					n"UObjectBindingTypedParent");
				UTexture2D ChildTexture = NewObject(
					ParentTexture,
					UTexture2D,
					n"UObjectBindingTypedChild");

				UTexture2D TypedTextureOuter = ChildTexture.GetTypedOuter(UTexture2D);
				UTexture TypedTextureBaseOuter = ChildTexture.GetTypedOuter(UTexture);

				int Result = ParentTexture != nullptr ? 1 : 0;
				Result |= TypedTextureOuter == ParentTexture ? 2 : 0;
				Result |= TypedTextureBaseOuter == ParentTexture ? 4 : 0;
				return Result;
			}
			)AS");

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASUObject_ExactResultOutputTypeSmoke"),
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject exact-result output-type module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int ExactResultOutputTypeSmoke()"),
			TEXT("NewObject and GetTypedOuter should preserve concrete output types and object identity"), 7)));
	}

	TEST_METHOD(TypeQueryAndCastContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_TypeCastSmoke"), ASTEST_AS(R"AS(
			int TypeQueryAndCastSmoke()
			{
				UObject Camera = NewObject(GetTransientPackage(), ACameraActor::StaticClass(), n"UObjectBindingCameraSmoke");
				if (Cast<AActor>(Camera) == nullptr)
				{
					return 0;
				}
				if (Cast<ACameraActor>(Camera) == nullptr)
				{
					return 0;
				}
				if (Cast<UTexture2D>(Camera) != nullptr)
				{
					return 0;
				}
				return Camera.IsA(AActor::StaticClass()) && Camera.IsA(ACameraActor::StaticClass()) ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject type/cast smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int TypeQueryAndCastSmoke()"),
			TEXT("Cast<T> and IsA should resolve for UObject handles"), 1)));
	}

	TEST_METHOD(FindObjectContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_FindObjectSmoke"), ASTEST_AS(R"AS(
			int FindObjectSmoke()
			{
				UObject Created = NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjectBindingFindSmoke");
				if (Created == nullptr)
				{
					return 0;
				}

				UObject FoundByPath = FindObject(Created.GetPathName());
				UObject FoundByOuter = FindObject(GetTransientPackage(), "UObjectBindingFindSmoke");
				return FoundByPath == Created && FoundByOuter == Created ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject FindObject smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int FindObjectSmoke()"),
			TEXT("FindObject overloads should resolve and return the same UObject handle"), 1)));
	}

	TEST_METHOD(NullAndIsValidContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_NullValidSmoke"), ASTEST_AS(R"AS(
			int NullAndIsValidSmoke()
			{
				UObject Obj = NewObject(GetTransientPackage(), UTexture2D::StaticClass());
				UObject Empty;
				if (!IsValid(Obj))
				{
					return 0;
				}
				if (IsValid(Empty))
				{
					return 0;
				}
				if (Obj == null)
				{
					return 0;
				}
				return Empty == null ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject null/IsValid smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int NullAndIsValidSmoke()"),
			TEXT("null comparison and IsValid should be callable for UObject handles"), 1)));
	}

	TEST_METHOD(ClassReflectionContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_ClassReflectionSmoke"), ASTEST_AS(R"AS(
			int ClassReflectionSmoke()
			{
				UClass CameraClass = ACameraActor::StaticClass();
				if (CameraClass.GetSuperClass() != AActor::StaticClass())
				{
					return 0;
				}
				if (!CameraClass.IsChildOf(AActor::StaticClass()))
				{
					return 0;
				}
				if (CameraClass.GetDefaultObject() == nullptr)
				{
					return 0;
				}
				return AActor::StaticClass().FindFunctionByName(n"ReceiveTick") != nullptr ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject class reflection smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, Module.GetModule(),
			TEXT("int ClassReflectionSmoke()"),
			TEXT("UClass reflection helpers should resolve and dispatch"), 1)));
	}

	TEST_METHOD(ReturnAndArgumentMarshallingContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_ReturnArgSmoke"), ASTEST_AS(R"AS(
			UObject MakeTexture()
			{
				return NewObject(GetTransientPackage(), UTexture2D::StaticClass(), n"UObjectBindingReturnSmoke");
			}

			FString DescribeObject(UObject Obj)
			{
				if (Obj == nullptr)
				{
					return "";
				}
				return Obj.GetClass().GetName().ToString() + ":" + Obj.GetName().ToString();
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject return/argument smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		FASGlobalFunctionInvoker MakeInvoker(*TestRunner, Engine, Module.GetModule(), TEXT("UObject MakeTexture()"));
		ASSERT_THAT(IsTrue(MakeInvoker.IsValid(), TEXT("MakeTexture should resolve")));
		UObject* Created = MakeInvoker.CallAndReturn<UObject*>(nullptr);
		ASSERT_THAT(IsNotNull(Created, TEXT("AS should return a UObject handle")));
		if (Created == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker DescribeInvoker(*TestRunner, Engine, Module.GetModule(), TEXT("FString DescribeObject(UObject Obj)"));
		ASSERT_THAT(IsTrue(DescribeInvoker.IsValid(), TEXT("DescribeObject should resolve")));
		DescribeInvoker.AddArgObject(Created);
		FString Description;
		ASSERT_THAT(IsTrue(DescribeInvoker.Call(), TEXT("DescribeObject should execute")));
		ASSERT_THAT(IsTrue(DescribeInvoker.ReadReturnStruct(Description), TEXT("DescribeObject return string should be readable")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Texture2D:UObjectBindingReturnSmoke")),
			Description,
			TEXT("UObject argument and FString return should cross the AS/C++ boundary")));
	}

	TEST_METHOD(CppToScriptPassthroughContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASUObject_CppToScriptSmoke"), ASTEST_AS(R"AS(
			int ReadCppObject(UObject Obj)
			{
				if (Obj == nullptr)
				{
					return 0;
				}
				if (!Obj.IsA(UTexture2D::StaticClass()))
				{
					return 0;
				}
				if (Obj.GetName() != n"UObjectBindingCppObjectSmoke")
				{
					return 0;
				}
				return Obj.GetOuter() == GetTransientPackage() ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("UObject C++ to script smoke module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		UObject* CppObject = NewObject<UTexture2D>(
			GetTransientPackage(),
			FName(TEXT("UObjectBindingCppObjectSmoke")),
			RF_Transient);
		ASSERT_THAT(IsNotNull(CppObject, TEXT("C++ should create object for passthrough smoke")));
		if (CppObject == nullptr)
		{
			return;
		}

		FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, Module.GetModule(), TEXT("int ReadCppObject(UObject Obj)"));
		ASSERT_THAT(IsTrue(Invoker.IsValid(), TEXT("ReadCppObject should resolve")));
		Invoker.AddArgObject(CppObject);
		ASSERT_THAT(AreEqual(
			1,
			Invoker.CallAndReturn<int32>(INDEX_NONE),
			TEXT("C++-created UObject should be readable by script")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
