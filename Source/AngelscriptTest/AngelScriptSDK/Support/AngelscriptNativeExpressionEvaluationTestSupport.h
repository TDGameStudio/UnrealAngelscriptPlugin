#pragma once

#include "AngelscriptNativeCoreTestSupport.h"

namespace AngelscriptNativeTestSupport
{
	inline constexpr asPWORD ExpressionEvaluationRecorderUserDataSlot =
		static_cast<asPWORD>(0x4E41544558505245ull);

	struct FExpressionEvaluationRecorder
	{
		void Reset()
		{
			Markers.Reset();
		}

		void Record(const int32 Marker)
		{
			Markers.Add(Marker);
		}

		TArray<int32> Markers;
	};

	inline FExpressionEvaluationRecorder* GetActiveExpressionEvaluationRecorder()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FExpressionEvaluationRecorder*>(Context->GetEngine()->GetUserData(
						 ExpressionEvaluationRecorderUserDataSlot))
				   : nullptr;
	}

	inline bool RecordExpressionBool(const int32 Marker, const bool Value)
	{
		if (FExpressionEvaluationRecorder* const Recorder = GetActiveExpressionEvaluationRecorder())
		{
			Recorder->Record(Marker);
		}
		return Value;
	}

	inline int32 RecordExpressionInt(const int32 Marker, const int32 Value)
	{
		if (FExpressionEvaluationRecorder* const Recorder = GetActiveExpressionEvaluationRecorder())
		{
			Recorder->Record(Marker);
		}
		return Value;
	}

	inline int32 RecordEagerStage(const int32 Marker, const int32 Value, const bool bRaiseException)
	{
		if (FExpressionEvaluationRecorder* const Recorder = GetActiveExpressionEvaluationRecorder())
		{
			Recorder->Record(Marker);
		}
		if (bRaiseException)
		{
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Eager expression stage exception");
			}
		}
		return Value;
	}

	inline int32 CompleteEagerBoundary(const int32 Value, const bool bRaiseException)
	{
		if (bRaiseException)
		{
			if (FExpressionEvaluationRecorder* const Recorder =
					GetActiveExpressionEvaluationRecorder())
			{
				Recorder->Record(1000);
			}
			if (asIScriptContext* const Context = asGetActiveContext())
			{
				Context->SetException("Eager expression completion boundary exception");
			}
		}
		return Value;
	}

	inline bool RegisterExpressionEvaluationFunctions(
		asIScriptEngine& Engine, FExpressionEvaluationRecorder& Recorder)
	{
		Engine.SetUserData(&Recorder, ExpressionEvaluationRecorderUserDataSlot);
		const ASAutoCaller::FunctionCaller BoolCaller =
			ASAutoCaller::MakeFunctionCaller(RecordExpressionBool);
		const ASAutoCaller::FunctionCaller IntCaller =
			ASAutoCaller::MakeFunctionCaller(RecordExpressionInt);
		const ASAutoCaller::FunctionCaller EagerStageCaller =
			ASAutoCaller::MakeFunctionCaller(RecordEagerStage);
		const ASAutoCaller::FunctionCaller EagerBoundaryCaller =
			ASAutoCaller::MakeFunctionCaller(CompleteEagerBoundary);
		return Engine.RegisterGlobalFunction("bool RecordExpressionBool(int Marker, bool Value)",
				   asFUNCTION(RecordExpressionBool),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&BoolCaller) >= 0 &&
			   Engine.RegisterGlobalFunction("int RecordExpressionInt(int Marker, int Value)",
				   asFUNCTION(RecordExpressionInt),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&IntCaller) >= 0 &&
			   Engine.RegisterGlobalFunction(
				   "int RecordEagerStage(int Marker, int Value, bool RaiseException)",
				   asFUNCTION(RecordEagerStage),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&EagerStageCaller) >= 0 &&
			   Engine.RegisterGlobalFunction(
				   "int CompleteEagerBoundary(int Value, bool RaiseException)",
				   asFUNCTION(CompleteEagerBoundary),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&EagerBoundaryCaller) >= 0;
	}
}
