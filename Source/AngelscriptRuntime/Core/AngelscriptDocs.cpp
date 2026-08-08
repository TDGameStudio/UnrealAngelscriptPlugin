#include "AngelscriptDocs.h"

#include "Containers/Set.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "UObject/UnrealType.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptInclude.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptengine.h"
//#include "as_scriptfunction.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	FAngelscriptDocumentationState LegacyDocumentationState;

	FAngelscriptDocumentationState& ResolveDocumentationState()
	{
		if (FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine())
		{
			if (FAngelscriptDocumentationState* State = Engine->GetDocumentationState())
			{
				return *State;
			}
		}
		return LegacyDocumentationState;
	}

	FAngelscriptDocumentationState& ResolveDocumentationState(const FAngelscriptEngine& Engine)
	{
		FAngelscriptDocumentationState* State = Engine.GetDocumentationState();
		check(State != nullptr);
		return *State;
	}
}

void FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptEngine& Engine, int FunctionId, FStringView Documentation, FStringView Category, UFunction* Function)
{
	FPassedDoc Doc;
	Doc.Tooltip = Documentation;
	Doc.Category = Category;
	Doc.Function = Function;

	ResolveDocumentationState(Engine).UnrealDocumentation.Add(FunctionId, MoveTemp(Doc));
}

void FAngelscriptDocs::AddUnrealDocumentation(int FunctionId, FStringView Documentation, FStringView Category, UFunction* Function)
{
	FPassedDoc Doc;
	Doc.Tooltip = Documentation;
	Doc.Category = Category;
	Doc.Function = Function;

	ResolveDocumentationState().UnrealDocumentation.Add(FunctionId, MoveTemp(Doc));
}

void FAngelscriptDocs::AddUnrealDocumentationForType(FAngelscriptEngine& Engine, int TypeId, FStringView Documentation)
{
	ResolveDocumentationState(Engine).UnrealTypeDocumentation.Add(TypeId, FString(Documentation));
}

void FAngelscriptDocs::AddUnrealDocumentationForType(int TypeId, FStringView Documentation)
{
	ResolveDocumentationState().UnrealTypeDocumentation.Add(TypeId, FString(Documentation));
}

void FAngelscriptDocs::AddUnrealDocumentationForProperty(
	FAngelscriptEngine& Engine,
	int TypeId,
	int PropertyOffset,
	FStringView Documentation)
{
	ResolveDocumentationState(Engine).UnrealPropertyDocumentation.Add(
		TPair<int, int>(TypeId, PropertyOffset),
		FString(Documentation));
}

void FAngelscriptDocs::AddUnrealDocumentationForProperty(int TypeId, int PropertyOffset, FStringView Documentation)
{
	ResolveDocumentationState().UnrealPropertyDocumentation.Add(TPair<int,int>(TypeId, PropertyOffset), FString(Documentation));
}

void FAngelscriptDocs::AddDocumentationForGlobalVariable(FAngelscriptEngine& Engine, int GlobalVariableId, FStringView Documentation)
{
	ResolveDocumentationState(Engine).GlobalVariableDocumentation.Add(GlobalVariableId, FString(Documentation));
}

void FAngelscriptDocs::AddDocumentationForGlobalVariable(int GlobalVariableId, FStringView Documentation)
{
	ResolveDocumentationState().GlobalVariableDocumentation.Add(GlobalVariableId, FString(Documentation));
}

const FString& FAngelscriptDocs::GetUnrealDocumentationForType(int TypeId)
{
	FString* Found = ResolveDocumentationState().UnrealTypeDocumentation.Find(TypeId);
	if (Found == nullptr)
	{
		static const FString EmptyString;
		return EmptyString;
	}
	else
	{
		return *Found;
	}
}

const FString& FAngelscriptDocs::GetUnrealDocumentationForProperty(int TypeId, int PropertyOffset)
{
	FString* Found = ResolveDocumentationState().UnrealPropertyDocumentation.Find(TPair<int,int>(TypeId, PropertyOffset));
	if (Found == nullptr)
	{
		static const FString EmptyString;
		return EmptyString;
	}
	else
	{
		return *Found;
	}
}

