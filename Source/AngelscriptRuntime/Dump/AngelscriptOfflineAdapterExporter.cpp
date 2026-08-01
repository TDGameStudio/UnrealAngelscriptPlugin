#include "AngelscriptOfflineAdapterExporter.h"

#include "AngelscriptOfflineContractIdentity.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		struct FAdapterDefinition
		{
			const TCHAR* Name;
			const TCHAR* Version;
			TArray<FString> RequiredTraits;
		};

		const TArray<FAdapterDefinition>& GetDefinitions()
		{
			static const TArray<FAdapterDefinition> Definitions = {
				{
					TEXT("TArray"),
					TEXT("1"),
					{
						TEXT("element.construct"),
						TEXT("element.destruct"),
						TEXT("element.copy"),
					},
				},
				{
					TEXT("TMap"),
					TEXT("1"),
					{
						TEXT("key.construct"),
						TEXT("key.destruct"),
						TEXT("key.copy"),
						TEXT("key.compare"),
						TEXT("key.hash"),
						TEXT("value.construct"),
						TEXT("value.destruct"),
						TEXT("value.copy"),
					},
				},
				{
					TEXT("TSet"),
					TEXT("1"),
					{
						TEXT("element.construct"),
						TEXT("element.destruct"),
						TEXT("element.copy"),
						TEXT("element.compare"),
						TEXT("element.hash"),
					},
				},
				{
					TEXT("TOptional"),
					TEXT("1"),
					{
						TEXT("value.construct"),
						TEXT("value.destruct"),
						TEXT("value.copy"),
					},
				},
				{
					TEXT("TObjectPtr"),
					TEXT("1"),
					{TEXT("subtype.uobject")},
				},
				{
					TEXT("TWeakObjectPtr"),
					TEXT("1"),
					{TEXT("subtype.uobject")},
				},
				{
					TEXT("TSoftObjectPtr"),
					TEXT("1"),
					{TEXT("subtype.uobject")},
				},
				{
					TEXT("TSubclassOf"),
					TEXT("1"),
					{TEXT("subtype.uclass")},
				},
				{
					TEXT("TSoftClassPtr"),
					TEXT("1"),
					{TEXT("subtype.uclass")},
				},
			};
			return Definitions;
		}

		bool IsAdapterOwnedType(
			const FString& AdapterName,
			const FString& TypeName)
		{
			if (TypeName == AdapterName)
			{
				return true;
			}
			if (AdapterName == TEXT("TArray"))
			{
				return TypeName == TEXT("TArrayIterator")
					|| TypeName == TEXT("TArrayConstIterator");
			}
			if (AdapterName == TEXT("TMap"))
			{
				return TypeName == TEXT("TMapIterator")
					|| TypeName == TEXT("TMapConstIterator");
			}
			if (AdapterName == TEXT("TSet"))
			{
				return TypeName == TEXT("TSetIterator")
					|| TypeName == TEXT("TSetConstIterator");
			}
			return false;
		}

		FString GetSurfaceFact(const FSymbolRecord& Symbol)
		{
			FString Declaration;
			switch (Symbol.Kind)
			{
			case ESymbolKind::Type:
			case ESymbolKind::Typedef:
			case ESymbolKind::Funcdef:
			case ESymbolKind::Delegate:
				Declaration = Symbol.Type.CompleteDeclaration;
				break;
			case ESymbolKind::Callable:
				Declaration = Symbol.Callable.Declaration;
				break;
			case ESymbolKind::Property:
			case ESymbolKind::Global:
				Declaration = Symbol.Property.CompleteDeclaration;
				break;
			default:
				break;
			}
			return FString::Printf(
				TEXT("%s|%s|%s"),
				LexToString(Symbol.Kind),
				*Symbol.StableId,
				*NormalizeDeclaration(Declaration));
		}
	}

	TArray<FAdapterRecord>
	FAngelscriptOfflineAdapterExporter::ExportAndAssign(
		TArray<FSymbolRecord>& Symbols)
	{
		TArray<FAdapterRecord> Result;
		for (const FAdapterDefinition& Definition : GetDefinitions())
		{
			TArray<FString> OwnerStableIds;
			for (const FSymbolRecord& Symbol : Symbols)
			{
				if ((Symbol.Kind == ESymbolKind::Type
					|| Symbol.Kind == ESymbolKind::Typedef)
					&& IsAdapterOwnedType(
						Definition.Name,
						Symbol.Type.Name))
				{
					OwnerStableIds.Add(Symbol.StableId);
				}
			}
			if (OwnerStableIds.IsEmpty())
			{
				continue;
			}
			OwnerStableIds.Sort();

			FAdapterRecord Adapter;
			Adapter.Name = Definition.Name;
			Adapter.Version = Definition.Version;
			Adapter.StableId = MakeStableAdapterId(
				Adapter.Name,
				Adapter.Version);
			Adapter.bDeclarativeOnly = false;
			Adapter.RequiredEngineProperties = {
				TEXT("angelscript.fork"),
				TEXT("unreal.major"),
				TEXT("unreal.minor"),
			};
			Adapter.RequiredTraits = Definition.RequiredTraits;

			TArray<FString> SurfaceFacts;
			for (FSymbolRecord& Symbol : Symbols)
			{
				const bool bAdapterType =
					OwnerStableIds.Contains(Symbol.StableId);
				const bool bAdapterMember =
					(Symbol.Kind == ESymbolKind::Callable
						&& OwnerStableIds.Contains(
							Symbol.Callable.OwnerStableId))
					|| ((Symbol.Kind == ESymbolKind::Property
							|| Symbol.Kind == ESymbolKind::Global)
						&& OwnerStableIds.Contains(
							Symbol.Property.OwnerStableId));
				if (!bAdapterType && !bAdapterMember)
				{
					continue;
				}

				SurfaceFacts.Add(GetSurfaceFact(Symbol));
				if (bAdapterType)
				{
					Symbol.Type.AdapterStableId = Adapter.StableId;
				}
				else if (Symbol.Kind == ESymbolKind::Callable)
				{
					Symbol.Callable.AdapterStableId =
						Adapter.StableId;
				}
				else
				{
					Symbol.Property.AdapterStableId =
						Adapter.StableId;
				}
			}
			SurfaceFacts.Sort();
			Adapter.SurfaceHash = Sha256Utf8(FString::Printf(
				TEXT("adapter-surface-v1\n%s\n%s\n%s"),
				*Adapter.Name,
				*Adapter.Version,
				*FString::Join(SurfaceFacts, TEXT("\n"))));
			Result.Add(MoveTemp(Adapter));
		}

		Result.Sort([](
			const FAdapterRecord& Left,
			const FAdapterRecord& Right)
		{
			return Left.StableId < Right.StableId;
		});
		return Result;
	}
}
