#include "StaticJIT/StaticJITConfig.h"
#if UE_BUILD_DEVELOPMENT
#ifndef AS_SKIP_JITTED_CODE

#include "../../../Engine/Source/Runtime/CoreUObject/Public/UObject/Object.h"

#include "StaticJIT/StaticJITHeader.h"


AS_FORCE_LINK FJitRef_Type TREF_UObject(0x216f8a1bd00);
AS_FORCE_LINK FJitRef_Type TREF_UStaticJITAotFunctionCarrier(0x21700a4c600);
AS_FORCE_LINK FJitRef_GlobalVar GREF___StaticType_UStaticJITAotFunctionCarrier(0x216ffee4200);
AS_FORCE_LINK FJitRef_SystemFunctionPointer SYSPTR_FAotObjectLastProbe___beh0(0x21700f13c00);
AS_FORCE_LINK FJitRef_SystemFunctionPointer SYSPTR___WorldContext(0x216fc85f800);
AS_FORCE_LINK FJitRef_SystemFunctionPointer SYSPTR_TSubclassOf_UObject__opImplConv(0x21700f15e00);

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
AS_FORCE_LINK FJitVerifyPropertyOffset PVERIFY_UStaticJITAotFunctionCarrier_StoredValue(0x60100075ab, POFFSET_UStaticJITAotFunctionCarrier_StoredValue);
void AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("void UStaticJITAotFunctionCarrier::StorePrimitiveArg(int)", 35);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x4aed6a11u);
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
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_Register(0x4aed6a11u, &AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_VMEntry, &AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__StorePrimitiveArg);

