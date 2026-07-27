#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FScriptClassSaveLoadLifecycleTests,
					  "Angelscript.TestModule.AngelScriptSDK.Module.ScriptClassSaveLoadLifecycle",
					  EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
  private:
	struct FLifecycleCase
	{
		const TCHAR* CatalogName;
		bool bRetainPredecessorEntry;
	};

	inline static constexpr FLifecycleCase LifecycleCases[] = {
		{TEXT("release-predecessor-before-source-discard"), false},
		{TEXT("retain-predecessor-across-destination-load"), true},
	};

	static FString BuildScriptClassSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("class FSaveLoadProbeBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 11;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Padding = 5;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class FSaveLoadProbeDerived : FSaveLoadProbeBase"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint DerivedValue = 7;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ScriptClassSaveLoadEntry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFSaveLoadProbeDerived Receiver = FSaveLoadProbeDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value + Receiver.Padding + Receiver.DerivedValue;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ScriptClassSaveLoadReadValue()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFSaveLoadProbeDerived Receiver = FSaveLoadProbeDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ScriptClassSaveLoadReadPadding()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFSaveLoadProbeDerived Receiver = FSaveLoadProbeDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.Padding;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ScriptClassSaveLoadReadDerivedValue()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tFSaveLoadProbeDerived Receiver = FSaveLoadProbeDerived();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Receiver.DerivedValue;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static void ReportLifecycleInfo(FAutomationTestBase & Test, const FString& Message)
	{
		Test.AddInfo(Message);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	}

	static bool ExecuteEntry(FAutomationTestBase & Test, asIScriptEngine * ScriptEngine, asIScriptModule * Module,
							 const char* EntryDeclaration, int32 ExpectedReturnValue, const FString& Description)
	{
		using namespace AngelscriptNativeTestSupport;

		FNoDiscardAsserter LocalAssert(Test);
		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, EntryDeclaration);
		if (!LocalAssert.IsNotNull(Entry, *(Description + TEXT(" should publish its exact script-class entry"))))
		{
			return false;
		}

		asIScriptContext* const Context = ScriptEngine != nullptr ? ScriptEngine->CreateContext() : nullptr;
		if (!LocalAssert.IsNotNull(Context, *(Description + TEXT(" should create an execution context"))))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Entry);
		const int32 ReturnValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();
		ReportLifecycleInfo(
			Test, FString::Printf(TEXT("[AS-LIFECYCLE-RETURN] Description=%s ExecuteResult=%d ReturnValue=%d"),
								  *Description, ExecuteResult, ReturnValue));
		const bool bExecutionFinished =
			LocalAssert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED), ExecuteResult,
								 *(Description + TEXT(" should execute after its exact lifecycle stage")));
		const bool bValuePreserved = LocalAssert.AreEqual(
			ExpectedReturnValue, ReturnValue, *(Description + TEXT(" should preserve stored inherited properties")));
		if (!bValuePreserved)
		{
			return bExecutionFinished;
		}
		return bExecutionFinished;
	}

	static bool ExecuteEntries(FAutomationTestBase & Test, asIScriptEngine * ScriptEngine, asIScriptModule * Module,
							   const FString& Description)
	{
		bool bAllExecutionsFinished = true;
		bAllExecutionsFinished = ExecuteEntry(Test, ScriptEngine, Module, "int ScriptClassSaveLoadEntry()", 23,
											  Description + TEXT(" sum entry")) &&
								 bAllExecutionsFinished;
		bAllExecutionsFinished = ExecuteEntry(Test, ScriptEngine, Module, "int ScriptClassSaveLoadReadValue()", 11,
											  Description + TEXT(" value entry")) &&
								 bAllExecutionsFinished;
		bAllExecutionsFinished = ExecuteEntry(Test, ScriptEngine, Module, "int ScriptClassSaveLoadReadPadding()", 5,
											  Description + TEXT(" padding entry")) &&
								 bAllExecutionsFinished;
		bAllExecutionsFinished = ExecuteEntry(Test, ScriptEngine, Module, "int ScriptClassSaveLoadReadDerivedValue()",
											  7, Description + TEXT(" derived-value entry")) &&
								 bAllExecutionsFinished;
		return bAllExecutionsFinished;
	}

	static void AssertExpectedTypeLayouts(FAutomationTestBase & Test, asIScriptModule * Module, const FString& CaseId,
										  const TCHAR* Stage)
	{
		struct FExpectedProperty
		{
			const char* Name;
			int32 Offset;
			bool bInherited;
		};

		struct FExpectedType
		{
			const char* Declaration;
			int32 Size;
			const FExpectedProperty* Properties;
			int32 PropertyCount;
		};

		static constexpr FExpectedProperty BaseProperties[] = {
			{"Value", 0, false},
			{"Padding", 4, false},
		};
		static constexpr FExpectedProperty DerivedProperties[] = {
			{"Value", 0, true},
			{"Padding", 4, true},
			{"DerivedValue", 8, false},
		};
		static const FExpectedType ExpectedTypes[] = {
			{"FSaveLoadProbeBase", 8, BaseProperties, UE_ARRAY_COUNT(BaseProperties)},
			{"FSaveLoadProbeDerived", 16, DerivedProperties, UE_ARRAY_COUNT(DerivedProperties)},
		};

		FNoDiscardAsserter LocalAssert(Test);
		bool bAllAssertionsPassed = true;
		for (const FExpectedType& ExpectedType : ExpectedTypes)
		{
			asITypeInfo* const TypeInfo =
				Module != nullptr ? Module->GetTypeInfoByDecl(ExpectedType.Declaration) : nullptr;
			const FString TypeName = UTF8_TO_TCHAR(ExpectedType.Declaration);
			const bool bTypeWasFound = LocalAssert.IsNotNull(
				TypeInfo, *(CaseId + FString::Printf(TEXT(" %s should expose %s type metadata"), Stage, *TypeName)));
			bAllAssertionsPassed &= bTypeWasFound;
			if (!bTypeWasFound)
			{
				continue;
			}

			bAllAssertionsPassed &= LocalAssert.AreEqual(
				ExpectedType.PropertyCount, static_cast<int32>(TypeInfo->GetPropertyCount()),
				*(CaseId + FString::Printf(TEXT(" %s should preserve %s property count"), Stage, *TypeName)));
			bAllAssertionsPassed &= LocalAssert.AreEqual(
				ExpectedType.Size, static_cast<int32>(TypeInfo->GetSize()),
				*(CaseId + FString::Printf(TEXT(" %s should preserve %s type size"), Stage, *TypeName)));

			for (int32 ExpectedPropertyIndex = 0; ExpectedPropertyIndex < ExpectedType.PropertyCount;
				 ++ExpectedPropertyIndex)
			{
				const FExpectedProperty& ExpectedProperty = ExpectedType.Properties[ExpectedPropertyIndex];
				int32 FoundPropertyIndex = INDEX_NONE;
				int ActualOffset = -1;
				for (asUINT PropertyIndex = 0; PropertyIndex < TypeInfo->GetPropertyCount(); ++PropertyIndex)
				{
					const char* ActualName = nullptr;
					int Offset = -1;
					if (TypeInfo->GetProperty(PropertyIndex, &ActualName, nullptr, nullptr, nullptr, &Offset) >= 0 &&
						ActualName != nullptr && FCStringAnsi::Strcmp(ActualName, ExpectedProperty.Name) == 0)
					{
						FoundPropertyIndex = static_cast<int32>(PropertyIndex);
						ActualOffset = Offset;
						break;
					}
				}

				bAllAssertionsPassed &=
					LocalAssert.IsTrue(FoundPropertyIndex != INDEX_NONE,
									   *(CaseId + FString::Printf(TEXT(" %s should preserve %s.%s metadata"), Stage,
																  *TypeName, UTF8_TO_TCHAR(ExpectedProperty.Name))));
				if (FoundPropertyIndex == INDEX_NONE)
				{
					continue;
				}

				bAllAssertionsPassed &=
					LocalAssert.AreEqual(ExpectedProperty.Offset, ActualOffset,
										 *(CaseId + FString::Printf(TEXT(" %s should preserve %s.%s offset"), Stage,
																	*TypeName, UTF8_TO_TCHAR(ExpectedProperty.Name))));
				bAllAssertionsPassed &= LocalAssert.AreEqual(
					ExpectedProperty.bInherited, TypeInfo->IsPropertyInherited(static_cast<asUINT>(FoundPropertyIndex)),
					*(CaseId + FString::Printf(TEXT(" %s should preserve %s.%s inheritance classification"), Stage,
											   *TypeName, UTF8_TO_TCHAR(ExpectedProperty.Name))));
			}
		}

		(void)bAllAssertionsPassed;
	}

	static void ReportTypeProperties(FAutomationTestBase & Test, asIScriptModule * Module, const FString& CaseId,
									 const TCHAR* Stage)
	{
		static constexpr const char* TypeDeclarations[] = {
			"FSaveLoadProbeBase",
			"FSaveLoadProbeDerived",
		};

		for (const char* TypeDeclaration : TypeDeclarations)
		{
			asITypeInfo* const TypeInfo = Module != nullptr ? Module->GetTypeInfoByDecl(TypeDeclaration) : nullptr;
			const FString TypeName = UTF8_TO_TCHAR(TypeDeclaration);
			if (TypeInfo == nullptr)
			{
				const FString Message = FString::Printf(
					TEXT("[AS-LIFECYCLE-PROPERTY-TYPE] Id=%s Stage=%s Type=%s Missing=1"), *CaseId, Stage, *TypeName);
				Test.AddError(Message);
				UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
				continue;
			}

			ReportLifecycleInfo(
				Test,
				FString::Printf(TEXT("[AS-LIFECYCLE-PROPERTY-TYPE] Id=%s Stage=%s Type=%s Count=%u Size=%u Base=%s"),
								*CaseId, Stage, *TypeName, TypeInfo->GetPropertyCount(), TypeInfo->GetSize(),
								TypeInfo->GetBaseType() != nullptr ? UTF8_TO_TCHAR(TypeInfo->GetBaseType()->GetName())
																   : TEXT("<none>")));

			for (asUINT PropertyIndex = 0; PropertyIndex < TypeInfo->GetPropertyCount(); ++PropertyIndex)
			{
				const char* PropertyName = nullptr;
				int TypeId = 0;
				bool bIsPrivate = false;
				bool bIsProtected = false;
				int Offset = -1;
				bool bIsReference = false;
				const int QueryResult = TypeInfo->GetProperty(PropertyIndex, &PropertyName, &TypeId, &bIsPrivate,
															  &bIsProtected, &Offset, &bIsReference);
				ReportLifecycleInfo(
					Test,
					FString::Printf(TEXT("[AS-LIFECYCLE-PROPERTY] Id=%s Stage=%s Type=%s Index=%u Query=%d Name=%s "
										 "TypeId=%d Offset=%d Private=%d Protected=%d Reference=%d Inherited=%d"),
									*CaseId, Stage, *TypeName, PropertyIndex, QueryResult,
									PropertyName != nullptr ? UTF8_TO_TCHAR(PropertyName) : TEXT("<null>"), TypeId,
									Offset, bIsPrivate ? 1 : 0, bIsProtected ? 1 : 0, bIsReference ? 1 : 0,
									TypeInfo->IsPropertyInherited(PropertyIndex) ? 1 : 0));
			}
		}
	}

	static void ReportLifecycleStage(FAutomationTestBase & Test, const FString& CaseId, const TCHAR* Stage)
	{
		const FString Message = FString::Printf(TEXT("[AS-LIFECYCLE-STAGE] Id=%s Stage=%s"), *CaseId, Stage);
		Test.AddInfo(Message);
		UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
	}

