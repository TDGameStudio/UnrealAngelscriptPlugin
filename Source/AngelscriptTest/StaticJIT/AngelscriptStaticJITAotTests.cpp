#include "CQTest.h"

#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "ClassGenerator/ASClass.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_StaticJIT_AOT_Private
{
	struct FPrimitiveArgParams
	{
		int32 Value = 0;
	};

	struct FReferenceParams
	{
		int32 Value = 0;
		int32 ReturnValue = 0;
	};

	struct FStaticWorldContextParams
	{
		UObject* WorldContextObject = nullptr;
		int32 Value = 0;
		int32 ReturnValue = 0;
	};

	asCScriptFunction* FindEntryFunction(FAngelscriptEngine& Engine)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			return nullptr;
		}

		FTCHARToUTF8 EntryDecl(*AngelscriptStaticJITAotFixture::GetEntryDeclaration());
		return static_cast<asCScriptFunction*>(ModuleDesc->ScriptModule->GetFunctionByDecl(EntryDecl.Get()));
	}

	FString GetFunctionNameFromDeclaration(const FString& Declaration)
	{
		int32 OpenParenIndex = INDEX_NONE;
		if (!Declaration.FindChar(TEXT('('), OpenParenIndex))
		{
			return FString();
		}

		const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
		int32 NameSeparatorIndex = INDEX_NONE;
		if (!Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
		{
			return Prefix;
		}

		return Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
	}

	FString GetAvailableMethodDeclarations(asITypeInfo& TypeInfo)
	{
		FString AvailableMethods;
		const asUINT MethodCount = TypeInfo.GetMethodCount();
		for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
		{
			asIScriptFunction* CandidateFunction = TypeInfo.GetMethodByIndex(MethodIndex);
			if (CandidateFunction == nullptr)
			{
				continue;
			}

			if (!AvailableMethods.IsEmpty())
			{
				AvailableMethods += TEXT(", ");
			}

			AvailableMethods += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
		}

		return AvailableMethods.IsEmpty() ? TEXT("<none>") : AvailableMethods;
	}

	asCScriptFunction* FindMethodFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Declaration)
	{
		UASClass* GeneratedClass = Cast<UASClass>(FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName()));
		if (GeneratedClass == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve generated class '%s' before looking up method '%s'."),
				*AngelscriptStaticJITAotFixture::GetGeneratedClassName().ToString(),
				*Declaration));
			return nullptr;
		}

		asITypeInfo* TypeInfo = static_cast<asITypeInfo*>(GeneratedClass->ScriptTypePtr);
		if (TypeInfo == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT generated class '%s' has no AngelScript type before looking up method '%s'."),
				*GeneratedClass->GetName(),
				*Declaration));
			return nullptr;
		}

		FTCHARToUTF8 MethodDecl(*Declaration);
		asIScriptFunction* Function = TypeInfo->GetMethodByDecl(MethodDecl.Get());
		if (Function == nullptr)
		{
			const FString MethodName = GetFunctionNameFromDeclaration(Declaration);
			if (!MethodName.IsEmpty())
			{
				FTCHARToUTF8 MethodNameUtf8(*MethodName);
				const asUINT MethodCount = TypeInfo->GetMethodCount();
				for (asUINT MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
				{
					asIScriptFunction* CandidateFunction = TypeInfo->GetMethodByIndex(MethodIndex);
					if (CandidateFunction != nullptr && FCStringAnsi::Strcmp(CandidateFunction->GetName(), MethodNameUtf8.Get()) == 0)
					{
						Function = CandidateFunction;
						break;
					}
				}
			}
		}

		if (Function == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve method '%s'; available methods on '%s': %s"),
				*Declaration,
				UTF8_TO_TCHAR(TypeInfo->GetName()),
				*GetAvailableMethodDeclarations(*TypeInfo)));
		}

		return static_cast<asCScriptFunction*>(Function);
	}

	FString GetAvailableGlobalFunctionDeclarations(asIScriptModule& Module)
	{
		FString AvailableFunctions;
		const asUINT FunctionCount = Module.GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
			if (CandidateFunction == nullptr)
			{
				continue;
			}

			if (!AvailableFunctions.IsEmpty())
			{
				AvailableFunctions += TEXT(", ");
			}

			AvailableFunctions += UTF8_TO_TCHAR(CandidateFunction->GetDeclaration());
		}

		return AvailableFunctions.IsEmpty() ? TEXT("<none>") : AvailableFunctions;
	}

	asCScriptFunction* FindGlobalFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Declaration)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve module '%s' before looking up global function '%s'."),
				*AngelscriptStaticJITAotFixture::GetModuleName().ToString(),
				*Declaration));
			return nullptr;
		}

		FTCHARToUTF8 FunctionDecl(*Declaration);
		asIScriptFunction* Function = ModuleDesc->ScriptModule->GetFunctionByDecl(FunctionDecl.Get());
		if (Function == nullptr)
		{
			const FString FunctionName = GetFunctionNameFromDeclaration(Declaration);
			if (!FunctionName.IsEmpty())
			{
				FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
				const asUINT FunctionCount = ModuleDesc->ScriptModule->GetFunctionCount();
				for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
				{
					asIScriptFunction* CandidateFunction = ModuleDesc->ScriptModule->GetFunctionByIndex(FunctionIndex);
					if (CandidateFunction != nullptr && FCStringAnsi::Strcmp(CandidateFunction->GetName(), FunctionNameUtf8.Get()) == 0)
					{
						Function = CandidateFunction;
						break;
					}
				}
			}
		}

		if (Function == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("StaticJIT.AOT should resolve global function '%s'; available functions in module '%s': %s"),
				*Declaration,
				*AngelscriptStaticJITAotFixture::GetModuleName().ToString(),
				*GetAvailableGlobalFunctionDeclarations(*ModuleDesc->ScriptModule)));
		}

		return static_cast<asCScriptFunction*>(Function);
	}

	bool RequireJitEntries(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asCScriptFunction* Function, const TCHAR* Label, uint32& OutFunctionId)
	{
		if (!Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should resolve a script function"), Label), Function))
		{
			return false;
		}

		if (!Test.TestTrue(*FString::Printf(TEXT("StaticJIT.AOT %s should expose a StaticJIT function id"), Label), FStaticJITDiagnostics::ResolveFunctionId(Engine, Function, OutFunctionId)))
		{
			return false;
		}

		return Test.TestTrue(*FString::Printf(TEXT("StaticJIT.AOT %s should register generated C++"), Label), FStaticJITDiagnostics::IsFunctionRegistered(OutFunctionId))
			&& Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should attach jitFunction"), Label), Function->jitFunction)
			&& Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should attach jitFunction_Raw"), Label), Function->jitFunction_Raw)
			&& Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT %s should attach jitFunction_ParmsEntry"), Label), Function->jitFunction_ParmsEntry);
	}

	bool LoadAotFixtureFromPrecompiledData(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		FString AvailabilityError;
		if (!Test.TestTrue(TEXT("StaticJIT.AOT generated output and local cache should be available before runtime verification"), AngelscriptStaticJITAotFixture::IsGeneratedOutputAvailable(&AvailabilityError)))
		{
			Test.AddError(AvailabilityError);
			return false;
		}

		FString LoadError;
		if (!Test.TestTrue(TEXT("StaticJIT.AOT should load fixture precompiled data"), FStaticJITDiagnostics::LoadPrecompiledData(Engine, AngelscriptStaticJITAotFixture::GetPrecompiledCacheFilename(), &LoadError)))
		{
			Test.AddError(LoadError);
			return false;
		}

		FString CompileError;
		if (!Test.TestTrue(TEXT("StaticJIT.AOT should compile fixture from precompiled data"), FStaticJITDiagnostics::CompileLoadedPrecompiledData(Engine, ECompileType::Initial, &CompileError)))
		{
			Test.AddError(CompileError);
			return false;
		}

		return true;
	}
}

