#include "Bind_FRandomStream.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * FRandomStream binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FRandomStream;                                                                      | Declares the deterministic random-stream value type.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRandomStream Stream();                                                                    | Constructs a stream with its default seed.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRandomStream Stream(int32 InSeed);                                                        | Constructs a stream from a signed seed.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FRandomStream Stream(uint32 InSeed);                                                       | Constructs a stream from an unsigned seed.                                                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRandomStream.Initialize(int32 InSeed);                                               | Reinitializes the stream from a signed seed.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRandomStream.Initialize(uint32 InSeed);                                              | Reinitializes the stream from an unsigned seed.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRandomStream.Initialize(FName InName);                                               | Reinitializes the stream from a deterministic name hash.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRandomStream.Reset() const;                                                          | Restores the stream to its initial seed.                                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FRandomStream.GetInitialSeed() const;                                                  | Returns the seed used to initialize the stream.                                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FRandomStream.GenerateNewSeed();                                                      | Reinitializes the stream from a newly generated seed.                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float32 FRandomStream.GetFraction() const;                                                 | Returns a random fraction in the engine-defined range.                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | uint32 FRandomStream.GetUnsignedInt() const;                                               | Returns a random unsigned integer.                                                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FRandomStream.GetCurrentSeed() const;                                                | Returns the stream current seed state.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 FRandomStream.RandRange(int32 Min, int32 Max) const;                                 | Returns a random integer in the inclusive range.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | float64 FRandomStream.RandRange(float64 Min, float64 Max) const;                           | Returns a random floating-point value in the requested range.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRandomStream.GetUnitVector() const;                                               | Returns a random unit vector.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRandomStream.VRand() const;                                                       | Returns a random unit vector.                                                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRandomStream.VRandCone(const FVector& Dir, float32 ConeHalfAngleRad)              | Returns a random vector within a symmetric cone around Dir.                                                          |
 * |     const;                                                                                 | @param ConeHalfAngleRad Cone half-angle in radians.                                                                  |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector FRandomStream.VRandCone(const FVector& Dir, float32 HorizontalConeHalfAngleRad,    | Returns a random vector within an elliptical cone around Dir.                                                        |
 * |     float32 VerticalConeHalfAngleRad) const;                                               | @param HorizontalConeHalfAngleRad Horizontal cone half-angle in radians.                                             |
 * |                                                                                            | @param VerticalConeHalfAngleRad Vertical cone half-angle in radians.                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text + Stream;                                                                             | Appends FRandomStream text to a string and returns the result.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text += Stream;                                                                            | Appends FRandomStream text to a string in place.                                                                     |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | Text.Append(Stream);                                                                       | Appends FRandomStream text to a temporary or existing string.                                                        |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FRandomStream.ToString() const;                                                    | Returns the engine string representation.                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FRandomStream_Type(
	TEXT("FRandomStream.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	[](FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Binds.ValueClassForTarget<FRandomStream>("FRandomStream", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FRandomStreamType>());
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FRandomStream(
	TEXT("FRandomStream.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FRandomStream_ = Binds.ExistingClassForTarget("FRandomStream");
		FRandomStream_.Constructor("void f()", &FAngelscriptFRandomStreamBinds::ConstructDefault);
		FRandomStream_.Constructor("void f(int32 InSeed)", &FAngelscriptFRandomStreamBinds::ConstructIntSeed);
		FRandomStream_.Constructor("void f(uint32 InSeed)", &FAngelscriptFRandomStreamBinds::ConstructUIntSeed);
		FRandomStream_.Method("void Initialize(int32 InSeed)", METHODPR_TRIVIAL(void, FRandomStream, Initialize, (int32)));
		FRandomStream_.Method("void Initialize(uint32 InSeed)", METHODPR_TRIVIAL(void, FRandomStream, Initialize, (int32)));
		FRandomStream_.Method("void Reset() const", METHOD_TRIVIAL(FRandomStream, Reset));
		FRandomStream_.Method("int GetInitialSeed() const", METHOD_TRIVIAL(FRandomStream, GetInitialSeed));
		FRandomStream_.Method("void GenerateNewSeed()", METHOD_TRIVIAL(FRandomStream, GenerateNewSeed));
		FRandomStream_.Method("float32 GetFraction() const", METHOD_TRIVIAL(FRandomStream, GetFraction));
		FRandomStream_.Method("uint32 GetUnsignedInt() const", METHOD_TRIVIAL(FRandomStream, GetUnsignedInt));
		FRandomStream_.Method("int32 GetCurrentSeed() const", METHOD_TRIVIAL(FRandomStream, GetCurrentSeed));
		FRandomStream_.Method("int32 RandRange(int32 Min, int32 Max) const", METHODPR_TRIVIAL(int32, FRandomStream, RandRange, (int32, int32) const));
		FRandomStream_.Method("float64 RandRange(float64 Min, float64 Max) const", METHODPR_TRIVIAL(double, FRandomStream, FRandRange, (double, double) const));
		FRandomStream_.Method("void Initialize(FName InName)", METHODPR_TRIVIAL(void, FRandomStream, Initialize, (FName)));
		FRandomStream_.Method("FVector GetUnitVector() const", METHOD_TRIVIAL(FRandomStream, GetUnitVector));
		FRandomStream_.Method("FVector VRand() const", METHOD_TRIVIAL(FRandomStream, VRand));
		FRandomStream_.Method("FVector VRandCone(const FVector& Dir, float32 ConeHalfAngleRad) const", METHODPR_TRIVIAL(FVector, FRandomStream, VRandCone, (FVector const&, float) const));
		FRandomStream_.Method("FVector VRandCone(const FVector& Dir, float32 HorizontalConeHalfAngleRad, float32 VerticalConeHalfAngleRad) const", METHODPR_TRIVIAL(FVector, FRandomStream, VRandCone, (FVector const&, float, float) const));
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FRandomStream_ToStringContribution(
	TEXT("FRandomStream.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FRandomStream"), &FAngelscriptFRandomStreamBinds::AppendToString);
	});
