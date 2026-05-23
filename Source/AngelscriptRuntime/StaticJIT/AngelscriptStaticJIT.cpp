#include "AngelscriptStaticJIT.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/StaticJITDiagnostics.h"
#include "StaticJIT/StaticJITHeader.h"

#include "AngelscriptBytecodes.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
//#include "as_datatype.h"
//#include "as_scriptfunction.h"
//#include "as_module.h"
//#include "as_scriptengine.h"
//#include "as_objecttype.h"
#include "source/as_datatype.h"
#include "source/as_scriptfunction.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

void FJITDatabase::Clear()
{
	auto& Database = FJITDatabase::Get();
	Database.Functions.Empty();
	Database.FunctionLookups.Empty();
	Database.SystemFunctionPointerLookups.Empty();
	Database.GlobalVarLookups.Empty();
	Database.TypeInfoLookups.Empty();
	Database.PropertyOffsetLookups.Empty();

#if AS_JIT_VERIFY_PROPERTY_OFFSETS
	Database.VerifyPropertyOffsets.Empty();
	Database.VerifyTypeSizes.Empty();
	Database.VerifyTypeAlignments.Empty();
#endif
}

FJITDatabase& FJITDatabase::Get()
{
	static FJITDatabase Database;
	return Database;
}

#if AS_WITH_STATIC_JIT_DIAGNOSTICS
void FStaticJITDiagnosticEntryMarkers::MarkEntry(uint32 FunctionId)
{
	FStaticJITDiagnostics::MarkEntry(FunctionId);
}
#endif

asCString GetScriptClassName(asITypeInfo* Type)
{
	asCString Str = Type->GetName();

	int32 SubTypeCount = Type->GetSubTypeCount();
	if (SubTypeCount != 0)
	{
		Str += "<";
		for (int32 i = 0; i < SubTypeCount; ++i)
		{
			if (i != 0)
				Str += ",";
			Str += FAngelscriptEngine::Get().Engine->GetTypeDeclaration(Type->GetSubTypeId(i));
		}
		Str += ">";
	}
	return Str;
}

int FAngelscriptStaticJIT::CompileFunction(asIScriptFunction* ScriptFunction, asJITFunction* OutJITFunction)
{
#if AS_CAN_GENERATE_JIT
	// In generate mode, output the C++ that represents the script function
	if (bGenerateOutputCode)
	{
		FunctionsToGenerate.Add((asCScriptFunction*)ScriptFunction, FGenerateFunction());
		*OutJITFunction = nullptr;
		return 1;
	}
#endif

	check(false);
	return 0;
}

void FAngelscriptStaticJIT::ReleaseJITFunction(asJITFunction JITFunction)
{
	// Doesn't do anything, we can't exactly release C++ functions
}

#if AS_CAN_GENERATE_JIT
const FString STANDARD_HEADER =
TEXT("#include \"StaticJIT/StaticJITConfig.h\"\n")
#if UE_BUILD_DEBUG
TEXT("#if UE_BUILD_DEBUG\n")
#elif UE_BUILD_DEVELOPMENT
TEXT("#if UE_BUILD_DEVELOPMENT\n")
#elif UE_BUILD_TEST
TEXT("#if UE_BUILD_TEST\n")
#elif UE_BUILD_SHIPPING
TEXT("#if UE_BUILD_SHIPPING\n")
#endif
TEXT("#ifndef AS_SKIP_JITTED_CODE\n\n")
;

const FString STANDARD_INCLUDES =
TEXT("#include \"StaticJIT/StaticJITHeader.h\"\n")
;

const FString STANDARD_FOOTER =
TEXT("#endif\n")
TEXT("#endif\n")
;

// Hack because some of the properties in NoExportTypes.h
// are named differently from how they're named in the real C++ types. :sweeney:
static const TSet<FString> GTypesExcludedFromPropertyNativization =
{
	TEXT("/Script/CoreUObject.Matrix"),
};

FJITFile& FAngelscriptStaticJIT::GetFile(asIScriptModule* Module)
{
	if (!JITFiles.Contains(Module))
	{
		auto File = MakeShared<FJITFile>();
		if (Module != nullptr)
		{
			FString ModuleSymbol = GetUniqueModuleName(*File, Module);
			File->Filename = FString::Printf(TEXT("%s.as.jit.hpp"), *ModuleSymbol);
			File->JITPrefix = TEXT("AS_") + ModuleSymbol + TEXT("__");
			File->ModuleName = ANSI_TO_TCHAR(Module->GetName());
		}
		else
		{
			File->Filename = TEXT("__no_module__.as.jit.hpp");
			File->JITPrefix = TEXT("AS_NA__");
			File->ModuleName = TEXT("NO_MODULE");
		}

		JITFiles.Add(Module, File);
		return *File;
	}
	else
	{
		return *JITFiles[Module];
	}
}

FString FAngelscriptStaticJIT::GetUniqueFunctionName(FJITFile& File, asIScriptFunction* ScriptFunction, int32 EntryCount)
{
	FString BaseName;

	auto* ObjectType = ScriptFunction->GetObjectType();
	if (ObjectType != nullptr)
	{
		BaseName = TEXT("AS_");
		BaseName += ANSI_TO_TCHAR(ObjectType->GetName());
		BaseName += TEXT("__");
		BaseName += ANSI_TO_TCHAR(ScriptFunction->GetName());
	}
	else
	{
		BaseName = File.JITPrefix + ANSI_TO_TCHAR(ScriptFunction->GetName());
	}

	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));


	UsedUniqueNames.Add(Name);
	return Name;
}

FString FAngelscriptStaticJIT::GetUniqueLabelName(FJITFile& File, asIScriptFunction* ScriptFunction, FStringView LabelSuffix)
{
	FString BaseName;

	auto* ObjectType = ScriptFunction->GetObjectType();
	if (ObjectType != nullptr)
	{
		BaseName = TEXT("LABEL_");
		BaseName += ANSI_TO_TCHAR(ObjectType->GetName());
		BaseName += TEXT("__");
		BaseName += ANSI_TO_TCHAR(ScriptFunction->GetName());

		if (LabelSuffix.Len() != 0)
		{
			BaseName += TEXT("_");
			BaseName += LabelSuffix;
		}
	}
	else
	{
		BaseName = TEXT("LABEL_");
		BaseName += ANSI_TO_TCHAR(ScriptFunction->GetName());

		if (LabelSuffix.Len() != 0)
		{
			BaseName += TEXT("_");
			BaseName += LabelSuffix;
		}
	}

	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));

	UsedUniqueNames.Add(Name);
	return Name;
}

FString FAngelscriptStaticJIT::GetUniqueModuleName(FJITFile& File, asIScriptModule* ScriptModule)
{
	FString BaseName = ANSI_TO_TCHAR(ScriptModule->GetName());

	int32 DotIndex;
	if (BaseName.FindLastChar((TCHAR)'.', DotIndex))
		BaseName = BaseName.Mid(DotIndex + 1);

	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));

	UsedUniqueNames.Add(Name);
	return Name;
}

FString FAngelscriptStaticJIT::GetUniqueSymbolName(const FString& InBaseName)
{
	FString BaseName = InBaseName;
	SanitizeSymbolName(BaseName);

	FString Name;
	int Num = 0;
	do
	{
		Name = BaseName;
		if (Num != 0)
			Name += FString::Printf(TEXT("__%d"), Num);
		Num++;
	} while (UsedUniqueNames.Contains(Name));

	UsedUniqueNames.Add(Name);
	return Name;
}

void FStaticJITContext::AdvanceBC(int32 AdvanceAmount)
{
	BC += AdvanceAmount;
}

void FStaticJITContext::AdvanceBC()
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)BC;
	int32 InstrSize = asBCTypeSize[asBCInfo[Instr].type];
	AdvanceBC(InstrSize);
}

asDWORD* FStaticJITContext::GetNextBC(asDWORD* FromBC)
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)FromBC;
	int32 InstrSize = asBCTypeSize[asBCInfo[Instr].type];
	return FromBC + InstrSize;
}

asEBCInstr FStaticJITContext::GetInstr(asDWORD* FromBC)
{
	return (asEBCInstr)*(asBYTE*)FromBC;
}

asEBCInstr FStaticJITContext::GetInstr()
{
	return (asEBCInstr)*(asBYTE*)BC;
}

int32 FStaticJITContext::GetInstrSize()
{
	asEBCInstr Instr = (asEBCInstr)*(asBYTE*)BC;
	return asBCTypeSize[asBCInfo[Instr].type];
}

void FStaticJITContext::Line(const TCHAR* Content)
{
	FunctionContent.Add(FEntryContent{ Content, BC, 0, LocalStackOffset, EEntryType::Line });
}

void FStaticJITContext::Entry(EEntryType Type)
{
	FunctionContent.Add(FEntryContent{ TEXT(""), nullptr, 0, LocalStackOffset, Type });
}

int32 FStaticJITContext::GetBCOffset()
{
	return (int32)(BC - BC_EntryStart);
}

FString FStaticJITContext::AllocateLocalVariable(const ANSICHAR* Type)
{
	if (!bInLocalState)
	{
		Line("{");
		bInLocalState = true;
	}

	FString Name = FString::Printf(TEXT("LOCAL_%d"), LocalVariableCount++);
	Line("{0} {1};", Type, Name);
	return Name;
}

void FStaticJITContext::AddHeader(const FString& Header)
{
	File->Headers.Add(Header);
}

void FStaticJITContext::DebugLineNumber()
{
	if (!JIT->bEmitDebugMetadataInOutput)
	{
		return;
	}

	int32 LineNumber = ScriptFunction->GetLineNumber(
		(BC - BC_FunctionStart), nullptr
	) & 0xFFFFF;

	Line("SCRIPT_DEBUG_CALLSTACK_LINE({0});", LineNumber);
}

