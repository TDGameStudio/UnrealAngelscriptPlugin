#include "Cache/AngelscriptCacheDependencyPropagation.h"

namespace AngelscriptCacheDependencyPropagation_Private
{
	struct FModuleView
	{
		FAngelscriptStableModuleKey ModuleKey;
		const FAngelscriptCachedModuleInterface* Interface = nullptr;
		const FAngelscriptCachedModuleState* State = nullptr;
		TArray<const FAngelscriptCachedTypeSchema*> Types;
		TArray<const FAngelscriptCachedFunctionBody*> FunctionBodies;
	};

	struct FDeclarationAuthority
	{
		EAngelscriptCacheReferenceKind Kind =
			EAngelscriptCacheReferenceKind::Invalid;
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedDeclaration* Declaration = nullptr;
	};

	struct FModuleAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedModuleInterface* Interface = nullptr;
	};

	struct FTypeAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedTypeSchema* Type = nullptr;
	};

	struct FPropertyAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedPropertySchema* Property = nullptr;
	};

	struct FGlobalAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedGlobalSchema* Global = nullptr;
	};

	struct FBodyAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedFunctionBody* Body = nullptr;
	};

	struct FHardValueAuthority
	{
		EAngelscriptCacheReferenceKind Kind =
			EAngelscriptCacheReferenceKind::Invalid;
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedHardValue* HardValue = nullptr;
	};

	struct FInitializerAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedInitializerUnit* Initializer = nullptr;
	};

	struct FImportAuthority
	{
		FAngelscriptHash256 StableKey;
		const FAngelscriptCachedImportDeclaration* Import = nullptr;
	};

	struct FCatalog
	{
		TArray<FModuleAuthority> Modules;
		TArray<FDeclarationAuthority> Declarations;
		TArray<FTypeAuthority> Types;
		TArray<FPropertyAuthority> Properties;
		TArray<FGlobalAuthority> Globals;
		TArray<FBodyAuthority> Bodies;
		TArray<FHardValueAuthority> HardValues;
		TArray<FInitializerAuthority> Initializers;
		TArray<FImportAuthority> Imports;
	};

	struct FResolvedAuthority
	{
		bool bFound = false;
		FAngelscriptHash256 Abi;
		TOptional<FAngelscriptHash256> ContentOrValue;
	};

	struct FExternalMemo
	{
		EAngelscriptCacheReferenceKind Kind =
			EAngelscriptCacheReferenceKind::Invalid;
		FAngelscriptHash256 StableKey;
		bool bFound = false;
		FAngelscriptCacheCurrentSymbol Symbol;
	};

	static FAngelscriptCacheDependencyPropagationResult Failure(
		const EAngelscriptCacheDependencyPropagationError Error,
		FString&& Detail)
	{
		FAngelscriptCacheDependencyPropagationResult Result;
		Result.Error = Error;
		Result.Detail = MoveTemp(Detail);
		return Result;
	}

	static int32 CompareHash(
		const FAngelscriptHash256& Left,
		const FAngelscriptHash256& Right)
	{
		if (Left == Right)
		{
			return 0;
		}
		return Left < Right ? -1 : 1;
	}

	static int32 CompareReferenceIdentity(
		const EAngelscriptCacheReferenceKind LeftKind,
		const FAngelscriptHash256& LeftKey,
		const EAngelscriptCacheReferenceKind RightKind,
		const FAngelscriptHash256& RightKey)
	{
		if (LeftKind != RightKind)
		{
			return static_cast<uint8>(LeftKind) < static_cast<uint8>(RightKind)
				? -1 : 1;
		}
		return CompareHash(LeftKey, RightKey);
	}

	template <typename EntryType, typename KeyProjectionType>
	static int32 LowerBoundHash(
		const TArray<EntryType>& Values,
		const FAngelscriptHash256& Key,
		KeyProjectionType&& Project)
	{
		int32 First = 0;
		int32 Count = Values.Num();
		while (Count > 0)
		{
			const int32 Step = Count / 2;
			const int32 Middle = First + Step;
			if (Project(Values[Middle]) < Key)
			{
				First = Middle + 1;
				Count -= Step + 1;
			}
			else
			{
				Count = Step;
			}
		}
		return First;
	}

	template <typename EntryType, typename KeyProjectionType>
	static const EntryType* FindHash(
		const TArray<EntryType>& Values,
		const FAngelscriptHash256& Key,
		KeyProjectionType&& Project)
	{
		const int32 Index = LowerBoundHash(Values, Key, Project);
		return Values.IsValidIndex(Index) && Project(Values[Index]) == Key
			? &Values[Index] : nullptr;
	}

	static int32 FindModuleIndex(
		const TArray<FModuleView>& Modules,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		const int32 Index = LowerBoundHash(
			Modules, ModuleKey.Hash,
			[](const FModuleView& Module)
			{
				return Module.ModuleKey.Hash;
			});
		return Modules.IsValidIndex(Index)
			&& Modules[Index].ModuleKey == ModuleKey ? Index : INDEX_NONE;
	}

	static TOptional<EAngelscriptCacheReferenceKind> ReferenceKindForDeclaration(
		const EAngelscriptCacheDeclarationKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCacheDeclarationKind::Type:
			return EAngelscriptCacheReferenceKind::ScriptType;
		case EAngelscriptCacheDeclarationKind::Function:
			return EAngelscriptCacheReferenceKind::ScriptFunction;
		case EAngelscriptCacheDeclarationKind::Global:
			return EAngelscriptCacheReferenceKind::ScriptGlobal;
		case EAngelscriptCacheDeclarationKind::Property:
			return EAngelscriptCacheReferenceKind::ScriptProperty;
		default:
			return {};
		}
	}

	static FAngelscriptCacheDependencyPropagationResult BuildModuleViews(
		const FAngelscriptValidatedGeneration& Generation,
		TArray<FModuleView>& OutModules)
	{
		OutModules.Reset();
		OutModules.Reserve(Generation.Manifest.ModuleSnapshots.Num());
		for (const FAngelscriptCacheModuleSnapshotLink& Root :
			Generation.Manifest.ModuleSnapshots)
		{
			FModuleView& Module = OutModules.AddDefaulted_GetRef();
			Module.ModuleKey = Root.ModuleKey;
		}

		for (const FAngelscriptDecodedCacheRecordHandle& Record :
			Generation.ReachableRecords)
		{
			if (const FAngelscriptCachedModuleInterface* Interface =
				Record->TryGetModuleInterface())
			{
				const int32 ModuleIndex = FindModuleIndex(
					OutModules, Interface->ModuleKey);
				if (ModuleIndex == INDEX_NONE
					|| OutModules[ModuleIndex].Interface != nullptr)
				{
					return Failure(
						EAngelscriptCacheDependencyPropagationError::
							DuplicateSemanticAuthority,
						TEXT("ModuleInterface authority is missing from roots or duplicated"));
				}
				OutModules[ModuleIndex].Interface = Interface;
			}
			else if (const FAngelscriptCachedModuleState* State =
				Record->TryGetModuleState())
			{
				const int32 ModuleIndex = FindModuleIndex(
					OutModules, State->ModuleKey);
				if (ModuleIndex == INDEX_NONE
					|| OutModules[ModuleIndex].State != nullptr)
				{
					return Failure(
						EAngelscriptCacheDependencyPropagationError::
							DuplicateSemanticAuthority,
						TEXT("ModuleState authority is missing from roots or duplicated"));
				}
				OutModules[ModuleIndex].State = State;
			}
			else if (const FAngelscriptCachedTypeSchema* Type =
				Record->TryGetTypeSchema())
			{
				const int32 ModuleIndex = FindModuleIndex(
					OutModules, Type->ModuleKey);
				if (ModuleIndex == INDEX_NONE)
				{
					return Failure(
						EAngelscriptCacheDependencyPropagationError::
							InvalidValidatedGeneration,
						TEXT("TypeSchema owner module is absent from Manifest roots"));
				}
				OutModules[ModuleIndex].Types.Add(Type);
			}
			else if (const FAngelscriptCachedFunctionBody* Body =
				Record->TryGetFunctionBody())
			{
				const int32 ModuleIndex = FindModuleIndex(
					OutModules, Body->ModuleKey);
				if (ModuleIndex == INDEX_NONE)
				{
					return Failure(
						EAngelscriptCacheDependencyPropagationError::
							InvalidValidatedGeneration,
						TEXT("FunctionBody owner module is absent from Manifest roots"));
				}
				OutModules[ModuleIndex].FunctionBodies.Add(Body);
			}
		}

		for (FModuleView& Module : OutModules)
		{
			if (Module.Interface == nullptr || Module.State == nullptr)
			{
				return Failure(
					EAngelscriptCacheDependencyPropagationError::
						InvalidValidatedGeneration,
					TEXT("Module root lacks its Interface or ModuleState authority"));
			}
			Module.Types.Sort([](
				const FAngelscriptCachedTypeSchema& Left,
				const FAngelscriptCachedTypeSchema& Right)
			{
				return Left.TypeKey.Hash < Right.TypeKey.Hash;
			});
			Module.FunctionBodies.Sort([](
				const FAngelscriptCachedFunctionBody& Left,
				const FAngelscriptCachedFunctionBody& Right)
			{
				return Left.Identity.FunctionKey.Hash
					< Right.Identity.FunctionKey.Hash;
			});
		}
		return {};
	}

	template <typename EntryType, typename CompareType, typename SameType>
	static bool SortAndCheckUnique(
		TArray<EntryType>& Values,
		CompareType&& Compare,
		SameType&& Same)
	{
		Values.Sort(Forward<CompareType>(Compare));
		for (int32 Index = 1; Index < Values.Num(); ++Index)
		{
			if (Same(Values[Index - 1], Values[Index]))
			{
				return false;
			}
		}
		return true;
	}

	static FAngelscriptCacheDependencyPropagationResult BuildCatalog(
		const TArray<FModuleView>& Modules,
		FCatalog& OutCatalog)
	{
		OutCatalog = {};
		for (const FModuleView& Module : Modules)
		{
			OutCatalog.Modules.Add({
				Module.ModuleKey.Hash, Module.Interface});
			for (const FAngelscriptCachedDeclaration& Declaration :
				Module.Interface->Declarations)
			{
				const TOptional<EAngelscriptCacheReferenceKind> Kind =
					ReferenceKindForDeclaration(Declaration.DeclarationKind);
				if (!Kind.IsSet())
				{
					return Failure(
						EAngelscriptCacheDependencyPropagationError::
							InvalidValidatedGeneration,
						TEXT("ModuleInterface contains an unsupported declaration kind"));
				}
				OutCatalog.Declarations.Add({
					Kind.GetValue(), Declaration.StableKey, &Declaration});
			}
			for (const FAngelscriptCachedImportDeclaration& Import :
				Module.Interface->Imports)
			{
				OutCatalog.Imports.Add({Import.ImportKey.Hash, &Import});
			}
			for (const FAngelscriptCachedTypeSchema* Type : Module.Types)
			{
				OutCatalog.Types.Add({Type->TypeKey.Hash, Type});
				for (const FAngelscriptCachedPropertySchema& Property :
					Type->OrderedProperties)
				{
					OutCatalog.Properties.Add({
						Property.PropertyKey.Hash, &Property});
				}
			}
			for (const FAngelscriptCachedFunctionBody* Body :
				Module.FunctionBodies)
			{
				OutCatalog.Bodies.Add({Body->Identity.FunctionKey.Hash, Body});
			}
			for (const FAngelscriptCachedGlobalSchema& Global :
				Module.State->OrderedGlobals)
			{
				OutCatalog.Globals.Add({Global.GlobalKey.Hash, &Global});
			}
			for (const FAngelscriptCachedHardValue& HardValue :
				Module.State->HardValues)
			{
				OutCatalog.HardValues.Add({
					HardValue.Owner.Kind,
					HardValue.Owner.StableKey,
					&HardValue});
			}
			for (const FAngelscriptCachedInitializerUnit& Initializer :
				Module.State->Initializers)
			{
				OutCatalog.Initializers.Add({
					Initializer.InitializerKey.Hash, &Initializer});
			}
		}

		const auto HashLess = [](const auto& Left, const auto& Right)
		{
			return Left.StableKey < Right.StableKey;
		};
		const auto HashEqual = [](const auto& Left, const auto& Right)
		{
			return Left.StableKey == Right.StableKey;
		};
		if (!SortAndCheckUnique(OutCatalog.Modules, HashLess, HashEqual)
			|| !SortAndCheckUnique(OutCatalog.Types, HashLess, HashEqual)
			|| !SortAndCheckUnique(OutCatalog.Properties, HashLess, HashEqual)
			|| !SortAndCheckUnique(OutCatalog.Globals, HashLess, HashEqual)
			|| !SortAndCheckUnique(OutCatalog.Bodies, HashLess, HashEqual)
			|| !SortAndCheckUnique(OutCatalog.Initializers, HashLess, HashEqual)
			|| !SortAndCheckUnique(OutCatalog.Imports, HashLess, HashEqual))
		{
			return Failure(
				EAngelscriptCacheDependencyPropagationError::
					DuplicateSemanticAuthority,
				TEXT("Candidate contains duplicate stable semantic authority"));
		}

		const auto ReferenceLess = [](const auto& Left, const auto& Right)
		{
			return CompareReferenceIdentity(
				Left.Kind, Left.StableKey, Right.Kind, Right.StableKey) < 0;
		};
		const auto ReferenceEqual = [](const auto& Left, const auto& Right)
		{
			return Left.Kind == Right.Kind
				&& Left.StableKey == Right.StableKey;
		};
		if (!SortAndCheckUnique(
				OutCatalog.Declarations, ReferenceLess, ReferenceEqual)
			|| !SortAndCheckUnique(
				OutCatalog.HardValues, ReferenceLess, ReferenceEqual))
		{
			return Failure(
				EAngelscriptCacheDependencyPropagationError::
					DuplicateSemanticAuthority,
				TEXT("Candidate contains duplicate typed declaration/value authority"));
		}
		return {};
	}

	static const FDeclarationAuthority* FindDeclaration(
		const FCatalog& Catalog,
		const EAngelscriptCacheReferenceKind Kind,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Count = Catalog.Declarations.Num();
		while (Count > 0)
		{
			const int32 Step = Count / 2;
			const int32 Middle = First + Step;
			if (CompareReferenceIdentity(
					Catalog.Declarations[Middle].Kind,
					Catalog.Declarations[Middle].StableKey,
					Kind, StableKey) < 0)
			{
				First = Middle + 1;
				Count -= Step + 1;
			}
			else
			{
				Count = Step;
			}
		}
		return Catalog.Declarations.IsValidIndex(First)
			&& Catalog.Declarations[First].Kind == Kind
			&& Catalog.Declarations[First].StableKey == StableKey
			? &Catalog.Declarations[First] : nullptr;
	}

	static const FHardValueAuthority* FindHardValue(
		const FCatalog& Catalog,
		const EAngelscriptCacheReferenceKind Kind,
		const FAngelscriptHash256& StableKey)
	{
		int32 First = 0;
		int32 Count = Catalog.HardValues.Num();
		while (Count > 0)
		{
			const int32 Step = Count / 2;
			const int32 Middle = First + Step;
			if (CompareReferenceIdentity(
					Catalog.HardValues[Middle].Kind,
					Catalog.HardValues[Middle].StableKey,
					Kind, StableKey) < 0)
			{
				First = Middle + 1;
				Count -= Step + 1;
			}
			else
			{
				Count = Step;
			}
		}
		return Catalog.HardValues.IsValidIndex(First)
			&& Catalog.HardValues[First].Kind == Kind
			&& Catalog.HardValues[First].StableKey == StableKey
			? &Catalog.HardValues[First] : nullptr;
	}

	class FAuthorityResolver
	{
	public:
		FAuthorityResolver(
			const FCatalog& InCatalog,
			const IAngelscriptCacheCurrentSymbolResolver* InExternal)
			: Catalog(InCatalog)
			, External(InExternal)
		{
		}

		FResolvedAuthority Resolve(
			const FAngelscriptCacheSemanticDependency& Dependency)
		{
			return ResolveInternal(Dependency, 0);
		}

	private:
		FResolvedAuthority ResolveLocal(
			const FAngelscriptCacheSemanticDependency& Dependency) const
		{
			FResolvedAuthority Result;
			const EAngelscriptCacheReferenceKind Kind = Dependency.Target.Kind;
			const FAngelscriptHash256& Key = Dependency.Target.StableKey;
			if (Kind == EAngelscriptCacheReferenceKind::CanonicalName
				|| Kind == EAngelscriptCacheReferenceKind::StringLiteral)
			{
				Result.bFound = true;
				return Result;
			}
			if (Kind == EAngelscriptCacheReferenceKind::ScriptModule)
			{
				const FModuleAuthority* Module = FindHash(
					Catalog.Modules, Key,
					[](const FModuleAuthority& Value)
					{
						return Value.StableKey;
					});
				if (Module != nullptr)
				{
					Result.bFound = true;
					Result.Abi = Module->Interface->InterfaceAbi;
				}
				return Result;
			}

			const FDeclarationAuthority* Declaration = FindDeclaration(
				Catalog, Kind, Key);
			if (Declaration == nullptr)
			{
				return Result;
			}
			Result.bFound = true;
			Result.Abi = Declaration->Declaration->SignatureHash;

			if (Kind == EAngelscriptCacheReferenceKind::ScriptType)
			{
				if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::ValueLayout)
				{
					const FTypeAuthority* Type = FindHash(
						Catalog.Types, Key,
						[](const FTypeAuthority& Value)
						{
							return Value.StableKey;
						});
					if (Type == nullptr)
					{
						return {};
					}
					Result.ContentOrValue = Type->Type->Layout.TypeLayoutHash;
				}
				if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::HardValue)
				{
					if (const FHardValueAuthority* HardValue = FindHardValue(
						Catalog, Kind, Key))
					{
						Result.ContentOrValue =
							HardValue->HardValue->HardValueHash;
					}
				}
				return Result;
			}

			if (Kind == EAngelscriptCacheReferenceKind::ScriptProperty)
			{
				if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::PropertyLayout)
				{
					const FPropertyAuthority* Property = FindHash(
						Catalog.Properties, Key,
						[](const FPropertyAuthority& Value)
						{
							return Value.StableKey;
						});
					if (Property == nullptr)
					{
						return {};
					}
					Result.ContentOrValue =
						Property->Property->PropertyLayoutFingerprint;
				}
				return Result;
			}

			if (Kind == EAngelscriptCacheReferenceKind::ScriptGlobal)
			{
				if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::GlobalStorage)
				{
					if (const FGlobalAuthority* Global = FindHash(
						Catalog.Globals, Key,
						[](const FGlobalAuthority& Value)
						{
							return Value.StableKey;
						}))
					{
						Result.ContentOrValue =
							Global->Global->StorageLayoutFingerprint;
					}
				}
				else if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::HardValue)
				{
					if (const FHardValueAuthority* HardValue = FindHardValue(
						Catalog, Kind, Key))
					{
						Result.ContentOrValue =
							HardValue->HardValue->HardValueHash;
					}
				}
				return Result;
			}

			if (Kind == EAngelscriptCacheReferenceKind::ScriptFunction)
			{
				if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::Initializer)
				{
					if (const FInitializerAuthority* Initializer = FindHash(
						Catalog.Initializers, Key,
						[](const FInitializerAuthority& Value)
						{
							return Value.StableKey;
						}))
					{
						Result.ContentOrValue = Initializer->Initializer->
							InitializerExecutionHash;
					}
				}
				else if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::FunctionContent)
				{
					if (const FBodyAuthority* Body = FindHash(
						Catalog.Bodies, Key,
						[](const FBodyAuthority& Value)
						{
							return Value.StableKey;
						}))
					{
						Result.ContentOrValue =
							Body->Body->Identity.Content.Execution;
					}
				}
			}
			return Result;
		}

		FResolvedAuthority ResolveExternal(
			const EAngelscriptCacheReferenceKind Kind,
			const FAngelscriptHash256& StableKey)
		{
			int32 First = 0;
			int32 Count = ExternalMemos.Num();
			while (Count > 0)
			{
				const int32 Step = Count / 2;
				const int32 Middle = First + Step;
				if (CompareReferenceIdentity(
						ExternalMemos[Middle].Kind,
						ExternalMemos[Middle].StableKey,
						Kind, StableKey) < 0)
				{
					First = Middle + 1;
					Count -= Step + 1;
				}
				else
				{
					Count = Step;
				}
			}
			if (!ExternalMemos.IsValidIndex(First)
				|| ExternalMemos[First].Kind != Kind
				|| ExternalMemos[First].StableKey != StableKey)
			{
				FExternalMemo Memo;
				Memo.Kind = Kind;
				Memo.StableKey = StableKey;
				if (External != nullptr)
				{
					const TOptional<FAngelscriptCacheCurrentSymbol> Current =
						External->Resolve(Kind, StableKey);
					if (Current.IsSet())
					{
						Memo.bFound = true;
						Memo.Symbol = Current.GetValue();
					}
				}
				ExternalMemos.Insert(MoveTemp(Memo), First);
			}
			const FExternalMemo& Memo = ExternalMemos[First];
			FResolvedAuthority Result;
			Result.bFound = Memo.bFound;
			if (Memo.bFound)
			{
				Result.Abi = Memo.Symbol.CurrentAbi;
				Result.ContentOrValue = Memo.Symbol.CurrentContentOrValue;
			}
			return Result;
		}

		FResolvedAuthority ResolveInternal(
			const FAngelscriptCacheSemanticDependency& Dependency,
			const uint32 Depth)
		{
			if (Depth > 4)
			{
				return {};
			}
			if (Dependency.Target.Kind
				== EAngelscriptCacheReferenceKind::ScriptImport)
			{
				const FImportAuthority* Import = FindHash(
					Catalog.Imports, Dependency.Target.StableKey,
					[](const FImportAuthority& Value)
					{
						return Value.StableKey;
					});
				if (Import != nullptr)
				{
					FAngelscriptCacheSemanticDependency TargetDependency;
					TargetDependency.Kind =
						EAngelscriptCacheSemanticDependencyKind::Import;
					TargetDependency.Target = Import->Import->TargetDeclaration;
					return ResolveInternal(TargetDependency, Depth + 1);
				}
			}

			FResolvedAuthority Result = ResolveLocal(Dependency);
			if (Result.bFound)
			{
				return Result;
			}
			return ResolveExternal(
				Dependency.Target.Kind, Dependency.Target.StableKey);
		}

		const FCatalog& Catalog;
		const IAngelscriptCacheCurrentSymbolResolver* External = nullptr;
		TArray<FExternalMemo> ExternalMemos;
	};

	static bool MissLess(
		const FAngelscriptCacheDependencyMiss& Left,
		const FAngelscriptCacheDependencyMiss& Right)
	{
		if (Left.OwnerRecordKind != Right.OwnerRecordKind)
		{
			return static_cast<uint8>(Left.OwnerRecordKind)
				< static_cast<uint8>(Right.OwnerRecordKind);
		}
		if (const int32 Owner = CompareHash(
			Left.OwnerStableKey, Right.OwnerStableKey); Owner != 0)
		{
			return Owner < 0;
		}
		if (Left.OwnerOrdinal != Right.OwnerOrdinal)
		{
			return Left.OwnerOrdinal < Right.OwnerOrdinal;
		}
		if (Left.DependencyOrdinal != Right.DependencyOrdinal)
		{
			return Left.DependencyOrdinal < Right.DependencyOrdinal;
		}
		if (Left.DependencyKind != Right.DependencyKind)
		{
			return static_cast<uint8>(Left.DependencyKind)
				< static_cast<uint8>(Right.DependencyKind);
		}
		if (Left.TargetKind != Right.TargetKind)
		{
			return static_cast<uint8>(Left.TargetKind)
				< static_cast<uint8>(Right.TargetKind);
		}
		if (const int32 Target = CompareHash(
			Left.TargetStableKey, Right.TargetStableKey); Target != 0)
		{
			return Target < 0;
		}
		return static_cast<uint8>(Left.Reason)
			< static_cast<uint8>(Right.Reason);
	}

	static void EvaluateDependency(
		FAuthorityResolver& Resolver,
		const EAngelscriptCacheRecordKind OwnerRecordKind,
		const FAngelscriptHash256& OwnerStableKey,
		const uint32 OwnerOrdinal,
		const uint32 DependencyOrdinal,
		const FAngelscriptCacheSemanticDependency& Dependency,
		TArray<FAngelscriptCacheDependencyMiss>& OutReasons)
	{
		if (Dependency.Target.Kind
				== EAngelscriptCacheReferenceKind::CanonicalName
			|| Dependency.Target.Kind
				== EAngelscriptCacheReferenceKind::StringLiteral)
		{
			return;
		}

		const FResolvedAuthority Current = Resolver.Resolve(Dependency);
		TOptional<EAngelscriptCacheDependencyMissReason> Reason;
		if (!Current.bFound)
		{
			Reason = EAngelscriptCacheDependencyMissReason::TargetUnresolved;
		}
		else if (!(Current.Abi == Dependency.Target.ExpectedAbi))
		{
			Reason = EAngelscriptCacheDependencyMissReason::AbiMismatch;
		}
		else if (Dependency.ExpectedContentOrValue.IsSet()
			&& !Current.ContentOrValue.IsSet())
		{
			Reason = EAngelscriptCacheDependencyMissReason::ContentUnavailable;
		}
		else if (Dependency.ExpectedContentOrValue.IsSet()
			&& !(Dependency.ExpectedContentOrValue.GetValue()
				== Current.ContentOrValue.GetValue()))
		{
			Reason = EAngelscriptCacheDependencyMissReason::ContentMismatch;
		}
		if (!Reason.IsSet())
		{
			return;
		}

		FAngelscriptCacheDependencyMiss& Miss =
			OutReasons.AddDefaulted_GetRef();
		Miss.Reason = Reason.GetValue();
		Miss.OwnerRecordKind = OwnerRecordKind;
		Miss.OwnerStableKey = OwnerStableKey;
		Miss.OwnerOrdinal = OwnerOrdinal;
		Miss.DependencyOrdinal = DependencyOrdinal;
		Miss.DependencyKind = Dependency.Kind;
		Miss.TargetKind = Dependency.Target.Kind;
		Miss.TargetStableKey = Dependency.Target.StableKey;
		Miss.ExpectedAbi = Dependency.Target.ExpectedAbi;
		Miss.ExpectedContentOrValue = Dependency.ExpectedContentOrValue;
		if (Current.bFound)
		{
			Miss.CurrentAbi = Current.Abi;
			Miss.CurrentContentOrValue = Current.ContentOrValue;
		}
	}

	static void EvaluateModule(
		const FModuleView& Module,
		FAuthorityResolver& Resolver,
		FAngelscriptCacheDependentModule& OutModule)
	{
		OutModule.ModuleKey = Module.ModuleKey;
		for (int32 DependencyOrdinal = 0;
			DependencyOrdinal < Module.Interface->Dependencies.Num();
			++DependencyOrdinal)
		{
			EvaluateDependency(
				Resolver,
				EAngelscriptCacheRecordKind::ModuleInterface,
				Module.ModuleKey.Hash,
				0,
				static_cast<uint32>(DependencyOrdinal),
				Module.Interface->Dependencies[DependencyOrdinal],
				OutModule.Reasons);
		}
		for (int32 ImportOrdinal = 0;
			ImportOrdinal < Module.Interface->Imports.Num(); ++ImportOrdinal)
		{
			const FAngelscriptCachedImportDeclaration& Import =
				Module.Interface->Imports[ImportOrdinal];
			FAngelscriptCacheSemanticDependency Dependency;
			Dependency.Kind = EAngelscriptCacheSemanticDependencyKind::Import;
			Dependency.Target.Kind = EAngelscriptCacheReferenceKind::ScriptImport;
			Dependency.Target.StableKey = Import.ImportKey.Hash;
			Dependency.Target.ExpectedAbi =
				Import.TargetDeclaration.ExpectedAbi;
			EvaluateDependency(
				Resolver,
				EAngelscriptCacheRecordKind::ModuleInterface,
				Import.ImportKey.Hash,
				static_cast<uint32>(ImportOrdinal),
				0,
				Dependency,
				OutModule.Reasons);
		}

		for (int32 TypeOrdinal = 0; TypeOrdinal < Module.Types.Num(); ++TypeOrdinal)
		{
			const FAngelscriptCachedTypeSchema& Type = *Module.Types[TypeOrdinal];
			for (int32 DependencyOrdinal = 0;
				DependencyOrdinal < Type.Dependencies.Num(); ++DependencyOrdinal)
			{
				EvaluateDependency(
					Resolver,
					EAngelscriptCacheRecordKind::TypeSchema,
					Type.TypeKey.Hash,
					static_cast<uint32>(TypeOrdinal),
					static_cast<uint32>(DependencyOrdinal),
					Type.Dependencies[DependencyOrdinal],
					OutModule.Reasons);
			}
		}

		for (int32 DependencyOrdinal = 0;
			DependencyOrdinal < Module.State->Dependencies.Num();
			++DependencyOrdinal)
		{
			EvaluateDependency(
				Resolver,
				EAngelscriptCacheRecordKind::ModuleState,
				Module.ModuleKey.Hash,
				0,
				static_cast<uint32>(DependencyOrdinal),
				Module.State->Dependencies[DependencyOrdinal],
				OutModule.Reasons);
		}
		for (int32 ActionOrdinal = 0;
			ActionOrdinal < Module.State->OrderedInitializationActions.Num();
			++ActionOrdinal)
		{
			const FAngelscriptCachedInitializationAction& Action =
				Module.State->OrderedInitializationActions[ActionOrdinal];
			for (int32 DependencyOrdinal = 0;
				DependencyOrdinal < Action.Dependencies.Num(); ++DependencyOrdinal)
			{
				EvaluateDependency(
					Resolver,
					EAngelscriptCacheRecordKind::ModuleState,
					Action.Target.StableKey,
					static_cast<uint32>(ActionOrdinal + 1),
					static_cast<uint32>(DependencyOrdinal),
					Action.Dependencies[DependencyOrdinal],
					OutModule.Reasons);
			}
		}

		for (int32 BodyOrdinal = 0;
			BodyOrdinal < Module.FunctionBodies.Num(); ++BodyOrdinal)
		{
			const FAngelscriptCachedFunctionBody& Body =
				*Module.FunctionBodies[BodyOrdinal];
			for (int32 DependencyOrdinal = 0;
				DependencyOrdinal < Body.ActualDependencies.Num();
				++DependencyOrdinal)
			{
				EvaluateDependency(
					Resolver,
					EAngelscriptCacheRecordKind::FunctionBody,
					Body.Identity.FunctionKey.Hash,
					static_cast<uint32>(BodyOrdinal),
					static_cast<uint32>(DependencyOrdinal),
					Body.ActualDependencies[DependencyOrdinal],
					OutModule.Reasons);
			}
		}
		OutModule.Reasons.Sort(MissLess);
	}
}

