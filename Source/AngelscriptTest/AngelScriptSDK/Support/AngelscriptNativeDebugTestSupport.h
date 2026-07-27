#pragma once

#include "AngelscriptNativeCoreTestSupport.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptNativeTestSupport
{
	struct FScopedNativeDebugCallbacks
	{
		FScopedNativeDebugCallbacks()
			: PreviousCanEverRunLineCallback(asCContext::CanEverRunLineCallback)
			, PreviousShouldAlwaysRunLineCallback(asCContext::ShouldAlwaysRunLineCallback)
		{
			// Raw SDK engines do not run the host runtime's debug-state update.
			asCContext::CanEverRunLineCallback = true;
			asCContext::ShouldAlwaysRunLineCallback = true;
		}

		~FScopedNativeDebugCallbacks()
		{
			asCContext::CanEverRunLineCallback = PreviousCanEverRunLineCallback;
			asCContext::ShouldAlwaysRunLineCallback = PreviousShouldAlwaysRunLineCallback;
		}

		bool PreviousCanEverRunLineCallback = false;
		bool PreviousShouldAlwaysRunLineCallback = false;
	};

	inline constexpr asPWORD NativeDebugRecorderUserDataSlot = static_cast<asPWORD>(0x4E41544445425547ull);

	enum class ENativeDebugEventKind : uint8
	{
		Exception,
		Instruction,
		Line,
		Loop,
		StackPop,
	};

	struct FNativeDebugEvent
	{
		ENativeDebugEventKind Kind = ENativeDebugEventKind::Instruction;
		int32 Line = INDEX_NONE;
		int32 Column = INDEX_NONE;
		FString Section;
		FString FunctionDeclaration;
		FString Text;
		asEVMInstructionPhase InstructionPhase = asVM_BEFORE_INSTRUCTION;
		uint8 Instruction = 0;
		int32 BytecodeOffset = INDEX_NONE;
		uint32 CallstackDepth = 0;
		UPTRINT PointerBegin = 0;
		UPTRINT PointerEnd = 0;
	};

	class FNativeDebugRecorder
	{
	public:
		void Reset()
		{
			Events.Reset();
		}

		void Add(FNativeDebugEvent Event)
		{
			Events.Add(MoveTemp(Event));
		}

		const TArray<FNativeDebugEvent>& GetEvents() const
		{
			return Events;
		}

		int32 Num(const ENativeDebugEventKind Kind) const
		{
			int32 Count = 0;
			for (const FNativeDebugEvent& Event : Events)
			{
				if (Event.Kind == Kind)
				{
					++Count;
				}
			}
			return Count;
		}

	private:
		TArray<FNativeDebugEvent> Events;
	};

	inline FNativeDebugRecorder* GetNativeDebugRecorder(asIScriptContext* Context)
	{
		return Context != nullptr
			? static_cast<FNativeDebugRecorder*>(Context->GetUserData(NativeDebugRecorderUserDataSlot))
			: nullptr;
	}

	inline void CaptureNativeException(asIScriptContext* Context, void* UserData)
	{
		FNativeDebugRecorder* const Recorder = static_cast<FNativeDebugRecorder*>(UserData);
		if (Context == nullptr || Recorder == nullptr)
		{
			return;
		}

		FNativeDebugEvent Event;
		Event.Kind = ENativeDebugEventKind::Exception;
		const char* Section = nullptr;
		Event.Line = Context->GetExceptionLineNumber(&Event.Column, &Section);
		Event.Section = UTF8_TO_TCHAR(Section != nullptr ? Section : "");
		Event.Text = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		if (asIScriptFunction* const Function = Context->GetExceptionFunction())
		{
			Event.FunctionDeclaration = UTF8_TO_TCHAR(Function->GetDeclaration());
		}
		Event.CallstackDepth = Context->GetCallstackSize();
		Recorder->Add(MoveTemp(Event));
	}

	inline void CaptureNativeInstruction(asIScriptContext*, const asSVMInstructionInfo* Info, void* UserData)
	{
		FNativeDebugRecorder* const Recorder = static_cast<FNativeDebugRecorder*>(UserData);
		if (Info == nullptr || Recorder == nullptr)
		{
			return;
		}

		FNativeDebugEvent Event;
		Event.Kind = ENativeDebugEventKind::Instruction;
		Event.InstructionPhase = Info->Phase;
		Event.Instruction = Info->Instruction;
		Event.Text = UTF8_TO_TCHAR(Info->InstructionName != nullptr ? Info->InstructionName : "");
		Event.BytecodeOffset = Info->BytecodeOffset;
		Event.CallstackDepth = Info->CallstackDepth;
		if (Info->CurrentFunction != nullptr)
		{
			Event.FunctionDeclaration = UTF8_TO_TCHAR(Info->CurrentFunction->GetDeclaration());
		}
		Recorder->Add(MoveTemp(Event));
	}

	inline void CaptureNativeLine(asCContext* Context)
	{
		FNativeDebugRecorder* const Recorder = GetNativeDebugRecorder(Context);
		if (Context == nullptr || Recorder == nullptr)
		{
			return;
		}

		FNativeDebugEvent Event;
		Event.Kind = ENativeDebugEventKind::Line;
		const char* Section = nullptr;
		Event.Line = Context->GetLineNumber(0, &Event.Column, &Section);
		Event.Section = UTF8_TO_TCHAR(Section != nullptr ? Section : "");
		if (asIScriptFunction* const Function = Context->GetFunction(0))
		{
			Event.FunctionDeclaration = UTF8_TO_TCHAR(Function->GetDeclaration());
		}
		Event.CallstackDepth = Context->GetCallstackSize();
		Recorder->Add(MoveTemp(Event));
	}

	inline void CaptureNativeLoop(asCContext* Context)
	{
		FNativeDebugRecorder* const Recorder = GetNativeDebugRecorder(Context);
		if (Context == nullptr || Recorder == nullptr)
		{
			return;
		}

		FNativeDebugEvent Event;
		Event.Kind = ENativeDebugEventKind::Loop;
		Event.Line = Context->GetLineNumber(0, nullptr, nullptr);
		Event.CallstackDepth = Context->GetCallstackSize();
		Recorder->Add(MoveTemp(Event));
	}

	inline void CaptureNativeStackPop(asCContext* Context, void* OldStackFrameStart, void* OldStackFrameEnd)
	{
		FNativeDebugRecorder* const Recorder = GetNativeDebugRecorder(Context);
		if (Recorder == nullptr)
		{
			return;
		}

		FNativeDebugEvent Event;
		Event.Kind = ENativeDebugEventKind::StackPop;
		Event.PointerBegin = reinterpret_cast<UPTRINT>(OldStackFrameStart);
		Event.PointerEnd = reinterpret_cast<UPTRINT>(OldStackFrameEnd);
		Event.CallstackDepth = Context != nullptr ? Context->GetCallstackSize() : 0;
		Recorder->Add(MoveTemp(Event));
	}
}
