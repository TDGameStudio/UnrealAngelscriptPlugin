#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"

#include "Engine/CollisionProfile.h"

/**
 * Project collision-profile constants generated from UCollisionProfile at bind time.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | const FName CollisionProfile::<ProfileIdentifier>;                                       | Names one configured collision profile; invalid identifier characters become underscores.                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

namespace CollisionProfileBind
{
	FString MakeIdentifier(const FName& ProfileName)
	{
		FString Identifier = ProfileName.ToString();
		for (int32 Index = Identifier.Len() - 1; Index >= 0; --Index)
		{
			if (!FAngelscriptEngine::IsValidIdentifierCharacter(Identifier[Index]))
			{
				Identifier[Index] = '_';
			}
		}

		if (!Identifier.IsEmpty() && Identifier[0] >= '0' && Identifier[0] <= '9')
		{
			Identifier = TEXT("_") + Identifier;
		}

		return Identifier;
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_CollisionProfile(
	TEXT("CollisionProfile"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "CollisionProfile");

		TArray<TSharedPtr<FName>> CollisionProfiles;
		UCollisionProfile::GetProfileNames(CollisionProfiles);

		static TArray<FName> CollisionProfileNames;
		static TSet<FString> BoundIdentifiers;

		CollisionProfileNames.Empty(CollisionProfiles.Num());
		BoundIdentifiers.Empty(CollisionProfiles.Num());

		for (int32 Index = 0; Index < CollisionProfiles.Num(); ++Index)
		{
			const FName CollisionProfileName = *CollisionProfiles[Index].Get();
			const FString Identifier = CollisionProfileBind::MakeIdentifier(CollisionProfileName);
			if (Identifier.IsEmpty())
			{
				UE_LOG(Angelscript, Warning, TEXT("Skipping empty CollisionProfile identifier for profile '%s'"), *CollisionProfileName.ToString());
				continue;
			}

			if (BoundIdentifiers.Contains(Identifier))
			{
				UE_LOG(Angelscript, Warning, TEXT("Skipping duplicate CollisionProfile identifier '%s' for profile '%s'"), *Identifier, *CollisionProfileName.ToString());
				continue;
			}

			BoundIdentifiers.Add(Identifier);
			CollisionProfileNames.Add(CollisionProfileName);

			const FString Declaration = TEXT("const FName ") + Identifier;
			FAngelscriptBoundProperty BoundProfile = Binds.BindGlobalVariableForTarget(TCHAR_TO_ANSI(*Declaration), &CollisionProfileNames.Last());

#if WITH_EDITORONLY_DATA
			FCollisionResponseTemplate CollisionResponseTemplate;
			if (BoundProfile.IsValid() && UCollisionProfile::Get()->GetProfileTemplate(CollisionProfileName, CollisionResponseTemplate))
			{
				FAngelscriptDocs::AddDocumentationForGlobalVariable(Binds.GetTargetEngine(), BoundProfile.GetPropertyId(), CollisionResponseTemplate.HelpMessage);
			}
#endif
		}
	});
