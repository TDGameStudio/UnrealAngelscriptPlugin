#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptClassGeneratorNameConflictTests,
	"Angelscript.TestModule.ClassGenerator.Analyze.NameConflict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName StructConflictModuleName = FName(TEXT("ASClassGeneratorStructNameConflict"));
	inline static const FName ClassConflictModuleName = FName(TEXT("ASClassGeneratorClassNameConflict"));
	inline static const FName ErrorRecoveryModuleName = FName(TEXT("ASClassGeneratorNameConflictRecovery"));

	static const FAngelscriptCompileTraceDiagnosticSummary* FindErrorDiagnosticContaining(
		const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics,
		const FString& Fragment)
	{
		return Diagnostics.FindByPredicate(
			[&Fragment](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
			{
				return Diagnostic.bIsError && Diagnostic.Message.Contains(Fragment);
			});
	}

	static UObject* CreatePackageConflictObject(const FName ObjectName)
	{
		return NewObject<UEnum>(
			FAngelscriptEngine::GetPackage(),
			ObjectName,
			RF_Transient);
	}

	static void RetireConflictObject(UObject* ConflictObject)
	{
		if (ConflictObject == nullptr)
		{
			return;
		}

		ConflictObject->Rename(
			nullptr,
			GetTransientPackage(),
			REN_DontCreateRedirectors | REN_NonTransactional);
		ConflictObject->MarkAsGarbage();
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

	TEST_METHOD(StructNameConflictWithNonStructObjectFailsClosed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		UObject* ConflictObject = CreatePackageConflictObject(TEXT("ClassGeneratorNameConflictStruct"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*StructConflictModuleName.ToString());
			RetireConflictObject(ConflictObject);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			USTRUCT()
			struct FClassGeneratorNameConflictStruct
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			StructConflictModuleName,
			TEXT("ClassGeneratorNameConflictStruct.as"),
			ScriptSource,
			true,
			Summary,
			true);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Struct-vs-nonstruct Unreal name conflict should fail compilation")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Struct-vs-nonstruct conflict should report an error compile result")));
		ASSERT_THAT(IsNotNull(
			FindErrorDiagnosticContaining(Summary.Diagnostics, TEXT("has a name conflict with non-struct unreal object")),
			TEXT("Struct-vs-nonstruct conflict should emit the class-generator diagnostic")));
		ASSERT_THAT(IsFalse(
			Engine.GetClass(TEXT("FClassGeneratorNameConflictStruct")).IsValid(),
			TEXT("Struct-vs-nonstruct conflict should not publish a generated struct desc")));
	}

	TEST_METHOD(ClassNameConflictWithNonClassObjectFailsClosed)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		UObject* ConflictObject = CreatePackageConflictObject(TEXT("UClassGeneratorNameConflictObject"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ClassConflictModuleName.ToString());
			RetireConflictObject(ConflictObject);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorNameConflictObject : UObject
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			ClassConflictModuleName,
			TEXT("ClassGeneratorNameConflictObject.as"),
			ScriptSource,
			true,
			Summary,
			true);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Class-vs-nonclass Unreal name conflict should fail compilation")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Class-vs-nonclass conflict should report an error compile result")));
		ASSERT_THAT(IsNotNull(
			FindErrorDiagnosticContaining(Summary.Diagnostics, TEXT("has a name conflict with non-class unreal object")),
			TEXT("Class-vs-nonclass conflict should emit the class-generator diagnostic")));
		ASSERT_THAT(IsNull(
			FindGeneratedClass(&Engine, TEXT("UClassGeneratorNameConflictObject")),
			TEXT("Class-vs-nonclass conflict should not publish the generated class")));
	}

	TEST_METHOD(FixingRejectedReloadPublishesCorrectedClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ErrorRecoveryModuleName.ToString());
		};

		const FString InitialSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorNameConflictRecovery : UObject
			{
				UPROPERTY()
				int Value = 1;
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, ErrorRecoveryModuleName, TEXT("ClassGeneratorNameConflictRecovery.as"), InitialSource),
			TEXT("Initial recovery module should compile")));
		ASSERT_THAT(IsNotNull(
			FindGeneratedClass(&Engine, TEXT("UClassGeneratorNameConflictRecovery")),
			TEXT("Initial recovery class should be published")));

		const FString RejectedSource = ASTEST_AS(R"AS(
			USTRUCT()
			struct FClassGeneratorNameConflictRecovery
			{
				UPROPERTY()
				int Value = 2;
			}
			)AS");

		ECompileResult RejectedResult = ECompileResult::FullyHandled;
		TestRunner->AddExpectedErrorPlain(
			TEXT("Full Reload is required due to UPROPERTY() or UFUNCTION() changes"),
			EAutomationExpectedErrorFlags::Contains,
			2);
		ASSERT_THAT(IsFalse(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, ErrorRecoveryModuleName, TEXT("ClassGeneratorNameConflictRecovery.as"), RejectedSource, RejectedResult),
			TEXT("Rejected reload should fail instead of swapping in the incompatible type kind")));
		ASSERT_THAT(IsTrue(
			RejectedResult == ECompileResult::Error || RejectedResult == ECompileResult::ErrorNeedFullReload,
			TEXT("Rejected reload should report an error result")));

		const FString FixedSource = ASTEST_AS(R"AS(
			UCLASS()
			class UClassGeneratorNameConflictRecovery : UObject
			{
				UPROPERTY()
				int Value = 3;
			}
			)AS");

		ECompileResult FixedResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, ErrorRecoveryModuleName, TEXT("ClassGeneratorNameConflictRecovery.as"), FixedSource, FixedResult),
			TEXT("Fixing a rejected reload should compile on the explicit full-reload path")));
		ASSERT_THAT(IsTrue(
			FixedResult == ECompileResult::FullyHandled || FixedResult == ECompileResult::PartiallyHandled,
			TEXT("Fixing a rejected reload should be handled")));

		UClass* FixedClass = FindGeneratedClass(&Engine, TEXT("UClassGeneratorNameConflictRecovery"));
		ASSERT_THAT(IsNotNull(FixedClass, TEXT("Fixing a rejected reload should publish the corrected class")));
		ASSERT_THAT(IsNotNull(
			FixedClass != nullptr ? FindFProperty<FIntProperty>(FixedClass, TEXT("Value")) : nullptr,
			TEXT("Fixing a rejected reload should publish the corrected reflected property")));
	}
};

#endif