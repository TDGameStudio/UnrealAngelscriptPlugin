#include "CQTest.h"

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"
#include "ClassGenerator/ASClass.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotFixture.h"
#include "StaticJIT/AOT/AngelscriptStaticJITAotGeneration.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_context.h"
#include "source/as_objecttype.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptStaticJITAotUASFunctionDispatchTests;
struct FAngelscriptStaticJITAotMultiEngineTests;

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotTests,
	"Angelscript.TestModule.StaticJIT.AOT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	friend struct FAngelscriptStaticJITAotUASFunctionDispatchTests;
	friend struct FAngelscriptStaticJITAotMultiEngineTests;

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

	struct FDoubleInt64ConversionResults
	{
		int64 SignedValue = 0;
		uint64 UnsignedValue = 0;
	};

	static void DiscardActiveModules(FAngelscriptEngine& Engine)
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	}

	static bool FunctionContainsOpcode(asCScriptFunction& Function, const asEBCInstr ExpectedOpcode)
	{
		asUINT BytecodeLength = 0;
		const asDWORD* Bytecode = Function.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (Opcode == ExpectedOpcode)
			{
				return true;
			}

			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				break;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				break;
			}

			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	static bool ExecuteDoubleArgumentFunction(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		asCScriptFunction& Function,
		const double Input,
		const TCHAR* Label,
		asQWORD& OutRawResult)
	{
		FNoDiscardAsserter LocalAssert(Test);
		asIScriptContext* Context = ScriptEngine.CreateContext();
		if (!LocalAssert.IsNotNull(Context, *FString::Printf(TEXT("%s should create an execution context"), Label)))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (!LocalAssert.AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->Prepare(&Function),
				*FString::Printf(TEXT("%s should prepare the conversion function"), Label))
			|| !LocalAssert.AreEqual(
				static_cast<int32>(asSUCCESS),
				Context->SetArgDouble(0, Input),
				*FString::Printf(TEXT("%s should bind the runtime double argument"), Label))
			|| !LocalAssert.AreEqual(
				static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*FString::Printf(TEXT("%s should finish conversion execution"), Label)))
		{
			return false;
		}

		OutRawResult = Context->GetReturnQWord();
		return true;
	}

	static asCScriptFunction* FindEntryFunction(FAngelscriptEngine& Engine)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(AngelscriptStaticJITAotFixture::GetModuleName().ToString());
		if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
		{
			return nullptr;
		}

		FTCHARToUTF8 EntryDecl(*AngelscriptStaticJITAotFixture::GetEntryDeclaration());
		return static_cast<asCScriptFunction*>(ModuleDesc->ScriptModule->GetFunctionByDecl(EntryDecl.Get()));
	}

	static FString GetFunctionNameFromDeclaration(const FString& Declaration)
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

	static FString GetAvailableMethodDeclarations(asITypeInfo& TypeInfo)
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

	static asCScriptFunction* FindMethodFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Declaration)
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

	static FString GetAvailableGlobalFunctionDeclarations(asIScriptModule& Module)
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

	static asCScriptFunction* FindGlobalFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, const FString& Declaration)
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

	static bool RequireJitEntries(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asCScriptFunction* Function, const TCHAR* Label, uint32& OutFunctionId)
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

	static bool LoadAotFixtureFromPrecompiledData(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		if (!Test.TestTrue(
				TEXT("StaticJIT.AOT should register the object-last native fixture surface before loading"),
				AngelscriptStaticJITAotFixture::RegisterObjectLastNativeSurface(
					*Engine.GetScriptEngine())))
		{
			return false;
		}

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

	static bool RunGeneratedOutputVerify(FAutomationTestBase& Test);
	static bool RunRuntimeRegistration(FAutomationTestBase& Test);
	static bool RunRuntimeExecution(FAutomationTestBase& Test);
	static bool RunUASFunctionJitEntryAttachment(FAutomationTestBase& Test);
	static bool RunUASFunctionRuntimeCallEvent(FAutomationTestBase& Test);
	static bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test);
	static bool RunMultiEngineSequentialLoad(FAutomationTestBase& Test);