void FStaticJITContext::GenerateNewFunction(asIScriptFunction* InScriptFunction)
{
	// Init the state for this entry
	BC_EntryStart = BC;
	UsageFlags = 0;
	ScriptFunction = (asCScriptFunction*)InScriptFunction;

	// Generate the surrounding code for the new function
	check(FunctionHead.Len() == 0);
	check(FunctionFoot.Len() == 0);
	check(FunctionContent.Num() == 0);
	check(BC == BC_FunctionStart); // We no longer support sharded JIT functions

	// Populate the parameters with their offsets
	FString VMEntryOutArgs;
	FString VMEntryFooter;
	FString VMEntryAssignment;
	FString ParmsEntryOutArgs;
	FString ParmsEntryHeader;
	FString ParmsEntryFooter;
	FString ParmsEntryAssignment;
	FString CppEntryInArgs;
	FString FunctionReturnType = TEXT("void");

	bool bParmsEntryValid = true;

	// Generate the actual C++ code for the main function
	FunctionHead += FString::Printf(TEXT("// == Jit at BC %d ==\n"), (asDWORD)(BC - BC_FunctionStart));
	if (JIT->bEmitDebugMetadataInOutput)
	{
		FunctionHead += FString::Printf(TEXT("%s(\"%s\", %d);\n"),
			(ScriptFunction->objectType != nullptr && (ScriptFunction->objectType->GetFlags() & asOBJ_REF) != 0)
				? TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT")
				: TEXT("SCRIPT_DEBUG_CALLSTACK_FRAME"),
			ANSI_TO_TCHAR(ScriptFunction->GetDeclarationStr(true, true, false, true, false).AddressOf()),
			(ScriptFunction->GetLineNumber(0, nullptr) & 0xFFFFF));
	}
	FunctionHead += TEXT("SCRIPT_ASSUME_NO_EXCEPTION()\n");
#if AS_WITH_STATIC_JIT_DIAGNOSTICS
	if (JIT->bEmitDiagnosticEntryMarkersInOutput)
	{
		FunctionHead += FString::Printf(TEXT("FStaticJITDiagnosticEntryMarkers::MarkEntry(0x%xu);\n"), FunctionId);
	}
#endif

	// Create the local stack space
	if (LocalStackSize > 0)
		FunctionHead += FString::Printf(TEXT("alignas(8) asBYTE l_stack[%d];\n"), LocalStackSize * 4);

	FunctionHead += TEXT("asQWORD l_valueRegister = 0;\n");
	FunctionHead += TEXT("asBYTE l_byteRegister = 0;\n");
	FunctionHead += TEXT("asDWORD l_dwordRegister = 0;\n");
	FunctionHead += TEXT("float l_floatRegister = 0;\n");
	FunctionHead += TEXT("double l_doubleRegister = 0;\n");
	FunctionHead += TEXT("void* l_objectRegister = nullptr;\n");

	// Populate the object pointer if we have it
	if (ScriptFunction->objectType != nullptr)
	{
		FAngelscriptTypeUsage ObjectType = FAngelscriptTypeUsage::FromTypeId(ScriptFunction->objectType->GetTypeId());
		FString LiteralType;

		if (ObjectType.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			ObjectType.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
				LiteralType = CppForm.CppType;
			else if (CppForm.CppGenericType.Len() != 0)
				LiteralType = CppForm.CppGenericType;
		}

		if (LiteralType.Len() == 0)
			LiteralType = TEXT("void*");
		else if (!LiteralType.EndsWith(TEXT("*")))
			LiteralType += TEXT("*");

		CppEntryInArgs += FString::Printf(TEXT(", %s l_This"), *LiteralType);
		VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)l_fp"), *LiteralType);
		ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s)Object"), *LiteralType);

		LiteralObjectType = LiteralType;
		bHasThisPointer = true;
	}

	// Populate the parameters with their offsets
	ParmsEntryHeader += TEXT("\tSIZE_T ParmsOffset = 0;\n");

	for (int i = 0, Count = ScriptFunction->parameterTypes.GetLength(); i < Count; ++i)
	{
		int32 Offset = -ScriptFunction->parameterOffsets[i];
		auto DataType = ScriptFunction->parameterTypes[i];

		FString ParmsEntryParamOffsetVar = FString::Printf(TEXT("ParmOffset_%d_%s"), i, ANSI_TO_TCHAR(ScriptFunction->parameterNames[i].AddressOf()));

		FVariableAsLocal Param;
		Param.LocalName = ANSI_TO_TCHAR(ScriptFunction->parameterNames[i].AddressOf());
		Param.LocalName = TEXT("p_") + Param.LocalName;
		Param.bIsConstant = true;

		Param.Type = FAngelscriptTypeUsage::FromParam(ScriptFunction, i);

		bool bHasNativeCppForm = false;

		if (Param.Type.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			Param.Type.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
			{
				Param.LiteralType = CppForm.CppType;
				bHasNativeCppForm = true;
			}
			else if (CppForm.CppGenericType.Len() != 0)
			{
				Param.LiteralType = CppForm.CppGenericType;
			}
		}

		if (DataType.IsReference() && DataType.IsReferenceType())
		{
			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = TEXT("UObject*");

			Param.bIsPointer = true;
			Param.bIsReference = true;
			Param.RepresentedType = EVariableType::Pointer;

			CppEntryInArgs += FString::Printf(TEXT(", %s* %s"), *Param.LiteralType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s**)(l_fp + %d)"), *Param.LiteralType, -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + %s)"), *Param.LiteralType, *ParmsEntryParamOffsetVar);

			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(void*);\n"));
		}
		else if (DataType.IsReferenceType())
		{
			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = TEXT("UObject*");

			Param.bIsPointer = true;
			Param.RepresentedType = EVariableType::Pointer;

			CppEntryInArgs += FString::Printf(TEXT(", %s %s"), *Param.LiteralType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)(l_fp + %d)"), *Param.LiteralType, -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)(((SIZE_T)Parms) + %s)"), *Param.LiteralType, *ParmsEntryParamOffsetVar);

			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(void*);\n"));
		}
		else if (DataType.IsObject() || DataType.IsReference())
		{
			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = TEXT("FUnknownValueType");

			Param.bIsReference = true;
			Param.RepresentedType = EVariableType::Pointer;

			if (DataType.GetSizeInMemoryBytes() < 4 && !DataType.IsObject())
			{
				Param.LiteralType = TEXT("asDWORD");

				// We can't take a reference to a sub-dword primitive from parms,
				// because angelscript expands it to a dword and unreal does not
				bParmsEntryValid = false;
			}
			else
			{
				ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + %s)"), *Param.LiteralType, *ParmsEntryParamOffsetVar);

				if (DataType.IsObject())
				{
					auto* ObjectType = (asCObjectType*)DataType.GetTypeInfo();
					if (bHasNativeCppForm && (ObjectType->flags & asOBJ_SCRIPT_OBJECT) == 0)
					{
						// Native C++ form is available, use size and align from that
						if (i != 0)
							ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *Param.LiteralType);
						ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(%s);\n"), *Param.LiteralType);
					}
					else if (!JIT->IsTypePotentiallyDifferent(ObjectType))
					{
						// Datatype is guaranteed the same on all platforms, so we can hardcode the alignment
						if (i != 0)
							ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %d);\n"), ObjectType->alignment);
						ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += %d;\n"), ObjectType->size);
					}
					else
					{
						// We don't know at compile time the size and align of this struct, so we need to look it up dynamically
						if (i != 0)
							ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %s->alignment);\n"), *ReferenceTypeInfo(ObjectType));
						ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += %s;\n"), *ReferenceTypeSize(ObjectType));
					}
				}
				else
				{
					// This is a reference to a primitive
					if (i != 0)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *Param.LiteralType);
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(%s);\n"), *Param.LiteralType);
				}
			}

			CppEntryInArgs += FString::Printf(TEXT(", %s* %s"), *Param.LiteralType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s**)(l_fp + %d)"), *Param.LiteralType, -Offset);
		}
		else if (DataType.IsFloatExtendedToDouble())
		{
			Param.LiteralType = TEXT("double");
			Param.bIsValue = true;
			Param.RepresentedType = EVariableType::Double;

			CppEntryInArgs += FString::Printf(TEXT(", double %s"), *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(double*)(l_fp + %d)"), -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(double)*(float*)(((SIZE_T)Parms) + %s)"), *ParmsEntryParamOffsetVar);

			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(float));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(float);\n"));
		}
		else
		{
			FString VMType;
			switch (DataType.GetSizeInMemoryBytes())
			{
			case 1:
				VMType = TEXT("asBYTE");
				Param.RepresentedType = EVariableType::Byte;
			break;
			case 2:
				VMType = TEXT("asWORD");
				Param.RepresentedType = EVariableType::Word;
			break;
			case 4:
				if (DataType.GetTokenType() == ttFloat32)
				{
					VMType = TEXT("float");
					Param.RepresentedType = EVariableType::Float;
				}
				else
				{
					VMType = TEXT("asDWORD");
					Param.RepresentedType = EVariableType::DWord;
				}
			break;
			case 8:
				if (DataType.GetTokenType() == ttFloat64)
				{
					VMType = TEXT("double");
					Param.RepresentedType = EVariableType::Double;
				}
				else
				{
					VMType = TEXT("asQWORD");
					Param.RepresentedType = EVariableType::QWord;
				}
			break;
			default: check(false); break;
			}

			if (Param.LiteralType.Len() == 0)
				Param.LiteralType = VMType;
			Param.bIsValue = true;

			// This is a primitive value with a defined C++ type
			if (i != 0)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *Param.LiteralType);
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset += sizeof(%s);\n"), *Param.LiteralType);

			CppEntryInArgs += FString::Printf(TEXT(", %s %s"), *VMType, *Param.LocalName);
			VMEntryOutArgs += FString::Printf(TEXT(",\n\t\t*(%s*)(l_fp + %d)"), *VMType, -Offset);
			ParmsEntryOutArgs += FString::Printf(TEXT(",\n\t\t(%s)*(%s*)(((SIZE_T)Parms) + %s)"), *VMType, *Param.LiteralType, *ParmsEntryParamOffsetVar);
		}

		LocalVariables.Add(Offset, Param);
	}

	// Populate the return pointer if we have it
	if (ScriptFunction->returnType.GetTokenType() != ttVoid)
	{
		bool bHasParameters = ScriptFunction->parameterTypes.GetLength() != 0;

		auto DataType = ScriptFunction->returnType;
		FAngelscriptTypeUsage ReturnType = FAngelscriptTypeUsage::FromReturn(ScriptFunction);
		FString LiteralType;

		int32 VMOffset = (ScriptFunction->objectType != nullptr) ? 2 : 0;

		bool bHasNativeCppForm = false;
		FString ParmsEntryParamOffsetVar = TEXT("ReturnParmOffset");

		if (ReturnType.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			ReturnType.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
			{
				LiteralType = CppForm.CppType;
				bHasNativeCppForm = true;
			}
			else if (CppForm.CppGenericType.Len() != 0)
			{
				LiteralType = CppForm.CppGenericType;
			}
		}

		if (ScriptFunction->returnType.IsReference() && ScriptFunction->returnType.IsReferenceType())
		{
			bReturnIsReference = true;
			bReturnIsPointer = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(void*);
			ReturnVariableType = EVariableType::Pointer;

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("UObject*");

			VMEntryAssignment = FString::Printf(TEXT("*(%s**)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s**)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			LiteralType += TEXT("*");
			FunctionReturnType = LiteralType;
		}
		else if (ScriptFunction->returnType.IsReferenceType())
		{
			bReturnIsPointer = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(void*);
			ReturnVariableType = EVariableType::Pointer;

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("UObject*");

			VMEntryAssignment = FString::Printf(TEXT("*(%s*)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s*)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(void*));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			FunctionReturnType = LiteralType;
		}
		else if (ScriptFunction->returnType.IsReference())
		{
			bReturnIsReference = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(void*);
			ReturnVariableType = EVariableType::Pointer;

			if (LiteralType.Len() == 0)
				LiteralType = TEXT("FUnknownValueType");

			VMEntryAssignment = FString::Printf(TEXT("*(%s**)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s**)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (DataType.IsObject())
			{
				auto* ObjectType = (asCObjectType*)DataType.GetTypeInfo();
				if (bHasNativeCppForm && (ObjectType->flags & asOBJ_SCRIPT_OBJECT) == 0)
				{
					// Native C++ form is available, use size and align from that
					if (bHasParameters)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
				}
				else if (!JIT->IsTypePotentiallyDifferent(ObjectType))
				{
					// Datatype is guaranteed the same on all platforms, so we can hardcode the alignment
					if (bHasParameters)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %d);\n"), ObjectType->alignment);
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
				}
				else
				{
					// We don't know at compile time the size and align of this struct, so we need to look it up dynamically
					if (bHasParameters)
						ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %s->alignment);\n"), *ReferenceTypeInfo(ObjectType));
					ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
				}
			}
			else
			{
				// This is a reference to a primitive
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}

			LiteralType += TEXT("*");
			FunctionReturnType = LiteralType;
		}
		else if (ScriptFunction->returnType.IsObject())
		{
			bReturnIsReference = true;
			bReturnOnStack = true;
			bReturnValueDirectly = false;
			ReturnVariableType = EVariableType::Pointer;

			check(ScriptFunction->DoesReturnOnStack());
			if (LiteralType.Len() == 0)
				LiteralType = TEXT("FUnknownValueType");

			CppEntryInArgs = FString::Printf(TEXT(", %s* l_ReturnValue"), *LiteralType) + CppEntryInArgs;
			VMEntryOutArgs = FString::Printf(TEXT(",\n\t\t*(%s**)(l_fp + %d)"), *LiteralType, VMOffset) + VMEntryOutArgs;
			ParmsEntryOutArgs = FString::Printf(TEXT(",\n\t\t(%s*)(((SIZE_T)Parms) + %s)"), *LiteralType, *ParmsEntryParamOffsetVar) + ParmsEntryOutArgs;

			auto* ObjectType = (asCObjectType*)DataType.GetTypeInfo();
			if (bHasNativeCppForm)
			{
				// Native C++ form is available, use size and align from that
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}
			else if (!JIT->IsTypePotentiallyDifferent(ObjectType))
			{
				// Datatype is guaranteed the same on all platforms, so we can hardcode the alignment
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %d);\n"), ObjectType->alignment);
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}
			else
			{
				// We don't know at compile time the size and align of this struct, so we need to look it up dynamically
				if (bHasParameters)
					ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, %s->alignment);\n"), *ReferenceTypeInfo(ObjectType));
				ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);
			}

			LiteralType += TEXT("*");
		}
		else if (ScriptFunction->returnType.IsFloatExtendedToDouble())
		{
			VMEntryAssignment = TEXT("*(float*)l_outValue = (float)");
			ParmsEntryAssignment = FString::Printf(TEXT("*(float*)(((SIZE_T)Parms) + %s) = "), *ParmsEntryParamOffsetVar);

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(float));\n"));
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			bReturnIsValue = true;
			bReturnIsFloatingPoint = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = sizeof(double);
			ReturnVariableType = EVariableType::Double;

			LiteralType = TEXT("double");
			FunctionReturnType = LiteralType;
		}
		else
		{
			if (LiteralType.Len() == 0)
			{
				switch (ScriptFunction->returnType.GetSizeInMemoryBytes())
				{
				case 1:
					LiteralType = TEXT("asBYTE");
					ReturnVariableType = EVariableType::Byte;
				break;
				case 2:
					LiteralType = TEXT("asWORD");
					ReturnVariableType = EVariableType::Word;
				break;
				case 4:
					if (ScriptFunction->returnType.GetTokenType() == ttFloat32)
					{
						LiteralType = TEXT("float");
						ReturnVariableType = EVariableType::Float;
					}
					else
					{
						LiteralType = TEXT("asDWORD");
						ReturnVariableType = EVariableType::DWord;
					}
				break;
				case 8:
					if (ScriptFunction->returnType.GetTokenType() == ttFloat64)
					{
						LiteralType = TEXT("double");
						ReturnVariableType = EVariableType::Double;
					}
					else
					{
						LiteralType = TEXT("asQWORD");
						ReturnVariableType = EVariableType::QWord;
					}
				break;
				default: check(false); break;
				}
			}

			if (bHasParameters)
				ParmsEntryHeader += FString::Printf(TEXT("\tParmsOffset = Align(ParmsOffset, alignof(%s));\n"), *LiteralType);
			ParmsEntryHeader += FString::Printf(TEXT("\tconst SIZE_T %s = ParmsOffset;\n"), *ParmsEntryParamOffsetVar);

			VMEntryAssignment = FString::Printf(TEXT("*(%s*)l_outValue = "), *LiteralType);
			ParmsEntryAssignment = FString::Printf(TEXT("*(%s*)(((SIZE_T)Parms) + %s) = "), *LiteralType, *ParmsEntryParamOffsetVar);

			if (ScriptFunction->returnType.GetTokenType() == ttFloat32 || ScriptFunction->returnType.GetTokenType() == ttFloat64)
				bReturnIsFloatingPoint = true;

			bReturnIsValue = true;
			bReturnValueDirectly = true;
			ReturnSizeBytes = ScriptFunction->returnType.GetSizeInMemoryBytes();
			FunctionReturnType = LiteralType;
		}

		LiteralReturnType = LiteralType;
	}

	// Populate the names for named variables
	TArray<bool> VariablesRepresented;
	int32 VariableSpace = ScriptFunction->scriptData->variableSpace;
	VariablesRepresented.AddZeroed(VariableSpace);
	
	auto MarkOffset = [&](int32 Offset, int32 Size)
	{
		for (int i = 0; i < Size; ++i)
		{
			check(!VariablesRepresented[VariableSpace - Offset + i]);
			VariablesRepresented[VariableSpace - Offset + i] = true;
		}
	};

	auto IsOffsetMarked = [&](int32 Offset)
	{
		return VariablesRepresented[VariableSpace - Offset];
	};

	auto RepresentStackVariable = [&](int32 Offset, const asCDataType& DataType, const asCString& VarName)
	{
		if (LocalVariables.Contains(Offset))
			return;
		if (Offset <= 0)
			return;

		FVariableAsLocal LocalVar;

		if (VarName.GetLength() == 0)
		{
			FString VarType;
			if (DataType.IsObject())
			{
				if (DataType.IsReference() || DataType.IsReferenceType())
					VarType = TEXT("ptr_");
			}
			else
			{
				switch (DataType.GetSizeInMemoryBytes())
				{
				case 1: VarType = TEXT("byte_"); break;
				case 2: VarType = TEXT("word_"); break;
				case 4:
					if (DataType.GetTokenType() == ttFloat32)
						VarType = TEXT("float_");
					else
						VarType = TEXT("dword_");
				break;
				case 8:
					if (DataType.GetTokenType() == ttFloat64)
						VarType = TEXT("double_");
					else
						VarType = TEXT("qword_");
				break;
				default: check(false); break;
				}
			}

			LocalVar.LocalName = FString::Printf(TEXT("v_TEMP_%s%d"), *VarType, Offset);
		}
		else
		{
			LocalVar.LocalName = ANSI_TO_TCHAR(VarName.AddressOf());
			LocalVar.LocalName = TEXT("v_") + LocalVar.LocalName;
		}

		LocalVar.Type = FAngelscriptTypeUsage::FromDataType(DataType);

		if (LocalNamesUsed.Contains(LocalVar.LocalName))
		{
			FString BaseName = LocalVar.LocalName;
			int32 NameIndex = 0;

			do
			{
				NameIndex += 1;
				LocalVar.LocalName = FString::Printf(TEXT("%s_%d"), *BaseName, NameIndex);
			} while (LocalNamesUsed.Contains(LocalVar.LocalName));
		}
		LocalNamesUsed.Add(LocalVar.LocalName);
		check(!IsOffsetMarked(Offset));

		if (LocalVar.Type.IsValid())
		{
			FAngelscriptType::FCppForm CppForm;
			LocalVar.Type.GetCppForm(CppForm);

			if (CppForm.CppHeader.Len() != 0)
				AddHeader(CppForm.CppHeader);

			if (CppForm.CppType.Len() != 0)
				LocalVar.LiteralType = CppForm.CppType;
			else if (CppForm.CppGenericType.Len() != 0)
				LocalVar.LiteralType = CppForm.CppGenericType;
		}

		if (DataType.IsReference() && DataType.IsReferenceType())
		{
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("UObject*");

			LocalVar.bIsPointer = true;
			LocalVar.bIsReference = true;

			FunctionHead += FString::Printf(TEXT("%s* %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);

			MarkOffset(Offset, 2);

			LocalVar.RepresentedType = EVariableType::Pointer;
			LocalVar.LiteralType += TEXT("*");
		}
		else if (DataType.IsNullHandle())
		{
			LocalVar.LiteralType = TEXT("void*");
			LocalVar.bIsPointer = true;
			LocalVar.RepresentedType = EVariableType::Pointer;
			FunctionHead += FString::Printf(TEXT("%s %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);

			MarkOffset(Offset, 2);
		}
		else if (DataType.IsReferenceType())
		{
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("UObject*");

			LocalVar.bIsPointer = true;
			LocalVar.RepresentedType = EVariableType::Pointer;
			FunctionHead += FString::Printf(TEXT("%s %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);

			MarkOffset(Offset, 2);
		}
		else if (DataType.IsReference())
		{
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("FUnknownValueType");

			LocalVar.bIsReference = true;
			LocalVar.RepresentedType = EVariableType::Pointer;
			FunctionHead += FString::Printf(TEXT("%s* %s = nullptr;\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);
			LocalVar.LiteralType += TEXT("*");

			MarkOffset(Offset, 2);
		}
		else if (DataType.IsObject())
		{
			FString NativeType;
			if (LocalVar.LiteralType.Len() == 0)
				LocalVar.LiteralType = TEXT("FUnknownValueType");
			else
				NativeType = LocalVar.LiteralType;

			LocalVar.bIsReference = true;
			LocalVar.RepresentedType = EVariableType::Object;

			int32 Align = DataType.GetAlignment();

			if (NativeType.Len() != 0 && !DataType.IsScriptObject())
			{
				if (Align > 1)
					FunctionHead += FString::Printf(TEXT("alignas(%d) "), Align);
				FunctionHead += FString::Printf(TEXT("asBYTE MEM_%s[sizeof(%s)];\n"),
					*LocalVar.LocalName, *NativeType);
			}
			else
			{
				bool bPotentiallyDifferent = JIT->IsTypePotentiallyDifferent((asCObjectType*)DataType.GetTypeInfo());
				if (bPotentiallyDifferent)
				{
					auto ComputedOffsets = JIT->GetComputedOffsets((asCObjectType*)DataType.GetTypeInfo());
					if (ComputedOffsets.IsValid() && (ComputedOffsets->bCanHardcodeSize || ComputedOffsets->bHasComputedSize))
					{
						FunctionHead += FString::Printf(TEXT("alignas(%s) asBYTE MEM_%s[%s];\n"),
							*ReferenceTypeAlignment(DataType.GetTypeInfo()),
							*LocalVar.LocalName,
							*ReferenceTypeSize(DataType.GetTypeInfo()));
					}
					else
					{
						// Don't need to do alignment here, Unreal Alloca is always aligned to 16 bytes already
						FunctionHead += FString::Printf(TEXT("asDWORD* MEM_%s = (asDWORD*)FMemory_Alloca(%s->size);\n"),
							*LocalVar.LocalName, *ReferenceTypeInfo(DataType.GetTypeInfo()));
					}
				}
				else
				{
					if (Align > 1)
						FunctionHead += FString::Printf(TEXT("alignas(%d) "), Align);
					FunctionHead += FString::Printf(TEXT("asBYTE MEM_%s[%d];\n"),
						*LocalVar.LocalName, FMath::Max(DataType.GetSizeInMemoryDWords() * 4, 1));
				}
			}

			MarkOffset(Offset, DataType.GetSizeInMemoryDWords());

			FunctionHead += FString::Printf(TEXT("%s& %s = (%s&)MEM_%s[0];\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName, *LocalVar.LiteralType, *LocalVar.LocalName);

			LocalVar.LiteralType += TEXT("&");
		}
		else
		{
			FString VMType;
			switch (DataType.GetSizeInMemoryBytes())
			{
			case 1:
				LocalVar.LiteralType = TEXT("asBYTE");
				LocalVar.RepresentedType = EVariableType::Byte;
			break;
			case 2:
				LocalVar.LiteralType = TEXT("asWORD");
				LocalVar.RepresentedType = EVariableType::Word;
			break;
			case 4:
				if (DataType.GetTokenType() == ttFloat32)
				{
					LocalVar.LiteralType = TEXT("float");
					LocalVar.RepresentedType = EVariableType::Float;
				}
				else
				{
					LocalVar.LiteralType = TEXT("asDWORD");
					LocalVar.RepresentedType = EVariableType::DWord;
				}
			break;
			case 8:
				if (DataType.GetTokenType() == ttFloat64)
				{
					LocalVar.LiteralType = TEXT("double");
					LocalVar.RepresentedType = EVariableType::Double;
				}
				else
				{
					LocalVar.LiteralType = TEXT("asQWORD");
					LocalVar.RepresentedType = EVariableType::QWord;
				}
			break;
			default: check(false); break;
			}

			MarkOffset(Offset, DataType.GetSizeInMemoryDWords());

			LocalVar.bIsValue = true;
			FunctionHead += FString::Printf(TEXT("%s %s = {};\n"),
				*LocalVar.LiteralType, *LocalVar.LocalName);
		}

		LocalVariables.Add(Offset, LocalVar);
	};

	// Create variables for any explicitly declared variables in the function
	for (int i = 0, Count = ScriptFunction->scriptData->variables.GetLength(); i < Count; ++i)
	{
		asSScriptVariable* Var = ScriptFunction->scriptData->variables[i];

		int32 Offset = Var->stackOffset;
		auto DataType = Var->type;

		RepresentStackVariable(Offset, DataType, Var->name);
	}

	// Create variables for any implicitly declared variables in the function
	for (int i = 0, Count = ScriptFunction->scriptData->objVariableTypes.GetLength(); i < Count; ++i)
	{
		int32 Offset = ScriptFunction->scriptData->objVariablePos[i];
		asCDataType DataType;

		auto* VarType = ScriptFunction->scriptData->objVariableTypes[i];
		if ( VarType != nullptr )
		{
			// Don't know why this is here but it's in the equivalent code in the angelscript library so I assume it's needed for something
			int idx = ScriptFunction->scriptData->objVariablePos.IndexOf(ScriptFunction->scriptData->objVariablePos[i]);

			bool isOnHeap = asUINT(idx) < ScriptFunction->scriptData->objVariablesOnHeap ? true : false;

			DataType = asCDataType::CreateType(VarType, false);
			if (isOnHeap)
			{
				if (VarType->GetFlags() & asOBJ_VALUE)
					DataType.MakeReference(true);
				else
					DataType.MakeHandle(true);
			}
		}
		else
		{
			DataType = asCDataType::CreateNullHandle();
		}

		RepresentStackVariable(Offset, DataType, asCString());
	}

	// Create variables for any temporary variables in the function
	for (int i = 0, Count = ScriptFunction->scriptData->temporaryVariables.GetLength(); i < Count; ++i)
	{
		int32 Offset = ScriptFunction->scriptData->temporaryVariables[i].Offset;
		asCDataType DataType = asCDataType::CreatePrimitive(ScriptFunction->scriptData->temporaryVariables[i].Token, false);

		RepresentStackVariable(Offset, DataType, asCString());
	}

	FunctionHead = FString::Printf(TEXT("%s %s(FScriptExecution& Execution%s)\n{\n"), *FunctionReturnType, *FunctionName, *CppEntryInArgs) + FunctionHead;
	FunctionDecl = FString::Printf(TEXT("%s %s(FScriptExecution& Execution%s)"), *FunctionReturnType, *FunctionName, *CppEntryInArgs);

	// Generate the stub entries
	FunctionFoot += TEXT("}\n");
	FunctionFoot += FString::Printf(TEXT("static void %s_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)\n{\n"), *FunctionName);
	FunctionFoot += FString::Printf(TEXT("\t%s%s(Execution%s);\n"), *VMEntryAssignment, *FunctionName, *VMEntryOutArgs);
	FunctionFoot += VMEntryFooter;
	FunctionFoot += TEXT("}\n");

	FString ParmsFunc = TEXT("nullptr");
	if (bParmsEntryValid)
	{
		FunctionFoot += FString::Printf(TEXT("static void %s_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)\n{\n"), *FunctionName);
		FunctionFoot += ParmsEntryHeader;
		FunctionFoot += FString::Printf(TEXT("\t%s%s(Execution%s);\n"), *ParmsEntryAssignment, *FunctionName, *ParmsEntryOutArgs);
		FunctionFoot += ParmsEntryFooter;
		FunctionFoot += TEXT("}\n");

		ParmsFunc = FString::Printf(TEXT("&%s_ParmsEntry"), *FunctionName);
	}

	// Make sure we're registering the function to the JIT
	FunctionFoot += FString::Printf(TEXT("AS_FORCE_LINK static const FStaticJITFunction %s_Register(0x%xu, &%s_VMEntry, %s, (asJITFunction_Raw)(void*)&%s);\n\n"),
		*FunctionName,
		FunctionId,
		*FunctionName,
		*ParmsFunc,
		*FunctionName);
}

void FStaticJITContext::NewInstruction()
{
	auto& Instr = Instructions[CurrentInstructionIndex];
	if (Instr.Label.Len() != 0)
	{
		MaterializeWholeStack();

		// Don't allow any use of the value register after landing a jump,
		// since it will have been optimized away by the jump instruction
		//MaterializeValueRegister();
		if (Instr.bHasMultipleValueRegisterStates)
		{
			ValueRegisterState = EValueRegisterState::Indeterminate;
		}
		else if (ValueRegisterState != EValueRegisterState::Indeterminate)
		{
			if (ValueRegisterState != Instr.ValueRegisterStateFromJump)
				ValueRegisterState = EValueRegisterState::Indeterminate;
		}
		else
		{
			ValueRegisterState = Instr.ValueRegisterStateFromJump;
		}

		LocalStackOffset = Instr.StackOffsetBeforeInstr;
		Line("{0}:", Instr.Label);
	}
	else if (!Instr.bJumpPartOfSwitch)
	{
		check(Instr.bMarked);
		check(Instr.StackOffsetBeforeInstr == LocalStackOffset);
	}

	// If this instruction modifies any variables that we've pushed to the stack before,
	// make sure those parts of the stack are materialized
	auto BCInstr = (asEBCInstr)*(asBYTE*)Instr.BC;
	const auto& BCInfo = asBCInfo[BCInstr];
	if((BCInfo.type == asBCTYPE_wW_rW_rW_ARG ||
		BCInfo.type == asBCTYPE_wW_rW_ARG    ||
		BCInfo.type == asBCTYPE_wW_rW_DW_ARG ||
		BCInfo.type == asBCTYPE_wW_ARG       ||
		BCInfo.type == asBCTYPE_wW_W_ARG     ||
		BCInfo.type == asBCTYPE_wW_DW_ARG    ||
		BCInfo.type == asBCTYPE_wW_QW_ARG))
	{
		int VariableOffset = asBC_SWORDARG0(Instr.BC);
		MaterializePushedVariableAtOffset(VariableOffset);
	}

	// If this instruction has the potential to modify anything volatile, materialize
	// the parts of the stack that could be edited by it
	if (Instr.Bytecode->CanModifyStackVolatiles(*this))
	{
		MaterializeStackVolatiles();
	}
}

bool FStaticJITContext::IsPartOfSwitch()
{
	return Instructions[CurrentInstructionIndex].bJumpPartOfSwitch;
}

void FStaticJITContext::AddLabel(int32 RelativeBCOffset, bool bConditional)
{
	asDWORD* TargetBC = BC + RelativeBCOffset;
	for (int32 i = 0, Count = Instructions.Num(); i < Count; ++i)
	{
		auto& TargetInstr = Instructions[i];
		if (TargetInstr.BC == TargetBC)
		{
			Instructions[CurrentInstructionIndex].JumpTargets.Add(i);
			Instructions[CurrentInstructionIndex].bIsConditionalJump = bConditional;
			TargetInstr.ReceivedJumps.Add(CurrentInstructionIndex);

			if (TargetInstr.Label.Len() == 0)
			{
				TargetInstr.Label = JIT->GetUniqueLabelName(
					*File, ScriptFunction,
					LexToString((int)(TargetBC - BC_FunctionStart)));
			}
			return;
		}
	}

	checkf(false, TEXT("Couldn't find instruction to jump to for label."));
}

FStaticJITContext::FInstruction& FStaticJITContext::GetLabelInstruction(int32 RelativeBCOffset)
{
	auto& CurrentInstr = Instructions[CurrentInstructionIndex];
	asDWORD* TargetBC = BC + RelativeBCOffset;
	for (int32 i = 0, Count = CurrentInstr.JumpTargets.Num(); i < Count; ++i)
	{
		auto& TargetInstr = Instructions[CurrentInstr.JumpTargets[i]];
		if (TargetInstr.BC == TargetBC)
		{
			return TargetInstr;
		}
	}

	check(false);
	return Instructions[0];
}

void FStaticJITContext::Jump(FInstruction& TargetInstruction)
{
	check(FloatingStackExpressions.Num() == 0);
	check(LocalStackOffset == TargetInstruction.StackOffsetBeforeInstr);

	if (ValueRegisterState != EValueRegisterState::Indeterminate)
	{
		if (TargetInstruction.ValueRegisterStateFromJump == EValueRegisterState::Indeterminate)
			TargetInstruction.ValueRegisterStateFromJump = ValueRegisterState;
		else if (TargetInstruction.ValueRegisterStateFromJump != ValueRegisterState)
			TargetInstruction.bHasMultipleValueRegisterStates = true;
	}
	else
	{
		TargetInstruction.bHasMultipleValueRegisterStates = true;
	}

	Line("goto {0};", TargetInstruction.Label);
}

void FStaticJITContext::ReturnEmptyValue()
{
	if (bReturnValueDirectly)
		Line("return {};");
	else
		Line("return;");
}

void MarkLiveObjectsForExceptionCleanup(FStaticJITContext& Context, bool bAfterCurrentOp, TArray<int, TInlineAllocator<64>>& liveObjects)
{
	// Destruct any objects that are alive at this point in the bytecode
	asDWORD* CleanupBC = Context.BC;
	if (bAfterCurrentOp)
	{
		// Move to next instruction for determining object live status
		CleanupBC = Context.GetNextBC(CleanupBC);
	}

	asUINT bcPos = CleanupBC - Context.BC_FunctionStart;

	auto& objVariableInfo = Context.ScriptFunction->scriptData->objVariableInfo;
	auto& objVariablePos = Context.ScriptFunction->scriptData->objVariablePos;

	liveObjects.SetNumZeroed(objVariablePos.GetLength());
	for( int n = 0; n < (int)objVariableInfo.GetLength(); n++ )
	{
		// Find the first variable info with a larger position than the current
		// As the variable info are always placed on the instruction right after the
		// one that initialized or freed the object, the current position needs to be
		// considered as valid.
		if( objVariableInfo[n].programPos > bcPos )
		{
			// We've determined how far the execution ran, now determine which variables are alive
			for( --n; n >= 0; n-- )
			{
				switch( objVariableInfo[n].option )
				{
				case asOBJ_UNINIT: // Object was destroyed
					{
						// TODO: optimize: This should have been done by the compiler already
						// Which variable is this?
						asUINT var = 0;
						for (asUINT v = 0; v < objVariablePos.GetLength(); v++)
						{
							if (objVariablePos[v] == objVariableInfo[n].variableOffset)
							{
								var = v;
								break;
							}
						}

						liveObjects[var] -= 1;
					}
					break;
				case asOBJ_INIT: // Object was created
					{
						// Which variable is this?
						asUINT var = 0;
						for (asUINT v = 0; v < objVariablePos.GetLength(); v++)
						{
							if (objVariablePos[v] == objVariableInfo[n].variableOffset)
							{
								var = v;
								break;
							}
						}
						liveObjects[var] += 1;
					}
					break;
				case asBLOCK_BEGIN: // Start block
					// We should ignore start blocks, since it just means the
					// program was within the block when the exception ocurred
					break;
				case asBLOCK_END: // End block
					// We need to skip the entire block, as the objects created
					// and destroyed inside this block are already out of scope
					{
						int nested = 1;
						while( nested > 0 )
						{
							int option = objVariableInfo[--n].option;
							if( option == 3 )
								nested++;
							if( option == 2 )
								nested--;
						}
					}
					break;
				}
			}

			// We're done with the investigation
			break;
		}
	}
}

void FStaticJITContext::ExceptionCleanupAndReturn(bool bAfterCurrentOp, bool bSetNullptrException)
{
	auto& objVariablePos = ScriptFunction->scriptData->objVariablePos;

	TArray<int, TInlineAllocator<64>> liveObjects;
	MarkLiveObjectsForExceptionCleanup(*this, bAfterCurrentOp, liveObjects);

	// Determine which variables need cleanup
	FExceptionCleanup Cleanup;

	for( asUINT n = 0; n < objVariablePos.GetLength(); n++ )
	{
		int pos = objVariablePos[n];
		if( n < ScriptFunction->scriptData->objVariablesOnHeap )
		{
			continue;
		}
		else
		{
			// Only destroy the object if it is truly alive
			if( liveObjects[n] > 0 )
			{
				auto* objType = (asCObjectType*)ScriptFunction->scriptData->objVariableTypes[n];
				if (objType->beh.destruct == 0)
					continue;

				Cleanup.Positions.Add(pos);
				Cleanup.Types.Add(objType);
			}
		}
	}

	// If we don't have any cleanup to do, just return
	if (Cleanup.Positions.Num() == 0)
	{
		if (bSetNullptrException)
			Line("SCRIPT_NULL_POINTER_EXCEPTION();");
		ReturnEmptyValue();
		return;
	}

	// Check if we have an existing cleanup label for this
	for (auto& ExistingLabel : ExceptionCleanupLabels)
	{
		if (ExistingLabel.Positions == Cleanup.Positions
			&& ExistingLabel.Types == Cleanup.Types)
		{
			if (bSetNullptrException)
				ExistingLabel.bCalledAddNullptrException = true;
			else
				ExistingLabel.bCalledWithoutAddNullptrException = true;

			Line("goto {0};", ExistingLabel.Label);
			return;
		}
	}

	// Create a new cleanup label
	Cleanup.Label = JIT->GetUniqueLabelName(
		*File, ScriptFunction,
		FString::Printf(TEXT("EXCEPTION_%d"), ExceptionCleanupLabels.Num()));
	if (bSetNullptrException)
		Cleanup.bCalledAddNullptrException = true;
	else
		Cleanup.bCalledWithoutAddNullptrException = true;
	ExceptionCleanupLabels.Add(Cleanup);

	Line("goto {0};", Cleanup.Label);
}

void FStaticJITContext::WriteOutFunction()
{
	File->Content.Add(FunctionHead);

	for (auto& StateVar : StateVars)
		File->Content.Add(FString::Printf(TEXT("%s %s;\n"), *StateVar.Type, *StateVar.Name));

	if (ParmStructSize != 0 || bParmIsExecuted)
		File->Content.Add(FString::Printf(TEXT("alignas(16) asBYTE l_ParmStruct[%d];\n"), FMath::Max(ParmStructSize, 1)));

	TArray<FEntryContent> ActualContent = MoveTemp(FunctionContent);
	FunctionContent.Empty();

	int32 Index = 0;
	int32 Count = ActualContent.Num();

	for (; Index < Count; ++Index)
	{
		const FEntryContent& Entry = ActualContent[Index];
		switch(Entry.Type)
		{
		case EEntryType::Line:
			File->Content.Add(Entry.Line + TEXT("\n"));
		break;
		}
	}
	File->Content.Add(FunctionFoot);

	check(FunctionContent.Num() == 0);
	FunctionHead.Empty();
	FunctionFoot.Empty();
	FunctionContent.Empty();
	StateVars.Empty();
}

FString FStaticJITContext::BCConstant_QWord()
{
	return FString::Printf(TEXT("(asQWORD)0x%llxu"), asBC_PTRARG(BC));
}

FString FStaticJITContext::BCConstant_DWord(int32 Offset)
{
	return FString::Printf(TEXT("(asDWORD)0x%xu"), asBC_DWORDARG(BC + Offset));
}

inline FString GetSymbolNameForObjectType(asCTypeInfo* TypeInfo)
{
	int32 SubTypes = TypeInfo->GetSubTypeCount();
	if (SubTypes == 0)
		return ANSI_TO_TCHAR(TypeInfo->name.AddressOf());

	FString SymbolName = ANSI_TO_TCHAR(TypeInfo->name.AddressOf());
	for (int32 i = 0; i < SubTypes; ++i)
	{
		SymbolName += TEXT("_");
		SymbolName += ANSI_TO_TCHAR(
			TypeInfo->GetEngine()->GetTypeDeclaration(
				TypeInfo->GetSubTypeId(i), false
			)
		);
	}
	return SymbolName;
}

FString FStaticJITContext::ReferenceFunction(asCScriptFunction* Function)
{
	FString* ExistingName = JIT->ExternalReferenceNames.Find(Function);
	if (ExistingName != nullptr)
	{
		// Existing reference, see if we've already imported it into our file
		if (!File->ExternalDeclarations.Contains(Function))
		{
			// Need to forward declare it in our file before we can use it
			File->ExternalDeclarations.Add(
				Function,
				FString::Printf(
					TEXT("extern FJitRef_Function %s;"),
					**ExistingName
				)
			);
		}

		return FString::Printf(TEXT("%s.Get()"), **ExistingName);
	}
	else
	{
		// Create a new reference for this function
		FString SymbolName;
		if (Function->objectType != nullptr)
		{
			SymbolName = FString::Printf(
				TEXT("FREF_%s__%s"),
				*GetSymbolNameForObjectType(Function->objectType),
				ANSI_TO_TCHAR(Function->name.AddressOf())
			);
		}
		else
		{
			SymbolName = FString::Printf(
				TEXT("FREF_%s"),
				ANSI_TO_TCHAR(Function->name.AddressOf())
			);
		}

		SymbolName = JIT->GetUniqueSymbolName(SymbolName);

		// Add reference to precompiled data
		auto Ref = JIT->PrecompiledData->ReferenceFunction(Function);

		FString Decl = FString::Printf(TEXT("AS_FORCE_LINK FJitRef_Function %s(0x%llx);"), *SymbolName, Ref.OldReference);
		File->ExternalDeclarations.Add(Function, Decl);
		JIT->ExternalReferenceNames.Add(Function, SymbolName);

		return FString::Printf(TEXT("%s.Get()"), *SymbolName);
	}
}

FString FStaticJITContext::ReferenceSystemFunctionPointer(asCScriptFunction* Function)
{
	// Use script-function identity, not raw sysFuncIntf->func, since multiple
	// AS functions can share a native entry but require distinct JIT refs.
	FString* ExistingName = JIT->ExternalSystemFunctionPointerReferenceNames.Find(Function);

	if (ExistingName != nullptr)
	{
		// Existing reference, see if we've already imported it into our file
		if (!File->ExternalSystemFunctionPointerDeclarations.Contains(Function))
		{
			// Need to forward declare it in our file before we can use it
			File->ExternalSystemFunctionPointerDeclarations.Add(
				Function,
				FString::Printf(
					TEXT("extern FJitRef_SystemFunctionPointer %s;"),
					**ExistingName
				)
			);
		}

		return *ExistingName;
	}
	else
	{
		// Create a new reference for this function
		FString SymbolName;
		if (Function->objectType != nullptr)
		{
			SymbolName = FString::Printf(
				TEXT("SYSPTR_%s__%s"),
				*GetSymbolNameForObjectType(Function->objectType),
				ANSI_TO_TCHAR(Function->name.AddressOf())
			);
		}
		else
		{
			SymbolName = FString::Printf(
				TEXT("SYSPTR_%s"),
				ANSI_TO_TCHAR(Function->name.AddressOf())
			);
		}

		SymbolName = JIT->GetUniqueSymbolName(SymbolName);

		// Add reference to precompiled data
		auto Ref = JIT->PrecompiledData->ReferenceFunction(Function);

		FString Decl = FString::Printf(TEXT("AS_FORCE_LINK FJitRef_SystemFunctionPointer %s(0x%llx);"), *SymbolName, Ref.OldReference);
		File->ExternalSystemFunctionPointerDeclarations.Add(Function, Decl);
		JIT->ExternalSystemFunctionPointerReferenceNames.Add(Function, SymbolName);

		return SymbolName;
	}
}

FString FStaticJITContext::ReferenceTypeInfo(asITypeInfo* TypeInfo)
{
	FString* ExistingName = JIT->ExternalReferenceNames.Find(TypeInfo);
	if (ExistingName != nullptr)
	{
		// Existing reference, see if we've already imported it into our file
		if (!File->ExternalDeclarations.Contains(TypeInfo))
		{
			// Need to forward declare it in our file before we can use it
			File->ExternalDeclarations.Add(
				TypeInfo,
				FString::Printf(
					TEXT("extern FJitRef_Type %s;"),
					**ExistingName
				)
			);
		}

		return FString::Printf(TEXT("%s.Get()"), **ExistingName);
	}
	else
	{
		// Create a new reference for this function
		FString SymbolName = FString::Printf(
			TEXT("TREF_%s"),
			*GetSymbolNameForObjectType((asCTypeInfo*)TypeInfo)
		);

		SymbolName = JIT->GetUniqueSymbolName(SymbolName);

		// Add reference to precompiled data
		auto Ref = JIT->PrecompiledData->ReferenceTypeInfo(TypeInfo);

		FString Decl = FString::Printf(TEXT("AS_FORCE_LINK FJitRef_Type %s(0x%llx);"), *SymbolName, Ref.OldReference);
		File->ExternalDeclarations.Add(TypeInfo, Decl);
		JIT->ExternalReferenceNames.Add(TypeInfo, SymbolName);

		return FString::Printf(TEXT("%s.Get()"), *SymbolName);
	}
}

FString FStaticJITContext::ReferenceTypeSize(asITypeInfo* TypeInfo)
{
	if (!JIT->IsTypePotentiallyDifferent(TypeInfo))
		return FString::Printf(TEXT("%d"), TypeInfo->GetSize());

	asCObjectType* ObjectType = CastToObjectType((asCTypeInfo*)TypeInfo);
	if (ObjectType != nullptr)
	{
		auto ComputedOffsets = JIT->GetComputedOffsets(ObjectType);
		if (ComputedOffsets.IsValid() && (ComputedOffsets->bCanHardcodeSize || ComputedOffsets->bHasComputedSize))
		{
			FString SizeExpr;
			FString AlignmentExpr;

			if (ComputedOffsets->bCanHardcodeSize)
			{
				SizeExpr = FString::Printf(TEXT("%d"), ComputedOffsets->HardcodedSize);
				AlignmentExpr = FString::Printf(TEXT("%d"), ComputedOffsets->HardcodedAlignment);
			}
			else if (ComputedOffsets->bHasComputedSize)
			{
				SizeExpr = ComputedOffsets->ComputedSizeVariable;
				AlignmentExpr = ComputedOffsets->ComputedAlignmentVariable;

				if (!ComputedOffsets->ContainingSharedHeader.IsEmpty())
					File->SharedHeaderDependencies.Add(ComputedOffsets->ContainingSharedHeader);
				if (!ComputedOffsets->AdditionalHeader.IsEmpty())
					File->Headers.Add(ComputedOffsets->AdditionalHeader);
			}

#if AS_JIT_VERIFY_PROPERTY_OFFSETS
			if (!JIT->VerifiedExternalReferences.Contains(ObjectType))
			{
				auto Ref = JIT->PrecompiledData->ReferenceTypeInfo(TypeInfo);
				File->Content.Add(
					FString::Printf(
						TEXT("AS_FORCE_LINK FJitVerifyTypeSize TSIZEVERIFY_%s(0x%llx, %s, %s);\n"),
						*GetSymbolNameForObjectType(ObjectType),
						Ref.OldReference, *SizeExpr, *AlignmentExpr
					)
				);
				JIT->VerifiedExternalReferences.Add(ObjectType);
			}
#endif

			return SizeExpr;
		}
	}

	return FString::Printf(TEXT("%s->size"), *ReferenceTypeInfo(TypeInfo));
}

FString FStaticJITContext::ReferenceTypeAlignment(asITypeInfo* TypeInfo)
{
	if (!JIT->IsTypePotentiallyDifferent(TypeInfo))
		return FString::Printf(TEXT("%d"), TypeInfo->alignment);

	asCObjectType* ObjectType = CastToObjectType((asCTypeInfo*)TypeInfo);
	if (ObjectType != nullptr)
	{
		auto ComputedOffsets = JIT->GetComputedOffsets(ObjectType);
		if (ComputedOffsets.IsValid() && (ComputedOffsets->bCanHardcodeSize || ComputedOffsets->bHasComputedSize))
		{
			FString SizeExpr;
			FString AlignmentExpr;

			if (ComputedOffsets->bCanHardcodeSize)
			{
				SizeExpr = FString::Printf(TEXT("%d"), ComputedOffsets->HardcodedSize);
				AlignmentExpr = FString::Printf(TEXT("%d"), ComputedOffsets->HardcodedAlignment);
			}
			else if (ComputedOffsets->bHasComputedSize)
			{
				SizeExpr = ComputedOffsets->ComputedSizeVariable;
				AlignmentExpr = ComputedOffsets->ComputedAlignmentVariable;

				if (!ComputedOffsets->ContainingSharedHeader.IsEmpty())
					File->SharedHeaderDependencies.Add(ComputedOffsets->ContainingSharedHeader);
				if (!ComputedOffsets->AdditionalHeader.IsEmpty())
					File->Headers.Add(ComputedOffsets->AdditionalHeader);
			}

#if AS_JIT_VERIFY_PROPERTY_OFFSETS
			if (!JIT->VerifiedExternalReferences.Contains(ObjectType))
			{
				auto Ref = JIT->PrecompiledData->ReferenceTypeInfo(TypeInfo);
				File->Content.Add(
					FString::Printf(
						TEXT("AS_FORCE_LINK FJitVerifyTypeSize TSIZEVERIFY_%s(0x%llx, %s, %s);\n"),
						*GetSymbolNameForObjectType(ObjectType),
						Ref.OldReference, *SizeExpr, *AlignmentExpr
					)
				);
				JIT->VerifiedExternalReferences.Add(ObjectType);
			}
#endif

			return AlignmentExpr;
		}
	}

	return FString::Printf(TEXT("%s->alignment"), *ReferenceTypeInfo(TypeInfo));
}

FString FStaticJITContext::ReferenceObjectType(asITypeInfo* TypeInfo)
{
	check((TypeInfo->GetFlags() & (asOBJ_VALUE | asOBJ_REF)) != 0);

	FString* ExistingName = JIT->ExternalReferenceNames.Find(TypeInfo);
	if (ExistingName != nullptr)
	{
		// Existing reference, see if we've already imported it into our file
		if (!File->ExternalDeclarations.Contains(TypeInfo))
		{
			// Need to forward declare it in our file before we can use it
			File->ExternalDeclarations.Add(
				TypeInfo,
				FString::Printf(
					TEXT("extern FJitRef_Type %s;"),
					**ExistingName
				)
			);
		}

		return FString::Printf(TEXT("%s.GetObjectType()"), **ExistingName);
	}
	else
	{
		// Create a new reference for this function
		FString SymbolName = FString::Printf(
			TEXT("TREF_%s"),
			*GetSymbolNameForObjectType((asCTypeInfo*)TypeInfo)
		);

		SymbolName = JIT->GetUniqueSymbolName(SymbolName);

		// Add reference to precompiled data
		auto Ref = JIT->PrecompiledData->ReferenceTypeInfo(TypeInfo);

		FString Decl = FString::Printf(TEXT("AS_FORCE_LINK FJitRef_Type %s(0x%llx);"), *SymbolName, Ref.OldReference);
		File->ExternalDeclarations.Add(TypeInfo, Decl);
		JIT->ExternalReferenceNames.Add(TypeInfo, SymbolName);

		return FString::Printf(TEXT("%s.GetObjectType()"), *SymbolName);
	}
}

FString FStaticJITContext::ReferenceTypeId(asDWORD TypeId)
{
	if (TypeId <= asTYPEID_LAST_PRIMITIVE)
		return FString::Printf(TEXT("0x%x"), TypeId);

	auto* TypeInfo = GetEngine()->GetTypeInfoById(TypeId);

	asQWORD ExtraFlags = asQWORD(asDWORD(TypeId) & ~(asTYPEID_MASK_SEQNBR | asTYPEID_MASK_OBJECT));
	if (ExtraFlags != 0)
	{
		return FString::Printf(TEXT("(%s->typeId | 0x%x)"),
			*ReferenceTypeInfo(TypeInfo),
			ExtraFlags);
	}
	else
	{
		return FString::Printf(TEXT("%s->typeId"),
			*ReferenceTypeInfo(TypeInfo));
	}
}

FString FStaticJITContext::ReferenceGlobalVariable(void* GlobalVar)
{
	FString* ExistingName = JIT->ExternalReferenceNames.Find(GlobalVar);
	if (ExistingName != nullptr)
	{
		// Existing reference, see if we've already imported it into our file
		if (!File->ExternalDeclarations.Contains(GlobalVar))
		{
			// Need to forward declare it in our file before we can use it
			File->ExternalDeclarations.Add(
				GlobalVar,
				FString::Printf(
					TEXT("extern FJitRef_GlobalVar %s;"),
					**ExistingName
				)
			);
		}

		return FString::Printf(TEXT("%s.Get()"), **ExistingName);
	}
	else
	{
		// Add reference to precompiled data
		FString VarName;
		auto Ref = JIT->PrecompiledData->ReferenceGlobalVariable(GlobalVar, &VarName);

		if (VarName.Len() == 0)
			VarName = FString::Printf(TEXT("%x"), GlobalVar);

		// Some strings are long and shouldn't get super long variable names
		if (VarName.Len() >= 50)
			VarName = VarName.Mid(0, 50);

		// Create a new reference for this function
		FString SymbolName = FString::Printf(
			TEXT("GREF_%s"),
			*VarName
		);

		SymbolName = JIT->GetUniqueSymbolName(SymbolName);

		FString Decl = FString::Printf(TEXT("AS_FORCE_LINK FJitRef_GlobalVar %s(0x%llx);"), *SymbolName, Ref.OldReference);
		File->ExternalDeclarations.Add(GlobalVar, Decl);
		JIT->ExternalReferenceNames.Add(GlobalVar, SymbolName);

		return FString::Printf(TEXT("%s.Get()"), *SymbolName);
	}
}

FString FStaticJITContext::ReferencePropertyOffset(short Offset, int TypeId)
{
	asCObjectType* ObjectType = (asCObjectType*)GetEngine()->GetTypeInfoById(TypeId);
	asCObjectType* InType = nullptr;
	asCObjectProperty* ASProperty = ObjectType->GetPropertyByOffset(Offset, &InType);

	// Try to turn this into a C++ offsetof() expression
	FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(InType->GetTypeId());
	if (Usage.IsValid())
	{
		UStruct* UnrealType = nullptr;
		if (auto* UnrealClass = Usage.GetClass())
			UnrealType = UnrealClass;
		else if (auto* UnrealStruct = Usage.GetUnrealStruct())
			UnrealType = UnrealStruct;

		FAngelscriptType::FCppForm CppForm;
		Usage.GetCppForm(CppForm);

		if (CppForm.CppType.Len() != 0 && UnrealType != nullptr)
		{
			// Find the FProperty that represents this
			for (TFieldIterator<FProperty> It(UnrealType); It; ++It)
			{
				FProperty* Property = *It;
				if (Property->GetOffset_ForUFunction() == Offset
					&& !Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected)
					&& Property->HasAllPropertyFlags(CPF_NativeAccessSpecifierPublic))
				{
					FString ASName = ANSI_TO_TCHAR(ASProperty->name.AddressOf());
					if (Property->GetNameCPP().Equals(ASName, ESearchCase::IgnoreCase)
						&& !GTypesExcludedFromPropertyNativization.Contains(UnrealType->GetPathName()))
					{
						if (CppForm.CppHeader.Len() != 0)
							AddHeader(CppForm.CppHeader);
						CppForm.CppType.RemoveFromEnd(TEXT("*"));
						return FString::Printf(TEXT("offsetof(%s, %s)"), *CppForm.CppType, *ASName);
					}
				}
			}
		}
	}

	// Script structs always have their properties in the same place
	bool bPotentiallyDifferent = JIT->IsTypePotentiallyDifferent(InType);
	if (!bPotentiallyDifferent)
		return FString::Printf(TEXT("(short)%d"), Offset);

	// We might be able to use a computed offset instead of a stored one
	TSharedPtr<FAngelscriptStaticJIT::FComputedTypeOffsets> ComputedOffsets = JIT->GetComputedOffsets(InType);
	if (ComputedOffsets.IsValid())
	{
		FString PropertyName = ANSI_TO_TCHAR(ASProperty->name.AddressOf());
		FAngelscriptStaticJIT::FComputedPropertyOffset* PropOffset = ComputedOffsets->Properties.Find(PropertyName);
		if (PropOffset != nullptr)
		{
			FString OffsetExpr;
			if (PropOffset->bCanHardcodeOffset)
			{
				OffsetExpr = FString::Printf(TEXT("%d"), PropOffset->HardcodedOffset);
			}
			else
			{
				check(!ComputedOffsets->ContainingSharedHeader.IsEmpty());
				File->SharedHeaderDependencies.Add(ComputedOffsets->ContainingSharedHeader);

				if (!ComputedOffsets->AdditionalHeader.IsEmpty())
					File->Headers.Add(ComputedOffsets->AdditionalHeader);
				OffsetExpr = PropOffset->ComputedOffsetVariable;
			}

			asCObjectProperty* Property = nullptr;
			auto Ref = JIT->PrecompiledData->ReferenceProperty(Offset, TypeId, &Property, &InType);
			void* PropertyId = (void*)Ref.OldReference;

#if AS_JIT_VERIFY_PROPERTY_OFFSETS
			if (!JIT->VerifiedExternalReferences.Contains(PropertyId))
			{
				FString TypeName = ANSI_TO_TCHAR(InType->name.AddressOf());
				File->Content.Add(
					FString::Printf(
						TEXT("AS_FORCE_LINK FJitVerifyPropertyOffset PVERIFY_%s_%s(0x%llx, %s);\n"),
						*GetSymbolNameForObjectType(InType),
						*PropertyName,
						Ref.OldReference, *OffsetExpr
					)
				);
				JIT->VerifiedExternalReferences.Add(PropertyId);
			}
#endif

			return OffsetExpr;
		}
	}

	asCObjectProperty* Property = nullptr;
	auto Ref = JIT->PrecompiledData->ReferenceProperty(Offset, TypeId, &Property, &InType);
	void* PropertyId = (void*)Ref.OldReference;

	FString* ExistingName = JIT->ExternalReferenceNames.Find(PropertyId);
	if (ExistingName != nullptr)
	{
		// Existing reference, see if we've already imported it into our file
		if (!File->ExternalDeclarations.Contains(PropertyId))
		{
			// Need to forward declare it in our file before we can use it
			File->ExternalDeclarations.Add(
				PropertyId,
				FString::Printf(
					TEXT("extern FJitRef_PropertyOffset %s;"),
					**ExistingName
				)
			);
		}

		return FString::Printf(TEXT("%s.Get()"), **ExistingName);
	}
	else
	{
		// Create a new reference for this function
		FString SymbolName = FString::Printf(
			TEXT("PREF_%s_%s"),
			*GetSymbolNameForObjectType(InType),
			ANSI_TO_TCHAR(Property->name.AddressOf())
		);

		SymbolName = JIT->GetUniqueSymbolName(SymbolName);

		FString Decl = FString::Printf(TEXT("AS_FORCE_LINK FJitRef_PropertyOffset %s(0x%llx);"), *SymbolName, Ref.OldReference);
		File->ExternalDeclarations.Add(PropertyId, Decl);
		JIT->ExternalReferenceNames.Add(PropertyId, SymbolName);

		return FString::Printf(TEXT("%s.Get()"), *SymbolName);
	}
}

FString FStaticJITContext::VArg(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return VAddress(asBC_SWORDARG0(BC));
	}
	else if (ArgumentIndex == 1)
	{
		return VAddress(asBC_SWORDARG1(BC));
	}
	else if (ArgumentIndex == 2)
	{
		return VAddress(asBC_SWORDARG2(BC));
	}
	else
	{
		check(false);
		return TEXT("");
	}
}

FString FStaticJITContext::VArg_Address(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return VAddress(asBC_SWORDARG0(BC));
	}
	else if (ArgumentIndex == 1)
	{
		return VAddress(asBC_SWORDARG1(BC));
	}
	else if (ArgumentIndex == 2)
	{
		return VAddress(asBC_SWORDARG2(BC));
	}
	else
	{
		check(false);
		return TEXT("");
	}
}

FString FStaticJITContext::VArg_Var(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return VVar(asBC_SWORDARG0(BC));
	}
	else if (ArgumentIndex == 1)
	{
		return VVar(asBC_SWORDARG1(BC));
	}
	else if (ArgumentIndex == 2)
	{
		return VVar(asBC_SWORDARG2(BC));
	}
	else
	{
		check(false);
		return TEXT("");
	}
}

FString FStaticJITContext::VArg_SignedVar(int32 ArgumentIndex)
{
	return FString::Printf(TEXT("((%s&)%s)"), *VArg_SignedTypename(ArgumentIndex), *VArg_Var(ArgumentIndex));
}

int32 FStaticJITContext::VArg_Offset(int32 ArgumentIndex)
{
	if (ArgumentIndex == 0)
	{
		return asBC_SWORDARG0(BC);
	}
	else if (ArgumentIndex == 1)
	{
		return asBC_SWORDARG1(BC);
	}
	else if (ArgumentIndex == 2)
	{
		return asBC_SWORDARG2(BC);
	}
	else
	{
		check(false);
		return 0;
	}
}

bool FStaticJITContext::VArg_IsConstant(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return true;
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.bIsConstant;
	}

	return false;
}

FStaticJITContext::EVariableType FStaticJITContext::VArg_Type(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return EVariableType::Pointer;
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.RepresentedType;
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		return ReturnVariableType;
	}

	check(false);
	return EVariableType::Byte;
}

FString FStaticJITContext::VArg_LiteralTypename(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return LiteralObjectType;
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.LiteralType;
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		return LiteralReturnType;
	}

	check(false);
	return TEXT("");
}

FString FStaticJITContext::VArg_SignedTypename(int32 ArgumentIndex)
{
	EVariableType Type = VArg_Type(ArgumentIndex);
	switch (Type)
	{
	case EVariableType::Byte:
		return TEXT("int8");
	case EVariableType::Word:
		return TEXT("int16");
	case EVariableType::DWord:
		return TEXT("int32");
	case EVariableType::QWord:
		return TEXT("int64");
	default:
		check(false);
		return TEXT("");
	}
}

bool FStaticJITContext::VArg_IsNumeric(int32 ArgumentIndex)
{
	EVariableType Type = VArg_Type(ArgumentIndex);
	switch (Type)
	{
	case EVariableType::Byte:
	case EVariableType::Word:
	case EVariableType::DWord:
	case EVariableType::QWord:
		return true;
	default:
		return false;
	}
}

bool FStaticJITContext::VArg_ContainsNonNullPtr(int32 ArgumentIndex)
{
	int32 VariableOffset = VArg_Offset(ArgumentIndex);
	if (bHasThisPointer && VariableOffset == 0)
		return true;

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
		{
			if (Elem.Value.bIsReference)
				return true;
			return false;
		}
	}

	return false;
}

FString FStaticJITContext::VAddress(short VariableOffset)
{
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return TEXT("(&l_This)");
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return FString::Printf(TEXT("(&%s)"), *Elem.Value.LocalName);
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		check(bReturnOnStack);
		return TEXT("(&l_ReturnValue)");
	}

	check(false);
	return TEXT("void");
}

