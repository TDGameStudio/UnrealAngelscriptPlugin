#include "Bind_FDateTime.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FDateTime binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime DateTime(int Year, int Month, int Day, int Hour = 0, int Minute = 0,             | Constructs a date and time from calendar components.                                                                 |
 * |     int Second = 0, int Millisecond = 0);                                                  |                                                                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime == Other;                                                                         | Compares two date-time values for equality.                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime.GetDate() const;                                                       | Returns this value with the time-of-day cleared.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FDateTime.GetDate(int& OutYear, int& OutMonth, int& OutDay) const;                    | Decomposes the calendar date.                                                                                        |
 * |                                                                                            | @param OutYear, OutMonth, OutDay Receive the date components.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetDay() const;                                                              | Returns the day of the month.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetDayOfYear() const;                                                        | Returns the one-based day of the year.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetHour() const;                                                             | Returns the hour in 24-hour form.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetHour12() const;                                                           | Returns the hour in 12-hour form.                                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetMillisecond() const;                                                      | Returns the millisecond component.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetMinute() const;                                                           | Returns the minute component.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetMonth() const;                                                            | Returns the month component.                                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetSecond() const;                                                           | Returns the second component.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime.GetYear() const;                                                             | Returns the year component.                                                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FDateTime.IsAfternoon() const;                                                        | Returns whether the time is noon or later.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FDateTime.IsMorning() const;                                                          | Returns whether the time is before noon.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 FDateTime.ToUnixTimestamp() const;                                                   | Returns seconds since the Unix epoch.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FDateTime.ToHttpDate() const;                                                      | Formats the value as an HTTP date.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FDateTime.ToIso8601() const;                                                       | Formats the value as ISO 8601.                                                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FDateTime.ToString(const FString& Format) const;                                   | Formats the value with the supplied engine date-time format.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int64 FDateTime.GetTicks() const;                                                          | Returns the underlying tick count.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime < Other;                                                                          | Compares chronologically; the same binding supplies <=, >, and >=.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime + Timespan;                                                                       | Returns a date-time offset by a timespan.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime += Timespan;                                                                      | Offsets this date-time in place.                                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime - Other;                                                                          | Returns the timespan between two date-time values.                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime - Timespan;                                                                       | Returns a date-time moved backward by a timespan.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | DateTime -= Timespan;                                                                      | Moves this date-time backward in place.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime::DaysInMonth(int Year, int Month);                                           | Returns the number of days in a month.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FDateTime::DaysInYear(int Year);                                                       | Returns the number of days in a year.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FDateTime::IsLeapYear(int Year);                                                      | Returns whether the supplied year is a leap year.                                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime::FromUnixTimestamp(int64 UnixTime);                                    | Creates a date-time from seconds since the Unix epoch.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime::MinValue();                                                           | Returns the minimum representable date-time.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime::MaxValue();                                                           | Returns the maximum representable date-time.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime::Now();                                                                | Returns the current local date and time.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime::UtcNow();                                                             | Returns the current UTC date and time.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FDateTime FDateTime::Today();                                                              | Returns the current local date with time-of-day cleared.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FDateTime::Parse(const FString& DateTimeString, FDateTime& OutDateTime);              | Parses an engine date-time string.                                                                                   |
 * |                                                                                            | @param OutDateTime Receives the parsed value on success.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FDateTime::ParseHttpDate(const FString& HttpDate, FDateTime& OutDateTime);            | Parses an HTTP date string.                                                                                          |
 * |                                                                                            | @param OutDateTime Receives the parsed value on success.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FDateTime::ParseIso8601(const FString& DateTimeString,                                | Parses an ISO 8601 string.                                                                                           |
 * |     FDateTime& OutDateTime);                                                               | @param OutDateTime Receives the parsed value on success.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + DateTime;                                                                           | Appends FDateTime text to a string and returns the result.                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += DateTime;                                                                          | Appends FDateTime text to a string in place.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(DateTime);                                                                     | Appends FDateTime text to a temporary or existing string.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FDateTime.ToString() const;                                                        | Returns the engine string representation.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FDateTime_ToStringContribution(
	TEXT("FDateTime.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FDateTime"), &FAngelscriptFDateTimeBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FDateTime(
	TEXT("FDateTime.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});
