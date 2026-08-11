/*
   AngelCode Scripting Library
   Copyright (c) 2003-2024 Andreas Jonsson

   This software is provided 'as-is', without any express or implied 
   warranty. In no event will the authors be held liable for any 
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any 
   purpose, including commercial applications, and to alter it and 
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you 
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product 
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and 
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source 
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/

   Andreas Jonsson
   andreas@angelcode.com
*/


//
// as_restore.h
//
// Functions for saving and restoring module bytecode
// asCRestore was originally written by Dennis Bollyn, dennis@gyrbo.be
// It was later split in two classes asCReader and asCWriter by me

#ifndef AS_RESTORE_H
#define AS_RESTORE_H

#include "as_scriptengine.h"
#include "as_context.h"
#include "as_map.h"

BEGIN_AS_NAMESPACE

//[UE++]: Bounded diagnostics for the detached Cache V2 function-artifact reader.
enum asEFunctionArtifactValidationStage
{
	asFUNCTION_ARTIFACT_STAGE_NONE = 0,
	asFUNCTION_ARTIFACT_STAGE_HEADER,
	asFUNCTION_ARTIFACT_STAGE_FUNCTION_MARKER,
	asFUNCTION_ARTIFACT_STAGE_FUNCTION_SIGNATURE,
	asFUNCTION_ARTIFACT_STAGE_FUNCTION_BYTECODE,
	asFUNCTION_ARTIFACT_STAGE_FUNCTION_STATE,
	asFUNCTION_ARTIFACT_STAGE_FUNCTION_LOCALS,
	asFUNCTION_ARTIFACT_STAGE_FUNCTION_COMPLETE,
	asFUNCTION_ARTIFACT_STAGE_EXACT_LENGTH
};

struct asSFunctionArtifactValidationDiagnostics
{
	int result;
	asUINT expectedSize;
	asUINT bytesRead;
	asUINT stage;
	bool hadError;
	bool wasNewFunction;
	// Byte coordinates are diagnostics/test support for the current versioned
	// function-artifact stream. They are never serialized into Cache V2 and use
	// asUINT(-1) when the reader did not reach the corresponding field.
	asUINT rootTraitsOffset;
	asDWORD rootTraits;
	asUINT runtimeStateOffset;
	asUINT stackNeededOffset;
	asUINT objVariablesOnHeapOffset;
	asUINT objectVariableCountOffset;
	asUINT firstObjectVariableTypeOffset;
	asUINT firstObjectVariablePositionOffset;
	asUINT objectVariableCount;
};

// Bounded writer-side diagnostics used to classify a fail-closed function
// artifact without exposing or persisting any process-local symbol. Counts are
// observations only; the host still needs a complete stable adapter before a
// nonempty table may become cacheable.
enum asEFunctionArtifactWriteStage
{
	asFUNCTION_ARTIFACT_WRITE_STAGE_NONE = 0,
	asFUNCTION_ARTIFACT_WRITE_STAGE_ROOT_FUNCTION,
	asFUNCTION_ARTIFACT_WRITE_STAGE_ROOT_SYMBOL_TABLES,
	asFUNCTION_ARTIFACT_WRITE_STAGE_FUNCTION_SIGNATURES,
	asFUNCTION_ARTIFACT_WRITE_STAGE_COMPLETE
};

struct asSFunctionArtifactWriteDiagnostics
{
	int result;
	asUINT bytesWritten;
	asUINT stage;
	asUINT usedTypeIdCount;
	asUINT usedTypeCount;
	asUINT usedFunctionCount;
	asUINT usedGlobalPropertyCount;
	asUINT usedStringConstantCount;
	asUINT usedObjectPropertyCount;
};

// One instruction operand that was resolved from the artifact's semantic
// function-signature table into the current engine/module. Cache V2 consumes
// this read-only summary to bind the VM relocation back to its stable key.
struct asSFunctionArtifactFunctionRelocation
{
	asUINT instructionOrdinal;
	asUINT operandSlot;
	asCScriptFunction *function;
};

