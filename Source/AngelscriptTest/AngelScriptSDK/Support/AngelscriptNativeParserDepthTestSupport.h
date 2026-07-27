#pragma once

#include "AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

template <typename TDerived, typename TAsserter>
class TParserDepthLifecycleTestSupport : public TTest<TDerived, TAsserter>
{
protected:
	static bool ParseScriptCase(
		FNoDiscardAsserter& Assert,
		asCScriptEngine* ScriptEngine,
		const FString& ModuleName,
		const FString& Source,
		asCScriptNode*& OutRoot,
		TUniquePtr<asCBuilder>& BuilderStorage,
		TUniquePtr<AngelscriptNativeTestSupport::FParserAccessor>& ParserStorage)
	{
		using namespace AngelscriptNativeTestSupport;

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asCModule* Module = CreateSdkModule(ScriptEngine, ModuleNameUtf8.Get());
		if (!Assert.IsNotNull(Module, FString::Printf(TEXT("%s should create a parser module"), *ModuleName)))
		{
			return false;
		}

		BuilderStorage = MakeUnique<asCBuilder>(ScriptEngine, Module);
		asCScriptCode Code;
		Code.SetCode(ModuleNameUtf8.Get(), SourceUtf8.Get(), true);
		ParserStorage = MakeUnique<FParserAccessor>(BuilderStorage.Get());
		const int ParseResult = ParserStorage->ParseScript(&Code);
		if (!Assert.AreEqual(0, ParseResult, FString::Printf(TEXT("%s should parse successfully"), *ModuleName)))
		{
			return false;
		}

		OutRoot = ParserStorage->GetScriptNode();
		return Assert.IsNotNull(OutRoot, FString::Printf(TEXT("%s should produce a script root"), *ModuleName));
	}

	static bool ReleaseParserCase(
		FNoDiscardAsserter& Assert,
		asCScriptEngine* ScriptEngine,
		const FString& ModuleName,
		asCScriptNode*& Root,
		TUniquePtr<AngelscriptNativeTestSupport::FParserAccessor>& ParserStorage,
		TUniquePtr<asCBuilder>& BuilderStorage)
	{
		Root = nullptr;
		ParserStorage.Reset();
		BuilderStorage.Reset();

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const int DiscardResult = ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		const bool bDiscarded = Assert.AreEqual(
			asSUCCESS,
			DiscardResult,
			FString::Printf(TEXT("%s should discard after releasing its parser tree"), *ModuleName));
		const bool bLookupCleared = Assert.IsNull(
			ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			FString::Printf(TEXT("%s should leave no name-visible parser module"), *ModuleName));
		return bDiscarded && bLookupCleared;
	}

	static bool ValidateSiblingLinks(const asCScriptNode* Root)
	{
		if (Root == nullptr || Root->parent != nullptr || Root->prev != nullptr)
		{
			return false;
		}

		for (const asCScriptNode* Current = Root; Current != nullptr; Current = Current->next)
		{
			if (Current->next != nullptr && Current->next->prev != Current)
			{
				return false;
			}
			if (Current->firstChild != nullptr && Current->firstChild->parent != Current)
			{
				return false;
			}
		}

		return true;
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