FString FStaticJITContext::VVar(short VariableOffset)
{
	if (ScriptFunction->objectType != nullptr)
	{
		if (VariableOffset == 0)
			return TEXT("l_This");
	}

	for (auto& Elem : LocalVariables)
	{
		if (Elem.Key == VariableOffset)
			return Elem.Value.LocalName;
	}

	if (VariableOffset == (ScriptFunction->objectType != nullptr ? -2 : 0)
		&& ScriptFunction->returnType.GetTokenType() != ttVoid
		&& ScriptFunction->DoesReturnOnStack())
	{
		check(bReturnOnStack);
		return TEXT("l_ReturnValue");
	}

	check(false);
	return TEXT("void");
}

void FStaticJITContext::Push(int32 DWords, EVariableType Type, const FString& Expression, bool bAssumeNonNull)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	const ANSICHAR* StackType = (DWords == 1 ? "asDWORD" : "asQWORD");
	Line("value_assign_safe<{0}>(&l_stack[{1}], {2});", StackType, LocalStackOffset * 4, Expression);
}

void FStaticJITContext::PushVolatile(int32 DWords, EVariableType Type, const FString& Expression, bool bAssumeNonNull)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	FStackExpression& Expr = FloatingStackExpressions.Emplace_GetRef();
	Expr.DWords = DWords;
	Expr.ValueType = Type;
	Expr.OffsetOnStack = LocalStackOffset;
	Expr.Expression = Expression;
	Expr.bIsVolatile = true;
}