// Every current-Engine semantic object used to relocate one opaque function
// artifact operand. These pointers are read-only observations and are never
// written to the artifact stream; the host converts them to stable Cache V2
// identities before the donor can be committed.
enum asEFunctionArtifactSymbolUseKind
{
	asFUNCTION_ARTIFACT_SYMBOL_TYPE_DECLARATION = 1,
	asFUNCTION_ARTIFACT_SYMBOL_TYPE_VALUE_LAYOUT = 2,
	asFUNCTION_ARTIFACT_SYMBOL_FUNCTION_SIGNATURE = 3,
	asFUNCTION_ARTIFACT_SYMBOL_GLOBAL_STORAGE = 4,
	asFUNCTION_ARTIFACT_SYMBOL_PROPERTY_LAYOUT = 5
};

struct asSFunctionArtifactSymbolUse
{
	asUINT instructionOrdinal;
	asUINT operandSlot;
	asEFunctionArtifactSymbolUseKind kind;
	asCTypeInfo *type;
	asCScriptFunction *function;
	asCGlobalProperty *globalProperty;
	asCTypeInfo *propertyOwnerType;
	asCObjectProperty *objectProperty;
};
//[UE--]

//[UE++]: Export restore reader internals for the AngelscriptTest module.
class ANGELSCRIPTRUNTIME_API asCReader
//[UE--]
{
public:
	asCReader(asCModule *module, asIBinaryStream *stream, asCScriptEngine *engine);
	~asCReader();

	int Read(bool *wasDebugInfoStripped);
	int ValidateFunctionArtifact(asUINT expectedSize, asSFunctionArtifactValidationDiagnostics *diagnostics = 0);
	// Reconstructs and relocates a complete function into an unpublished donor.
	// The caller may validate/apply host-owned debug metadata to the donor before
	// atomically committing it to the builder's already-declared target.
	int RestoreFunctionArtifactDetached(asUINT expectedSize,
		asCScriptFunction **outFunction,
		asSFunctionArtifactValidationDiagnostics *diagnostics = 0);
	int CommitFunctionArtifactToExisting(asCScriptFunction *artifact,
		asCScriptFunction *target);
	asUINT GetFunctionArtifactFunctionRelocationCount() const
	{
		return functionArtifactFunctionRelocations.GetLength();
	}
	const asSFunctionArtifactFunctionRelocation *
		GetFunctionArtifactFunctionRelocation(asUINT index) const
	{
		return index < functionArtifactFunctionRelocations.GetLength()
			? &functionArtifactFunctionRelocations[index] : 0;
	}
	asUINT GetFunctionArtifactSymbolUseCount() const
	{
		return functionArtifactSymbolUses.GetLength();
	}
	const asSFunctionArtifactSymbolUse *GetFunctionArtifactSymbolUse(
		asUINT index) const
	{
		return index < functionArtifactSymbolUses.GetLength()
			? &functionArtifactSymbolUses[index] : 0;
	}
	//[UE++]: Restore one previously validated Cache V2 global-function artifact
	// into this reader's staging module. The function receives an id owned by the
	// current engine; no serialized numeric id is accepted.
	int RestoreGlobalFunctionArtifact(asUINT expectedSize, asCScriptFunction **outFunction,
		asSFunctionArtifactValidationDiagnostics *diagnostics = 0);
	//[UE--]

protected:
	asCModule       *module;
	asIBinaryStream *stream;
	asCScriptEngine *engine;
	bool             noDebugInfo;
	bool             error;
	asUINT           bytesRead;
	bool             validatingFunctionArtifact;
	asUINT           functionArtifactValidationStage;

	int                Error(const char *msg);

	int                ReadInner();

	int                ReadData(void *data, asUINT size);
	void               ReadString(asCString *str);
	asCScriptFunction *ReadFunction(bool &isNew, bool addToModule = true, bool addToEngine = true, bool addToGC = true, bool *isExternal = 0);
	void               ReadFunctionSignature(asCScriptFunction *func, asCObjectType **parentClass = 0);
	void               ReadGlobalProperty();
	void               ReadObjectProperty(asCObjectType *ot);
	void               RebuildRestoredScriptClassLayouts();
	bool               RebuildRestoredScriptClassLayout(asCObjectType *ot, asCArray<asCObjectType*> &layoutingTypes, asCArray<asCObjectType*> &layoutedTypes);
	void               ReadDataType(asCDataType *dt);
	asCTypeInfo       *ReadTypeInfo();
	void               ReadTypeDeclaration(asCTypeInfo *ot, int phase, bool *isExternal = 0);
	void               ReadByteCode(asCScriptFunction *func);
	asWORD             ReadEncodedUInt16();
	asUINT             ReadEncodedUInt();
	int                ReadEncodedInt();
	asQWORD            ReadEncodedUInt64();
	asUINT             SanityCheck(asUINT val, asUINT max);
	int                SanityCheck(int val, asUINT max);

	void ReadUsedTypeIds();
	void ReadUsedFunctions();
	void ReadUsedGlobalProps();
	void ReadUsedStringConstants();
	void ReadUsedObjectProps();
	void ReadFunctionArtifactSymbolTables();
	void ReadFunctionArtifactRuntimeState();
	void ApplyFunctionArtifactRuntimeState(asCScriptFunction *func);

	asCTypeInfo *      FindType(int idx);
	int                FindTypeId(int idx);
	short              FindObjectPropOffset(asWORD index,
		asUINT instructionOrdinal, asUINT operandSlot);
	asCScriptFunction *FindFunction(int idx);

	// After loading, each function needs to be translated to update pointers, function ids, etc
	void TranslateFunction(asCScriptFunction *func);
	void RecordFunctionArtifactRelocation(asUINT instructionOrdinal,
		asUINT operandSlot, asCScriptFunction *function);
	void RecordFunctionArtifactTypeUse(asUINT instructionOrdinal,
		asUINT operandSlot, asCTypeInfo *type);
	void RecordFunctionArtifactTypeIdUse(asUINT instructionOrdinal,
		asUINT operandSlot, int serializedTypeIdIndex);
	void RecordFunctionArtifactGlobalUse(asUINT instructionOrdinal,
		asUINT operandSlot, asUINT globalPropertyIndex);
	void RecordFunctionArtifactPropertyUse(asUINT instructionOrdinal,
		asUINT operandSlot, asCTypeInfo *ownerType,
		asCObjectProperty *property);
	void CalculateAdjustmentByPos(asCScriptFunction *func);
	int  AdjustStackPosition(int pos);
	int  AdjustGetOffset(int offset, asCScriptFunction *func, asDWORD programPos);
	void CalculateStackNeeded(asCScriptFunction *func);

	// Temporary storage for persisting variable data
	asCArray<int>                usedTypeIds;
	asCArray<asCTypeInfo*>       usedTypes;
	asCArray<asCScriptFunction*> usedFunctions;
	asCArray<void*>              usedGlobalProperties;
	asCArray<void*>              usedStringConstants;

	asCArray<asCScriptFunction*>  savedFunctions;
	asCArray<asCDataType>         savedDataTypes;
	asCArray<asCString>           savedStrings;
	asCArray<asSFunctionArtifactFunctionRelocation>
		functionArtifactFunctionRelocations;
	asCArray<asSFunctionArtifactSymbolUse> functionArtifactSymbolUses;
	asCArray<asCGlobalProperty*> functionArtifactGlobalProperties;
	asUINT                         functionArtifactStackNeeded;
	asUINT                         functionArtifactObjVariablesOnHeap;
	asUINT                         functionArtifactRuntimeStateOffset;
	asUINT                         functionArtifactStackNeededOffset;
	asUINT                         functionArtifactObjVariablesOnHeapOffset;
	asUINT                         functionArtifactObjectVariableCountOffset;
	asUINT                         functionArtifactFirstObjectVariableTypeOffset;
	asUINT                         functionArtifactFirstObjectVariablePositionOffset;
	asCArray<asCTypeInfo*>         functionArtifactObjVariableTypes;
	asCArray<int>                  functionArtifactObjVariablePositions;

	asCArray<int>                 adjustByPos;
	asCArray<int>                 adjustNegativeStackByPos;

	struct SObjProp
	{
		asCObjectType     *objType;
		asCObjectProperty *prop;
	};
	asCArray<SObjProp> usedObjectProperties;

	asCMap<void*,bool>              existingShared;
	asCMap<asCScriptFunction*,bool> dontTranslate;

	// Helper class for adjusting offsets within initialization list buffers
	struct SListAdjuster
	{
		SListAdjuster(asCReader *rd, asDWORD *bc, asCObjectType *ot);
		void AdjustAllocMem();
		int  AdjustOffset(int offset);
		void SetRepeatCount(asUINT rc);
		void SetNextType(int typeId);

		struct SInfo
		{
			asUINT              repeatCount;
			asSListPatternNode *startNode;
		};
		asCArray<SInfo> stack;

		asCReader          *reader;
		asDWORD            *allocMemBC;
		asUINT              maxOffset;
		asCObjectType      *patternType;
		asUINT              repeatCount;
		int                 lastOffset;
		int                 nextOffset;
		asUINT              lastAdjustedOffset;
		asSListPatternNode *patternNode;
		int                 nextTypeId;
	};
	asCArray<SListAdjuster*> listAdjusters;

	// Used by FindObjectPropOffset
	asCObjectProperty* lastCompositeProp;
};

