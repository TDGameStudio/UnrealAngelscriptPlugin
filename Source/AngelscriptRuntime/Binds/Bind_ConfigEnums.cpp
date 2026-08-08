#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"

#include "Engine/CollisionProfile.h"

namespace
{
	void BindConfigEnums(FAngelscriptBinds& Binds)
	{
		auto ETraceTypeQuery_ = Binds.EnumForTarget("ETraceTypeQuery");
		auto ECollisionChannel_ = Binds.EnumForTarget("ECollisionChannel");
		auto EObjectTypeQuery_ = Binds.EnumForTarget("EObjectTypeQuery");

		UCollisionProfile* Collision = UCollisionProfile::Get();
		//WILL-EDIT
		//for (FCustomChannelSetup& Profile : Collision->DefaultChannelResponses)
		int32 count = Collision->GetNumOfProfiles();
		for (int i = 0; i < count; i++)
		{
			//const FString BPName = Profile.Name.ToString().Replace(TEXT(" "), TEXT("_"));
			const FCollisionResponseTemplate* temp = Collision->GetProfileByIndex(i);
			const FString BPName = temp->Name.ToString().Replace(TEXT(" "), TEXT("_"));

			//if (Profile.bTraceType)
			if (temp->CollisionEnabled == ECollisionEnabled::Type::QueryOnly)
			{
				//int32 BPValue = (int32)Collision->ConvertToTraceType(Profile.Channel);
				int32 BPValue = (int32)Collision->ConvertToTraceType(temp->ObjectType);
				ETraceTypeQuery_[BPName] = BPValue;
			}
			else
			{
				//int32 BPValue = (int32)Collision->ConvertToObjectType(Profile.Channel);
				int32 BPValue = (int32)Collision->ConvertToObjectType(temp->ObjectType);
				EObjectTypeQuery_[BPName] = BPValue;
			}

			//ECollisionChannel_[BPName] = (int32)Profile.Channel;
			ECollisionChannel_[BPName] = (int32)temp->ObjectType;
		}

		ETraceTypeQuery_["Visibility"] = (int32)Collision->ConvertToTraceType(ECC_Visibility);
		ETraceTypeQuery_["Camera"] = (int32)Collision->ConvertToTraceType(ECC_Camera);

		EObjectTypeQuery_["WorldStatic"] = (int32)Collision->ConvertToObjectType(ECC_WorldStatic);
		EObjectTypeQuery_["WorldDynamic"] = (int32)Collision->ConvertToObjectType(ECC_WorldDynamic);
		EObjectTypeQuery_["PhysicsBody"] = (int32)Collision->ConvertToObjectType(ECC_PhysicsBody);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_ConfigEnums(
	TEXT("ConfigEnums"),
	EAngelscriptBindPhase::TypeDeclarations,
	&BindConfigEnums);