namespace AngelscriptTest_StaticJIT_AOT_Private
{

bool RunGeneratedOutputVerify(FAutomationTestBase& Test)
{
	using namespace AngelscriptStaticJITAotGeneration;
	const FStaticJITAotGenerationResult Result = Run(EStaticJITAotGenerationMode::Verify);
	if (!Test.TestTrue(TEXT("StaticJIT.AOT generated output should match the fixture"), Result.bSuccess))
	{
		Test.AddError(Result.Error);
	}
	return Result.bSuccess;
}

bool RunRuntimeRegistration(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(Test, Engine))
	{
		return false;
	}

	asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT should resolve the fixture entry function"), EntryFunction))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!Test.TestTrue(TEXT("StaticJIT.AOT should map the fixture function to a StaticJIT function id"), FStaticJITDiagnostics::ResolveFunctionId(Engine, EntryFunction, FunctionId)))
	{
		return false;
	}

	Test.TestTrue(TEXT("StaticJIT.AOT should register generated C++ for the fixture function id"), FStaticJITDiagnostics::IsFunctionRegistered(FunctionId));
	Test.TestTrue(TEXT("StaticJIT.AOT should attach a non-null jitFunction to the fixture entry function"), EntryFunction->jitFunction != nullptr);
	return true;
}

