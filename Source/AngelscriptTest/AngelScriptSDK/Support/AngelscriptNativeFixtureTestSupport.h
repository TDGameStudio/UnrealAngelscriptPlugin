#pragma once

#include "AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeLifecycleTestSupport.h"

namespace AngelscriptNativeTestSupport
{
	inline constexpr asPWORD NativeLifecycleRecorderUserDataSlot = static_cast<asPWORD>(0x4E41544C49464543ull);
	inline constexpr asPWORD NativeLifecycleFaultUserDataSlot = static_cast<asPWORD>(0x4E41544C4641554Cull);

	inline FNativeLifecycleRecorder* GetActiveNativeLifecycleRecorder()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FNativeLifecycleRecorder*>(Context->GetEngine()->GetUserData(NativeLifecycleRecorderUserDataSlot))
			: nullptr;
	}

	inline FNativeLifecycleFaultController* GetActiveNativeLifecycleFaultController()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
			? static_cast<FNativeLifecycleFaultController*>(Context->GetEngine()->GetUserData(NativeLifecycleFaultUserDataSlot))
			: nullptr;
	}

	inline void ConstructNativeCaseValue(FNativeTrackedValue* Address)
	{
		new (Address) FNativeTrackedValue(GetActiveNativeLifecycleRecorder());
	}

	inline void ConstructNativeCaseValueWithInt(const int32 Value, FNativeTrackedValue* Address)
	{
		new (Address) FNativeTrackedValue(GetActiveNativeLifecycleRecorder(), Value);
	}

	inline void ConstructNativeCaseValueWithIntPair(
		const int32 Left,
		const int32 Right,
		FNativeTrackedValue* Address)
	{
		new (Address) FNativeTrackedValue(
			GetActiveNativeLifecycleRecorder(),
			Left + Right);
	}

	inline void ConstructNativeCaseValueWithInt64(
		const int64 Value,
		FNativeTrackedValue* Address)
	{
		new (Address) FNativeTrackedValue(
			GetActiveNativeLifecycleRecorder(),
			static_cast<int32>(Value));
	}

	inline void CopyConstructNativeCaseValue(const FNativeTrackedValue& Other, FNativeTrackedValue* Address)
	{
		new (Address) FNativeTrackedValue(Other);
		FNativeLifecycleFaultController* const FaultController = GetActiveNativeLifecycleFaultController();
		if (FaultController != nullptr && FaultController->ConsumeCopyFault())
		{
			// The behavior callback has completed placement construction before it
			// reports the script exception. The public behavior API has no separate
			// partially-constructed return channel, so it owns retirement of this
			// destination before allowing VM exception cleanup to continue.
			Address->~FNativeTrackedValue();
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Native case value copy construction fault");
			}
		}
	}

	inline void ArmNextNativeCaseValueCopyFault()
	{
		if (FNativeLifecycleFaultController* const FaultController = GetActiveNativeLifecycleFaultController())
		{
			FaultController->ArmNextCopy();
		}
	}

	inline void DestructNativeCaseValue(FNativeTrackedValue* Address)
	{
		Address->~FNativeTrackedValue();
	}

	inline bool RegisterNativeCaseValue(
		asIScriptEngine& Engine,
		FNativeLifecycleRecorder& Recorder,
		FNativeLifecycleFaultController* FaultController = nullptr)
	{
		Engine.SetUserData(&Recorder, NativeLifecycleRecorderUserDataSlot);
		Engine.SetUserData(FaultController, NativeLifecycleFaultUserDataSlot);
		const ASAutoCaller::FunctionCaller DefaultConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeCaseValue);
		const ASAutoCaller::FunctionCaller ValueConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeCaseValueWithInt);
		const ASAutoCaller::FunctionCaller PairConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeCaseValueWithIntPair);
		const ASAutoCaller::FunctionCaller Int64ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeCaseValueWithInt64);
		const ASAutoCaller::FunctionCaller CopyConstructorCaller = ASAutoCaller::MakeFunctionCaller(CopyConstructNativeCaseValue);
		const ASAutoCaller::FunctionCaller DestructorCaller = ASAutoCaller::MakeFunctionCaller(DestructNativeCaseValue);
		const ASAutoCaller::FunctionCaller AssignmentCaller = ASAutoCaller::MakeFunctionCaller(
			static_cast<FNativeTrackedValue& (FNativeTrackedValue::*)(const FNativeTrackedValue&)>(&FNativeTrackedValue::operator=));
		const ASAutoCaller::FunctionCaller ArmCopyFaultCaller = ASAutoCaller::MakeFunctionCaller(ArmNextNativeCaseValueCopyFault);
		return Engine.RegisterObjectType(
			"FNativeCaseValue",
			sizeof(FNativeTrackedValue),
			asOBJ_VALUE | asGetTypeTraits<FNativeTrackedValue>() | asOBJ_APP_CLASS_ALLINTS) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseValue", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructNativeCaseValue), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&DefaultConstructorCaller) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseValue", asBEHAVE_CONSTRUCT, "void f(int Value)", asFUNCTION(ConstructNativeCaseValueWithInt), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ValueConstructorCaller) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseValue", asBEHAVE_CONSTRUCT, "void f(int Left, int Right)", asFUNCTION(ConstructNativeCaseValueWithIntPair), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&PairConstructorCaller) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseValue", asBEHAVE_CONSTRUCT, "void f(int64 Value)", asFUNCTION(ConstructNativeCaseValueWithInt64), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&Int64ConstructorCaller) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseValue", asBEHAVE_CONSTRUCT, "void f(const FNativeCaseValue& in Other)", asFUNCTION(CopyConstructNativeCaseValue), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&CopyConstructorCaller) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseValue", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(DestructNativeCaseValue), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&DestructorCaller) >= 0
			&& Engine.RegisterObjectMethod("FNativeCaseValue", "FNativeCaseValue& opAssign(const FNativeCaseValue& in Other)", asMETHODPR(FNativeTrackedValue, operator=, (const FNativeTrackedValue&), FNativeTrackedValue&), asCALL_THISCALL, *(asFunctionCaller*)&AssignmentCaller) >= 0
			&& Engine.RegisterObjectProperty("FNativeCaseValue", "int Value", asOFFSET(FNativeTrackedValue, Value)) >= 0
			&& Engine.RegisterGlobalFunction("void ArmNextNativeCaseValueCopyFault()", asFUNCTION(ArmNextNativeCaseValueCopyFault), asCALL_CDECL, *(asFunctionCaller*)&ArmCopyFaultCaller) >= 0;
	}

	inline int32 BeginNativeScriptLifecycle(const int32 Value)
	{
		FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			return INDEX_NONE;
		}

		const int32 ObjectId = Recorder->AllocateObjectId();
		Recorder->Record(ENativeLifecycleEvent::ValueConstruct, ObjectId, INDEX_NONE, Value);
		return ObjectId;
	}

	inline int32 CopyNativeScriptLifecycle(const int32 SourceObjectId, const int32 Value)
	{
		FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			return INDEX_NONE;
		}

		const int32 ObjectId = Recorder->AllocateObjectId();
		Recorder->Record(ENativeLifecycleEvent::CopyConstruct, ObjectId, SourceObjectId, Value);
		return ObjectId;
	}

	inline void AssignNativeScriptLifecycle(
		const int32 ObjectId,
		const int32 SourceObjectId,
		const int32 Value)
	{
		if (FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder())
		{
			Recorder->Record(ENativeLifecycleEvent::Assign, ObjectId, SourceObjectId, Value);
		}
	}

	inline void EndNativeScriptLifecycle(const int32 ObjectId, const int32 Value)
	{
		if (FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder())
		{
			Recorder->Record(ENativeLifecycleEvent::Destruct, ObjectId, INDEX_NONE, Value);
		}
	}

	inline bool RegisterNativeScriptLifecycleBridge(
		asIScriptEngine& Engine,
		FNativeLifecycleRecorder& Recorder)
	{
		Engine.SetUserData(&Recorder, NativeLifecycleRecorderUserDataSlot);
		const ASAutoCaller::FunctionCaller BeginCaller = ASAutoCaller::MakeFunctionCaller(BeginNativeScriptLifecycle);
		const ASAutoCaller::FunctionCaller CopyCaller = ASAutoCaller::MakeFunctionCaller(CopyNativeScriptLifecycle);
		const ASAutoCaller::FunctionCaller AssignCaller = ASAutoCaller::MakeFunctionCaller(AssignNativeScriptLifecycle);
		const ASAutoCaller::FunctionCaller EndCaller = ASAutoCaller::MakeFunctionCaller(EndNativeScriptLifecycle);
		return Engine.RegisterGlobalFunction(
			"int BeginNativeScriptLifecycle(int Value)",
			asFUNCTION(BeginNativeScriptLifecycle),
			asCALL_CDECL,
			*(asFunctionCaller*)&BeginCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"int CopyNativeScriptLifecycle(int SourceObjectId, int Value)",
				asFUNCTION(CopyNativeScriptLifecycle),
				asCALL_CDECL,
				*(asFunctionCaller*)&CopyCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void AssignNativeScriptLifecycle(int ObjectId, int SourceObjectId, int Value)",
				asFUNCTION(AssignNativeScriptLifecycle),
				asCALL_CDECL,
				*(asFunctionCaller*)&AssignCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void EndNativeScriptLifecycle(int ObjectId, int Value)",
				asFUNCTION(EndNativeScriptLifecycle),
				asCALL_CDECL,
				*(asFunctionCaller*)&EndCaller) >= 0;
	}

	class FNativeCaseReference
	{
	public:
		explicit FNativeCaseReference(FNativeLifecycleRecorder& InRecorder, const int32 InValue = 0)
			: Recorder(InRecorder)
			, ObjectId(Recorder.AllocateObjectId())
			, Value(InValue)
		{
			Recorder.Record(ENativeLifecycleEvent::ValueConstruct, ObjectId, INDEX_NONE, Value);
		}

		void AddRef()
		{
			++ReferenceCount;
			Recorder.Record(ENativeLifecycleEvent::AddRef, ObjectId, INDEX_NONE, ReferenceCount);
		}

		void Release()
		{
			const int32 NewReferenceCount = --ReferenceCount;
			Recorder.Record(ENativeLifecycleEvent::Release, ObjectId, INDEX_NONE, NewReferenceCount);
			if (NewReferenceCount == 0)
			{
				delete this;
			}
		}

		int32 Value = 0;

	private:
		~FNativeCaseReference()
		{
			Recorder.Record(ENativeLifecycleEvent::Destruct, ObjectId, INDEX_NONE, Value);
		}

		FNativeLifecycleRecorder& Recorder;
		int32 ObjectId = INDEX_NONE;
		int32 ReferenceCount = 1;
	};

	inline void AddRefNativeCaseReference(FNativeCaseReference* Object)
	{
		if (Object != nullptr)
		{
			Object->AddRef();
		}
	}

	inline void ReleaseNativeCaseReference(FNativeCaseReference* Object)
	{
		if (Object != nullptr)
		{
			Object->Release();
		}
	}

	inline void CreateNativeCaseReference(asIScriptGeneric* Generic)
	{
		FNativeLifecycleRecorder* const Recorder = GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			Generic->SetReturnAddress(nullptr);
			return;
		}

		const int32 Value = static_cast<int32>(Generic->GetArgDWord(0));
		Generic->SetReturnAddress(new FNativeCaseReference(*Recorder, Value));
	}

	inline bool RegisterNativeCaseReference(
		asIScriptEngine& Engine,
		FNativeLifecycleRecorder* Recorder = nullptr)
	{
		if (Recorder != nullptr)
		{
			Engine.SetUserData(Recorder, NativeLifecycleRecorderUserDataSlot);
		}
		return Engine.RegisterObjectType(
			"FNativeCaseReference",
			0,
			asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseReference", asBEHAVE_ADDREF, "void f()", asFUNCTION(AddRefNativeCaseReference), asCALL_CDECL_OBJFIRST) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseReference", asBEHAVE_RELEASE, "void f()", asFUNCTION(ReleaseNativeCaseReference), asCALL_CDECL_OBJFIRST) >= 0
			&& Engine.RegisterObjectProperty("FNativeCaseReference", "int Value", asOFFSET(FNativeCaseReference, Value)) >= 0
			&& Engine.RegisterGlobalFunction("FNativeCaseReference CreateNativeCaseReference(int Value)", asFUNCTION(CreateNativeCaseReference), asCALL_GENERIC) >= 0;
	}

	struct FNativeCaseRange
	{
		int32 Values[4] = { 0, 0, 0, 0 };
		int32 Count = 0;
		FNativeLifecycleRecorder* Recorder = nullptr;

		int32 opForBegin()
		{
			if (Recorder != nullptr)
			{
				Recorder->Record(ENativeLifecycleEvent::IteratorBegin, INDEX_NONE, INDEX_NONE, Count);
			}
			return Count > 0 ? 0 : INDEX_NONE;
		}

		void opForNext(int32& Iterator)
		{
			++Iterator;
			if (Iterator >= Count)
			{
				Iterator = INDEX_NONE;
			}
			if (Recorder != nullptr)
			{
				Recorder->Record(ENativeLifecycleEvent::IteratorNext, INDEX_NONE, INDEX_NONE, Iterator);
			}
		}

		bool opForEnd(const int32 Iterator) const
		{
			return Iterator < 0;
		}

		int32& opForValue(const int32 Iterator)
		{
			if (Recorder != nullptr)
			{
				Recorder->Record(ENativeLifecycleEvent::IteratorValue, INDEX_NONE, INDEX_NONE, Iterator);
			}
			return Values[Iterator];
		}
	};

	inline void ConstructNativeCaseRange(FNativeCaseRange* Address)
	{
		new (Address) FNativeCaseRange();
		Address->Recorder = GetActiveNativeLifecycleRecorder();
	}

	inline void RaiseNativeCaseException()
	{
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("foreach callback exception");
		}
	}

	inline bool RegisterNativeCaseRange(asIScriptEngine& Engine, FNativeLifecycleRecorder& Recorder)
	{
		Engine.SetUserData(&Recorder, NativeLifecycleRecorderUserDataSlot);
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeCaseRange);
		const ASAutoCaller::FunctionCaller BeginCaller = ASAutoCaller::MakeFunctionCaller(&FNativeCaseRange::opForBegin);
		const ASAutoCaller::FunctionCaller EndCaller = ASAutoCaller::MakeFunctionCaller(&FNativeCaseRange::opForEnd);
		const ASAutoCaller::FunctionCaller NextCaller = ASAutoCaller::MakeFunctionCaller(&FNativeCaseRange::opForNext);
		const ASAutoCaller::FunctionCaller ValueCaller = ASAutoCaller::MakeFunctionCaller(&FNativeCaseRange::opForValue);
		const ASAutoCaller::FunctionCaller ExceptionCaller = ASAutoCaller::MakeFunctionCaller(RaiseNativeCaseException);
		return Engine.RegisterObjectType("FNativeCaseRange", sizeof(FNativeCaseRange), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS) >= 0
			&& Engine.RegisterObjectBehaviour("FNativeCaseRange", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ConstructNativeCaseRange), asCALL_CDECL_OBJLAST, *(asFunctionCaller*)&ConstructorCaller) >= 0
			&& Engine.RegisterObjectProperty("FNativeCaseRange", "int Count", asOFFSET(FNativeCaseRange, Count)) >= 0
			&& Engine.RegisterObjectMethod("FNativeCaseRange", "int opForBegin()", asMETHOD(FNativeCaseRange, opForBegin), asCALL_THISCALL, *(asFunctionCaller*)&BeginCaller) >= 0
			&& Engine.RegisterObjectMethod("FNativeCaseRange", "bool opForEnd(const int Iterator)", asMETHOD(FNativeCaseRange, opForEnd), asCALL_THISCALL, *(asFunctionCaller*)&EndCaller) >= 0
			&& Engine.RegisterObjectMethod("FNativeCaseRange", "void opForNext(int& inout Iterator)", asMETHOD(FNativeCaseRange, opForNext), asCALL_THISCALL, *(asFunctionCaller*)&NextCaller) >= 0
			&& Engine.RegisterObjectMethod("FNativeCaseRange", "int& opForValue(const int Iterator)", asMETHOD(FNativeCaseRange, opForValue), asCALL_THISCALL, *(asFunctionCaller*)&ValueCaller) >= 0
			&& Engine.RegisterGlobalFunction("void RaiseNativeCaseException()", asFUNCTION(RaiseNativeCaseException), asCALL_CDECL, *(asFunctionCaller*)&ExceptionCaller) >= 0;
	}
}
