/*
   AngelCode Scripting Library
   Copyright (c) 2003-2014 Andreas Jonsson

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
// as_memory.h
//
// Overload the default memory management functions so that we
// can let the application decide how to do it.
//



#ifndef AS_MEMORY_H
#define AS_MEMORY_H

#include "as_config.h"

BEGIN_AS_NAMESPACE

extern asALLOCSCRIPTOBJECTFUNC_t  userAllocScriptObject;
extern asFINISHCONSTRUCTOBJECTFUNC_t  userFinishConstructObject;
extern asRESOLVEOBJECTPTRFUNC_t  userResolveObjectPtr;

#ifdef WIP_16BYTE_ALIGN

// TODO: This declaration should be in angelscript.h
//       when the application can register it's own
//       aligned memory routines
typedef void *(*asALLOCALIGNEDFUNC_t)(size_t, size_t);
typedef void (*asFREEALIGNEDFUNC_t)(void *);
extern asALLOCALIGNEDFUNC_t userAllocAligned;
extern asFREEALIGNEDFUNC_t  userFreeAligned;
typedef void *(*asALLOCALIGNEDFUNCDEBUG_t)(size_t, size_t, const char *, unsigned int);

// The maximum type alignment supported.
const int MAX_TYPE_ALIGNMENT = 16;

// Utility function used for assertions.
bool isAligned(const void* const pointer, asUINT alignment);

#endif // WIP_16BYTE_ALIGN

// We don't overload the new operator as that would affect the application as well

//[UE++]: Route AS-internal allocations through a single inline gateway so the
//[UE++]: AngelscriptRuntime LLM tag (Angelscript) attributes the byte count to
//[UE++]: the right bucket. The gateway preserves per-type alignment that the
//[UE++]: original `FMemory::Malloc(size, alignof(x))` macros relied on.
//[UE++]: Declared in the global namespace (independent of AS_USE_NAMESPACE) so
//[UE++]: the asNEW/asNEWARRAY macros expand to a fully qualified path no matter
//[UE++]: whether they fire inside or outside `BEGIN_AS_NAMESPACE`.
END_AS_NAMESPACE
namespace AngelscriptSDK
{
	ANGELSCRIPTRUNTIME_API void* SDKAlloc(size_t Size, size_t Alignment);
	ANGELSCRIPTRUNTIME_API void  SDKFree(void* Ptr);
}
BEGIN_AS_NAMESPACE

#define asNEW(x)        new(::AngelscriptSDK::SDKAlloc(sizeof(x), alignof(x))) x
#define asDELETE(ptr,x) {void *tmp = ptr; (ptr)->~x(); ::AngelscriptSDK::SDKFree(tmp);}

#define asNEWARRAY(x,cnt)  (x*)::AngelscriptSDK::SDKAlloc(sizeof(x)*cnt, alignof(x))
#define asDELETEARRAY(ptr) ::AngelscriptSDK::SDKFree(ptr)
//[UE--]

#ifdef WIP_16BYTE_ALIGN
	#define asNEWARRAYALIGNED(x,cnt, alignment)  (x*)userAllocAligned(sizeof(x)*cnt, alignment)
	#define asDELETEARRAYALIGNED(ptr) userFreeAligned(ptr)
#endif

END_AS_NAMESPACE

#include <new>
#include "Containers/Array.h"
#include "as_criticalsection.h"
#include "as_array.h"

BEGIN_AS_NAMESPACE

//[UE++]: Export memory manager internals for the AngelscriptTest module.
class ANGELSCRIPTRUNTIME_API asCMemoryMgr
//[UE--]
{
public:
	asCMemoryMgr();
	~asCMemoryMgr();

	void FreeUnusedMemory();

	//[UE++]: Restore the stock memory-pool surface so internals tests can exercise
	//[UE++]: script-node and byte-instruction allocation reuse on the APV2 branch.
	void *AllocScriptNode();
	void FreeScriptNode(void *ptr);

#ifndef AS_NO_COMPILER
	void *AllocByteInstruction();
	void FreeByteInstruction(void *ptr);
#endif

protected:
	DECLARECRITICALSECTION(cs);
	TArray<void*> scriptNodePool;
	TArray<void*> byteInstructionPool;
	//[UE--]
};

END_AS_NAMESPACE

#endif
