#include "CQTest.h"
#include "Performance/AngelscriptPerformanceTestTypes.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptPerformanceTestUtils.h"
#include "AngelscriptReflectiveAccess.h"
#include "AngelscriptTestMacros.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private
{
	constexpr int32 WarmupRuns = 1;
	constexpr int32 MeasurementRuns = 3;
	constexpr int32 IterationsPerMeasurement = 10000;

	struct FMeasuredSamples
	{
		TArray<double> Samples;
		int32 Checksum = 0;
	};

	template <typename CallableType>
	FMeasuredSamples MeasureSamples(CallableType&& Callable)
	{
		for (int32 WarmupIndex = 0; WarmupIndex < WarmupRuns; ++WarmupIndex)
		{
			Callable();
		}

		FMeasuredSamples Result;
		for (int32 MeasurementIndex = 0; MeasurementIndex < MeasurementRuns; ++MeasurementIndex)
		{
			const double StartSeconds = FPlatformTime::Seconds();
			Result.Checksum = Callable();
			Result.Samples.Add(FPlatformTime::Seconds() - StartSeconds);
		}
		return Result;
	}

	void AddMetric(
		TArray<FAngelscriptPerformanceMetric>& Metrics,
		const FString& Name,
		const TArray<double>& Samples,
		const FString& Source)
	{
		Metrics.Add({ Name, Samples, ComputeMedian(Samples), TEXT("seconds"), Source });
	}

	bool WriteAndVerifyMetrics(
		FAutomationTestBase& Test,
		const FString& RunId,
		const FString& TestGroup,
		const TArray<FAngelscriptPerformanceMetric>& Metrics)
	{
		const FString MetricsPath = WritePerformanceMetricsArtifact(
			RunId,
			TestGroup,
			Metrics,
			{
				FString::Printf(TEXT("warmup_runs=%d"), WarmupRuns),
				FString::Printf(TEXT("measurement_runs=%d"), MeasurementRuns),
				FString::Printf(TEXT("iterations_per_measurement=%d"), IterationsPerMeasurement),
			});

		return Test.TestTrue(
			TEXT("Runtime performance microbenchmark should write metrics.json"),
			FPlatformFileManager::Get().GetPlatformFile().FileExists(*MetricsPath));
	}

	UClass* CompileBenchmarkCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const FString& ScriptSource,
		FName GeneratedClassName)
	{
		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			FString::Printf(TEXT("%s.as"), *ModuleName.ToString()),
			ScriptSource,
			GeneratedClassName);
	}

	TUniquePtr<FAngelscriptEngine> CreatePerformanceEditorBindingEngine()
	{
		UAngelscriptPerformanceTestTargetObject::StaticClass();

		FAngelscriptEngineConfig Config = FAngelscriptEngineConfig::FromCurrentProcess();
		Config.bIsEditor = true;
		Config.bForcePreprocessEditorCode = true;

		return CreateScriptScanFreeFullEngineForTesting(Config, FAngelscriptEngineDependencies::CreateDefault());
	}
}


namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private
{

bool RunRuntimePerformanceScriptSelf(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	bool bWroteMetrics = false;
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	do
	{
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASRuntimePerformanceScriptSelf"));
		ASTEST_RESET_ENGINE(Engine);
	};

	UClass* ScriptClass = CompileBenchmarkCarrier(
		Test,
		Engine,
		TEXT("ASRuntimePerformanceScriptSelf"),
		TEXT(R"AS(
UCLASS()
class URuntimePerformanceScriptSelfCarrier : UObject
{
	UFUNCTION()
	int RunEmpty(int Iterations)
	{
		int Checksum = 0;
		for (int Index = 0; Index < Iterations; ++Index)
		{
			Empty();
			Checksum += Index & 1;
		}
		return Checksum;
	}

	UFUNCTION()
	int RunArithmetic(int Iterations)
	{
		int Checksum = 0;
		for (int Index = 0; Index < Iterations; ++Index)
		{
			Checksum += Add(Index, 3);
			Checksum -= Subtract(Index, 2);
			Checksum += Multiply(Index & 3, 2);
			Checksum += Divide(Index + 4, 2);
		}
		return Checksum;
	}

	void Empty()
	{
	}

	int Add(int A, int B) { return A + B; }
	int Subtract(int A, int B) { return A - B; }
	int Multiply(int A, int B) { return A * B; }
	int Divide(int A, int B) { return Math::IntegerDivisionTrunc(A, B); }
}
)AS"),
		TEXT("URuntimePerformanceScriptSelfCarrier"));
	if (!Test.TestNotNull(TEXT("Script self benchmark carrier should compile"), ScriptClass))
	{
		break;
	}

	UObject* Carrier = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("RuntimePerformanceScriptSelfCarrier"));
	if (!Test.TestNotNull(TEXT("Script self benchmark carrier should instantiate"), Carrier))
	{
		break;
	}

	auto RunScriptFunction = [&Test, Carrier](const FName FunctionName) -> int32
	{
		FFunctionInvoker Invoker(Test, Carrier, FunctionName);
		Invoker.AddParam<int32>(IterationsPerMeasurement);
		return Invoker.CallAndReturn<int32>(INDEX_NONE);
	};

	const FMeasuredSamples ScriptEmpty = MeasureSamples([&]() { return RunScriptFunction(TEXT("RunEmpty")); });
	const FMeasuredSamples ScriptArithmetic = MeasureSamples([&]() { return RunScriptFunction(TEXT("RunArithmetic")); });
	const FMeasuredSamples NativeEmpty = MeasureSamples([&]()
	{
		int32 Checksum = 0;
		for (int32 Index = 0; Index < IterationsPerMeasurement; ++Index)
		{
			UAngelscriptPerformanceTestTargetObject::StaticNoOp();
			Checksum += Index & 1;
		}
		return Checksum;
	});
	const FMeasuredSamples NativeArithmetic = MeasureSamples([&]()
	{
		int32 Checksum = 0;
		for (int32 Index = 0; Index < IterationsPerMeasurement; ++Index)
		{
			Checksum += UAngelscriptPerformanceTestTargetObject::StaticAdd(Index, 3);
			Checksum -= UAngelscriptPerformanceTestTargetObject::StaticSubtract(Index, 2);
			Checksum += UAngelscriptPerformanceTestTargetObject::StaticMultiply(Index & 3, 2);
			Checksum += UAngelscriptPerformanceTestTargetObject::StaticDivide(Index + 4, 2);
		}
		return Checksum;
	});

	Test.TestEqual(TEXT("Script and native empty-call checksums should match"), ScriptEmpty.Checksum, NativeEmpty.Checksum);
	Test.TestEqual(TEXT("Script and native arithmetic checksums should match"), ScriptArithmetic.Checksum, NativeArithmetic.Checksum);

	TArray<FAngelscriptPerformanceMetric> Metrics;
	AddMetric(Metrics, TEXT("runtime.as.script_self.empty_seconds"), ScriptEmpty.Samples, TEXT("Angelscript"));
	AddMetric(Metrics, TEXT("runtime.as.script_self.arithmetic_seconds"), ScriptArithmetic.Samples, TEXT("Angelscript"));
	AddMetric(Metrics, TEXT("runtime.native.script_self.empty_seconds"), NativeEmpty.Samples, TEXT("NativeCpp"));
	AddMetric(Metrics, TEXT("runtime.native.script_self.arithmetic_seconds"), NativeArithmetic.Samples, TEXT("NativeCpp"));
	bWroteMetrics = WriteAndVerifyMetrics(Test, TEXT("RuntimeMicrobenchmark_ScriptSelf"), TEXT("Angelscript.TestModule.Performance.ScriptSelf"), Metrics);

	}
	while (false);
	}
	return !Test.HasAnyErrors() && bWroteMetrics;
}

