#include "Bind_Primitives.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptDocs.h"
#include "AngelscriptType.h"

#include "Helper_ToString.h"

/**
 * Primitive Type adapters, configured aliases, constants, and formatter contributions.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <int8 | int16 | int32 | int64> Value;                                                                | Registers signed integer primitives; int is an alias of int32.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <uint8 | uint16 | uint32 | uint64> Value;                                                            | Registers unsigned integer primitives; uint is an alias of uint32.                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Value;                                                                                          | Registers the script Boolean primitive.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | <float | float32 | float64 | double> Value;                                                          | Registers floating-point primitives and aliases. The project setting selects whether float is 32-bit or 64-bit.  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const <uint8 | uint16 | uint32 | uint64 | int8 | int16 | int32 | int64> MIN_<type>;                  | Provides the minimum value for every fixed-width integer type.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const <uint8 | uint16 | uint32 | uint64 | int8 | int16 | int32 | int64> MAX_<type>;                  | Provides the maximum value for every fixed-width integer type.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const float64 <MIN_dbl | MAX_dbl | NAN_dbl>;                                                         | Provides double limits and NaN when the legacy double spelling is enabled.                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const <configured-float-type> <MIN_flt | MAX_flt | NAN_flt>;                                         | Provides limits and NaN using the configured script float width.                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const float64 <EULERS_NUMBER | PI | HALF_PI | TWO_PI | SMALL_NUMBER | KINDA_SMALL_NUMBER |           | Provides common double-precision mathematical constants.                                                         |
 * |     BIG_NUMBER>;                                                                                     |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const float64 <THRESH_VECTOR_NORMALIZED | THRESH_NORMALS_ARE_PARALLEL |                              | Provides double-precision vector and normal tolerances.                                                          |
 * |     THRESH_NORMALS_ARE_ORTHOGONAL>;                                                                  |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const float32 <__EULERS_NUMBER_flt | __PI_flt | __HALF_PI_flt | __TWO_PI_flt | __SMALL_NUMBER_flt |  | Provides internal single-precision mathematical constants used by float bindings.                                |
 * |     __KINDA_SMALL_NUMBER_flt | __BIG_NUMBER_flt>;                                                    |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | const float32 <__THRESH_VECTOR_NORMALIZED_flt | __THRESH_NORMALS_ARE_PARALLEL_flt |                  | Provides internal single-precision vector and normal tolerances.                                                 |
 * |     __THRESH_NORMALS_ARE_ORTHOGONAL_flt>;                                                            |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = f"{<int8 | int16 | int32 | int64 | uint8 | uint16 | uint32 | uint64 | float32 |       | Formats every registered primitive through the shared string formatter contribution.                             |
 * |     float64 | bool>}";                                                                               |                                                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

static uint8 AS_MIN_uint8 = MIN_uint8;
static uint16 AS_MIN_uint16 = MIN_uint16;
static uint32 AS_MIN_uint32 = MIN_uint32;
static uint64 AS_MIN_uint64 = MIN_uint64;
static int8 AS_MIN_int8 = MIN_int8;
static int16 AS_MIN_int16 = MIN_int16;
static int32 AS_MIN_int32 = MIN_int32;
static int64 AS_MIN_int64 = MIN_int64;

static uint8 AS_MAX_uint8 = MAX_uint8;
static uint16 AS_MAX_uint16 = MAX_uint16;
static uint32 AS_MAX_uint32 = MAX_uint32;
static uint64 AS_MAX_uint64 = MAX_uint64;
static int8 AS_MAX_int8 = MAX_int8;
static int16 AS_MAX_int16 = MAX_int16;
static int32 AS_MAX_int32 = MAX_int32;
static int64 AS_MAX_int64 = MAX_int64;

static float AS_MIN_flt = MIN_flt;
static float AS_MAX_flt = MAX_flt;
static double AS_MIN_dbl = MIN_dbl;
static double AS_MAX_dbl = MAX_dbl;

static double AS_PI = PI;
static double AS_HALF_PI = HALF_PI;
static double AS_TWO_PI = 2.f*PI;
static double AS_SMALL_NUMBER = SMALL_NUMBER;
static double AS_KINDA_SMALL_NUMBER = KINDA_SMALL_NUMBER;
static double AS_BIG_NUMBER = BIG_NUMBER;
static double AS_EULERS_NUMBER = EULERS_NUMBER;
static double AS_THRESH_VECTOR_NORMALIZED = THRESH_VECTOR_NORMALIZED;
static double AS_THRESH_NORMALS_ARE_PARALLEL = THRESH_NORMALS_ARE_PARALLEL;
static double AS_THRESH_NORMALS_ARE_ORTHOGONAL = THRESH_NORMALS_ARE_ORTHOGONAL;

static float AS_PI_flt = PI;
static float AS_HALF_PI_flt = HALF_PI;
static float AS_TWO_PI_flt = 2.f*PI;
static float AS_SMALL_NUMBER_flt = SMALL_NUMBER;
static float AS_KINDA_SMALL_NUMBER_flt = KINDA_SMALL_NUMBER;
static float AS_BIG_NUMBER_flt = BIG_NUMBER;
static float AS_EULERS_NUMBER_flt = EULERS_NUMBER;
static float AS_THRESH_VECTOR_NORMALIZED_flt = THRESH_VECTOR_NORMALIZED;
static float AS_THRESH_NORMALS_ARE_PARALLEL_flt = THRESH_NORMALS_ARE_PARALLEL;
static float AS_THRESH_NORMALS_ARE_ORTHOGONAL_flt = THRESH_NORMALS_ARE_ORTHOGONAL;

static float AS_NAN_flt = NAN;
static double AS_NAN_dbl = NAN;

namespace
{
	template<typename T>
	void BindPrimitiveConstant(
		FAngelscriptBinds& Binds,
		const ANSICHAR* Declaration,
		T* Address,
		const TCHAR* Documentation = nullptr)
	{
		static_assert(sizeof(T) <= sizeof(asQWORD));

		asQWORD RawValue = 0;
		FMemory::Memcpy(&RawValue, Address, sizeof(T));
		FAngelscriptBoundProperty BoundConstant = Binds.BindGlobalVariableForTarget(Declaration, Address)
			.PureConstant(RawValue);

#if WITH_EDITOR
		if (BoundConstant.IsValid() && Documentation != nullptr)
		{
			FAngelscriptDocs::AddDocumentationForGlobalVariable(
				Binds.GetTargetEngine(),
				BoundConstant.GetPropertyId(),
				Documentation);
		}
#endif
	}



}

AS_FORCE_LINK const FAngelscriptBind Bind_PrimitiveTypes_TypeInfrastructure(
	TEXT("PrimitiveTypes.TypeInfrastructure"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptTypeDatabase& TypeDatabase = Binds.GetTargetTypeDatabase();
		const UAngelscriptSettings* ConfigSettings = Binds.GetTargetEngine().ConfigSettings;
		check(ConfigSettings != nullptr);

		auto IntType = MakeShared<FIntType>();
		Binds.RegisterTypeForTarget(IntType);

		auto UIntType = MakeShared<FUIntType>();
		Binds.RegisterTypeForTarget(UIntType);

		Binds.RegisterTypeForTarget(MakeShared<FInt64Type>());
		Binds.RegisterTypeForTarget(MakeShared<FUInt64Type>());
		Binds.RegisterTypeForTarget(MakeShared<FInt16Type>());
		Binds.RegisterTypeForTarget(MakeShared<FUInt16Type>());
		Binds.RegisterTypeForTarget(MakeShared<FInt8Type>());
		Binds.RegisterTypeForTarget(MakeShared<FUInt8Type>());

		auto BoolType = MakeShared<FBoolType>();
		TypeDatabase.ScriptBoolType = BoolType;
		Binds.RegisterTypeForTarget(BoolType);

		auto FloatType = MakeShared<FFloatType>(
			ConfigSettings->bScriptFloatIsFloat64 ? TEXT("float32") : TEXT("float"));
		TypeDatabase.ScriptFloatType = FloatType;
		Binds.RegisterTypeForTarget(FloatType);

		auto DoubleType = MakeShared<FDoubleType>(
			ConfigSettings->bScriptFloatIsFloat64 ? TEXT("float") : TEXT("float64"));
		TypeDatabase.ScriptDoubleType = DoubleType;
		Binds.RegisterTypeForTarget(DoubleType);

		auto ExtendedType = MakeShared<FUnrealFloatParamExtendedToDoubleType>(
			ConfigSettings->bScriptFloatIsFloat64 ? TEXT("float") : TEXT("float64"));
		TypeDatabase.ScriptFloatParamExtendedToDoubleType = ExtendedType;

		// Make sure all the aliased types will be found correctly.
		TypeDatabase.TypesByAngelscriptName.Add(TEXT("double"), DoubleType);
		TypeDatabase.TypesByAngelscriptName.Add(TEXT("int32"), IntType);
		TypeDatabase.TypesByAngelscriptName.Add(TEXT("uint32"), UIntType);

		if (ConfigSettings->bScriptFloatIsFloat64)
			TypeDatabase.TypesByAngelscriptName.Add(TEXT("float64"), DoubleType);
		else
			TypeDatabase.TypesByAngelscriptName.Add(TEXT("float32"), FloatType);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_PrimitiveTypes_ToStringContribution(
	TEXT("PrimitiveTypes.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("int8"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%d"), *(int8*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("int16"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%d"), *(int16*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("int32"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%d"), *(int32*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("int64"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%lld"), *(int64*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("uint8"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%u"), *(uint8*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("uint16"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%u"), *(uint16*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("uint32"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%u"), *(uint32*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("uint64"), [](void* Ptr, FString& Str)
		{
			Str += FString::Printf(TEXT("%llu"), *(uint64*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("float32"), [](void* Ptr, FString& Str)
		{
			Str += FString::SanitizeFloat(*(float*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("float64"), [](void* Ptr, FString& Str)
		{
			Str += FString::SanitizeFloat(*(double*)Ptr);
		});

		FToStringHelper::Register(Binds, TEXT("bool"), [](void* Ptr, FString& Str)
		{
			Str += *(bool*)Ptr ? TEXT("true") : TEXT("false");
		});
	});

AS_FORCE_LINK const FAngelscriptBind Bind_PrimitiveTypes(
	TEXT("PrimitiveTypes.Constants"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		const UAngelscriptSettings* ConfigSettings = Binds.GetTargetEngine().ConfigSettings;
		check(ConfigSettings != nullptr);

		BindPrimitiveConstant(Binds, "const uint8 MIN_uint8", &AS_MIN_uint8, TEXT("Lowest number a uint8 can hold (0)"));
		BindPrimitiveConstant(Binds, "const uint16 MIN_uint16", &AS_MIN_uint16, TEXT("Lowest number a uint16 can hold (0)"));
		BindPrimitiveConstant(Binds, "const uint32 MIN_uint32", &AS_MIN_uint32, TEXT("Lowest number a uint32 can hold (0)"));
		BindPrimitiveConstant(Binds, "const uint64 MIN_uint64", &AS_MIN_uint64, TEXT("Lowest number a uint64 can hold (0)"));
		BindPrimitiveConstant(Binds, "const int8 MIN_int8", &AS_MIN_int8, TEXT("Lowest number an int8 can hold (-128)"));
		BindPrimitiveConstant(Binds, "const int16 MIN_int16", &AS_MIN_int16, TEXT("Lowest number an int16 can hold (-32768)"));
		BindPrimitiveConstant(Binds, "const int32 MIN_int32", &AS_MIN_int32, TEXT("Lowest number an int32 can hold (-2147483648)"));
		BindPrimitiveConstant(Binds, "const int64 MIN_int64", &AS_MIN_int64, TEXT("Lowest number an int64 can hold (-9223372036854775808)"));

		BindPrimitiveConstant(Binds, "const uint8 MAX_uint8", &AS_MAX_uint8, TEXT("Highest number a uint8 can hold (255)"));
		BindPrimitiveConstant(Binds, "const uint16 MAX_uint16", &AS_MAX_uint16, TEXT("Highest number a uint16 can hold (65535)"));
		BindPrimitiveConstant(Binds, "const uint32 MAX_uint32", &AS_MAX_uint32, TEXT("Highest number a uint32 can hold (4294967295)"));
		BindPrimitiveConstant(Binds, "const uint64 MAX_uint64", &AS_MAX_uint64, TEXT("Highest number a uint64 can hold (18446744073709551615)"));
		BindPrimitiveConstant(Binds, "const int8 MAX_int8", &AS_MAX_int8, TEXT("Highest number an int8 can hold (127)"));
		BindPrimitiveConstant(Binds, "const int16 MAX_int16", &AS_MAX_int16, TEXT("Highest number an int8 can hold (32767)"));
		BindPrimitiveConstant(Binds, "const int32 MAX_int32", &AS_MAX_int32, TEXT("Highest number an int32 can hold (2147483647)"));
		BindPrimitiveConstant(Binds, "const int64 MAX_int64", &AS_MAX_int64, TEXT("Highest number an int64 can hold (9223372036854775807)"));

		if (!ConfigSettings->bDeprecateDoubleType)
		{
			BindPrimitiveConstant(Binds, "const float64 MIN_dbl", &AS_MIN_dbl, TEXT("Smallest positive number a double can hold (~0.0000...001)"));
			BindPrimitiveConstant(Binds, "const float64 MAX_dbl", &AS_MAX_dbl, TEXT("Largest positive number a double can hold (~10^308)"));
			BindPrimitiveConstant(Binds, "const float64 NAN_dbl", &AS_NAN_dbl, TEXT("Special Not-a-Number value for double floating point"));
		}

		if (ConfigSettings->bScriptFloatIsFloat64)
		{
			BindPrimitiveConstant(Binds, "const float64 MIN_flt", &AS_MIN_dbl, TEXT("Smallest positive number a float can hold (~0.0000...001)"));
			BindPrimitiveConstant(Binds, "const float64 MAX_flt", &AS_MAX_dbl, TEXT("Largest positive number a float can hold (~10^308)"));
			BindPrimitiveConstant(Binds, "const float64 NAN_flt", &AS_NAN_dbl, TEXT("Special Not-a-Number value for floating point"));
		}
		else
		{
			BindPrimitiveConstant(Binds, "const float32 MIN_flt", &AS_MIN_flt, TEXT("Smallest positive number a float can hold (~0.0000...001)"));
			BindPrimitiveConstant(Binds, "const float32 MAX_flt", &AS_MAX_flt, TEXT("Largest positive number a float can hold (~10^38)"));
			BindPrimitiveConstant(Binds, "const float32 NAN_flt", &AS_NAN_flt, TEXT("Special Not-a-Number value for floating point"));
		}

		BindPrimitiveConstant(Binds, "const float64 EULERS_NUMBER", &AS_EULERS_NUMBER, TEXT("Euler's number, also known as `e` (2.71828...)"));
		BindPrimitiveConstant(Binds, "const float64 PI", &AS_PI, TEXT("Pi, also known as `π` (3.14159...)"));
		BindPrimitiveConstant(Binds, "const float64 HALF_PI", &AS_HALF_PI, TEXT("Half the value of Pi (1.57079...)"));
		BindPrimitiveConstant(Binds, "const float64 TWO_PI", &AS_TWO_PI, TEXT("Double the value of Pi (6.28318...)"));
		BindPrimitiveConstant(Binds, "const float64 SMALL_NUMBER", &AS_SMALL_NUMBER, TEXT("A very small number (0.00000001, or 1e-8)"));
		BindPrimitiveConstant(Binds, "const float64 KINDA_SMALL_NUMBER", &AS_KINDA_SMALL_NUMBER, TEXT("A somewhat small number (0.0001, or 1e-4)"));
		BindPrimitiveConstant(Binds, "const float64 BIG_NUMBER", &AS_BIG_NUMBER, TEXT("A very large number (~10^38)"));
		BindPrimitiveConstant(Binds, "const float64 THRESH_VECTOR_NORMALIZED", &AS_THRESH_VECTOR_NORMALIZED);
		BindPrimitiveConstant(Binds, "const float64 THRESH_NORMALS_ARE_PARALLEL", &AS_THRESH_NORMALS_ARE_PARALLEL);
		BindPrimitiveConstant(Binds, "const float64 THRESH_NORMALS_ARE_ORTHOGONAL", &AS_THRESH_NORMALS_ARE_ORTHOGONAL);

		BindPrimitiveConstant(Binds, "const float32 __EULERS_NUMBER_flt", &AS_EULERS_NUMBER_flt);
		BindPrimitiveConstant(Binds, "const float32 __PI_flt", &AS_PI_flt);
		BindPrimitiveConstant(Binds, "const float32 __HALF_PI_flt", &AS_HALF_PI_flt);
		BindPrimitiveConstant(Binds, "const float32 __TWO_PI_flt", &AS_TWO_PI_flt);
		BindPrimitiveConstant(Binds, "const float32 __SMALL_NUMBER_flt", &AS_SMALL_NUMBER_flt);
		BindPrimitiveConstant(Binds, "const float32 __KINDA_SMALL_NUMBER_flt", &AS_KINDA_SMALL_NUMBER_flt);
		BindPrimitiveConstant(Binds, "const float32 __BIG_NUMBER_flt", &AS_BIG_NUMBER_flt);
		BindPrimitiveConstant(Binds, "const float32 __THRESH_VECTOR_NORMALIZED_flt", &AS_THRESH_VECTOR_NORMALIZED_flt);
		BindPrimitiveConstant(Binds, "const float32 __THRESH_NORMALS_ARE_PARALLEL_flt", &AS_THRESH_NORMALS_ARE_PARALLEL_flt);
		BindPrimitiveConstant(Binds, "const float32 __THRESH_NORMALS_ARE_ORTHOGONAL_flt", &AS_THRESH_NORMALS_ARE_ORTHOGONAL_flt);
	});
