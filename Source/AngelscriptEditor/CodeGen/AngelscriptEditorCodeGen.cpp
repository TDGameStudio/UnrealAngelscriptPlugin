#include "Core/AngelscriptEditorModule.h"
#include "AngelscriptSettings.h"
#include "AngelscriptRuntimeModule.h"
#include "AngelscriptEngine.h"
#include "Binds/Bind_FGameplayTag.h"
#include "ClassGenerator/ASClass.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "SourceCodeNavigation.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptBinds.h"
#include "UObject/UObjectGlobals.h"

TMap<FString, FString> HeaderCache = TMap<FString, FString>();
void ProcessStaticFunction(UClass* Class, UFunction* Function, TArray<FString>& lines, TArray<FString>& IncludeList, TSet<FString>& IncludeSet, TSet<FString>& ModuleSet);
void AddParameterInclude(FProperty* prop, TArray<FString>& IncludeList, TSet<FString>& IncludeSet, TSet<FString>& ModuleSet);
void FunctionTests()
{	
	TArray<FString> lines;

	for (UFunction* Function : TObjectRange<UFunction>())
	{
		if (Function->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Native | FUNC_Static))
		{			
			FString name = Function->GetName();
			FString args = "(";
			FString retType = FString();
			FString ClassType = Function->GetOuterUClass()->GetName() + "::";
			bool firstArg = true;
			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				FProperty* prop = *It;

				if (firstArg)
					firstArg = false;
				else
					args += ", ";

				FString* retOrArg = nullptr;
				if (!prop->HasAnyPropertyFlags(CPF_ReturnParm))
					retOrArg = &args;
				else
					retOrArg = &retType;
								
				if (prop->HasAnyPropertyFlags(CPF_ConstParm))
					*retOrArg += "const ";

				FString Extend;
				FString type = prop->GetCPPType(&Extend) + Extend;
				*retOrArg += type;

				if (prop->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_OutParm))
					*retOrArg += "&";

				if (!prop->HasAnyPropertyFlags(CPF_ReturnParm))
					args += " " + prop->GetName();				
			}

			args += ")";

			if (retType.IsEmpty())
				retType = "void ";

			FString line = retType + " " + ClassType + name + args + "\n";
			lines.Add(line);
		}
	}
	
	FFileHelper::SaveStringArrayToFile(lines, *(FAngelscriptEngine::Get().GetScriptRootDirectory() / TEXT("Static-Functions-Test.txt")));

	//FString Path = "DSP/Filter.h";
	//FString FullPath = FPaths::ConvertRelativePathToFull(Path);

	//FString ClassPath;
	//if (FSourceCodeNavigation::FindClassHeaderPath(Function, ClassPath))
	//{
	//	FString ModulePath = FAngelscriptEditorModule::GetIncludeForModule(Function->GetOuterUClass(), ClassPath);
	//	lines.Add(ModulePath);
	//}


	//FInterfaceProperty
	//TArray<FString> lines;
	//
	//for (UFunction* Function : TObjectRange<UFunction>())
	//{
	//	if (Function->HasAllFunctionFlags(FUNC_BlueprintCallable | FUNC_Native))
	//	{
	//		FString line = Function->GetOuterUClass()->GetName() + " " + Function->GetName();
	//		lines.Add(line);
	//	}
	//}
	//
	//FFileHelper::SaveStringArrayToFile(lines, *(FAngelscriptEngine::Get().GetScriptRootDirectory() / TEXT("Functions-Test.txt")));

	///*
	//for (UClass* Class : TObjectRange<UClass>())
	//{
	//			
	//	TArray<FName> FuncNames;
	//	Class->GenerateFunctionList(FuncNames);		
	//
	//	UPackage* Package = Class->GetPackage();
	//	FString packname = Package->GetFName().ToString();
	//
	//	for (FName FuncName : FuncNames)
	//	{					
	//		UFunction* Function = Class->FindFunctionByName(FuncName);
	//		if (Function != nullptr && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
	//		{
	//			for (TFieldIterator<FProperty> It(Function); It; ++It)
	//			{
	//				FProperty* prop = *It;					
	//				
	//				if (prop->IsA<FEnumProperty>())
	//				{
	//					FEnumProperty* newProp = (FEnumProperty*)prop;
	//					UEnum* real = newProp->GetEnum();
	//					FString header;							
	//					FSourceCodeNavigation::FindClassHeaderPath(real, header);	
	//					UE_LOG(Angelscript, Log, TEXT("Hi"));
	//				}
	//				
	//				if (prop->IsA<FClassProperty>())
	//				{
	//					//TObjectPtr<UObject> ptr = newProp->GetDefaultPropertyValue();
	//					//UClass* real = ptr.Get()->GetClass();
	//					FClassProperty* newProp = (FClassProperty*)prop;							
	//					FString ext;// = newProp->GetAuthoredName();
	//					FString type = newProp->GetCPPType(&ext, 0) + ext;
	//					UClass* real = newProp->MetaClass;						
	//					FString header;
	//					FSourceCodeNavigation::FindClassHeaderPath(real, header);						
	//					UE_LOG(Angelscript, Log, TEXT("Hi"));
	//				}
	//
	//				if (prop->IsA<FObjectProperty>())
	//				{
	//					FObjectProperty* newProp = (FObjectProperty*)prop;
	//					UClass* real = newProp->PropertyClass;
	//					FString type = real->GetName();
	//					FString header;
	//					FSourceCodeNavigation::FindClassHeaderPath(real, header);
	//					UE_LOG(Angelscript, Log, TEXT("Hi"));
	//				}
	//									
	//				if (prop->IsA<FStructProperty>())
	//				{
	//					FStructProperty* newProp = (FStructProperty*)prop;						
	//					UScriptStruct* real = newProp->Struct;
	//					FString header;
	//					FSourceCodeNavigation::FindClassHeaderPath(real, header);
	//					UE_LOG(Angelscript, Log, TEXT("Hi"));
	//				}
	//				//for (TPropertyValueIterator<FProperty> It(prop, Function); It; ++It)
	//				
	//				//FFieldVariant variant(prop);
	//				//if (UClass* owner = variant.GetOwnerClass())
	//				//{											    
	//				//    FString header;					    
	//				//    FString Module;
	//				//    bool found1 = FSourceCodeNavigation::FindClassHeaderPath(owner, header);					    
	//				//    bool found2 = FSourceCodeNavigation::FindClassModuleName(owner, Module);
	//				//    
	//				//    if (found1 || found2)
	//				//    	UE_LOG(Angelscript, Log, TEXT("YAY"));
	//				//}
	//			}
	//
	//			//for (UProperty* field : TFieldRange<UProperty>(Function))
	//			//{
	//			//	UClass* fieldClass = field->GetClass();
	//			//	FString header;
	//			//	FString classHeader;
	//			//	FString Module;
	//			//	bool found1 = FSourceCodeNavigation::FindClassHeaderPath(field, header);
	//			//	bool found2 = FSourceCodeNavigation::FindClassHeaderPath(fieldClass, classHeader);
	//			//	bool found3 = FSourceCodeNavigation::FindClassModuleName(fieldClass, Module);
	//			//
	//			//	if (found1 || found2 || found3)
	//			//		UE_LOG(Angelscript, Log, TEXT("YAY"));
	//			//}
	//
	//			//for (TFieldIterator<FProperty> It(Function); It; ++It)
	//			//{
	//			//	FProperty* prop = *It;
	//			//	FFieldVariant field = FindUField<UField>(Function, prop->GetFName());
	//			//	
	//			//
	//			//	//FFieldClass* field = prop->GetClass();
	//			//	//FString str;					
	//			//	//FString type = prop->GetCPPType(&str) + str;
	//			//	
	//			//	//FField* cdo = field->GetDefaultObject();
	//			//	//FLinkerLoad* linker = cdo->GetLinker();
	//			//	//FString file = linker->Filename;
	//			//
	//			//	//UPropertyWrapper* wrap = prop->GetUPropertyWrapper();
	//			//	//UClass* wrapClass = wrap->GetClass();
	//			//	
	//			//	
	//			//}
	//			
	//			//for (UProperty* prop : TPropertyValueRange<UProperty>(Function, Function->ptr))
	//			
	//			//if (!Class->HasAnyClassFlags(CLASS_MinimalAPI))
	//	        //	continue;	
	//	        //
	//	        //FString path;
	//	        //if (FSourceCodeNavigation::FindClassSourcePath(Class, path))
	//	        //{
	//	        //	FString line = Class->GetName() + ": " + path;
	//	        //	lines.Add(line);
	//	        //}
	//
	//			//if (Function->HasAnyFunctionFlags(FUNC_EditorOnly))
	//			//{
	//			//	if (!Function->HasAnyFunctionFlags(FUNC_RequiredAPI))
	//			//	{
	//			//		//UE_LOG(Angelscript, Log, TEXT("Check this out"));
	//			//		FString line = Class->GetName() + "::" + Function->GetName() + " " + packname + " EditorOnly";
	//			//		lines.Add(line);
	//			//	}					
	//			//}
	//			//
	//			//else
	//			//{
	//			//	if (!Function->HasAnyFunctionFlags(FUNC_RequiredAPI))
	//			//	{
	//			//		//UE_LOG(Angelscript, Log, TEXT("Check this out"));
	//			//		FString line = Class->GetName() + "::" + Function->GetName() + " " + packname + " Runtime/Both";
	//			//		lines.Add(line);
	//			//	}
	//			//}
	//		}
	//	}
	//}
	//*/
	
	//FFileHelper::SaveStringArrayToFile(lines, *(FAngelscriptEngine::Get().GetScriptRootDirectory() / TEXT("Class-Source-Paths.txt")));

}

