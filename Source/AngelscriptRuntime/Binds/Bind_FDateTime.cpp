#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

#include "Bind_FDateTime_Functions.h"

namespace
{
	void BindFDateTimeToStringContribution(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FDateTime"), &FAngelscriptFDateTimeBinds::AppendToString);
	}

	void BindFDateTimeFunctions(FAngelscriptBinds& Binds)
	{
		auto FDateTime_ = Binds.ExistingClassForTarget("FDateTime");
		FDateTime_.Constructor(
			"void f(int Year, int Month, int Day, int Hour = 0, int Minute = 0, int Second = 0, int Millisecond = 0)",
			&FAngelscriptFDateTimeBinds::Construct,
			"FDateTime",
			true)
			.NoDiscard();

		FDateTime_.Method("bool opEquals(const FDateTime& Other) const", METHODPR_TRIVIAL(bool, FDateTime, operator==, (const FDateTime&) const));
		FDateTime_.Method("FDateTime GetDate() const", METHODPR_TRIVIAL(FDateTime, FDateTime, GetDate, () const));
		FDateTime_.Method("void GetDate(int& OutYear, int& OutMonth, int& OutDay) const", METHODPR_TRIVIAL(void, FDateTime, GetDate, (int32&, int32&, int32&) const));
		FDateTime_.Method("int GetDay() const", METHOD_TRIVIAL(FDateTime, GetDay));
		FDateTime_.Method("int GetDayOfYear() const", METHOD_TRIVIAL(FDateTime, GetDayOfYear));
		FDateTime_.Method("int GetHour() const", METHOD_TRIVIAL(FDateTime, GetHour));
		FDateTime_.Method("int GetHour12() const", METHOD_TRIVIAL(FDateTime, GetHour12));
		FDateTime_.Method("int GetMillisecond() const", METHOD_TRIVIAL(FDateTime, GetMillisecond));
		FDateTime_.Method("int GetMinute() const", METHOD_TRIVIAL(FDateTime, GetMinute));
		FDateTime_.Method("int GetMonth() const", METHOD_TRIVIAL(FDateTime, GetMonth));
		FDateTime_.Method("int GetSecond() const", METHOD_TRIVIAL(FDateTime, GetSecond));
		FDateTime_.Method("int GetYear() const", METHOD_TRIVIAL(FDateTime, GetYear));
		FDateTime_.Method("bool IsAfternoon() const", METHOD_TRIVIAL(FDateTime, IsAfternoon));
		FDateTime_.Method("bool IsMorning() const", METHOD_TRIVIAL(FDateTime, IsMorning));
		FDateTime_.Method("int64 ToUnixTimestamp() const", METHOD_TRIVIAL(FDateTime, ToUnixTimestamp));
		FDateTime_.Method("FString ToHttpDate() const", METHOD_TRIVIAL(FDateTime, ToHttpDate));
		FDateTime_.Method("FString ToIso8601() const", METHOD_TRIVIAL(FDateTime, ToIso8601));
		FDateTime_.Method("FString ToString(const FString& Format) const", &FAngelscriptFDateTimeBinds::ToStringFormat);
		FDateTime_.Method("int64 GetTicks() const", METHOD_TRIVIAL(FDateTime, GetTicks));
		FDateTime_.Method("int opCmp(const FDateTime& Other) const", &FAngelscriptFDateTimeBinds::Compare);
		FDateTime_.Method("FDateTime opAdd(const FTimespan& Other) const", METHODPR_TRIVIAL(FDateTime, FDateTime, operator+, (const FTimespan&) const));
		FDateTime_.Method("FDateTime& opAddAssign(const FTimespan& Other)", METHODPR_TRIVIAL(FDateTime&, FDateTime, operator+=, (const FTimespan&)));
		FDateTime_.Method("FTimespan opSub(const FDateTime& Other) const", METHODPR_TRIVIAL(FTimespan, FDateTime, operator-, (const FDateTime&) const));
		FDateTime_.Method("FDateTime opSub(const FTimespan& Other) const", METHODPR_TRIVIAL(FDateTime, FDateTime, operator-, (const FTimespan&) const));
		FDateTime_.Method("FDateTime& opSubAssign(const FTimespan& Other)", METHODPR_TRIVIAL(FDateTime&, FDateTime, operator-=, (const FTimespan&)));

		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FDateTime");
		Binds.BindGlobalFunctionForTarget("int DaysInMonth(int Year, int Month) no_discard", FUNC_TRIVIAL(FDateTime::DaysInMonth));
		Binds.BindGlobalFunctionForTarget("int DaysInYear(int Year) no_discard", FUNC_TRIVIAL(FDateTime::DaysInYear));
		Binds.BindGlobalFunctionForTarget("bool IsLeapYear(int Year) no_discard", FUNC_TRIVIAL(FDateTime::IsLeapYear));
		Binds.BindGlobalFunctionForTarget("FDateTime FromUnixTimestamp(int64 UnixTime) no_discard", FUNC_TRIVIAL(FDateTime::FromUnixTimestamp));
		Binds.BindGlobalFunctionForTarget("FDateTime MinValue() no_discard", FUNC_TRIVIAL(FDateTime::MinValue));
		Binds.BindGlobalFunctionForTarget("FDateTime MaxValue() no_discard", FUNC_TRIVIAL(FDateTime::MaxValue));
		Binds.BindGlobalFunctionForTarget("FDateTime Now() no_discard", FUNC_TRIVIAL(FDateTime::Now));
		Binds.BindGlobalFunctionForTarget("FDateTime UtcNow() no_discard", FUNC_TRIVIAL(FDateTime::UtcNow));
		Binds.BindGlobalFunctionForTarget("FDateTime Today() no_discard", FUNC_TRIVIAL(FDateTime::Today));
		Binds.BindGlobalFunctionForTarget("bool Parse(const FString& DateTimeString, FDateTime& OutDateTime)", FUNC_TRIVIAL(FDateTime::Parse));
		Binds.BindGlobalFunctionForTarget("bool ParseHttpDate(const FString& HttpDate, FDateTime& OutDateTime)", FUNC_TRIVIAL(FDateTime::ParseHttpDate));
		Binds.BindGlobalFunctionForTarget(
			"bool ParseIso8601(const FString& DateTimeString, FDateTime& OutDateTime)",
			&FAngelscriptFDateTimeBinds::ParseIso8601);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FDateTime_ToStringContribution(
	TEXT("FDateTime.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindFDateTimeToStringContribution);

AS_FORCE_LINK const FAngelscriptBind Bind_FDateTime(
	TEXT("FDateTime.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFDateTimeFunctions);