int32 AS_UStaticJITAotFunctionCarrier__ReturnPrimitive(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::ReturnPrimitive()", 41);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0xcdaa99e8u);
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
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_Register(0xcdaa99e8u, &AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_VMEntry, &AS_UStaticJITAotFunctionCarrier__ReturnPrimitive_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__ReturnPrimitive);

int32 AS_UStaticJITAotFunctionCarrier__BumpReference(FScriptExecution& Execution, UObject* l_This, int32* p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UStaticJITAotFunctionCarrier::BumpReference(int&)", 47);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0xe08e87e3u);
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
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__BumpReference_Register(0xe08e87e3u, &AS_UStaticJITAotFunctionCarrier__BumpReference_VMEntry, &AS_UStaticJITAotFunctionCarrier__BumpReference_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__BumpReference);

UObject* AS_UStaticJITAotFunctionCarrier__ReturnSelfObject(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UObject UStaticJITAotFunctionCarrier::ReturnSelfObject()", 55);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x9fe56f97u);
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
UObject* v_TEMP_ptr_2 = nullptr;
// SUSPEND
// PshVPtr v0
// RefCpyV v2, *
{
  asPWORD* d = (asPWORD*)(&v_TEMP_ptr_2);
  asPWORD s = (asPWORD)(l_This);
  asCObjectType* objType = (asCObjectType*)TREF_UObject.Get();
  if (*d != s)
  {
    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)
    {
      if (*d != 0 && objType->beh.release != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);
      if (s != 0 && objType->beh.addref != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);
    }
  }
  if (*d != s)
    *d = s;
}
// PopPtr
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_ptr_2;
v_TEMP_ptr_2 = nullptr;
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
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_Register(0x9fe56f97u, &AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_VMEntry, &AS_UStaticJITAotFunctionCarrier__ReturnSelfObject_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__ReturnSelfObject);

void AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UStaticJITAotFunctionCarrier::UStaticJITAotFunctionCarrier()", 30);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x6afb1262u);
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
AS_FORCE_LINK static const FStaticJITFunction AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_Register(0x6afb1262u, &AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_VMEntry, &AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier_ParmsEntry, (asJITFunction_Raw)(void*)&AS_UStaticJITAotFunctionCarrier__UStaticJITAotFunctionCarrier);

UObject* AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UStaticJITAotFunctionCarrier UStaticJITAotFunctionCarrier()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0xa9f33334u);
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
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_Register(0xa9f33334u, &AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_VMEntry, &AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__UStaticJITAotFunctionCarrier);

int32 AS_ASStaticJITAotFixture__AddForAOT(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int AddForAOT(int)", 3);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x403f6aa7u);
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
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__AddForAOT_Register(0x403f6aa7u, &AS_ASStaticJITAotFixture__AddForAOT_VMEntry, &AS_ASStaticJITAotFixture__AddForAOT_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__AddForAOT);

int32 AS_ASStaticJITAotFixture__Entry(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int Entry()", 8);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x39d2b91cu);
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
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__Entry_Register(0x39d2b91cu, &AS_ASStaticJITAotFixture__Entry_VMEntry, &AS_ASStaticJITAotFixture__Entry_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__Entry);

int64 AS_ASStaticJITAotFixture__DoubleToInt64ForAOT(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int64 DoubleToInt64ForAOT(float)", 13);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0xc758bd85u);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
// SUSPEND
// dTOi64 v2, v0
((int64&)v_TEMP_qword_2) = int64(p_Value);
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 2
  return (int64)l_valueRegister;
}
static void AS_ASStaticJITAotFixture__DoubleToInt64ForAOT_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int64*)l_outValue = AS_ASStaticJITAotFixture__DoubleToInt64ForAOT(Execution,
		*(double*)(l_fp + 0));
}
static void AS_ASStaticJITAotFixture__DoubleToInt64ForAOT_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(int64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int64*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__DoubleToInt64ForAOT(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__DoubleToInt64ForAOT_Register(0xc758bd85u, &AS_ASStaticJITAotFixture__DoubleToInt64ForAOT_VMEntry, &AS_ASStaticJITAotFixture__DoubleToInt64ForAOT_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__DoubleToInt64ForAOT);

uint64 AS_ASStaticJITAotFixture__DoubleToUint64ForAOT(FScriptExecution& Execution, double p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("uint64 DoubleToUint64ForAOT(float)", 18);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x471d2af7u);
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asQWORD v_TEMP_qword_2 = {};
// SUSPEND
// dTOu64 v2, v0
v_TEMP_qword_2 = uint64(p_Value);
// CpyVtoR8 v2
l_valueRegister = v_TEMP_qword_2;
// RET 2
  return (uint64)l_valueRegister;
}
static void AS_ASStaticJITAotFixture__DoubleToUint64ForAOT_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(uint64*)l_outValue = AS_ASStaticJITAotFixture__DoubleToUint64ForAOT(Execution,
		*(double*)(l_fp + 0));
}
static void AS_ASStaticJITAotFixture__DoubleToUint64ForAOT_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(uint64));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(uint64*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__DoubleToUint64ForAOT(Execution,
		(double)*(double*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__DoubleToUint64ForAOT_Register(0x471d2af7u, &AS_ASStaticJITAotFixture__DoubleToUint64ForAOT_VMEntry, &AS_ASStaticJITAotFixture__DoubleToUint64ForAOT_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__DoubleToUint64ForAOT);

int32 AS_ASStaticJITAotFixture__ObjectLastNativeForAOT(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int ObjectLastNativeForAOT()", 23);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x419fb26cu);
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
alignas(8) asBYTE MEM_v_Value[4];
FUnknownValueType& v_Value = (FUnknownValueType&)MEM_v_Value[0];
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// SUSPEND
// PshC4 97
// PshC4 39
// PSF v2
// CALLSYS *
// FAotObjectLastProbe::FAotObjectLastProbe(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = SYSPTR_FAotObjectLastProbe___beh0.GetFunction();
  auto CastedFuncPtr = (void(*)(int32,int32,void*))RawFuncPtr;
  void* Object = (void*)((&v_Value));
SCRIPT_DEBUG_CALLSTACK_LINE(23);
CastedFuncPtr(value_as<int32>(((asDWORD)0x27u)),value_as<int32>(((asDWORD)0x61u)),Object);
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// SUSPEND
// LoadVObjR
l_valueRegister = (asQWORD)((&v_Value)) + (short)0;
// RDR4 v3
v_TEMP_dword_3 = value_read<asDWORD>((void*)l_valueRegister);
// CpyVtoR4 v3
l_dwordRegister = v_TEMP_dword_3;
// RET 0
  return (int32)l_dwordRegister;
}
static void AS_ASStaticJITAotFixture__ObjectLastNativeForAOT_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_ASStaticJITAotFixture__ObjectLastNativeForAOT(Execution);
}
static void AS_ASStaticJITAotFixture__ObjectLastNativeForAOT_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_ASStaticJITAotFixture__ObjectLastNativeForAOT(Execution);
}
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__ObjectLastNativeForAOT_Register(0x419fb26cu, &AS_ASStaticJITAotFixture__ObjectLastNativeForAOT_VMEntry, &AS_ASStaticJITAotFixture__ObjectLastNativeForAOT_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__ObjectLastNativeForAOT);

int32 AS_ASStaticJITAotFixture__StaticWorldContextCheck(FScriptExecution& Execution, UObject* p_WorldContextObject, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int StaticWorldContextCheck(UObject, int)", 62);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0xb0f546d1u);
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
SCRIPT_DEBUG_CALLSTACK_LINE(62);
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
// RefCpyV v4, *
{
  asPWORD* d = (asPWORD*)(&v_TEMP_ptr_4);
  asPWORD s = (asPWORD)(p_WorldContextObject);
  asCObjectType* objType = (asCObjectType*)TREF_UObject.Get();
  if (*d != s)
  {
    if ((objType->flags & (asOBJ_NOCOUNT | asOBJ_VALUE)) == 0)
    {
      if (*d != 0 && objType->beh.release != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)*d, objType->beh.release);
      if (s != 0 && objType->beh.addref != 0)
        SCRIPT_ENGINE->CallObjectMethod((void*)s, objType->beh.addref);
    }
  }
  if (*d != s)
    *d = s;
}
// PopPtr
// CmpPtr v2, v4
l_byteRegister = ((void*)v_TEMP_ptr_2 == (void*)v_TEMP_ptr_4) ? 0 : 1;
// TNZ
if(l_byteRegister != 0)
   l_byteRegister = 1;
else
   l_byteRegister = 0;
// CpyRtoV4 v5
v_TEMP_byte_5 = l_byteRegister;
// FREE v2, *
{
  void* obj = (void*)v_TEMP_ptr_2;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)TREF_UObject.Get();
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
v_TEMP_ptr_2 = nullptr;
// FREE v4, *
{
  void* obj = (void*)v_TEMP_ptr_4;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)TREF_UObject.Get();
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
v_TEMP_ptr_4 = nullptr;
// CpyVtoR1 v5
l_byteRegister = v_TEMP_byte_5;
// JLowZ 6
if(l_byteRegister == 0) {
goto LABEL_StaticWorldContextCheck_29;
}
// SUSPEND
// SetV4 v6, -1
v_TEMP_dword_6 = 0xffffffffu;
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
// JMP 5
goto LABEL_StaticWorldContextCheck_34;
LABEL_StaticWorldContextCheck_29:
// SUSPEND
// ADDIi v6, v-2, 2
((int32&)v_TEMP_dword_6) = ((int32&)p_Value) + value_as<int>((asDWORD)0x2u);
// CpyVtoR4 v6
l_dwordRegister = v_TEMP_dword_6;
LABEL_StaticWorldContextCheck_34:
// FREE v0, *
{
  void* obj = (void*)p_WorldContextObject;
  if (obj != nullptr)
  {
    asCObjectType* objType = (asCObjectType*)TREF_UObject.Get();
    if ((objType->flags & asOBJ_NOCOUNT) == 0 && objType->beh.release != 0)
      SCRIPT_ENGINE->CallObjectMethod(obj, objType->beh.release);
  }
}
p_WorldContextObject = nullptr;
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
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__StaticWorldContextCheck_Register(0xb0f546d1u, &AS_ASStaticJITAotFixture__StaticWorldContextCheck_VMEntry, &AS_ASStaticJITAotFixture__StaticWorldContextCheck_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__StaticWorldContextCheck);

UClass* AS_ASStaticJITAotFixture__StaticClass(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UClass UStaticJITAotFunctionCarrier::StaticClass()", 69);
SCRIPT_ASSUME_NO_EXCEPTION()
FStaticJITDiagnosticEntryMarkers::MarkEntry(0x95a1389u);
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
SCRIPT_DEBUG_CALLSTACK_LINE(69);
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
// CALLSYS *
// UClass TSubclassOf::opImplConv() const
{
  asFUNCTION_t RawFuncPtr = SYSPTR_TSubclassOf_UObject__opImplConv.GetFunction();
  auto CastedFuncPtr = (UObject*(*)(void*))RawFuncPtr;
  void* Object = (void*)((asQWORD&)l_stack[0]);
SCRIPT_DEBUG_CALLSTACK_LINE(69);
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
AS_FORCE_LINK static const FStaticJITFunction AS_ASStaticJITAotFixture__StaticClass_Register(0x95a1389u, &AS_ASStaticJITAotFixture__StaticClass_VMEntry, &AS_ASStaticJITAotFixture__StaticClass_ParmsEntry, (asJITFunction_Raw)(void*)&AS_ASStaticJITAotFixture__StaticClass);

#endif
#endif
