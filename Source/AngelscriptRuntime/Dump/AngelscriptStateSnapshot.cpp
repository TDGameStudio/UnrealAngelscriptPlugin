#include "Dump/AngelscriptStateSnapshot.h"

#include "Core/AngelscriptEngine.h"
#if WITH_AS_COVERAGE
#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"
#endif

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typeinfo.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	FString SnapshotBoolToString(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString PointerToString(const void* Pointer)
	{
		return Pointer != nullptr ? FString::Printf(TEXT("0x%p"), Pointer) : TEXT("null");
	}

	FString CharToString(const char* Value)
	{
		return Value != nullptr ? UTF8_TO_TCHAR(Value) : FString();
	}

	FString NameSpaceToString(const asSNameSpace* NameSpace)
	{
		return NameSpace != nullptr ? CharToString(NameSpace->name.AddressOf()) : FString();
	}

	FString SourceKindToString(const EAngelscriptSourceKind SourceKind)
	{
		switch (SourceKind)
		{
		case EAngelscriptSourceKind::Game:
			return TEXT("Game");
		case EAngelscriptSourceKind::Plugin:
			return TEXT("Plugin");
		case EAngelscriptSourceKind::Memory:
			return TEXT("Memory");
		case EAngelscriptSourceKind::Unknown:
		default:
			return TEXT("Unknown");
		}
	}

	FString TypeIdentity(const asCTypeInfo* Type)
	{
		if (Type == nullptr)
		{
			return TEXT("<null-type>");
		}

		return FString::Printf(TEXT("TypeId%d::%s"), Type->GetTypeId(), *CharToString(Type->GetName()));
	}

	FString FunctionIdentity(const asCScriptFunction* Function)
	{
		if (Function == nullptr)
		{
			return TEXT("<null-function>");
		}

		const FString ModuleName = Function->GetModuleName() != nullptr ? CharToString(Function->GetModuleName()) : TEXT("<engine>");
		return FString::Printf(TEXT("%s::%s"), *ModuleName, *CharToString(Function->GetName()));
	}

	void AddCountRow(FAngelscriptStateSnapshot& Snapshot, const FString& Category, const FString& Identity, const FString& Field, const int32 Count, const FString& Source)
	{
		Snapshot.AddRow(Category, Identity, Field, LexToString(Count), TEXT("Count"), Source);
	}

	void AddBoolRow(FAngelscriptStateSnapshot& Snapshot, const FString& Category, const FString& Identity, const FString& Field, const bool bValue, const FString& Source)
	{
		Snapshot.AddRow(Category, Identity, Field, SnapshotBoolToString(bValue), TEXT("Boolean"), Source);
	}

	void AddPointerRow(FAngelscriptStateSnapshot& Snapshot, const FString& Category, const FString& Identity, const FString& Field, const void* Pointer, const FString& Source)
	{
		Snapshot.AddRow(Category, Identity, Field, PointerToString(Pointer), TEXT("Pointer"), Source);
	}

	FString FilenamePairIdentity(const FAngelscriptEngine::FFilenamePair& FilenamePair)
	{
		if (!FilenamePair.VirtualPath.IsEmpty())
		{
			return FilenamePair.VirtualPath;
		}
		if (!FilenamePair.RelativePath.IsEmpty())
		{
			return FilenamePair.RelativePath;
		}
		return FilenamePair.AbsolutePath;
	}

	FString ModuleDescIdentity(const TSharedPtr<FAngelscriptModuleDesc>& Module)
	{
		return Module.IsValid() ? Module->ModuleName : TEXT("<null-module-desc>");
	}

	void AddFilenamePairRows(
		FAngelscriptStateSnapshot& Snapshot,
		const FString& CollectionName,
		const FAngelscriptEngine::FFilenamePair& FilenamePair,
		const FString& Source)
	{
		const FString Identity = FString::Printf(TEXT("%s/%s"), *CollectionName, *FilenamePairIdentity(FilenamePair));
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("AbsolutePath"), FilenamePair.AbsolutePath, TEXT("String"), Source);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("RelativePath"), FilenamePair.RelativePath, TEXT("String"), Source);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("VirtualPath"), FilenamePair.VirtualPath, TEXT("String"), Source);
	}

	template <typename ElementType>
	int32 AsArrayCount(const asCArray<ElementType>& Array)
	{
		return static_cast<int32>(Array.GetLength());
	}

	template <typename SymbolMapType>
	int32 CountAsMapEntries(const SymbolMapType& Map)
	{
		int32 Count = 0;
		Map.IterateAll(
			[&Count](auto)
			{
				++Count;
			});
		return Count;
	}

	void AddTypeRows(FAngelscriptStateSnapshot& Snapshot, const asCTypeInfo* Type, const FString& Source)
	{
		if (Type == nullptr)
		{
			return;
		}

		const FString Identity = TypeIdentity(Type);
		Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Name"), CharToString(Type->GetName()), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Namespace"), NameSpaceToString(Type->nameSpace), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("TypeId"), LexToString(Type->GetTypeId()), TEXT("Integer"), Source);
		Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Flags"), LexToString(Type->GetFlags()), TEXT("Integer"), Source);
		Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Size"), LexToString(Type->GetSize()), TEXT("Integer"), Source);
		AddPointerRow(Snapshot, TEXT("AsTypeInternal"), Identity, TEXT("Pointer"), Type, Source);

		if (const asCObjectType* ObjectType = CastToObjectType(const_cast<asCTypeInfo*>(Type)))
		{
			Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Kind"), TEXT("ObjectType"), TEXT("String"), Source);
			AddCountRow(Snapshot, TEXT("AsTypeInternal"), Identity, TEXT("MethodCount"), AsArrayCount(ObjectType->methods), Source);
			AddCountRow(Snapshot, TEXT("AsTypeInternal"), Identity, TEXT("PropertyCount"), AsArrayCount(ObjectType->properties), Source);
			AddCountRow(Snapshot, TEXT("AsTypeInternal"), Identity, TEXT("InterfaceCount"), AsArrayCount(ObjectType->interfaces), Source);
			AddBoolRow(Snapshot, TEXT("AsTypeInternal"), Identity, TEXT("bIsInterface"), ObjectType->IsInterface(), Source);
		}
		else if (CastToEnumType(const_cast<asCTypeInfo*>(Type)) != nullptr)
		{
			Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Kind"), TEXT("EnumType"), TEXT("String"), Source);
			AddCountRow(Snapshot, TEXT("AsTypeInternal"), Identity, TEXT("EnumValueCount"), Type->GetEnumValueCount(), Source);
		}
		else if (CastToTypedefType(const_cast<asCTypeInfo*>(Type)) != nullptr)
		{
			Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Kind"), TEXT("TypedefType"), TEXT("String"), Source);
		}
		else if (CastToFuncdefType(const_cast<asCTypeInfo*>(Type)) != nullptr)
		{
			Snapshot.AddRow(TEXT("AsTypeInternal"), Identity, TEXT("Kind"), TEXT("FuncdefType"), TEXT("String"), Source);
		}
	}

	void AddFunctionRows(FAngelscriptStateSnapshot& Snapshot, const asCScriptFunction* Function, const FString& Source)
	{
		if (Function == nullptr)
		{
			return;
		}

		const FString Identity = FunctionIdentity(Function);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("Name"), CharToString(Function->GetName()), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("Declaration"), CharToString(Function->GetDeclaration(true, true, true)), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("Id"), LexToString(Function->GetId()), TEXT("Integer"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("FuncType"), LexToString(static_cast<int32>(Function->GetFuncType())), TEXT("Integer"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("ParamCount"), LexToString(Function->GetParamCount()), TEXT("Count"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("ReturnType"), CharToString(Function->returnType.Format(Function->nameSpace, true).AddressOf()), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("Namespace"), NameSpaceToString(Function->nameSpace), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("AccessMask"), LexToString(Function->accessMask), TEXT("Integer"), Source);
		Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("SignatureId"), LexToString(Function->signatureId), TEXT("Integer"), Source);
		AddPointerRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("ObjectType"), Function->objectType, Source);
		AddPointerRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("Pointer"), Function, Source);
		AddBoolRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("bIsReadOnly"), Function->IsReadOnly(), Source);
		AddBoolRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("bIsPrivate"), Function->IsPrivate(), Source);
		AddBoolRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("bIsProtected"), Function->IsProtected(), Source);
		AddBoolRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("bIsMixin"), Function->IsMixin(), Source);
		AddBoolRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("bHasJITFunction"), Function->jitFunction != nullptr, Source);
		AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("DefaultArgCount"), AsArrayCount(Function->defaultArgs), Source);
		AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("ParameterTypeCount"), AsArrayCount(Function->parameterTypes), Source);
		AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("TemplateSubTypeCount"), AsArrayCount(Function->templateSubTypes), Source);
		AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("ParameterOffsetCount"), AsArrayCount(Function->parameterOffsets), Source);
		if (Function->scriptData != nullptr)
		{
			AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("ByteCodeDwordCount"), AsArrayCount(Function->scriptData->byteCode), Source);
			AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("LocalVariableCount"), AsArrayCount(Function->scriptData->variables), Source);
			AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("LineNumberEntryCount"), AsArrayCount(Function->scriptData->lineNumbers), Source);
			AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("ObjectVariableTypeCount"), AsArrayCount(Function->scriptData->objVariableTypes), Source);
			AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("TryCatchInfoCount"), AsArrayCount(Function->scriptData->tryCatchInfo), Source);
			AddCountRow(Snapshot, TEXT("AsFunctionInternal"), Identity, TEXT("SectionIndexEntryCount"), AsArrayCount(Function->scriptData->sectionIdxs), Source);
			Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("StackNeeded"), LexToString(Function->scriptData->stackNeeded), TEXT("Integer"), Source);
			Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("VariableSpace"), LexToString(Function->scriptData->variableSpace), TEXT("Integer"), Source);
			Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("ScriptSectionIndex"), LexToString(Function->scriptData->scriptSectionIdx), TEXT("Integer"), Source);
			Snapshot.AddRow(TEXT("AsFunctionInternal"), Identity, TEXT("DeclaredAt"), LexToString(Function->scriptData->declaredAt), TEXT("Integer"), Source);
		}
	}

	void AddModuleRows(FAngelscriptStateSnapshot& Snapshot, const asCModule* Module)
	{
		if (Module == nullptr)
		{
			return;
		}

		const FString ModuleName = CharToString(Module->GetName());
		const FString Source = TEXT("asCModule");
		Snapshot.AddRow(TEXT("AsModuleInternal"), ModuleName, TEXT("Name"), ModuleName, TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsModuleInternal"), ModuleName, TEXT("BaseModuleName"), CharToString(Module->baseModuleName.AddressOf()), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsModuleInternal"), ModuleName, TEXT("AccessMask"), LexToString(Module->accessMask), TEXT("Integer"), Source);
		Snapshot.AddRow(TEXT("AsModuleInternal"), ModuleName, TEXT("DefaultNamespace"), NameSpaceToString(Module->defaultNamespace), TEXT("String"), Source);
		AddPointerRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("Pointer"), Module, Source);
		AddPointerRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("Engine"), Module->engine, Source);
		AddPointerRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("Builder"), Module->builder, Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("UserDataCount"), AsArrayCount(Module->userData), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ScriptFunctionCount"), AsArrayCount(Module->scriptFunctions), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("GlobalFunctionCount"), AsArrayCount(Module->globalFunctionList), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("BindInformationCount"), AsArrayCount(Module->bindInformations), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("TemplateInstanceCount"), Module->templateInstances.Num(), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ScriptGlobalCount"), AsArrayCount(Module->scriptGlobalsList), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ImportedModuleCount"), AsArrayCount(Module->importedModules), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ModuleDependencyCount"), Module->moduleDependencies.Num(), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("LocalTypeCount"), CountAsMapEntries(Module->allLocalTypes), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ClassTypeCount"), AsArrayCount(Module->classTypes), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("EnumTypeCount"), AsArrayCount(Module->enumTypes), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("TypedefCount"), AsArrayCount(Module->typeDefs), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("FuncdefCount"), AsArrayCount(Module->funcDefs), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ExternalTypeCount"), AsArrayCount(Module->externalTypes), Source);
		AddCountRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ExternalFunctionCount"), AsArrayCount(Module->externalFunctions), Source);
		AddBoolRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("bIsGlobalVarInitialized"), Module->isGlobalVarInitialized, Source);
		AddBoolRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("bHasBreakPoints"), Module->hasBreakPoints, Source);
		AddBoolRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("bDiscarded"), Module->discarded, Source);
		Snapshot.AddRow(TEXT("AsModuleInternal"), ModuleName, TEXT("ReloadState"), LexToString(static_cast<int32>(Module->ReloadState)), TEXT("Integer"), Source);
		AddPointerRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ReloadOldModule"), Module->ReloadOldModule, Source);
		AddPointerRow(Snapshot, TEXT("AsModuleInternal"), ModuleName, TEXT("ReloadNewModule"), Module->ReloadNewModule, Source);

		for (asUINT DependencyIndex = 0; DependencyIndex < Module->importedModules.GetLength(); ++DependencyIndex)
		{
			const asCModule* ImportedModule = Module->importedModules[DependencyIndex];
			const FString DependencyIdentity = FString::Printf(TEXT("%s/ImportedModule/%s"), *ModuleName, *CharToString(ImportedModule != nullptr ? ImportedModule->GetName() : nullptr));
			Snapshot.AddRow(TEXT("AsModuleInternal"), DependencyIdentity, TEXT("Name"), ImportedModule != nullptr ? CharToString(ImportedModule->GetName()) : TEXT("<null-module>"), TEXT("String"), Source);
		}

		for (const TPair<asCModule*, asCModule::FModuleDependencyInfo>& Pair : Module->moduleDependencies)
		{
			const asCModule* DependencyModule = Pair.Key;
			const FString DependencyName = DependencyModule != nullptr ? CharToString(DependencyModule->GetName()) : TEXT("<null-module>");
			const FString DependencyIdentity = FString::Printf(TEXT("%s/ModuleDependency/%s"), *ModuleName, *DependencyName);
			Snapshot.AddRow(TEXT("AsModuleInternal"), DependencyIdentity, TEXT("FirstLineNumber"), LexToString(Pair.Value.FirstLineNumber), TEXT("Integer"), Source);
			Snapshot.AddRow(TEXT("AsModuleInternal"), DependencyIdentity, TEXT("FirstColumn"), LexToString(Pair.Value.FirstColumn), TEXT("Integer"), Source);
			AddBoolRow(Snapshot, TEXT("AsModuleInternal"), DependencyIdentity, TEXT("bIsHardValueDependency"), Pair.Value.bIsHardValueDependency, Source);
			AddBoolRow(Snapshot, TEXT("AsModuleInternal"), DependencyIdentity, TEXT("bIsStructuralDependency"), Pair.Value.bIsStructuralDependency, Source);
		}

		for (asUINT FunctionIndex = 0; FunctionIndex < Module->scriptFunctions.GetLength(); ++FunctionIndex)
		{
			AddFunctionRows(Snapshot, Module->scriptFunctions[FunctionIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < Module->classTypes.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, Module->classTypes[TypeIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < Module->enumTypes.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, Module->enumTypes[TypeIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < Module->typeDefs.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, Module->typeDefs[TypeIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < Module->funcDefs.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, Module->funcDefs[TypeIndex], Source);
		}
	}
}

FString FAngelscriptStateSnapshotRow::MakeKey() const
{
	if (!SortKey.IsEmpty())
	{
		return SortKey;
	}

	return FString::Printf(TEXT("%s\x1f%s\x1f%s"), *Category, *Identity, *Field);
}

void FAngelscriptStateSnapshot::AddRow(
	const FString& Category,
	const FString& Identity,
	const FString& Field,
	const FString& Value,
	const FString& ValueKind,
	const FString& Source)
{
	const FString Key = FString::Printf(TEXT("%s\x1f%s\x1f%s"), *Category, *Identity, *Field);
	if (RowKeys.Contains(Key))
	{
		return;
	}

	RowKeys.Add(Key);

	FAngelscriptStateSnapshotRow& Row = Rows.AddDefaulted_GetRef();
	Row.Category = Category;
	Row.Identity = Identity;
	Row.Field = Field;
	Row.Value = Value;
	Row.ValueKind = ValueKind;
	Row.Source = Source;
	Row.SortKey = Key;
}

void FAngelscriptStateSnapshot::SortRows()
{
	Rows.Sort(
		[](const FAngelscriptStateSnapshotRow& Left, const FAngelscriptStateSnapshotRow& Right)
		{
			return Left.MakeKey() < Right.MakeKey();
		});
}

bool FAngelscriptStateSnapshot::AreRowsSorted() const
{
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		if (Rows[RowIndex - 1].MakeKey() > Rows[RowIndex].MakeKey())
		{
			return false;
		}
	}
	return true;
}

bool FAngelscriptStateSnapshot::HasDuplicateKeys() const
{
	TSet<FString> SeenKeys;
	for (const FAngelscriptStateSnapshotRow& Row : Rows)
	{
		const FString Key = Row.MakeKey();
		if (SeenKeys.Contains(Key))
		{
			return true;
		}
		SeenKeys.Add(Key);
	}
	return false;
}

FAngelscriptStateSnapshot FAngelscriptStateSnapshotBuilder::Capture(FAngelscriptEngine& Engine)
{
	FAngelscriptStateSnapshot Snapshot;
	const FString EngineSource = TEXT("FAngelscriptEngine");

	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Lifecycle"), TEXT("bIsInitialCompileFinished"), Engine.bIsInitialCompileFinished, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Lifecycle"), TEXT("bDidInitialCompileSucceed"), Engine.bDidInitialCompileSucceed, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Lifecycle"), TEXT("bCompletedAssetScan"), Engine.bCompletedAssetScan, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bSimulateCooked"), Engine.bSimulateCooked, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bTestErrors"), Engine.bTestErrors, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bIsHotReloading"), Engine.bIsHotReloading, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bUseEditorScripts"), Engine.bUseEditorScripts, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bUseAutomaticImportMethod"), Engine.bUseAutomaticImportMethod, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("StaticJITCompatibility"), TEXT("bCollectStaticJITCompatibilityBinds"), Engine.bCollectStaticJITCompatibilityBinds, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("StaticJITCompatibility"), TEXT("bUseStaticJITCompatibilityData"), Engine.bUseStaticJITCompatibilityData, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bScriptDevelopmentMode"), Engine.bScriptDevelopmentMode, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Mode"), TEXT("bHotReloadThreadStarted"), Engine.bHotReloadThreadStarted, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Diagnostics"), TEXT("bDiagnosticsDirty"), Engine.bDiagnosticsDirty, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("Diagnostics"), TEXT("bIgnoreCompileErrorDiagnostics"), Engine.bIgnoreCompileErrorDiagnostics, EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Packages"), TEXT("AngelscriptPackage"), Engine.AngelscriptPackage, EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Packages"), TEXT("AssetsPackage"), Engine.AssetsPackage, EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("World"), TEXT("WorldContextObject"), Engine.GetCurrentWorldContextObject(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Core"), TEXT("ScriptEngine"), Engine.GetScriptEngine(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("TypeDatabase"), Engine.GetTypeDatabase(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("BindState"), Engine.GetBindState(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("ToStringList"), Engine.GetToStringList(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("BindDatabase"), Engine.GetBindDatabase(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("BlueprintEventSignatureRegistry"), Engine.GetBlueprintEventSignatureRegistry(), EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("PrecompiledData"), Engine.PrecompiledData, EngineSource);
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("StaticJIT"), Engine.StaticJIT, EngineSource);
#if WITH_AS_COVERAGE
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("CodeCoverage"), FAngelscriptCodeCoverageExtension::GetForEngine(Engine), EngineSource);
#else
	Snapshot.AddRow(TEXT("EngineMember"), TEXT("Services"), TEXT("CodeCoverage"), TEXT("Unavailable"), TEXT("Unavailable"), EngineSource);
#endif
#if WITH_AS_DEBUGSERVER
	AddPointerRow(Snapshot, TEXT("EngineMember"), TEXT("Services"), TEXT("DebugServer"), Engine.DebugServer, EngineSource);
#else
	Snapshot.AddRow(TEXT("EngineMember"), TEXT("Services"), TEXT("DebugServer"), TEXT("Unavailable"), TEXT("Unavailable"), EngineSource);
#endif

	const FAngelscriptEngineConfig& Config = Engine.GetRuntimeConfig();
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("RuntimeConfig"), TEXT("bForceThreadedInitialize"), Config.bForceThreadedInitialize, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("RuntimeConfig"), TEXT("bSkipThreadedInitialize"), Config.bSkipThreadedInitialize, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("RuntimeConfig"), TEXT("bSkipInitialCompile"), Config.bSkipInitialCompile, EngineSource);
	AddBoolRow(Snapshot, TEXT("EngineMember"), TEXT("RuntimeConfig"), TEXT("bCollectStaticJITCompatibilityBinds"), Config.bCollectStaticJITCompatibilityBinds, EngineSource);

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ActiveModules"), TEXT("Count"), ActiveModules.Num(), EngineSource);
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		Snapshot.AddRow(TEXT("EngineCollection"), FString::Printf(TEXT("ActiveModules/%s"), *Module->ModuleName), TEXT("ModuleName"), Module->ModuleName, TEXT("String"), EngineSource);
		Snapshot.AddRow(TEXT("EngineCollection"), FString::Printf(TEXT("ActiveModules/%s"), *Module->ModuleName), TEXT("CodeHash"), LexToString(Module->CodeHash), TEXT("Integer"), EngineSource);
		AddCountRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("ActiveModules/%s"), *Module->ModuleName), TEXT("ClassCount"), Module->Classes.Num(), EngineSource);
	}

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("RootPaths"), TEXT("Count"), Engine.AllRootPaths.Num(), EngineSource);
	for (int32 RootIndex = 0; RootIndex < Engine.AllRootPaths.Num(); ++RootIndex)
	{
		Snapshot.AddRow(TEXT("EngineCollection"), FString::Printf(TEXT("RootPaths/%03d"), RootIndex), TEXT("Path"), Engine.AllRootPaths[RootIndex], TEXT("String"), EngineSource);
	}

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ScriptRoots"), TEXT("Count"), Engine.AllScriptRoots.Num(), EngineSource);
	for (int32 RootIndex = 0; RootIndex < Engine.AllScriptRoots.Num(); ++RootIndex)
	{
		const FAngelscriptSourceRoot& Root = Engine.AllScriptRoots[RootIndex];
		const FString Identity = FString::Printf(TEXT("ScriptRoots/%03d"), RootIndex);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("AbsolutePath"), Root.AbsolutePath, TEXT("String"), EngineSource);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("SourceKind"), SourceKindToString(Root.SourceKind), TEXT("String"), EngineSource);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("MountName"), Root.MountName, TEXT("String"), EngineSource);
	}

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("Diagnostics"), TEXT("FileCount"), Engine.Diagnostics.Num(), EngineSource);
	for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& Pair : Engine.Diagnostics)
	{
		AddCountRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("Diagnostics/%s"), *Pair.Key), TEXT("MessageCount"), Pair.Value.Diagnostics.Num(), EngineSource);
		AddBoolRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("Diagnostics/%s"), *Pair.Key), TEXT("bHasEmittedAny"), Pair.Value.bHasEmittedAny, EngineSource);
		AddBoolRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("Diagnostics/%s"), *Pair.Key), TEXT("bIsCompiling"), Pair.Value.bIsCompiling, EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("LastEmittedDiagnostics"), TEXT("FileCount"), Engine.LastEmittedDiagnostics.Num(), EngineSource);
	for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& Pair : Engine.LastEmittedDiagnostics)
	{
		AddCountRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("LastEmittedDiagnostics/%s"), *Pair.Key), TEXT("MessageCount"), Pair.Value.Diagnostics.Num(), EngineSource);
		AddBoolRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("LastEmittedDiagnostics/%s"), *Pair.Key), TEXT("bHasEmittedAny"), Pair.Value.bHasEmittedAny, EngineSource);
		AddBoolRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("LastEmittedDiagnostics/%s"), *Pair.Key), TEXT("bIsCompiling"), Pair.Value.bIsCompiling, EngineSource);
	}

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("FileChangesDetectedForReload"), TEXT("Count"), Engine.FileChangesDetectedForReload.Num(), EngineSource);
	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : Engine.FileChangesDetectedForReload)
	{
		AddFilenamePairRows(Snapshot, TEXT("FileChangesDetectedForReload"), FilenamePair, EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("FileDeletionsDetectedForReload"), TEXT("Count"), Engine.FileDeletionsDetectedForReload.Num(), EngineSource);
	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : Engine.FileDeletionsDetectedForReload)
	{
		AddFilenamePairRows(Snapshot, TEXT("FileDeletionsDetectedForReload"), FilenamePair, EngineSource);
	}

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ModulesByScriptModule"), TEXT("Count"), Engine.ModulesByScriptModule.Num(), EngineSource);
	for (const TPair<asIScriptModule*, TSharedPtr<FAngelscriptModuleDesc>>& Pair : Engine.ModulesByScriptModule)
	{
		const FString Identity = FString::Printf(TEXT("ModulesByScriptModule/%s"), *PointerToString(Pair.Key));
		AddPointerRow(Snapshot, TEXT("EngineCollection"), Identity, TEXT("ScriptModule"), Pair.Key, EngineSource);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("ModuleName"), ModuleDescIdentity(Pair.Value), TEXT("String"), EngineSource);
	}