public:
	TEST_METHOD(PredecessorRetentionBySaveLoadLifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("MOD-SCRIPT-CLASS-SAVELOAD-LIFECYCLE",
						  ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata |
							  ENativeEvidence::Bytecode | ENativeEvidence::Lifecycle | ENativeEvidence::Cleanup |
							  ENativeEvidence::Isolation);

		for (const FLifecycleCase& LifecycleCase : LifecycleCases)
		{

			const FString CaseId =
				FString::Printf(TEXT("MOD-SCRIPT-CLASS-SAVELOAD-LIFECYCLE-%s"), LifecycleCase.CatalogName);
			const FString CaseName(LifecycleCase.CatalogName);
			const FString SourceModuleName = TEXT("ScriptClassSaveLoadSource_") + CaseName;
			const FString DestinationModuleName = TEXT("ScriptClassSaveLoadDestination_") + CaseName;
			const FString Source = BuildScriptClassSource();
			PrintGeneratedAsSource(*TestRunner, CaseId, SourceModuleName, Source);

			FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			asIScriptEngine* const ScriptEngine = Engine.Get();
			ASSERT_THAT(IsNotNull(ScriptEngine, *(CaseId + TEXT(" should create an isolated raw SDK engine"))));
			if (ScriptEngine == nullptr)
			{
				Engine.Destroy();
				continue;
			}

			const FTCHARToUTF8 SourceModuleNameUtf8(*SourceModuleName);
			const FTCHARToUTF8 DestinationModuleNameUtf8(*DestinationModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			asIScriptModule* SourceModule = nullptr;
			Engine.ResetMessages();
			ASSERT_THAT(
				AreEqual(static_cast<int32>(asSUCCESS),
						 CompileNativeModule(ScriptEngine, SourceModuleNameUtf8.Get(), SourceUtf8.Get(), SourceModule),
						 *(CaseId + TEXT(" should compile its script-class source"))));
			ASSERT_THAT(IsNotNull(SourceModule, *(CaseId + TEXT(" should publish its source module"))));
			if (SourceModule == nullptr)
			{
				TestRunner->AddError(Engine.GetMessagesText());
				Engine.Destroy();
				continue;
			}
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("source_compiled"));

			ASSERT_THAT(AreEqual(2, static_cast<int32>(SourceModule->GetObjectTypeCount()),
								 *(CaseId + TEXT(" should compile both source script classes"))));
			ReportTypeProperties(*TestRunner, SourceModule, CaseId, TEXT("source_metadata"));
			AssertExpectedTypeLayouts(*TestRunner, SourceModule, CaseId, TEXT("source_metadata"));
			if (!ExecuteEntries(*TestRunner, ScriptEngine, SourceModule, CaseId + TEXT(" source module")))
			{
				Engine.Destroy();
				continue;
			}
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("source_executed"));

			FMemoryBinaryStream Bytecode;
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), SourceModule->SaveByteCode(&Bytecode, false),
								 *(CaseId + TEXT(" should save bytecode before source discard"))));
			ASSERT_THAT(IsTrue(Bytecode.Num() > 0,
							   *(CaseId + TEXT(" should produce a non-empty script-class bytecode stream"))));
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("source_saved"));

			asIScriptFunction* RetainedSourceEntry = nullptr;
			if (LifecycleCase.bRetainPredecessorEntry)
			{
				RetainedSourceEntry = GetNativeFunctionByExactDecl(SourceModule, "int ScriptClassSaveLoadEntry()");
				ASSERT_THAT(IsNotNull(RetainedSourceEntry,
									  *(CaseId + TEXT(" should resolve the predecessor entry before source discard"))));
				if (RetainedSourceEntry != nullptr)
				{
					RetainedSourceEntry->AddRef();
					ReportLifecycleStage(*TestRunner, CaseId, TEXT("predecessor_retained"));
				}
			}

			ReportLifecycleStage(*TestRunner, CaseId, TEXT("source_discard_begin"));
			ScriptEngine->DiscardModule(SourceModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(SourceModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							   *(CaseId + TEXT(" should discard its source module before destination load"))));
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("source_discarded"));

			Bytecode.ResetReadPosition();
			asIScriptModule* const DestinationModule =
				ScriptEngine->GetModule(DestinationModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
			ASSERT_THAT(IsNotNull(DestinationModule,
								  *(CaseId + TEXT(" should create a destination module before bytecode load"))));
			if (DestinationModule == nullptr)
			{
				if (RetainedSourceEntry != nullptr)
				{
					RetainedSourceEntry->Release();
				}
				Engine.Destroy();
				continue;
			}
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("destination_created"));

			bool bWasDebugInfoStripped = true;
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("destination_load_begin"));
			ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS),
								 DestinationModule->LoadByteCode(&Bytecode, &bWasDebugInfoStripped),
								 *(CaseId + TEXT(" should load the script-class bytecode"))));
			ASSERT_THAT(
				IsFalse(bWasDebugInfoStripped, *(CaseId + TEXT(" should preserve script-class debug information"))));
			ASSERT_THAT(AreEqual(2, static_cast<int32>(DestinationModule->GetObjectTypeCount()),
								 *(CaseId + TEXT(" should restore both script classes"))));
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("destination_loaded"));
			ReportTypeProperties(*TestRunner, DestinationModule, CaseId, TEXT("destination_metadata"));
			AssertExpectedTypeLayouts(*TestRunner, DestinationModule, CaseId, TEXT("destination_metadata"));
			if (!ExecuteEntries(*TestRunner, ScriptEngine, DestinationModule, CaseId + TEXT(" destination module")))
			{
				if (RetainedSourceEntry != nullptr)
				{
					RetainedSourceEntry->Release();
				}
				Engine.Destroy();
				continue;
			}
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("destination_executed"));

			if (RetainedSourceEntry != nullptr)
			{
				ReportLifecycleStage(*TestRunner, CaseId, TEXT("predecessor_release_begin"));
				RetainedSourceEntry->Release();
				ReportLifecycleStage(*TestRunner, CaseId, TEXT("predecessor_released"));
			}
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("destination_discard_begin"));
			ScriptEngine->DiscardModule(DestinationModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(DestinationModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							   *(CaseId + TEXT(" should discard the loaded destination module"))));
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("destination_discarded"));
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("engine_destroy_begin"));
			Engine.Destroy();
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("engine_destroyed"));

			const FString PostTeardownAllocation = FString::ChrN(1024, TEXT("P")[0]);
			ASSERT_THAT(AreEqual(1024, PostTeardownAllocation.Len(),
								 *(CaseId + TEXT(" should leave the heap valid for post-teardown allocation"))));
			ReportLifecycleStage(*TestRunner, CaseId, TEXT("post_teardown_allocation_completed"));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