const FString& FAngelscriptDocs::GetDocumentationForGlobalVariable(int GlobalVariableId)
{
	FString* Found = ResolveDocumentationState().GlobalVariableDocumentation.Find(GlobalVariableId);
	if (Found == nullptr)
	{
		static const FString EmptyString;
		return EmptyString;
	}
	else
	{
		return *Found;
	}
}

int32 FAngelscriptDocs::GetUnrealDocumentationCount()
{
	return ResolveDocumentationState().UnrealDocumentation.Num();
}

int32 FAngelscriptDocs::GetUnrealTypeDocumentationCount()
{
	return ResolveDocumentationState().UnrealTypeDocumentation.Num();
}

int32 FAngelscriptDocs::GetGlobalVariableDocumentationCount()
{
	return ResolveDocumentationState().GlobalVariableDocumentation.Num();
}

int32 FAngelscriptDocs::GetUnrealPropertyDocumentationCount()
{
	return ResolveDocumentationState().UnrealPropertyDocumentation.Num();
}

void FAngelscriptDocs::ResetAllDocumentation()
{
	FAngelscriptDocumentationState& State = ResolveDocumentationState();
	State.UnrealDocumentation.Empty();
	State.UnrealTypeDocumentation.Empty();
	State.GlobalVariableDocumentation.Empty();
	State.UnrealPropertyDocumentation.Empty();
}

UFunction* FAngelscriptDocs::LookupAngelscriptFunction(int FunctionId)
{
	FPassedDoc* Found = ResolveDocumentationState().UnrealDocumentation.Find(FunctionId);
	if (Found == nullptr)
		return nullptr;
	else
		return Found->Function;
}

const FPassedDoc& FAngelscriptDocs::GetFullUnrealDocumentation(int FunctionId)
{
	FPassedDoc* Found = ResolveDocumentationState().UnrealDocumentation.Find(FunctionId);
	if (Found == nullptr)
	{
		static const FPassedDoc EmptyDoc;
		return EmptyDoc;
	}
	else
	{
		return *Found;
	}
}

const FString& FAngelscriptDocs::GetUnrealDocumentation(const FAngelscriptEngine& Engine, int FunctionId)
{
	FPassedDoc* Found = ResolveDocumentationState(Engine).UnrealDocumentation.Find(FunctionId);
	if (Found == nullptr)
	{
		static const FString EmptyString;
		return EmptyString;
	}
	return Found->Tooltip;
}

const FString& FAngelscriptDocs::GetUnrealDocumentation(int FunctionId)
{
	FPassedDoc* Found = ResolveDocumentationState().UnrealDocumentation.Find(FunctionId);
	if (Found == nullptr)
	{
		static const FString EmptyString;
		return EmptyString;
	}
	else
	{
		return Found->Tooltip;
	}
}

struct FDocFunc
{
	FString Name;
	FString Declaration;
	FString Documentation;
	FString Category;
	bool bStatic = false;

	bool operator<(const FDocFunc& Other) const
	{
		if (Category.Len() == 0 && Other.Category.Len() == 0)
		{
			if (bStatic)
				return false;
			else if (Other.bStatic)
				return true;
			return false;
		}
		if (Category.Len() == 0)
			return false;
		if (Other.Category.Len() == 0)
			return true;
		return Category < Other.Category;
	}
};

struct FDocProperty
{
	FString Name;
	FString Declaration;
	FString Documentation;
	FString Category;
	bool bStatic = false;

	bool operator<(const FDocProperty& Other) const
	{
		if (Category.Len() == 0 && Other.Category.Len() == 0)
		{
			if (bStatic)
				return false;
			else if (Other.bStatic)
				return true;
			return false;
		}
		if (Category.Len() == 0)
			return false;
		if (Other.Category.Len() == 0)
			return true;
		return Category < Other.Category;
	}
};

