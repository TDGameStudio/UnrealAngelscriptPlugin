#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "ClassGenerator/ASStruct.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptStructHotReloadTests,
	"Angelscript.TestModule.Generator.ASStruct.HotReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CheckTrue(FAutomationTestBase& Test, const TCHAR* Message, bool bActual)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bActual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreEqual(Expected, Actual, Message);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckEqual(FAutomationTestBase& Test, const FString& Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		return CheckEqual(Test, *Message, Actual, Expected);
	}

	template <typename ActualType, typename ExpectedType>
	static bool CheckNotEqual(FAutomationTestBase& Test, const TCHAR* Message, const ActualType& Actual, const ExpectedType& Expected)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.AreNotEqual(Expected, Actual, Message);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNotNull(Value, Message);
	}

	template <typename ValueType>
	static bool CheckNotNull(FAutomationTestBase& Test, const FString& Message, const ValueType& Value)
	{
		return CheckNotNull(Test, *Message, Value);
	}

	template <typename ValueType>
	static bool CheckNull(FAutomationTestBase& Test, const TCHAR* Message, const ValueType& Value)
	{
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsNull(Value, Message);
	}
	struct FVersionChainCase
	{
	inline static const FName ModuleName = FName(TEXT("ScriptStructHotReloadVersionChain"));
	inline static const FName UnrealStructName = FName(TEXT("ScriptStructHotReloadVersionChain"));
	inline static const FString ScriptFilename = FString(TEXT("ScriptStructHotReloadVersionChain.as"));

	static FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), ScriptFilename);
	}

	static UASStruct* FindStructByName(const FName StructName)
	{
		return FindObject<UASStruct>(FAngelscriptEngine::GetPackage(), *StructName.ToString());
	}

	static UASStruct* FindCurrentStruct()
	{
		return FindStructByName(UnrealStructName);
	}

	static FProperty* FindStructProperty(UASStruct* Struct, const FName PropertyName)
	{
		return Struct != nullptr ? Struct->FindPropertyByName(PropertyName) : nullptr;
	}

	static bool VerifyHandledReloadResult(FAutomationTestBase& Test, const TCHAR* Context, const ECompileResult ReloadResult)
	{
		return CheckTrue(
			Test,
			Context,
			ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled);
	}
	};

	struct FCustomGuidCase
	{
	inline static const FName StableModuleName = FName(TEXT("ScriptStructCustomGuidStable"));
	inline static const FName StableStructName = FName(TEXT("StableGuidStruct"));
	inline static const FString StableScriptFilename = FString(TEXT("ScriptStructCustomGuidStable.as"));
	inline static const FName DifferentModuleName = FName(TEXT("ScriptStructCustomGuidDifferent"));
	inline static const FName DifferentStructName = FName(TEXT("DifferentGuidStruct"));
	inline static const FString DifferentScriptFilename = FString(TEXT("ScriptStructCustomGuidDifferent.as"));

	static FString GetScriptAbsoluteFilename(const FString& InScriptFilename)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), InScriptFilename);
	}

	static FString GetStableScriptAbsoluteFilename()
	{
		return GetScriptAbsoluteFilename(StableScriptFilename);
	}

	static FString GetDifferentScriptAbsoluteFilename()
	{
		return GetScriptAbsoluteFilename(DifferentScriptFilename);
	}

	static UASStruct* FindStableStruct()
	{
		return FVersionChainCase::FindStructByName(StableStructName);
	}

	static UASStruct* FindDifferentStruct()
	{
		return FVersionChainCase::FindStructByName(DifferentStructName);
	}
	};

	struct FCapabilityReloadCase
	{
	inline static const FName ModuleName = FName(TEXT("ScriptStructCapabilityReload"));
	inline static const FName StructName = FName(TEXT("ReloadableCapabilityStruct"));
	inline static const FString ScriptFilename = FString(TEXT("ScriptStructCapabilityReload.as"));

	static FString GetScriptAbsoluteFilename()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), ScriptFilename);
	}

	static UASStruct* FindCurrentStruct()
	{
		return FVersionChainCase::FindStructByName(StructName);
	}

	static bool VerifyCapabilityState(
		FAutomationTestBase& Test,
		UASStruct* Struct,
		const TCHAR* StageLabel,
		bool bExpectIdentical,
		bool bExpectHash)
	{
		const FString StructMessage = FString::Printf(TEXT("%s should publish a script struct"), StageLabel);
		if (!CheckNotNull(Test, StructMessage, Struct))
		{
			return false;
		}

		UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
		const FString OpsMessage = FString::Printf(TEXT("%s should expose cpp struct ops"), StageLabel);
		if (!CheckNotNull(Test, OpsMessage, CppStructOps))
		{
			return false;
		}

		const bool bStructFlagMatches = CheckEqual(
			Test,
			FString::Printf(TEXT("%s should keep the expected STRUCT_IdenticalNative flag"), StageLabel),
			EnumHasAnyFlags(Struct->StructFlags, STRUCT_IdenticalNative),
			bExpectIdentical);
		const bool bHasIdenticalMatches = CheckEqual(
			Test,
			FString::Printf(TEXT("%s should keep the expected cpp-ops identical capability"), StageLabel),
			CppStructOps->HasIdentical(),
			bExpectIdentical);
		const bool bHasTypeHashMatches = CheckEqual(
			Test,
			FString::Printf(TEXT("%s should keep the expected cpp-ops hash capability"), StageLabel),
			CppStructOps->HasGetTypeHash(),
			bExpectHash);
		const bool bComputedPropertyFlagMatches = CheckEqual(
			Test,
			FString::Printf(TEXT("%s should keep the expected CPF_HasGetValueTypeHash computed property flag"), StageLabel),
			EnumHasAnyFlags(CppStructOps->GetComputedPropertyFlags(), CPF_HasGetValueTypeHash),
			bExpectHash);
		return bStructFlagMatches
			&& bHasIdenticalMatches
			&& bHasTypeHashMatches
			&& bComputedPropertyFlagMatches;
	}
	};

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(GetNewestVersionAfterFullReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FVersionChainCase::ModuleName.ToString());
			IFileManager::Get().Delete(*FVersionChainCase::GetScriptAbsoluteFilename(), false, true, true);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FScriptStructHotReloadVersionChain
			{
				UPROPERTY()
				int Value = 1;
			};
			)AS");
		const FString ScriptV2 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FScriptStructHotReloadVersionChain
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			};
			)AS");
		const FString ScriptV3 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FScriptStructHotReloadVersionChain
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;

				UPROPERTY()
				int TailValue = 3;
			};
			)AS");

		if (!CheckTrue(
				*TestRunner,
				TEXT("Initial script struct compile should succeed"),
				CompileAnnotatedModuleFromMemory(&Engine, FVersionChainCase::ModuleName, FVersionChainCase::ScriptFilename, ScriptV1)))
		{ return; }

		UASStruct* FirstVersion = FVersionChainCase::FindCurrentStruct();
		if (!CheckNotNull(*TestRunner, TEXT("Initial script struct should be registered in the Angelscript package"), FirstVersion)) { return; }
		if (!CheckNotNull(*TestRunner, TEXT("Initial script struct should expose the original reflected property"), FVersionChainCase::FindStructProperty(FirstVersion, TEXT("Value")))) { return; }
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(FirstVersion, TEXT("AddedValue")), TEXT("Initial script struct should not expose the first added property before reload")));
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(FirstVersion, TEXT("TailValue")), TEXT("Initial script struct should not expose the second added property before reload")));

		ECompileResult ReloadResultV2 = ECompileResult::Error;
		if (!CheckTrue(
				*TestRunner,
				TEXT("First structural script struct reload should compile successfully"),
				CompileModuleWithResult(&Engine, ECompileType::FullReload, FVersionChainCase::ModuleName, FVersionChainCase::ScriptFilename, ScriptV2, ReloadResultV2)))
		{ return; }
		if (!FVersionChainCase::VerifyHandledReloadResult(*TestRunner, TEXT("First structural script struct reload should be handled by the full reload pipeline"), ReloadResultV2))
		{ return; }

		UASStruct* SecondVersion = FVersionChainCase::FindCurrentStruct();
		if (!CheckNotNull(*TestRunner, TEXT("First full reload should publish a new canonical script struct"), SecondVersion)) { return; }

		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(FirstVersion), static_cast<UScriptStruct*>(SecondVersion), TEXT("First full reload should replace the original struct object")));
		ASSERT_THAT(AreEqual(SecondVersion, FirstVersion->NewerVersion, TEXT("First full reload should wire the old struct directly to the second version")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(SecondVersion), FirstVersion->GetNewestVersion(), TEXT("GetNewestVersion should resolve the second version after the first reload")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(SecondVersion), SecondVersion->GetNewestVersion(), TEXT("The current canonical struct should consider itself the newest version")));
		ASSERT_THAT(IsTrue(FirstVersion->GetFName() != FVersionChainCase::UnrealStructName, TEXT("The first version should no longer own the canonical struct name after reload")));
		if (!CheckNotNull(*TestRunner, TEXT("First full reload should expose the newly added reflected property"), FVersionChainCase::FindStructProperty(SecondVersion, TEXT("AddedValue")))) { return; }
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(FirstVersion, TEXT("AddedValue")), TEXT("The replaced first version should keep its original reflected layout")));
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(SecondVersion, TEXT("TailValue")), TEXT("The second version should not expose the third-version-only property yet")));

		ECompileResult ReloadResultV3 = ECompileResult::Error;
		if (!CheckTrue(
				*TestRunner,
				TEXT("Second structural script struct reload should compile successfully"),
				CompileModuleWithResult(&Engine, ECompileType::FullReload, FVersionChainCase::ModuleName, FVersionChainCase::ScriptFilename, ScriptV3, ReloadResultV3)))
		{ return; }
		if (!FVersionChainCase::VerifyHandledReloadResult(*TestRunner, TEXT("Second structural script struct reload should also be handled by the full reload pipeline"), ReloadResultV3))
		{ return; }

		UASStruct* ThirdVersion = FVersionChainCase::FindCurrentStruct();
		if (!CheckNotNull(*TestRunner, TEXT("Second full reload should publish the newest canonical script struct"), ThirdVersion)) { return; }

		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(SecondVersion), static_cast<UScriptStruct*>(ThirdVersion), TEXT("Second full reload should replace the second struct object")));
		ASSERT_THAT(AreEqual(ThirdVersion, SecondVersion->NewerVersion, TEXT("Second full reload should wire the second version directly to the third version")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(ThirdVersion), FirstVersion->GetNewestVersion(), TEXT("The original struct should walk the full version chain to the newest struct")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(ThirdVersion), SecondVersion->GetNewestVersion(), TEXT("The middle struct should also resolve to the newest struct")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(ThirdVersion), ThirdVersion->GetNewestVersion(), TEXT("The newest struct should still resolve to itself")));
		ASSERT_THAT(AreEqual(ThirdVersion, FVersionChainCase::FindCurrentStruct(), TEXT("Canonical lookup should resolve to the newest struct after multiple full reloads")));
		ASSERT_THAT(IsTrue(SecondVersion->GetFName() != FVersionChainCase::UnrealStructName, TEXT("The second version should also lose the canonical struct name after the next reload")));
		if (!CheckNotNull(*TestRunner, TEXT("The newest struct should expose the tail property introduced by the second reload"), FVersionChainCase::FindStructProperty(ThirdVersion, TEXT("TailValue")))) { return; }
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(SecondVersion, TEXT("TailValue")), TEXT("The middle replaced struct should keep the layout it had when it was canonical")));
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(FirstVersion, TEXT("AddedValue")), TEXT("The oldest replaced struct should remain frozen at its original layout")));
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(FirstVersion, TEXT("TailValue")), TEXT("The oldest replaced struct should never gain later properties")));
	}

	TEST_METHOD(CustomGuidStableAcrossSameNameReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FCustomGuidCase::StableModuleName.ToString());
			Engine.DiscardModule(*FCustomGuidCase::DifferentModuleName.ToString());
			IFileManager::Get().Delete(*FCustomGuidCase::GetStableScriptAbsoluteFilename(), false, true, true);
			IFileManager::Get().Delete(*FCustomGuidCase::GetDifferentScriptAbsoluteFilename(), false, true, true);
		};

		const FString StableScriptV1 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FStableGuidStruct
			{
				UPROPERTY()
				int Value = 1;
			};
			)AS");
		const FString StableScriptV2 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FStableGuidStruct
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int AddedValue = 2;
			};
			)AS");
		const FString DifferentScript = ASTEST_AS(R"AS(
			USTRUCT()
			struct FDifferentGuidStruct
			{
				UPROPERTY()
				int Value = 7;
			};
			)AS");

		if (!CheckTrue(
				*TestRunner,
				TEXT("Initial stable script struct compile should succeed"),
				CompileAnnotatedModuleFromMemory(&Engine, FCustomGuidCase::StableModuleName, FCustomGuidCase::StableScriptFilename, StableScriptV1)))
		{ return; }

		UASStruct* InitialStableStruct = FCustomGuidCase::FindStableStruct();
		if (!CheckNotNull(*TestRunner, TEXT("Initial stable script struct should be registered in the Angelscript package"), InitialStableStruct)) { return; }
		const FGuid StableGuidBeforeReload = InitialStableStruct->GetCustomGuid();
		ASSERT_THAT(IsTrue(StableGuidBeforeReload.IsValid(), TEXT("Initial stable script struct should publish a valid custom GUID")));

		ECompileResult StableReloadResult = ECompileResult::Error;
		if (!CheckTrue(
				*TestRunner,
				TEXT("Stable script struct full reload should compile successfully"),
				CompileModuleWithResult(&Engine, ECompileType::FullReload, FCustomGuidCase::StableModuleName, FCustomGuidCase::StableScriptFilename, StableScriptV2, StableReloadResult)))
		{ return; }
		if (!FVersionChainCase::VerifyHandledReloadResult(*TestRunner, TEXT("Stable script struct full reload should be handled by the reload pipeline"), StableReloadResult))
		{ return; }

		UASStruct* ReloadedStableStruct = FCustomGuidCase::FindStableStruct();
		if (!CheckNotNull(*TestRunner, TEXT("Stable script struct full reload should publish a replacement struct"), ReloadedStableStruct)) { return; }

		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(ReloadedStableStruct), static_cast<UScriptStruct*>(InitialStableStruct), TEXT("Stable script struct full reload should replace the canonical struct object")));
		ASSERT_THAT(AreEqual(StableGuidBeforeReload, ReloadedStableStruct->GetCustomGuid(), TEXT("Stable script struct full reload should preserve the original custom GUID")));
		ASSERT_THAT(AreEqual(StableGuidBeforeReload, InitialStableStruct->GetCustomGuid(), TEXT("Replaced stable script struct should retain its original custom GUID")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(ReloadedStableStruct), InitialStableStruct->GetNewestVersion(), TEXT("Replaced stable script struct should resolve the reload result as its newest version")));

		if (!CheckTrue(
				*TestRunner,
				TEXT("Different-name script struct compile should succeed"),
				CompileAnnotatedModuleFromMemory(&Engine, FCustomGuidCase::DifferentModuleName, FCustomGuidCase::DifferentScriptFilename, DifferentScript)))
		{ return; }

		UASStruct* DifferentStruct = FCustomGuidCase::FindDifferentStruct();
		if (!CheckNotNull(*TestRunner, TEXT("Different-name script struct should be registered in the Angelscript package"), DifferentStruct)) { return; }
		const FGuid DifferentGuid = DifferentStruct->GetCustomGuid();
		ASSERT_THAT(IsTrue(DifferentGuid.IsValid(), TEXT("Different-name script struct should publish a valid custom GUID")));
		ASSERT_THAT(AreNotEqual(StableGuidBeforeReload, DifferentGuid, TEXT("Different-name script struct should not collide with the stable struct custom GUID")));
	}

	TEST_METHOD(UpdateScriptTypeClearsIdenticalAndHashCapabilitiesAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FCapabilityReloadCase::ModuleName.ToString());
			IFileManager::Get().Delete(*FCapabilityReloadCase::GetScriptAbsoluteFilename(), false, true, true);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FReloadableCapabilityStruct
			{
				UPROPERTY()
				int Value = 1;

				bool opEquals(const FReloadableCapabilityStruct& Other) const
				{
					return Value == Other.Value;
				}

				uint32 Hash() const
				{
					return uint32(Value + 7);
				}

				FString ToString() const
				{
					return "HasAllCapabilities";
				}
			};
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			USTRUCT()
			struct FReloadableCapabilityStruct
			{
				UPROPERTY()
				int Value = 2;

				UPROPERTY()
				int AddedValue = 9;

				FString ToString() const
				{
					return "ToStringOnly";
				}
			};
			)AS");

		if (!CheckTrue(
				*TestRunner,
				TEXT("Capability baseline script struct compile should succeed"),
				CompileAnnotatedModuleFromMemory(&Engine, FCapabilityReloadCase::ModuleName, FCapabilityReloadCase::ScriptFilename, ScriptV1)))
		{ return; }

		UASStruct* InitialStruct = FCapabilityReloadCase::FindCurrentStruct();
		if (!FCapabilityReloadCase::VerifyCapabilityState(*TestRunner, InitialStruct, TEXT("Capability baseline struct"), true, true))
		{ return; }
		if (!CheckNotNull(*TestRunner, TEXT("Capability baseline struct should keep the script ToString binding"), InitialStruct->GetToStringFunction()))
		{ return; }

		ECompileResult ReloadResult = ECompileResult::Error;
		if (!CheckTrue(
				*TestRunner,
				TEXT("Capability reload script struct compile should succeed"),
				CompileModuleWithResult(&Engine, ECompileType::FullReload, FCapabilityReloadCase::ModuleName, FCapabilityReloadCase::ScriptFilename, ScriptV2, ReloadResult)))
		{ return; }
		if (!FVersionChainCase::VerifyHandledReloadResult(*TestRunner, TEXT("Capability reload should be handled by the full reload pipeline"), ReloadResult))
		{ return; }

		UASStruct* ReloadedStruct = FCapabilityReloadCase::FindCurrentStruct();
		if (!CheckNotNull(*TestRunner, TEXT("Capability reload should publish a replacement script struct"), ReloadedStruct)) { return; }

		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(ReloadedStruct), static_cast<UScriptStruct*>(InitialStruct), TEXT("Capability reload should replace the canonical struct object")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(ReloadedStruct), InitialStruct->GetNewestVersion(), TEXT("Capability reload should wire the previous struct to the replacement version")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(ReloadedStruct), ReloadedStruct->GetNewestVersion(), TEXT("Capability reload replacement should consider itself the newest version")));
		ASSERT_THAT(IsNotNull(FVersionChainCase::FindStructProperty(ReloadedStruct, TEXT("AddedValue")), TEXT("Capability reload replacement should expose the added property that forces full reload")));
		ASSERT_THAT(IsNull(FVersionChainCase::FindStructProperty(InitialStruct, TEXT("AddedValue")), TEXT("Capability reload should keep the replaced struct frozen at its original layout")));

		if (!FCapabilityReloadCase::VerifyCapabilityState(*TestRunner, ReloadedStruct, TEXT("Capability reload replacement struct"), false, false))
		{ return; }

		ASSERT_THAT(IsNotNull(ReloadedStruct->GetToStringFunction(), TEXT("Capability reload replacement should keep the ToString binding after dropping opEquals and Hash")));

	}
};

#endif
