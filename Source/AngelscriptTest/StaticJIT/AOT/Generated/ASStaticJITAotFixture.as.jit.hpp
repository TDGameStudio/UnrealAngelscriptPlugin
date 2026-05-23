#include "StaticJIT/StaticJITConfig.h"
#if UE_BUILD_DEVELOPMENT
#ifndef AS_SKIP_JITTED_CODE

#include "../../../Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h"

#include "StaticJIT/StaticJITHeader.h"


AS_FORCE_LINK FJitRef_Type TREF_UStaticJITAotFunctionCarrier(0x1ccb4d6d500);
AS_FORCE_LINK FJitRef_GlobalVar GREF___StaticType_UStaticJITAotFunctionCarrier(0x1ccb4d4dee0);
AS_FORCE_LINK FJitRef_SystemFunctionPointer SYSPTR___WorldContext(0x1ccb2726600);
AS_FORCE_LINK FJitRef_SystemFunctionPointer SYSPTR_TSubclassOf_UObject__opImplConv(0x1ccb51e2c00);

extern void AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier(FScriptExecution& Execution, UObject* l_This);
extern int32 AS_ASStaticJITAotFixture__AddForAOT(FScriptExecution& Execution, asDWORD p_Value);


#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASStaticJITAotFixture = "ASStaticJITAotFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASStaticJITAotFixture
#endif


constexpr SIZE_T POFFSET_UStaticJITAotFunctionCarrier_StoredValue = Align(sizeof(UObject) + 0, 4);
constexpr SIZE_T PALIGN_UStaticJITAotFunctionCarrier_StoredValue = AlignmentMax(alignof(UObject), 4);
constexpr SIZE_T TALIGN_UStaticJITAotFunctionCarrier = PALIGN_UStaticJITAotFunctionCarrier_StoredValue;
constexpr SIZE_T TSIZE_UStaticJITAotFunctionCarrier = Align(POFFSET_UStaticJITAotFunctionCarrier_StoredValue + 4, TALIGN_UStaticJITAotFunctionCarrier);
AS_FORCE_LINK FJitVerifyPropertyOffset PVERIFY_UStaticJITAotFunctionCarrier_StoredValue(0x6010006129, POFFSET_UStaticJITAotFunctionCarrier_StoredValue);
void AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::StorePrimitiveArg(int)", 19);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x581ba130u);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// SUSPEND 
// ADDIi v1, v-2, 3
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x3u);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND 
// RET 3
  return;
}
static void AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
static void AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_Register(0x581ba130u, &AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_VMEntry, &AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg);

int32 AS_UStaticJITAotFunctionCarrier__ReturnPrimitive(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReturnPrimitive()", 25);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0xc36d30ecu);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// SUSPEND 
// SetV4 v1, 61
v_TEMP_dword_1 = 0x3du;
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
static void AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_UStaticJITAotFunctionCarrier__ReturnPrimitive(Execution,
		*(UObject**)l_fp);
}
static void AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_UStaticJITAotFunctionCarrier__ReturnPrimitive(Execution,
		(UObject*)Object);
}
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_Register(0xc36d30ecu, &AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_VMEntry, &AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__ReturnPrimitive);

int32 AS_UStaticJITAotFunctionCarrier__BumpReference(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReference(int&)", 31);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x6eb950c0u);
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// TrackRef v-2
// SUSPEND 
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr 
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// ADDIi v1, v1, 5
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_1) + value_as<int>((asDWORD)0x5u);
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// SUSPEND 
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr 
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v2
v_TEMP_dword_2 = value_read<asDWORD>((void*)l_valueRegister);
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v2
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_2), 4);
// SUSPEND 
// ValidateRef v-2
// PshVPtr v-2
// PopRPtr 
l_valueRegister = (asQWORD)(p_Value);
// RDR4 v1
v_TEMP_dword_1 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// UntrackRef v-2
// RET 4
  return (int32)l_dwordRegister;
}
static void AS_UStaticJITAotFunctionCarrier__BumpReference_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_UStaticJITAotFunctionCarrier__BumpReference(Execution,
		*(UObject**)l_fp,
		*(int32**)(l_fp + 2));
}
static void AS_UStaticJITAotFunctionCarrier__BumpReference_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_UStaticJITAotFunctionCarrier__BumpReference(Execution,
		(UObject*)Object,
		(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__BumpReference_Register(0x6eb950c0u, &AS_UStaticJITAotFunctionCarrier__BumpReference_VMEntry, &AS_UStaticJITAotFunctionCarrier__BumpReference_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__BumpReference);

UObject* AS_UStaticJITAotFunctionCarrier__ReturnSelfObject(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UObject UStaticJITAotFunctionCarrier::ReturnSelfObject()", 39);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0xfbcbdbedu);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// SUSPEND 
// LOADOBJ v0
l_objectRegister = (void*)l_This;
l_This = nullptr;
// RET 2
  return (UObject*)l_objectRegister;
}
static void AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = AS_UStaticJITAotFunctionCarrier__ReturnSelfObject(Execution,
		*(UObject**)l_fp);
}
static void AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = AS_UStaticJITAotFunctionCarrier__ReturnSelfObject(Execution,
		(UObject*)Object);
}
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_Register(0xfbcbdbedu, &AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_VMEntry, &AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__ReturnSelfObject);

void AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()", 14);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0xbf22cbcfu);
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// SUSPEND 
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// LoadThisR +48
l_valueRegister = ((asQWORD)l_This) + POFFSET_UStaticJITAotFunctionCarrier_StoredValue;
// WRTV4 v1
memcpy((void*)l_valueRegister, (void*)(&v_TEMP_dword_1), 4);
// PshVPtr v0
// FinConstruct *
{
  asIScriptObject* Object = (asIScriptObject*)(l_This);
  asITypeInfo* TypeInfo = (asITypeInfo*)TREF_UStaticJITAotFunctionCarrier.Get();
  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);
}
// RET 2
  return;
}
static void AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier(Execution,
		*(UObject**)l_fp);
}
static void AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier(Execution,
		(UObject*)Object);
}
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_Register(0xbf22cbcfu, &AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_VMEntry, &AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier);

UObject* AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x10bc161fu);
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_2 = nullptr;
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)TREF_UStaticJITAotFunctionCarrier.Get();
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_ptr_2));
  if(a != nullptr) *a = (asPWORD)mem;
value_assign_safe<asQWORD>(&l_stack[0], mem);
// UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()
SCRIPT_DEBUG_CALLSTACK_LINE(0);
{
UObject* CallObject = (UObject*)((asQWORD&)l_stack[0]);
if (CallObject != nullptr)
{
 AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier(Execution,
		(UObject*)CallObject);
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
}
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_ptr_2;
v_TEMP_ptr_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
static void AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier(Execution);
}
static void AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier(Execution);
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_Register(0x10bc161fu, &AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_VMEntry, &AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier);