bool RunRuntimePerformanceNativeProperty(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private;
	TUniquePtr<FAngelscriptEngine> EngineOwner = CreatePerformanceEditorBindingEngine();
	if (!Test.TestTrue(TEXT("Native property benchmark engine should create"), EngineOwner.IsValid()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = *EngineOwner;
	bool bWroteMetrics = false;
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	do
	{
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASRuntimePerformanceNativeProperty"));
	};

	UClass* ScriptClass = CompileBenchmarkCarrier(
		Test,
		Engine,
		TEXT("ASRuntimePerformanceNativeProperty"),
		TEXT(R"AS(
#if EDITOR
UCLASS()
class URuntimePerformanceNativePropertyCarrier : UObject
{
	UFUNCTION()
	int RunScalarProperties(int Iterations)
	{
		UAngelscriptPerformanceTestTargetObject Target = Cast<UAngelscriptPerformanceTestTargetObject>(FindClass("UAngelscriptPerformanceTestTargetObject").GetDefaultObject());
		int Checksum = 0;
		for (int Index = 0; Index < Iterations; ++Index)
		{
			Target.BoolValue = (Index & 1) == 0;
			Target.Int32Value = Index;
			Target.DoubleValue = double(Index) * 0.5;
			Target.NameValue = n"RuntimePerformance";
			Target.StringValue = "RuntimePerformance";
			Checksum += Target.BoolValue ? 1 : 0;
			Checksum += Target.Int32Value;
			Checksum += int(Target.DoubleValue);
			Checksum += Target.StringValue.Len();
		}
		return Checksum;
	}

	UFUNCTION()
	int RunContainerProperties(int Iterations)
	{
		UAngelscriptPerformanceTestTargetObject Target = Cast<UAngelscriptPerformanceTestTargetObject>(FindClass("UAngelscriptPerformanceTestTargetObject").GetDefaultObject());
		int Checksum = 0;
		for (int Index = 0; Index < Iterations; ++Index)
		{
			Target.ArrayValue.Empty();
			Target.ArrayValue.Add(Index);
			Target.SetValue.Empty();
			Target.SetValue.Add(Index);
			Target.MapValue.Empty();
			Target.MapValue.Add(n"Value", Index);
			Checksum += Target.ArrayValue[0];
			Checksum += Target.SetValue.Contains(Index) ? 1 : 0;
			int Found = 0;
			Target.MapValue.Find(n"Value", Found);
			Checksum += Found;
		}
		return Checksum;
	}
}
#endif
)AS"),
		TEXT("URuntimePerformanceNativePropertyCarrier"));
	if (!Test.TestNotNull(TEXT("Native property benchmark carrier should compile"), ScriptClass))
	{
		break;
	}

	UObject* Carrier = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("RuntimePerformanceNativePropertyCarrier"));
	UAngelscriptPerformanceTestTargetObject* Target = GetMutableDefault<UAngelscriptPerformanceTestTargetObject>();
	if (!Test.TestNotNull(TEXT("Native property benchmark carrier should instantiate"), Carrier)
		|| !Test.TestNotNull(TEXT("Native property benchmark target CDO should resolve"), Target))
	{
		break;
	}
	ON_SCOPE_EXIT
	{
		Target->ResetValues();
	};

	auto RunScriptFunction = [&Test, Carrier](const FName FunctionName) -> int32
	{
		FFunctionInvoker Invoker(Test, Carrier, FunctionName);
		Invoker.AddParam<int32>(IterationsPerMeasurement);
		return Invoker.CallAndReturn<int32>(INDEX_NONE);
	};

	const FMeasuredSamples ScriptScalar = MeasureSamples([&]() { Target->ResetValues(); return RunScriptFunction(TEXT("RunScalarProperties")); });
	const FMeasuredSamples ScriptContainers = MeasureSamples([&]() { Target->ResetValues(); return RunScriptFunction(TEXT("RunContainerProperties")); });
	const FMeasuredSamples NativeScalar = MeasureSamples([&]() { Target->ResetValues(); return Target->RunNativeScalarPropertyLoop(IterationsPerMeasurement); });
	const FMeasuredSamples NativeContainers = MeasureSamples([&]() { Target->ResetValues(); return Target->RunNativeContainerPropertyLoop(IterationsPerMeasurement); });

	Test.TestEqual(TEXT("Script and native scalar property checksums should match"), ScriptScalar.Checksum, NativeScalar.Checksum);
	Test.TestEqual(TEXT("Script and native container property checksums should match"), ScriptContainers.Checksum, NativeContainers.Checksum);

	TArray<FAngelscriptPerformanceMetric> Metrics;
	AddMetric(Metrics, TEXT("runtime.as.property.scalar_seconds"), ScriptScalar.Samples, TEXT("Angelscript"));
	AddMetric(Metrics, TEXT("runtime.as.property.container_seconds"), ScriptContainers.Samples, TEXT("Angelscript"));
	AddMetric(Metrics, TEXT("runtime.native.property.scalar_seconds"), NativeScalar.Samples, TEXT("NativeCpp"));
	AddMetric(Metrics, TEXT("runtime.native.property.container_seconds"), NativeContainers.Samples, TEXT("NativeCpp"));
	bWroteMetrics = WriteAndVerifyMetrics(Test, TEXT("RuntimeMicrobenchmark_NativeProperty"), TEXT("Angelscript.TestModule.Performance.NativeProperty"), Metrics);

	}
	while (false);
	}
	return !Test.HasAnyErrors() && bWroteMetrics;
}

