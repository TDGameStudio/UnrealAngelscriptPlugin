#include "AngelscriptOfflineContractSerializer.h"

#include "Algo/Unique.h"

namespace AngelscriptOfflineContract
{
	namespace
	{
		FCanonicalJsonValue StringArray(const TArray<FString>& Values)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Array();
			for (const FString& Value : Values)
			{
				Result.Add(FCanonicalJsonValue::String(Value));
			}
			return Result;
		}

		FCanonicalJsonValue SortedStringArray(const TArray<FString>& Values)
		{
			TArray<FString> Sorted = Values;
			Sorted.Sort([](const FString& Left, const FString& Right)
			{
				return Left.Compare(
					Right,
					ESearchCase::CaseSensitive) < 0;
			});
			Sorted.SetNum(Algo::Unique(Sorted));
			return StringArray(Sorted);
		}

		FCanonicalJsonValue StringMap(const TMap<FString, FString>& Values)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			for (const TPair<FString, FString>& Pair : Values)
			{
				Result.Set(
					Pair.Key,
					FCanonicalJsonValue::String(Pair.Value));
			}
			return Result;
		}

		FCanonicalJsonValue BooleanMap(const TMap<FString, bool>& Values)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			for (const TPair<FString, bool>& Pair : Values)
			{
				Result.Set(
					Pair.Key,
					FCanonicalJsonValue::Boolean(Pair.Value));
			}
			return Result;
		}

		FCanonicalJsonValue OriginJson(const FOriginRecord& Origin)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("kind"),
				FCanonicalJsonValue::String(LexToString(Origin.Kind)));
			Result.Set(TEXT("layer"),
				FCanonicalJsonValue::String(LexToString(Origin.Layer)));
			Result.Set(TEXT("module"),
				FCanonicalJsonValue::String(Origin.Module));
			Result.Set(TEXT("plugin"),
				FCanonicalJsonValue::String(Origin.Plugin));
			Result.Set(TEXT("stableModuleId"),
				FCanonicalJsonValue::String(Origin.StableModuleId));
			return Result;
		}

		FCanonicalJsonValue ScopeJson(const FScopeRecord& Scope)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("complete"),
				FCanonicalJsonValue::Boolean(Scope.bComplete));
			Result.Set(TEXT("included"),
				SortedStringArray(Scope.Included));
			Result.Set(TEXT("skipped"),
				SortedStringArray(Scope.Skipped));
			Result.Set(TEXT("state"),
				FCanonicalJsonValue::String(Scope.State));
			return Result;
		}

		FCanonicalJsonValue AvailabilityJson(
			const EAvailability Availability)
		{
			return FCanonicalJsonValue::String(
				LexToString(Availability));
		}

		FCanonicalJsonValue ParameterJson(
			const FParameterRecord& Parameter)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("const"),
				FCanonicalJsonValue::Boolean(Parameter.bConst));
			Result.Set(TEXT("defaultExpression"),
				FCanonicalJsonValue::String(Parameter.DefaultExpression));
			Result.Set(TEXT("direction"),
				FCanonicalJsonValue::String(
					LexToString(Parameter.Direction)));
			Result.Set(TEXT("handle"),
				FCanonicalJsonValue::Boolean(Parameter.bHandle));
			Result.Set(TEXT("hasDefault"),
				FCanonicalJsonValue::Boolean(Parameter.bHasDefault));
			Result.Set(TEXT("name"),
				FCanonicalJsonValue::String(Parameter.Name));
			Result.Set(TEXT("reference"),
				FCanonicalJsonValue::Boolean(Parameter.bReference));
			Result.Set(TEXT("resourceKind"),
				FCanonicalJsonValue::String(Parameter.ResourceKind));
			Result.Set(TEXT("resourceTypeStableId"),
				FCanonicalJsonValue::String(
					Parameter.ResourceTypeStableId));
			Result.Set(TEXT("type"),
				FCanonicalJsonValue::String(
					Parameter.TypeDeclaration));
			return Result;
		}

		FCanonicalJsonValue TraitsJson(const FTypeTraitsRecord& Traits)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("comparable"),
				FCanonicalJsonValue::Boolean(Traits.bComparable));
			Result.Set(TEXT("constructible"),
				FCanonicalJsonValue::Boolean(Traits.bConstructible));
			Result.Set(TEXT("copyAssignable"),
				FCanonicalJsonValue::Boolean(Traits.bCopyAssignable));
			Result.Set(TEXT("copyConstructible"),
				FCanonicalJsonValue::Boolean(Traits.bCopyConstructible));
			Result.Set(TEXT("destructible"),
				FCanonicalJsonValue::Boolean(Traits.bDestructible));
			Result.Set(TEXT("garbageCollected"),
				FCanonicalJsonValue::Boolean(
					Traits.bGarbageCollected));
			Result.Set(TEXT("hashable"),
				FCanonicalJsonValue::Boolean(Traits.bHashable));
			Result.Set(TEXT("templateEligible"),
				FCanonicalJsonValue::Boolean(
					Traits.bTemplateEligible));
			return Result;
		}

		FCanonicalJsonValue TypeJson(const FTypeRecord& Type)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("adapterStableId"),
				FCanonicalJsonValue::String(Type.AdapterStableId));
			Result.Set(TEXT("availability"),
				AvailabilityJson(Type.Availability));
			Result.Set(TEXT("baseStableId"),
				FCanonicalJsonValue::String(Type.BaseStableId));
			Result.Set(TEXT("compileAlignment"),
				FCanonicalJsonValue::Integer(Type.CompileAlignment));
			Result.Set(TEXT("compileSize"),
				FCanonicalJsonValue::Integer(Type.CompileSize));
			Result.Set(TEXT("completeDeclaration"),
				FCanonicalJsonValue::String(Type.CompleteDeclaration));

			FCanonicalJsonValue EnumValues = FCanonicalJsonValue::Array();
			TArray<FEnumValueRecord> SortedEnumValues = Type.EnumValues;
			SortedEnumValues.Sort([](
				const FEnumValueRecord& Left,
				const FEnumValueRecord& Right)
			{
				return Left.StableId < Right.StableId;
			});
			for (const FEnumValueRecord& EnumValue : SortedEnumValues)
			{
				FCanonicalJsonValue Value = FCanonicalJsonValue::Object();
				Value.Set(TEXT("name"),
					FCanonicalJsonValue::String(EnumValue.Name));
				Value.Set(TEXT("stableId"),
					FCanonicalJsonValue::String(EnumValue.StableId));
				Value.Set(TEXT("value"),
					FCanonicalJsonValue::Integer(EnumValue.Value));
				EnumValues.Add(MoveTemp(Value));
			}
			Result.Set(TEXT("enumValues"), MoveTemp(EnumValues));
			Result.Set(TEXT("flags"), SortedStringArray(Type.Flags));
			Result.Set(TEXT("handle"),
				FCanonicalJsonValue::Boolean(Type.bHandle));
			Result.Set(TEXT("interfaces"),
				SortedStringArray(Type.InterfaceStableIds));
			Result.Set(TEXT("kind"),
				FCanonicalJsonValue::String(LexToString(Type.Kind)));
			Result.Set(TEXT("members"),
				SortedStringArray(Type.MemberStableIds));
			Result.Set(TEXT("name"),
				FCanonicalJsonValue::String(Type.Name));
			Result.Set(TEXT("namespace"),
				FCanonicalJsonValue::String(Type.Namespace));
			Result.Set(TEXT("origin"), OriginJson(Type.Origin));
			Result.Set(TEXT("stableId"),
				FCanonicalJsonValue::String(Type.StableId));
			Result.Set(TEXT("templateDefinition"),
				FCanonicalJsonValue::Boolean(Type.bTemplateDefinition));
			Result.Set(TEXT("templateSubtypes"),
				StringArray(Type.TemplateSubtypeDeclarations));
			Result.Set(TEXT("traits"), TraitsJson(Type.Traits));
			Result.Set(TEXT("ueTypePath"),
				FCanonicalJsonValue::String(Type.UETypePath));
			return Result;
		}

		FCanonicalJsonValue CallableJson(const FCallableRecord& Callable)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("access"),
				FCanonicalJsonValue::String(Callable.Access));
			Result.Set(TEXT("adapterStableId"),
				FCanonicalJsonValue::String(
					Callable.AdapterStableId));
			Result.Set(TEXT("availability"),
				AvailabilityJson(Callable.Availability));
			Result.Set(TEXT("behavior"),
				FCanonicalJsonValue::String(Callable.Behavior));
			Result.Set(TEXT("const"),
				FCanonicalJsonValue::Boolean(Callable.bConst));
			Result.Set(TEXT("declaration"),
				FCanonicalJsonValue::String(Callable.Declaration));
			Result.Set(TEXT("final"),
				FCanonicalJsonValue::Boolean(Callable.bFinal));
			Result.Set(TEXT("kind"),
				FCanonicalJsonValue::String(
					LexToString(Callable.Kind)));
			Result.Set(TEXT("name"),
				FCanonicalJsonValue::String(Callable.Name));
			Result.Set(TEXT("namespace"),
				FCanonicalJsonValue::String(Callable.Namespace));
			Result.Set(TEXT("origin"), OriginJson(Callable.Origin));
			Result.Set(TEXT("override"),
				FCanonicalJsonValue::Boolean(Callable.bOverride));
			Result.Set(TEXT("ownerStableId"),
				FCanonicalJsonValue::String(
					Callable.OwnerStableId));

			FCanonicalJsonValue Parameters = FCanonicalJsonValue::Array();
			for (const FParameterRecord& Parameter : Callable.Parameters)
			{
				Parameters.Add(ParameterJson(Parameter));
			}
			Result.Set(TEXT("parameters"), MoveTemp(Parameters));
			Result.Set(TEXT("propertyAccessor"),
				FCanonicalJsonValue::Boolean(
					Callable.bPropertyAccessor));
			Result.Set(TEXT("returnType"),
				FCanonicalJsonValue::String(Callable.ReturnType));
			Result.Set(TEXT("stableId"),
				FCanonicalJsonValue::String(Callable.StableId));
			Result.Set(TEXT("static"),
				FCanonicalJsonValue::Boolean(Callable.bStatic));
			Result.Set(TEXT("ueFunctionPath"),
				FCanonicalJsonValue::String(
					Callable.UEFunctionPath));
			return Result;
		}

		FCanonicalJsonValue PropertyJson(const FPropertyRecord& Property)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("access"),
				FCanonicalJsonValue::String(Property.Access));
			Result.Set(TEXT("adapterStableId"),
				FCanonicalJsonValue::String(
					Property.AdapterStableId));
			Result.Set(TEXT("availability"),
				AvailabilityJson(Property.Availability));
			Result.Set(TEXT("completeDeclaration"),
				FCanonicalJsonValue::String(
					Property.CompleteDeclaration));
			Result.Set(TEXT("const"),
				FCanonicalJsonValue::Boolean(Property.bConst));
			Result.Set(TEXT("name"),
				FCanonicalJsonValue::String(Property.Name));
			Result.Set(TEXT("namespace"),
				FCanonicalJsonValue::String(Property.Namespace));
			Result.Set(TEXT("origin"), OriginJson(Property.Origin));
			Result.Set(TEXT("ownerStableId"),
				FCanonicalJsonValue::String(
					Property.OwnerStableId));
			Result.Set(TEXT("readable"),
				FCanonicalJsonValue::Boolean(Property.bReadable));
			Result.Set(TEXT("stableId"),
				FCanonicalJsonValue::String(Property.StableId));
			Result.Set(TEXT("static"),
				FCanonicalJsonValue::Boolean(Property.bStatic));
			Result.Set(TEXT("type"),
				FCanonicalJsonValue::String(
					Property.TypeDeclaration));
			Result.Set(TEXT("uePropertyPath"),
				FCanonicalJsonValue::String(
					Property.UEPropertyPath));
			Result.Set(TEXT("writable"),
				FCanonicalJsonValue::Boolean(Property.bWritable));
			return Result;
		}

		FCanonicalJsonValue AdapterJson(const FAdapterRecord& Adapter)
		{
			FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
			Result.Set(TEXT("declarativeOnly"),
				FCanonicalJsonValue::Boolean(
					Adapter.bDeclarativeOnly));
			Result.Set(TEXT("name"),
				FCanonicalJsonValue::String(Adapter.Name));
			Result.Set(TEXT("requiredEngineProperties"),
				SortedStringArray(Adapter.RequiredEngineProperties));
			Result.Set(TEXT("requiredTraits"),
				SortedStringArray(Adapter.RequiredTraits));
			Result.Set(TEXT("stableId"),
				FCanonicalJsonValue::String(Adapter.StableId));
			Result.Set(TEXT("surfaceHash"),
				FCanonicalJsonValue::String(Adapter.SurfaceHash));
			Result.Set(TEXT("version"),
				FCanonicalJsonValue::String(Adapter.Version));
			return Result;
		}
	}

	FCanonicalJsonValue ToCanonicalJson(const FSymbolRecord& Record)
	{
		FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
		Result.Set(TEXT("canonicalIdentity"),
			FCanonicalJsonValue::String(Record.CanonicalIdentity));
		Result.Set(TEXT("kind"),
			FCanonicalJsonValue::String(LexToString(Record.Kind)));
		Result.Set(TEXT("origin"), OriginJson(Record.Origin));
		Result.Set(TEXT("schema"),
			FCanonicalJsonValue::String(SchemaVersion));
		Result.Set(TEXT("stableId"),
			FCanonicalJsonValue::String(Record.StableId));

		switch (Record.Kind)
		{
		case ESymbolKind::Type:
		case ESymbolKind::Typedef:
		case ESymbolKind::Funcdef:
		case ESymbolKind::Delegate:
			Result.Set(TEXT("type"), TypeJson(Record.Type));
			break;
		case ESymbolKind::Callable:
			Result.Set(TEXT("callable"), CallableJson(Record.Callable));
			break;
		case ESymbolKind::Property:
		case ESymbolKind::Global:
			Result.Set(TEXT("property"), PropertyJson(Record.Property));
			break;
		default:
			break;
		}
		return Result;
	}

	FCanonicalJsonValue ToCanonicalJson(const FAssetRecord& Record)
	{
		FCanonicalJsonValue Result = FCanonicalJsonValue::Object();
		Result.Set(TEXT("assetClassPath"),
			FCanonicalJsonValue::String(Record.AssetClassPath));
		Result.Set(TEXT("availability"),
			AvailabilityJson(Record.Availability));
		Result.Set(TEXT("baseClassPath"),
			FCanonicalJsonValue::String(Record.BaseClassPath));
		Result.Set(TEXT("generatedClassPath"),
			FCanonicalJsonValue::String(Record.GeneratedClassPath));
		Result.Set(TEXT("mountPoint"),
			FCanonicalJsonValue::String(Record.MountPoint));
		Result.Set(TEXT("objectPath"),
			FCanonicalJsonValue::String(Record.ObjectPath));
		Result.Set(TEXT("originModule"),
			FCanonicalJsonValue::String(Record.OriginModule));
		Result.Set(TEXT("originPlugin"),
			FCanonicalJsonValue::String(Record.OriginPlugin));
		Result.Set(TEXT("packagePath"),
			FCanonicalJsonValue::String(Record.PackagePath));
		Result.Set(TEXT("redirectSource"),
			FCanonicalJsonValue::String(Record.RedirectSource));
		Result.Set(TEXT("redirectTarget"),
			FCanonicalJsonValue::String(Record.RedirectTarget));
		Result.Set(TEXT("schema"),
			FCanonicalJsonValue::String(SchemaVersion));
		Result.Set(TEXT("stableId"),
			FCanonicalJsonValue::String(Record.StableId));
		Result.Set(TEXT("typeCheckTags"),
			StringMap(Record.TypeCheckTags));
		return Result;
	}

	FCanonicalJsonValue ToCanonicalJson(const FManifestRecord& Record)
	{
		FCanonicalJsonValue Result = FCanonicalJsonValue::Object();

		FCanonicalJsonValue Adapters = FCanonicalJsonValue::Array();
		TArray<FAdapterRecord> SortedAdapters = Record.Adapters;
		SortedAdapters.Sort([](
			const FAdapterRecord& Left,
			const FAdapterRecord& Right)
		{
			return Left.StableId < Right.StableId;
		});
		for (const FAdapterRecord& Adapter : SortedAdapters)
		{
			Adapters.Add(AdapterJson(Adapter));
		}

		FCanonicalJsonValue Files = FCanonicalJsonValue::Array();
		TArray<FFileRecord> SortedFiles = Record.Files;
		SortedFiles.Sort([](
			const FFileRecord& Left,
			const FFileRecord& Right)
		{
			return Left.Name < Right.Name;
		});
		for (const FFileRecord& File : SortedFiles)
		{
			FCanonicalJsonValue FileJson = FCanonicalJsonValue::Object();
			FileJson.Set(TEXT("byteCount"),
				FCanonicalJsonValue::Integer(File.ByteCount));
			FileJson.Set(TEXT("name"),
				FCanonicalJsonValue::String(File.Name));
			FileJson.Set(TEXT("recordCount"),
				FCanonicalJsonValue::Integer(File.RecordCount));
			FileJson.Set(TEXT("sha256"),
				FCanonicalJsonValue::String(File.Sha256));
			Files.Add(MoveTemp(FileJson));
		}

		FCanonicalJsonValue Schema = FCanonicalJsonValue::Object();
		Schema.Set(TEXT("major"),
			FCanonicalJsonValue::Integer(Record.Schema.Major));
		Schema.Set(TEXT("minor"),
			FCanonicalJsonValue::Integer(Record.Schema.Minor));

		Result.Set(TEXT("adapters"), MoveTemp(Adapters));
		Result.Set(TEXT("assetScope"), ScopeJson(Record.AssetScope));
		Result.Set(TEXT("bundleIdentity"),
			FCanonicalJsonValue::String(Record.BundleIdentity));
		Result.Set(TEXT("bundleKind"),
			FCanonicalJsonValue::String(LexToString(Record.BundleKind)));
		Result.Set(TEXT("compilerContractVersion"),
			FCanonicalJsonValue::String(
				Record.CompilerContractVersion));
		Result.Set(TEXT("configuration"),
			FCanonicalJsonValue::String(Record.Configuration));
		Result.Set(TEXT("engineProperties"),
			StringMap(Record.EngineProperties));
		Result.Set(TEXT("featureFlags"),
			BooleanMap(Record.FeatureFlags));
		Result.Set(TEXT("files"), MoveTemp(Files));
		Result.Set(TEXT("forkVersion"),
			FCanonicalJsonValue::String(Record.ForkVersion));
		Result.Set(TEXT("loadedModules"),
			SortedStringArray(Record.LoadedModules));
		Result.Set(TEXT("loadedPlugins"),
			SortedStringArray(Record.LoadedPlugins));
		Result.Set(TEXT("platform"),
			FCanonicalJsonValue::String(Record.Platform));
		Result.Set(TEXT("pluginVersion"),
			FCanonicalJsonValue::String(Record.PluginVersion));
		Result.Set(TEXT("producerName"),
			FCanonicalJsonValue::String(Record.ProducerName));
		Result.Set(TEXT("producerVersion"),
			FCanonicalJsonValue::String(Record.ProducerVersion));
		Result.Set(TEXT("requiredFields"),
			SortedStringArray(Record.RequiredFields));
		Result.Set(TEXT("schema"), MoveTemp(Schema));
		Result.Set(TEXT("symbolScope"), ScopeJson(Record.SymbolScope));
		Result.Set(TEXT("unrealVersion"),
			FCanonicalJsonValue::String(Record.UnrealVersion));
		return Result;
	}

	TArray<uint8> SerializeSymbolRecords(
		const TArray<FSymbolRecord>& Records)
	{
		TArray<FCanonicalJsonLine> Lines;
		Lines.Reserve(Records.Num());
		for (const FSymbolRecord& Record : Records)
		{
			Lines.Add({Record.StableId, ToCanonicalJson(Record)});
		}
		return SerializeCanonicalJsonLines(Lines);
	}

	TArray<uint8> SerializeAssetRecords(
		const TArray<FAssetRecord>& Records)
	{
		TArray<FCanonicalJsonLine> Lines;
		Lines.Reserve(Records.Num());
		for (const FAssetRecord& Record : Records)
		{
			Lines.Add({Record.StableId, ToCanonicalJson(Record)});
		}
		return SerializeCanonicalJsonLines(Lines);
	}
}