bool RunRuntimeExecution(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(Test, Engine))
	{
		return false;
	}

	asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT should resolve the fixture entry function before execution"), EntryFunction))
	{
		return false;
	}

	uint32 FunctionId = 0;
	if (!Test.TestTrue(TEXT("StaticJIT.AOT should expose the fixture StaticJIT function id before execution"), FStaticJITDiagnostics::ResolveFunctionId(Engine, EntryFunction, FunctionId)))
	{
		return false;
	}

	FStaticJITDiagnostics::ResetEntryCounters();

	asIScriptContext* Context = Engine.GetScriptEngine()->CreateContext();
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT should create an execution context"), Context))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	if (!Test.TestEqual(TEXT("StaticJIT.AOT should prepare the fixture entry function"), Context->Prepare(EntryFunction), asSUCCESS))
	{
		return false;
	}

	if (!Test.TestEqual(TEXT("StaticJIT.AOT should execute the fixture entry function"), Context->Execute(), asEXECUTION_FINISHED))
	{
		return false;
	}

	Test.TestEqual(TEXT("StaticJIT.AOT should return the expected fixture result"), static_cast<int32>(Context->GetReturnDWord()), AngelscriptStaticJITAotFixture::GetExpectedEntryResult());
	Test.TestEqual(TEXT("StaticJIT.AOT should mark exactly one generated entry execution"), FStaticJITDiagnostics::GetEntryCount(FunctionId), 1);
	return true;
}

bool RunUASFunctionJitEntryAttachment(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(Test, Engine))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture class"), GeneratedClass))
	{
		return false;
	}

	const TArray<TPair<FString, const TCHAR*>> MethodCases =
	{
		{ AngelscriptStaticJITAotFixture::GetMethodPrimitiveArgDeclaration(), TEXT("primitive argument method") },
		{ AngelscriptStaticJITAotFixture::GetMethodPrimitiveReturnDeclaration(), TEXT("primitive return method") },
		{ AngelscriptStaticJITAotFixture::GetMethodReferenceDeclaration(), TEXT("reference writeback method") },
		{ AngelscriptStaticJITAotFixture::GetMethodObjectReturnDeclaration(), TEXT("object return method") },
	};

	for (const TPair<FString, const TCHAR*>& MethodCase : MethodCases)
	{
		uint32 FunctionId = 0;
		if (!RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, MethodCase.Key), MethodCase.Value, FunctionId))
		{
			return false;
		}
	}

	uint32 StaticFunctionId = 0;
	if (!RequireJitEntries(
			Test,
			Engine,
			FindGlobalFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetStaticWorldContextDeclaration()),
			TEXT("static world-context function"),
			StaticFunctionId))
	{
		return false;
	}

	return true;
}