FStaticJITContext::FStackExpression& FStaticJITContext::PushStatic(int32 DWords)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	FStackExpression& Expr = FloatingStackExpressions.Emplace_GetRef();
	Expr.DWords = DWords;
	Expr.ValueType = DWords == 1 ? EVariableType::DWord : EVariableType::QWord;
	Expr.OffsetOnStack = LocalStackOffset;
	return Expr;
}

void FStaticJITContext::PushVArgValue(int32 DWords, int32 ArgumentIndex)
{
	LocalStackOffset -= DWords;
	check(LocalStackOffset >= 0);

	FStackExpression& Expr = FloatingStackExpressions.Emplace_GetRef();
	Expr.DWords = DWords;
	Expr.bIsVArgValue = true;
	Expr.VArgValueOffset = VArg_Offset(ArgumentIndex);
	Expr.OffsetOnStack = LocalStackOffset;
	Expr.ValueType = VArg_Type(ArgumentIndex);
	Expr.Expression = VArg_Var(ArgumentIndex);
	Expr.bGuaranteeNonNull = VArg_ContainsNonNullPtr(ArgumentIndex);
}

void FStaticJITContext::Pop(int32 DWords)
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		if (FloatingStackExpressions[i].OffsetOnStack == LocalStackOffset)
		{
			check(FloatingStackExpressions[i].DWords == DWords);
			FloatingStackExpressions.RemoveAt(i);
			break;
		}
	}

	LocalStackOffset += DWords;
	check(LocalStackOffset <= LocalStackSize);
}