FAngelscriptCacheDependencyPropagationResult
PlanAngelscriptCacheDependentRecompileWave(
	const FAngelscriptValidatedGeneration& CandidateGeneration,
	const FAngelscriptCacheReadLimits& Limits,
	const IAngelscriptCacheCurrentSymbolResolver* ExternalCurrentSymbols,
	FAngelscriptCacheDependentRecompileWave& OutWave)
{
	using namespace AngelscriptCacheDependencyPropagation_Private;
	OutWave.Reset();

	const FAngelscriptCacheValidationResult ManifestValidation =
		ValidateAngelscriptCacheGenerationManifestValue(
			CandidateGeneration.Manifest, Limits);
	if (!ManifestValidation.IsSuccess())
	{
		FAngelscriptCacheDependencyPropagationResult Result = Failure(
			EAngelscriptCacheDependencyPropagationError::InvalidManifest,
			FString::Printf(
				TEXT("Candidate Manifest validation failed: Error=%u Stage=%u Offset=%llu"),
				static_cast<uint32>(ManifestValidation.Error),
				static_cast<uint32>(ManifestValidation.Stage),
				static_cast<unsigned long long>(ManifestValidation.ByteOffset)));
		Result.Validation = ManifestValidation;
		return Result;
	}

	const FAngelscriptCacheSemanticDiffResult SelfDiff =
		DiffAngelscriptCacheValidatedGenerations(
			CandidateGeneration, CandidateGeneration);
	if (!SelfDiff.IsSuccess())
	{
		FAngelscriptCacheDependencyPropagationResult Result = Failure(
			EAngelscriptCacheDependencyPropagationError::
				InvalidValidatedGeneration,
			FString::Printf(
				TEXT("Candidate semantic self-validation failed: Error=%u Detail=%s"),
				static_cast<uint32>(SelfDiff.Error), *SelfDiff.Detail));
		Result.SemanticDiffError = SelfDiff.Error;
		return Result;
	}

	TArray<FModuleView> Modules;
	FAngelscriptCacheDependencyPropagationResult Result =
		BuildModuleViews(CandidateGeneration, Modules);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FCatalog Catalog;
	Result = BuildCatalog(Modules, Catalog);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	FAuthorityResolver Resolver(Catalog, ExternalCurrentSymbols);
	FAngelscriptCacheDependentRecompileWave Candidate;
	Candidate.Modules.Reserve(Modules.Num());
	int32 ReasonCount = 0;
	for (const FModuleView& Module : Modules)
	{
		FAngelscriptCacheDependentModule Planned;
		EvaluateModule(Module, Resolver, Planned);
		if (!Planned.Reasons.IsEmpty())
		{
			ReasonCount += Planned.Reasons.Num();
			Candidate.Modules.Add(MoveTemp(Planned));
		}
	}

	OutWave = MoveTemp(Candidate);
	Result.Detail = FString::Printf(
		TEXT("Planned %d dependent module(s) from %d typed miss reason(s)"),
		OutWave.Modules.Num(), ReasonCount);
	return Result;
}