bool RunUASFunctionRuntimeCallEvent(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(Test, Engine))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture class"), GeneratedClass))
	{
		return false;
	}

	UObject* Instance = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("StaticJITAotFunctionCarrierInstance"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should instantiate the fixture class"), Instance))
	{
		return false;
	}

	UASFunction* StorePrimitiveArgFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("StorePrimitiveArg")));
	UASFunction* ReturnPrimitiveFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ReturnPrimitive")));
	UASFunction* BumpReferenceFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("BumpReference")));
	UASFunction* ReturnSelfObjectFunction = Cast<UASFunction>(FindGeneratedFunction(GeneratedClass, TEXT("ReturnSelfObject")));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose StorePrimitiveArg"), StorePrimitiveArgFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose ReturnPrimitive"), ReturnPrimitiveFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose BumpReference"), BumpReferenceFunction)
		|| !Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose ReturnSelfObject"), ReturnSelfObjectFunction))
	{
		return false;
	}

	uint32 StorePrimitiveArgId = 0;
	uint32 ReturnPrimitiveId = 0;
	uint32 BumpReferenceId = 0;
	uint32 ReturnSelfObjectId = 0;
	if (!RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodPrimitiveArgDeclaration()), TEXT("primitive argument method"), StorePrimitiveArgId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodPrimitiveReturnDeclaration()), TEXT("primitive return method"), ReturnPrimitiveId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodReferenceDeclaration()), TEXT("reference writeback method"), BumpReferenceId)
		|| !RequireJitEntries(Test, Engine, FindMethodFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetMethodObjectReturnDeclaration()), TEXT("object return method"), ReturnSelfObjectId))
	{
		return false;
	}

	FStaticJITDiagnostics::ResetEntryCounters();

	FPrimitiveArgParams PrimitiveArgParams;
	PrimitiveArgParams.Value = 41;
	StorePrimitiveArgFunction->RuntimeCallEvent(Instance, &PrimitiveArgParams);
	FIntProperty* StoredValueProperty = FindFProperty<FIntProperty>(GeneratedClass, TEXT("StoredValue"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose StoredValue"), StoredValueProperty))
	{
		return false;
	}
	Test.TestEqual(TEXT("RuntimeCallEvent JIT primitive arg should update object state"), StoredValueProperty->GetPropertyValue_InContainer(Instance), AngelscriptStaticJITAotFixture::GetExpectedPrimitiveArgStoredValue());
	Test.TestEqual(TEXT("RuntimeCallEvent primitive arg should mark generated JIT entry"), FStaticJITDiagnostics::GetEntryCount(StorePrimitiveArgId), 1);

	FStructOnScope PrimitiveReturnParams(ReturnPrimitiveFunction);
	ReturnPrimitiveFunction->RuntimeCallEvent(Instance, PrimitiveReturnParams.GetStructMemory());
	FIntProperty* PrimitiveReturnProperty = CastField<FIntProperty>(ReturnPrimitiveFunction->GetReturnProperty());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose primitive return property"), PrimitiveReturnProperty))
	{
		return false;
	}
	Test.TestEqual(TEXT("RuntimeCallEvent JIT primitive return should write reflected return value"), PrimitiveReturnProperty->GetPropertyValue_InContainer(PrimitiveReturnParams.GetStructMemory()), AngelscriptStaticJITAotFixture::GetExpectedPrimitiveReturnValue());
	Test.TestEqual(TEXT("RuntimeCallEvent primitive return should mark generated JIT entry"), FStaticJITDiagnostics::GetEntryCount(ReturnPrimitiveId), 1);

	FReferenceParams ReferenceParams;
	ReferenceParams.Value = 13;
	BumpReferenceFunction->RuntimeCallEvent(Instance, &ReferenceParams);
	Test.TestEqual(TEXT("RuntimeCallEvent JIT reference arg should write back parameter memory"), ReferenceParams.Value, AngelscriptStaticJITAotFixture::GetExpectedReferenceReturnValue());
	Test.TestEqual(TEXT("RuntimeCallEvent JIT reference arg should write reflected return value"), ReferenceParams.ReturnValue, AngelscriptStaticJITAotFixture::GetExpectedReferenceReturnValue());
	Test.TestEqual(TEXT("RuntimeCallEvent reference arg should mark generated JIT entry"), FStaticJITDiagnostics::GetEntryCount(BumpReferenceId), 1);

	FStructOnScope ObjectReturnParams(ReturnSelfObjectFunction);
	ReturnSelfObjectFunction->RuntimeCallEvent(Instance, ObjectReturnParams.GetStructMemory());
	FObjectProperty* ObjectReturnProperty = CastField<FObjectProperty>(ReturnSelfObjectFunction->GetReturnProperty());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose object return property"), ObjectReturnProperty))
	{
		return false;
	}
	Test.TestTrue(TEXT("RuntimeCallEvent JIT object return should preserve object identity"), ObjectReturnProperty->GetObjectPropertyValue_InContainer(ObjectReturnParams.GetStructMemory()) == Instance);
	Test.TestEqual(TEXT("RuntimeCallEvent object return should mark generated JIT entry"), FStaticJITDiagnostics::GetEntryCount(ReturnSelfObjectId), 1);

	return true;
}

bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	FAngelscriptEngineScope EngineScope(Engine);

	if (!LoadAotFixtureFromPrecompiledData(Test, Engine))
	{
		return false;
	}

	UClass* StaticsClass = FindGeneratedClass(&Engine, TEXT("UModule_ASStaticJITAotFixtureStatics"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture statics class"), StaticsClass))
	{
		return false;
	}

	UClass* GeneratedClass = FindGeneratedClass(&Engine, AngelscriptStaticJITAotFixture::GetGeneratedClassName());
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should generate the fixture class for world context"), GeneratedClass))
	{
		return false;
	}

	UASFunction* StaticWorldContextFunction = Cast<UASFunction>(FindGeneratedFunction(StaticsClass, TEXT("StaticWorldContextCheck")));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should expose StaticWorldContextCheck"), StaticWorldContextFunction))
	{
		return false;
	}

	uint32 StaticWorldContextId = 0;
	if (!RequireJitEntries(
			Test,
			Engine,
			FindGlobalFunction(Test, Engine, AngelscriptStaticJITAotFixture::GetStaticWorldContextDeclaration()),
			TEXT("static world-context function"),
			StaticWorldContextId))
	{
		return false;
	}

	FStaticJITDiagnostics::ResetEntryCounters();

	UObject* WorldContextObject = NewObject<UObject>(GetTransientPackage(), GeneratedClass, TEXT("StaticJITAotWorldContextObject"));
	if (!Test.TestNotNull(TEXT("StaticJIT.AOT UASFunction dispatch should instantiate a concrete world-context object"), WorldContextObject))
	{
		return false;
	}

	FStaticWorldContextParams Params;
	Params.WorldContextObject = WorldContextObject;
	Params.Value = 31;
	StaticWorldContextFunction->RuntimeCallEvent(StaticsClass->GetDefaultObject(), &Params);

	Test.TestEqual(TEXT("RuntimeCallEvent JIT static world-context should preserve script result"), Params.ReturnValue, AngelscriptStaticJITAotFixture::GetExpectedStaticWorldContextResult());
	Test.TestEqual(TEXT("RuntimeCallEvent static world-context should mark generated JIT entry"), FStaticJITDiagnostics::GetEntryCount(StaticWorldContextId), 1);
	return true;
}

bool RunMultiEngineSequentialLoad(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_StaticJIT_AOT_Private;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		if (!LoadAotFixtureFromPrecompiledData(Test, Engine))
		{
			return false;
		}

		asCScriptFunction* EntryFunction = FindEntryFunction(Engine);
		if (!Test.TestNotNull(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should resolve the fixture entry function"), Index), EntryFunction))
		{
			return false;
		}

		uint32 FunctionId = 0;
		if (!Test.TestTrue(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should expose the fixture function id"), Index), FStaticJITDiagnostics::ResolveFunctionId(Engine, EntryFunction, FunctionId)))
		{
			return false;
		}

		if (!Test.TestTrue(*FString::Printf(TEXT("StaticJIT.AOT multi-engine pass %d should keep generated registry visible"), Index), FStaticJITDiagnostics::IsFunctionRegistered(FunctionId)))
		{
			return false;
		}
	}

	return true;
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotTests,
	"Angelscript.TestModule.StaticJIT.AOT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedOutputVerify)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunGeneratedOutputVerify(*TestRunner)));
	}

	TEST_METHOD(RuntimeRegistersGeneratedFunction)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunRuntimeRegistration(*TestRunner)));
	}

	TEST_METHOD(RuntimeExecuteUsesGeneratedEntry)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunRuntimeExecution(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotUASFunctionDispatchTests,
	"Angelscript.TestModule.StaticJIT.AOT.UASFunctionDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExposesJitEntries)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunUASFunctionJitEntryAttachment(*TestRunner)));
	}

	TEST_METHOD(RuntimeCallEventUsesGeneratedJit)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunUASFunctionRuntimeCallEvent(*TestRunner)));
	}

	TEST_METHOD(StaticWorldContextUsesGeneratedJit)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunStaticWorldContextRuntimeCallEvent(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotMultiEngineTests,
	"Angelscript.TestModule.StaticJIT.AOT.MultiEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SequentialLoadsKeepGeneratedRegistryVisible)
	{
		using namespace AngelscriptTest_StaticJIT_AOT_Private;
		ASSERT_THAT(IsTrue(RunMultiEngineSequentialLoad(*TestRunner)));
	}
};

#endif