void FStaticJITContext::PopMultiple(int32 TotalDWords)
{
	LocalStackOffset += TotalDWords;
	check(LocalStackOffset <= LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack < LocalStackOffset)
			FloatingStackExpressions.RemoveAt(i);
		else
			check(Expr.OffsetOnStack >= LocalStackOffset);
	}
}

void FStaticJITContext::MaterializeStackAtOffset(int32 Offset)
{
	int32 FindOffset = LocalStackOffset + Offset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			const ANSICHAR* StackType = (Expr.DWords == 1 ? "asDWORD" : "asQWORD");

			Line("// Materialize stack {0}", Expr.OffsetOnStack);
			Line("value_assign_safe<{0}>(&l_stack[{1}], {2});", StackType, Expr.OffsetOnStack * 4, Expr.Expression);

			if (Expr.bGuaranteeNonNull)
			{
				check(Expr.DWords == 2);
				Line("SCRIPT_ASSUME_NOT_NULL(*(asPWORD*)&l_stack[{0}])", Expr.OffsetOnStack * 4);
			}

			FloatingStackExpressions.RemoveAt(i);
			break;
		}
	}
}

void FStaticJITContext::MaterializeAllPushedVariables()
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.bIsVArgValue)
		{
			MaterializeStackAtOffset(Expr.OffsetOnStack - LocalStackOffset);
		}
	}
}

void FStaticJITContext::MaterializePushedVariableAtOffset(int32 VariableOffset)
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.bIsVArgValue && Expr.VArgValueOffset == VariableOffset)
		{
			MaterializeStackAtOffset(Expr.OffsetOnStack - LocalStackOffset);
		}
	}
}

void FStaticJITContext::MaterializeStackVolatiles()
{
	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.bIsVolatile)
		{
			MaterializeStackAtOffset(Expr.OffsetOnStack - LocalStackOffset);
		}
	}
}

bool FStaticJITContext::IsStackOffsetMaterialized(int32 Offset)
{
	int32 FindOffset = LocalStackOffset + Offset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			return false;
		}
	}

	return true;
}

FStaticJITContext::FStackExpression& FStaticJITContext::GetStaticAtStackOffset(int32 Offset)
{
	int32 FindOffset = LocalStackOffset + Offset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = FloatingStackExpressions.Num() - 1; i >= 0; --i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			return Expr;
		}
	}

	check(false);
	return FloatingStackExpressions.Last();
}

void FStaticJITContext::MaterializeWholeStack()
{
	if (FloatingStackExpressions.Num() == 0)
		return;

	Line("// Materialize whole stack");
	for (int32 i = 0, Count = FloatingStackExpressions.Num(); i < Count; ++i)
	{
		auto& Expr = FloatingStackExpressions[i];
		const ANSICHAR* StackType = (Expr.DWords == 1 ? "asDWORD" : "asQWORD");
		Line("value_assign_safe<{0}>(&l_stack[{1}], {2});", StackType, Expr.OffsetOnStack * 4, Expr.Expression);

		if (Expr.bGuaranteeNonNull)
		{
			check(Expr.DWords == 2);
			Line("SCRIPT_ASSUME_NOT_NULL(*(asPWORD*)&l_stack[{0}])", Expr.OffsetOnStack * 4);
		}
	}

	FloatingStackExpressions.Empty();
}

void FStaticJITContext::MaterializeValueRegister(bool bAllowIndeterminate)
{
	switch (ValueRegisterState)
	{
	case EValueRegisterState::ByteRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = (asQWORD)l_byteRegister;");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::DWordRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = (asQWORD)l_dwordRegister;");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::FloatRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = value_extend_safe<asQWORD>(l_floatRegister);");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::DoubleRegister:
		Line("// Materialize Value Register");
		Line("l_valueRegister = value_as_safe<asQWORD>(l_doubleRegister);");
		ValueRegisterState = EValueRegisterState::ValueRegister;
	break;
	case EValueRegisterState::ValueRegister:
	break;
	case EValueRegisterState::Indeterminate:
		check(bAllowIndeterminate);
	break;
	default:
		check(false);
	break;
	}
}

FString FStaticJITContext::GetValueRegisterForUnsignedComparisonMaybeMaterialize()
{
	switch (ValueRegisterState)
	{
	case EValueRegisterState::ByteRegister:
		return TEXT("l_byteRegister");
	break;
	case EValueRegisterState::DWordRegister:
		return TEXT("l_dwordRegister");
	break;
	case EValueRegisterState::FloatRegister:
	case EValueRegisterState::DoubleRegister:
		MaterializeValueRegister();
		return TEXT("l_valueRegister");
	break;
	case EValueRegisterState::ValueRegister:
		return TEXT("l_valueRegister");
	break;
	default:
		check(false);
		return TEXT("");
	break;
	}
}