bool RunRuntimePerformanceNativeFunction(FAutomationTestBase& Test)
{
	using namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private;
	TUniquePtr<FAngelscriptEngine> EngineOwner = CreatePerformanceEditorBindingEngine();
	if (!Test.TestTrue(TEXT("Native function benchmark engine should create"), EngineOwner.IsValid()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = *EngineOwner;
	bool bWroteMetrics = false;
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	do
	{
	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASRuntimePerformanceNativeFunction"));
	};

	UClass* ScriptClass = CompileBenchmarkCarrier(
		Test,
		Engine,
		TEXT("ASRuntimePerformanceNativeFunction"),
		TEXT(R"AS(
#if EDITOR
UCLASS()
class URuntimePerformanceNativeFunctionCarrier : UObject
{
	UFUNCTION()
	int RunScalarFunctions(int Iterations)
	{
		UAngelscriptPerformanceTestTargetObject Target = Cast<UAngelscriptPerformanceTestTargetObject>(FindClass("UAngelscriptPerformanceTestTargetObject").GetDefaultObject());
		int Checksum = 0;
		for (int Index = 0; Index < Iterations; ++Index)
		{
			UAngelscriptPerformanceTestTargetObject::StaticNoOp();
			Checksum += UAngelscriptPerformanceTestTargetObject::StaticAdd(Index, 3);
			Target.MemberNoOp();
			Target.SetInt32ValueFunction(Index);
			Checksum += Target.GetInt32ValueFunction();
			Target.SetDoubleValueFunction(double(Index) * 0.5);
			Checksum += int(Target.GetDoubleValueFunction());
			Target.SetStringValueFunction("RuntimePerformance");
			Checksum += Target.GetStringValueFunction().Len();
		}
		return Checksum;
	}

	UFUNCTION()
	int RunContainerFunctions(int Iterations)
	{
		UAngelscriptPerformanceTestTargetObject Target = Cast<UAngelscriptPerformanceTestTargetObject>(FindClass("UAngelscriptPerformanceTestTargetObject").GetDefaultObject());
		int Checksum = 0;
		for (int Index = 0; Index < Iterations; ++Index)
		{
			TArray<int> ArrayValue;
			ArrayValue.Add(Index);
			Target.SetArrayValueFunction(ArrayValue);
			TArray<int> ReturnedArray = Target.GetArrayValueFunction();
			Checksum += ReturnedArray[0];

			TSet<int> SetValue;
			SetValue.Add(Index);
			Target.SetSetValueFunction(SetValue);
			TSet<int> ReturnedSet = Target.GetSetValueFunction();
			Checksum += ReturnedSet.Contains(Index) ? 1 : 0;

			TMap<FName, int> MapValue;
			MapValue.Add(n"Value", Index);
			Target.SetMapValueFunction(MapValue);
			TMap<FName, int> ReturnedMap = Target.GetMapValueFunction();
			int Found = 0;
			ReturnedMap.Find(n"Value", Found);
			Checksum += Found;
		}
		return Checksum;
	}
}
#endif
)AS"),
		TEXT("URuntimePerformanceNativeFunctionCarrier"));
	if (!Test.TestNotNull(TEXT("Native function benchmark carrier should compile"), ScriptClass))
	{
		break;
	}

	UObject* Carrier = NewObject<UObject>(GetTransientPackage(), ScriptClass, TEXT("RuntimePerformanceNativeFunctionCarrier"));
	UAngelscriptPerformanceTestTargetObject* Target = GetMutableDefault<UAngelscriptPerformanceTestTargetObject>();
	if (!Test.TestNotNull(TEXT("Native function benchmark carrier should instantiate"), Carrier)
		|| !Test.TestNotNull(TEXT("Native function benchmark target CDO should resolve"), Target))
	{
		break;
	}
	ON_SCOPE_EXIT
	{
		Target->ResetValues();
	};

	auto RunScriptFunction = [&Test, Carrier](const FName FunctionName) -> int32
	{
		FFunctionInvoker Invoker(Test, Carrier, FunctionName);
		Invoker.AddParam<int32>(IterationsPerMeasurement);
		return Invoker.CallAndReturn<int32>(INDEX_NONE);
	};

	const FMeasuredSamples ScriptScalar = MeasureSamples([&]() { Target->ResetValues(); return RunScriptFunction(TEXT("RunScalarFunctions")); });
	const FMeasuredSamples ScriptContainers = MeasureSamples([&]() { Target->ResetValues(); return RunScriptFunction(TEXT("RunContainerFunctions")); });
	const FMeasuredSamples NativeScalar = MeasureSamples([&]() { Target->ResetValues(); return Target->RunNativeScalarFunctionLoop(IterationsPerMeasurement); });
	const FMeasuredSamples NativeContainers = MeasureSamples([&]() { Target->ResetValues(); return Target->RunNativeContainerFunctionLoop(IterationsPerMeasurement); });

	Test.TestEqual(TEXT("Script and native scalar function checksums should match"), ScriptScalar.Checksum, NativeScalar.Checksum);
	Test.TestEqual(TEXT("Script and native container function checksums should match"), ScriptContainers.Checksum, NativeContainers.Checksum);

	TArray<FAngelscriptPerformanceMetric> Metrics;
	AddMetric(Metrics, TEXT("runtime.as.function.scalar_seconds"), ScriptScalar.Samples, TEXT("Angelscript"));
	AddMetric(Metrics, TEXT("runtime.as.function.container_seconds"), ScriptContainers.Samples, TEXT("Angelscript"));
	AddMetric(Metrics, TEXT("runtime.native.function.scalar_seconds"), NativeScalar.Samples, TEXT("NativeCpp"));
	AddMetric(Metrics, TEXT("runtime.native.function.container_seconds"), NativeContainers.Samples, TEXT("NativeCpp"));
	bWroteMetrics = WriteAndVerifyMetrics(Test, TEXT("RuntimeMicrobenchmark_NativeFunction"), TEXT("Angelscript.TestModule.Performance.NativeFunction"), Metrics);

	}
	while (false);
	}
	return !Test.HasAnyErrors() && bWroteMetrics;
}

}

TEST_CLASS_WITH_FLAGS(FAngelscriptRuntimeMicrobenchmarkTests,
	"Angelscript.TestModule.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ScriptSelf)
	{
		using namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private;
		ASSERT_THAT(IsTrue(RunRuntimePerformanceScriptSelf(*TestRunner)));
	}

	TEST_METHOD(NativeProperty)
	{
		using namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private;
		ASSERT_THAT(IsTrue(RunRuntimePerformanceNativeProperty(*TestRunner)));
	}

	TEST_METHOD(NativeFunction)
	{
		using namespace AngelscriptTest_Performance_RuntimeMicrobenchmarkTests_Private;
		ASSERT_THAT(IsTrue(RunRuntimePerformanceNativeFunction(*TestRunner)));
	}
};

#endif