#ifndef AS_NO_COMPILER

//[UE++]: Export restore writer internals for the AngelscriptTest module.
class ANGELSCRIPTRUNTIME_API asCWriter
//[UE--]
{
public:
	asCWriter(asCModule *module, asIBinaryStream *stream, asCScriptEngine *engine, bool stripDebugInfo);

	int Write();
	int WriteFunctionArtifact(asCScriptFunction *func,
		asSFunctionArtifactWriteDiagnostics *diagnostics = 0);

protected:
	asCModule       *module;
	asIBinaryStream *stream;
	asCScriptEngine *engine;
	bool             stripDebugInfo;
	bool             error;
	asUINT           bytesWritten;

	int              Error(const char *msg);

	int  WriteData(const void *data, asUINT size);

	void WriteString(asCString *str);
	void WriteFunction(asCScriptFunction *func);
	void WriteFunctionSignature(asCScriptFunction *func);
	void WriteGlobalProperty(asCGlobalProperty *prop);
	void WriteObjectProperty(asCObjectProperty *prop);
	void WriteDataType(const asCDataType *dt);
	void WriteTypeInfo(asCTypeInfo *ot);
	void WriteTypeDeclaration(asCTypeInfo *ot, int phase);
	void WriteByteCode(asCScriptFunction *func);
	void WriteEncodedInt64(asINT64 i);

	// Helper functions for storing variable data
	int FindTypeInfoIdx(asCTypeInfo *ti);
	int FindTypeIdIdx(int typeId);
	int FindFunctionIndex(asCScriptFunction *func);
	int FindGlobalPropPtrIndex(void *);
	int FindStringConstantIndex(void *str);
	int FindObjectPropIndex(short offset, int typeId, asDWORD *bc);

	void CalculateAdjustmentByPos(asCScriptFunction *func);
	int  AdjustStackPosition(int pos);
	int  AdjustProgramPosition(int pos);
	int  AdjustGetOffset(int offset, asCScriptFunction *func, asDWORD programPos);

	// Intermediate data used for storing that which isn't constant, function id's, pointers, etc
	void WriteUsedTypeIds();
	void WriteUsedFunctions();
	void WriteUsedGlobalProps();
	void WriteUsedStringConstants();
	void WriteUsedObjectProps();

	// Temporary storage for persisting variable data
	asCArray<int>                usedTypeIds;
	asCArray<asCTypeInfo*>       usedTypes;
	asCArray<asCScriptFunction*> usedFunctions;
	asCArray<void*>              usedGlobalProperties;
	asCArray<void*>              usedStringConstants;
	asCMap<void*, int>           stringToIndexMap;

	asCArray<asCScriptFunction*>  savedFunctions;
	asCArray<asCDataType>         savedDataTypes;
	asCArray<asCString>           savedStrings;
	asCMap<asCString, int>        stringToIdMap;
	asCArray<int>                 adjustStackByPos;
	asCArray<int>                 adjustNegativeStackByPos;
	asCArray<int>                 bytecodeNbrByPos;

	struct SObjProp
	{
		asCObjectType     *objType;
		asCObjectProperty *prop;
	};
	asCArray<SObjProp>           usedObjectProperties;

	// Helper class for adjusting offsets within initialization list buffers
	struct SListAdjuster
	{
		SListAdjuster(asCObjectType *ot);
		int  AdjustOffset(int offset, asCObjectType *listPatternType);
		void SetRepeatCount(asUINT rc);
		void SetNextType(int typeId);

		struct SInfo
		{
			asUINT              repeatCount;
			asSListPatternNode *startNode;
		};
		asCArray<SInfo> stack;

		asCObjectType      *patternType;
		asUINT              repeatCount;
		asSListPatternNode *patternNode;
		asUINT              entries;
		int                 lastOffset;
		int                 nextOffset;
		int                 nextTypeId;
	};
	asCArray<SListAdjuster*> listAdjusters;

	// Used by FindObjectPropIndex
	bool lastWasComposite;
};

#endif

END_AS_NAMESPACE

#endif // AS_RESTORE_H
