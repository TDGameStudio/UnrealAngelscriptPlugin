#pragma once

#include "AngelscriptNativeTestSupport.h"
#include "AngelscriptSDKTestExecutionHelpers.h"

#include "Misc/AutomationTest.h"

#include <cstring>

#include "StartAngelscriptHeaders.h"
#include "source/as_builder.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptBuilderTestSupport
{
	using namespace AngelscriptNativeTestSupport;

	inline asCModule* CreateBuilderModule(asIScriptEngine* ScriptEngine, const char* ModuleName)
	{
		return ScriptEngine != nullptr
			? static_cast<asCModule*>(ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE))
			: nullptr;
	}

	inline bool AddBuilderSection(asCModule& Module, const char* SectionName, const char* Source)
	{
		const int Result = Module.AddScriptSection(SectionName, Source, std::strlen(Source), 0);
		return Result >= 0;
	}

	inline const TCHAR* BoolText(bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	inline FString ToTestString(const char* Value)
	{
		return Value != nullptr ? FString(UTF8_TO_TCHAR(Value)) : FString(TEXT("<null>"));
	}

	inline int32 CountSourceLines(const char* Source)
	{
		if (Source == nullptr || Source[0] == '\0')
		{
			return 0;
		}

		int32 LineCount = 1;
		for (const char* Cursor = Source; *Cursor != '\0'; ++Cursor)
		{
			if (*Cursor == '\n')
			{
				++LineCount;
			}
		}

		return LineCount;
	}

	inline FString JoinItems(const TArray<FString>& Items)
	{
		if (Items.IsEmpty())
		{
			return TEXT("<empty>");
		}

		FString Result = Items[0];
		for (int32 Index = 1; Index < Items.Num(); ++Index)
		{
			Result += TEXT(", ");
			Result += Items[Index];
		}

		return Result;
	}

	inline FString DescribeBuilderCounts(const asCBuilder& Builder, bool bIncludeDiagnosticCounters)
	{
		const FString ErrorCount = bIncludeDiagnosticCounters ? FString::FromInt(Builder.numErrors) : FString(TEXT("<unknown>"));
		const FString WarningCount = bIncludeDiagnosticCounters ? FString::FromInt(Builder.numWarnings) : FString(TEXT("<unknown>"));
		return FString::Printf(
			TEXT("builder{scripts=%d, parsers=%d, classDecls=%d, interfaceDecls=%d, namedTypeDecls=%d, funcDefs=%d, mixins=%d, factories=%d, functions=%d, globals=%d, errors=%s, warnings=%s, scriptsParsed=%s, silent=%s}"),
			static_cast<int32>(Builder.scripts.GetLength()),
			static_cast<int32>(Builder.parsers.GetLength()),
			static_cast<int32>(Builder.classDeclarations.GetLength()),
			static_cast<int32>(Builder.interfaceDeclarations.GetLength()),
			static_cast<int32>(Builder.namedTypeDeclarations.GetLength()),
			static_cast<int32>(Builder.funcDefs.GetLength()),
			static_cast<int32>(Builder.mixinClasses.GetLength()),
			static_cast<int32>(Builder.factories.GetLength()),
			static_cast<int32>(Builder.functions.GetLength()),
			static_cast<int32>(Builder.globVariableList.GetLength()),
			*ErrorCount,
			*WarningCount,
			BoolText(Builder.scriptsParsed),
			BoolText(Builder.silent));
	}

	inline FString DescribeModuleCounts(const asCModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("module{<null>}");
		}

		return FString::Printf(
			TEXT("module{name=%s, functions=%d, globals=%d, objectTypes=%d, enums=%d, typedefs=%d, imports=%d, builder=%s}"),
			*ToTestString(Module->GetName()),
			static_cast<int32>(Module->GetFunctionCount()),
			static_cast<int32>(Module->GetGlobalVarCount()),
			static_cast<int32>(Module->GetObjectTypeCount()),
			static_cast<int32>(Module->GetEnumCount()),
			static_cast<int32>(Module->GetTypedefCount()),
			static_cast<int32>(Module->GetImportedFunctionCount()),
			BoolText(Module->builder != nullptr));
	}

	inline FString DescribeClassDeclarations(const asCBuilder& Builder)
	{
		TArray<FString> Items;
		for (asUINT Index = 0; Index < Builder.classDeclarations.GetLength(); ++Index)
		{
			const sClassDeclaration* Declaration = Builder.classDeclarations[Index];
			if (Declaration == nullptr)
			{
				Items.Add(TEXT("<null>"));
				continue;
			}

			Items.Add(FString::Printf(
				TEXT("%s{type=%s, propInits=%d, valid=%d, resolved=%s, layouted=%s}"),
				*ToTestString(Declaration->name.AddressOf()),
				Declaration->typeInfo != nullptr ? *ToTestString(Declaration->typeInfo->GetName()) : TEXT("<null>"),
				static_cast<int32>(Declaration->propInits.GetLength()),
				Declaration->validState,
				BoolText(Declaration->hasResolved),
				BoolText(Declaration->hasLayouted)));
		}

		return JoinItems(Items);
	}

	inline FString DescribeNamedTypeDeclarations(const asCBuilder& Builder)
	{
		TArray<FString> Items;
		for (asUINT Index = 0; Index < Builder.namedTypeDeclarations.GetLength(); ++Index)
		{
			const sClassDeclaration* Declaration = Builder.namedTypeDeclarations[Index];
			Items.Add(Declaration != nullptr ? ToTestString(Declaration->name.AddressOf()) : FString(TEXT("<null>")));
		}

		return JoinItems(Items);
	}

	inline FString DescribeFunctionDescriptions(const asCBuilder& Builder)
	{
		TArray<FString> Items;
		for (asUINT Index = 0; Index < Builder.functions.GetLength(); ++Index)
		{
			const sFunctionDescription* Function = Builder.functions[Index];
			if (Function == nullptr || Function->funcId < 0)
			{
				Items.Add(TEXT("<null>"));
				continue;
			}

			const FString Owner = Function->objType != nullptr ? ToTestString(Function->objType->name.AddressOf()) : FString(TEXT("<global>"));
			Items.Add(FString::Printf(
				TEXT("%s::%s{id=%d, shared=%s}"),
				*Owner,
				*ToTestString(Function->name.AddressOf()),
				Function->funcId,
				BoolText(Function->isExistingShared)));
		}

		return JoinItems(Items);
	}

	inline FString DescribeGlobalDescriptions(const asCBuilder& Builder)
	{
		TArray<FString> Items;
		for (asUINT Index = 0; Index < Builder.globVariableList.GetLength(); ++Index)
		{
			const sGlobalVariableDescription* Global = Builder.globVariableList[Index];
			if (Global == nullptr)
			{
				Items.Add(TEXT("<null>"));
				continue;
			}

			Items.Add(FString::Printf(
				TEXT("%s{index=%d, property=%s, compiled=%s, pureConst=%s, enumValue=%s}"),
				*ToTestString(Global->name.AddressOf()),
				Global->index,
				BoolText(Global->property != nullptr),
				BoolText(Global->isCompiled),
				BoolText(Global->isPureConstant),
				BoolText(Global->isEnumValue)));
		}

		return JoinItems(Items);
	}

	inline FString DescribeModuleFunctions(const asCModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null>");
		}

		TArray<FString> Items;
		for (asUINT Index = 0; Index < Module->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(Index);
			if (Function == nullptr)
			{
				Items.Add(TEXT("<null>"));
				continue;
			}

			asUINT BytecodeLength = 0;
			Function->GetByteCode(&BytecodeLength);
			Items.Add(FString::Printf(
				TEXT("%s{ns=%s, section=%s, bytecode=%d}"),
				*ToTestString(Function->GetDeclaration(true, true, false, true)),
				*ToTestString(Function->GetNamespace()),
				*ToTestString(Function->GetScriptSectionName()),
				static_cast<int32>(BytecodeLength)));
		}

		return JoinItems(Items);
	}

	inline FString DescribeModuleGlobals(const asCModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null>");
		}

		TArray<FString> Items;
		for (asUINT Index = 0; Index < Module->GetGlobalVarCount(); ++Index)
		{
			const char* GlobalName = nullptr;
			const char* GlobalNamespace = nullptr;
			int GlobalTypeId = asINVALID_TYPE;
			bool bIsConst = false;
			Module->GetGlobalVar(Index, &GlobalName, &GlobalNamespace, &GlobalTypeId, &bIsConst);
			Items.Add(FString::Printf(
				TEXT("%s{name=%s, ns=%s, typeId=%d, const=%s}"),
				*ToTestString(Module->GetGlobalVarDeclaration(Index, true)),
				*ToTestString(GlobalName),
				*ToTestString(GlobalNamespace),
				GlobalTypeId,
				BoolText(bIsConst)));
		}

		return JoinItems(Items);
	}

	inline FString DescribeModuleTypes(const asCModule* Module)
	{
		if (Module == nullptr)
		{
			return TEXT("<null>");
		}

		TArray<FString> Items;
		for (asUINT Index = 0; Index < Module->GetObjectTypeCount(); ++Index)
		{
			asITypeInfo* TypeInfo = Module->GetObjectTypeByIndex(Index);
			if (TypeInfo == nullptr)
			{
				Items.Add(TEXT("<null>"));
				continue;
			}

			asITypeInfo* BaseType = TypeInfo->GetBaseType();
			Items.Add(FString::Printf(
				TEXT("%s%s%s{props=%d, methods=%d, base=%s}"),
				*ToTestString(TypeInfo->GetNamespace()),
				TypeInfo->GetNamespace() != nullptr && TypeInfo->GetNamespace()[0] != '\0' ? TEXT("::") : TEXT(""),
				*ToTestString(TypeInfo->GetName()),
				static_cast<int32>(TypeInfo->GetPropertyCount()),
				static_cast<int32>(TypeInfo->GetMethodCount()),
				BaseType != nullptr ? *ToTestString(BaseType->GetName()) : TEXT("<none>")));
		}

		return JoinItems(Items);
	}

	inline void LogBuilderState(FAutomationTestBase& Test, const FString& Stage, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true, bool bIncludeDiagnosticCounters = true)
	{
		Test.AddInfo(FString::Printf(TEXT("[Builder][%s] %s | %s"), *Stage, *DescribeBuilderCounts(Builder, bIncludeDiagnosticCounters), *DescribeModuleCounts(Module)));
		if (bExpandBuilderDescriptions)
		{
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] classDecls: %s"), *Stage, *DescribeClassDeclarations(Builder)));
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] namedTypes: %s"), *Stage, *DescribeNamedTypeDeclarations(Builder)));
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] functionDescs: %s"), *Stage, *DescribeFunctionDescriptions(Builder)));
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] globalDescs: %s"), *Stage, *DescribeGlobalDescriptions(Builder)));
		}
		if (Module != nullptr)
		{
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] moduleTypes: %s"), *Stage, *DescribeModuleTypes(Module)));
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] moduleFunctions: %s"), *Stage, *DescribeModuleFunctions(Module)));
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] moduleGlobals: %s"), *Stage, *DescribeModuleGlobals(Module)));
		}
	}

	inline void LogBuilderSectionInput(FAutomationTestBase& Test, const FString& Stage, const char* SectionName, const char* Source)
	{
		Test.AddInfo(FString::Printf(
			TEXT("[Builder][%s] add section name=%s bytes=%d lines=%d"),
			*Stage,
			*ToTestString(SectionName),
			Source != nullptr ? static_cast<int32>(std::strlen(Source)) : 0,
			CountSourceLines(Source)));
	}

	inline void LogBuilderStageResult(FAutomationTestBase& Test, const FString& Stage, int Result, const asCBuilder& Builder, const asCModule* Module = nullptr, bool bExpandBuilderDescriptions = true)
	{
		Test.AddInfo(FString::Printf(TEXT("[Builder][%s] result=%d"), *Stage, Result));
		LogBuilderState(Test, Stage, Builder, Module, bExpandBuilderDescriptions);
	}

	inline void LogScriptExecutionResult(FAutomationTestBase& Test, const FString& Stage, const char* Declaration, int32 Result)
	{
		Test.AddInfo(FString::Printf(TEXT("[Builder][%s] executed %s => %d"), *Stage, *ToTestString(Declaration), Result));
	}

	inline void ReportBuilderFailureDiagnostics(FAutomationTestBase& Test, const FNativeTestEngine& Engine)
	{
		const FString Messages = Engine.GetMessagesText();
		if (!Messages.IsEmpty())
		{
			Test.AddInfo(Messages);
		}
	}

	inline bool AddBuilderSectionWithLog(FAutomationTestBase& Test, asCModule& Module, const char* SectionName, const char* Source, const FString& Stage)
	{
		LogBuilderSectionInput(Test, Stage, SectionName, Source);
		const bool bAdded = AddBuilderSection(Module, SectionName, Source);
		Test.AddInfo(FString::Printf(TEXT("[Builder][%s] AddScriptSection result=%s"), *Stage, BoolText(bAdded)));
		if (Module.builder != nullptr)
		{
			LogBuilderState(Test, Stage, *Module.builder, &Module, true, false);
		}
		else
		{
			Test.AddInfo(FString::Printf(TEXT("[Builder][%s] %s"), *Stage, *DescribeModuleCounts(&Module)));
		}
		return bAdded;
	}

	inline bool RunBuilderStage(FAutomationTestBase& Test, asCBuilder& Builder, const FString& Stage, int (asCBuilder::*StageMethod)(), const asCModule* Module = nullptr)
	{
		LogBuilderState(Test, FString::Printf(TEXT("%s.before"), *Stage), Builder, Module, true, false);
		const int Result = (Builder.*StageMethod)();
		LogBuilderStageResult(Test, FString::Printf(TEXT("%s.after"), *Stage), Result, Builder, Module);
		return Result == asSUCCESS;
	}

	inline bool RunBuilderPipelineThroughLayout(FAutomationTestBase& Test, asCBuilder& Builder, const asCModule* Module = nullptr)
	{
		if (!RunBuilderStage(Test, Builder, TEXT("BuildParallelParseScripts"), &asCBuilder::BuildParallelParseScripts, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Test, Builder, TEXT("BuildGenerateTypes"), &asCBuilder::BuildGenerateTypes, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Test, Builder, TEXT("BuildGenerateFunctions"), &asCBuilder::BuildGenerateFunctions, Module))
		{
			return false;
		}
		if (!RunBuilderStage(Test, Builder, TEXT("BuildLayoutClasses"), &asCBuilder::BuildLayoutClasses, Module))
		{
			return false;
		}
		LogBuilderState(Test, TEXT("BuildAllocateGlobalVariables.before"), Builder, Module, true, false);
		Builder.BuildAllocateGlobalVariables();
		LogBuilderState(Test, TEXT("BuildAllocateGlobalVariables.after"), Builder, Module);
		return RunBuilderStage(Test, Builder, TEXT("BuildLayoutFunctions"), &asCBuilder::BuildLayoutFunctions, Module);
	}

	inline sClassDeclaration* FindClassDeclarationByName(asCBuilder& Builder, const char* Name)
	{
		for (asUINT Index = 0; Index < Builder.classDeclarations.GetLength(); ++Index)
		{
			sClassDeclaration* Declaration = Builder.classDeclarations[Index];
			if (Declaration != nullptr && Declaration->name == Name)
			{
				return Declaration;
			}
		}

		return nullptr;
	}

	inline sClassDeclaration* FindNamedTypeDeclarationByName(asCBuilder& Builder, const char* Name)
	{
		for (asUINT Index = 0; Index < Builder.namedTypeDeclarations.GetLength(); ++Index)
		{
			sClassDeclaration* Declaration = Builder.namedTypeDeclarations[Index];
			if (Declaration != nullptr && Declaration->name == Name)
			{
				return Declaration;
			}
		}

		return nullptr;
	}

	inline sFunctionDescription* FindFunctionDescriptionByName(asCBuilder& Builder, const char* Name, const char* ObjectTypeName = nullptr)
	{
		for (asUINT Index = 0; Index < Builder.functions.GetLength(); ++Index)
		{
			sFunctionDescription* Function = Builder.functions[Index];
			if (Function == nullptr || Function->name != Name)
			{
				continue;
			}

			const bool bObjectMatches = ObjectTypeName == nullptr
				? Function->objType == nullptr
				: Function->objType != nullptr && Function->objType->name == ObjectTypeName;
			if (bObjectMatches)
			{
				return Function;
			}
		}

		return nullptr;
	}

	inline sGlobalVariableDescription* FindGlobalVariableDescriptionByName(asCBuilder& Builder, const char* Name)
	{
		for (asUINT Index = 0; Index < Builder.globVariableList.GetLength(); ++Index)
		{
			sGlobalVariableDescription* Global = Builder.globVariableList[Index];
			if (Global != nullptr && Global->name == Name)
			{
				return Global;
			}
		}

		return nullptr;
	}

	inline asIScriptFunction* FindModuleFunctionByNameAndParamCount(asIScriptModule* Module, const char* Name, int32 ParamCount, const char* Namespace = "")
	{
		if (Module == nullptr || Name == nullptr || Namespace == nullptr)
		{
			return nullptr;
		}

		for (asUINT Index = 0; Index < Module->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = Module->GetFunctionByIndex(Index);
			if (Function != nullptr &&
				FCStringAnsi::Strcmp(Function->GetName(), Name) == 0 &&
				FCStringAnsi::Strcmp(Function->GetNamespace(), Namespace) == 0 &&
				static_cast<int32>(Function->GetParamCount()) == ParamCount)
			{
				return Function;
			}
		}

		return nullptr;
	}

	inline int32 FindGlobalVarIndexByNameAndNamespace(asIScriptModule* Module, const char* Name, const char* Namespace = "")
	{
		if (Module == nullptr || Name == nullptr || Namespace == nullptr)
		{
			return INDEX_NONE;
		}

		for (asUINT Index = 0; Index < Module->GetGlobalVarCount(); ++Index)
		{
			const char* GlobalName = nullptr;
			const char* GlobalNamespace = nullptr;
			if (Module->GetGlobalVar(Index, &GlobalName, &GlobalNamespace, nullptr, nullptr) >= 0 &&
				GlobalName != nullptr &&
				GlobalNamespace != nullptr &&
				FCStringAnsi::Strcmp(GlobalName, Name) == 0 &&
				FCStringAnsi::Strcmp(GlobalNamespace, Namespace) == 0)
			{
				return static_cast<int32>(Index);
			}
		}

		return INDEX_NONE;
	}

	inline asIScriptFunction* FindTypeMethodByNameAndParamCount(asITypeInfo* TypeInfo, const char* Name, int32 ParamCount)
	{
		if (TypeInfo == nullptr || Name == nullptr)
		{
			return nullptr;
		}

		for (asUINT Index = 0; Index < TypeInfo->GetMethodCount(); ++Index)
		{
			asIScriptFunction* Function = TypeInfo->GetMethodByIndex(Index);
			if (Function != nullptr &&
				FCStringAnsi::Strcmp(Function->GetName(), Name) == 0 &&
				static_cast<int32>(Function->GetParamCount()) == ParamCount)
			{
				return Function;
			}
		}

		return nullptr;
	}

	inline bool HasBytecode(asIScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		return Function->GetByteCode(&BytecodeLength) != nullptr && BytecodeLength > 0;
	}

	inline int32 CountGlobalFunctionDescriptions(asCBuilder& Builder, const char* Name)
	{
		int32 Count = 0;
		for (asUINT Index = 0; Index < Builder.functions.GetLength(); ++Index)
		{
			sFunctionDescription* Function = Builder.functions[Index];
			if (Function != nullptr && Function->objType == nullptr && Function->name == Name)
			{
				++Count;
			}
		}

		return Count;
	}

	struct FExpectedBuilderDiagnostic
	{
		asEMsgType Type = asMSGTYPE_ERROR;
		FString Section;
		int32 Row = INDEX_NONE;
		int32 Column = INDEX_NONE;
		FString MessageContains;

		static FExpectedBuilderDiagnostic Error(const TCHAR* InSection, int32 InRow, const TCHAR* InMessageContains, int32 InColumn = INDEX_NONE)
		{
			FExpectedBuilderDiagnostic Diagnostic;
			Diagnostic.Type = asMSGTYPE_ERROR;
			Diagnostic.Section = InSection != nullptr ? InSection : TEXT("");
			Diagnostic.Row = InRow;
			Diagnostic.Column = InColumn;
			Diagnostic.MessageContains = InMessageContains != nullptr ? InMessageContains : TEXT("");
			return Diagnostic;
		}

		static FExpectedBuilderDiagnostic Warning(const TCHAR* InSection, int32 InRow, const TCHAR* InMessageContains, int32 InColumn = INDEX_NONE)
		{
			FExpectedBuilderDiagnostic Diagnostic;
			Diagnostic.Type = asMSGTYPE_WARNING;
			Diagnostic.Section = InSection != nullptr ? InSection : TEXT("");
			Diagnostic.Row = InRow;
			Diagnostic.Column = InColumn;
			Diagnostic.MessageContains = InMessageContains != nullptr ? InMessageContains : TEXT("");
			return Diagnostic;
		}
	};

	inline bool MatchesDiagnostic(const FNativeMessageEntry& Actual, const FExpectedBuilderDiagnostic& Expected)
	{
		if (Actual.Type != Expected.Type)
		{
			return false;
		}
		if (!Expected.Section.IsEmpty() && Actual.Section != Expected.Section)
		{
			return false;
		}
		if (Expected.Row != INDEX_NONE && Actual.Row != Expected.Row)
		{
			return false;
		}
		if (Expected.Column != INDEX_NONE && Actual.Column != Expected.Column)
		{
			return false;
		}
		if (!Expected.MessageContains.IsEmpty() && !Actual.Message.Contains(Expected.MessageContains))
		{
			return false;
		}
		return true;
	}

	inline bool AssertBuilderDiagnostic(FAutomationTestBase& Test, const FNativeMessageCollector& Messages, const FExpectedBuilderDiagnostic& Expected, const TCHAR* Context)
	{
		for (const FNativeMessageEntry& Actual : Messages.Entries)
		{
			if (MatchesDiagnostic(Actual, Expected))
			{
				Test.AddInfo(FString::Printf(
					TEXT("[Builder][Diagnostic][PASS] %s matched %s:%d:%d %s"),
					Context != nullptr ? Context : TEXT("<context>"),
					*Actual.Section,
					Actual.Row,
					Actual.Column,
					*Actual.Message));
				return true;
			}
		}

		Test.AddInfo(FString::Printf(
			TEXT("[Builder][Diagnostic][FAIL] %s expected type=%d section=%s row=%d col=%d contains=%s"),
			Context != nullptr ? Context : TEXT("<context>"),
			static_cast<int32>(Expected.Type),
			*Expected.Section,
			Expected.Row,
			Expected.Column,
			*Expected.MessageContains));
		Test.AddInfo(CollectMessages(Messages));
		return false;
	}
}