FString FStaticJITContext::GetValueRegisterForSignedComparisonMaybeMaterialize()
{
	switch (ValueRegisterState)
	{
	case EValueRegisterState::ByteRegister:
		return TEXT("((int8&)l_byteRegister)");
	break;
	case EValueRegisterState::DWordRegister:
		return TEXT("((int32&)l_dwordRegister)");
	break;
	case EValueRegisterState::FloatRegister:
	case EValueRegisterState::DoubleRegister:
		MaterializeValueRegister();
		return TEXT("((int64&)l_valueRegister)");
	break;
	case EValueRegisterState::ValueRegister:
		return TEXT("((int64&)l_valueRegister)");
	break;
	default:
		check(false);
		return TEXT("");
	break;
	}
}

FString FStaticJITContext::StackAddress_At(int32 DWordOffset)
{
	check(IsStackOffsetMaterialized(DWordOffset));
	return FString::Printf(TEXT("&l_stack[%d]"), (LocalStackOffset + DWordOffset) * 4);
}

FString FStaticJITContext::StackValue_At(int32 DWordOffset, int32 DWordSize)
{
	int32 FindOffset = LocalStackOffset + DWordOffset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = 0, Count = FloatingStackExpressions.Num(); i < Count; ++i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			check(DWordSize == Expr.DWords);
			return FString::Printf(TEXT("(%s)"), *Expr.Expression);
		}
	}

	if (DWordSize == 1)
		return FString::Printf(TEXT("((asDWORD&)l_stack[%d])"), FindOffset * 4);
	else
		return FString::Printf(TEXT("((asQWORD&)l_stack[%d])"), FindOffset * 4);
}

bool FStaticJITContext::IsStackValueNonNull(int32 DWordOffset)
{
	int32 FindOffset = LocalStackOffset + DWordOffset;
	check(FindOffset >= 0);
	check(FindOffset < LocalStackSize);

	for (int32 i = 0, Count = FloatingStackExpressions.Num(); i < Count; ++i)
	{
		auto& Expr = FloatingStackExpressions[i];
		if (Expr.OffsetOnStack == FindOffset)
		{
			return Expr.bGuaranteeNonNull;
		}
	}

	return false;
}

void FStaticJITContext::AddStateVar(const FString& InType, const FString& InName)
{
	for (FStateVar& Var : StateVars)
	{
		if (Var.Name == InName)
		{
			check(Var.Type == InType);
			return;
		}
	}

	StateVars.Add({InType, InName});
}

class asCScriptEngine* FStaticJITContext::GetEngine()
{
	return ScriptFunction->engine;
}

bool FAngelscriptStaticJIT::IsTypePotentiallyDifferent(asITypeInfo* TypeInfo)
{
	if ((TypeInfo->GetFlags() & (asOBJ_REF | asOBJ_VALUE)) == 0)
		return false;

	auto* ObjectType = (asCObjectType*)TypeInfo;
	asQWORD Flags = ObjectType->GetFlags();

	// Assumption that any template containers are never different
	if ((Flags & asOBJ_TEMPLATE) != 0)
	{
		if ((ObjectType->templateBaseType->flags & asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) == 0)
			return false;
		else
			return true;
	}

	if ((Flags & asOBJ_SCRIPT_OBJECT) == 0)
	{
		if ((Flags & asOBJ_REF) != 0)
		{
			// C++ Reference types can always be different
			return true;
		}
		else
		{
			bool* ExistingCheck = CachedPotentiallyDifferent.Find(ObjectType);
			if (ExistingCheck != nullptr)
				return *ExistingCheck;

			FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
			UStruct* UnrealStruct = Usage.GetUnrealStruct();

			bool bPotentialDifference = false;
			if (UnrealStruct != nullptr)
			{
				// Hardcode some base struct types that we know won't change size
				static TSet<UStruct*> GuaranteedNotDifferentStructs = {
					TBaseStructure<FVector>::Get(),
					TBaseStructure<FRotator>::Get(),
					TBaseStructure<FQuat>::Get(),
					TBaseStructure<FTransform>::Get(),
					TBaseStructure<FVector2D>::Get(),
					TBaseStructure<FLinearColor>::Get(),
					TBaseStructure<FVector4>::Get(),
					TBaseStructure<FColor>::Get(),
					TBaseStructure<FFloatInterval>::Get(),
					TBaseStructure<FFloatRange>::Get(),
					TBaseStructure<FGuid>::Get(),
					TBaseStructure<FIntPoint>::Get(),
					TBaseStructure<FIntVector>::Get(),
					TBaseStructure<FIntVector4>::Get(),
					TVariantStructure<FVector2f>::Get(),
					TVariantStructure<FVector3f>::Get(),
					TVariantStructure<FVector4f>::Get(),
					TVariantStructure<FRotator3f>::Get(),
					TVariantStructure<FQuat4f>::Get(),
					TVariantStructure<FTransform3f>::Get(),
				};

				if (!GuaranteedNotDifferentStructs.Contains(UnrealStruct))
				{
					bPotentialDifference = true;
				}
			}

			CachedPotentiallyDifferent.Add(ObjectType, bPotentialDifference);
			return bPotentialDifference;
		}
	}
	else
	{
		if ((Flags & asOBJ_REF) != 0)
		{
			// Angelscript reference types can always be different
			return true;
		}
		else
		{
			// Recursively classify if the angelscript value type can be different
			bool* ExistingCheck = CachedPotentiallyDifferent.Find(ObjectType);
			if (ExistingCheck != nullptr)
				return *ExistingCheck;

			bool bPotentialDifference = false;
			for (int32 i = 0, Count = ObjectType->properties.GetLength(); i < Count; ++i)
			{
				auto& DataType = ObjectType->properties[i]->type;
				if (DataType.IsObject() && !DataType.IsReference() && !DataType.IsReferenceType())
				{
					if (IsTypePotentiallyDifferent((asCObjectType*)DataType.GetTypeInfo()))
					{
						bPotentialDifference = true;
						break;
					}
				}
			}

			CachedPotentiallyDifferent.Add(ObjectType, bPotentialDifference);
			return bPotentialDifference;
		}
	}
}


TSharedPtr<FAngelscriptStaticJIT::FSharedHeader> FAngelscriptStaticJIT::GetActiveSharedHeader()
{
	if (ActiveSharedHeader.IsValid())
		return ActiveSharedHeader;
	if (LatestUsedSharedHeaderName.IsEmpty())
		LatestUsedSharedHeaderName = TEXT("GlobalDeclarations");
	return GetSharedHeader(LatestUsedSharedHeaderName);
}

TSharedPtr<FAngelscriptStaticJIT::FSharedHeader> FAngelscriptStaticJIT::GetSharedHeader(const FString& ModuleName)
{
	LatestUsedSharedHeaderName = ModuleName;

	FString HeaderFilename = ModuleName;
	int32 DotIndex;
	if (HeaderFilename.FindLastChar((TCHAR)'.', DotIndex))
		HeaderFilename = HeaderFilename.Mid(DotIndex + 1);
	SanitizeSymbolName(HeaderFilename);
	HeaderFilename = FString::Printf(TEXT("SHARED_%s.as.jit.h"), *HeaderFilename);

	TSharedPtr<FSharedHeader>* Existing = SharedHeaders.Find(HeaderFilename);
	if (Existing != nullptr)
		return *Existing;

	TSharedPtr<FSharedHeader> NewHeader = MakeShared<FSharedHeader>();
	SharedHeaders.Add(HeaderFilename, NewHeader);
	NewHeader->Filename = HeaderFilename;
	return NewHeader;
}

TSharedPtr<FAngelscriptStaticJIT::FComputedTypeOffsets> FAngelscriptStaticJIT::GetComputedOffsets(asCObjectType* ObjectType)
{
	if (ObjectType == nullptr)
		return nullptr;

	auto* Existing = ComputedOffsets.Find(ObjectType);
	if (Existing != nullptr)
		return *Existing;

	TSharedPtr<FComputedTypeOffsets> Offsets = MakeShared<FComputedTypeOffsets>();
	ComputedOffsets.Add(ObjectType, Offsets);

	if ((ObjectType->flags & asOBJ_SCRIPT_OBJECT) == 0)
	{
		if (IsTypePotentiallyDifferent(ObjectType))
		{
			// If we have a native type available, we can still compute the size
			FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(ObjectType->GetTypeId());
			if (Usage.IsValid())
			{
				FAngelscriptType::FCppForm CppForm;
				Usage.GetCppForm(CppForm);

				FString CppType = CppForm.CppType;
				if (CppType.IsEmpty() && !CppForm.CppGenericType.IsEmpty())
					CppType = CppForm.CppGenericType;

				if (!CppType.IsEmpty())
				{
					CppType.RemoveFromEnd(TEXT("*"));

					Offsets->bHasComputedSize = true;
					Offsets->ComputedSizeVariable = FString::Printf(TEXT("sizeof(%s)"), *CppType);
					Offsets->ComputedAlignmentVariable = FString::Printf(TEXT("alignof(%s)"), *CppType);
					Offsets->AdditionalHeader = CppForm.CppHeader;
				}
				else
				{
					if (ObjectType->templateBaseType != nullptr && (ObjectType->templateBaseType->flags & asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE) != 0)
					{
						auto subType = ObjectType->templateSubTypes[0];
						if (subType.IsPrimitive() || subType.IsEnumType())
						{
							Offsets->bCanHardcodeSize = true;
							Offsets->HardcodedAlignment = FMath::Max(ObjectType->templateBaseType->alignment, subType.GetAlignment());
							Offsets->HardcodedSize = Align(ObjectType->templateBaseType->size + subType.GetSizeInMemoryBytes(), Offsets->HardcodedAlignment);
						}
						else if (subType.IsReferenceType())
						{
							Offsets->bCanHardcodeSize = true;
							Offsets->HardcodedAlignment = FMath::Max((SIZE_T)ObjectType->templateBaseType->alignment, alignof(void*));
							Offsets->HardcodedSize = Align(ObjectType->size + sizeof(void*), Offsets->HardcodedAlignment);
						}
						else if (subType.IsObject())
						{
							asCObjectType* SubObjectType = (asCObjectType*)subType.GetTypeInfo();
							if (IsTypePotentiallyDifferent(SubObjectType))
							{
								auto SubTypeOffsets = GetComputedOffsets(SubObjectType);
								if (SubTypeOffsets.IsValid())
								{
									if (SubTypeOffsets->bCanHardcodeSize)
									{
										Offsets->bCanHardcodeSize = true;
										Offsets->HardcodedAlignment = FMath::Max((SIZE_T)ObjectType->templateBaseType->alignment, SubTypeOffsets->HardcodedAlignment);
										Offsets->HardcodedSize = Align(ObjectType->templateBaseType->size + SubTypeOffsets->HardcodedSize, Offsets->HardcodedAlignment);
									}
									else if (SubTypeOffsets->bHasComputedSize)
									{
										Offsets->bHasComputedSize = true;
										Offsets->ComputedSizeVariable = FString::Printf(TEXT("Align(%s + %d, AlignmentMax(%d, %s))"),
											*SubTypeOffsets->ComputedSizeVariable,
											ObjectType->templateBaseType->size,
											ObjectType->templateBaseType->alignment,
											*SubTypeOffsets->ComputedAlignmentVariable
										);
										Offsets->ComputedAlignmentVariable = FString::Printf(TEXT("AlignmentMax(%d, %s)"),
											ObjectType->templateBaseType->alignment,
											*SubTypeOffsets->ComputedAlignmentVariable);
										Offsets->AdditionalHeader = SubTypeOffsets->AdditionalHeader;
										Offsets->ContainingSharedHeader = SubTypeOffsets->ContainingSharedHeader;
									}
								}
							}
							else
							{
								Offsets->bCanHardcodeSize = true;
								Offsets->HardcodedAlignment = FMath::Max(ObjectType->templateBaseType->alignment, SubObjectType->alignment);
								Offsets->HardcodedSize = Align(ObjectType->templateBaseType->size + SubObjectType->size, Offsets->HardcodedAlignment);
							}
						}
					}
				}
			}
		}
		else
		{
			// We can hardcode the size and alignment of this C++ type
			Offsets->bCanHardcodeSize = true;
			Offsets->HardcodedSize = ObjectType->size;
			Offsets->HardcodedAlignment = ObjectType->alignment;
		}
	}
	else
	{
		FString TypeName = ANSI_TO_TCHAR(ObjectType->name.AddressOf());

		TSharedPtr<FSharedHeader> Header = ActiveSharedHeader;
		if (!Header.IsValid())
			Header = GetSharedHeader(TypeName);

		TGuardValue ScopeActivateHeader(ActiveSharedHeader, Header);
		int PropertyBaseOffset = 0;

		Offsets->ContainingSharedHeader = Header->Filename;

		// Script objects should recursively classify their properties
		TSharedPtr<FComputedTypeOffsets> ParentOffsets;
		if (ObjectType->derivedFrom != nullptr)
		{
			ParentOffsets = GetComputedOffsets(ObjectType->derivedFrom);
			PropertyBaseOffset = ObjectType->derivedFrom->size;
			if (!ParentOffsets.IsValid())
				return Offsets;
		}
		else if (ObjectType->shadowType != nullptr)
		{
			ParentOffsets = GetComputedOffsets((asCObjectType*)ObjectType->shadowType);
			PropertyBaseOffset = ObjectType->basePropertyOffset;
			if (!ParentOffsets.IsValid())
				return Offsets;
		}

		// Go through all properties in this class to compute the offsets
		bool bValidToHardcode = true;
		SIZE_T HardcodedSize = 0;
		SIZE_T HardcodedAlignment = 0;
		FString PreviousSizeVariable;
		FString PreviousAlignmentVariable;

		// Script structs are always at least alignment 8
		if ((ObjectType->flags & asOBJ_VALUE) != 0 || (ObjectType->flags & asOBJ_SCRIPT_OBJECT) != 0)
		{
			HardcodedAlignment = 8;
		}

		bool bPreviousPropertyAllowsHardcode = true;
		SIZE_T PreviousPropertyHardcodedSize = 0;
		FString PreviousPropertyAlignmentExpression;
		FString PreviousPropertySizeExpression;

		if (ParentOffsets.IsValid())
		{
			if (ParentOffsets->bCanHardcodeSize)
			{
				bValidToHardcode = true;
				HardcodedSize = ParentOffsets->HardcodedSize;
				HardcodedAlignment = ParentOffsets->HardcodedAlignment;
			}
			else if (ParentOffsets->bHasComputedSize)
			{
				bValidToHardcode = false;
				PreviousSizeVariable = ParentOffsets->ComputedSizeVariable;
				PreviousAlignmentVariable = ParentOffsets->ComputedAlignmentVariable;

				if (!ParentOffsets->ContainingSharedHeader.IsEmpty() && ParentOffsets->ContainingSharedHeader != Header->Filename)
				{
					Header->SharedHeaderDependencies.Add(ParentOffsets->ContainingSharedHeader);

					auto OtherHeader = SharedHeaders[ParentOffsets->ContainingSharedHeader];
					OtherHeader->bHasDependentSharedHeaders = true;
				}

				if (!ParentOffsets->AdditionalHeader.IsEmpty())
				{
					Header->Includes.Add(ParentOffsets->AdditionalHeader);
				}
			}
			else
			{
				return Offsets;
			}
		}

		bool bHadUnknownProperty = false;
		for (int i = 0, Count = ObjectType->properties.GetLength(); i < Count; ++i)
		{
			asCObjectProperty* Property = ObjectType->properties[i];
			if (Property == nullptr)
				continue;

			// Skip inherited properties
			if (Property->byteOffset < PropertyBaseOffset)
				continue;

			// Classify what information we have about this property
			bool bCanHardcodeProperty = false;
			SIZE_T PropertyHardcodedSize = 0;
			SIZE_T PropertyHardcodedAlignment = 0;

			bool bCanComputeProperty = false;
			FString PropertyComputedAlignment;
			FString PropertyComputedSize;

			if (Property->type.IsPrimitive() || Property->type.IsEnumType())
			{
				bCanHardcodeProperty = true;
				PropertyHardcodedSize = Property->type.GetSizeInMemoryBytes();
				PropertyHardcodedAlignment = Property->type.GetAlignment();
			}
			else if (Property->type.IsReferenceType())
			{
				bCanHardcodeProperty = true;
				PropertyHardcodedSize = sizeof(void*);
				PropertyHardcodedAlignment = alignof(void*);
			}
			else if (auto* PropertyObjectType = CastToObjectType(Property->type.GetTypeInfo()))
			{
				if (IsTypePotentiallyDifferent(PropertyObjectType))
				{
					// If we have a native type available, we can still compute the size
					FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(PropertyObjectType->GetTypeId());
					if (Usage.IsValid())
					{
						FAngelscriptType::FCppForm CppForm;
						Usage.GetCppForm(CppForm);

						FString CppType = CppForm.CppType;
						if (CppType.IsEmpty() && !CppForm.CppGenericType.IsEmpty())
							CppType = CppForm.CppGenericType;

						if (!CppType.IsEmpty())
						{
							CppType.RemoveFromEnd(TEXT("*"));
							if (!CppForm.CppHeader.IsEmpty())
								Header->Includes.Add(CppForm.CppHeader);

							bCanComputeProperty = true;
							PropertyComputedSize = FString::Printf(TEXT("sizeof(%s)"), *CppType);
							PropertyComputedAlignment = FString::Printf(TEXT("alignof(%s)"), *CppType);
						}
					}

					if (!bCanComputeProperty)
					{
						// Try to recursively compute the size of script structs if we can
						TSharedPtr<FComputedTypeOffsets> MemberOffsets = GetComputedOffsets(PropertyObjectType);
						if (MemberOffsets.IsValid())
						{
							if (MemberOffsets->bCanHardcodeSize)
							{
								bCanHardcodeProperty = true;
								PropertyHardcodedSize = MemberOffsets->HardcodedSize;
								PropertyHardcodedAlignment = MemberOffsets->HardcodedAlignment;
							}
							else if (MemberOffsets->bHasComputedSize)
							{
								if (!MemberOffsets->ContainingSharedHeader.IsEmpty() && MemberOffsets->ContainingSharedHeader != Header->Filename)
								{
									Header->SharedHeaderDependencies.Add(MemberOffsets->ContainingSharedHeader);

									auto OtherHeader = SharedHeaders[MemberOffsets->ContainingSharedHeader];
									OtherHeader->bHasDependentSharedHeaders = true;
								}

								if (!MemberOffsets->AdditionalHeader.IsEmpty())
									Header->Includes.Add(MemberOffsets->AdditionalHeader);

								bCanComputeProperty = true;
								PropertyComputedSize = MemberOffsets->ComputedSizeVariable;
								PropertyComputedAlignment = MemberOffsets->ComputedAlignmentVariable;
							}
						}
					}
				}
				else
				{
					bCanHardcodeProperty = true;
					PropertyHardcodedSize = PropertyObjectType->size;
					PropertyHardcodedAlignment = PropertyObjectType->alignment;
				}
			}

			if (!bCanHardcodeProperty && !bCanComputeProperty)
			{
				bHadUnknownProperty = true;
				break;
			}

			FString PropertyName = ANSI_TO_TCHAR(Property->name.AddressOf());
			FComputedPropertyOffset& PropertyOffset = Offsets->Properties.Emplace(PropertyName);
			if (bValidToHardcode && bCanHardcodeProperty)
			{
				HardcodedSize = Align(HardcodedSize + PreviousPropertyHardcodedSize, PropertyHardcodedAlignment);
				HardcodedAlignment = FMath::Max(HardcodedAlignment, PropertyHardcodedAlignment);

				check(HardcodedSize == Property->byteOffset);

				PropertyOffset.bCanHardcodeOffset = true;
				PropertyOffset.HardcodedOffset = HardcodedSize;
				PropertyOffset.HardcodedAlignment = HardcodedAlignment;

				bPreviousPropertyAllowsHardcode = true;
				PreviousPropertyHardcodedSize = PropertyHardcodedSize;
			}
			else
			{
				FString OffsetName = GetUniqueSymbolName(FString::Printf(TEXT("POFFSET_%s_%s"), *TypeName, *PropertyName));
				FString AlignmentName = GetUniqueSymbolName(FString::Printf(TEXT("PALIGN_%s_%s"), *TypeName, *PropertyName));

				if (bValidToHardcode)
				{
					check(!PropertyComputedSize.IsEmpty() && !PropertyComputedAlignment.IsEmpty());
					check(bPreviousPropertyAllowsHardcode);

					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%d + %d, %s);\n"), *OffsetName, HardcodedSize, PreviousPropertyHardcodedSize, *PropertyComputedAlignment));
					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = AlignmentMax(%d, %s);\n"), *AlignmentName, HardcodedAlignment, *PropertyComputedAlignment));
				}
				else if (bCanHardcodeProperty)
				{
					if (bPreviousPropertyAllowsHardcode)
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %d, %d);\n"), *OffsetName, *PreviousSizeVariable, PreviousPropertyHardcodedSize, PropertyHardcodedAlignment));
					else
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %s, %d);\n"), *OffsetName, *PreviousSizeVariable, *PreviousPropertySizeExpression, PropertyHardcodedAlignment));

					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = AlignmentMax(%s, %d);\n"), *AlignmentName, *PreviousAlignmentVariable, PropertyHardcodedAlignment));
				}
				else
				{
					if (bPreviousPropertyAllowsHardcode)
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %d, %s);\n"), *OffsetName, *PreviousSizeVariable, PreviousPropertyHardcodedSize, *PropertyComputedAlignment));
					else
						Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %s, %s);\n"), *OffsetName, *PreviousSizeVariable, *PreviousPropertySizeExpression, *PropertyComputedAlignment));

					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = AlignmentMax(%s, %s);\n"), *AlignmentName, *PreviousAlignmentVariable, *PropertyComputedAlignment));
				}

				PropertyOffset.bCanHardcodeOffset = false;
				PropertyOffset.ComputedOffsetVariable = OffsetName;
				PropertyOffset.ComputedAlignmentVariable = AlignmentName;

				PreviousSizeVariable = OffsetName;
				PreviousAlignmentVariable = AlignmentName;

				bPreviousPropertyAllowsHardcode = bCanHardcodeProperty;
				PreviousPropertyAlignmentExpression = PropertyComputedAlignment;
				PreviousPropertySizeExpression = PropertyComputedSize;
				PreviousPropertyHardcodedSize = PropertyHardcodedSize;

				bValidToHardcode = false;
			}
		}

		if (!bHadUnknownProperty)
		{
			if (bValidToHardcode)
			{
				Offsets->bCanHardcodeSize = true;
				Offsets->HardcodedSize = Align(HardcodedSize, HardcodedAlignment);
				Offsets->HardcodedAlignment = HardcodedAlignment;

				check(Offsets->HardcodedSize == ObjectType->size);
				check(Offsets->HardcodedAlignment == ObjectType->alignment);
			}
			else
			{
				FString SizeName = GetUniqueSymbolName(FString::Printf(TEXT("TSIZE_%s"), *TypeName));
				FString AlignmentName = GetUniqueSymbolName(FString::Printf(TEXT("TALIGN_%s"), *TypeName));

				Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = %s;\n"), *AlignmentName, *PreviousAlignmentVariable));

				if (bPreviousPropertyAllowsHardcode)
					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %d, %s);\n"), *SizeName, *PreviousSizeVariable, PreviousPropertyHardcodedSize, *AlignmentName));
				else
					Header->Content.Add(FString::Printf(TEXT("constexpr SIZE_T %s = Align(%s + %s, %s);\n"), *SizeName, *PreviousSizeVariable, *PreviousPropertySizeExpression, *AlignmentName));

				Offsets->bHasComputedSize = true;
				Offsets->ComputedSizeVariable = SizeName;
				Offsets->ComputedAlignmentVariable = AlignmentName;
			}
		}
	}

	return Offsets;
}

