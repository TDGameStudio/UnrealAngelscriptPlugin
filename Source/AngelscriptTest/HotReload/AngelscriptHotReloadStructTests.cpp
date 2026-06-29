#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/ASStruct.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadStructTests,
	"Angelscript.TestModule.HotReload.Struct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName VersionChainModuleName = FName(TEXT("HotReloadStructVersionChain"));
	inline static const FString VersionChainFilename = FString(TEXT("HotReloadStructVersionChain.as"));
	inline static const FString VersionChainStructName = FString(TEXT("FHotReloadStructPayload"));

	inline static const FName PropertyRetargetModuleName = FName(TEXT("HotReloadStructPropertyRetarget"));
	inline static const FString PropertyRetargetFilename = FString(TEXT("HotReloadStructPropertyRetarget.as"));
	inline static const FString PropertyRetargetStructName = FString(TEXT("FHotReloadStructPropertyPayload"));
	inline static const FName PropertyRetargetOwnerClassName = FName(TEXT("UHotReloadStructPropertyOwner"));

	inline static const FName FunctionParameterModuleName = FName(TEXT("HotReloadStructFunctionParameter"));
	inline static const FString FunctionParameterFilename = FString(TEXT("HotReloadStructFunctionParameter.as"));
	inline static const FString FunctionParameterStructName = FString(TEXT("FHotReloadStructFunctionPayload"));
	inline static const FName FunctionParameterReceiverClassName = FName(TEXT("UHotReloadStructFunctionReceiver"));

	struct FStructReloadObservation
	{
		int32 StructReloadCount = 0;
		UScriptStruct* OldStructSeenDuringReload = nullptr;
		UScriptStruct* NewStructSeenDuringReload = nullptr;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static asIScriptModule* FindScriptModule(FAngelscriptEngine& Engine, const FName ModuleName)
	{
		const TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ModuleName.ToString());
		return ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	}

	static UASStruct* FindScriptStruct(FAngelscriptEngine& Engine, const FString& StructName)
	{
		const TSharedPtr<FAngelscriptClassDesc> StructDesc = Engine.GetClass(StructName);
		return StructDesc.IsValid() ? Cast<UASStruct>(StructDesc->Struct) : nullptr;
	}

	static bool CompileReload(
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const FString& Filename,
		const FString& Source,
		ECompileResult& OutReloadResult)
	{
		if (!CompileModuleWithResult(&Engine, ECompileType::FullReload, ModuleName, Filename, Source, OutReloadResult))
		{
			return false;
		}

		return IsHandledReloadResult(OutReloadResult);
	}

	static UObject* CreateObjectForClass(FAutomationTestBase& Test, UClass* ObjectClass, const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(ObjectClass, Context))
		{
			return nullptr;
		}

		UObject* Object = NewObject<UObject>(GetTransientPackage(), ObjectClass);
		if (!LocalAssert.IsNotNull(Object, TEXT("HotReload struct test should create a UObject instance")))
		{
			return nullptr;
		}

		return Object;
	}

	static bool ExecuteIntGlobal(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FName ModuleName,
		const TCHAR* Declaration,
		UObject* ObjectArgument,
		const int32 ExpectedResult,
		const TCHAR* Context)
	{
		asIScriptModule* Module = FindScriptModule(Engine, ModuleName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Module, TEXT("HotReload struct test should expose the script module")))
		{
			return false;
		}

		FAngelscriptTestExecutor Executor(Test, Engine, *Module, Declaration);
		if (!LocalAssert.IsTrue(Executor.IsValid(), TEXT("HotReload struct test should resolve the global entry function")))
		{
			return false;
		}

		const int32 Actual = Executor
			.AddArgObject(ObjectArgument)
			.ExecuteAndGet<int32>(INDEX_NONE);
		return LocalAssert.AreEqual(ExpectedResult, Actual, Context);
	}

	static bool ValidateStructProperty(
		FAutomationTestBase& Test,
		UStruct* Owner,
		const TCHAR* PropertyName,
		UScriptStruct* ExpectedStruct,
		FStructProperty*& OutProperty,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		OutProperty = FindFProperty<FStructProperty>(Owner, PropertyName);
		if (!LocalAssert.IsNotNull(OutProperty, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedStruct, OutProperty->Struct, Context);
	}

	static bool ValidateStructFunctionParameter(
		FAutomationTestBase& Test,
		UFunction* Function,
		const TCHAR* ParameterName,
		UScriptStruct* ExpectedStruct,
		const TCHAR* Context)
	{
		FStructProperty* ParameterProperty = FindFProperty<FStructProperty>(Function, ParameterName);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(ParameterProperty, Context))
		{
			return false;
		}

		return LocalAssert.AreEqual(ExpectedStruct, ParameterProperty->Struct, Context);
	}

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

	TEST_METHOD(StructFullReloadReplacesScriptStructAndKeepsVersionChain)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		FStructReloadObservation Observation;
		FDelegateHandle StructReloadHandle;

		ON_SCOPE_EXIT
		{
			Engine.GetOnStructReload().Remove(StructReloadHandle);
			Engine.DiscardModule(*VersionChainModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadStructPayload
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, VersionChainModuleName, VersionChainFilename, ReloadV1Source),
			TEXT("Initial script struct version-chain compile should succeed")));

		UASStruct* StructBeforeReload = FindScriptStruct(Engine, VersionChainStructName);
		ASSERT_THAT(IsNotNull(StructBeforeReload, TEXT("Script struct should exist before full reload")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StructBeforeReload, TEXT("Value")), TEXT("Script struct should expose Value before full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(StructBeforeReload, TEXT("Bonus")), TEXT("Script struct should not expose Bonus before full reload")));

		StructReloadHandle = Engine.GetOnStructReload().AddLambda(
			[&Observation](UScriptStruct* OldStruct, UScriptStruct* NewStruct)
			{
				++Observation.StructReloadCount;
				Observation.OldStructSeenDuringReload = OldStruct;
				Observation.NewStructSeenDuringReload = NewStruct;
			});

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadStructPayload
			{
				UPROPERTY()
				int Value = 1;

				UPROPERTY()
				int Bonus = 2;
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileReload(Engine, VersionChainModuleName, VersionChainFilename, ReloadV2Source, ReloadResult),
			TEXT("Script struct full reload should compile and be handled")));

		UASStruct* StructAfterReload = FindScriptStruct(Engine, VersionChainStructName);
		ASSERT_THAT(IsNotNull(StructAfterReload, TEXT("Script struct should exist after full reload")));
		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(StructBeforeReload), static_cast<UScriptStruct*>(StructAfterReload), TEXT("Full reload should replace the script struct object")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructAfterReload), StructBeforeReload->GetNewestVersion(), TEXT("Old struct should resolve to the reloaded struct as newest version")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructAfterReload), StructAfterReload->GetNewestVersion(), TEXT("Reloaded struct should resolve to itself as newest version")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(StructBeforeReload, TEXT("Bonus")), TEXT("Old script struct should keep its original layout")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StructAfterReload, TEXT("Value")), TEXT("Reloaded script struct should expose Value")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StructAfterReload, TEXT("Bonus")), TEXT("Reloaded script struct should expose Bonus")));
		ASSERT_THAT(AreEqual(1, Observation.StructReloadCount, TEXT("Script struct full reload should broadcast struct reload exactly once")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructBeforeReload), Observation.OldStructSeenDuringReload, TEXT("Struct reload should expose the old struct")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructAfterReload), Observation.NewStructSeenDuringReload, TEXT("Struct reload should expose the new struct")));
	}

	TEST_METHOD(StructPropertyRetargetsToReloadedStruct)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*PropertyRetargetModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadStructPropertyPayload
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class UHotReloadStructPropertyOwner : UObject
			{
				UPROPERTY()
				FHotReloadStructPropertyPayload Payload;

				UFUNCTION()
				int ConfigureAndRead()
				{
					Payload.Value = 12;
					return Payload.Value;
				}
			}

			int RunOwner(UHotReloadStructPropertyOwner Owner)
			{
				return Owner.ConfigureAndRead();
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, PropertyRetargetModuleName, PropertyRetargetFilename, ReloadV1Source),
			TEXT("Initial script struct property-retarget compile should succeed")));

		UASStruct* StructBeforeReload = FindScriptStruct(Engine, PropertyRetargetStructName);
		ASSERT_THAT(IsNotNull(StructBeforeReload, TEXT("Property payload struct should exist before full reload")));
		UClass* OwnerClassBeforeReload = FindGeneratedClass(&Engine, PropertyRetargetOwnerClassName);
		ASSERT_THAT(IsNotNull(OwnerClassBeforeReload, TEXT("Property owner class should exist before full reload")));

		FStructProperty* PayloadPropertyBeforeReload = nullptr;
		ASSERT_THAT(IsTrue(ValidateStructProperty(
			*TestRunner,
			OwnerClassBeforeReload,
			TEXT("Payload"),
			StructBeforeReload,
			PayloadPropertyBeforeReload,
			TEXT("Owner Payload property should point at the initial script struct"))));

		UObject* OwnerBeforeReload = CreateObjectForClass(*TestRunner, OwnerClassBeforeReload, TEXT("Property owner class should instantiate before reload"));
		ASSERT_THAT(IsNotNull(OwnerBeforeReload));
		ASSERT_THAT(IsTrue(ExecuteIntGlobal(
			*TestRunner,
			Engine,
			PropertyRetargetModuleName,
			TEXT("int RunOwner(UHotReloadStructPropertyOwner)"),
			OwnerBeforeReload,
			12,
			TEXT("Property owner V1 should execute against the initial script struct layout"))));

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadStructPropertyPayload
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				int Bonus = 0;
			}

			UCLASS()
			class UHotReloadStructPropertyOwner : UObject
			{
				UPROPERTY()
				FHotReloadStructPropertyPayload Payload;

				UFUNCTION()
				int ConfigureAndRead()
				{
					Payload.Value = 20;
					Payload.Bonus = 22;
					return Payload.Value + Payload.Bonus;
				}
			}

			int RunOwner(UHotReloadStructPropertyOwner Owner)
			{
				return Owner.ConfigureAndRead();
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileReload(Engine, PropertyRetargetModuleName, PropertyRetargetFilename, ReloadV2Source, ReloadResult),
			TEXT("Script struct property-retarget full reload should compile and be handled")));

		UASStruct* StructAfterReload = FindScriptStruct(Engine, PropertyRetargetStructName);
		ASSERT_THAT(IsNotNull(StructAfterReload, TEXT("Property payload struct should exist after full reload")));
		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(StructBeforeReload), static_cast<UScriptStruct*>(StructAfterReload), TEXT("Property payload struct should be replaced by full reload")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(StructBeforeReload, TEXT("Bonus")), TEXT("Old property payload struct should keep its original layout")));
		ASSERT_THAT(IsNotNull(FindFProperty<FIntProperty>(StructAfterReload, TEXT("Bonus")), TEXT("Reloaded property payload struct should expose Bonus")));

		UClass* OwnerClassAfterReload = FindGeneratedClass(&Engine, PropertyRetargetOwnerClassName);
		ASSERT_THAT(IsNotNull(OwnerClassAfterReload, TEXT("Property owner class should exist after full reload")));
		ASSERT_THAT(AreNotEqual(OwnerClassBeforeReload, OwnerClassAfterReload, TEXT("Property owner class should be replaced by structural full reload")));

		FStructProperty* PayloadPropertyAfterReload = nullptr;
		ASSERT_THAT(IsTrue(ValidateStructProperty(
			*TestRunner,
			OwnerClassAfterReload,
			TEXT("Payload"),
			StructAfterReload,
			PayloadPropertyAfterReload,
			TEXT("Reloaded owner Payload property should point at the reloaded script struct"))));
		ASSERT_THAT(AreNotEqual(PayloadPropertyBeforeReload, PayloadPropertyAfterReload, TEXT("Full reload should publish a replacement owner Payload property")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructBeforeReload), PayloadPropertyBeforeReload->Struct, TEXT("Old owner Payload property should remain pointed at the old struct")));

		UObject* OwnerAfterReload = CreateObjectForClass(*TestRunner, OwnerClassAfterReload, TEXT("Property owner class should instantiate after reload"));
		ASSERT_THAT(IsNotNull(OwnerAfterReload));
		ASSERT_THAT(IsTrue(ExecuteIntGlobal(
			*TestRunner,
			Engine,
			PropertyRetargetModuleName,
			TEXT("int RunOwner(UHotReloadStructPropertyOwner)"),
			OwnerAfterReload,
			42,
			TEXT("Property owner V2 should execute against the reloaded script struct layout"))));
	}

	TEST_METHOD(StructUFunctionParameterExecutesAfterReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		FStructReloadObservation Observation;
		FDelegateHandle StructReloadHandle;

		ON_SCOPE_EXIT
		{
			Engine.GetOnStructReload().Remove(StructReloadHandle);
			Engine.DiscardModule(*FunctionParameterModuleName.ToString());
		};

		const FString ReloadV1Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadStructFunctionPayload
			{
				UPROPERTY()
				int Value = 0;
			}

			UCLASS()
			class UHotReloadStructFunctionReceiver : UObject
			{
				UFUNCTION()
				int ConsumePayload(const FHotReloadStructFunctionPayload& Payload)
				{
					return Payload.Value;
				}
			}

			int RunPayload(UHotReloadStructFunctionReceiver Receiver)
			{
				FHotReloadStructFunctionPayload Payload;
				Payload.Value = 12;
				return Receiver.ConsumePayload(Payload);
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, FunctionParameterModuleName, FunctionParameterFilename, ReloadV1Source),
			TEXT("Initial script struct function-parameter compile should succeed")));

		UASStruct* StructBeforeReload = FindScriptStruct(Engine, FunctionParameterStructName);
		ASSERT_THAT(IsNotNull(StructBeforeReload, TEXT("Function payload struct should exist before full reload")));
		UClass* ReceiverClassBeforeReload = FindGeneratedClass(&Engine, FunctionParameterReceiverClassName);
		ASSERT_THAT(IsNotNull(ReceiverClassBeforeReload, TEXT("Function receiver class should exist before full reload")));
		UFunction* FunctionBeforeReload = ReceiverClassBeforeReload->FindFunctionByName(TEXT("ConsumePayload"));
		ASSERT_THAT(IsNotNull(FunctionBeforeReload, TEXT("Function receiver should expose ConsumePayload before reload")));
		ASSERT_THAT(IsTrue(ValidateStructFunctionParameter(
			*TestRunner,
			FunctionBeforeReload,
			TEXT("Payload"),
			StructBeforeReload,
			TEXT("ConsumePayload parameter should point at the initial script struct"))));

		UObject* ReceiverBeforeReload = CreateObjectForClass(*TestRunner, ReceiverClassBeforeReload, TEXT("Function receiver class should instantiate before reload"));
		ASSERT_THAT(IsNotNull(ReceiverBeforeReload));
		ASSERT_THAT(IsTrue(ExecuteIntGlobal(
			*TestRunner,
			Engine,
			FunctionParameterModuleName,
			TEXT("int RunPayload(UHotReloadStructFunctionReceiver)"),
			ReceiverBeforeReload,
			12,
			TEXT("Function parameter V1 should pass the initial script struct fields"))));

		StructReloadHandle = Engine.GetOnStructReload().AddLambda(
			[&Observation](UScriptStruct* OldStruct, UScriptStruct* NewStruct)
			{
				++Observation.StructReloadCount;
				Observation.OldStructSeenDuringReload = OldStruct;
				Observation.NewStructSeenDuringReload = NewStruct;
			});

		const FString ReloadV2Source = ASTEST_AS(R"AS(
			USTRUCT()
			struct FHotReloadStructFunctionPayload
			{
				UPROPERTY()
				int Value = 0;

				UPROPERTY()
				int Bonus = 0;
			}

			UCLASS()
			class UHotReloadStructFunctionReceiver : UObject
			{
				UFUNCTION()
				int ConsumePayload(const FHotReloadStructFunctionPayload& Payload)
				{
					return Payload.Value + Payload.Bonus;
				}
			}

			int RunPayload(UHotReloadStructFunctionReceiver Receiver)
			{
				FHotReloadStructFunctionPayload Payload;
				Payload.Value = 20;
				Payload.Bonus = 22;
				return Receiver.ConsumePayload(Payload);
			}
			)AS");

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileReload(Engine, FunctionParameterModuleName, FunctionParameterFilename, ReloadV2Source, ReloadResult),
			TEXT("Script struct function-parameter full reload should compile and be handled")));

		UASStruct* StructAfterReload = FindScriptStruct(Engine, FunctionParameterStructName);
		ASSERT_THAT(IsNotNull(StructAfterReload, TEXT("Function payload struct should exist after full reload")));
		ASSERT_THAT(AreNotEqual(static_cast<UScriptStruct*>(StructBeforeReload), static_cast<UScriptStruct*>(StructAfterReload), TEXT("Function payload struct should be replaced by full reload")));
		ASSERT_THAT(AreEqual(1, Observation.StructReloadCount, TEXT("Function payload full reload should broadcast struct reload exactly once")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructBeforeReload), Observation.OldStructSeenDuringReload, TEXT("Function payload struct reload should expose the old struct")));
		ASSERT_THAT(AreEqual(static_cast<UScriptStruct*>(StructAfterReload), Observation.NewStructSeenDuringReload, TEXT("Function payload struct reload should expose the new struct")));

		UClass* ReceiverClassAfterReload = FindGeneratedClass(&Engine, FunctionParameterReceiverClassName);
		ASSERT_THAT(IsNotNull(ReceiverClassAfterReload, TEXT("Function receiver class should exist after full reload")));
		UFunction* FunctionAfterReload = ReceiverClassAfterReload->FindFunctionByName(TEXT("ConsumePayload"));
		ASSERT_THAT(IsNotNull(FunctionAfterReload, TEXT("Function receiver should expose ConsumePayload after reload")));
		ASSERT_THAT(IsTrue(ValidateStructFunctionParameter(
			*TestRunner,
			FunctionAfterReload,
			TEXT("Payload"),
			StructAfterReload,
			TEXT("Reloaded ConsumePayload parameter should point at the reloaded script struct"))));

		UObject* ReceiverAfterReload = CreateObjectForClass(*TestRunner, ReceiverClassAfterReload, TEXT("Function receiver class should instantiate after reload"));
		ASSERT_THAT(IsNotNull(ReceiverAfterReload));
		ASSERT_THAT(IsTrue(ExecuteIntGlobal(
			*TestRunner,
			Engine,
			FunctionParameterModuleName,
			TEXT("int RunPayload(UHotReloadStructFunctionReceiver)"),
			ReceiverAfterReload,
			42,
			TEXT("Function parameter V2 should pass the reloaded script struct fields"))));
	}
};
