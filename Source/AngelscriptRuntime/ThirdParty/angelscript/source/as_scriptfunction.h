/*
   AngelCode Scripting Library
   Copyright (c) 2003-2017 Andreas Jonsson

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
// as_scriptfunction.h
//
// A container for a compiled script function
//



#ifndef AS_SCRIPTFUNCTION_H
#define AS_SCRIPTFUNCTION_H

#include "as_config.h"
#include "as_string.h"
#include "as_array.h"
#include "as_datatype.h"
#include "as_atomic.h"
#include "as_buildartifact.h"

BEGIN_AS_NAMESPACE

class asCScriptEngine;
class asCModule;
class asCConfigGroup;
class asCGlobalProperty;
class asCScriptNode;
class asCFuncdefType;
struct asSNameSpace;

struct asSScriptVariable
{
	asCString   name;
	asCDataType type;
	int         stackOffset;
	//[UE++]: Carry in the stock restore-local variable storage bit without disturbing APV2 debug data ownership.
	bool        onHeap = false;
	//[UE--]
	asUINT      declaredAtProgramPos;
};

//[UE++]: Carry in the stock restore try/catch metadata container surface for later 3-way merge work.
struct asSTryCatchInfo
{
	asUINT tryPos = 0;
	asUINT catchPos = 0;
	int stackOffset = 0;
};
//[UE--]

enum asEListPatternNodeType
{
	asLPT_REPEAT,
	asLPT_REPEAT_SAME,
	asLPT_START,
	asLPT_END,
	asLPT_TYPE
};

struct asSListPatternNode
{
	asSListPatternNode(asEListPatternNodeType t) : type(t), next(0) {}
	virtual ~asSListPatternNode() {};
	virtual asSListPatternNode *Duplicate() { return asNEW(asSListPatternNode)(type); }
	asEListPatternNodeType  type;
	asSListPatternNode     *next;
};

struct asSListPatternDataTypeNode : public asSListPatternNode
{
	asSListPatternDataTypeNode(const asCDataType &dt) : asSListPatternNode(asLPT_TYPE), dataType(dt) {}
	asSListPatternNode *Duplicate() { return asNEW(asSListPatternDataTypeNode)(dataType); }
	asCDataType dataType;
};

enum asEObjVarInfoOption
{
	asOBJ_UNINIT,
	asOBJ_INIT,
	asBLOCK_BEGIN,
	asBLOCK_END,
	asOBJ_VARDECL
};

enum asEFuncTrait
{
	asTRAIT_CONSTRUCTOR = 0x1,
	asTRAIT_DESTRUCTOR  = 0x2,
	asTRAIT_CONST       = 0x4,
	asTRAIT_PRIVATE     = 0x8,
	asTRAIT_PROTECTED   = 0x10,
	asTRAIT_FINAL       = 0x20,
	asTRAIT_OVERRIDE    = 0x40,
	asTRAIT_SHARED      = 0x80,
	asTRAIT_EXTERNAL    = 0x100,
	asTRAIT_PROPERTY    = 0x200,
	asTRAIT_IMPLICITCONSTRUCTOR = 0x400,
	asTRAIT_MIXIN = 0x800,
	asTRAIT_LOCAL = 0x1000,
	asTRAIT_NODISCARD = 0x2000,
	asTRAIT_DEPRECATED = 0x4000,
	asTRAIT_GENERIC_TEMPLATE_FUNCTION = 0x8000,
	asTRAIT_USES_WORLDCONTEXT = 0x10000,
	asTRAIT_ACCEPT_TEMPORARY_OBJECT = 0x20000,
	asTRAIT_GENERATED_FUNCTION = 0x40000,
	asTRAIT_NOT_CALLABLE = 0x80000,
	asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS = 0x100000,
	asTRAIT_EXTERNAL_IMPLICIT_THIS = 0x200000,
	asTRAIT_ALLOWDISCARD = 0x400000,
	asTRAIT_EDITOR_ONLY = 0x800000,
	//[UE++]: Reintroduce the frozen explicit-trait bit needed by the restore prerequisite surface.
	asTRAIT_EXPLICIT = 0x1000000,
	asTRAIT_UNSAFE_DURING_CONSTRUCTION = 0x2000000,
	asTRAIT_DEFAULTS_ONLY = 0x4000000,
	//[UE--]
};

enum class asECompileOutType
{
	CompileCalls,
	CompileOutEntirely,
	ReplaceWithFirstParam,
	CompileOutAsMethodChain,
};

struct asSFunctionTraits
{
	asSFunctionTraits() : traits(0) {}
	void SetTrait(asEFuncTrait trait, bool set) { if (set) traits |= trait; else traits &= ~trait; }
	bool GetTrait(asEFuncTrait trait) const { return (traits & trait) ? true : false; }
//protected:
	asDWORD traits;
};

struct asSObjectVariableInfo
{
	asUINT              programPos;
	int                 variableOffset;
	asEObjVarInfoOption option;
};

struct asSSystemFunctionInterface;

// TODO: Might be interesting to allow enumeration of accessed global variables, and
//       also functions/methods that are being called. This could be used to build a
//       code database with call graphs, etc.

void RegisterScriptFunction(asCScriptEngine *engine);

//[UE++]: Rename export macro to match the runtime module rename.
class ANGELSCRIPTRUNTIME_API asCScriptFunction final : public asIScriptFunction
//[UE--]
{
public:
	// From asIScriptFunction
	asIScriptEngine     *GetEngine() const;

	// Memory management
	int AddRef() const;
	int Release() const;

	// Miscellaneous
	int                  GetId() const;
	asEFuncType          GetFuncType() const;
	const char          *GetModuleName() const;
	asIScriptModule     *GetModule() const;
	const char          *GetScriptSectionName() const;
	const char          *GetConfigGroup() const;
	asDWORD              GetAccessMask() const;
	void                *GetAuxiliary() const;
	bool IsNoOp() const;

	static asUINT GenerateExposedType(bool editable, bool readable, bool writeable);

	// Function signature
	asITypeInfo         *GetObjectType() const;
	const char          *GetObjectName() const;
	const char          *GetName() const;
	const char          *GetNamespace() const;
	const char          *GetDeclaration(bool includeObjectName = true, bool includeNamespace = false, bool includeParamNames = false, bool forDisplay = false) const;
	bool                 IsReadOnly() const;
	bool                 IsSetterAndOnlyValidDuringInitDefaults() const;
	bool                 IsPrivate() const;
	bool                 IsProtected() const;
	bool                 IsFinal() const;
	bool                 IsOverride() const;
	bool                 IsShared() const;
	bool                 IsProperty() const;
	bool                 IsMixin() const;
	asUINT               GetParamCount() const;
	int                  GetParam(asUINT index, int *typeId, asDWORD *flags = 0, const char **name = 0, const char **defaultArg = 0) const;
	int                  GetReturnTypeId(asDWORD *flags = 0) const;

	// Type id for function pointers
	int                  GetTypeId() const;
	bool                 IsCompatibleWithTypeId(int typeId) const;

	// Delegates
	void                *GetDelegateObject() const;
	asITypeInfo         *GetDelegateObjectType() const;
	asIScriptFunction   *GetDelegateFunction() const;

	// Debug information
	asUINT               GetVarCount() const;
	int                  GetVar(asUINT index, const char **name, int *typeId = 0) const;
	const char *         GetVarDecl(asUINT index, bool includeNamespace = false) const;
	int                  FindNextLineWithCode(int line) const;

	// For JIT compilation
	asDWORD             *GetByteCode(asUINT *length = 0);

	// User data
	void                *SetUserData(void *userData, asPWORD type);
	void                *GetUserData() const { return userData; }

	void* DebugPrototypePtr = nullptr;

public:
	//-----------------------------------
	// Internal methods

	void SetShared(bool set) {traits.SetTrait(asTRAIT_SHARED, set);}
	void SetReadOnly(bool set) { traits.SetTrait(asTRAIT_CONST, set); }
	void SetFinal(bool set) { traits.SetTrait(asTRAIT_FINAL, set); }
	void SetOverride(bool set) { traits.SetTrait(asTRAIT_OVERRIDE, set); }
	void SetProtected(bool set) { traits.SetTrait(asTRAIT_PROTECTED, set); }
	void SetPrivate(bool set) { traits.SetTrait(asTRAIT_PRIVATE, set); }
	void SetProperty(bool set) { traits.SetTrait(asTRAIT_PROPERTY, set); }
	void SetMixin(bool set) { traits.SetTrait(asTRAIT_MIXIN, set); }
	void SetExplicit(bool set) { traits.SetTrait(asTRAIT_EXPLICIT, set); }

	asCScriptFunction(asCScriptEngine *engine, asCModule *mod, asEFuncType funcType);
	~asCScriptFunction();

	// Keep an internal reference counter to separate references coming from
	// application or script objects and references coming from the script code
	int AddRefInternal();
	int ReleaseInternal();

	void     DestroyHalfCreated();

	// TODO: operator==
	// TODO: The asIScriptFunction should provide operator== and operator!= that should do a
	//       a value comparison. Two delegate objects that point to the same object and class method should compare as equal
	// TODO: The operator== should also be provided in script as opEquals to allow the same comparison in script
	//       To do this we'll need some way to adapt the argtype for opEquals for each funcdef, preferrably without instantiating lots of different methods
	//       Perhaps reusing 'auto' to mean the same type as the object
	//bool      operator==(const asCScriptFunction &other) const;

	void      DestroyInternal();

	void      AddVariable(asCString &name, asCDataType &type, int stackOffset);
	void      AddVariable(const asCString &name, const asCDataType &type, int stackOffset, bool onHeap);

	int       GetSpaceNeededForArguments();
	int       GetSpaceNeededForReturnValue();
	asCString GetDeclarationStr(bool includeObjectName = true, bool includeNamespace = false, bool includeParamNames = false, bool forDisplay = false, bool includeDefaultValues = true) const;
	int       GetLineNumber(int programPosition, int *sectionIdx);
	void      ComputeSignatureId();
	bool      IsSignatureEqual(const asCScriptFunction *func) const;
	bool      IsSignatureExceptNameEqual(const asCScriptFunction *func) const;
	bool      IsSignatureExceptNameEqual(const asCDataType &retType, const asCArray<asCDataType> &paramTypes, const asCArray<asETypeModifiers> &inOutFlags, const asCObjectType *type, bool isReadOnly) const;
	bool      IsSignatureExceptNameAndReturnTypeEqual(const asCScriptFunction *fun) const;
	bool      IsSignatureExceptNameAndReturnTypeEqual(const asCArray<asCDataType> &paramTypes, const asCArray<asETypeModifiers> &inOutFlags, const asCObjectType *type, bool isReadOnly) const;
	bool      IsSignatureExceptNameAndObjectTypeEqual(const asCScriptFunction *func) const;

	asCTypeInfo *GetTypeInfoOfLocalVar(short varOffset);

	void      MakeDelegate(asCScriptFunction *func, void *obj);

	int       RegisterListPattern(const char *decl, asCScriptNode *listPattern);
	int       ParseListPattern(asSListPatternNode *&target, const char *decl, asCScriptNode *listPattern);

	bool      DoesReturnOnStack() const;

	void      JITCompile();

	void      AddReferences();
	void      ReleaseReferences();

	void      AllocateScriptFunctionData();
	void      DeallocateScriptFunctionData();

	asCGlobalProperty *GetPropertyByGlobalVarPtr(void *gvarPtr);

	// GC methods (for delegates)
	int  GetRefCount();
	void SetFlag();
	bool GetFlag();
	void EnumReferences(asIScriptEngine *engine);
	void ReleaseAllHandles(asIScriptEngine *engine);

	void CalculateParameterOffsets();

public:
	//-----------------------------------
	// Properties

	mutable asCAtomic            externalRefCount; // Used for external referneces
	        asCAtomic            internalRefCount; // Used for internal references
	asCScriptEngine             *engine;
	asCModule                   *module;

	// Function signature
	asCString                      name;
	asCDataType                  returnType;
	asCArray<asCDataType>        parameterTypes;
	asCArray<asCDataType>        templateSubTypes;
	asCArray<asCString>          parameterNames;
	asCArray<asETypeModifiers>   inOutFlags;
	asCArray<asCString *>        defaultArgs;
	asCObjectType               *objectType;
	void* userData;
	asUINT                       exposedType;
	asDWORD                      accessMask = 0xFFFFFFFF;
	int                          signatureId = 0;
	asSAccessSpecifier*          accessSpecifier;

	asCArray<int>				 parameterOffsets;
	asCString                    hiddenArgumentDefault;

	// Namespace will be null for funcdefs that are declared as child funcdefs
	// of a class. In this case the namespace shall be taken from the parentClass
	// in the funcdefType
	asSNameSpace                *nameSpace;

	// Used by asFUNC_DELEGATE
	static asCFuncdefType              *funcdefType; // Doesn't increase refCount
	static void              *objForDelegate;
	static asCScriptFunction *funcForDelegate;

	// Used by list factory behaviour
	static asSListPatternNode *listPattern;

	// JIT compiled code of this function
	asJITFunction                   jitFunction = 0;
	asJITFunction_ParmsEntry        jitFunction_ParmsEntry = 0;
	asJITFunction_Raw               jitFunction_Raw = 0;

	// Used by asFUNC_SCRIPT
	struct ScriptFunctionData
	{
		// Bytecode for the script function
		asCArray<asDWORD>               byteCode;

		// The stack space needed for the local variables
		asDWORD                         variableSpace;

		// These hold information on objects and function pointers, including temporary
		// variables used by exception handler and when saving bytecode
		asCArray<asCTypeInfo*>          objVariableTypes;
		asCArray<int>                   objVariablePos;

		// The first variables in above array are allocated on the heap, the rest on the stack.
		// This variable shows how many are on the heap.
		asUINT                          objVariablesOnHeap;

		// Holds information on scope for object variables on the stack
		asCArray<asSObjectVariableInfo> objVariableInfo;

		asCArray<asSTryCatchInfo>       tryCatchInfo;

		// The stack needed to execute the function
		int                             stackNeeded;

		// Hold debug information about temporary variables and their types
		struct ScriptTempVariable
		{
			int Offset;
			eTokenType Token;
		};
		asCArray<ScriptTempVariable>    temporaryVariables;

		// Holds debug information on explicitly declared variables
		asCArray<asSScriptVariable*>    variables;
		// Store position, line number pairs for debug information
		asCArray<int>                   lineNumbers;
		// Store the script section where the code was declared
		int                             scriptSectionIdx;
		// Store the location where the function was declared  (row in the lower 20 bits, and column in the upper 12)
		int                             declaredAt;
		// Store position/index pairs if the bytecode is compiled from multiple script sections
		asCArray<int>                   sectionIdxs;

		// Canonical token slice retained for stable external artifact identity. This
		// contains no comments or whitespace and is independent of source line moves.
		asCString                       artifactCanonicalSource;

		// Raw engine-local dependencies observed by the authoritative compiler for
		// this successful invocation. Failed and NotCacheable invocations leave this
		// empty. The Runtime bridge maps these pointers to stable semantic keys before
		// anything is persisted.
		asCArray<asSBuildArtifactDependency> artifactDependencies;
	};
	ScriptFunctionData          *scriptData;

	// Used by asFUNC_SYSTEM
	asSSystemFunctionInterface  *sysFuncIntf;

	// Used by asFUNC_VIRTUAL
	int                          vfTableIdx;

	int spaceNeededForArguments = 0;
	int totalSpaceBeforeFunction = 0;
	int8 hiddenArgumentIndex = -1;
	int8 determinesOutputTypeArgumentIndex = -1;

	int                          id;
	asEFuncType                  funcType;
	asSFunctionTraits            traits;
	//[UE++]: Current-Engine declaration metadata for Cache V2 stable identity.
	// The owner pointer is transient and is never serialized; the Runtime maps it
	// to a stable TypeKey before any artifact is persisted.
	asEBuildArtifactInvocationKind artifactInvocationKind =
		asBUILD_ARTIFACT_INVOCATION_INVALID;
	asCTypeInfo* artifactOwnerType = nullptr;
	//[UE--]
	asECompileOutType            compileOutType = asECompileOutType::CompileCalls;

	// Stub functions and delegates don't own the object and parameters
	mutable bool                 gcFlag;
	bool                         dontCleanUpOnException : 1;
	bool                         isInUse : 1;

#if WITH_EDITOR
	asCString deprecationMessage;
#endif

};

const char * const DELEGATE_FACTORY = "$dlgte";
asCScriptFunction *CreateDelegate(asCScriptFunction *func, void *obj);

END_AS_NAMESPACE

#endif