#if AS_CAN_HOTRELOAD
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ActiveClassesByName"), TEXT("Count"), Engine.ActiveClassesByName.Num(), EngineSource);
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ActiveEnumsByName"), TEXT("Count"), Engine.ActiveEnumsByName.Num(), EngineSource);
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ActiveDelegatesByName"), TEXT("Count"), Engine.ActiveDelegatesByName.Num(), EngineSource);
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("FileHotReloadState"), TEXT("Count"), Engine.FileHotReloadState.Num(), EngineSource);
	for (const TPair<FString, FAngelscriptEngine::FHotReloadState>& Pair : Engine.FileHotReloadState)
	{
		const FString Identity = FString::Printf(TEXT("FileHotReloadState/%s"), *Pair.Key);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("LastChange"), Pair.Value.LastChange.ToString(), TEXT("DateTime"), EngineSource);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("ContentHash"), LexToString(Pair.Value.ContentHash), TEXT("Integer"), EngineSource);
		AddBoolRow(Snapshot, TEXT("EngineCollection"), Identity, TEXT("bHasContentHash"), Pair.Value.bHasContentHash, EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("PreviouslyFailedReloadFiles"), TEXT("Count"), Engine.PreviouslyFailedReloadFiles.Num(), EngineSource);
	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : Engine.PreviouslyFailedReloadFiles)
	{
		AddFilenamePairRows(Snapshot, TEXT("PreviouslyFailedReloadFiles"), FilenamePair, EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("QueuedFullReloadFiles"), TEXT("Count"), Engine.QueuedFullReloadFiles.Num(), EngineSource);
	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : Engine.QueuedFullReloadFiles)
	{
		AddFilenamePairRows(Snapshot, TEXT("QueuedFullReloadFiles"), FilenamePair, EngineSource);
	}