int32 AS_ASStaticJITAotFixture__AddForAOT(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int AddForAOT(int)", 3);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x6704d30du);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
// SUSPEND 
// ADDIi v1, v0, 7
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) + value_as<int>((asDWORD)0x7u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
static void AS_ASStaticJITAotFixture__AddForAOT_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_ASStaticJITAotFixture__AddForAOT(Execution,
		*(asDWORD*)(l_fp + 0));
}
static void AS_ASStaticJITAotFixture__AddForAOT_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__AddForAOT(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__AddForAOT_Register(0x6704d30du, &AS_ASStaticJITAotFixture__AddForAOT_VMEntry, &AS_ASStaticJITAotFixture__AddForAOT_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__AddForAOT);

int32 AS_ASStaticJITAotFixture__Entry(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int Entry()", 8);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x7955c32eu);
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// SUSPEND 
// PshC4 35
// CALL
// int AddForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_LINE(8);
{
l_dwordRegister = (asDWORD) AS_ASStaticJITAotFixture__AddForAOT(Execution,
		value_as<asDWORD>(((asDWORD)0x23u)));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// CpyRtoV4 v2
v_TEMP_dword_2 = l_dwordRegister;
// CpyVtoR4 v2
l_dwordRegister = v_TEMP_dword_2;
// RET 0
  return (int32)l_dwordRegister;
}
static void AS_ASStaticJITAotFixture__Entry_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_ASStaticJITAotFixture__Entry(Execution);
}
static void AS_ASStaticJITAotFixture__Entry_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__Entry(Execution);
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__Entry_Register(0x7955c32eu, &AS_ASStaticJITAotFixture__Entry_VMEntry, &AS_ASStaticJITAotFixture__Entry_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__Entry);

int32 AS_ASStaticJITAotFixture__StaticWorldContextCheck(FScriptExecution& Execution, UObject* p_WorldContextObject, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int StaticWorldContextCheck(UObject, int)", 46);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x6ff89890u);
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_2 = nullptr;
UObject* v_TEMP_ptr_4 = nullptr;
asBYTE v_TEMP_byte_5 = {};
asDWORD v_TEMP_dword_6 = {};
// SUSPEND 
// CALLSYS *
// UObject __WorldContext()
{
  asFUNCTION_t RawFuncPtr = SYSPTR___WorldContext.GetFunction();
  auto CastedFuncPtr = (UObject*(*)())RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_LINE(46);
UObject* FunctionReturnValue = CastedFuncPtr();
l_objectRegister = (void*)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// STOREOBJ v2
v_TEMP_ptr_2 = (UObject*)l_objectRegister;
l_objectRegister = nullptr;
// PshVPtr v0
// RefCpyV v4
v_TEMP_ptr_4 = (UObject*)(p_WorldContextObject);
// CmpPtr v2, v4
l_byteRegister = ((void*)v_TEMP_ptr_2 == (void*)v_TEMP_ptr_4) ? 0 : 1;
// JZ 6
if(l_byteRegister == 0) {
goto LABEL_StaticWorldContextCheck_17;
}
// SUSPEND 
// SetV4 v6, -1
v_TEMP_dword_6 = 0xffffffffu;
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
// JMP 5
goto LABEL_StaticWorldContextCheck_22;
LABEL_StaticWorldContextCheck_17:
// SUSPEND 
// ADDIi v6, v-2, 2
((int32&)v_TEMP_dword_6) = ((int32&)p_Value) + value_as<int>((asDWORD)0x2u);
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
LABEL_StaticWorldContextCheck_22:
// RET 3
  return (int32)l_dwordRegister;
}
static void AS_ASStaticJITAotFixture__StaticWorldContextCheck_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_ASStaticJITAotFixture__StaticWorldContextCheck(Execution,
		*(UObject**)(l_fp + 0),
		*(asDWORD*)(l_fp + 2));
}
static void AS_ASStaticJITAotFixture__StaticWorldContextCheck_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_WorldContextObject = ParmsOffset;
	ParmsOffset += sizeof(void*);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__StaticWorldContextCheck(Execution,
		*(UObject**)(((SIZE_T)Parms) + ParmOffset_0_WorldContextObject),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__StaticWorldContextCheck_Register(0x6ff89890u, &AS_ASStaticJITAotFixture__StaticWorldContextCheck_VMEntry, &AS_ASStaticJITAotFixture__StaticWorldContextCheck_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__StaticWorldContextCheck);

UClass* AS_ASStaticJITAotFixture__StaticClass(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UClass UStaticJITAotFunctionCarrier::StaticClass()", 52);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITTestHooks::MarkEntry(0x6fccf793u);
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UClass* v_TEMP_ptr_2 = nullptr;
// SUSPEND 
// PshGPtr ::__StaticType_UStaticJITAotFunctionCarrier
// Materialize stack 0
value_assign_safe<asQWORD>(&l_stack[0], *(asPWORD*)GREF___StaticType_UStaticJITAotFunctionCarrier.Get());
// CHKREF 
if (((asQWORD&)l_stack[0]) == 0) [[unlikely]]
{
SCRIPT_DEBUG_CALLSTACK_LINE(52);
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
// CALLSYS *
// UClass TSubclassOf::opImplConv() const
{
  asFUNCTION_t RawFuncPtr = SYSPTR_TSubclassOf_UObject__opImplConv.GetFunction();
  auto CastedFuncPtr = (UObject*(*)(void*))RawFuncPtr;
  void* Object = (void*)((asQWORD&)l_stack[0]);
SCRIPT_DEBUG_CALLSTACK_LINE(52);
  if(Object != nullptr)
  {
UObject* FunctionReturnValue = CastedFuncPtr(Object);
l_objectRegister = (void*)FunctionReturnValue;
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// STOREOBJ v2
v_TEMP_ptr_2 = (UClass*)l_objectRegister;
l_objectRegister = nullptr;
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_ptr_2;
v_TEMP_ptr_2 = nullptr;
// RET 0
  return (UClass*)l_objectRegister;
}
static void AS_ASStaticJITAotFixture__StaticClass_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UClass**)l_outValue = AS_ASStaticJITAotFixture__StaticClass(Execution);
}
static void AS_ASStaticJITAotFixture__StaticClass_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UClass**)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__StaticClass(Execution);
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__StaticClass_Register(0x6fccf793u, &AS_ASStaticJITAotFixture__StaticClass_VMEntry, &AS_ASStaticJITAotFixture__StaticClass_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__StaticClass);

#endif
#endif