void FAngelscriptStaticJIT::GenerateCppCode(asIScriptFunction* ScriptFunction, FGenerateFunction& Generate)
{
	if (PrecompiledData == nullptr)
	{
		ensure(false);
		return;
	}

	uint32 FunctionId = PrecompiledData->CreateFunctionId(ScriptFunction);
	FJITFile& File = GetFile(ScriptFunction->GetModule());

	FString Code;

	asUINT BytecodeLength;

	FStaticJITContext Context;
	Context.FunctionId = FunctionId;
	Context.ScriptFunction = (asCScriptFunction*)ScriptFunction;
	Context.File = &File;
	Context.FunctionName = Generate.FunctionSymbolName;
	Context.JIT = this;
	Context.BC = ScriptFunction->GetByteCode(&BytecodeLength);
	Context.BC_FunctionStart = Context.BC;
	Context.BC_End = Context.BC + BytecodeLength;

	// Lookup all instructions in the bytecode
	while (Context.BC < Context.BC_End)
	{
		asEBCInstr Instr = Context.GetInstr();
		FAngelscriptBytecode& Bytecode = FAngelscriptBytecode::GetBytecode(Instr);

		FStaticJITContext::FInstruction& NewInstr = Context.Instructions.Emplace_GetRef();
		NewInstr.BC = Context.BC;
		NewInstr.Bytecode = &Bytecode;

		Context.AdvanceBC();
	}

	// Run the pre-pass to add labels where needed
	for (int32 i = 0, Count = Context.Instructions.Num(); i < Count; ++i)
	{
		Context.CurrentInstructionIndex = i;
		Context.BC = Context.Instructions[i].BC;
		Context.Instructions[i].Bytecode->PrePass(Context);
	}

	// Handle stack offsets for all labels
	Context.LocalStackSize = (Context.ScriptFunction->scriptData->stackNeeded - Context.ScriptFunction->scriptData->variableSpace);
	Context.LocalStackOffset = Context.LocalStackSize;

	if (Context.Instructions.Num() != 0)
	{
		TArray<int32> PathsToCheck;
		PathsToCheck.Add(0);
		Context.Instructions[0].StackOffsetBeforeInstr = Context.LocalStackSize;
		Context.Instructions[0].bMarked = true;

		while (PathsToCheck.Num() > 0)
		{
			int32 CurPath = PathsToCheck[0];
			PathsToCheck.RemoveAt(0);

			FStaticJITContext::FInstruction& StartInstr = Context.Instructions[CurPath];
			check(StartInstr.bMarked);

			int32 StackOffset = StartInstr.StackOffsetBeforeInstr;
			StartInstr.bMarked = false;

			while (CurPath < Context.Instructions.Num())
			{
				FStaticJITContext::FInstruction& Instr = Context.Instructions[CurPath];
				if (Instr.bMarked)
				{
					check(Instr.StackOffsetBeforeInstr == StackOffset);
					break;
				}

				Instr.StackOffsetBeforeInstr = StackOffset;
				Instr.bMarked = true;

				Context.CurrentInstructionIndex = CurPath;
				Context.BC = Instr.BC;

				StackOffset -= Instr.Bytecode->GetStackSizeChange(Context);
				check(StackOffset >= 0);
				check(StackOffset <= Context.LocalStackSize);

				if (Instr.JumpTargets.Num() != 0)
				{
					for (int32 JumpIndex : Instr.JumpTargets)
					{
						auto& JumpInstr = Context.Instructions[JumpIndex];
						if (JumpInstr.bMarked)
						{
							check(JumpInstr.StackOffsetBeforeInstr == StackOffset);
							continue;
						}

						JumpInstr.bMarked = true;
						JumpInstr.StackOffsetBeforeInstr = StackOffset;
						PathsToCheck.Add(JumpIndex);
					}

					if (!Instr.bIsConditionalJump)
						break;
				}

				CurPath += 1;
			}
		}
	}

	// Reset the context after the pre-pass
	Context.BC = Context.BC_FunctionStart;

	// Start a new function 
	Context.GenerateNewFunction(ScriptFunction);
	Generate.FunctionDeclaration = Context.FunctionDecl;

	for (int32 i = 0, Count = Context.Instructions.Num(); i < Count; ++i)
	{
		Context.CurrentInstructionIndex = i;
		Context.BC = Context.Instructions[i].BC;

		asEBCInstr Instr = Context.GetInstr();

		FAngelscriptBytecode& Bytecode = FAngelscriptBytecode::GetBytecode(Instr);
		Context.NewInstruction();
		Context.UsageFlags |= Bytecode.GetUsageFlags(Context);

		Bytecode.WriteDebugComment(Context);
		bool bImplemented = Bytecode.Implement(Context);
		check(bImplemented);
	}

	// Add blocks for any exception cleanup we need to do
	if (Context.ExceptionCleanupLabels.Num() != 0)
	{
		Context.Line("SCRIPT_ASSUME(false);\n");

		for (FStaticJITContext::FExceptionCleanup& Cleanup : Context.ExceptionCleanupLabels)
		{
			FString Variables;
			for (int Pos : Cleanup.Positions)
				Variables += FString::Printf(TEXT("%d, "), Pos);

			Context.Line("{0}:", Cleanup.Label);
			Context.Line("// Exception cleanup for variables {0}", Variables);

			if (Cleanup.bCalledAddNullptrException)
			{
				if (Cleanup.bCalledWithoutAddNullptrException)
				{
					// Can be called both with or without an existing exception
					Context.Line("if (!Execution.bExceptionThrown) { SCRIPT_NULL_POINTER_EXCEPTION(); }");
				}
				else
				{
					// Always adds a nullpointer exception
					Context.Line("SCRIPT_NULL_POINTER_EXCEPTION();");
				}
			}

			for (int i = 0, Count = Cleanup.Positions.Num(); i < Count; ++i)
			{
				int Pos = Cleanup.Positions[i];
				asCObjectType* ObjectType = Cleanup.Types[i];

				FDestructorCall DestructorCall(ObjectType);
				DestructorCall.CallDestroy(Context, Context.VAddress(Pos));
			}

			Context.ReturnEmptyValue();
		}
	}

	Context.WriteOutFunction();
}

void FAngelscriptStaticJIT::DetectScriptType(asCObjectType* ObjectType)
{
	// For each virtual method we override from the base type, mark the base type's functions
	// as not being final anymore.
	for (int32 i = 0, Count = ObjectType->virtualFunctionTable.GetLength(); i < Count; ++i)
	{
		auto* Function = ObjectType->virtualFunctionTable[i];
		if (Function->vfTableIdx == -1)
			continue;

		// We inherited this function from a base class and didn't override it, we don't need to do anything
		if (Function->objectType != ObjectType)
			continue;
		
		auto* BaseType = ObjectType->derivedFrom;
		while (BaseType != nullptr)
		{
			if (Function->vfTableIdx < (int)BaseType->virtualFunctionTable.GetLength())
			{
				asCScriptFunction* BaseFunction = BaseType->virtualFunctionTable[Function->vfTableIdx];
				FunctionsWithVirtualOverrides.Add(BaseFunction);

				BaseType = BaseFunction->objectType->derivedFrom;
			}
			else
			{
				break;
			}
		}
	}
}

asCScriptFunction* FAngelscriptStaticJIT::DevirtualizeFunction(asCScriptFunction* VirtualFunction)
{
	if (VirtualFunction->vfTableIdx != -1)
	{
		if (bAllowDevirtualize && !FunctionsWithVirtualOverrides.Contains(VirtualFunction))
		{
			return VirtualFunction;
		}
	}
	else if (VirtualFunction->funcType == asFUNC_IMPORTED)
	{
		if (bAllowDevirtualize)
		{
			// Devirtualize to the function that is bound currently
			int BoundId = Engine->importedFunctions[(int)VirtualFunction->GetId() & ~FUNC_IMPORTED]->boundFunctionId;
			if (BoundId != -1)
				return Engine->scriptFunctions[BoundId];
		}
	}
	else if (VirtualFunction->funcType == asFUNC_SCRIPT)
	{
		return VirtualFunction;
	}

	return nullptr;
}

bool FAngelscriptStaticJIT::IsFunctionAlwaysJIT(asCScriptFunction* VirtualFunction)
{
	if (VirtualFunction->vfTableIdx == -1)
		return FunctionsToGenerate.Contains(VirtualFunction);

	if (bAllowComprehensiveJIT)
		return true;

	return false;
}

void FAngelscriptStaticJIT::AnalyzeScriptFunction(asCScriptFunction* ScriptFunction, FGenerateFunction& Generate)
{
	FJITFile& File = GetFile(ScriptFunction->GetModule());
	Generate.FunctionSymbolName = GetUniqueFunctionName(File, ScriptFunction, 0);

	// If nothing overrides this method, we mark it as final, this allows some runtime
	// devirtualization optimizations to happen when loading from precompiled scripts.
	if (!FunctionsWithVirtualOverrides.Contains(ScriptFunction))
	{
		ScriptFunction->traits.SetTrait(asTRAIT_FINAL, true);
	}
	else
	{
		check(!ScriptFunction->traits.GetTrait(asTRAIT_FINAL));
	}
}

