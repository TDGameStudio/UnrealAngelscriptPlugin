#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FModuleGlobalTests, "Angelscript.TestModule.AngelScriptSDK.Module.Globals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int FindByName(asIScriptModule* Module, const char* Name)
	{
		for (asUINT Index = 0; Module != nullptr && Index < Module->GetGlobalVarCount(); ++Index)
		{
			const char* Current = nullptr;
			if (Module->GetGlobalVar(Index, &Current) >= 0
				&& Current != nullptr
				&& std::strcmp(Current, Name) == 0)
			{
				return static_cast<int>(Index);
			}
		}
		return -1;
	}

public:
	TEST_METHOD(ModuleGlobalEnumerate)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		AS_NATIVE_PRODUCT("MOD-GLOBAL-STATE-LIFECYCLE",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModule Module(*TestRunner, Engine, "ModuleGlobalEnumerate", ASTEST_AS_ANSI(R"AS(
			const int a = 1;
			const double b = 2.0;
			const double c = 35.2;
			const uint d = 0xC0DE;

			uint Entry()
			{
				return d;
			}
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}
		const asUINT GlobalCount = Module->GetGlobalVarCount();
		ASSERT_THAT(AreEqual(4, static_cast<int32>(GlobalCount), TEXT("Module globals should enumerate every declaration")));
		if (GlobalCount != 4)
		{
			return;
		}
		const char* const ExpectedNames[] = { "a", "b", "c", "d" };
		const char* const ExpectedDeclarations[] = { "const int a", "const float b", "const float c", "const uint d" };
		const int32 ExpectedTypeIds[] =
		{
			asTYPEID_INT32,
			asTYPEID_FLOAT64,
			asTYPEID_FLOAT64,
			asTYPEID_UINT32,
		};
		for (asUINT GlobalIndex = 0; GlobalIndex < GlobalCount; ++GlobalIndex)
		{
			const char* GlobalName = nullptr;
			const char* GlobalNamespace = nullptr;
			int GlobalTypeId = 0;
			bool bGlobalConst = false;
			ASSERT_THAT(IsTrue(Module->GetGlobalVar(GlobalIndex, &GlobalName, &GlobalNamespace, &GlobalTypeId, &bGlobalConst) >= 0,
				TEXT("Module globals should return metadata for every indexed declaration")));
			ASSERT_THAT(AreEqual(FString(UTF8_TO_TCHAR(ExpectedNames[GlobalIndex])), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")),
				TEXT("Module globals should preserve declaration order and exact names")));
			ASSERT_THAT(AreEqual(FString(), FString(UTF8_TO_TCHAR(GlobalNamespace != nullptr ? GlobalNamespace : "")),
				TEXT("Module globals should retain the empty namespace")));
			ASSERT_THAT(AreEqual(ExpectedTypeIds[GlobalIndex], GlobalTypeId,
				TEXT("Module globals should preserve each declaration's exact type id")));
			ASSERT_THAT(IsTrue(bGlobalConst, TEXT("Module globals should retain const metadata for every declaration")));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(ExpectedDeclarations[GlobalIndex])),
				FString(UTF8_TO_TCHAR(Module->GetGlobalVarDeclaration(GlobalIndex, true))),
				TEXT("Module globals should preserve each exact declaration")));
		}
		ASSERT_THAT(AreEqual(1, FindByName(Module, "b"), TEXT("Module globals should locate a declaration by name")));
		const int Index = FindByName(Module, "c");
		const char* Name = nullptr;
		bool bConst = false;
		ASSERT_THAT(IsTrue(Module->GetGlobalVar(Index, &Name, nullptr, nullptr, &bConst) >= 0, TEXT("Module globals should return metadata")));
		ASSERT_THAT(IsTrue(bConst, TEXT("Module globals should retain const metadata")));
		asUINT* const Value = static_cast<asUINT*>(Module->GetAddressOfGlobalVar(3));
		ASSERT_THAT(IsNotNull(Value, TEXT("Module globals should expose initialized storage")));
		if (Value != nullptr)
		{
			ASSERT_THAT(AreEqual(static_cast<uint32>(0xC0DE), static_cast<uint32>(*Value), TEXT("Module globals should preserve initializer values")));
		}

		int32 EntryResult = 0;
		if (!ExecuteScriptFunction(*TestRunner, Engine.Get(), Module, "uint Entry()", EntryResult))
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<uint32>(0xC0DE), static_cast<uint32>(EntryResult),
			TEXT("Module globals should execute against the same initialized storage exposed by metadata")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), static_cast<int32>(Module.Discard()),
			TEXT("Module globals should explicitly discard the owning module")));
		ASSERT_THAT(IsNull(Engine.Get()->GetModule("ModuleGlobalEnumerate", asGM_ONLY_IF_EXISTS),
			TEXT("Module globals should remove the discarded module from the engine")));

		FNativeTestEngine IsolatedEngine;
		IsolatedEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			IsolatedEngine.Destroy();
		};
		ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("Module globals should create an independent engine")));
		if (IsolatedEngine.Get() == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(IsolatedEngine.Get() != Engine.Get(), TEXT("Module globals should isolate engine state")));
		ASSERT_THAT(IsNull(IsolatedEngine.Get()->GetModule("ModuleGlobalEnumerate", asGM_ONLY_IF_EXISTS),
			TEXT("Module globals should not publish declarations into an independent engine")));
	}

	TEST_METHOD(ModuleGlobalResetState)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-GLOBAL-STATE-LIFECYCLE", "reset_preserves_inventory");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModule Module(*TestRunner, Engine, "ModuleGlobalReset", ASTEST_AS_ANSI(R"AS(
			const double First = 2.0;
			const double Second = 5.0;
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		const int FirstIndex = FindByName(Module, "First");
		const int SecondIndex = FindByName(Module, "Second");
		ASSERT_THAT(AreEqual(0, FirstIndex, TEXT("Module global reset should retain the first declaration index")));
		ASSERT_THAT(AreEqual(1, SecondIndex, TEXT("Module global reset should retain the second declaration index")));
		if (FirstIndex < 0 || SecondIndex < 0)
		{
			return;
		}
		double* const FirstValue = static_cast<double*>(Module->GetAddressOfGlobalVar(FirstIndex));
		double* const SecondValue = static_cast<double*>(Module->GetAddressOfGlobalVar(SecondIndex));
		ASSERT_THAT(IsNotNull(FirstValue, TEXT("Module global reset should expose first global storage")));
		ASSERT_THAT(IsNotNull(SecondValue, TEXT("Module global reset should expose second global storage")));
		if (FirstValue == nullptr || SecondValue == nullptr)
		{
			return;
		}

		*FirstValue = 99.0;
		*SecondValue = -7.0;
		ASSERT_THAT(AreEqual(asSUCCESS, Module->ResetGlobalVars(), TEXT("Module globals should reset successfully")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Module global reset should preserve declarations")));
		ASSERT_THAT(IsNear(99.0, *FirstValue, 0.0001, TEXT("Module global reset should preserve native mutation of a pure constant")));
		ASSERT_THAT(IsNear(-7.0, *SecondValue, 0.0001, TEXT("Module global reset should skip reinitializing every pure constant")));
		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] ResetGlobalVars returns success but CallInit deliberately skips pure constant globals, so direct storage mutations remain visible"));
	}

	TEST_METHOD(RemoveBeforeDiscard)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT_PART("MOD-GLOBAL-STATE-LIFECYCLE", "remove_reindexes_inventory");

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModule Module(*TestRunner, Engine, "ModuleGlobalRemove", ASTEST_AS_ANSI(R"AS(
			const int First = 1;
			const int Second = 2;
		)AS"));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(Module->RemoveGlobalVar(0) >= 0, TEXT("Module globals should allow explicit removal before discard")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Module global removal should update enumeration")));
		ASSERT_THAT(AreEqual(-1, FindByName(Module, "First"), TEXT("Module global removal should erase the selected declaration")));
		ASSERT_THAT(AreEqual(0, FindByName(Module, "Second"), TEXT("Module global removal should reindex the retained declaration")));
		const int RemainingIndex = FindByName(Module, "Second");
		if (RemainingIndex < 0)
		{
			return;
		}
		const int32* const RemainingValue = static_cast<const int32*>(Module->GetAddressOfGlobalVar(RemainingIndex));
		ASSERT_THAT(IsNotNull(RemainingValue, TEXT("Module global removal should retain the remaining storage")));
		if (RemainingValue != nullptr)
		{
			ASSERT_THAT(AreEqual(2, *RemainingValue, TEXT("Module global removal should preserve the remaining initializer")));
		}
	}
};
#endif