FString FAngelscriptEditorModule::GetIncludeForModule(UField* Class, FString& HeaderPath)
{
	if (HeaderPath.IsEmpty())
		FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);

	FString ModulePath = HeaderPath;

	int32 pubIndex = HeaderPath.Find("Public/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
	int32 privIndex = HeaderPath.Find("Private/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
	int32 classesIndex = HeaderPath.Find("Classes/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);

	if (pubIndex > 0)
		ModulePath = HeaderPath.RightChop(pubIndex + 7);
	if (privIndex > 0)
		ModulePath = HeaderPath.RightChop(privIndex + 8);
	if (classesIndex > 0)
		ModulePath = HeaderPath.RightChop(classesIndex + 8);

	FString Include = "#include ";
	Include += '"';
	Include += ModulePath;
	Include += '"';
	return Include;
}

void FAngelscriptEditorModule::GenerateNativeBinds()
{
	// Native bind generation produces large transient buffers (per-class
	// header + cpp content arrays). Tag the entire pass under Angelscript.
	LLM_SCOPE_BYTAG(Angelscript);

	//FAngelscriptBindDatabase& BindDB = FAngelscriptBindDatabase::Get();

	GenerateBindDatabases();

	const uint32 ModuleCount = 10; //was 10
	TArray<FString> Keys;	
	FAngelscriptBinds::GetRuntimeClassDB().GetKeys(Keys);
	TArray<FString> ModuleArray;
	FAngelscriptBinds::GetBindModuleNames().Empty();

	for (int i = 0; i < Keys.Num(); i++)
	{
		ModuleArray.Add(Keys[i]);

		if (ModuleArray.Num() >= ModuleCount)
		{
			FString ModuleName = FString("ASRuntimeBind_");
			ModuleName += FString::FromInt(i - (ModuleArray.Num() - 1));
			GenerateNewModule(ModuleName, ModuleArray, false);
			ModuleArray.Empty();
			FAngelscriptBinds::GetBindModuleNames().Add(ModuleName);
		}
	}

	if (!ModuleArray.IsEmpty())
	{
		FString ModuleName = FString("ASRuntimeBind_");
		ModuleName += FString::FromInt(Keys.Num() - ModuleArray.Num());
		GenerateNewModule(ModuleName, ModuleArray, false);
		ModuleArray.Empty();
		FAngelscriptBinds::GetBindModuleNames().Add(ModuleName);
	}
	
	Keys.Empty();
	FAngelscriptBinds::GetEditorClassDB().GetKeys(Keys);

	for (int i = 0; i < Keys.Num(); i++)
	{
		ModuleArray.Add(Keys[i]);

		if (ModuleArray.Num() >= ModuleCount)
		{
			FString ModuleName = FString("ASEditorBind_");
			ModuleName += FString::FromInt(i - (ModuleArray.Num() - 1));
			GenerateNewModule(ModuleName, ModuleArray, true);
			ModuleArray.Empty();
			FAngelscriptBinds::GetBindModuleNames().Add(ModuleName);
		}
	}

	if (!ModuleArray.IsEmpty())
	{
		FString ModuleName = FString("ASEditorBind_");
		ModuleName += FString::FromInt(Keys.Num() - ModuleArray.Num());
		GenerateNewModule(ModuleName, ModuleArray, true);
		ModuleArray.Empty();
		FAngelscriptBinds::GetBindModuleNames().Add(ModuleName);
	}

	TArray<FString> PublicDepends
	{
		FString("Core"),
		FString("CoreUObject"),
		FString("Engine"),
		FString("AngelscriptRuntime"),
	};

	//TArray<FString> UpdatedBuildFile;
	//GenerateBuildFile("AngelscriptNativeBinds", PublicDepends, FAngelscriptBinds::BindModuleNames, UpdatedBuildFile, false);
	//FString NativeBindsPath;
	//if (FSourceCodeNavigation::FindModulePath("AngelscriptNativeBinds", NativeBindsPath))
	//{
	//	NativeBindsPath += "/AngelscriptNativeBinds.Build.cs";
	//	FFileHelper::SaveStringArrayToFile(UpdatedBuildFile, *NativeBindsPath);
	//}
	
	FAngelscriptBinds::SaveBindModules(FString(FAngelscriptEngine::GetScriptRootDirectory() / "BindModules.Cache"));
	//GameProjectUtils::BuildCodeProject(GameProjectUtils::GetCurrentProjectModules())	
}

void FAngelscriptEditorModule::GenerateBindDatabases() 
{
	//TMap<UPackage*, TArray<UClass*>> ClassGroups = TMap<UPackage*, TArray<UClass*>>();	
	//TMap<FString, TArray<TObjectPtr<UClass>>> ClassGroups = TMap<FString, TArray<TObjectPtr<UClass>>>();
	//Should probably add something on checking if already loaded or something
	//FAngelscriptBindDatabase& BindDB = FAngelscriptBindDatabase::Get();

	for (UClass* Class : TObjectRange<UClass>())
	{
		FString Name;
		Class->GetPackage()->GetName(Name);		
		FString ClassName = Class->GetName();

		if (FAngelscriptBinds::CheckForSkipClass(Class->GetFName()))
			continue;

		if (ClassName.Contains("DEPRECATED", ESearchCase::CaseSensitive))
			continue;

		if (Name.Contains("Angelscript"))
			continue;

		if (ClassName.Contains("Angelscript"))
			continue;

		//Should be able to remove the Minimal restriction soon
		//if (Class->HasAnyClassFlags(CLASS_Interface | CLASS_Abstract | CLASS_MinimalAPI))
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
			continue;

		FString HeaderPath = FString();
		bool headerFound = FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);
		if (!headerFound) continue;

		if (HeaderPath.Contains("Private", ESearchCase::IgnoreCase, ESearchDir::FromEnd)) 
			continue;

		TArray<FName> FuncNames;
		Class->GenerateFunctionList(FuncNames);
		if (FuncNames.IsEmpty()) continue;

		bool HasFunctions = false;

		for (FName FuncName : FuncNames)
		{
			UFunction* Function = Class->FindFunctionByName(FuncName);		
			if (Function != nullptr && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			{
				HasFunctions = true;
				break;
			}
		}

		if (!HasFunctions) continue;

		if (!HeaderPath.Contains("Editor/")) //Was !Class->EditorOnly which didn't work
		{			
			if (FAngelscriptBinds::GetRuntimeClassDB().Contains(Name))
			{
				FAngelscriptBinds::GetRuntimeClassDB()[Name].Add(Class);
			}

			else
			{
				FAngelscriptBinds::GetRuntimeClassDB().Add(Name, TArray<TObjectPtr<UClass>>()).Add(Class);
			}
		}

		else
		{
			if (FAngelscriptBinds::GetEditorClassDB().Contains(Name))
			{				
				FAngelscriptBinds::GetEditorClassDB()[Name].Add(Class);
			}

			else
			{				
				FAngelscriptBinds::GetEditorClassDB().Add(Name, TArray<TObjectPtr<UClass>>()).Add(Class);
			}
		}
	}
}



void FAngelscriptEditorModule::GenerateNewModule(FString ModuleName, TArray<FString> ModuleList, bool bIsEditor)
{
	TArray<FString> PublicDepends
	{
		FString("Core"),
		FString("CoreUObject"),
		FString("Engine"),
		FString("AngelscriptRuntime"),
	};

	if (bIsEditor)
		PublicDepends.Add("AngelscriptEditor");

	//We don't want it actually inside another module, it won't compile
	FString BaseDir;
	FSourceCodeNavigation::FindModulePath("AngelscriptRuntime", BaseDir);
	BaseDir.RemoveFromEnd("AngelscriptRuntime");

	//FString NewDir = BaseDir;
	//BaseDir Already ends with a / so we don't need 2
	FString NewDir = BaseDir + ModuleName + "/";
	FString BuildDir = NewDir + ModuleName + ".Build.cs";

	FString HeaderDir = NewDir + "Public/" + ModuleName + "Module.h";
	FString CPPDir = NewDir + "Private/" + ModuleName + "Module.cpp";
	FString BaseCPPDir = NewDir + "Private/";
	//FString HeaderDir = NewDir + "Source/" + ModuleName + "Module.h";
	//FString CPPDir = NewDir + "Source/" + ModuleName + "Module.cpp";

	TArray<FString> Header, CPPFile;
	//GenerateSourceFiles(ModuleName, ModuleList, bIsEditor, Header, CPPFile);
	GenerateSourceFilesV2(ModuleName, ModuleList, bIsEditor, Header, BaseCPPDir);
	
	TArray<FString> BuildFile;
	GenerateBuildFile(ModuleName, PublicDepends, ModuleList, BuildFile);

	//Create new directory structures for the module
	//FString BindModule;

	FFileHelper::SaveStringArrayToFile(BuildFile, *BuildDir);
	FFileHelper::SaveStringArrayToFile(Header, *HeaderDir);
	//FFileHelper::SaveStringArrayToFile(CPPFile, *CPPDir);	

	HeaderCache.Empty();
}



void FAngelscriptEditorModule::GenerateBuildFile(FString ModuleName, TArray<FString>& PublicDependencies, TArray<FString>& PrivateDependencies, TArray<FString>& OutBuildFile, bool removeFirst)
{
	TArray<FString>& Lines = OutBuildFile;
	Lines.Add("using System.IO;");
	Lines.Add("using UnrealBuildTool;\n");
	Lines.Add("namespace UnrealBuildTool.Rules");
	Lines.Add("{");
	Lines.Add("\tpublic class " + ModuleName + " : ModuleRules");
	Lines.Add("\t{");
	Lines.Add("\t\tpublic " + ModuleName + "(ReadOnlyTargetRules Target) : base(Target)\n\t\t{");
	
	Lines.Add("\t\t\tPCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;\n");

	Lines.Add("\t\t\tPublicDependencyModuleNames.AddRange\n\t\t\t(");
	Lines.Add("\t\t\t\tnew string[]");
	Lines.Add("\t\t\t\t{");

	for (FString str : PublicDependencies)
	{
		FString line = FString("\t\t\t\t\t");		
		//FString include = line;
		//int32 modIndex = 0;
		//str.FindLastChar('/', modIndex);
		//include = str.RightChop(modIndex);
		//include.RemoveAt(0, 1, true);

		line += '"';
		line += str;
		line += '"';
		line += ",";
		Lines.Add(line);
	}

	Lines.Add("\t\t\t\t}");

	Lines.Add("\t\t\t);\n"); //End Public Dependencies;


	Lines.Add("\t\t\tPrivateDependencyModuleNames.AddRange\n\t\t\t(");

	Lines.Add("\t\t\t\tnew string[]");
	Lines.Add("\t\t\t\t{");

	for (FString str : PrivateDependencies)
	{
		FString line = FString("\t\t\t\t\t");
		FString include = line;
		int32 modIndex = 0;
		str.FindLastChar('/', modIndex);		
		include = str.RightChop(modIndex);
		if (removeFirst)
			include.RemoveAt(0, 1, EAllowShrinking::Yes);

		line += '"';
		line += include;
		line += '"';
		line += ",";
		Lines.Add(line);
	}

	Lines.Add("\t\t\t\t}");

	Lines.Add("\t\t\t);"); //End Private Dependencies;

	Lines.Add("\t\t}"); //End Constructor

	Lines.Add("\t}"); //End Class

	Lines.Add("}"); //End Namespace
}

void FAngelscriptEditorModule::GenerateSourceFilesV2(FString NewModuleName, TArray<FString>& ModuleList, bool bIsEditor, TArray<FString>& Header, FString CPPDir)
{
	//It's probably best to just do all of our bind registration inside the Module class
	//I'd also review whether we really want to keep/serialize the database dictionaries
	//or just have a far simpler array of module names be saved.
	//FAngelscriptBindDatabase& BindDB = FAngelscriptBindDatabase::Get();

	TArray<TObjectPtr<UClass>> Classes;

	//Generate Header
	Header.Add("#pragma once\n");
	Header.Add("#include \"CoreMinimal.h\"");
	Header.Add("#include \"Modules/ModuleManager.h\"\n");
	//Header.Add("#include \"AngelscriptBinds.h\"");

	FString ClassLine = FString("class ");
	FString API = NewModuleName.ToUpper() + "_API ";
	//ClassLine += API;	
	ClassLine += "F" + NewModuleName + "Module : public FDefaultModuleImpl";
	Header.Add(ClassLine);

	Header.Add("{");
	Header.Add("public:");
	Header.Add("\tvirtual void StartupModule() override;");
	Header.Add("\tvirtual void ShutdownModule() override;");
	//Header.Add("};"); //Probably want to comment this out so the others can be declared later
	TArray<FString> ModuleCPP;
	FString ModuleDir = CPPDir + NewModuleName + "Module.cpp";

	ModuleCPP.Add("#include \"" + NewModuleName + "Module.h\"");
	ModuleCPP.Add("#include \"AngelscriptBinds.h\"");

	//Generate CPP File
	FString ModuleClass = FString("F") + NewModuleName + "Module";
	FString MacroLine = FString("IMPLEMENT_MODULE(") + ModuleClass + ", " + NewModuleName + ");\n";
	ModuleCPP.Add(MacroLine);

	ModuleCPP.Add(FString("void ") + ModuleClass + "::StartupModule()\n{\n");

	ModuleCPP.Add("\tFAngelscriptBinds::RegisterBinds\n\t(");
	ModuleCPP.Add("\t\t(int32)FAngelscriptBinds::EOrder::Late,");
	ModuleCPP.Add("\t\t[]()");
	ModuleCPP.Add("\t\t{");
	TArray<FString> BindFunctionNames;
	TSet<FString> ModuleSet(ModuleList);
	int32 moduleCount = ModuleSet.Num();

	for (FString Module : ModuleList)
	{
		Classes.Empty(); //Ensure that the list is cleared to avoid repeats if lookups fail

		if (!bIsEditor)
		{
			if (TArray<TObjectPtr<UClass>>* list = FAngelscriptBinds::GetRuntimeClassDB().Find(Module))
			{
				Classes = *list;
			}

			else
			{
				UE_LOG(Angelscript, Log, TEXT("No new list"));
			}
		}
		else
		{
			if (TArray<TObjectPtr<UClass>>* list = FAngelscriptBinds::GetEditorClassDB().Find(Module))
			{
				Classes = *list;
			}

			else
			{
				UE_LOG(Angelscript, Log, TEXT("No new list"));
			}
		}

		//Add Include loop here for all dependencies
		//TSet<FString> CurrentIncludes = TSet<FString>();
		//TArray<FString> Headers;
		//TArray<FString> ModuleNames;

		for (auto Class : Classes)
		{
			TArray<FString> NewCPPFile;
			TArray<FString> CPPFuncs;
			TSet<FString> CurrentIncludes;

			FString HeaderPath = FString();
			bool headerFound = FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);
			if (!headerFound) continue;

			//CPPFile.Add("#include \"" + NewModuleName + "Module.h\"");
			//CPPFile.Add("#include \"AngelscriptBinds.h\"");
			NewCPPFile.Add("#include \"" + NewModuleName + "Module.h\"");
			NewCPPFile.Add("#include \"AngelscriptBinds.h\"");
			FString ModuleName;
			FString HeaderInclude = HeaderPath;
			FSourceCodeNavigation::FindClassModuleName(Class, ModuleName);
						
			int32 pubIndex = HeaderPath.Find("Public/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
			int32 privIndex = HeaderPath.Find("Private/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
			int32 classesIndex = HeaderPath.Find("Classes/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);

			if (pubIndex > 0)
				HeaderInclude = HeaderInclude.RightChop(pubIndex + 7);
			if (privIndex > 0)
				HeaderInclude = HeaderInclude.RightChop(privIndex + 8);
			if (classesIndex > 0)
				HeaderInclude = HeaderInclude.RightChop(classesIndex + 8);

			FString Include = "#include ";
			Include += '"';
			Include += HeaderInclude;
			//Include += HeaderPath;
			Include += '"';

			//if (!CurrentIncludes.Contains(HeaderPath))
			if (!CurrentIncludes.Contains(HeaderInclude))
			{
				//CurrentIncludes.Add(HeaderInclude);
				CurrentIncludes.Add(Include);
				NewCPPFile.Add(Include);
				//CurrentIncludes.Add(HeaderPath);
				//CPPFile.Add(Include);
			}

			FString BindFunction = "Bind_" + Class->GetName();
			
			CPPFuncs.Add("void " + ModuleClass + "::" + BindFunction + "()");
			CPPFuncs.Add("{");

			int32 emptyCheck = CPPFuncs.Num();
			
			//NewCPPFile is the CurrentIncludes Array, CurrentIncludes is IncludeSet
			//It would probably be better to just use the IncludeSet
			GenerateFunctionEntries(Class, CPPFuncs, NewCPPFile, CurrentIncludes, HeaderPath, Module, ModuleSet);			

			if (CPPFuncs.Num() > emptyCheck) //Don't save the file if no functions were added
			{
				CPPFuncs.Add("}");
				FString NewCPPDir = CPPDir + BindFunction + ".cpp";
				
				//NewCPPFile.Append(CurrentIncludes.Array().GetData(), CurrentIncludes.Num());
				NewCPPFile.Append(CPPFuncs.GetData(), CPPFuncs.Num());

				FFileHelper::SaveStringArrayToFile(NewCPPFile, *NewCPPDir);
				BindFunctionNames.Add(BindFunction);
			}

			else
			{
				FString DeleteDir = CPPDir + BindFunction + ".cpp";
				if (FPaths::FileExists(DeleteDir))
				{
					IFileManager& FileManager = IFileManager::Get();
					FileManager.Delete(*DeleteDir);
				}
			}
		}		
	}

	if (ModuleSet.Num() > moduleCount)
	{
		ModuleList = ModuleSet.Array();
	}

	for (FString str : BindFunctionNames)
	{
		ModuleCPP.Add("\t\t\t" + str + "();");
	}

	ModuleCPP.Add("\t\t}"); //End Lambda
	ModuleCPP.Add("\t);\n"); //End Register call

	ModuleCPP.Add("}\n"); //End Startup Def

	//Shutdown shouldn't have to do anything so can one line it
	ModuleCPP.Add(FString("void ") + ModuleClass + "::ShutdownModule()\n{\n}\n");

	for (FString str : BindFunctionNames)
	{
		Header.Add("\tstatic void " + str + "();");
	}

	Header.Add("};"); 

	FFileHelper::SaveStringArrayToFile(ModuleCPP, *ModuleDir);	
}

void FAngelscriptEditorModule::GenerateSourceFiles(FString NewModuleName, TArray<FString> IncludeList, bool bIsEditor, TArray<FString>& Header, TArray<FString>& CPPFile)
{
	//It's probably best to just do all of our bind registration inside the Module class
	//I'd also review whether we really want to keep/serialize the database dictionaries
	//or just have a far simpler array of module names be saved.
	//FAngelscriptBindDatabase& BindDB = FAngelscriptBindDatabase::Get();

	TArray<TObjectPtr<UClass>> Classes;
	
	for (FString str : IncludeList)
	{
		if (!bIsEditor)
		{
			if (TArray<TObjectPtr<UClass>>* list = FAngelscriptBinds::GetRuntimeClassDB().Find(str))
			{
				Classes += *list;
			}
		}
		else
		{
			if (TArray<TObjectPtr<UClass>>* list = FAngelscriptBinds::GetEditorClassDB().Find(str))
			{
				Classes += *list;
			}			
		}
	}
	
	if (Classes.IsEmpty())
		return;

	//Generate Header
	Header.Add("#pragma once\n");
	Header.Add("#include \"CoreMinimal.h\"");
	Header.Add("#include \"Modules/ModuleManager.h\"\n");
	
	FString ClassLine = FString("class ");
	FString API = NewModuleName.ToUpper() + "_API ";	
	//ClassLine += API;
	ClassLine += "F" + NewModuleName + "Module : public FDefaultModuleImpl";
	Header.Add(ClassLine);
	
	Header.Add("{");
	Header.Add("public:");
	Header.Add("\tvirtual void StartupModule() override;");
	Header.Add("\tvirtual void ShutdownModule() override;");
	Header.Add("};");

	//Generate CPP File
	FString ModuleClass = FString("F") + NewModuleName + "Module";	

	CPPFile.Add("#include \"" + NewModuleName + "Module.h\"");
	//CPPFile.Add("#include \"FunctionCallers.h\""); //Already referenced in binds, avoid redefinition
	CPPFile.Add("#include \"AngelscriptBinds.h\"");
	//Add Include loop here for all dependencies
	TSet<FString> CurrentIncludes = TSet<FString>();
	TArray<FString> Headers;
	TArray<FString> ModuleNames;

	for (auto Class : Classes)
	{
		FString HeaderPath = FString();
		bool headerFound = FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);		
		if (!headerFound) continue;
		
		Headers.Add(HeaderPath);
		FString ModuleName;
		FSourceCodeNavigation::FindClassModuleName(Class, ModuleName);
		ModuleNames.Add(ModuleName);

		int32 pubIndex = HeaderPath.Find("Public/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
		int32 privIndex = HeaderPath.Find("Private/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
		int32 classesIndex = HeaderPath.Find("Classes/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);

		if (pubIndex > 0)
			HeaderPath = HeaderPath.RightChop(pubIndex + 7);
		if (privIndex > 0)
			HeaderPath = HeaderPath.RightChop(privIndex + 8);
		if (classesIndex > 0)
			HeaderPath = HeaderPath.RightChop(classesIndex + 8);

		FString Include = "#include ";
		Include += '"';
		Include += HeaderPath;
		Include += '"';

		if (!CurrentIncludes.Contains(Include))
		{
			CPPFile.Add(Include);
			CurrentIncludes.Add(Include);
		}
	}

	FString MacroLine = FString("IMPLEMENT_MODULE(") + ModuleClass + ", " + NewModuleName + ");\n";
	CPPFile.Add(MacroLine);

	CPPFile.Add(FString("void ") + ModuleClass + "::StartupModule()\n{\n");
	
	CPPFile.Add("\tFAngelscriptBinds::RegisterBinds\n\t(");
	CPPFile.Add("\t\t(int32)FAngelscriptBinds::EOrder::Late,");
	CPPFile.Add("\t\t[]()");
	CPPFile.Add("\t\t{");

	for (int i = 0; i < Classes.Num(); i++)
	{		
		//GenerateFunctionEntries(Classes[i], CPPFile, Headers[i], ModuleNames[i]); //Old signature
		//CPPFile.Add("\n");
	}

	CPPFile.Add("\t\t}"); //End Lambda
	CPPFile.Add("\t);\n"); //End Register call

	CPPFile.Add("}\n"); //End Startup Def

	//Shutdown shouldn't have to do anything so can one line it
	CPPFile.Add(FString("void ") + ModuleClass + "::ShutdownModule()\n{\n}\n");
}

void FAngelscriptEditorModule::GenerateFunctionEntries(UClass* Class, TArray<FString>& File, TArray<FString>& CurrentIncludes, TSet<FString>& IncludeSet, FString HeaderPath, FString ModuleName, TSet<FString>& ModuleSet)
{
	FString ClassName = Class->GetPrefixCPP();
	ClassName += Class->GetName();

	TArray<FName> FuncNames;
	Class->GenerateFunctionList(FuncNames);
	if (FuncNames.IsEmpty()) return;

	FString Header; 
	bool isCached = false;

	if (HeaderCache.Contains(HeaderPath))
	{
		Header = HeaderCache[HeaderPath];
		isCached = true;
	}
	else
	{
		FFileHelper::LoadFileToString(Header, *HeaderPath);
	}
	
	//if (ClassName.Contains("NiagaraEffect"))
	//	UE_LOG(Angelscript, Log, TEXT("Found Target"));
	//TArray<FString> HeaderArray;
	//FFileHelper::LoadFileToStringArray(HeaderArray, *HeaderPath);
	
	auto FindAll = [&](FString& target, FString subStr, FString endSubStr, bool findNextEnd) -> TArray<TTuple<int32, int32>>	
	{		
		TArray<TTuple<int32, int32>> indices;
		int32 startIndex = -1;
		bool sameStrs = subStr == endSubStr;

		while (startIndex < target.Len())
		{				
			int32 find = target.Find(subStr, ESearchCase::CaseSensitive, ESearchDir::FromStart, startIndex);

			if (find == INDEX_NONE)
				break;

			indices.Add(TTuple<int32, int32>(find, -1));
			startIndex = find + subStr.Len();

			if (findNextEnd)
			{
				int32 nextStart = find;
				
				if (sameStrs)
					nextStart += subStr.Len();

				int32 findNext = target.Find(endSubStr, ESearchCase::CaseSensitive, ESearchDir::FromStart, nextStart);

				if (findNext != INDEX_NONE)									
					indices.Last().Value = findNext;				
			}
		}
		return indices;
	};

	auto ReplaceRange = [](FString& target, int32 start, int32 end, FString::ElementType replace)
	{		
		for (int i = start; i < end; i++)
		{
			target[i] = replace;
		}
	};

	auto FindMin = [](TArray<TTuple<int32, int32>>& list) -> int32
	{
		int32 min = 0;
		int32 index = -1;

		for (int i = 0; i < list.Num(); i++)
		{
			auto& elem = list[i];
			if (elem.Value == -1) continue;
			int32 current = elem.Value - elem.Key;

			if (min == 0)
			{
				min = current;
				index = i;
			}

			if (current < min)
			{
				min = current;
				index = i;
			}
		}

		return index;
	};

	auto FindAllN = [](FString file, TArray<FString> subStrs) -> TArray<TArray<int32>>
	{
		TArray<TArray<int32>> indices;
		int32 startIndex = -1;

		if (subStrs.IsEmpty()) return indices;

		while (startIndex < file.Len())
		{
			int32 find = file.Find(subStrs[0], ESearchCase::CaseSensitive, ESearchDir::FromStart, startIndex);
			if (find == INDEX_NONE) break;

			TArray<int32> newList;
			newList.Add(find);
			startIndex = find + subStrs[0].Len();
			int i;
			for (i = 1; i < subStrs.Num(); i++)
			{
				int32 findNext = file.Find(subStrs[i], ESearchCase::CaseSensitive, ESearchDir::FromStart, find);
				if (findNext != INDEX_NONE)
				{
					newList.Add(findNext);
					find = findNext;
				}
				else break;
			}
			indices.Add(newList);
		}
		return indices;
	};

	auto FindMinMulti = [](TArray<TArray<int32>>& list, int32 termCount) -> int32
	{
		int32 index = -1;
		for (int i = 0; i < list.Num(); i++)
		{
			TArray<int32>& current = list[i];
			int32 num = current.Num();

			if (num == termCount)
			{
				if (index == -1) index = i;

				else
				{
					TArray<int32>& min = list[index];
					int32 minDiff = min[num - 1] - min[0];
					int32 diff = current[num - 1] - current[0];
					if (diff < minDiff) index = i;
				}
			}
		}
		return index;
	};
	
	//Clean out File of Comments to prevent false positives
	if (!isCached)
	{
		TArray<TTuple<int32, int32>> comments = FindAll(Header, "//", "\n", true);
		TArray<TTuple<int32, int32>> blockComments = FindAll(Header,"/*", "*/", true);

		for (auto elem : comments)
			ReplaceRange(Header, elem.Key, elem.Value, L' ');

		for (auto elem : blockComments)
			ReplaceRange(Header, elem.Key, elem.Value + 2, L' '); //since two char

		HeaderCache.Add(HeaderPath, Header);
	}

	//Need to account for multiple classes in one file
	
	TArray<TTuple<int32, int32>> allUClass = FindAll(Header, "UCLASS(", "\n", true);
	TTuple<int32, int32> targetBound;
	bool setBound = false;

	//Find our target class and find the index range for the start and end of the class to get correct ufunctions
	for (int32 i = 0; i < allUClass.Num(); i++)
	{
		if (allUClass[i].Value == -1) continue;
		if (setBound) break;

		for (int j = allUClass[i].Value; j < Header.Len(); j++)
		{
			if (Header[j] == '{')
			{
				auto def = TTuple<int32, int32>(allUClass[i].Key, j);
				FString str = Header.Mid(def.Key, def.Value - def.Key);

				str.ReplaceCharInline(L'\n', L' ');

				if (str.Contains(" " + ClassName + " : ", ESearchCase::CaseSensitive))
				{
					if (str.Contains("Deprecated"))
					{
						UE_LOG(Angelscript, Log, TEXT("Skipping Deprecated Class"));
						return;
					}

					targetBound.Key = allUClass[i].Key;
					
					if (i + 1 < allUClass.Num())
						targetBound.Value = allUClass[i + 1].Key;
					else
						targetBound.Value = Header.Len();

					setBound = true;
				}
				//classDefs.Add(def);
				//classDefStrs.Add(str);				
				break;
			}
		}
	}
	
	TArray<TTuple<int32, int32>> allUE_Deprecs = FindAll(Header, "UE_DEPRECATED(", "\n", true);
	TArray<TTuple<int32, int32>> allUFuncs = FindAll(Header, "UFUNCTION", "\n", true);
	TArray<TTuple<int32, int32>> UFuncs;
	TArray<TTuple<int32, int32>> funcRanges;

	//for (auto& elem : allUFuncs)
	for (int32 i = 0; i < allUFuncs.Num(); i++)
	{
		if (allUFuncs[i].Value == -1) continue;

		if (setBound)
		{
			if (allUFuncs[i].Key < targetBound.Key || allUFuncs[i].Key > targetBound.Value)
			{
				UE_LOG(Angelscript, Log, TEXT("Skipping Out of Range Function"));				
				continue;
			}
		}
	
		//FString testStr = Header.Mid(elem.Key, elem.Value - elem.Key);
		FString testStr = Header.Mid(allUFuncs[i].Key, allUFuncs[i].Value - allUFuncs[i].Key);
		bool shouldContinue = false;

		//for (auto& dep : allUE_Deprecs)
		for (int j = 0; j < allUE_Deprecs.Num(); j++)
		{
			if (allUE_Deprecs[j].Key < targetBound.Key || allUE_Deprecs[j].Key > targetBound.Value)
				continue;

			if (allUE_Deprecs[j].Value != -1)
			{
				int32 test = allUFuncs[i].Key - allUE_Deprecs[j].Value;

				if (test == 2)
				{
					UE_LOG(Angelscript, Log, TEXT("Skipping Deprecated Function (before macro)"));						
					shouldContinue = true;
					break;
				}
			}
		}

		if (shouldContinue) continue;

		if (testStr.Contains("BlueprintCallable") || testStr.Contains("BlueprintPure"))
		{
			//if (!test.Contains("DEPRECATED") && !test.Contains("DeprecatedFunction"))
			if (!testStr.Contains("DEPRECATED"))
				UFuncs.Add(allUFuncs[i]);
		}
		
	}

	//TArray<FString> funcDefs;
	TArray<TTuple<FString, FString>> funcDefs;
	TArray<TCHAR> endChars;
	
	for (int i = 0; i < UFuncs.Num(); i++)
	{
		//for (int j = UFuncs[i].Value; j < UFuncs[i + 1].Key; j++)
		for (int j = UFuncs[i].Value; j < Header.Len(); j++)
		{
			if (Header[j] == ';' || Header[j] == '{')
			{
				TTuple<int32, int32> full = TTuple<int32, int32>(UFuncs[i].Key, j);
				FString str = Header.Mid(full.Key, full.Value - full.Key);
				
				TTuple<int32, int32> func = TTuple<int32, int32>(UFuncs[i].Value + 1, j);
				FString str2 = Header.Mid(func.Key, func.Value - func.Key);

				TTuple<FString, FString> entry = TTuple<FString, FString>(str, str2);
				funcDefs.Add(entry);
				endChars.Add(Header[j]);
				break;
			}
		}
	}
	
	//Start Main Loop
	for (FName FuncName : FuncNames)
	{
		UFunction* Function = Class->FindFunctionByName(FuncName);
		if (!Function) continue;

		if (!Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			continue;
								
		if (Function->HasAnyFunctionFlags(FUNC_Private | FUNC_Protected | FUNC_Static)) //Will do later
			continue;

		//if (FAngelscriptBinds::CheckForSkipEntry(Class->GetName(), Function->GetName()))
		if (FAngelscriptBinds::CheckForSkipEntry(Class->GetFName(), FuncName))
		{
			continue;
		}
		
		//if (Class->HasAnyClassFlags(CLASS_MinimalAPI) && Function->HasAnyFunctionFlags(FUNC_RequiredAPI))

		//if (Function->HasAnyFunctionFlags(FUNC_Static))
		//{
		//	ProcessStaticFunction(Class, Function, File, CurrentIncludes, IncludeSet, ModuleSet);
		//	continue;
		//}
		
		FString def = FString("");
		FString ret;
		FString name;
		FString args;
		TArray<FString> argStrs;
		TArray<FString> argNames;
		FProperty* retProp = retProp = Function->GetReturnProperty();

		if (retProp != nullptr)
		{
			if (retProp->HasAnyPropertyFlags(CPF_ConstParm))
			{				
				ret += "const ";
			}

			FString ExtendedType;
			ret += retProp->GetCPPType(&ExtendedType) + ExtendedType;

			if (retProp->HasAnyPropertyFlags(CPF_ReferenceParm))
				ret += "&";

			def += ret + " ";

			//Insert Include add check here	
			AddParameterInclude(retProp, CurrentIncludes, IncludeSet, ModuleSet);
		}

		else
		{
			ret = "void";
			def += ret + " ";
		}

		name = FuncName.ToString();		
		FString nameFix = " " + name + "(";
		def += name;

		args = "(";

		bool firstArg = true;

		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* prop = *It;

			if (prop == retProp)
				continue;

			if (prop->HasAnyPropertyFlags(CPF_ReturnParm))
				continue;

			if (firstArg)
				firstArg = false;
			else
				args += ", ";
			
			if (prop->HasAnyPropertyFlags(CPF_ConstParm))
				args += "const ";

			FString ExtendedType;
			FString type = prop->GetCPPType(&ExtendedType) + ExtendedType;// +" " + prop->GetName();
			argStrs.Add(type);
			args += type;
			
			if (prop->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_OutParm))
				args += "&";

			args += " " + prop->GetName();
			argNames.Add(prop->GetName());
			
			AddParameterInclude(prop, CurrentIncludes, IncludeSet, ModuleSet);
		}

		args += ")";

		if (Function->HasAnyFunctionFlags(FUNC_Const))
			args += " const";
				
		def += args;
		
		//Note BlueprintPure seems to imply BlueprintCallable
		FString retName = ret + " " + name;
		FString origdef;
		FString funcOnly;
		TCHAR endChar = ' ';

		//for (TTuple<FString, FString> func : funcDefs)
		for (int32 i = 0; i < funcDefs.Num(); i++)
		{
			TTuple<FString, FString> func = funcDefs[i];

			if (func.Key.Contains(nameFix)) //Case Insensitive version which is much faster
			{
				if (func.Key.Contains(nameFix, ESearchCase::CaseSensitive)) //proper check
				{
					origdef = func.Key;
					funcOnly = func.Value;
					endChar = endChars[i];
					break;
				}
			}
		}

		if (origdef.IsEmpty())
			continue;

		//Split these 3 up for debug purposes
		if (origdef.Contains("BlueprintGetter"))			  
			continue;

		if (origdef.Contains("BlueprintSetter"))
			continue;

		if (origdef.Contains("DEPRECATED") || origdef.Contains("DeprecatedFunction"))
			continue;

		//If the class is MinimalAPI, and the header doesn't have the function body or the API macro on function
		//Skip this function as it's inaccessible.
		//Will make a UFunction Call Wrapper implementation later
		if (Class->HasAnyClassFlags(CLASS_MinimalAPI) && !Function->HasAnyFunctionFlags(FUNC_RequiredAPI))
		{
			if (endChar == ';')
				continue;
		}

		int32 nameStart = funcOnly.Find(name, ESearchCase::CaseSensitive); //might need nameFix
		FString origRet = funcOnly.Mid(0, nameStart);
		
		TArray<FString> finalArgs;
		
		for (int32 i = 0; i < argNames.Num(); i++)
		{
			//FString match = argStrs[i] + " " + argNames[i];
			//auto names = FindAll(funcOnly, " " + argNames[i], "", false);
			//TTuple<int32, int32> targetIndex = TTuple<int32, int32>(-1, -1);
			int32 trueArgStart = -1;
			
			//FString arg = funcOnly.Mid(names[min].Key, names[min].Value - names[min].Key);		
			//I don't know why this is randomly dropping variables, unless we have to check from both sides
			//Or maybe we have to ensure only one of the checks writes than lock out the rest/jump to end			
			int32 argNameIndex = -1;
			int32 check1 = funcOnly.Find(" " + argNames[i] + " ", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check2 = funcOnly.Find(" " + argNames[i] + ",", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check3 = funcOnly.Find(" " + argNames[i] + ")", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check4 = funcOnly.Find(" " + argNames[i] + "(", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check5 = funcOnly.Find(" " + argNames[i] + "=", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check6 = funcOnly.Find(" " + argNames[i] + "\n", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check7 = funcOnly.Find(" " + argNames[i] + "\t", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check8 = funcOnly.Find(" " + argNames[i], ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 check9 = funcOnly.Find(argNames[i], ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		
			if (check1 != -1)
				argNameIndex = check1;
			if (check2 != -1)
				argNameIndex = check2;
			if (check3 != -1)
				argNameIndex = check3;
			if (check4 != -1)
				argNameIndex = check4;
			if (check5 != -1)
				argNameIndex = check5;
			if (check6 != -1)
				argNameIndex = check6;
			if (check7 != -1)
				argNameIndex = check7;
			if (check8 != -1 && argNameIndex == -1)
				argNameIndex = check8;
			if (check9 != -1 && argNameIndex == -1)
				argNameIndex = check9;
		
			int32 templateStack = 0;
		
			for (int32 j = argNameIndex; j > 0; j--)
			{
				if (funcOnly[j] == '>')
					templateStack++;

				if (funcOnly[j] == '<')
				{
					templateStack--;
					if (templateStack < 0)
						templateStack = 0;
				}

				if (funcOnly[j] == '(' || (funcOnly[j] == ',' && templateStack == 0) || funcOnly[j] == ')')
				{
					trueArgStart = j + 1;
					break;
				}
			}
			
			FString argType = funcOnly.Mid(trueArgStart, (argNameIndex - trueArgStart)); //might need +1				
			finalArgs.Add(argType);					
		}

		for (int i = 0; i < origRet.Len(); i++)
		{
			if (origRet[i] != '\t' && origRet[i] != '\n' && origRet[i] != ' ')
			{
				origRet = origRet.Right(origRet.Len() - i); //possibly j - 1
				break;
			}
		}

		//finalDef += ")";
		//if (Function->HasAnyFunctionFlags(FUNC_Const))
		//	finalDef += " const";
		
		origRet = origRet.Replace(L"virtual", L"", ESearchCase::CaseSensitive);		
		origRet = origRet.Replace(L"FORCEINLINE", L"", ESearchCase::CaseSensitive);
		origRet = origRet.Replace(L"FORCENOINLINE", L"", ESearchCase::CaseSensitive);

		FString line;

		if (!Function->HasAnyFunctionFlags(FUNC_Static))
		{
			line = "\t\t\tFAngelscriptBinds::AddFunctionEntry(" + ClassName + "::StaticClass(), \"" + name + "\"";
			line += ", { ERASE_METHOD_PTR(";
			line += ClassName + ", ";
			line += name + ", (";
		}
		
		else
		{
			origRet = origRet.Replace(L"static", L"", ESearchCase::CaseSensitive);

			line = "\t\t\tFAngelscriptBinds::AddFunctionEntry(" + ClassName + "::StaticClass(), \"" + name + "\"";
			line += ", { ERASE_FUNCTION_PTR(";
			//line += ClassName + ", ";
			//line += name + ", (";
			line += ClassName + "::" + name + ", (";
		}
		
		

		bool firstArg2 = true;
		for (FString type : finalArgs)
		{			
			if (firstArg2)
				firstArg2 = false;
			else
				line += ", ";
			
			line += type;
		}

		line += ")";

		TTuple<int32, int32> constChecks(-1, -1);

		if (Function->HasAnyFunctionFlags(FUNC_Const)) 
		{
			//Apparently the Const flag cannot be 100% trusted, so we do this check to ensure
			int32 parenCheck = funcOnly.Find(")", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			int32 constFind = funcOnly.Find("const", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (parenCheck != -1 && constFind != -1)
			{
				constChecks.Key = parenCheck;
				constChecks.Value = constFind;
				if (constFind >= parenCheck)
				{
					//UE_LOG(Angelscript, Log, TEXT("CHECK THIS"));
					line += " const";
				}
			}
		}		
		//else
		//{
		//	int32 parenCheck = funcOnly.Find(")", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		//	int32 constFind = funcOnly.Find("const", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		//	if (parenCheck != -1 && constFind != -1)
		//	{
		//		if (constFind > parenCheck)
		//		{
		//			UE_LOG(Angelscript, Log, TEXT("CHECK THIS"));
		//			line += " const";
		//		}
		//	}
		//}
		
		//auto apiCheck = FindAll(finalRet, "(", "_API " + origRet, true);
		//for (auto& elem : apiCheck)
		//{
		//	if (elem.Key != -1 && elem.Value != -1)
		//		finalRet.RemoveAt(elem.Key + 1, (elem.Value - (elem.Key + 1)), false);
		//}
		
		int32 apiCheck = origRet.Find("_API ", ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		if (apiCheck != -1)
		{
			origRet = origRet.Right((origRet.Len() - apiCheck) - 4);
		}

		line += ", ERASE_ARGUMENT_PACK(";
		line += origRet + ")) } );";

		//line += finalRet;
		//Add some part here to look for a UPARAM then find the next ( then ) to delete all of that

		TArray<TTuple<int32, int32>> UPars = FindAll(line, "UPARAM", ")", true);
		
		for (auto& elem : UPars)
		{
			if (elem.Key != -1 && elem.Value != -1)
				line.RemoveAt(elem.Key, (elem.Value - elem.Key) + 1, EAllowShrinking::Yes);
		}

		File.Add(line);
	}	
}

void ProcessStaticFunction(UClass* Class, UFunction* Function, TArray<FString>& lines, TArray<FString>& IncludeList, TSet<FString>& IncludeSet, TSet<FString>& ModuleSet)
{

}

void AddParameterInclude(FProperty* prop, TArray<FString>& IncludeList, TSet<FString>& IncludeSet, TSet<FString>& ModuleSet)
{
	FString NewHeader;
	UField* field = nullptr;

	if (prop->IsA<FArrayProperty>())
	{
		FArrayProperty* newProp = (FArrayProperty*)prop;
		AddParameterInclude(newProp->Inner, IncludeList, IncludeSet, ModuleSet);
	}

	if (prop->IsA<FSetProperty>())
	{
		FSetProperty* newProp = (FSetProperty*)prop;
		AddParameterInclude(newProp->ElementProp, IncludeList, IncludeSet, ModuleSet);
	}

	if (prop->IsA<FMapProperty>())
	{
		FMapProperty* newProp = (FMapProperty*)prop;
		AddParameterInclude(newProp->KeyProp, IncludeList, IncludeSet, ModuleSet);
		AddParameterInclude(newProp->ValueProp, IncludeList, IncludeSet, ModuleSet);
	}

	if (prop->IsA<FEnumProperty>())
	{
		FEnumProperty* newProp = (FEnumProperty*)prop;
		UEnum* real = newProp->GetEnum();		
		field = (UField*)real;
		FSourceCodeNavigation::FindClassHeaderPath(real, NewHeader);
	}

	if (prop->IsA<FClassProperty>())
	{		
		FClassProperty* newProp = (FClassProperty*)prop;		
		//FString type = newProp->GetCPPType(&ext, 0);
		UClass* real = newProp->MetaClass;		
		field = (UField*)real;
		FSourceCodeNavigation::FindClassHeaderPath(real, NewHeader);
	}

	if (prop->IsA<FObjectProperty>())
	{
		FObjectProperty* newProp = (FObjectProperty*)prop;
		UClass* real = newProp->PropertyClass;
		//FString type = real->GetName();		
		field = (UField*)real;
		FSourceCodeNavigation::FindClassHeaderPath(real, NewHeader);
	}

	if (prop->IsA<FStructProperty>())
	{
		FStructProperty* newProp = (FStructProperty*)prop;
		UScriptStruct* real = newProp->Struct;		
		field = (UField*)real;
		FSourceCodeNavigation::FindClassHeaderPath(real, NewHeader);
	}

	
		
	if (!NewHeader.IsEmpty())
	{
		FString ModuleName;
		field->GetPackage()->GetName(ModuleName);
		FString NewInclude = FAngelscriptEditorModule::GetIncludeForModule(field, NewHeader);

		if (!IncludeSet.Contains(NewInclude))
		{			
			IncludeList.Add(NewInclude);
			IncludeSet.Add(NewInclude);
		}

		if (!ModuleSet.Contains(ModuleName))
		{
			ModuleSet.Add(ModuleName);
		}
	}
}

void FAngelscriptEditorModule::GeneratePluginDirectory(FString PluginName, TArray<FString>& PluginFile, TArray<FString> ModuleNames)
{
	TArray<FString>& Lines = PluginFile;
	Lines.Add("{");
	Lines.Add("\t\"FileVersion\": 1,");
	Lines.Add("\t\"Version\": 1,");
	Lines.Add("\t\"VersionName\": \"1.0\",");
	Lines.Add("\t\"FriendlyName\": " + PluginName + ",");
	Lines.Add("\t\"Description\": \"Auto Generated Binds for Angelscript Standalone Plugin\",");
	Lines.Add("\t\"Category\": \"Scripting\",");
	Lines.Add("\t\"CreatedBy\": \"An Author\",");
	Lines.Add("\t\"CreatedByURL\": \"\",");
	Lines.Add("\t\"DocsURL\": \"\",");
	Lines.Add("\t\"MarketplaceURL\": \"\",");
	Lines.Add("\t\"SupportURL\": \"\",");
	Lines.Add("\t\"EnabledByDefault\": true,");
	Lines.Add("\t\"CanContainContent\": false,");
	Lines.Add("\t\"IsBetaVersion\": false,");
	Lines.Add("\t\"Installed\": false,");

	Lines.Add("\t\"Modules\":");
	Lines.Add("\t[");

	for (int i = 0; i < ModuleNames.Num(); i++)
	{
		Lines.Add("\t\t{");
		Lines.Add("\t\t\t\"Name\": \"" + ModuleNames[i] + "\",");
		Lines.Add("\t\t\t\"Type\": \"Runtime\",");
		Lines.Add("\t\t\t\"LoadingPhase\": \"PostDefault\"");

		if (i < ModuleNames.Num() - 1)
			Lines.Add("\t\t},");
		else
			Lines.Add("\t\t}");
	}

	Lines.Add("\t]"); //End Modules
	Lines.Add("}"); //End File Brace
}

void MultiApproach(FString ClassName, FString Header, FString HeaderPath, TArray<FString>& File)
{
	//Can Ignore if classBounds.Num == 1
	//int32 classDecl; 
	//bool setBound = false;
	//TTuple<int32, int32> targetBound;
	//TArray<FString> terms{ "UCLASS", "class ", ClassName, ":", "public"};
	//auto results = FindAllN(Header, terms);
	//auto min = FindMinMulti(results, terms.Num());

	//TArray<TTuple<int32, int32>> classBounds = FindAll(Header, "UCLASS", "UCLASS", true);
	//
	//for (int i = 0; i < classBounds.Num(); i++)
	//{		
	//	int32 find = Header.Find(" " + ClassName + " : ", ESearchCase::CaseSensitive, ESearchDir::FromStart, classBounds[i].Key);
	//	int32 find2 = Header.Find("class " + ClassName + " : ", ESearchCase::CaseSensitive, ESearchDir::FromStart, classBounds[i].Key);
	//	if (find != -1)
	//	{
	//		if (classBounds[i].Value != -1)
	//		{
	//			if (find > classBounds[i].Key && find < classBounds[i].Value)
	//			{
	//				classDecl = find;
	//				targetBound = classBounds[i];
	//				setBound = true;
	//				break;
	//			}
	//		}
	//
	//		else
	//		{
	//			if (find > classBounds[i].Key && find < Header.Len())
	//			{
	//				classDecl = find;
	//				targetBound = classBounds[i];
	//				setBound = true;
	//				break;
	//			}
	//		}
	//	}
	//}

	//TArray<TTuple<int32, int32>> classDefs;
	//TArray<FString> classDefStrs;

	//FString BuildFilename = ModuleName + ".Build.cs";
	//FString HeaderFilename = ModuleName + "_Module.h";
	//FString CPPFilename = ModuleName + "_Module.cpp";
	//FText fail, fail2, fail3;
	//GameProjectUtils::GeneratePluginModuleBuildFile(BuildFilename, ModuleName, PublicDepends, ModuleList, fail);

	//TArray<FString> IncludeList { "CoreMinimal.h", "Modules/ModuleManager.h" };
	//GameProjectUtils::GeneratePluginModuleHeaderFile(HeaderFilename, IncludeList, fail2);
	//FString Startup = FString();
	//GameProjectUtils::GeneratePluginModuleCPPFile(CPPFilename, ModuleName, Startup, fail3);
	//FSourceCodeNavigation::FindModulePath(BindModule, BaseDir);

	//Catch any UE_DEPRECATED here
	//if (str.Contains("Deprecated"))
	//{
	//	UE_LOG(Angelscript, Log, TEXT("Skipping Deprecated Function"));
	//	break;
	//	//continue; 
	//}

	//bool shouldBreak = false;
	//
	//for (auto& dep : allUE_Deprecs)
	//{
	//	if (dep.Value != -1)
	//	{
	//		int32 test = UFuncs[i].Key - dep.Value;
	//
	//		if (test <= 1)
	//		{
	//			UE_LOG(Angelscript, Log, TEXT("Skipping Deprecated Function (before macro)"));
	//			shouldBreak = true;
	//			break;
	//		}
	//	}
	//}
	//
	//if (shouldBreak) break;

	//UE_DEPRECATED can also appear one line before the UFUNCTION macro
	//int32 beforeMacro = Header.Find("UE_DEPRECATED(", ESearchCase::CaseSensitive, ESearchDir::FromEnd, full.Key);
	//if (beforeMacro != -1)
	//{
	//	FString check = Header.Mid(beforeMacro, full.Key - beforeMacro);
	//	auto count = FindAll(check, "\n", "", false);
	//	//int32 sameUFunc = Header.Find("UFUNCTION(", ESearchCase::CaseSensitive, ESearchDir::FromStart, beforeMacro);
	//	//if (sameUFunc != -1 && sameUFunc == full.Key)
	//	//FString check = Header.Mid(beforeMacro, full.Value - beforeMacro);
	//	if (count.Num() <= 1)
	//	{
	//		UE_LOG(Angelscript, Log, TEXT("Skipping Deprecated Function (before macro)"));
	//		break;
	//		//continue;
	//	}
	//}

	auto FindAllN = [](FString file, TArray<FString> subStrs) -> TArray<TArray<int32>>
	{
		TArray<TArray<int32>> indices;
		int32 startIndex = -1;

		if (subStrs.IsEmpty()) return indices;

		while (startIndex < file.Len())
		{
			int32 find = file.Find(subStrs[0], ESearchCase::CaseSensitive, ESearchDir::FromStart, startIndex);

			if (find == INDEX_NONE)
				break;

			TArray<int32> newList;
			newList.Add(find);
			startIndex = find + subStrs[0].Len();

			int i;
			for (i = 1; i < subStrs.Num(); i++)
			{
				int32 findNext = file.Find(subStrs[i], ESearchCase::CaseSensitive, ESearchDir::FromStart, find);

				if (findNext != INDEX_NONE)
				{
					newList.Add(findNext);
					find = findNext;
				}

				else
				{
					break;
				}
			}

			indices.Add(newList);

			//if (i >= subStrs.Num()) 
			//	i = subStrs.Num() - 1;
			//startIndex = find + subStrs[i].Len();
		}

		return indices;
	};

	auto FindMinMulti = [](TArray<TArray<int32>>& list, int32 termCount) -> int32
	{
		int32 index = -1;
		for (int i = 0; i < list.Num(); i++)
		{
			TArray<int32>& current = list[i];
			int32 num = current.Num();

			if (num == termCount)
			{
				if (index == -1)
					index = i;

				else
				{
					TArray<int32>& min = list[index];
					int32 minDiff = min[num - 1] - min[0];
					int32 diff = current[num - 1] - current[0];

					if (diff < minDiff)
						index = i;
				}
			}
		}

		return index;
	};

	FString def;
	FString targetDef;
	FString ret, name;

	TArray<FString> terms{ "UFUNCTION", ret, name, ";" };
	TArray<FString> terms2{ "UFUNCTION", ret, name, "{" };
	TArray<TArray<int32>> semicolon = FindAllN(Header, terms);
	TArray<TArray<int32>> bracket = FindAllN(Header, terms2);

	int32 minSemi = FindMinMulti(semicolon, terms.Num());
	int32 minBracket = FindMinMulti(bracket, terms2.Num());
	TTuple<int32, int32> semiRange;
	TTuple<int32, int32> bracketRange;
	TTuple<int32, int32> finalRange;
	int32 semiDiff = -1;
	int32 bracketDiff = -1;

	if (minSemi != -1)
	{
		semiDiff = semicolon[minSemi].Last() - semicolon[minSemi][0];
		semiRange.Key = semicolon[minSemi][0];
		semiRange.Value = semicolon[minSemi].Last();
	}

	if (minBracket != -1)
	{
		bracketDiff = bracket[minBracket].Last() - bracket[minBracket][0];
		bracketRange.Key = bracket[minBracket][0];
		bracketRange.Value = bracket[minBracket].Last();
	}

	if (semiDiff != -1 && bracketDiff != -1)
	{
		if (semiRange < bracketRange)
			finalRange = semiRange;
		if (bracketRange < semiRange) //was just an else before
			finalRange = bracketRange;
	}

	if (semiDiff != -1 && bracketDiff == -1)
		finalRange = semiRange;

	if (semiDiff == -1 && bracketDiff != -1)
		finalRange = bracketRange;

	targetDef = Header.Mid(finalRange.Key, (finalRange.Value - finalRange.Key));
	//int32 newline = targetDef.Find("\n", ESearchCase::IgnoreCase, ESearchDir::FromStart);
	//int32 nameIndex = targetDef.Find(name, ESearchCase::CaseSensitive, ESearchDir::FromStart, newline);
	//int32 endUFunc = targetDef.Find(")", ESearchCase::IgnoreCase, ESearchDir::FromEnd, nameIndex);
	//int32 semiCheck = targetDef.Find(";", ESearchCase::IgnoreCase, ESearchDir::FromStart, nameIndex);
	//int32 braceCheck = targetDef.Find("{", ESearchCase::IgnoreCase, ESearchDir::FromStart, nameIndex);

	//int32 finalIndex = -1;
	//
	//if (semiCheck != -1 && braceCheck != -1)
	//{
	//	if (semiCheck < braceCheck)
	//		finalIndex = semiCheck;
	//
	//	if (braceCheck < semiCheck)
	//		finalIndex = braceCheck;
	//}
	//
	//if (semiCheck != -1 && braceCheck == -1)
	//	finalIndex = semiCheck;
	//
	//if (semiCheck == -1 && braceCheck != -1)
	//	finalIndex = braceCheck;
	//
	//FString funcDef;
	//
	//if (finalIndex != -1)
	//	funcDef = targetDef.Mid(endUFunc + 1, finalIndex - (endUFunc + 1));
	//else
	//	funcDef = targetDef.Mid(endUFunc + 1);

	//funcDef = funcDef.Replace(TEXT("\n"), TEXT(" "), ESearchCase::IgnoreCase);
	//funcDef = funcDef.Replace(TEXT("\t"), TEXT(" "), ESearchCase::IgnoreCase);

	//FString line = "class " + ClassName + " " + name + ": " + " path: " + HeaderPath + funcDef;
	FString line = "class " + ClassName + " " + name + ": " + " path: " + HeaderPath + "\n" + targetDef;
	File.Add(line + "\n" + def + "\n");	

	//Additional Old Pruning
	//bool foundTarget = false;
	//for (auto& elem : names)
	//{
	//	if (elem.Key == -1) continue;
	//
	//	for (int32 j = elem.Key; j > 0; j--)
	//	{
	//		if (funcOnly[j] == '(' || funcOnly[j] == ',' || funcOnly[j] == ')')
	//		{						
	//			for (int32 k = elem.Key; k < funcOnly.Len(); k++)
	//			{
	//				if (k - elem.Key >= argNames[i].Len()) 
	//				{
	//					if (funcOnly[k] == '(' || funcOnly[k] == ',' || funcOnly[k] == ')' || funcOnly[k] == ' ')
	//					{
	//						foundTarget = true;
	//						trueArgStart = j + 1;
	//						targetIndex = elem;
	//						break;
	//					}
	//
	//					else
	//					{
	//						break;
	//					}
	//				}
	//			}
	//
	//			if (foundTarget) break;						
	//		}
	//	}
	//
	//	if (foundTarget) break;
	//}

	//for (FString arg : argNames)
	//{
	//	//add space to avoid any substrings in other parts e.g. Obj in UObject*
	//	int32 argName = funcOnly.Find(" " + arg, ESearchCase::CaseSensitive, ESearchDir::FromEnd);			
	//	int32 trueArgStart = -1;
	//	for (int i = argName; i > 0; i--)
	//	{
	//		if (funcOnly[i] == '(' || funcOnly[i] == ',' || funcOnly[i] == ')')
	//		{
	//			trueArgStart = i + 1;
	//			break;
	//		}
	//	}
	//	
	//	FString argType = funcOnly.Mid(trueArgStart, (argName - trueArgStart)); //might need +1			
	//	finalArgs.Add(argType);
	//}

	//FString finalDef = origRet + " " + name + "(";
	//FString finalDef = origRet + name + "(";

	//Attempt at whitespace cleanup
	//for (int i = 0; i < finalArgs.Num(); i++)
	//{
	//	//finalArgs[i] = finalArgs[i].Replace(L" ", L"");
	//	//finalArgs[i] = finalArgs[i].Replace(L"\t", L"");
	//	//finalArgs[i] = finalArgs[i].Replace(L"\n", L"");
	//	//finalArgs[i].ReplaceCharInline(L'\n', L' ');
	//
	//	for (int j = 0; j < finalArgs[i].Len(); j++)
	//	{
	//		//if (finalArgs[i][j] != '\t' && finalArgs[i][j] != '\n' && finalArgs[i][j] != ' ')
	//		if (finalArgs[i][j] != L'\t' && finalArgs[i][j] != L'\n' && finalArgs[i][j] != L' ')
	//		{
	//			finalArgs[i] = finalArgs[i].Right(finalArgs[i].Len() - j); //possibly j - 1
	//			break;
	//		}
	//	}
	//}
}

void SingleApproach(FString Header)
{
	/*
	auto FindMin = [](TArray<TTuple<int32, int32>>& list) -> int32
	{
		int32 min = 0;
		int32 index = -1;

		for (int i = 0; i < list.Num(); i++)
		{
			auto& elem = list[i];
			if (elem.Value == -1) continue;
			int32 current = elem.Value - elem.Key;

			if (min == 0)
			{
				min = current;
				index = i;
			}

			if (current < min)
			{
				min = current;
				index = i;
			}
		}

		return index;
	};
	
	//See Find All in Main Generate
	FString retName, ret, name;

	TArray<TTuple<int32, int32>> retNameToSemi = FindAll(retName, ";", true);
	TArray<TTuple<int32, int32>> retNameToBracket = FindAll(retName, "{", true);
	TArray<TTuple<int32, int32>> macroToRetName = FindAll("UFUNCTION", retName, true);
	TArray<TTuple<int32, int32>> macroToName = FindAll("UFUNCTION", name, true);
	TArray<TTuple<int32, int32>> macroToRet = FindAll("UFUNCTION", ret, true);
	TArray<TTuple<int32, int32>> macroToSemi = FindAll("UFUNCTION", retName, true);
	TArray<TTuple<int32, int32>> macroToBracket = FindAll("UFUNCTION", retName, true);

	TArray<TTuple<int32, int32>> macros = FindAll("UFUNCTION", name, true);
	//Could search for lowest distance via subtracting Value - Key

	//Could use a map or a set of some kind to work out the same target
	//TArray<int32> minDists;

	TTuple<int32, int32> macroNameRange;		
	TTuple<int32, int32> semiRange;
	TTuple<int32, int32> bracketRange;
	TTuple<int32, int32> finalRange = TTuple<int32, int32>(-1, -1);

	FString macroToNameStr;
	FString retNameToEndSemi;
	FString retNameToEndBracket;
	FString targetDef;
	
	int32 index = FindMin(macroToRetName);
	if (index != -1)
	{
		macroNameRange = macroToRetName[index];
		macroToNameStr = Header.Mid(macroNameRange.Key, (macroNameRange.Value - macroNameRange.Key));
	}
	
	index = FindMin(retNameToSemi);
	if (index != -1)
	{
		semiRange = retNameToSemi[index];
		finalRange = semiRange;
		retNameToEndSemi = Header.Mid(semiRange.Key, (semiRange.Value - semiRange.Key));
		//targetDef = retNameToEndSemi;
	}
	
	index = FindMin(retNameToBracket);
	if (index != -1)
	{
		bracketRange = retNameToBracket[index];
	
		if (finalRange.Key == -1)
			finalRange = bracketRange;
	
		if (finalRange.Key != -1 && (bracketRange.Value - bracketRange.Key) < (finalRange.Value - finalRange.Key))
			finalRange = bracketRange;
	
		retNameToEndBracket = Header.Mid(bracketRange.Key, (bracketRange.Value - bracketRange.Key));
		//if (!retNameToEndBracket.IsEmpty() && retNameToEndBracket.Len() < retNameToEndSemi.Len())
		//	targetDef = retNameToEndBracket;
	}

	finalRange.Key = macroNameRange.Key;
	targetDef = Header.Mid(finalRange.Key, (finalRange.Value - finalRange.Key));
	*/
}

void FAngelscriptEditorModule::GenerateFunctionEntriesOld2(UClass* Class, TArray<FString>& File, FString HeaderPath, FString ModuleName)
{
	FString ClassName = Class->GetPrefixCPP();
	ClassName += Class->GetName();
	
	TArray<FName> FuncNames;
	Class->GenerateFunctionList(FuncNames);
	if (FuncNames.IsEmpty()) return;
	
	FString Header;	
	//FFileHelper::LoadFileToString(Header, *HeaderPath);
	TArray<FString> HeaderArray;
	FFileHelper::LoadFileToStringArray(HeaderArray, *HeaderPath);
	
	for (FString str : HeaderArray)
	{
		Header += str;
	}


	TArray<int32> UFuncMacros;
	TArray<int32> lineIndices;
	//int32 startIndex = -1;	
	//while (startIndex < Header.Len())
	//{
	//	FString subStr = "UFUNCTION";
	//	int32 find = Header.Find(subStr, ESearchCase::CaseSensitive, ESearchDir::FromStart, startIndex);
	//
	//	if (find == INDEX_NONE)
	//		break;
	//
	//	UFuncMacros.Add(find);
	//	startIndex = find + subStr.Len();
	//}

	for (int i = 0; i < HeaderArray.Num(); i++)
	{
		int32 find = HeaderArray[i].Find("UFUNCTION", ESearchCase::CaseSensitive);
		
		if (find != INDEX_NONE)
		{
			//Might have to check from both directions
			int32 comment = HeaderArray[i].Find("//");
			int32 blockOpen = HeaderArray[i].Find("/*");
			int32 blockClose = HeaderArray[i].Find("*/");

			//This UFUNCTION is in a comment
			if (comment > -1 && comment < find) 
				continue;

			//If Block comment open less then find, and close is not found or has a greater index than find fail
			if (blockOpen > -1 && blockOpen < find && (blockClose == -1 || (blockClose > -1 && blockClose > find)))
				continue;

			//Mostly the same as above, but extra insurance
			if (blockClose > -1 && blockClose > find && (blockOpen == -1) || (blockOpen > -1 && blockOpen < find))
				continue;

			UFuncMacros.Add(find);
			lineIndices.Add(i);
		}
	}

	TMap<UFunction*, TArray<int32>> FuncMap;
	
	for (FName Name : FuncNames)
	{
		if (UFunction* Function = Class->FindFunctionByName(Name))
		{
			if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) && !Function->HasAnyFunctionFlags(FUNC_Static))
			{
				FString str = Name.ToString() + "(";

				for (int line : lineIndices)
				{					
					int32 index = HeaderArray[line + 1].Find(str, ESearchCase::CaseSensitive, ESearchDir::FromStart);
					if (index > -1)
					{
						int32 fullIndex = 0;
						for (int i = 0; i < line + 1; i++)
						{
							fullIndex += HeaderArray[i].Len();
						}
						//would there be a +1?
						FuncMap.Add(Function, TArray<int32>()).Add(fullIndex + index);
						break;
					}
				}
			}
		}
	}

	/*
		Using the index we obtain, need to find the semicolon or {
		Alternatively could try using a char stack to track opens and closes
		Should just aim to copy the header file once the true start and end have been found
		from there, we can use the UFunction args to remove the arg names, and using the return type
		uproperty should yield the true start, I don't think there can be const returns but should check

		As a general reminder, I think all we really need to do is find the potentially missing
		const, class(fwd-decl), &, * 
		
		Ultimately we probably can run with what we originally had we just need better solutions
		for then finding the UFunction generated version and peeking for the extra characters around 
		the target terms

		Maybe we do just need to use the TArray lines approach or possibly both

	*/

	for (auto& elem : FuncMap)
	{
		//name start in array at index 0
		int32 startArgs = -1;
		int32 endArgs = -1;
		int32 retStart = -1;
		int32 retEnd = -1;
		int32 endFunc = -1; //Can be either ; or { this needs to include the func_const as well	

		//Find the start of parameters and the definitive end of the function declaration
		for (int32 i = elem.Value[0]; i < Header.Len(); i++)
		{
			if (startArgs != -1 && endFunc != -1)
				break;

			if (startArgs == -1 && Header[i] == '(')
				startArgs = i;

			if (endFunc == -1 && Header[i] == ';')
				endFunc = i;			

			if (endFunc == -1 && Header[i] == '{')
				endFunc = i;
		}

		//Now find end of parameters - should be the first one you find starting from end of declaration
		for (int32 i = endFunc; i > 0; i--)
		{
			if (endArgs == -1 && Header[i] == ')')
			{
				endArgs = i;
				break;
			}
		}

		//I really might have to find the ) on the UFUNCTION macro

		//Find RetStart
		if (FProperty* retProp = elem.Key->GetReturnProperty())
		{
			FString ExtendedType;
			FString type = retProp->GetCPPType(&ExtendedType) + ExtendedType;
			
			if (retProp->HasAnyPropertyFlags(CPF_ConstParm))
				retStart = Header.Find("const", ESearchCase::CaseSensitive, ESearchDir::FromEnd, elem.Value[0]);
			else
				retStart = Header.Find(type, ESearchCase::CaseSensitive, ESearchDir::FromEnd, elem.Value[0]);	
		}
		else
		{
			retStart = Header.Find("void", ESearchCase::CaseSensitive, ESearchDir::FromEnd, elem.Value[0]);
		}

		//Find the End, to be absolutely certain that any * or & are not missed
		for (int i = elem.Value[0]; i > retStart; i--)
		{
			if (Header[i] != ' ' && i < elem.Value[0])
			{
				retEnd = i;
				break;
			}
		}

		//elem funcNameStart
		FuncMap[elem.Key].Add(startArgs);
		FuncMap[elem.Key].Add(endArgs);
		FuncMap[elem.Key].Add(retStart);
		FuncMap[elem.Key].Add(retEnd);
		FuncMap[elem.Key].Add(endFunc);
	}

	for (auto& elem : FuncMap)
	{
		FString line = "\t\t\tFAngelscriptBinds::AddFunctionEntry(" + ClassName + "::StaticClass(), \"" + elem.Key->GetName() + "\"";
		line += ", { ERASE_METHOD_PTR(";

		int32 funcName = 0;
		int32 startArgs = 1;
		int32 endArgs = 2;
		int32 retStart = 3;
		int32 retEnd = 4;
		int32 endFunc = 5;
		UFunction* Function = elem.Key;

		line += ClassName + ", ";
		line += elem.Key->GetName() + ", ";
		//line += "(";
				
		//FString args = Header.Mid(elem.Value[startArgs], (elem.Value[endArgs] - elem.Value[startArgs]) + 1);
		FString args = FString("(");
		bool firstArg = true;

		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* prop = *It;

			if (firstArg)
				firstArg = false;
			else
				args += ", ";

			args += prop->GetCPPType() + " " + prop->GetName();			

			//args = args.Replace(*prop->GetName(), L"", ESearchCase::CaseSensitive);
			//Can find argName then keep searching left until you find a , or (
			//however that doesn't account for default values
		}

		args += ")";

		if (Function->HasAnyFunctionFlags(FUNC_Const))
			args += "const";

		line += args + ", ";

		FString ret = "ERASE_ARGUMENT_PACK(";
		FString type = Header.Mid(elem.Value[retStart], (elem.Value[retEnd] - elem.Value[retStart]) + 1);
		line += ret + type + ")) } );";
		File.Add(line);
	}
}

void FAngelscriptEditorModule::GenerateFunctionEntriesOld(UClass* Class, TArray<FString>& File, FString HeaderPath, FString ModuleName)
{
	FString ClassName = Class->GetPrefixCPP();
	ClassName += Class->GetName();

	TArray<FName> FuncNames;
	Class->GenerateFunctionList(FuncNames);
	if (FuncNames.IsEmpty()) return;
	
	//FString HeaderPath;
	//TArray<FString> HeaderFile;
	//FString HeaderFile;
	//bool HeaderFound = FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);
	//if (HeaderFound)
	//{
	//	//FFileHelper::LoadFileToStringArray(HeaderFile, *HeaderPath);
	//	FFileHelper::LoadFileToString(HeaderFile, *HeaderPath);
	//}

	//ModuleName = FPaths::GetBaseFilename(ModuleName);
	for (FName Name : FuncNames)
	{
		UFunction* Function = Class->FindFunctionByName(Name);
		//TSharedPtr<FAngelscriptFunctionDesc> funcDesc = classDesc->GetMethodByScriptName(Name.ToString());
		
		if (Function != nullptr && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			if (Function->HasAnyFunctionFlags(FUNC_Static)) //TO-DO: Add support for statics I think
				continue;
			
			FString line = "\t\t\tFAngelscriptBinds::AddFunctionEntry(" + ClassName + "::StaticClass(), \"" + Name.ToString() + "\"";
			line += ", { ERASE_METHOD_PTR(";

			line += ClassName + ", ";
			line += Name.ToString() + ", ";
			line += "(";

			FProperty* RetParm = nullptr;
			bool firstArg = true;
			
			//const FString SymbolName = FString::Printf(TEXT("%s%s::%s"), Class->GetPrefixCPP(), *Class->GetName(), *Function->GetFullName());
			//FString FileName = FString("something");
			//uint32 LineNum = 0;
			//bool FoundLine = FindFunctionDefinitionLine(SymbolName, ModuleName, LineNum, FileName);

			//if (FileName == HeaderPath)
			//	UE_LOG(Angelscript, Log, TEXT("Found Function definition file is header file"));
			
			//if (int32 index = HeaderFile.Find(Name.ToString(), ESearchCase::CaseSensitive))
			//{
			//
			//}

			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				FProperty* prop = *It;
				

				//TO-DO: Use the HeaderFile TArray somewhere here to compare and properly check the func def

				if (prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					RetParm = prop;
					continue;
				}

				if (firstArg)
					firstArg = false;
				else
					line += ", ";

				if (prop->HasAnyPropertyFlags(CPF_ConstParm))
					line += "const ";

				FString ExtendedType;
				//FString type = prop->GetCPPType(&ExtendedType, CPPF_Implementation | CPPF_ArgumentOrReturnValue);
				FString type = prop->GetCPPType(&ExtendedType);
				FString fullType = type + ExtendedType;
				line += fullType;

				if (prop->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_OutParm) && !prop->HasAnyPropertyFlags(CPF_NonNullable) && !fullType.EndsWith("*") && !FChar::IsLower(fullType[0]))
					line += "&";
			}

			line += ")";

			if (Function->HasAnyFunctionFlags(FUNC_Const))
				line += "const";

			line += ", ";

			if (RetParm != nullptr)
			{
				line += "ERASE_ARGUMENT_PACK(";

				if (RetParm->HasAnyPropertyFlags(CPF_ConstParm))
					line += "const ";
				
				FString ExtendedType;
				FString type = RetParm->GetCPPType(&ExtendedType);
				//FString type = RetParm->GetCPPType(&ExtendedType, CPPF_Implementation | CPPF_ArgumentOrReturnValue);
				FString fullType = type + ExtendedType;
				line += fullType;

				//if (RetParm->HasAnyPropertyFlags(CPF_ReferenceParm))
				if (RetParm->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_OutParm) && !fullType.EndsWith("*") && !FChar::IsLower(fullType[0]))
					line += "&";

				line += ")";
			}

			else
			{
				line += "ERASE_ARGUMENT_PACK(void)";
			}

			line += ") } );";				

			File.Add(line);
		}
	} //END FUNCTIONS LOOP	
}

void PrintMetaData(FProperty* prop)
{
	TArray<FString> MyLog;
	if (UField* Field = prop->GetOwnerUField())
	{
		if (UPackage* Package = Field->GetOutermost())
		{
			FMetaData& Meta = Package->GetMetaData();
			if (TMap<FName, FString>* ObjectValues = Meta.ObjectMetaDataMap.Find(Field))
			{
				FString LogStr;
				TArray<FName> Keys;
				ObjectValues->GetKeys(Keys);

				for (auto key : Keys)
				{
					LogStr += key.ToString() + ": " + *ObjectValues->Find(key) + ", ";
				}

				MyLog.Add(LogStr);
			}
		}
	}
}

//Originally from SourceCodeNavigation.cpp line 461
bool FORCENOINLINE FAngelscriptEditorModule::FindFunctionDefinitionLine(const FString& FunctionSymbolName, const FString& FunctionModuleName, uint32& OutLineNumber, FString& OutSourceFile)
{
	//ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
	//ISourceCodeAccessor& SourceCodeAccessor = SourceCodeAccessModule.GetAccessor();
	IModuleInterface* Interface = FModuleManager::Get().LoadModule("SourceCodeAccess");

	if (Interface)
		UE_LOG(Angelscript, Log, TEXT("Source Code Access Loaded Successfully"));

#if PLATFORM_WINDOWS
	FString SourceFileName = FString("something");
	//uint32 SourceLineNumber = 1;
	uint32 SourceColumnNumber = 0;
	if (FPlatformStackWalk::GetFunctionDefinitionLocation(FunctionSymbolName, FunctionModuleName, OutSourceFile, OutLineNumber, SourceColumnNumber))
	{		
		if (FPaths::FileExists(SourceFileName))
		{
			OutSourceFile = SourceFileName;
			return true;
		}
	}

#elif PLATFORM_MAC //I won't be able to test this, but tried my best to make it work for Mac

	int32 ReturnCode = 0;
	FString Results;
	FString Errors;
	for (uint32 Index = 0; Index < _dyld_image_count(); Index++)
	{
		char const* IndexName = _dyld_get_image_name(Index);
		FString FullModulePath(IndexName);
		FString Name = FPaths::GetBaseFilename(FullModulePath);
		if (Name == FunctionModuleName)
		{
			FString FullDsymPath = FPaths::ChangeExtension(FullModulePath, TEXT("dsym"));
			const FString SourceCodeLookupCommand = FString::Printf(TEXT("%sBuild/BatchFiles/Mac/SourceCodeLookup.sh \"%s\" \"%s\" \"%s\""), *FPaths::EngineDir(), *FunctionSymbolName, *FullModulePath, *FullDsymPath);
			FPlatformProcess::ExecProcess(TEXT("/bin/sh"), *SourceCodeLookupCommand, &ReturnCode, &Results, &Errors);
			if (ReturnCode == 0 && !Results.IsEmpty())
			{
				// find the last occurance of (filepath:linenumber)
				int32 OpenIndex = -1;
				int32 ColonIndex = -1;
				int32 CloseIndex = -1;
				if (Results.FindLastChar(TCHAR('('), OpenIndex) && Results.FindLastChar(TCHAR(':'), ColonIndex) && Results.FindLastChar(TCHAR(')'), CloseIndex)
					&& CloseIndex > ColonIndex && OpenIndex < ColonIndex)
				{
					int32 FileNamePos = OpenIndex + 1;
					int32 FileNameLen = ColonIndex - FileNamePos;
					FString FileName = Results.Mid(FileNamePos, FileNameLen);
					int32 LineNumberPos = ColonIndex + 1;
					int32 LineNumberLen = CloseIndex - LineNumberPos;
					FString LineNumber = Results.Mid(LineNumberPos, LineNumberLen);

					if (FPaths::FileExists(FileName))
					{
						OutLineNumber = FCString::Atoi(*LineNumber);
						return true;
					}					
				}
				else
				{
					if (!FPaths::FileExists(FullDsymPath))
					{
						UE_LOG(LogSelectionDetails, Warning, TEXT("NavigateToFunctionSource: Missing dSYM files, please install Editor symbols for debugging"), *Results);
						return false;
					}
					else
					{
						UE_LOG(LogSelectionDetails, Warning, TEXT("NavigateToFunctionSource: Unexpected SourceCodeLookup.sh output: %s"), *Results);
						return false;
					}
				}
			}
			else if (ReturnCode != 0 || !Errors.IsEmpty())
			{
				UE_LOG(LogSelectionDetails, Warning, TEXT("NavigateToFunctionSource: SourceCodeLookup.sh failed with code: %d\n%s"), ReturnCode, *Errors);
				return false;
			}
		}
	}

	UE_LOG(LogSelectionDetails, Warning, TEXT("NavigateToFunctionSource: Unable to look up symbol: %s in module:%s"), *FunctionSymbolName, *FunctionModuleName);	
#endif	// PLATFORM_WINDOWS
	return false;
}

/*
void GenerateSourceFilesOG(FString NewModuleName, TArray<FString> IncludeList, bool bIsEditor, TArray<FString>& Header, TArray<FString>& CPPFile)
{
	//It's probably best to just do all of our bind registration inside the Module class
	//I'd also review whether we really want to keep/serialize the database dictionaries
	//or just have a far simpler array of module names be saved.
	//FAngelscriptBindDatabase& BindDB = FAngelscriptBindDatabase::Get();

	TArray<TObjectPtr<UClass>> Classes;

	for (FString str : IncludeList)
	{
		if (!bIsEditor)
		{
			if (TArray<TObjectPtr<UClass>>* list = FAngelscriptBinds::GetRuntimeClassDB().Find(str))
			{
				Classes += *list;
			}
		}
		else
		{
			if (TArray<TObjectPtr<UClass>>* list = FAngelscriptBinds::GetEditorClassDB().Find(str))
			{
				Classes += *list;
			}
		}
	}

	if (Classes.IsEmpty())
		return;

	//Generate Header
	Header.Add("#pragma once\n");
	Header.Add("#include \"CoreMinimal.h\"");
	Header.Add("#include \"Modules/ModuleManager.h\"\n");

	FString ClassLine = FString("class ");
	FString API = NewModuleName.ToUpper() + "_API ";
	ClassLine += API;
	ClassLine += "F" + NewModuleName + "Module : public FDefaultModuleImpl";
	Header.Add(ClassLine);

	Header.Add("{");
	Header.Add("public:");
	Header.Add("\tvirtual void StartupModule() override;");
	Header.Add("\tvirtual void ShutdownModule() override;");
	Header.Add("};");

	//Generate CPP File
	FString ModuleClass = FString("F") + NewModuleName + "Module";

	CPPFile.Add("#include \"" + NewModuleName + "Module.h\"");
	//CPPFile.Add("#include \"FunctionCallers.h\""); //Already referenced in binds, avoid redefinition
	CPPFile.Add("#include \"AngelscriptBinds.h\"");
	//Add Include loop here for all dependencies
	TSet<FString> CurrentIncludes = TSet<FString>();

	for (auto Class : Classes)
	{
		FString HeaderPath = FString();
		bool headerFound = FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);

		if (!headerFound) continue;

		int32 pubIndex = HeaderPath.Find("Public/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
		int32 privIndex = HeaderPath.Find("Private/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
		int32 classesIndex = HeaderPath.Find("Classes/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);

		if (pubIndex > 0)
			HeaderPath = HeaderPath.RightChop(pubIndex + 7);
		if (privIndex > 0)
			HeaderPath = HeaderPath.RightChop(privIndex + 8);
		if (classesIndex > 0)
			HeaderPath = HeaderPath.RightChop(classesIndex + 8);

		FString Include = "#include ";
		Include += '"';
		Include += HeaderPath;
		Include += '"';

		if (!CurrentIncludes.Contains(Include))
		{
			CPPFile.Add(Include);
			CurrentIncludes.Add(Include);
		}
	}

	FString MacroLine = FString("IMPLEMENT_MODULE(") + ModuleClass + ", " + NewModuleName + ");\n";
	CPPFile.Add(MacroLine);

	CPPFile.Add(FString("void ") + ModuleClass + "::StartupModule()\n{\n");

	CPPFile.Add("\tFAngelscriptBinds::RegisterBinds\n\t((int32)FAngelscriptBinds::EOrder::Late,\n");

	CPPFile.Add("\t\t[]()");
	CPPFile.Add("\t\t{");

	for (auto Class : Classes)
	{
		GenerateFunctionEntries(Class, CPPFile);
		//CPPFile.Add("\n");
	}

	CPPFile.Add("\t\t}"); //End Lambda
	CPPFile.Add("\t);\n"); //End Register call

	CPPFile.Add("}\n"); //End Startup Def

	//Shutdown shouldn't have to do anything so can one line it
	CPPFile.Add(FString("void ") + ModuleClass + "::ShutdownModule()\n{\n}\n");
}
*/

void FAngelscriptEditorModule::OriginalGenerate()
{
	//FAngelscriptRuntimeModule Module = FModuleManager::LoadModuleChecked<FAngelscriptRuntimeModule>("AngelscriptRuntime");

	//Use a temporary list since we don't know which functions will be valid if there are any
	TArray<FString> lines = TArray<FString>();
	TArray<FString> newLines = TArray<FString>();
	TArray<FString> finalLines = TArray<FString>();
	TSet<FString> includeLines = TSet<FString>();
	TSet<FString> modules = TSet<FString>();
	includeLines.Add("#include \"FunctionCallers.h\""); //Add Initial header
	includeLines.Add("#include \"AngelscriptBinds.h\"");
	lines.Add("FFuncEntry Entries[]");
	lines.Add("{");

	for (UClass* Class : TObjectRange<UClass>())
	{
		bool addNewLines = false;

		FString ClassName = Class->GetPrefixCPP();
		ClassName += Class->GetName();

		TArray<FName> FuncNames;
		Class->GenerateFunctionList(FuncNames);
		if (FuncNames.IsEmpty()) continue;

		FString ModuleName = Class->GetOutermost()->GetName();
		int32 modIndex = 0;
		ModuleName.FindLastChar('/', modIndex);
		ModuleName = ModuleName.RightChop(modIndex);
		ModuleName.RemoveAt(0, 1, EAllowShrinking::Yes);
		//lines.Add("//Module: " + ModuleName);
		//newLines.Add("//Module: " + ModuleName);

		//lines.Add("#include \"FunctionCallers.h\"");					
		FString HeaderPath = FString();
		bool headerFound = FSourceCodeNavigation::FindClassHeaderPath(Class, HeaderPath);

		if (!headerFound)
		{
			newLines.Empty();
			continue;
		}

		int32 pubIndex = HeaderPath.Find("Public/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
		int32 privIndex = HeaderPath.Find("Private/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);
		int32 classesIndex = HeaderPath.Find("Classes/", ESearchCase::IgnoreCase, ESearchDir::Type::FromEnd);

		if (pubIndex > 0)
			HeaderPath = HeaderPath.RightChop(pubIndex + 7);
		if (privIndex > 0)
			HeaderPath = HeaderPath.RightChop(privIndex + 8);
		if (classesIndex > 0)
			HeaderPath = HeaderPath.RightChop(classesIndex + 8);

		FString Include = "#include ";
		Include += '"';
		Include += HeaderPath;
		Include += '"';
		//Include += "\n";					
		//lines.Add(Include);
		//newLines.Add(Include);
		//includeLines.Add(Include);
		//TSharedPtr<FAngelscriptClassDesc>classDesc = FAngelscriptEngine::Get().GetClass(Class->GetName());

		//ModuleName = FPaths::GetBaseFilename(ModuleName);
		for (FName Name : FuncNames)
		{
			UFunction* Function = Class->FindFunctionByName(Name);
			//TSharedPtr<FAngelscriptFunctionDesc> funcDesc = classDesc->GetMethodByScriptName(Name.ToString());

			if (Function != nullptr && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
			{
				if (Function->HasAnyFunctionFlags(FUNC_Static))
					continue;

				FString line = FString("{ ERASE_METHOD_PTR(");

				line += ClassName + ", ";
				line += Name.ToString() + ", ";
				line += "(";

				FProperty* RetParm = nullptr;
				bool firstArg = true;

				for (TFieldIterator<FProperty> It(Function); It; ++It)
				{
					FProperty* prop = *It;
					if (prop->HasAnyPropertyFlags(CPF_ReturnParm))
					{
						RetParm = prop;
						continue;
					}

					if (firstArg)
						firstArg = false;
					else
						line += ", ";

					if (prop->HasAnyPropertyFlags(CPF_ConstParm))
						line += "const ";

					FString ExtendedType;
					FString type = prop->GetCPPType(&ExtendedType);
					line += type + ExtendedType;

					if (prop->HasAnyPropertyFlags(CPF_ReferenceParm))
						line += "&";
				}

				line += ")";

				if (Function->HasAnyFunctionFlags(FUNC_Const))
					line += "const";

				line += ", ";

				if (RetParm != nullptr)
				{
					line += "ERASE_ARGUMENT_PACK(";

					if (RetParm->HasAnyPropertyFlags(CPF_ConstParm))
						line += "const ";

					//line += RetParm->GetCPPType(&ExtendedType);
					FString ExtendedType;
					FString type = RetParm->GetCPPType(&ExtendedType);
					line += type + ExtendedType;

					if (RetParm->HasAnyPropertyFlags(CPF_ReferenceParm))
						line += "&";

					line += ")";
				}

				else
				{
					line += "ERASE_ARGUMENT_PACK(void)";
				}

				line += ") },";
				//lines.Add(line);
				addNewLines = true;
				newLines.Add(line);
			}
		} //END FUNCTIONS LOOP

		//lines.Add(FString("\n//END " + ClassName + "\n"));
		//newLines.Add(FString("\n//END " + ClassName + "\n"));
		newLines.Add(FString("//END " + ClassName + "\n"));

		if (addNewLines)
		{
			includeLines.Add(Include);

			FString ModName = FString();
			ModName += '"';
			ModName += ModuleName;
			ModName += '"';
			ModName += ',';
			modules.Add(ModName);

			for (FString str : newLines)
			{
				lines.Add(str);
			}
		}

		newLines.Empty();
	}

	if (lines.Num() > 0)
	{
		for (FString str : includeLines)
			finalLines.Add(str);
		finalLines.Add(FString("\n"));

		finalLines.Add("/*");
		for (FString str : modules)
			finalLines.Add(str);
		finalLines.Add(FString("*/\n"));

		lines.Add("};");
		for (FString str : lines)
			finalLines.Add(str);

		FString RootPath = FAngelscriptEngine::Get().AllRootPaths[0];
		//FFileHelper::SaveStringArrayToFile(lines, *(RootPath + TEXT("/FunctionCallers_Full") + TEXT(".cpp")));
		FFileHelper::SaveStringArrayToFile(finalLines, *(RootPath + TEXT("/FunctionCallers_Full") + TEXT(".cpp")));
	}
}