void FAngelscriptStaticJIT::WriteOutputCode(TMap<FString, FString>* OutGeneratedFiles)
{
	Engine = (asCScriptEngine*)FAngelscriptEngine::Get().Engine;
	bAllowDevirtualize = true;
	bAllowComprehensiveJIT = true;

	// Detect all script types in the engine
	for (int32 i = 0, Count = Engine->scriptModules.GetLength(); i < Count; ++i)
	{
		auto* Module = Engine->scriptModules[i];
		if (Module == nullptr)
			continue;
		for (int32 j = 0, jCount = Module->classTypes.GetLength(); j < jCount; ++j)
		{
			asCObjectType* objType = Module->classTypes[j];
			if (objType == nullptr)
				continue;
			DetectScriptType(objType);
		}
	}

	// Pre-Analyze the functions we're going to generate
	for (auto& Elem : FunctionsToGenerate)
		AnalyzeScriptFunction(Elem.Key, Elem.Value);

	// Run all scriptfunctions through the generate process
	for (auto& Elem : FunctionsToGenerate)
		GenerateCppCode(Elem.Key, Elem.Value);

	FString GenDir;
	IFileManager* FileManager = nullptr;
	TArray<FString> PreviousFiles;
	if (OutGeneratedFiles == nullptr)
	{
		GenDir = FPaths::RootDir() / TEXT("AS_JITTED_CODE");

		// Delete and recreate the folder so we don't keep any old code
		FileManager = &IFileManager::Get();
		FileManager->MakeDirectory(*GenDir, true);
		FileManager->FindFiles(PreviousFiles, *GenDir);
	}

	TSet<FString> CurrentFiles;

	int32 CombinedFileIndex = 0;
	int32 LinesInCombinedFile = 0;

	TArray<FString> FilesToCombine;

	// Mark which shader headers are used by more than one module
	for(auto ModuleElem : JITFiles)
	{
		for (const FString& HeaderName : ModuleElem.Value->SharedHeaderDependencies)
		{
			auto Header = SharedHeaders[HeaderName];
			Header->DependentModules.Add(ModuleElem.Key);
		}
	}

	// Write the shared header files to disk
	for (auto& HeaderElem : SharedHeaders)
	{
		FSharedHeader& Header = *HeaderElem.Value.Get();
		if (Header.Content.IsEmpty())
			continue;
		if (Header.DependentModules.Num() == 0)
			continue;

		// If there's only one dependency, we can add the header straight into the module's code file
		if (Header.DependentModules.Num() == 1 && !Header.bHasDependentSharedHeaders)
		{
			for (asIScriptModule* Dependent : Header.DependentModules)
			{
				TSharedPtr<FJITFile> DependentFile = JITFiles[Dependent];
				for (auto& Include : Header.Includes)
					DependentFile->Headers.Add(Include);
				for (auto& SharedHeaderDependency : Header.SharedHeaderDependencies)
					DependentFile->SharedHeaderDependencies.Add(SharedHeaderDependency);

				DependentFile->Content.Insert(Header.Content, 0);
				DependentFile->SharedHeaderDependencies.Remove(HeaderElem.Key);
			}

			continue;
		}

		FString FullContent = TEXT("#pragma once\n\n");
		for (const FString& Content : Header.Includes)
			FullContent.Append(Content+TEXT("\n"));
		for (const FString& Content : Header.SharedHeaderDependencies)
			FullContent.Append(FString::Printf(TEXT("#include \"%s\"\n"), *Content));
		FullContent += TEXT("\n");
		for (const FString& Content : Header.Content)
			FullContent.Append(Content);
		FullContent.Append(TEXT("\n\n"));

		CurrentFiles.Add(Header.Filename);
		if (OutGeneratedFiles != nullptr)
		{
			OutGeneratedFiles->Add(Header.Filename, FullContent);
		}
		else
		{
			const FString FullFilename = GenDir / Header.Filename;
			bool bWriteOutFile = true;

			FString ExistingFile;
			if (FFileHelper::LoadFileToString(ExistingFile, *FullFilename))
			{
				if (ExistingFile == FullContent)
					bWriteOutFile = false;
			}

			if (bWriteOutFile)
				FFileHelper::SaveStringToFile(FullContent, *FullFilename);
		}
	}

	// Write each file we have generate code for
	int32 FileCount = JITFiles.Num();
	int32 FileIndex = 0;
	for(auto ModuleElem : JITFiles)
	{
		FJITFile& File = *ModuleElem.Value;

		FString FullContent = STANDARD_HEADER;
		for (const FString& Content : File.Headers)
			FullContent.Append(Content+"\n");
		FullContent += TEXT("\n");
		FullContent += STANDARD_INCLUDES;
		FullContent += TEXT("\n");
		for (const FString& Content : File.SharedHeaderDependencies)
			FullContent.Append(FString::Printf(TEXT("#include \"%s\"\n"), *Content));
		FullContent += TEXT("\n");
		for (auto& Elem : File.ExternalDeclarations)
		{
			FullContent.Append(Elem.Value + "\n");
		}
		for (auto& Elem : File.ExternalSystemFunctionPointerDeclarations)
		{
			FullContent.Append(Elem.Value + "\n");
		}
		FullContent += TEXT("\n");
		for (auto* ExternFunc : File.ExternFunctions)
		{
			FGenerateFunction& GenerateData = FunctionsToGenerate.FindChecked(ExternFunc);
			check(GenerateData.FunctionDeclaration.Len() != 0);
			FullContent.Append(FString::Printf(TEXT("extern %s;\n"), *GenerateData.FunctionDeclaration));
		}
		FullContent.Append(TEXT("\n\n"));

		if (bEmitDebugMetadataInOutput)
		{
			FString FilenameSymbol = GetUniqueSymbolName(TEXT("MODULENAME_") + File.ModuleName);
			FullContent.Append(FString::Printf(
				TEXT("#if AS_JIT_DEBUG_CALLSTACKS\n")
				TEXT("#undef SCRIPT_DEBUG_FILENAME\n")
				TEXT("static const char* %s = \"%s\";\n")
				TEXT("#define SCRIPT_DEBUG_FILENAME %s\n")
				TEXT("#endif\n"),
				*FilenameSymbol,
				*File.ModuleName,
				*FilenameSymbol
			));
		}

		FullContent.Append(TEXT("\n\n"));
		for (const FString& Content : File.Content)
			FullContent.Append(Content);
		FullContent += STANDARD_FOOTER;

		CurrentFiles.Add(File.Filename);
		if (OutGeneratedFiles != nullptr)
		{
			OutGeneratedFiles->Add(File.Filename, FullContent);
		}
		else
		{
			const FString FullFilename = GenDir / File.Filename;
			bool bWriteOutFile = true;

			FString ExistingFile;
			if (FFileHelper::LoadFileToString(ExistingFile, *FullFilename))
			{
				if (ExistingFile == FullContent)
					bWriteOutFile = false;
			}

			if (bWriteOutFile)
				FFileHelper::SaveStringToFile(FullContent, *FullFilename);
		}

		// Output combined unity build files
		FilesToCombine.Add(File.Filename);

		{
			LinesInCombinedFile += File.Content.Num();

			if (LinesInCombinedFile > 200000 || FileIndex == (FileCount-1))
			{
				FString CombinedFilename = FString::Printf(TEXT("AngelscriptJitCode_%d.jit.cpp"), CombinedFileIndex);
				CurrentFiles.Add(CombinedFilename);

				FString CombinedContent;
				for (const FString& FileToInclude : FilesToCombine)
					CombinedContent += FString::Printf(TEXT("#include \"%s\"\n"), *FileToInclude);

				if (OutGeneratedFiles != nullptr)
				{
					OutGeneratedFiles->Add(CombinedFilename, CombinedContent);
				}
				else
				{
					const FString CombinedPath = GenDir / CombinedFilename;
					bool bWriteOutFile = true;

					FString ExistingContent;
					if (FFileHelper::LoadFileToString(ExistingContent, *CombinedPath))
					{
						if (ExistingContent == CombinedContent)
							bWriteOutFile = false;
					}

					if (bWriteOutFile)
						FFileHelper::SaveStringToFile(CombinedContent, *CombinedPath);
				}

				LinesInCombinedFile = 0;
				CombinedFileIndex += 1;
				FilesToCombine.Reset();
			}
		}

		FileIndex += 1;
	}

	// Output information about which precompiled data was compiled in
	FString InfoFile = TEXT("AngelscriptJitInfo.jit.cpp");
	FString InfoContent = FString::Printf(
		TEXT("#include \"StaticJIT/StaticJITHeader.h\"\n")
		TEXT("\n")
		TEXT("AS_FORCE_LINK static const FStaticJITCompiledInfo JitInfo(FGuid(%d, %d, %d, %d));\n"),
		PrecompiledData->DataGuid.A,
		PrecompiledData->DataGuid.B,
		PrecompiledData->DataGuid.C,
		PrecompiledData->DataGuid.D
	);

	CurrentFiles.Add(InfoFile);
	if (OutGeneratedFiles != nullptr)
	{
		OutGeneratedFiles->Add(InfoFile, InfoContent);
	}
	else
	{
		FFileHelper::SaveStringToFile(InfoContent, *(GenDir / InfoFile));
	}

	// Delete files we no longer want to have
	if (OutGeneratedFiles == nullptr)
	{
		for (const FString& PrevFile : PreviousFiles)
		{
			if (!CurrentFiles.Contains(PrevFile))
				FileManager->Delete(*(GenDir / PrevFile));
		}
	}
}

#if AS_WITH_STATIC_JIT_DIAGNOSTICS && AS_CAN_GENERATE_JIT
namespace
{
	bool GenerateStaticJITFilesForDiagnosticsInternal(
		asIScriptModule* Module,
		TMap<FString, FString>& OutGeneratedFiles,
		bool bEmitDebugMetadata,
		bool bEmitDiagnosticEntryMarkers,
		FString* OutError)
	{
		OutGeneratedFiles.Reset();

		asCModule* ScriptModule = reinterpret_cast<asCModule*>(Module);
		if (ScriptModule == nullptr)
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("GenerateStaticJITFilesForDiagnostics failed: module was null.");
			}
			return false;
		}

		asCScriptEngine* ScriptEngine = reinterpret_cast<asCScriptEngine*>(ScriptModule->GetEngine());
		if (ScriptEngine == nullptr)
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("GenerateStaticJITFilesForDiagnostics failed: script engine was null.");
			}
			return false;
		}

		FAngelscriptStaticJIT JIT;
		FAngelscriptPrecompiledData PrecompiledData(ScriptEngine);
		JIT.PrecompiledData = &PrecompiledData;
		JIT.bGenerateOutputCode = true;
		JIT.bEmitDebugMetadataInOutput = bEmitDebugMetadata;
		JIT.bEmitDiagnosticEntryMarkersInOutput = bEmitDiagnosticEntryMarkers;

		asIJITCompiler* PreviousJITCompiler = ScriptEngine->GetJITCompiler();
		ScriptEngine->SetJITCompiler(&JIT);
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetJITCompiler(PreviousJITCompiler);
		};

		ScriptModule->JITCompile();
		JIT.WriteOutputCode(&OutGeneratedFiles);

		const TSharedPtr<FJITFile>* JITFile = JIT.JITFiles.Find(ScriptModule);
		if (JITFile == nullptr || !JITFile->IsValid())
		{
			if (OutError != nullptr)
			{
				*OutError = TEXT("GenerateStaticJITFilesForDiagnostics failed: module did not produce a JIT file.");
			}
			return false;
		}

		if (!OutGeneratedFiles.Contains((*JITFile)->Filename))
		{
			if (OutError != nullptr)
			{
				*OutError = FString::Printf(TEXT("GenerateStaticJITFilesForDiagnostics failed: generated source file '%s' was missing."), *(*JITFile)->Filename);
			}
			return false;
		}

		return true;
	}
}

bool GenerateStaticJITSourceTextForDiagnostics(asIScriptModule* Module, FString& OutSourceText, bool bEmitDebugMetadata, FString* OutError)
{
	OutSourceText.Reset();

	TMap<FString, FString> GeneratedFiles;
	if (!GenerateStaticJITFilesForDiagnosticsInternal(Module, GeneratedFiles, bEmitDebugMetadata, false, OutError))
	{
		return false;
	}

	FString SourceFilename;
	for (const TPair<FString, FString>& GeneratedFile : GeneratedFiles)
	{
		if (GeneratedFile.Key.EndsWith(TEXT(".as.jit.hpp")))
		{
			SourceFilename = GeneratedFile.Key;
			break;
		}
	}

	const FString* GeneratedSource = GeneratedFiles.Find(SourceFilename);
	if (GeneratedSource == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("GenerateStaticJITSourceTextForDiagnostics failed: generated module source was missing.");
		}
		return false;
	}

	OutSourceText = *GeneratedSource;
	return true;
}

bool GenerateStaticJITFilesForDiagnostics(
	asIScriptModule* Module,
	TMap<FString, FString>& OutGeneratedFiles,
	bool bEmitDebugMetadata,
	bool bEmitDiagnosticEntryMarkers,
	FString* OutError)
{
	return GenerateStaticJITFilesForDiagnosticsInternal(Module, OutGeneratedFiles, bEmitDebugMetadata, bEmitDiagnosticEntryMarkers, OutError);
}

bool GenerateStaticJITAotArtifactsForDiagnostics(
	asIScriptModule* Module,
	const FString& PrecompiledCacheFilename,
	const FGuid& PrecompiledDataGuid,
	TMap<FString, FString>& OutGeneratedFiles,
	bool bEmitDebugMetadata,
	bool bEmitDiagnosticEntryMarkers,
	FString* OutError)
{
	OutGeneratedFiles.Reset();

	asCModule* ScriptModule = reinterpret_cast<asCModule*>(Module);
	if (ScriptModule == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("GenerateStaticJITAotArtifactsForDiagnostics failed: module was null.");
		}
		return false;
	}

	asCScriptEngine* ScriptEngine = reinterpret_cast<asCScriptEngine*>(ScriptModule->GetEngine());
	if (ScriptEngine == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("GenerateStaticJITAotArtifactsForDiagnostics failed: script engine was null.");
		}
		return false;
	}

	FAngelscriptStaticJIT JIT;
	FAngelscriptPrecompiledData PrecompiledData(ScriptEngine);
	PrecompiledData.DataGuid = PrecompiledDataGuid;
	JIT.PrecompiledData = &PrecompiledData;
	JIT.bGenerateOutputCode = true;
	JIT.bEmitDebugMetadataInOutput = bEmitDebugMetadata;
	JIT.bEmitDiagnosticEntryMarkersInOutput = bEmitDiagnosticEntryMarkers;

	asIJITCompiler* PreviousJITCompiler = ScriptEngine->GetJITCompiler();
	ScriptEngine->SetJITCompiler(&JIT);
	ON_SCOPE_EXIT
	{
		ScriptEngine->SetJITCompiler(PreviousJITCompiler);
	};

	ScriptModule->JITCompile();
	PrecompiledData.InitFromActiveScript();
	JIT.WriteOutputCode(&OutGeneratedFiles);
	if (OutGeneratedFiles.Num() == 0)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("GenerateStaticJITAotArtifactsForDiagnostics failed: no generated files were produced.");
		}
		return false;
	}

	PrecompiledData.Save(PrecompiledCacheFilename);
	return true;
}
#endif

void FAngelscriptStaticJIT::SanitizeSymbolName(FString& InOutSymbol)
{
	for (int32 i = 0, Count = InOutSymbol.Len(); i < Count; ++i)
	{
		auto Chr = InOutSymbol[i];
		if (Chr == '_')
			continue;
		if (Chr >= '0' && Chr <= '9')
			continue;
		if (Chr >= 'a' && Chr <= 'z')
			continue;
		if (Chr >= 'A' && Chr <= 'Z')
			continue;

		// Tilde characters indicate destructors, so make that more clear
		if (Chr == '~')
		{
			InOutSymbol.RemoveAt(i, 1);
			InOutSymbol.InsertAt(i, TEXT("Destruct__"));
			Count = InOutSymbol.Len();
			continue;
		}

		// Replace anything that isn't alphanumeric or underscore with underscore
		InOutSymbol[i] = '_';
	}
}

#endif // AS_CAN_GENERATE_JIT
