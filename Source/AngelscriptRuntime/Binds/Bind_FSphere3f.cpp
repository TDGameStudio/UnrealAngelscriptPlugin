#include "AngelscriptBinds.h"

#include "Helper_StructType.h"

#include "Bind_FSphere3f_Functions.h"

struct FGetSphere3f
{
	static UScriptStruct* Get();
};

UScriptStruct* FGetSphere3f::Get()
{
	static UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Sphere3f"));
	return ScriptStruct;
}

struct FSphere3fType : TAngelscriptCoreStructType<FSphere, FGetSphere3f, false>
{
	FString GetAngelscriptTypeName() const override { return TEXT("FSphere3f"); }

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override
	{
		OutCppForm.CppType = GetAngelscriptTypeName();
		return true;
	}
};

namespace
{
	void BindFSphere3fType(FAngelscriptBinds& Binds)
	{
		FBindFlags Flags;
		Flags.bPOD = true;
		Flags.ExtraFlags |= asOBJ_BASICMATHTYPE;
		Binds.ValueClassForTarget<FSphere3f>("FSphere3f", Flags);
		Binds.RegisterTypeForTarget(MakeShared<FSphere3fType>());
	}

	void BindFSphere3fFunctions(FAngelscriptBinds& Binds)
	{
		auto FSphere3f_ = Binds.ExistingClassForTarget("FSphere3f");
		FSphere3f_.Constructor("void f()", &FAngelscriptFSphere3fBinds::ConstructDefault)
			.NoDiscard()
			.NativeConstructor("FSphere3f", true, "ForceInit");
		FSphere3f_.Constructor(
			"void f(FVector3f InV, float32 InW)",
			&FAngelscriptFSphere3fBinds::ConstructCenterRadius,
			"FSphere3f",
			true)
			.NoDiscard();
		FSphere3f_.Constructor(
			"void f(const FSphere3f& Sphere)",
			&FAngelscriptFSphere3fBinds::ConstructCopy,
			"FSphere3f",
			true)
			.NoDiscard();
		FSphere3f_.Constructor(
			"void f(const FSphere& Sphere)",
			&FAngelscriptFSphere3fBinds::ConstructFromSphere,
			"FSphere3f",
			true)
			.NoDiscard();
		FSphere3f_.Constructor(
			"void f(const TArray<FVector3f>& Points)",
			&FAngelscriptFSphere3fBinds::ConstructFromPoints)
			.NoDiscard();
		FSphere3f_.Property("float32 W", &FSphere3f::W);
		FSphere3f_.Property("FVector3f Center", &FSphere3f::Center);
		FSphere3f_.Method("FSphere3f opAdd(const FSphere3f& Other) const", METHODPR_TRIVIAL(FSphere3f, FSphere3f, operator+, (const FSphere3f&) const));
		FSphere3f_.Method("FSphere3f& opAddAssign(const FSphere3f& Other)", METHODPR_TRIVIAL(FSphere3f&, FSphere3f, operator+=, (const FSphere3f&)));
		FSphere3f_.Method("bool Equals(const FSphere3f& Sphere, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere3f, Equals, (const FSphere3f&, float) const));
		FSphere3f_.Method("bool IsInside(const FSphere3f& Other, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere3f, IsInside, (const FSphere3f&, float) const));
		FSphere3f_.Method("bool Intersects(const FSphere3f& Other, float32 Tolerance = KINDA_SMALL_NUMBER) const", METHODPR_TRIVIAL(bool, FSphere3f, Intersects, (const FSphere3f&, float) const));
		FSphere3f_.Method("float32 GetVolume() const", METHOD_TRIVIAL(FSphere3f, GetVolume));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere3f_Type(
	TEXT("FSphere3f.Type"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindFSphere3fType);

AS_FORCE_LINK const FAngelscriptBind Bind_FSphere3f(
	TEXT("FSphere3f.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFSphere3fFunctions);
