#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

#include "Bind_SoftObjectPath_Functions.h"

namespace
{
	void BindSoftObjectPathFunctions(FAngelscriptBinds& Binds)
	{
		auto SoftObjectPath_ = Binds.ExistingClassForTarget("FSoftObjectPath");
		SoftObjectPath_.Constructor("void f(const FString& Path)", &FAngelscriptSoftObjectPathBinds::ConstructObjectPathFromString);
		SoftObjectPath_.Constructor("void f(const UObject InObject)", &FAngelscriptSoftObjectPathBinds::ConstructObjectPathFromObject);
		SoftObjectPath_.Method("FString GetLongPackageName() const", METHOD_TRIVIAL(FSoftObjectPath, GetLongPackageName));
		SoftObjectPath_.Method("FString GetAssetName() const", METHOD_TRIVIAL(FSoftObjectPath, GetAssetName));
		SoftObjectPath_.Method("FTopLevelAssetPath GetAssetPath() const", METHOD_TRIVIAL(FSoftObjectPath, GetAssetPath));
		SoftObjectPath_.Method("bool IsValid() const", METHOD_TRIVIAL(FSoftObjectPath, IsValid));
		SoftObjectPath_.Method("bool IsNull() const", METHOD_TRIVIAL(FSoftObjectPath, IsNull));
		SoftObjectPath_.Method("bool IsAsset() const", METHOD_TRIVIAL(FSoftObjectPath, IsAsset));
		SoftObjectPath_.Method("bool IsSubobject() const", METHOD_TRIVIAL(FSoftObjectPath, IsSubobject));
		SoftObjectPath_.Method("bool opEquals(const FSoftObjectPath& Other) const", METHOD_TRIVIAL(FSoftObjectPath, operator==));
		SoftObjectPath_.Method("UObject TryLoad() const", &FAngelscriptSoftObjectPathBinds::TryLoadObject);
		SoftObjectPath_.Method("UObject ResolveObject() const", METHOD_TRIVIAL(FSoftObjectPath, ResolveObject));

		auto SoftClassPath_ = Binds.ExistingClassForTarget("FSoftClassPath");
		SoftClassPath_.Constructor("void f(const FString& Path)", &FAngelscriptSoftObjectPathBinds::ConstructClassPathFromString);
		SoftClassPath_.Constructor("void f(const UClass InClass)", &FAngelscriptSoftObjectPathBinds::ConstructClassPathFromClass);
		SoftClassPath_.Method("FString GetLongPackageName() const", METHOD_TRIVIAL(FSoftClassPath, GetLongPackageName));
		SoftClassPath_.Method("FString GetAssetName() const", METHOD_TRIVIAL(FSoftClassPath, GetAssetName));
		SoftClassPath_.Method("FTopLevelAssetPath GetAssetPath() const", METHOD_TRIVIAL(FSoftClassPath, GetAssetPath));
		SoftClassPath_.Method("bool IsValid() const", METHOD_TRIVIAL(FSoftClassPath, IsValid));
		SoftClassPath_.Method("bool IsNull() const", METHOD_TRIVIAL(FSoftClassPath, IsNull));
		SoftClassPath_.Method("bool IsAsset() const", METHOD_TRIVIAL(FSoftClassPath, IsAsset));
		SoftClassPath_.Method("bool IsSubobject() const", METHOD_TRIVIAL(FSoftClassPath, IsSubobject));
		SoftClassPath_.Method("UClass ResolveClass() const", &FAngelscriptSoftObjectPathBinds::ResolveClass);
		SoftClassPath_.Method("UClass TryLoadClass() const", &FAngelscriptSoftObjectPathBinds::TryLoadClass);
	}

	void BindSoftObjectPathToStringContributions(FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FSoftObjectPath"), &FAngelscriptSoftObjectPathBinds::AppendObjectPathToString);
		FToStringHelper::Register(Binds, TEXT("FSoftClassPath"), &FAngelscriptSoftObjectPathBinds::AppendClassPathToString);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_SoftObjectPath(
	TEXT("SoftObjectPath.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindSoftObjectPathFunctions);

AS_FORCE_LINK const FAngelscriptBind Bind_SoftObjectPath_ToStringContributions(
	TEXT("SoftObjectPath.ToStringContributions"),
	EAngelscriptBindPhase::TypeInfrastructure,
	&BindSoftObjectPathToStringContributions);
