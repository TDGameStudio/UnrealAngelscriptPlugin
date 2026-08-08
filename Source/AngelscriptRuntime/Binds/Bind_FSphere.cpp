#include "AngelscriptBinds.h"

#include "Helper_StructType.h"

#include "Bind_FSphere_Functions.h"

struct FGetSphere
{
	static UScriptStruct* Get();
};

UScriptStruct* FGetSphere::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Sphere"));
	return ScriptStruct;
}

struct FSphereType : TAngelscriptCoreStructType<FSphere, FGetSphere, false>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FSphere"); }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFSphereType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FSphere>("FSphere", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FSphereType>());
	}

	void BindFSphereFunctions(FAngelscriptBinds& Binds)
	{
		auto FSphere_ = Binds.ExistingClassForTarget("FSphere");
		FSphere_.Constructor("void f()", &FAngelscriptFSphereBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FSphere", true, "ForceInit");
		FSphere_.Constructor(
			"void f(FVector InV, float32 InW)",
			&FAngelscriptFSphereBinds::ConstructCenterRadius,
			"FSphere",
			true)
			.NoDiscard();
		FSphere_.Constructor(
			"void f(const FSphere& Sphere)",
			&FAngelscriptFSphereBinds::ConstructCopy,
			"FSphere",
			true)
			.NoDiscard();
		FSphere_.Constructor(
			"void f(const FSphere3f& Sphere)",
			&FAngelscriptFSphereBinds::ConstructFromSphere3f,
			"FSphere",
			true)
			.NoDiscard();
		FSphere_.Constructor(
			"void f(const TArray<FVector>& Points)",
			&FAngelscriptFSphereBinds::ConstructFromPoints)
			.NoDiscard();
		FSphere_.Property("float64 W", &FSphere::W);
		FSphere_.Property("FVector Center", &FSphere::Center);
		FSphere_.Method("FSphere opAdd(const FSphere& Other) const", METHODPR_TRIVIAL(FSphere, FSphere, operator+, (const FSphere&) const));
		FSphere_.Method("FSphere& opAddAssign(const FSphere& Other)", METHODPR_TRIVIAL(FSphere&, FSphere, operator+=, (const FSphere&)));
		FSphere_.Method("bool Equals(const FSphere& Sphere, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, Equals, (const FSphere&, double) const));
		FSphere_.Method("bool IsInside(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, IsInside, (const FSphere&, double) const));
		FSphere_.Method("bool IsInside(const FVector& In, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, IsInside, (const FVector&, double) const));
		FSphere_.Method("bool Intersects(const FSphere& Other, float64 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere, Intersects, (const FSphere&, double) const));
		FSphere_.Method("FSphere TransformBy( const FTransform& M ) const", METHODPR_TRIVIAL(FSphere, FSphere, TransformBy, (const FTransform&) const));
		FSphere_.Method("float32 GetVolume() const", METHOD_TRIVIAL(FSphere, GetVolume));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere_Type(
	TEXT("FSphere.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFSphereType);

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere(
	TEXT("FSphere.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFSphereFunctions);
