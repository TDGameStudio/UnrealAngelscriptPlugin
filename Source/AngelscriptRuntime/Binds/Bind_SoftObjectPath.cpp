#include "Bind_SoftObjectPath.h"

#include "AngelscriptBinds.h"

#include "Helper_ToString.h"

/**
 * Soft object and class path binding surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSoftObjectPath Value(const FString& Path);                                                                                  | Constructs an object path from its string representation.                                                            |
 * |                                                                                                                              | @param Path Top-level asset path with an optional subobject suffix.                                                  |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSoftObjectPath Value(const UObject InObject);                                                                               | Constructs the current soft path for InObject, or a null path.                                                       |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FSoftObjectPath.GetLongPackageName() const;                                                                          | Returns the long package name portion of the path.                                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FSoftObjectPath.GetAssetName() const;                                                                                | Returns the top-level asset name portion of the path.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTopLevelAssetPath FSoftObjectPath.GetAssetPath() const;                                                                     | Returns the package-and-asset portion without a subobject suffix.                                                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftObjectPath.IsValid() const;                                                                                        | Reports whether the path has a syntactically valid asset identifier.                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftObjectPath.IsNull() const;                                                                                         | Reports whether no asset path is stored.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftObjectPath.IsAsset() const;                                                                                        | Reports whether the path names a top-level asset rather than a subobject.                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftObjectPath.IsSubobject() const;                                                                                    | Reports whether the path contains a subobject suffix.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = ObjectPath == Other;                                                                                           | Reports exact soft object path equality.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject FSoftObjectPath.TryLoad() const;                                                                                     | Synchronously loads and returns the referenced object, or null on failure.                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UObject FSoftObjectPath.ResolveObject() const;                                                                               | Returns the already-loaded referenced object without loading, or null.                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSoftClassPath Value(const FString& Path);                                                                                   | Constructs a class path from its string representation.                                                              |
 * |                                                                                                                              | @param Path Soft path naming a class asset.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FSoftClassPath Value(const UClass InClass);                                                                                  | Constructs the current soft path for InClass, or a null path.                                                        |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FSoftClassPath.GetLongPackageName() const;                                                                           | Returns the long package name portion of the class path.                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FSoftClassPath.GetAssetName() const;                                                                                 | Returns the top-level class asset name.                                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTopLevelAssetPath FSoftClassPath.GetAssetPath() const;                                                                      | Returns the class package-and-asset portion without a subobject suffix.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftClassPath.IsValid() const;                                                                                         | Reports whether the class path has a syntactically valid asset identifier.                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftClassPath.IsNull() const;                                                                                          | Reports whether no class path is stored.                                                                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftClassPath.IsAsset() const;                                                                                         | Reports whether the path names a top-level class asset.                                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FSoftClassPath.IsSubobject() const;                                                                                     | Reports whether the class path contains a subobject suffix.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass FSoftClassPath.ResolveClass() const;                                                                                  | Returns the already-loaded referenced class without loading, or null.                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UClass FSoftClassPath.TryLoadClass() const;                                                                                  | Synchronously loads and returns the referenced class, or null on failure.                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_SoftObjectPath(
	TEXT("SoftObjectPath.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});

AS_FORCE_LINK const FAngelscriptBind Bind_SoftObjectPath_ToStringContributions(
	TEXT("SoftObjectPath.ToStringContributions"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FSoftObjectPath"), &FAngelscriptSoftObjectPathBinds::AppendObjectPathToString);
		FToStringHelper::Register(Binds, TEXT("FSoftClassPath"), &FAngelscriptSoftObjectPathBinds::AppendClassPathToString);
	});