public:
	TEST_METHOD(GeneratedOutputVerify)
	{
		ASSERT_THAT(IsTrue(RunGeneratedOutputVerify(*TestRunner)));
	}

	TEST_METHOD(RuntimeRegistersGeneratedFunction)
	{
		ASSERT_THAT(IsTrue(RunRuntimeRegistration(*TestRunner)));
	}

	TEST_METHOD(RuntimeExecuteUsesGeneratedEntry)
	{
		ASSERT_THAT(IsTrue(RunRuntimeExecution(*TestRunner)));
	}

	TEST_METHOD(DoubleInt64ConversionsMatchInterpreter)
	{
		constexpr double SignedInput = -4294967296.75;
		constexpr double UnsignedInput = 4294967296.75;
		constexpr int64 ExpectedSigned = -4294967296LL;
		constexpr uint64 ExpectedUnsigned = 4294967296ULL;
		FDoubleInt64ConversionResults InterpreterResults;

		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-STATICJIT-AOT-SOURCE-BEGIN][DoubleInt64ConversionsMatchInterpreter]\n%s[AS-STATICJIT-AOT-SOURCE-END][DoubleInt64ConversionsMatchInterpreter]"),
			*AngelscriptStaticJITAotFixture::GetScriptSource()));

		{
			TUniquePtr<FAngelscriptEngine> InterpreterEngine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(InterpreterEngine.Get(), TEXT("StaticJIT.AOT parity should create an isolated interpreter engine")));
			FAngelscriptEngineScope InterpreterScope(*InterpreterEngine);
			ON_SCOPE_EXIT
			{
				DiscardActiveModules(*InterpreterEngine);
			};
			ASSERT_THAT(IsTrue(
				AngelscriptStaticJITAotFixture::RegisterObjectLastNativeSurface(
					*InterpreterEngine->GetScriptEngine()),
				TEXT("StaticJIT.AOT parity should register the fixture object-last native surface")));

			ASSERT_THAT(IsTrue(
				CompileAnnotatedModuleFromMemory(
					InterpreterEngine.Get(),
					AngelscriptStaticJITAotFixture::GetModuleName(),
					AngelscriptStaticJITAotFixture::GetSourceFilename(),
					AngelscriptStaticJITAotFixture::GetScriptSource()),
				TEXT("StaticJIT.AOT parity should compile the fixture for interpreter execution")));

			asCScriptFunction* SignedFunction = FindGlobalFunction(
				*TestRunner,
				*InterpreterEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToInt64Declaration());
			asCScriptFunction* UnsignedFunction = FindGlobalFunction(
				*TestRunner,
				*InterpreterEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToUint64Declaration());
			ASSERT_THAT(IsNotNull(SignedFunction, TEXT("Interpreter should resolve the signed double conversion")));
			ASSERT_THAT(IsNotNull(UnsignedFunction, TEXT("Interpreter should resolve the unsigned double conversion")));

			const int32 DoubleTypeId = InterpreterEngine->GetScriptEngine()->GetTypeIdByDecl("double");
			int32 SignedParameterTypeId = asINVALID_TYPE;
			int32 UnsignedParameterTypeId = asINVALID_TYPE;
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), SignedFunction->GetParamCount(), TEXT("Signed interpreter conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), UnsignedFunction->GetParamCount(), TEXT("Unsigned interpreter conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(asSUCCESS, SignedFunction->GetParam(0, &SignedParameterTypeId), TEXT("Signed interpreter parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(asSUCCESS, UnsignedFunction->GetParam(0, &UnsignedParameterTypeId), TEXT("Unsigned interpreter parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(DoubleTypeId, SignedParameterTypeId, TEXT("Signed interpreter conversion should accept double")));
			ASSERT_THAT(AreEqual(DoubleTypeId, UnsignedParameterTypeId, TEXT("Unsigned interpreter conversion should accept double")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT64), SignedFunction->GetReturnTypeId(), TEXT("Signed interpreter conversion should return int64")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_UINT64), UnsignedFunction->GetReturnTypeId(), TEXT("Unsigned interpreter conversion should return uint64")));

			ASSERT_THAT(IsNull(SignedFunction->jitFunction, TEXT("Signed interpreter conversion should not attach jitFunction")));
			ASSERT_THAT(IsNull(SignedFunction->jitFunction_Raw, TEXT("Signed interpreter conversion should not attach jitFunction_Raw")));
			ASSERT_THAT(IsNull(SignedFunction->jitFunction_ParmsEntry, TEXT("Signed interpreter conversion should not attach jitFunction_ParmsEntry")));
			ASSERT_THAT(IsNull(UnsignedFunction->jitFunction, TEXT("Unsigned interpreter conversion should not attach jitFunction")));
			ASSERT_THAT(IsNull(UnsignedFunction->jitFunction_Raw, TEXT("Unsigned interpreter conversion should not attach jitFunction_Raw")));
			ASSERT_THAT(IsNull(UnsignedFunction->jitFunction_ParmsEntry, TEXT("Unsigned interpreter conversion should not attach jitFunction_ParmsEntry")));
			ASSERT_THAT(IsTrue(FunctionContainsOpcode(*SignedFunction, asBC_dTOi64), TEXT("Signed interpreter conversion should retain asBC_dTOi64 bytecode")));
			ASSERT_THAT(IsTrue(FunctionContainsOpcode(*UnsignedFunction, asBC_dTOu64), TEXT("Unsigned interpreter conversion should retain asBC_dTOu64 bytecode")));

			asQWORD SignedRawResult = 0;
			asQWORD UnsignedRawResult = 0;
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*InterpreterEngine->GetScriptEngine(),
				*SignedFunction,
				SignedInput,
				TEXT("Signed interpreter conversion"),
				SignedRawResult)));
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*InterpreterEngine->GetScriptEngine(),
				*UnsignedFunction,
				UnsignedInput,
				TEXT("Unsigned interpreter conversion"),
				UnsignedRawResult)));
			FMemory::Memcpy(&InterpreterResults.SignedValue, &SignedRawResult, sizeof(InterpreterResults.SignedValue));
			InterpreterResults.UnsignedValue = static_cast<uint64>(UnsignedRawResult);
			ASSERT_THAT(AreEqual(ExpectedSigned, InterpreterResults.SignedValue, TEXT("Interpreter signed conversion should truncate toward zero exactly")));
			ASSERT_THAT(AreEqual(ExpectedUnsigned, InterpreterResults.UnsignedValue, TEXT("Interpreter unsigned conversion should preserve the 64-bit value exactly")));
			DiscardActiveModules(*InterpreterEngine);
			ASSERT_THAT(AreEqual(0, InterpreterEngine->GetActiveModules().Num(), TEXT("Interpreter parity phase should discard every active module")));
		}

		{
			TUniquePtr<FAngelscriptEngine> AotEngine = CreateIsolatedFullEngine();
			ASSERT_THAT(IsNotNull(AotEngine.Get(), TEXT("StaticJIT.AOT parity should create an isolated loaded-AOT engine")));
			FAngelscriptEngineScope AotScope(*AotEngine);
			ON_SCOPE_EXIT
			{
				DiscardActiveModules(*AotEngine);
			};

			ASSERT_THAT(IsTrue(
				LoadAotFixtureFromPrecompiledData(*TestRunner, *AotEngine),
				TEXT("StaticJIT.AOT parity should load the precompiled fixture")));

			asCScriptFunction* SignedFunction = FindGlobalFunction(
				*TestRunner,
				*AotEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToInt64Declaration());
			asCScriptFunction* UnsignedFunction = FindGlobalFunction(
				*TestRunner,
				*AotEngine,
				AngelscriptStaticJITAotFixture::GetDoubleToUint64Declaration());
			ASSERT_THAT(IsNotNull(SignedFunction, TEXT("Loaded AOT should resolve the signed double conversion")));
			ASSERT_THAT(IsNotNull(UnsignedFunction, TEXT("Loaded AOT should resolve the unsigned double conversion")));

			const int32 DoubleTypeId = AotEngine->GetScriptEngine()->GetTypeIdByDecl("double");
			int32 SignedParameterTypeId = asINVALID_TYPE;
			int32 UnsignedParameterTypeId = asINVALID_TYPE;
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), SignedFunction->GetParamCount(), TEXT("Signed loaded-AOT conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1), UnsignedFunction->GetParamCount(), TEXT("Unsigned loaded-AOT conversion should expose one parameter")));
			ASSERT_THAT(AreEqual(asSUCCESS, SignedFunction->GetParam(0, &SignedParameterTypeId), TEXT("Signed loaded-AOT parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(asSUCCESS, UnsignedFunction->GetParam(0, &UnsignedParameterTypeId), TEXT("Unsigned loaded-AOT parameter metadata should be readable")));
			ASSERT_THAT(AreEqual(DoubleTypeId, SignedParameterTypeId, TEXT("Signed loaded-AOT conversion should accept double")));
			ASSERT_THAT(AreEqual(DoubleTypeId, UnsignedParameterTypeId, TEXT("Unsigned loaded-AOT conversion should accept double")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_INT64), SignedFunction->GetReturnTypeId(), TEXT("Signed loaded-AOT conversion should return int64")));
			ASSERT_THAT(AreEqual(static_cast<int32>(asTYPEID_UINT64), UnsignedFunction->GetReturnTypeId(), TEXT("Unsigned loaded-AOT conversion should return uint64")));

			uint32 SignedFunctionId = 0;
			uint32 UnsignedFunctionId = 0;
			ASSERT_THAT(IsTrue(RequireJitEntries(
				*TestRunner,
				*AotEngine,
				SignedFunction,
				TEXT("signed double-to-int64 conversion"),
				SignedFunctionId)));
			ASSERT_THAT(IsTrue(RequireJitEntries(
				*TestRunner,
				*AotEngine,
				UnsignedFunction,
				TEXT("unsigned double-to-uint64 conversion"),
				UnsignedFunctionId)));

			FStaticJITDiagnostics::ResetEntryCounters();
			asQWORD SignedRawResult = 0;
			asQWORD UnsignedRawResult = 0;
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*AotEngine->GetScriptEngine(),
				*SignedFunction,
				SignedInput,
				TEXT("Signed loaded-AOT conversion"),
				SignedRawResult)));
			ASSERT_THAT(IsTrue(ExecuteDoubleArgumentFunction(
				*TestRunner,
				*AotEngine->GetScriptEngine(),
				*UnsignedFunction,
				UnsignedInput,
				TEXT("Unsigned loaded-AOT conversion"),
				UnsignedRawResult)));

			FDoubleInt64ConversionResults AotResults;
			FMemory::Memcpy(&AotResults.SignedValue, &SignedRawResult, sizeof(AotResults.SignedValue));
			AotResults.UnsignedValue = static_cast<uint64>(UnsignedRawResult);
			ASSERT_THAT(AreEqual(ExpectedSigned, AotResults.SignedValue, TEXT("Loaded AOT signed conversion should truncate toward zero exactly")));
			ASSERT_THAT(AreEqual(ExpectedUnsigned, AotResults.UnsignedValue, TEXT("Loaded AOT unsigned conversion should preserve the 64-bit value exactly")));
			ASSERT_THAT(AreEqual(InterpreterResults.SignedValue, AotResults.SignedValue, TEXT("Loaded AOT signed conversion should match the interpreter exactly")));
			ASSERT_THAT(AreEqual(InterpreterResults.UnsignedValue, AotResults.UnsignedValue, TEXT("Loaded AOT unsigned conversion should match the interpreter exactly")));
			ASSERT_THAT(AreEqual(1, FStaticJITDiagnostics::GetEntryCount(SignedFunctionId), TEXT("Signed conversion should execute its generated entry exactly once")));
			ASSERT_THAT(AreEqual(1, FStaticJITDiagnostics::GetEntryCount(UnsignedFunctionId), TEXT("Unsigned conversion should execute its generated entry exactly once")));
			DiscardActiveModules(*AotEngine);
			ASSERT_THAT(AreEqual(0, AotEngine->GetActiveModules().Num(), TEXT("Loaded-AOT parity phase should discard every active module")));
		}
	}

	TEST_METHOD(ObjectLastNativeConstructorUsesGeneratedEntry)
	{
		TestRunner->AddInfo(FString::Printf(
			TEXT("[AS-STATICJIT-AOT-SOURCE-BEGIN][ObjectLastNativeConstructorUsesGeneratedEntry]\n%s[AS-STATICJIT-AOT-SOURCE-END][ObjectLastNativeConstructorUsesGeneratedEntry]"),
			*AngelscriptStaticJITAotFixture::GetScriptSource()));

		TUniquePtr<FAngelscriptEngine> AotEngine = CreateIsolatedFullEngine();
		ASSERT_THAT(IsNotNull(
			AotEngine.Get(),
			TEXT("StaticJIT.AOT object-last test should create an isolated loaded-AOT engine")));
		if (!AotEngine)
		{
			return;
		}
		FAngelscriptEngineScope AotScope(*AotEngine);
		ON_SCOPE_EXIT
		{
			DiscardActiveModules(*AotEngine);
		};

		ASSERT_THAT(IsTrue(
			LoadAotFixtureFromPrecompiledData(*TestRunner, *AotEngine),
			TEXT("StaticJIT.AOT object-last test should load the precompiled fixture")));
		asCScriptFunction* const Function = FindGlobalFunction(
			*TestRunner,
			*AotEngine,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeEntryDeclaration());
		ASSERT_THAT(IsNotNull(
			Function,
			TEXT("StaticJIT.AOT object-last test should resolve its exact generated function")));
		if (Function == nullptr)
		{
			return;
		}

		uint32 FunctionId = 0;
		ASSERT_THAT(IsTrue(
			RequireJitEntries(
				*TestRunner,
				*AotEngine,
				Function,
				TEXT("object-last native constructor"),
				FunctionId),
			TEXT("StaticJIT.AOT object-last function should expose every generated entry")));

		AngelscriptStaticJITAotFixture::ResetObjectLastNativeObservation();
		FStaticJITDiagnostics::ResetEntryCounters();
		asIScriptContext* const Context =
			AotEngine->GetScriptEngine()->CreateContext();
		ASSERT_THAT(IsNotNull(
			Context,
			TEXT("StaticJIT.AOT object-last test should create an execution context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			Context->Prepare(Function),
			TEXT("StaticJIT.AOT object-last test should prepare its generated function")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			TEXT("StaticJIT.AOT object-last test should execute generated code")));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedObjectLastNativeResult(),
			static_cast<int32>(Context->GetReturnDWord()),
			TEXT("StaticJIT.AOT object-last test should preserve the constructed object result")));
		ASSERT_THAT(AreEqual(
			1,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeCallCount(),
			TEXT("StaticJIT.AOT object-last native constructor should execute exactly once")));
		ASSERT_THAT(AreEqual(
			39,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeLeftSentinel(),
			TEXT("StaticJIT.AOT object-last constructor should receive the first explicit argument")));
		ASSERT_THAT(AreEqual(
			97,
			AngelscriptStaticJITAotFixture::GetObjectLastNativeRightSentinel(),
			TEXT("StaticJIT.AOT object-last constructor should receive the final explicit argument")));
		ASSERT_THAT(AreEqual(
			AngelscriptStaticJITAotFixture::GetExpectedObjectLastNativeResult(),
			AngelscriptStaticJITAotFixture::GetObjectLastNativeObjectValue(),
			TEXT("StaticJIT.AOT object-last constructor should receive destination storage after explicit arguments")));
		ASSERT_THAT(AreEqual(
			1,
			FStaticJITDiagnostics::GetEntryCount(FunctionId),
			TEXT("StaticJIT.AOT object-last scenario should execute its generated entry exactly once")));
	}
};