#endif

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("GlobalContextPool"), TEXT("Count"), Engine.GlobalContextPool.Num(), EngineSource);
	for (int32 ContextIndex = 0; ContextIndex < Engine.GlobalContextPool.Num(); ++ContextIndex)
	{
		AddPointerRow(Snapshot, TEXT("EngineCollection"), FString::Printf(TEXT("GlobalContextPool/%03d"), ContextIndex), TEXT("Context"), Engine.GlobalContextPool[ContextIndex], EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("InterfaceMethodSignatures"), TEXT("Count"), Engine.InterfaceMethodSignatures.Num(), EngineSource);
	for (const TUniquePtr<FInterfaceMethodSignature>& Signature : Engine.InterfaceMethodSignatures)
	{
		if (Signature.IsValid())
		{
			Snapshot.AddRow(TEXT("EngineCollection"), FString::Printf(TEXT("InterfaceMethodSignatures/%s"), *Signature->FunctionName.ToString()), TEXT("FunctionName"), Signature->FunctionName.ToString(), TEXT("Name"), EngineSource);
		}
	}

	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("StaticNames"), TEXT("Count"), FAngelscriptEngine::GetStaticNameCount(), EngineSource);
	for (int32 StaticNameIndex = 0; StaticNameIndex < Engine.StaticNames.Num(); ++StaticNameIndex)
	{
		Snapshot.AddRow(TEXT("EngineCollection"), FString::Printf(TEXT("StaticNames/%03d"), StaticNameIndex), TEXT("Name"), Engine.StaticNames[StaticNameIndex].ToString(), TEXT("Name"), EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("StaticNamesByIndex"), TEXT("Count"), Engine.StaticNamesByIndex.Num(), EngineSource);
	for (const TPair<FName, int32>& Pair : Engine.StaticNamesByIndex)
	{
		const FString Identity = FString::Printf(TEXT("StaticNamesByIndex/%s"), *Pair.Key.ToString());
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("Name"), Pair.Key.ToString(), TEXT("Name"), EngineSource);
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("Index"), LexToString(Pair.Value), TEXT("Integer"), EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("ScriptEnumTypeLookupByName"), TEXT("Count"), Engine.GetScriptEnumTypeLookup().Num(), EngineSource);
	for (const TPair<FName, asITypeInfo*>& Pair : Engine.GetScriptEnumTypeLookup())
	{
		const FString Identity = FString::Printf(TEXT("ScriptEnumTypeLookupByName/%s"), *Pair.Key.ToString());
		Snapshot.AddRow(TEXT("EngineCollection"), Identity, TEXT("Name"), Pair.Key.ToString(), TEXT("Name"), EngineSource);
		AddPointerRow(Snapshot, TEXT("EngineCollection"), Identity, TEXT("TypeInfo"), Pair.Value, EngineSource);
	}
	AddCountRow(Snapshot, TEXT("EngineCollection"), TEXT("BoundBlueprintEventArgumentSpecializations"), TEXT("Count"), Engine.BoundBlueprintEventArgumentSpecializations.Num(), EngineSource);
	for (const FString& Specialization : Engine.BoundBlueprintEventArgumentSpecializations)
	{
		Snapshot.AddRow(TEXT("EngineCollection"), FString::Printf(TEXT("BoundBlueprintEventArgumentSpecializations/%s"), *Specialization), TEXT("Specialization"), Specialization, TEXT("String"), EngineSource);
	}

	asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(Engine.GetScriptEngine());
	if (ScriptEngine != nullptr)
	{
		const FString Source = TEXT("asCScriptEngine");
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ModuleCount"), AsArrayCount(ScriptEngine->scriptModules), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredObjectTypeCount"), AsArrayCount(ScriptEngine->registeredObjTypes), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredTypedefCount"), AsArrayCount(ScriptEngine->registeredTypeDefs), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredEnumCount"), AsArrayCount(ScriptEngine->registeredEnums), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredGlobalPropertyCount"), AsArrayCount(ScriptEngine->registeredGlobalProps), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredGlobalFunctionCount"), AsArrayCount(ScriptEngine->registeredGlobalFuncs), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredFuncdefCount"), AsArrayCount(ScriptEngine->registeredFuncDefs), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("RegisteredTemplateTypeCount"), AsArrayCount(ScriptEngine->registeredTemplateTypes), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("TemplateSubTypeCount"), AsArrayCount(ScriptEngine->templateSubTypes), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("UnvalidatedTemplateInstanceCount"), ScriptEngine->unvalidatedTemplateInstances.Num(), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("TemplateInstanceBucketCount"), ScriptEngine->templateInstanceBuckets.Num(), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ListPatternTypeCount"), AsArrayCount(ScriptEngine->listPatternTypes), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("GlobalPropertyCount"), AsArrayCount(ScriptEngine->globalProperties), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ScriptFunctionCount"), AsArrayCount(ScriptEngine->scriptFunctions), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ImportedFunctionCount"), AsArrayCount(ScriptEngine->importedFunctions), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("SharedScriptTypeCount"), AsArrayCount(ScriptEngine->sharedScriptTypes), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("FuncDefCount"), AsArrayCount(ScriptEngine->funcDefs), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ScriptSectionNameCount"), AsArrayCount(ScriptEngine->scriptSectionNames), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("TypeIdMapCount"), ScriptEngine->mapTypeIdToTypeInfo.Num(), Source);
		int32 ScriptModulesByNameCount = 0;
		ScriptEngine->scriptModulesByName.IterateAll(
			[&ScriptModulesByNameCount](asCModule*)
			{
				++ScriptModulesByNameCount;
			});
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ScriptModulesByNameCount"), ScriptModulesByNameCount, Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("AllRegisteredTypesCount"), CountAsMapEntries(ScriptEngine->allRegisteredTypes), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("AllScriptGlobalFunctionCount"), CountAsMapEntries(ScriptEngine->allScriptGlobalFunctions), Source);
		int32 AllScriptDeclaredTypeCount = 0;
		ScriptEngine->allScriptDeclaredTypes.IterateAll(
			[&AllScriptDeclaredTypeCount](asCTypeInfo*)
			{
				++AllScriptDeclaredTypeCount;
			});
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("AllScriptDeclaredTypeCount"), AllScriptDeclaredTypeCount, Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("AllScriptGlobalVariableCount"), CountAsMapEntries(ScriptEngine->allScriptGlobalVariables), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("VarAddressMapCount"), ScriptEngine->varAddressMap.Num(), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("FreeGlobalPropertyIdCount"), AsArrayCount(ScriptEngine->freeGlobalPropertyIds), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("FreeScriptFunctionIdCount"), AsArrayCount(ScriptEngine->freeScriptFunctionIds), Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("FreeImportedFunctionIndexCount"), AsArrayCount(ScriptEngine->freeImportedFunctionIdxs), Source);
		Snapshot.AddRow(TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("DefaultNamespace"), NameSpaceToString(ScriptEngine->defaultNamespace), TEXT("String"), Source);
		Snapshot.AddRow(TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("DefaultAccessMask"), LexToString(ScriptEngine->defaultAccessMask), TEXT("Integer"), Source);
		AddBoolRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("bConfigFailed"), ScriptEngine->configFailed, Source);
		AddBoolRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("bIsBuilding"), ScriptEngine->isBuilding, Source);
		AddBoolRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("bDeferValidationOfTemplateTypes"), ScriptEngine->deferValidationOfTemplateTypes, Source);
		AddBoolRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("bDeferCalculatingTemplateSize"), ScriptEngine->deferCalculatingTemplateSize, Source);
		AddBoolRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("bMessageCallback"), ScriptEngine->msgCallback, Source);
		AddPointerRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("MessageCallbackObject"), ScriptEngine->msgCallbackObj, Source);
		AddBoolRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("bPreMessageSet"), ScriptEngine->preMessage.isSet, Source);
		Snapshot.AddRow(TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("TypeIdSeqNbr"), LexToString(ScriptEngine->typeIdSeqNbr), TEXT("Integer"), Source);

		for (asUINT SectionIndex = 0; SectionIndex < ScriptEngine->scriptSectionNames.GetLength(); ++SectionIndex)
		{
			const asCString* SectionName = ScriptEngine->scriptSectionNames[SectionIndex];
			Snapshot.AddRow(TEXT("AsEngineInternal"), FString::Printf(TEXT("ScriptSectionNames/%03u"), SectionIndex), TEXT("Name"), SectionName != nullptr ? CharToString(SectionName->AddressOf()) : TEXT("<null-section>"), TEXT("String"), Source);
		}
		for (const TPair<int, asCTypeInfo*>& Pair : ScriptEngine->mapTypeIdToTypeInfo)
		{
			const FString Identity = FString::Printf(TEXT("TypeIdMap/%d"), Pair.Key);
			Snapshot.AddRow(TEXT("AsEngineInternal"), Identity, TEXT("TypeId"), LexToString(Pair.Key), TEXT("Integer"), Source);
			Snapshot.AddRow(TEXT("AsEngineInternal"), Identity, TEXT("Type"), PointerToString(Pair.Value), TEXT("Pointer"), Source);
			AddPointerRow(Snapshot, TEXT("AsEngineInternal"), Identity, TEXT("TypeInfo"), Pair.Value, Source);
		}

		asUINT GCCurrentSize = 0;
		asUINT GCTotalDestroyed = 0;
		asUINT GCTotalDetected = 0;
		asUINT GCNewObjects = 0;
		asUINT GCTotalNewDestroyed = 0;
		ScriptEngine->GetGCStatistics(&GCCurrentSize, &GCTotalDestroyed, &GCTotalDetected, &GCNewObjects, &GCTotalNewDestroyed);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("GarbageCollector"), TEXT("CurrentSize"), GCCurrentSize, Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("GarbageCollector"), TEXT("TotalDestroyed"), GCTotalDestroyed, Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("GarbageCollector"), TEXT("TotalDetected"), GCTotalDetected, Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("GarbageCollector"), TEXT("NewObjects"), GCNewObjects, Source);
		AddCountRow(Snapshot, TEXT("AsEngineInternal"), TEXT("GarbageCollector"), TEXT("TotalNewDestroyed"), GCTotalNewDestroyed, Source);

		for (asUINT ModuleIndex = 0; ModuleIndex < ScriptEngine->scriptModules.GetLength(); ++ModuleIndex)
		{
			AddModuleRows(Snapshot, ScriptEngine->scriptModules[ModuleIndex]);
		}
		for (asUINT TypeIndex = 0; TypeIndex < ScriptEngine->registeredObjTypes.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, ScriptEngine->registeredObjTypes[TypeIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < ScriptEngine->registeredEnums.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, ScriptEngine->registeredEnums[TypeIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < ScriptEngine->registeredTypeDefs.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, ScriptEngine->registeredTypeDefs[TypeIndex], Source);
		}
		for (asUINT TypeIndex = 0; TypeIndex < ScriptEngine->registeredFuncDefs.GetLength(); ++TypeIndex)
		{
			AddTypeRows(Snapshot, ScriptEngine->registeredFuncDefs[TypeIndex], Source);
		}
	}
	else
	{
		Snapshot.AddRow(TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("State"), TEXT("Unavailable"), TEXT("Unavailable"), TEXT("asCScriptEngine"));
	}

	Snapshot.SortRows();
	return Snapshot;
}
