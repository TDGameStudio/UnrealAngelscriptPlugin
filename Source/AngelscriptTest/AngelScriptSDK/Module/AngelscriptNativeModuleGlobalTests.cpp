#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace ModuleGlobalTest
{
	static int FindByName(asIScriptModule* Module, const char* Name)
	{
		for (asUINT Index = 0; Module != nullptr && Index < Module->GetGlobalVarCount(); ++Index)
		{
			const char* Current = nullptr;
			if (Module->GetGlobalVar(Index, &Current) >= 0 && Current != nullptr && std::strcmp(Current, Name) == 0) return static_cast<int>(Index);
		}
		return -1;
	}
}

TEST_CLASS_WITH_FLAGS(FModuleGlobalTests, "Angelscript.TestModule.AngelScriptSDK.Module.Globals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ModuleGlobalEnumerate)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine; Engine.Create(*TestRunner); ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "ModuleGlobalEnumerate", "const int a = 1; const double b = 2.0; const double c = 35.2; const uint d = 0xC0DE;");
		if (!Module.IsValid()) return;
		ASSERT_THAT(AreEqual(4, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Module globals should enumerate every declaration")));
		ASSERT_THAT(AreEqual(1, ModuleGlobalTest::FindByName(Module, "b"), TEXT("Module globals should locate a declaration by name")));
		const int Index = ModuleGlobalTest::FindByName(Module, "c");
		const char* Name = nullptr; bool bConst = false;
		ASSERT_THAT(IsTrue(Module->GetGlobalVar(Index, &Name, nullptr, nullptr, &bConst) >= 0, TEXT("Module globals should return metadata")));
		ASSERT_THAT(IsTrue(bConst, TEXT("Module globals should retain const metadata")));
		asUINT* const Value = static_cast<asUINT*>(Module->GetAddressOfGlobalVar(3));
		ASSERT_THAT(IsNotNull(Value, TEXT("Module globals should expose initialized storage")));
		if (Value != nullptr) ASSERT_THAT(AreEqual(static_cast<uint32>(0xC0DE), static_cast<uint32>(*Value), TEXT("Module globals should preserve initializer values")));
	}

	TEST_METHOD(ModuleGlobalResetState)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine; Engine.Create(*TestRunner); ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "ModuleGlobalReset", "const double First = 2.0; const double Second = 5.0;");
		if (!Module.IsValid()) return;
		ASSERT_THAT(AreEqual(asSUCCESS, Module->ResetGlobalVars(), TEXT("Module globals should reset successfully")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Module global reset should preserve declarations")));
	}

	TEST_METHOD(RemoveBeforeDiscard)
	{
		using namespace AngelscriptNativeTestSupport;
		FNativeTestEngine Engine; Engine.Create(*TestRunner); ON_SCOPE_EXIT { Engine.Destroy(); };
		FScopedNativeModule Module(*TestRunner, Engine, "ModuleGlobalRemove", "const int First = 1; const int Second = 2;");
		if (!Module.IsValid()) return;
		ASSERT_THAT(IsTrue(Module->RemoveGlobalVar(0) >= 0, TEXT("Module globals should allow explicit removal before discard")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("Module global removal should update enumeration")));
	}
};
#endif