bool FAngelscriptStaticJITAotTests::RunGeneratedOutputVerify(FAutomationTestBase& Test)
{
	using namespace AngelscriptStaticJITAotGeneration;
	const FStaticJITAotGenerationResult Result = Run(EStaticJITAotGenerationMode::Verify);
	if (!Test.TestTrue(TEXT("StaticJIT.AOT generated output should match the fixture"), Result.bSuccess))
	{
		Test.AddError(Result.Error);
	}
	return Result.bSuccess;
}

bool FAngelscriptStaticJITAotTests::RunRuntimeRegistration(FAutomationTestBase& Test)
{
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

bool FAngelscriptStaticJITAotTests::RunRuntimeExecution(FAutomationTestBase& Test)
{
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

bool FAngelscriptStaticJITAotTests::RunUASFunctionJitEntryAttachment(FAutomationTestBase& Test)
{
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

bool FAngelscriptStaticJITAotTests::RunUASFunctionRuntimeCallEvent(FAutomationTestBase& Test)
{
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

bool FAngelscriptStaticJITAotTests::RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test)
{
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

bool FAngelscriptStaticJITAotTests::RunMultiEngineSequentialLoad(FAutomationTestBase& Test)
{
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

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotUASFunctionDispatchTests,
	"Angelscript.TestModule.StaticJIT.AOT.UASFunctionDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExposesJitEntries)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunUASFunctionJitEntryAttachment(*TestRunner)));
	}

	TEST_METHOD(RuntimeCallEventUsesGeneratedJit)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunUASFunctionRuntimeCallEvent(*TestRunner)));
	}

	TEST_METHOD(StaticWorldContextUsesGeneratedJit)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunStaticWorldContextRuntimeCallEvent(*TestRunner)));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotMultiEngineTests,
	"Angelscript.TestModule.StaticJIT.AOT.MultiEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(SequentialLoadsKeepGeneratedRegistryVisible)
	{
		ASSERT_THAT(IsTrue(FAngelscriptStaticJITAotTests::RunMultiEngineSequentialLoad(*TestRunner)));
	}
};

#endif