static FString GetFunctionTooltip(const FString& InJavadoc)
{
	FString Tooltip = InJavadoc;
	if (!Tooltip.IsEmpty())
	{
		// Strip off the doxygen nastiness
		static const FString DoxygenParam(TEXT("@param"));
		static const FString DoxygenReturn(TEXT("@return"));
		static const FString DoxygenSee(TEXT("@see"));
		static const FString TooltipSee(TEXT("See:"));
		static const FString DoxygenNote(TEXT("@note"));
		static const FString TooltipNote(TEXT("Note:"));

		Tooltip.Split(DoxygenParam, &Tooltip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		Tooltip.Split(DoxygenReturn, &Tooltip, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		Tooltip.ReplaceInline(*DoxygenSee, *TooltipSee);
		Tooltip.ReplaceInline(*DoxygenNote, *TooltipNote);

		Tooltip.TrimStartAndEndInline();

		return Tooltip;
	}
	else
	{
		return TEXT("");
	}
}

FString GetParamTooltip(const FString& FunctionToolTipText, const FString& ParamName)
{
	FString TagStr = TEXT("@param");

	bool bIsReturn = false;
	if (ParamName.Len() == 0)
	{
		TagStr = TEXT("@return");
		bIsReturn = true;
	}

	FString OutTooltip;
	
	int32 CurStrPos = INDEX_NONE;
	int32 FullToolTipLen = FunctionToolTipText.Len();
	// parse the full function tooltip text, looking for tag lines
	do 
	{
		CurStrPos = FunctionToolTipText.Find(TagStr, ESearchCase::IgnoreCase, ESearchDir::FromStart, CurStrPos);
		if (CurStrPos == INDEX_NONE) // if the tag wasn't found
		{
			break;
		}

		// advance past the tag
		CurStrPos += TagStr.Len();

		// handle people having done @returns instead of @return
		if (bIsReturn && CurStrPos < FullToolTipLen && FunctionToolTipText[CurStrPos] == TEXT('s'))
		{
			++CurStrPos;
		}

		// advance past whitespace
		while(CurStrPos < FullToolTipLen && FChar::IsWhitespace(FunctionToolTipText[CurStrPos]))
		{
			++CurStrPos;
		}

		// if this is a parameter pin
		if (!ParamName.IsEmpty())
		{
			FString TagParamName;

			// copy the parameter name
			while (CurStrPos < FullToolTipLen && !FChar::IsWhitespace(FunctionToolTipText[CurStrPos]))
			{
				TagParamName.AppendChar(FunctionToolTipText[CurStrPos++]);
			}

			// if this @param tag doesn't match the param we're looking for
			if (TagParamName != ParamName)
			{
				continue;
			}
		}

		// advance past whitespace (get to the meat of the comment)
		// since many doxygen style @param use the format "@param <param name> - <comment>" we also strip - if it is before we get to any other non-whitespace
		while(CurStrPos < FullToolTipLen && (FChar::IsWhitespace(FunctionToolTipText[CurStrPos]) || FunctionToolTipText[CurStrPos] == '-'))
		{
			++CurStrPos;
		}


		FString ParamDesc;
		// collect the param/return-val description
		while (CurStrPos < FullToolTipLen && FunctionToolTipText[CurStrPos] != TEXT('@'))
		{
			// advance past newline
			while(CurStrPos < FullToolTipLen && FChar::IsLinebreak(FunctionToolTipText[CurStrPos]))
			{
				++CurStrPos;

				// advance past whitespace at the start of a new line
				while(CurStrPos < FullToolTipLen && FChar::IsWhitespace(FunctionToolTipText[CurStrPos]))
				{
					++CurStrPos;
				}

				// replace the newline with a single space
				if(CurStrPos < FullToolTipLen && !FChar::IsLinebreak(FunctionToolTipText[CurStrPos]))
				{
					ParamDesc.AppendChar(TEXT(' '));
				}
			}

			if (CurStrPos < FullToolTipLen && FunctionToolTipText[CurStrPos] != TEXT('@'))
			{
				ParamDesc.AppendChar(FunctionToolTipText[CurStrPos++]);
			}
		}

		// trim any trailing whitespace from the descriptive text
		ParamDesc.TrimEndInline();

		// if we came up with a valid description for the param/return-val
		if (!ParamDesc.IsEmpty())
		{
			OutTooltip += ParamDesc;
			break; // we found a match, so there's no need to continue
		}

	} while (CurStrPos < FullToolTipLen);

	return OutTooltip;
}

struct FDocClass
{
	FString ClassName;
	FString SuperClass;
	FString Documentation;
	TArray<FDocFunc> Functions;
	TArray<FDocProperty> Properties;
	TArray<FString> Enums;

	void AddFunction(asIScriptFunction* ScriptFunction)
	{
		FDocFunc Func;
		Func.bStatic = ScriptFunction->GetObjectType() == nullptr;
		Func.Name = ANSI_TO_TCHAR(ScriptFunction->GetName());

		Func.Declaration = ANSI_TO_TCHAR(ScriptFunction->GetDeclaration(Func.bStatic, Func.bStatic, true, true));
		Func.Declaration = Func.Declaration.Replace(TEXT("&inout"), TEXT("&"));
		Func.Declaration = Func.Declaration.Replace(TEXT("@"), TEXT(""));

		auto& UnrealDoc = FAngelscriptDocs::GetFullUnrealDocumentation(ScriptFunction->GetId());

		// Reduce documentation to just the tooltip
		Func.Documentation = GetFunctionTooltip(UnrealDoc.Tooltip);
		Func.Category = UnrealDoc.Category;

		// Extract documentation for parameters
		bool bDocumentedParams = false;
		int32 ParamCount = ScriptFunction->GetParamCount();
		for (int32 i = 0; i < ParamCount; ++i)
		{
			const char* RawName;
			ScriptFunction->GetParam(i, nullptr, nullptr, &RawName);

			FString ParamName = ANSI_TO_TCHAR(RawName);
			if (ParamName.Len() == 0)
				continue;

			FString ParamTooltip = GetParamTooltip(UnrealDoc.Tooltip, ParamName);
			if (ParamTooltip.Len() == 0)
				continue;

			if (!bDocumentedParams)
			{
				Func.Documentation += TEXT("\n\nParameters:");
				bDocumentedParams = true;
			}

			ParamTooltip = ParamTooltip.Replace(TEXT("\n"), TEXT(""));
			Func.Documentation += FString::Printf(TEXT("\n    %s - %s"),
				*ParamName, *ParamTooltip);
		}

		// Extract documentation for return value
		FString ReturnTooltip;
		ReturnTooltip = GetParamTooltip(UnrealDoc.Tooltip, ReturnTooltip);;

		if (ReturnTooltip.Len() != 0)
			Func.Documentation += FString::Printf(TEXT("\n\nReturns:\n    %s"), *ReturnTooltip);


		Functions.Add(Func);
	}
};

void FAngelscriptDocs::DumpDocumentation(asIScriptEngine* Engine)
{
#if WITH_EDITOR
	TMap<FString, FDocClass> Classes;

	auto* ScriptEngine = FAngelscriptEngine::Get().Engine;

	// ** Gather documentation into intermediate data

	auto GetDecl = [&](int TypeId, asDWORD* Flags = nullptr) -> FString
	{
		if (TypeId == -1)
			return TEXT("?");

		const char* DeclRaw = ScriptEngine->GetTypeDeclaration(TypeId);
		FString Decl = ANSI_TO_TCHAR(DeclRaw);
		if (Decl.EndsWith(TEXT("@")))
			Decl = Decl.LeftChop(1);
		if (Flags != nullptr)
		{
			if((*Flags & asTM_INOUTREF) != 0 && (*Flags & asTM_CONST) == 0)
				Decl += TEXT("&");
		}
		return Decl;
	};

	TMap<FString, FString> GetAccessors;
	TMap<FString, FString> SetAccessors;

	auto AddFunction = [&](FDocClass& Class, asIScriptFunction* ScriptFunction, bool bRecordAccessors)
	{
		int32 ArgCount = ScriptFunction->GetParamCount();
		int32 HiddenParam = ((asCScriptFunction*)ScriptFunction)->hiddenArgumentIndex;

		asDWORD Flags;
		FString ReturnType = GetDecl(ScriptFunction->GetReturnTypeId(&Flags), &Flags);

		FString Name = ANSI_TO_TCHAR(ScriptFunction->GetName());
		if (Name == TEXT("opImplCast"))
			return;
		if (Name.StartsWith(TEXT("__")))
			return;

		if (bRecordAccessors)
		{
			if ((Name.StartsWith(TEXT("Get")) && Name.Len() > 3 && (FChar::IsUpper(Name[3]) || (Name.Len() > 4 && Name[3] == 'b' && FChar::IsUpper(Name[4])))
				&& (ArgCount == 0 || (ArgCount == 1 && HiddenParam == 0 && false))) && ReturnType != TEXT("void"))
			{
				GetAccessors.Add(Name.RightChop(3), ReturnType);
			}
			else if (Name.StartsWith(TEXT("Set")) && Name.Len() > 3 && (FChar::IsUpper(Name[3]) || (Name.Len() > 4 && Name[3] == 'b' && FChar::IsUpper(Name[4])))
				&& (ArgCount == 1 || (ArgCount == 2 && HiddenParam == 0 && false)))
			{
				const char* ParamNameRaw;
				asDWORD ParamFlags;
				int ParamType;

				ScriptFunction->GetParam(ArgCount == 1 ? 0 : 1, &ParamType, &ParamFlags, &ParamNameRaw);

				SetAccessors.Add(Name.RightChop(3), GetDecl(ParamType, &ParamFlags));
			}
		}

		Class.AddFunction(ScriptFunction);
	};

	auto ResolveAccessors = [&](FDocClass& Class, UClass* UnrealClass = nullptr, const FString* StaticName = nullptr)
	{
		for (auto& Elem : GetAccessors)
		{
			FString Decl = Elem.Value;
			if (!SetAccessors.Contains(Elem.Key) && !Decl.StartsWith(TEXT("const ")))
				Decl = TEXT("const ") + Decl;

			FDocProperty Prop;
			Prop.Name = Elem.Key;
			Prop.bStatic = StaticName != nullptr;

			if(Prop.bStatic)
				Prop.Declaration = Decl + TEXT(" ") + *StaticName + TEXT("::") + Prop.Name;
			else
				Prop.Declaration = Decl + TEXT(" ") + Prop.Name;

			if (UnrealClass != nullptr)
			{
				FProperty* PropDesc = UnrealClass->FindPropertyByName(*Prop.Name);
				if (PropDesc != nullptr)
				{
					Prop.Documentation = PropDesc->GetMetaData("ToolTip");
					Prop.Category = PropDesc->GetMetaData("Category");
				}
				else
				{
					UFunction* FuncDesc = UnrealClass->FindFunctionByName(*(TEXT("Get") + Prop.Name));
					if (FuncDesc != nullptr)
					{
						Prop.Documentation = GetFunctionTooltip(FuncDesc->GetMetaData("ToolTip"));
						Prop.Category = FuncDesc->GetMetaData("Category");
					}
				}
			}

			Class.Properties.Add(Prop);
		}

		GetAccessors.Empty();
		SetAccessors.Empty();
	};

	TSet<asITypeInfo*> ProcessedScriptTypes;

	auto ProcessScriptType = [&](asITypeInfo* ScriptType)
	{
		if (ScriptType == nullptr || ProcessedScriptTypes.Contains(ScriptType))
			return;

		ProcessedScriptTypes.Add(ScriptType);
		FString TypeName = ANSI_TO_TCHAR(ScriptType->GetName());

		if (TypeName.StartsWith(TEXT("_")))
			return;

		FDocClass& ClassDoc = Classes.FindOrAdd(TypeName);
		ClassDoc.ClassName = TypeName;

		auto ASType = FAngelscriptType::GetByAngelscriptTypeName(TypeName);
		UClass* Class = nullptr;
		if (ASType.IsValid())
			Class = ASType->GetClass(FAngelscriptTypeUsage::DefaultUsage);

		if (Class != nullptr)
		{
			ClassDoc.Documentation = Class->GetMetaData("ToolTip");

			auto* SuperClass = Class->GetSuperClass();
			if (SuperClass != nullptr)
				ClassDoc.SuperClass = FAngelscriptType::GetBoundClassName(SuperClass);
		}

		int32 PropertyCount = ScriptType->GetPropertyCount();
		for (int32 PropertyIndex = 0; PropertyIndex < PropertyCount; ++PropertyIndex)
		{
			const char* Name;
			int TypeId;
			ScriptType->GetProperty(PropertyIndex, &Name, &TypeId);

			FDocProperty Prop;
			Prop.Name = ANSI_TO_TCHAR(Name);
			Prop.Declaration = GetDecl(TypeId) + TEXT(" ") + Prop.Name;
			Prop.bStatic = false;

			if (Class != nullptr)
			{
				FProperty* PropDesc = Class->FindPropertyByName(*Prop.Name);
				if (PropDesc != nullptr)
				{
					if (PropDesc->GetOwnerClass() != Class)
						continue;

					Prop.Documentation = PropDesc->GetMetaData("ToolTip");
					Prop.Category = PropDesc->GetMetaData("Category");
				}
				else
				{
					continue;
				}
			}
			else
			{
				continue;
			}

			ClassDoc.Properties.Add(Prop);
		}

		int32 MethodCount = ScriptType->GetMethodCount();
		for (int32 MethodIndex = 0; MethodIndex < MethodCount; ++MethodIndex)
		{
			auto* ScriptFunction = ScriptType->GetMethodByIndex(MethodIndex);
			if (ScriptFunction->GetObjectType() != ScriptType)
				continue;

			AddFunction(ClassDoc, ScriptFunction, true);
		}

		ResolveAccessors(ClassDoc, Class);
	};

	int32 TypeCount = ScriptEngine->GetObjectTypeCount();
	for (int32 TypeIndex = 0; TypeIndex < TypeCount; ++TypeIndex)
	{
		ProcessScriptType(ScriptEngine->GetObjectTypeByIndex(TypeIndex));
	}

	int32 ModuleCount = ScriptEngine->GetModuleCount();
	for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
	{
		asIScriptModule* ScriptModule = ScriptEngine->GetModuleByIndex(ModuleIndex);
		if (ScriptModule == nullptr)
			continue;

		int32 ModuleTypeCount = ScriptModule->GetObjectTypeCount();
		for (int32 TypeIndex = 0; TypeIndex < ModuleTypeCount; ++TypeIndex)
		{
			ProcessScriptType(ScriptModule->GetObjectTypeByIndex(TypeIndex));
		}
	}

	struct FNS
	{
		TArray<asIScriptFunction*> Functions;
		TArray<int32> Variables;
		TArray<FString> Enums;
	};
	TMap<FString, FNS> NSFunctions;

	int32 GlobalFuncCount = ScriptEngine->GetGlobalFunctionCount();
	for (int32 GlobalFuncIndex = 0; GlobalFuncIndex < GlobalFuncCount; ++GlobalFuncIndex)
	{
		asIScriptFunction* Func = ScriptEngine->GetGlobalFunctionByIndex(GlobalFuncIndex);
		NSFunctions.FindOrAdd(ANSI_TO_TCHAR(Func->GetNamespace())).Functions.Add(Func);
	}

	int32 GlobalVarCount = ScriptEngine->GetGlobalPropertyCount();
	for (int32 GlobalVarIndex = 0; GlobalVarIndex < GlobalVarCount; ++GlobalVarIndex)
	{
		const char* NS;
		ScriptEngine->GetGlobalPropertyByIndex(GlobalVarIndex, nullptr, &NS);

		NSFunctions.FindOrAdd(ANSI_TO_TCHAR(NS)).Variables.Add(GlobalVarIndex);
	}

	int32 EnumCount = ScriptEngine->GetEnumCount();
	for (int32 EnumIndex = 0; EnumIndex < EnumCount; ++EnumIndex)
	{
		auto* EnumType = ScriptEngine->GetEnumByIndex(EnumIndex);
		auto& NS = NSFunctions.FindOrAdd(ANSI_TO_TCHAR(EnumType->GetName()));

		for (int32 i = 0, Count = EnumType->GetEnumValueCount(); i < Count; ++i)
		{
			int32 Value;
			const char* ValueName = EnumType->GetEnumValueByIndex(i, &Value);
			NS.Enums.Add(ANSI_TO_TCHAR(ValueName));
		}
	}

	for (auto& NS : NSFunctions)
	{
		FString NSName = NS.Key;
		if (NSName.Len() == 0)
			NSName = TEXT("Global");

		FDocClass& ClassDoc = Classes.FindOrAdd(NSName);
		ClassDoc.ClassName = NSName;

		for (asIScriptFunction* ScriptFunction : NS.Value.Functions)
		{
			AddFunction(ClassDoc, ScriptFunction, true);
		}

		for (int32 GlobalVarIndex : NS.Value.Variables)
		{
			const char* Name;
			int TypeId;
			bool isConst;
			ScriptEngine->GetGlobalPropertyByIndex(GlobalVarIndex, &Name, nullptr, &TypeId, &isConst);

			FString Decl = GetDecl(TypeId);
			if (isConst)
				Decl = TEXT("const ") + Decl;

			FDocProperty Prop;
			Prop.Name = ANSI_TO_TCHAR(Name);

			if(NS.Key.Len() == 0)
				Prop.Declaration = Decl + TEXT(" ") + Prop.Name;
			else
				Prop.Declaration = Decl + TEXT(" ") + NSName + TEXT("::") + Prop.Name;

			Prop.bStatic = true;

			ClassDoc.Properties.Add(Prop);
		}

		for (FString& EnumValue : NS.Value.Enums)
		{
			ClassDoc.Enums.Add(EnumValue);
		}

		ResolveAccessors(ClassDoc, nullptr, &NSName);
	}

	// ** Dump documentation into generated header files
	const FString GeneratedDocsDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Docs"), TEXT("angelscript"), TEXT("generated"));
	IFileManager::Get().MakeDirectory(*GeneratedDocsDir, true);

	for (auto It : Classes)
	{
		FDocClass& ClassDoc = It.Value;
		ClassDoc.Properties.Sort();
		ClassDoc.Functions.Sort();

		const FString Filename = FPaths::Combine(GeneratedDocsDir, ClassDoc.ClassName + TEXT(".hpp"));

		FString Content;
		FString CurCategory;

		Content += FString::Printf(TEXT("/* Class: %s \n %s */ \n class %s"),
			*ClassDoc.ClassName, *ClassDoc.Documentation, *ClassDoc.ClassName);
		if (ClassDoc.SuperClass.Len() != 0)
			Content += FString::Printf(TEXT(" : public %s"), *ClassDoc.SuperClass);
		Content += TEXT("\n{\npublic:");

		for (FDocProperty& PropDoc : ClassDoc.Properties)
		{
			const TCHAR* NewCategory;
			if (PropDoc.Category.Len() != 0)
				NewCategory = *PropDoc.Category;
			else if(PropDoc.bStatic)
				NewCategory = TEXT("Static Variables");
			else
				NewCategory = TEXT("Variables");

			if (CurCategory != NewCategory)
			{
				CurCategory = NewCategory;
				Content += FString::Printf(TEXT("\n// Group: %s\n"), NewCategory);
			}

			Content += FString::Printf(TEXT("\n/* Variable: %s \n %s */\n"), *PropDoc.Name, *PropDoc.Documentation);
			if (PropDoc.bStatic)
				Content += TEXT("static ");
			Content += FString::Printf(TEXT("%s;"), *PropDoc.Declaration);
		}

		CurCategory = TEXT("-");
		for (FDocFunc& FuncDoc : ClassDoc.Functions)
		{
			const TCHAR* NewCategory;
			if (FuncDoc.Category.Len() != 0)
				NewCategory = *FuncDoc.Category;
			else if(FuncDoc.bStatic)
				NewCategory = TEXT("Static Functions");
			else
				NewCategory = TEXT("Functions");

			if (CurCategory != NewCategory)
			{
				CurCategory = NewCategory;
				Content += FString::Printf(TEXT("\n// Group: %s\n"), NewCategory);
			}

			Content += FString::Printf(TEXT("\n/* Function: %s \n %s */\n"), *FuncDoc.Name, *FuncDoc.Documentation);
			if (FuncDoc.bStatic)
				Content += TEXT("static ");
			Content += FString::Printf(TEXT("%s {}"), *FuncDoc.Declaration);
		}

		Content += TEXT("\n}\n");

		if (ClassDoc.Enums.Num() != 0)
		{
			FString DocValues;
			FString DeclValues;

			for (FString& EnumValue : ClassDoc.Enums)
			{
				DocValues += FString::Printf(TEXT("\n    %s - Enum"), *EnumValue);
				DeclValues += FString::Printf(TEXT("\n%s,"), *EnumValue);
			}

			Content += FString::Printf(TEXT("/* Enum: %s \n %s */ \n enum %s { %s \n}"),
				*ClassDoc.ClassName, *DocValues, *ClassDoc.ClassName, *DeclValues);
		}

		FFileHelper::SaveStringToFile(Content, *Filename);
	}
#endif
}
