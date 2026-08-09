#include "AngelscriptBinds.h"

#include "Misc/FrameTime.h"

struct FAngelscriptFrameTimeBinds
{
	static void ConstructFrameNumber(FFrameNumber* Address, int32 Value)
	{
		new (Address) FFrameNumber(Value);
	}

	static void ConstructFrameTime(FFrameTime* Address, FFrameNumber Frame, float SubFrame)
	{
		new (Address) FFrameTime(Frame, SubFrame);
	}

	static FFrameTime Multiply(const FFrameTime& Time, double Scalar)
	{
		return Time * Scalar;
	}
};

/**
 * Frame-time binding surface.
 * +------------------------------------------------------------------------------------+---------------------------------------------------------------------+
 * | AngelScript usage signature                                                        | Purpose / parameter notes                                           |
 * +------------------------------------------------------------------------------------+---------------------------------------------------------------------+
 * | FFrameNumber Frame(int32 Value);                                                   | Constructs a whole frame number.                                    |
 * +------------------------------------------------------------------------------------+---------------------------------------------------------------------+
 * | FFrameTime Time(FFrameNumber Frame, float32 SubFrame);                            | Constructs time with a fractional frame. @param SubFrame is [0,1).  |
 * +------------------------------------------------------------------------------------+---------------------------------------------------------------------+
 */
AS_FORCE_LINK const FAngelscriptBind Bind_FFrameTime(
	TEXT("FFrameTime"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FrameNumber = Binds.ExistingClassForTarget("FFrameNumber");
		FrameNumber.Constructor("void f(int32 Value)", &FAngelscriptFrameTimeBinds::ConstructFrameNumber, "FFrameNumber", true);
		auto FrameTime = Binds.ExistingClassForTarget("FFrameTime");
		FrameTime.Constructor("void f(FFrameNumber Frame, float32 SubFrame)", &FAngelscriptFrameTimeBinds::ConstructFrameTime, "FFrameTime", true);
		FrameTime.Method("FFrameNumber GetFrame() const", METHOD_TRIVIAL(FFrameTime, GetFrame));
		FrameTime.Method("float32 GetSubFrame() const", METHOD_TRIVIAL(FFrameTime, GetSubFrame));
		FrameTime.Method("FFrameNumber FloorToFrame() const", METHOD_TRIVIAL(FFrameTime, FloorToFrame));
		FrameTime.Method("FFrameNumber CeilToFrame() const", METHOD_TRIVIAL(FFrameTime, CeilToFrame));
		FrameTime.Method("FFrameNumber RoundToFrame() const", METHOD_TRIVIAL(FFrameTime, RoundToFrame));
		FrameTime.Method("float64 AsDecimal() const", METHOD_TRIVIAL(FFrameTime, AsDecimal));
		FrameTime.Method("FFrameTime opMul(float64 Scalar) const", &FAngelscriptFrameTimeBinds::Multiply);
	});
