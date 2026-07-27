#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypeInfoFunctionMetadataDepthTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.TypeInfoFunctionMetadataDepth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FMetadataToken
	{
		int32 Value = 0;
	};

	struct FTypeIdentityCase
	{
		const TCHAR* Id;
		const TCHAR* Declaration;
		asITypeInfo* Type;
		asIScriptModule* ExpectedModule;
	};

	struct FFunctionMetadataCase
	{
		const TCHAR* Id;
		asIScriptFunction* Function;
		bool bExpectedNoOp;
		bool bExpectedShared;
	};

	static void NativeMetaCallback()
	{
	}

	static FString BuildScriptMetadataSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("namespace MetaSpace"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tclass BaseMeta"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint BaseValue;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tclass DerivedMeta : BaseMeta"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint Link;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void EmptyMeta()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("void EffectMeta()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeMetaCallback();"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildNativeReviewSource(
		const TCHAR* Kind,
		const TCHAR* Declaration)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Kind: %s"), Kind));
		AppendGeneratedAsLine(
			Source,
			FString::Printf(TEXT("// Declaration: %s"), Declaration));
		return Source;
	}

	static asUINT FindPropertyIndex(
		const asITypeInfo& Type,
		const char* ExpectedName)
	{
		for (asUINT Index = 0; Index < Type.GetPropertyCount(); ++Index)
		{
			const char* Name = nullptr;
			if (Type.GetProperty(Index, &Name) >= 0
				&& Name != nullptr
				&& FCStringAnsi::Strcmp(Name, ExpectedName) == 0)
			{
				return Index;
			}
		}
		return MAX_uint32;
	}

	static asITypeInfo* FindRegisteredTypedef(
		asIScriptEngine& ScriptEngine,
		const char* ExpectedName)
	{
		for (asUINT Index = 0; Index < ScriptEngine.GetTypedefCount(); ++Index)
		{
			asITypeInfo* const Type = ScriptEngine.GetTypedefByIndex(Index);
			if (Type != nullptr
				&& FCStringAnsi::Strcmp(Type->GetName(), ExpectedName) == 0)
			{
				return Type;
			}
		}
		return nullptr;
	}

	static asITypeInfo* FindModuleTypeInNamespace(
		asIScriptModule& Module,
		const char* Namespace,
		const char* Name)
	{
		const char* const CurrentNamespace = Module.GetDefaultNamespace();
		const std::string PreviousNamespace =
			CurrentNamespace != nullptr ? CurrentNamespace : "";
		if (Module.SetDefaultNamespace(Namespace) < 0)
		{
			return nullptr;
		}

		asITypeInfo* const Type = Module.GetTypeInfoByName(Name);
		Module.SetDefaultNamespace(PreviousNamespace.c_str());
		return Type;
	}

	static bool RegisterMetadataContracts(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine)
	{
		FNoDiscardAsserter Assert(Test);
		bool bSuccess = true;

		bSuccess &= Assert.IsTrue(
			ScriptEngine.BeginConfigGroup("MetadataDepthGroup") >= 0,
			TEXT("Metadata depth should begin its current-fork configuration group"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"NativeMetaType",
				0,
				asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("Metadata depth should register its native reference type"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterObjectType(
				"NativeFuncdefOwner",
				0,
				asOBJ_REF | asOBJ_NOCOUNT) >= 0,
			TEXT("Metadata depth should register its native child-funcdef owner"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterTypedef("MetaAlias", "int64") >= 0,
			TEXT("Metadata depth should register its typedef"));
		FuncdefTypeId = ScriptEngine.RegisterFuncdef("void FMetaCallback()");
		bSuccess &= Assert.IsTrue(
			FuncdefTypeId >= 0,
			TEXT("Metadata depth should register its function-pointer signature"));
		ChildFuncdefTypeId = ScriptEngine.RegisterFuncdef(
			"void NativeFuncdefOwner::FChildMetaCallback(int Value)");
		bSuccess &= Assert.IsTrue(
			ChildFuncdefTypeId >= 0,
			TEXT("Metadata depth should register its object-owned function-pointer signature"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.RegisterGlobalFunction(
				"void NativeMetaCallback()",
				asFUNCTION(NativeMetaCallback),
				asCALL_CDECL,
				nullptr,
				&AuxiliaryToken) >= 0,
			TEXT("Metadata depth should register its native callback"));
		bSuccess &= Assert.IsTrue(
			ScriptEngine.EndConfigGroup() >= 0,
			TEXT("Metadata depth should end its current-fork configuration group"));
		return bSuccess;
	}

public:
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;
	inline static FMetadataToken AuxiliaryToken{ 0x2A };
	inline static FMetadataToken UserDataA{ 0x11 };
	inline static FMetadataToken UserDataB{ 0x22 };
	inline static int32 FuncdefTypeId = asINVALID_TYPE;
	inline static int32 ChildFuncdefTypeId = asINVALID_TYPE;
	inline static bool bContractsRegistered = false;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (ScriptEngine != nullptr)
		{
			bContractsRegistered =
				RegisterMetadataContracts(*TestRunner, *ScriptEngine);
		}
	}

	AFTER_ALL()
	{
		Engine.Destroy();
		FuncdefTypeId = asINVALID_TYPE;
		ChildFuncdefTypeId = asINVALID_TYPE;
		bContractsRegistered = false;
	}

	BEFORE_EACH()
	{
		Engine.Reset(*TestRunner);
	}

	TEST_METHOD(TypeIdentityOwnershipByKind)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-TYPEINFO-IDENTITY-OWNERSHIP",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("TypeInfo identity product should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		const FString ScriptSource = BuildScriptMetadataSource();
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("TYPE-TYPEINFO-IDENTITY-OWNERSHIP-SCRIPT-CLASS"),
			TEXT("TypeInfoIdentityScriptClass"),
			ScriptSource);
		const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypeInfoIdentityScriptClass",
			ScriptSourceUtf8.Get());
		if (!Module.IsValid())
		{
			return;
		}

		asITypeInfo* const NativeType =
			ScriptEngine->GetTypeInfoByName("NativeMetaType");
		asITypeInfo* const ScriptType =
			FindModuleTypeInNamespace(
				*Module.Get(),
				"MetaSpace",
				"DerivedMeta");
		asITypeInfo* const TypedefType =
			FindRegisteredTypedef(*ScriptEngine, "MetaAlias");
		asITypeInfo* const FuncdefType =
			ScriptEngine->GetFuncdefCount() > 0
				? ScriptEngine->GetFuncdefByIndex(0)
				: nullptr;
		const FTypeIdentityCase Cases[] =
		{
			{ TEXT("native-object"), TEXT("NativeMetaType"), NativeType, nullptr },
			{ TEXT("script-class"), TEXT("MetaSpace::DerivedMeta"), ScriptType, Module.Get() },
			{ TEXT("typedef"), TEXT("MetaAlias"), TypedefType, nullptr },
			{ TEXT("funcdef"), TEXT("FMetaCallback"), FuncdefType, nullptr },
		};

		int32 ObservedCaseCount = 0;
		for (const FTypeIdentityCase& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"TYPE-TYPEINFO-IDENTITY-OWNERSHIP",
				{ Case.Id });
			if (FCString::Strcmp(Case.Id, TEXT("script-class")) != 0)
			{
				const FString ReviewSource =
					BuildNativeReviewSource(Case.Id, Case.Declaration);
				PrintGeneratedAsSource(
					*TestRunner,
					*CaseId,
					TEXT("TypeInfoIdentityNativeReview"),
					ReviewSource);
			}

			ASSERT_THAT(IsNotNull(
				Case.Type,
				TEXT("Every TypeInfo identity kind should resolve its exact declaration")));
			if (Case.Type == nullptr)
			{
				continue;
			}

			asITypeInfo* const Type = Case.Type;
			ASSERT_THAT(AreEqual(
				ScriptEngine,
				Type->GetEngine(),
				TEXT("TypeInfo should expose its exact owning engine")));
			ASSERT_THAT(AreEqual(
				Case.ExpectedModule,
				Type->GetModule(),
				TEXT("TypeInfo should distinguish module-owned and application-owned types")));
			ASSERT_THAT(IsNull(
				Type->GetConfigGroup(),
				TEXT("Current fork should expose the recorded null TypeInfo configuration group")));
			ASSERT_THAT(AreEqual(
				MAX_uint32,
				Type->GetAccessMask(),
				TEXT("Current fork TypeInfo access masks should retain all bits")));

			const int32 RefCountAfterAdd = Type->AddRef();
			ASSERT_THAT(IsTrue(
				RefCountAfterAdd > 0,
				TEXT("TypeInfo AddRef should create an observable external reference")));
			ASSERT_THAT(AreEqual(
				RefCountAfterAdd - 1,
				Type->Release(),
				TEXT("TypeInfo Release should balance the exact external reference")));
			++ObservedCaseCount;
		}

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] BeginConfigGroup/EndConfigGroup succeed, but TypeInfo GetConfigGroup remains null because current-fork config-group lookup is stubbed"));
		ASSERT_THAT(AreEqual(
			4,
			ObservedCaseCount,
			TEXT("TypeInfo identity product should observe every registered and script kind")));
	}

	TEST_METHOD(ObjectStructureFactoriesAndBoundaries)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-TYPEINFO-OBJECT-STRUCTURE",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("TypeInfo object-structure product should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		const FString Source = BuildScriptMetadataSource();
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("TYPE-TYPEINFO-OBJECT-STRUCTURE-BASE"),
			TEXT("TypeInfoObjectStructure"),
			Source);
		const FTCHARToUTF8 SourceUtf8(*Source);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypeInfoObjectStructure",
			SourceUtf8.Get());
		if (!Module.IsValid())
		{
			return;
		}

		asITypeInfo* const BaseType =
			FindModuleTypeInNamespace(
				*Module.Get(),
				"MetaSpace",
				"BaseMeta");
		asITypeInfo* const DerivedType =
			FindModuleTypeInNamespace(
				*Module.Get(),
				"MetaSpace",
				"DerivedMeta");
		ASSERT_THAT(IsNotNull(
			BaseType,
			TEXT("TypeInfo object-structure product should publish its base class")));
		ASSERT_THAT(IsNotNull(
			DerivedType,
			TEXT("TypeInfo object-structure product should publish its derived class")));
		if (BaseType == nullptr || DerivedType == nullptr)
		{
			return;
		}

		const struct
		{
			const TCHAR* Id;
			asITypeInfo* Type;
			const char* PropertyName;
			const TCHAR* ExpectedDeclaration;
			const char* FactoryDeclaration;
		} Cases[] =
		{
			{
				TEXT("base"),
				BaseType,
				"BaseValue",
				TEXT("int BaseValue"),
				"MetaSpace::BaseMeta@ ProbeFactory()",
			},
			{
				TEXT("derived"),
				DerivedType,
				"Link",
				TEXT("int Link"),
				"MetaSpace::DerivedMeta@ ProbeFactory()",
			},
		};

		int32 ObservedCaseCount = 0;
		for (const auto& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"TYPE-TYPEINFO-OBJECT-STRUCTURE",
				{ Case.Id });
			const FString ReviewSource = BuildNativeReviewSource(
				Case.Id,
				UTF8_TO_TCHAR(Case.FactoryDeclaration));
			PrintGeneratedAsSource(
				*TestRunner,
				*CaseId,
				TEXT("TypeInfoObjectStructureReview"),
				ReviewSource);

			ASSERT_THAT(AreEqual(
				static_cast<int32>(0),
				static_cast<int32>(Case.Type->GetSubTypeCount()),
				TEXT("Non-template script classes should expose no subtypes")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(asERROR),
				Case.Type->GetSubTypeId(),
				TEXT("Non-template script classes should return the current-fork no-subtype result")));
			ASSERT_THAT(IsNull(
				Case.Type->GetSubType(),
				TEXT("Non-template script classes should expose no subtype object")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(0),
				static_cast<int32>(Case.Type->GetInterfaceCount()),
				TEXT("Current-fork script classes should expose no accepted interface list")));
			ASSERT_THAT(IsTrue(
				Case.Type->Implements(Case.Type),
				TEXT("TypeInfo Implements should recognize the exact same object type")));
			ASSERT_THAT(IsTrue(
				!Case.Type->Implements(
					Case.Type == BaseType ? DerivedType : BaseType),
				TEXT("TypeInfo Implements should not confuse inheritance with interfaces")));

			const asUINT PropertyIndex =
				FindPropertyIndex(*Case.Type, Case.PropertyName);
			ASSERT_THAT(IsTrue(
				PropertyIndex != MAX_uint32,
				TEXT("TypeInfo should publish the exact requested property")));
			if (PropertyIndex != MAX_uint32)
			{
				const char* const Declaration =
					Case.Type->GetPropertyDeclaration(PropertyIndex, true);
				ASSERT_THAT(IsNotNull(
					Declaration,
					TEXT("TypeInfo should publish a property declaration")));
				if (Declaration != nullptr)
				{
					ASSERT_THAT(AreEqual(
						FString(Case.ExpectedDeclaration),
						FString(UTF8_TO_TCHAR(Declaration)),
						TEXT("TypeInfo property declaration should retain exact qualified type and name")));
				}
			}

			ASSERT_THAT(IsTrue(
				Case.Type->GetFactoryCount() > 0,
				TEXT("Script reference classes should publish at least one factory")));
			asIScriptFunction* const FactoryByIndex =
				Case.Type->GetFactoryByIndex(0);
			ASSERT_THAT(IsNotNull(
				FactoryByIndex,
				TEXT("TypeInfo should resolve its first factory by index")));
			if (FactoryByIndex != nullptr)
			{
				const std::string FactoryDeclaration =
					FactoryByIndex->GetDeclaration();
				asIScriptFunction* const FactoryByDeclaration =
					Case.Type->GetFactoryByDecl(FactoryDeclaration.c_str());
				ASSERT_THAT(AreEqual(
					FactoryByIndex,
					FactoryByDeclaration,
					TEXT("TypeInfo should round-trip the same factory by its copied canonical declaration")));
			}
			ASSERT_THAT(IsNull(
				Case.Type->GetFactoryByIndex(Case.Type->GetFactoryCount()),
				TEXT("Factory index lookup should reject the exact out-of-range boundary")));
			ASSERT_THAT(IsNull(
				Case.Type->GetFactoryByDecl("void DefinitelyMissingFactory()"),
				TEXT("Factory declaration lookup should reject an incompatible signature")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(0),
				static_cast<int32>(Case.Type->GetChildFuncdefCount()),
				TEXT("Current script class source should publish no child funcdefs")));
			ASSERT_THAT(IsNull(
				Case.Type->GetChildFuncdef(0),
				TEXT("Child funcdef lookup should reject an empty child list safely")));
			++ObservedCaseCount;
		}

		asITypeInfo* const FuncdefType =
			ScriptEngine->GetFuncdefCount() > 0
				? ScriptEngine->GetFuncdefByIndex(0)
				: nullptr;
		ASSERT_THAT(IsNotNull(
			FuncdefType,
			TEXT("Parent-type boundary should resolve the registered funcdef")));
		if (FuncdefType != nullptr)
		{
			ASSERT_THAT(IsNull(
				FuncdefType->GetParentType(),
				TEXT("Application-registered funcdef should expose no script parent type")));
		}

		asITypeInfo* const NativeFuncdefOwner =
			ScriptEngine->GetTypeInfoByName("NativeFuncdefOwner");
		asITypeInfo* const ChildFuncdef =
			ScriptEngine->GetFuncdefCount() > 1
				? ScriptEngine->GetFuncdefByIndex(1)
				: nullptr;
		ASSERT_THAT(IsNotNull(
			NativeFuncdefOwner,
			TEXT("Child-funcdef product should resolve its native owner type")));
		ASSERT_THAT(IsNotNull(
			ChildFuncdef,
			TEXT("Child-funcdef product should resolve its registered child type")));
		if (NativeFuncdefOwner != nullptr && ChildFuncdef != nullptr)
		{
			ASSERT_THAT(AreEqual(
				ChildFuncdefTypeId,
				ChildFuncdef->GetTypeId(),
				TEXT("Child funcdef should retain its exact registered type ID")));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(1),
				static_cast<int32>(NativeFuncdefOwner->GetChildFuncdefCount()),
				TEXT("Native funcdef owner should publish exactly one child")));
			ASSERT_THAT(AreEqual(
				ChildFuncdef,
				NativeFuncdefOwner->GetChildFuncdef(0),
				TEXT("Child funcdef index should expose the registered child identity")));
			ASSERT_THAT(IsNull(
				NativeFuncdefOwner->GetChildFuncdef(1),
				TEXT("Child funcdef lookup should reject the exact out-of-range boundary")));
			ASSERT_THAT(AreEqual(
				NativeFuncdefOwner,
				ChildFuncdef->GetParentType(),
				TEXT("Child funcdef should expose its exact parent type")));
		}

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] GetInterface(index) is not invoked for a zero-count interface list because this fork performs no bounds check"));
		ASSERT_THAT(AreEqual(
			2,
			ObservedCaseCount,
			TEXT("TypeInfo object-structure product should observe base and derived inputs")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module.Discard(),
			TEXT("TypeInfo object-structure product should explicitly discard its script module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"TypeInfoObjectStructure",
				asGM_ONLY_IF_EXISTS),
			TEXT("TypeInfo object-structure module should be absent after explicit cleanup")));
	}

	TEST_METHOD(TypedefAndPlainUserDataLifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-TYPEINFO-TYPEDEF-USERDATA",
			ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("TypeInfo typedef/user-data product should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		asITypeInfo* const Type =
			FindRegisteredTypedef(*ScriptEngine, "MetaAlias");
		ASSERT_THAT(IsNotNull(
			Type,
			TEXT("TypeInfo typedef/user-data product should resolve its typedef")));
		if (Type == nullptr)
		{
			return;
		}

		const FString Source = BuildNativeReviewSource(
			TEXT("int64-alias"),
			TEXT("typedef int64 MetaAlias"));
		PrintGeneratedAsSource(
			*TestRunner,
			TEXT("TYPE-TYPEINFO-TYPEDEF-USERDATA-INT64-REPLACE-CLEAR"),
			TEXT("TypeInfoTypedefUserData"),
			Source);

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asTYPEID_INT64),
			Type->GetTypedefTypeId(),
			TEXT("Typedef TypeInfo should expose its exact aliased primitive type")));
		ASSERT_THAT(IsNull(
			Type->GetUserData(),
			TEXT("TypeInfo plain user data should begin null")));
		ASSERT_THAT(IsNull(
			Type->SetUserData(&UserDataA),
			TEXT("First TypeInfo user-data install should return no prior pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataA),
			Type->GetUserData(),
			TEXT("TypeInfo should expose the exact installed plain user-data pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataA),
			Type->SetUserData(&UserDataB),
			TEXT("TypeInfo user-data replacement should return the prior pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataB),
			Type->GetUserData(),
			TEXT("TypeInfo should expose the exact replacement pointer")));
		ASSERT_THAT(AreEqual(
			static_cast<void*>(&UserDataB),
			Type->SetUserData(nullptr),
			TEXT("TypeInfo user-data clear should return the replacement pointer")));
		ASSERT_THAT(IsNull(
			Type->GetUserData(),
			TEXT("TypeInfo plain user data should be null after explicit cleanup")));
	}

	TEST_METHOD(FunctionMetadataByImplementationKind)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-SCRIPTFUNCTION-METADATA-KINDS",
			ENativeEvidence::Compile
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine,
			TEXT("ScriptFunction metadata product should create a raw SDK engine")));
		if (ScriptEngine == nullptr || !bContractsRegistered)
		{
			return;
		}

		const FString ScriptSource = BuildScriptMetadataSource();
		const FTCHARToUTF8 ScriptSourceUtf8(*ScriptSource);
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"ScriptFunctionMetadataKinds",
			ScriptSourceUtf8.Get());
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptFunction* const NativeFunction =
			ScriptEngine->GetGlobalFunctionByDecl("void NativeMetaCallback()");
		asIScriptFunction* const EmptyFunction =
			GetNativeFunctionByDecl(Module.Get(), "void EmptyMeta()");
		asIScriptFunction* const EffectFunction =
			GetNativeFunctionByDecl(Module.Get(), "void EffectMeta()");
		asITypeInfo* const FuncdefType =
			ScriptEngine->GetFuncdefCount() > 0
				? ScriptEngine->GetFuncdefByIndex(0)
				: nullptr;
		asIScriptFunction* const FuncdefFunction =
			FuncdefType != nullptr
				? FuncdefType->GetFuncdefSignature()
				: nullptr;
		const FFunctionMetadataCase Cases[] =
		{
			{ TEXT("native-system"), NativeFunction, false, true },
			{ TEXT("registered-funcdef"), FuncdefFunction, false, true },
			{ TEXT("script-noop"), EmptyFunction, true, false },
			{ TEXT("script-effect"), EffectFunction, false, false },
		};

		int32 ObservedCaseCount = 0;
		for (const FFunctionMetadataCase& Case : Cases)
		{
			const FString CaseId = MakeNativeCaseId(
				"TYPE-SCRIPTFUNCTION-METADATA-KINDS",
				{ Case.Id });
			const FString ReviewSource =
				FCString::Strcmp(Case.Id, TEXT("native-system")) == 0
					? BuildNativeReviewSource(
						Case.Id,
						TEXT("void NativeMetaCallback()"))
					: ScriptSource;
			PrintGeneratedAsSource(
				*TestRunner,
				*CaseId,
				TEXT("ScriptFunctionMetadataKinds"),
				ReviewSource);

			ASSERT_THAT(IsNotNull(
				Case.Function,
				TEXT("Every ScriptFunction metadata kind should resolve exactly")));
			if (Case.Function == nullptr)
			{
				continue;
			}

			asIScriptFunction* const Function = Case.Function;
			ASSERT_THAT(AreEqual(
				ScriptEngine,
				Function->GetEngine(),
				TEXT("ScriptFunction should expose its exact owning engine")));
			ASSERT_THAT(IsNull(
				Function->GetConfigGroup(),
				TEXT("Current fork should expose the recorded null function configuration group")));
			ASSERT_THAT(IsNull(
				Function->GetAuxiliary(),
				TEXT("Current fork should expose the recorded null function auxiliary pointer")));
			ASSERT_THAT(AreEqual(
				Case.bExpectedNoOp,
				Function->IsNoOp(),
				TEXT("ScriptFunction IsNoOp should distinguish empty bytecode from native/effectful functions")));
			ASSERT_THAT(AreEqual(
				Case.bExpectedShared,
				Function->IsShared(),
				TEXT("ScriptFunction IsShared should distinguish system and ordinary script functions")));
			ASSERT_THAT(AreEqual(
				FuncdefTypeId,
				Function->GetTypeId(),
				TEXT("ScriptFunction should resolve the registered matching function-pointer type")));
			++ObservedCaseCount;
		}

		TestRunner->AddInfo(
			TEXT("[AS-FORK-LIMITATION] Function GetConfigGroup and GetAuxiliary remain null for the provided native registration because both current-fork lookup paths are stubs"));
		ASSERT_THAT(AreEqual(
			4,
			ObservedCaseCount,
			TEXT("ScriptFunction metadata product should observe system, funcdef, no-op, and effectful kinds")));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Module.Discard(),
			TEXT("ScriptFunction metadata product should explicitly discard its script module")));
		ASSERT_THAT(IsNull(
			ScriptEngine->GetModule(
				"ScriptFunctionMetadataKinds",
				asGM_ONLY_IF_EXISTS),
			TEXT("ScriptFunction metadata module should be absent after explicit cleanup")));
	}
};

#endif
