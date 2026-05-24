#include "AngelscriptDebuggerScriptFixture.h"

#include "Misc/AssertionMacros.h"

namespace
{
	FAngelscriptDebuggerScriptFixture MakeFixture(
		FName ModuleName,
		FName GeneratedClassName,
		FName EntryFunctionName,
		FString EntryFunctionDeclaration,
		const FString& Filename,
		const FString& RawScriptSource,
		TMap<FName, FString> EvalPaths,
		bool bUseAnnotatedCompilation = true)
	{
		TArray<FString> Lines;
		RawScriptSource.ParseIntoArrayLines(Lines, false);

		TMap<FName, int32> LineMarkers;
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			FString& Line = Lines[LineIndex];
			int32 MarkerStart = INDEX_NONE;
			while (Line.FindChar(TEXT('/'), MarkerStart))
			{
				if (!Line.Mid(MarkerStart).StartsWith(TEXT("/*MARK:")))
				{
					MarkerStart += 1;
					continue;
				}

				const int32 MarkerNameStart = MarkerStart + 7;
				const int32 MarkerEnd = Line.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, MarkerNameStart);
				if (MarkerEnd == INDEX_NONE)
				{
					break;
				}

				const FString MarkerName = Line.Mid(MarkerNameStart, MarkerEnd - MarkerNameStart);
				LineMarkers.Add(FName(*MarkerName), LineIndex + 1);
				Line.RemoveAt(MarkerStart, (MarkerEnd - MarkerStart) + 2, EAllowShrinking::No);
			}
		}

		FAngelscriptDebuggerScriptFixture Fixture;
		Fixture.ModuleName = ModuleName;
		Fixture.GeneratedClassName = GeneratedClassName;
		Fixture.EntryFunctionName = EntryFunctionName;
		Fixture.EntryFunctionDeclaration = MoveTemp(EntryFunctionDeclaration);
		Fixture.Filename = Filename;
		Fixture.ScriptSource = FString::Join(Lines, TEXT("\n"));
		Fixture.LineMarkers = MoveTemp(LineMarkers);
		Fixture.EvalPaths = MoveTemp(EvalPaths);
		Fixture.bUseAnnotatedCompilation = bUseAnnotatedCompilation;
		return Fixture;
	}
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreateBreakpointFixture()
{
	return CreateNamedBreakpointFixture(
		TEXT("DebuggerBreakpointFixture"),
		TEXT("DebuggerBreakpointFixture.as"),
		5);
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreateNamedBreakpointFixture(
	FName ModuleName,
	const FString& Filename,
	int32 StoredValue)
{
	const FString ScriptSource = FString::Printf(TEXT(R"AS(int Helper(int Input)
{
int StoredValue = %d;

/*MARK:BreakpointHelperLine*/ int LocalValue = Input + StoredValue;
return LocalValue;
}

int RunTestCase()
{
	/*MARK:BreakpointEntryLine*/ int StartValue = 3;
	if (StartValue > 0)
	{
		return Helper(StartValue);
	}

	/*MARK:BreakpointInactiveBranchLine*/ return Helper(-1);
}
)AS"), StoredValue);

	return MakeFixture(
		ModuleName,
		NAME_None,
		NAME_None,
		TEXT("int RunTestCase()"),
		Filename,
		ScriptSource,
		{
			{TEXT("LocalValuePath"), TEXT("0:LocalValue")},
			{TEXT("ThisStoredValuePath"), TEXT("0:this.StoredValue")},
			{TEXT("ThisScopePath"), TEXT("0:%this%")}
		},
		false);
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreateSteppingFixture()
{
	return MakeFixture(
		TEXT("DebuggerSteppingFixture"),
		NAME_None,
		NAME_None,
		TEXT("int RunTestCase()"),
		TEXT("DebuggerSteppingFixture.as"),
		TEXT(R"AS(int Inner(int Value)
{
/*MARK:StepInnerEntryLine*/ int StoredValue = 9;

/*MARK:StepInnerLine*/ int InnerValue = Value + StoredValue;
return InnerValue;
}

int RunTestCase()
{
	/*MARK:StepCallLine*/ int Result = Inner(4);
	/*MARK:StepAfterCallLine*/ Result += 1;
	int FinalResult = Result;
	return FinalResult;
}
)AS"),
		{
			{TEXT("InnerValuePath"), TEXT("0:InnerValue")},
			{TEXT("ResultPath"), TEXT("0:Result")}
		},
		false);
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreatePauseFixture()
{
	return MakeFixture(
		TEXT("DebuggerPauseFixture"),
		NAME_None,
		NAME_None,
		TEXT("int RunTestCase()"),
		TEXT("DebuggerPauseFixture.as"),
		TEXT(R"AS(int RunTestCase()
{
/*MARK:PauseReadyLine*/ int Total = 1;
for (int Outer = 0; Outer < 50; ++Outer)
{
	/*MARK:PauseForHeaderLine*/ for (int Inner = 0; Inner < 100; ++Inner)
	{
		/*MARK:PauseLoopLine*/ Total += 1;
	}
}

return Total;
}
)AS"),
		{},
		false);
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreateCallstackFixture()
{
	return MakeFixture(
		TEXT("DebuggerCallstackFixture"),
		TEXT("UDebuggerCallstackFixture"),
		TEXT("Entry"),
		TEXT("int Entry()"),
		TEXT("DebuggerCallstackFixture.as"),
		TEXT(R"AS(const int GlobalCounter = 7;

UCLASS()
class UDebuggerCallstackFixture : UObject
{
UPROPERTY()
int MemberValue = 5;

UFUNCTION()
int Leaf(int LocalValue)
{
	/*MARK:CallstackLeafLine*/ int Combined = LocalValue + MemberValue + GlobalCounter;
	return Combined;
}

UFUNCTION()
int Middle(int LocalValue)
/*MARK:CallstackMiddleLine*/ {
	return Leaf(LocalValue + 1);
}

UFUNCTION()
int Entry()
/*MARK:CallstackEntryLine*/ {
	return Middle(3);
}
}
)AS"),
		{
			{TEXT("LeafLocalValuePath"), TEXT("0:LocalValue")},
			{TEXT("LeafCombinedPath"), TEXT("0:Combined")},
			{TEXT("ThisMemberValuePath"), TEXT("0:this.MemberValue")},
			{TEXT("ThisScopePath"), TEXT("0:%this%")},
			{TEXT("ModuleGlobalCounterPath"), TEXT("0:%module%.GlobalCounter")},
			{TEXT("LocalScopePath"), TEXT("0:%local%")},
			{TEXT("ModuleScopePath"), TEXT("0:%module%")}
		});
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreateBindingFixture()
{
	return MakeFixture(
		TEXT("DebuggerBindingFixture"),
		TEXT("UDebuggerBindingFixture"),
		TEXT("TriggerDebugBreak"),
		TEXT("void TriggerDebugBreak()"),
		TEXT("DebuggerBindingFixture.as"),
		TEXT(R"AS(UCLASS()
class UDebuggerBindingFixture : UObject
{
UFUNCTION()
void TriggerDebugBreak()
/*MARK:BindingDebugBreakLine*/ {
	DebugBreak();
}

UFUNCTION()
bool TriggerEnsure(bool Condition, const FString& Message)
/*MARK:BindingEnsureLine*/ {
	return ensure(Condition, Message);
}

UFUNCTION()
void TriggerCheck(bool Condition, const FString& Message)
/*MARK:BindingCheckLine*/ {
	check(Condition, Message);
}

UFUNCTION()
FString FormatCurrentCallstack()
{
	/*MARK:BindingCallstackLine*/ return FormatAngelscriptCallstack();
}
}
)AS"),
		{
			{TEXT("ThisScopePath"), TEXT("0:%this%")}
		});
}

FAngelscriptDebuggerScriptFixture FAngelscriptDebuggerScriptFixture::CreateBlueprintFrameFixture()
{
	return MakeFixture(
		TEXT("DebuggerBlueprintFrameFixture"),
		TEXT("UDebuggerBlueprintFrameFixture"),
		NAME_None,
		TEXT(""),
		TEXT("DebuggerBlueprintFrameFixture.as"),
		TEXT(R"AS(UCLASS()
class UDebuggerBlueprintFrameFixture : UObject
{
UPROPERTY()
int ScriptValue = 5;

UPROPERTY()
int LastBreakResult = -1;

UFUNCTION(BlueprintCallable)
int BreakInScript(int Input)
{
	/*MARK:BlueprintScriptBreakLine*/ int Result = Input + ScriptValue;
	LastBreakResult = Result;
	return Result;
}
}
)AS"),
		{});
}

bool FAngelscriptDebuggerScriptFixture::Compile(FAngelscriptEngine& Engine) const
{
	if (bUseAnnotatedCompilation)
	{
		return CompileAnnotatedModuleFromMemory(&Engine, ModuleName, Filename, ScriptSource);
	}

	return CompileModuleFromMemory(&Engine, ModuleName, Filename, ScriptSource);
}

int32 FAngelscriptDebuggerScriptFixture::GetLine(FName Marker) const
{
	const int32* Line = LineMarkers.Find(Marker);
	check(Line != nullptr);
	return *Line;
}

FString FAngelscriptDebuggerScriptFixture::GetEvalPath(FName Marker) const
{
	const FString* EvalPath = EvalPaths.Find(Marker);
	check(EvalPath != nullptr);
	return *EvalPath;
}

UClass* FAngelscriptDebuggerScriptFixture::FindGeneratedClass(FAngelscriptEngine& Engine) const
{
	return ::FindGeneratedClass(&Engine, GeneratedClassName);
}

UFunction* FAngelscriptDebuggerScriptFixture::FindGeneratedFunction(FAngelscriptEngine& Engine, FName FunctionName) const
{
	UClass* GeneratedClass = FindGeneratedClass(Engine);
	return GeneratedClass != nullptr ? ::FindGeneratedFunction(GeneratedClass, FunctionName) : nullptr;
}

