#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "../../AngelscriptRuntime/Core/AngelscriptType.h"

#include "GameFramework/Actor.h"
#include "CQTest.h"
#include "UObject/UnrealType.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS




TEST_CLASS_WITH_FLAGS(FAngelscriptTypeUsageTests,
	"Angelscript.TestModule.AngelScriptSDK",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static asITypeInfo* FindTypeInfoByDecl(FAutomationTestBase& Test, asIScriptModule& Module, const FString& Declaration)
	{
		FTCHARToUTF8 DeclarationUtf8(*Declaration);
		asITypeInfo* TypeInfo = Module.GetTypeInfoByDecl(DeclarationUtf8.Get());
		FNoDiscardAsserter Assert(Test);
		(void)Assert.IsNotNull(
			TypeInfo,
			*FString::Printf(TEXT("Compiled module should expose script type '%s'"), *Declaration));
		return TypeInfo;
	}

	static int GetPropertyTypeIdByName(FAutomationTestBase& Test, asITypeInfo& ScriptType, const FString& PropertyName)
	{
		FTCHARToUTF8 PropertyNameUtf8(*PropertyName);
		for (asUINT PropertyIndex = 0, PropertyCount = ScriptType.GetPropertyCount(); PropertyIndex < PropertyCount; ++PropertyIndex)
		{
			const char* ActualName = nullptr;
			int TypeId = asINVALID_TYPE;
			ScriptType.GetProperty(PropertyIndex, &ActualName, &TypeId);
			if (ActualName != nullptr && FCStringAnsi::Strcmp(ActualName, PropertyNameUtf8.Get()) == 0)
			{
				return TypeId;
			}
		}

		Test.AddError(FString::Printf(
			TEXT("Compiled script type '%s' should expose property '%s'"),
			UTF8_TO_TCHAR(ScriptType.GetName()),
			*PropertyName));
		return asINVALID_TYPE;
	}

	static int32 GetPropertyIndexByName(FAutomationTestBase& Test, asITypeInfo& ScriptType, const FString& PropertyName)
	{
		FTCHARToUTF8 PropertyNameUtf8(*PropertyName);
		for (asUINT PropertyIndex = 0, PropertyCount = ScriptType.GetPropertyCount(); PropertyIndex < PropertyCount; ++PropertyIndex)
		{
			const char* ActualName = nullptr;
			ScriptType.GetProperty(PropertyIndex, &ActualName, nullptr);
			if (ActualName != nullptr && FCStringAnsi::Strcmp(ActualName, PropertyNameUtf8.Get()) == 0)
			{
				return static_cast<int32>(PropertyIndex);
			}
		}

		Test.AddError(FString::Printf(
			TEXT("Compiled script type '%s' should expose property index for '%s'"),
			UTF8_TO_TCHAR(ScriptType.GetName()),
			*PropertyName));
		return INDEX_NONE;
	}

	static asITypeInfo* FindTypeInfoById(FAutomationTestBase& Test, asIScriptEngine& ScriptEngine, int TypeId, const FString& Context)
	{
		asITypeInfo* TypeInfo = (TypeId != asINVALID_TYPE) ? ScriptEngine.GetTypeInfoById(TypeId) : nullptr;
		FNoDiscardAsserter Assert(Test);
		(void)Assert.IsNotNull(TypeInfo, *FString::Printf(TEXT("%s should resolve to a script type"), *Context));
		return TypeInfo;
	}

	static asITypeInfo* FindArrayIntTypeInfo(FAutomationTestBase& Test, asIScriptEngine& ScriptEngine)
	{
		static constexpr const ANSICHAR* CandidateDecls[] =
		{
			"TArray<int>",
			"array<int>",
		};

		for (const ANSICHAR* CandidateDecl : CandidateDecls)
		{
			if (asITypeInfo* TypeInfo = ScriptEngine.GetTypeInfoByDecl(CandidateDecl))
			{
				return TypeInfo;
			}
		}

		Test.AddError(TEXT("TypeUsage FromDataType test could not resolve the array<int>/TArray<int> script type."));
		return nullptr;
	}

	static FProperty* FindPropertyByName(FAutomationTestBase& Test, UStruct& Owner, const TCHAR* PropertyName)
	{
		FProperty* Property = FindFProperty<FProperty>(&Owner, PropertyName);
		FNoDiscardAsserter Assert(Test);
		(void)Assert.IsNotNull(
			Property,
			*FString::Printf(TEXT("Generated owner '%s' should expose property '%s'"), *Owner.GetName(), PropertyName));
		return Property;
	}

	static bool ExpectUsageMatches(
		FAutomationTestBase& Test,
		const FString& Context,
		const FAngelscriptTypeUsage& Usage,
		const TSharedPtr<FAngelscriptType>& ExpectedType,
		asITypeInfo* ExpectedScriptClass)
	{
		FNoDiscardAsserter Assert(Test);
		bool bMatches = true;
		bMatches &= Assert.IsTrue(
			Usage.IsValid(),
			*FString::Printf(TEXT("%s should resolve to a valid type usage"), *Context));
		bMatches &= Assert.IsTrue(
			Usage.Type.Get() == ExpectedType.Get(),
			*FString::Printf(TEXT("%s should resolve to the expected script kind"), *Context));
		bMatches &= Assert.AreEqual(
			ExpectedScriptClass,
			Usage.ScriptClass,
			*FString::Printf(TEXT("%s should preserve the originating script type"), *Context));
		return bMatches;
	}

	static bool ExpectQualifierFlags(
		FAutomationTestBase& Test,
		const FString& Context,
		const FAngelscriptTypeUsage& Usage,
		const bool bExpectedConst,
		const bool bExpectedReference)
	{
		FNoDiscardAsserter Assert(Test);
		bool bMatches = true;
		bMatches &= Assert.IsTrue(
			Usage.IsValid(),
			*FString::Printf(TEXT("%s should resolve to a valid type usage"), *Context));
		bMatches &= Assert.AreEqual(
			bExpectedConst,
			Usage.bIsConst,
			*FString::Printf(TEXT("%s should preserve the const qualifier"), *Context));
		bMatches &= Assert.AreEqual(
			bExpectedReference,
			Usage.bIsReference,
			*FString::Printf(TEXT("%s should preserve the reference qualifier"), *Context));
		return bMatches;
	}

public:
	TEST_METHOD(TypeUsageFromTypeIdScriptKinds)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"ASTypeUsageFromTypeIdScriptKinds",
			TEXT(R"ANGELSCRIPT(
enum ETypeUsageMode
{
	Waiting,
	Running = 4
}

delegate void FTypeUsageDelegate(int32 Value);
event void FTypeUsageEvent(int32 Value);

struct FTypeUsagePayload
{
	int32 Value = 0;
}

UCLASS()
class UTypeUsageCarrier : UObject
{
	UPROPERTY()
	FTypeUsageDelegate OnDone;

	UPROPERTY()
	FTypeUsageEvent OnDoneMulti;

	UPROPERTY()
	TArray<FTypeUsagePayload> Payloads;
}
)ANGELSCRIPT"));

		if (Module != nullptr)
		{
			asITypeInfo* EnumTypeInfo = FindTypeInfoByDecl(*TestRunner, *Module, TEXT("ETypeUsageMode"));
			asITypeInfo* StructTypeInfo = FindTypeInfoByDecl(*TestRunner, *Module, TEXT("FTypeUsagePayload"));
			asITypeInfo* CarrierTypeInfo = FindTypeInfoByDecl(*TestRunner, *Module, TEXT("UTypeUsageCarrier"));
			asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
			ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Type usage test should expose a script engine")));

			if (EnumTypeInfo != nullptr && StructTypeInfo != nullptr && CarrierTypeInfo != nullptr && ScriptEngine != nullptr)
			{
				const int DelegateTypeId = GetPropertyTypeIdByName(*TestRunner, *CarrierTypeInfo, TEXT("OnDone"));
				const int MulticastDelegateTypeId = GetPropertyTypeIdByName(*TestRunner, *CarrierTypeInfo, TEXT("OnDoneMulti"));
				const int ArrayTypeId = GetPropertyTypeIdByName(*TestRunner, *CarrierTypeInfo, TEXT("Payloads"));

				asITypeInfo* DelegateTypeInfo = FindTypeInfoById(*TestRunner, *ScriptEngine, DelegateTypeId, TEXT("Single-cast delegate property type id"));
				asITypeInfo* MulticastDelegateTypeInfo = FindTypeInfoById(*TestRunner, *ScriptEngine, MulticastDelegateTypeId, TEXT("Multicast delegate property type id"));
				asITypeInfo* ArrayTypeInfo = FindTypeInfoById(*TestRunner, *ScriptEngine, ArrayTypeId, TEXT("Container property type id"));

				const FAngelscriptTypeUsage EnumUsage = FAngelscriptTypeUsage::FromTypeId(EnumTypeInfo->GetTypeId());
				const FAngelscriptTypeUsage DelegateUsage = FAngelscriptTypeUsage::FromTypeId(DelegateTypeId);
				const FAngelscriptTypeUsage MulticastDelegateUsage = FAngelscriptTypeUsage::FromTypeId(MulticastDelegateTypeId);
				const FAngelscriptTypeUsage StructUsage = FAngelscriptTypeUsage::FromTypeId(StructTypeInfo->GetTypeId());
				const FAngelscriptTypeUsage ScriptObjectUsage = FAngelscriptTypeUsage::FromTypeId(CarrierTypeInfo->GetTypeId());
				const FAngelscriptTypeUsage ContainerUsage = FAngelscriptTypeUsage::FromTypeId(ArrayTypeId);

				ExpectUsageMatches(*TestRunner, TEXT("Script enum type id"), EnumUsage, FAngelscriptType::GetScriptEnum(), EnumTypeInfo);
				ExpectUsageMatches(*TestRunner, TEXT("Single-cast delegate type id"), DelegateUsage, FAngelscriptType::GetScriptDelegate(), DelegateTypeInfo);
				ExpectUsageMatches(*TestRunner, TEXT("Multicast delegate type id"), MulticastDelegateUsage, FAngelscriptType::GetScriptMulticastDelegate(), MulticastDelegateTypeInfo);
				ExpectUsageMatches(*TestRunner, TEXT("Script struct type id"), StructUsage, FAngelscriptType::GetScriptStruct(), StructTypeInfo);
				ExpectUsageMatches(*TestRunner, TEXT("Script object type id"), ScriptObjectUsage, FAngelscriptType::GetScriptObject(), CarrierTypeInfo);

				const TSharedPtr<FAngelscriptType> ArrayType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("TArray"));
				ASSERT_THAT(IsTrue(ArrayType.IsValid() && ContainerUsage.Type.Get() == ArrayType.Get(), TEXT("Container type id should resolve to the bound TArray type")));
				ASSERT_THAT(AreEqual(ArrayTypeInfo, ContainerUsage.ScriptClass, TEXT("Container type id should preserve the instantiated container type")));
				ASSERT_THAT(AreEqual(1, ContainerUsage.SubTypes.Num(), TEXT("Container type id should expose exactly one template subtype")));

				if (ContainerUsage.SubTypes.Num() == 1)
				{
					ExpectUsageMatches(
						*TestRunner,
						TEXT("Container subtype type id"),
						ContainerUsage.SubTypes[0],
						FAngelscriptType::GetScriptStruct(),
						StructTypeInfo);
					ASSERT_THAT(AreEqual(0, ContainerUsage.SubTypes[0].SubTypes.Num(), TEXT("Container subtype should not recurse any further for a plain script struct")));
				}
			}
		}

		}
	}

	TEST_METHOD(TypeUsageFromPropertyScriptMemberMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"ASTypeUsageFromPropertyScriptMemberMatrix",
			TEXT(R"ANGELSCRIPT(
enum EMode
{
	Idle = 3,
	Running = 7
}

class FPayload
{
	int Value = 11;
}

class FHolder
{
	int Count;
	TArray<int> Values;
	EMode Mode;
	FPayload Payload;
}
)ANGELSCRIPT"));

		if (Module != nullptr)
		{
			asITypeInfo* HolderTypeInfo = FindTypeInfoByDecl(*TestRunner, *Module, TEXT("FHolder"));
			asITypeInfo* EnumTypeInfo = FindTypeInfoByDecl(*TestRunner, *Module, TEXT("EMode"));
			asITypeInfo* PayloadTypeInfo = FindTypeInfoByDecl(*TestRunner, *Module, TEXT("FPayload"));
			asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
			ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("FromProperty matrix test should expose a script engine")));

			if (HolderTypeInfo != nullptr && EnumTypeInfo != nullptr && PayloadTypeInfo != nullptr && ScriptEngine != nullptr)
			{
				const int32 CountIndex = GetPropertyIndexByName(*TestRunner, *HolderTypeInfo, TEXT("Count"));
				const int32 ValuesIndex = GetPropertyIndexByName(*TestRunner, *HolderTypeInfo, TEXT("Values"));
				const int32 ModeIndex = GetPropertyIndexByName(*TestRunner, *HolderTypeInfo, TEXT("Mode"));
				const int32 PayloadIndex = GetPropertyIndexByName(*TestRunner, *HolderTypeInfo, TEXT("Payload"));

				if (CountIndex != INDEX_NONE && ValuesIndex != INDEX_NONE && ModeIndex != INDEX_NONE && PayloadIndex != INDEX_NONE)
				{
					int ValuesTypeId = asINVALID_TYPE;
					HolderTypeInfo->GetProperty(static_cast<asUINT>(ValuesIndex), nullptr, &ValuesTypeId);
					asITypeInfo* ValuesTypeInfo = FindTypeInfoById(*TestRunner, *ScriptEngine, ValuesTypeId, TEXT("Container property type id"));

					const FAngelscriptTypeUsage CountUsage = FAngelscriptTypeUsage::FromProperty(HolderTypeInfo, CountIndex);
					const FAngelscriptTypeUsage ValuesUsage = FAngelscriptTypeUsage::FromProperty(HolderTypeInfo, ValuesIndex);
					const FAngelscriptTypeUsage ModeUsage = FAngelscriptTypeUsage::FromProperty(HolderTypeInfo, ModeIndex);
					const FAngelscriptTypeUsage PayloadUsage = FAngelscriptTypeUsage::FromProperty(HolderTypeInfo, PayloadIndex);

					ASSERT_THAT(IsTrue(CountUsage.IsValid(), TEXT("Primitive member usage should resolve to a valid type")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("int")),
						CountUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
						TEXT("Primitive member usage should render as int")));
					ASSERT_THAT(AreEqual(0, CountUsage.SubTypes.Num(), TEXT("Primitive member usage should not report template subtypes")));

					const TSharedPtr<FAngelscriptType> ArrayType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("TArray"));
					ASSERT_THAT(IsTrue(ValuesUsage.IsValid(), TEXT("Container member usage should resolve to a valid type")));
					ASSERT_THAT(IsTrue(ArrayType.IsValid() && ValuesUsage.Type.Get() == ArrayType.Get(), TEXT("Container member usage should resolve to the bound TArray type")));
					ASSERT_THAT(AreEqual(ValuesTypeInfo, ValuesUsage.ScriptClass, TEXT("Container member usage should preserve the instantiated container type")));
					ASSERT_THAT(AreEqual(1, ValuesUsage.SubTypes.Num(), TEXT("Container member usage should expose exactly one subtype")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("TArray<int>")),
						ValuesUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
						TEXT("Container member usage should render with its element type")));

					if (ValuesUsage.SubTypes.Num() == 1)
					{
						ASSERT_THAT(IsTrue(ValuesUsage.SubTypes[0].IsValid(), TEXT("Container element usage should resolve to a valid type")));
						ASSERT_THAT(AreEqual(
							FString(TEXT("int")),
							ValuesUsage.SubTypes[0].GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
							TEXT("Container element usage should render as int")));
						ASSERT_THAT(AreEqual(0, ValuesUsage.SubTypes[0].SubTypes.Num(), TEXT("Container element usage should not recurse further")));
					}

					ExpectUsageMatches(*TestRunner, TEXT("Script enum member usage"), ModeUsage, FAngelscriptType::GetScriptEnum(), EnumTypeInfo);
					ASSERT_THAT(AreEqual(
						FString(TEXT("EMode")),
						ModeUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
						TEXT("Script enum member usage should render the enum declaration")));

					ExpectUsageMatches(*TestRunner, TEXT("Script object member usage"), PayloadUsage, FAngelscriptType::GetScriptObject(), PayloadTypeInfo);
					ASSERT_THAT(AreEqual(
						FString(TEXT("FPayload")),
						PayloadUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
						TEXT("Script object member usage should render the payload declaration")));
				}
			}
		}

		}
	}

	TEST_METHOD(DataTypeTypeUsageQualifiers)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			"ASTypeUsageQualifiers",
			TEXT(R"ANGELSCRIPT(
void Qualifiers(const int&in Input, int&out Output, bool Flag)
{
	Output = Flag ? Input : 0;
}

int Produce()
{
	return 7;
}
)ANGELSCRIPT"));

		if (Module != nullptr)
		{
			asIScriptFunction* QualifiersFunction = GetFunctionByDecl(
				*TestRunner,
				*Module,
				TEXT("void Qualifiers(const int&, int&, bool)"));
			asIScriptFunction* ProduceFunction = GetFunctionByDecl(
				*TestRunner,
				*Module,
				TEXT("int Produce()"));

			if (QualifiersFunction != nullptr && ProduceFunction != nullptr)
			{
				const FAngelscriptTypeUsage InputUsage = FAngelscriptTypeUsage::FromParam(QualifiersFunction, 0);
				const FAngelscriptTypeUsage OutputUsage = FAngelscriptTypeUsage::FromParam(QualifiersFunction, 1);
				const FAngelscriptTypeUsage FlagUsage = FAngelscriptTypeUsage::FromParam(QualifiersFunction, 2);
				const FAngelscriptTypeUsage ReturnUsage = FAngelscriptTypeUsage::FromReturn(ProduceFunction);

				ExpectQualifierFlags(*TestRunner, TEXT("Qualifiers parameter 0"), InputUsage, true, true);
				ExpectQualifierFlags(*TestRunner, TEXT("Qualifiers parameter 1"), OutputUsage, false, true);
				ExpectQualifierFlags(*TestRunner, TEXT("Qualifiers parameter 2"), FlagUsage, true, false);
				ExpectQualifierFlags(*TestRunner, TEXT("Produce return value"), ReturnUsage, false, false);

				ASSERT_THAT(AreEqual(
					FString(TEXT("const int&")),
					InputUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
					TEXT("Input qualifier declaration should render as a const reference")));
				ASSERT_THAT(AreEqual(
					FString(TEXT("int&")),
					OutputUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
					TEXT("Output qualifier declaration should render as a mutable reference")));
				ASSERT_THAT(AreEqual(
					FString(TEXT("const bool")),
					FlagUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
					TEXT("Plain bool value parameter should currently render with the propagated const qualifier")));
				ASSERT_THAT(AreEqual(
					FString(TEXT("int")),
					ReturnUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionReturnValue),
					TEXT("Return qualifier declaration should render without extra qualifiers")));

				ASSERT_THAT(IsTrue(InputUsage.EqualsUnqualified(OutputUsage), TEXT("Input and output qualifiers should still compare equal when ignoring qualifiers")));
				ASSERT_THAT(IsTrue(InputUsage.EqualsUnqualified(ReturnUsage), TEXT("Input qualifier and plain return value should still compare equal when ignoring qualifiers")));
				ASSERT_THAT(IsFalse(InputUsage.EqualsUnqualified(FlagUsage), TEXT("Integer qualifiers should not compare equal to a different base type when ignoring qualifiers")));
			}
		}

		}
	}

	TEST_METHOD(TypeUsageFromPropertyNativeQualifierMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString ScriptSource = TEXT(R"ANGELSCRIPT(
UCLASS()
class ATypeUsageNativePropertyProbe : AActor
{
	UFUNCTION()
	void Qualifiers(const int&in Input, int&out Output, bool Flag)
	{
		Output = Flag ? Input : 0;
	}
}
)ANGELSCRIPT");

		bool bCompiled = false;
		{
			FAngelscriptEngineScope EngineScope(Engine);
			bCompiled = CompileAnnotatedModuleFromMemory(
				&Engine,
				TEXT("ASTypeUsageFromPropertyNativeQualifierMatrix"),
				TEXT("ASTypeUsageFromPropertyNativeQualifierMatrix.as"),
				ScriptSource);
			ASSERT_THAT(IsTrue(bCompiled, TEXT("Type usage native-property probe should compile")));
		}

		if (bCompiled)
		{
			UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("ATypeUsageNativePropertyProbe"));
			ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Native-property probe class should be generated")));

			if (GeneratedClass != nullptr)
			{
				UFunction* QualifiersFunction = GeneratedClass->FindFunctionByName(TEXT("Qualifiers"));
				ASSERT_THAT(IsNotNull(QualifiersFunction, TEXT("Generated class should expose the Qualifiers function")));

				if (QualifiersFunction != nullptr)
				{
					FProperty* InputProperty = FindPropertyByName(*TestRunner, *QualifiersFunction, TEXT("Input"));
					FProperty* OutputProperty = FindPropertyByName(*TestRunner, *QualifiersFunction, TEXT("Output"));
					FProperty* FlagProperty = FindPropertyByName(*TestRunner, *QualifiersFunction, TEXT("Flag"));

					if (InputProperty != nullptr && OutputProperty != nullptr && FlagProperty != nullptr)
					{
						const FAngelscriptTypeUsage InputUsage = FAngelscriptTypeUsage::FromProperty(InputProperty);
						const FAngelscriptTypeUsage OutputUsage = FAngelscriptTypeUsage::FromProperty(OutputProperty);
						const FAngelscriptTypeUsage FlagUsage = FAngelscriptTypeUsage::FromProperty(FlagProperty);

						ExpectQualifierFlags(*TestRunner, TEXT("Native property Input"), InputUsage, true, true);
						ExpectQualifierFlags(*TestRunner, TEXT("Native property Output"), OutputUsage, false, true);
						ExpectQualifierFlags(*TestRunner, TEXT("Native property Flag"), FlagUsage, false, false);

						ASSERT_THAT(AreEqual(
							FString(TEXT("const int&")),
							InputUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
							TEXT("Native property Input should render as a const reference")));
						ASSERT_THAT(AreEqual(
							FString(TEXT("int&")),
							OutputUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
							TEXT("Native property Output should render as a mutable reference")));
						ASSERT_THAT(AreEqual(
							FString(TEXT("bool")),
							FlagUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
							TEXT("Native property Flag should render as a plain bool")));
					}
				}
			}
		}

		}
	}

	TEST_METHOD(TypeUsageFromDataTypeQualifierAndContainerMatrix)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("FromDataType matrix test should expose a script engine")));

		if (ScriptEngine != nullptr)
		{
			asCDataType IntConstRef = asCDataType::CreatePrimitive(ttInt, true);
			IntConstRef.MakeReference(true);

			asCTypeInfo* ActorTypeInfo = static_cast<asCTypeInfo*>(ScriptEngine->GetTypeInfoByName("AActor"));
			ASSERT_THAT(IsNotNull(ActorTypeInfo, TEXT("FromDataType matrix test should resolve the native AActor script type")));

			asITypeInfo* ArrayTypeInfo = FindArrayIntTypeInfo(*TestRunner, *ScriptEngine);
			if (ActorTypeInfo != nullptr && ArrayTypeInfo != nullptr)
			{
				asCDataType ActorHandle = asCDataType::CreateObjectHandle(ActorTypeInfo, false);
				ActorHandle.MakeHandleToConst(true);

				asCDataType ArrayValue = asCDataType::CreateType(static_cast<asCTypeInfo*>(ArrayTypeInfo), false);

				const FAngelscriptTypeUsage IntConstRefUsage = FAngelscriptTypeUsage::FromDataType(IntConstRef);
				const FAngelscriptTypeUsage ActorHandleUsage = FAngelscriptTypeUsage::FromDataType(ActorHandle);
				const FAngelscriptTypeUsage ArrayValueUsage = FAngelscriptTypeUsage::FromDataType(ArrayValue);

				const TSharedPtr<FAngelscriptType> IntType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("int"));
				ASSERT_THAT(IsTrue(IntType.IsValid() && IntConstRefUsage.Type.Get() == IntType.Get(), TEXT("const int& data type should resolve to the int wrapper")));
				ExpectQualifierFlags(*TestRunner, TEXT("const int& data type"), IntConstRefUsage, true, true);
				ASSERT_THAT(AreEqual(
					FString(TEXT("const int&")),
					IntConstRefUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument),
					TEXT("const int& data type should render its qualifiers")));

				ExpectUsageMatches(
					*TestRunner,
					TEXT("const AActor handle data type"),
					ActorHandleUsage,
					FAngelscriptType::GetByClass(AActor::StaticClass()),
					ActorTypeInfo);
				ExpectQualifierFlags(*TestRunner, TEXT("const AActor handle data type"), ActorHandleUsage, true, false);
				ASSERT_THAT(IsTrue(ActorHandleUsage.GetClass() == AActor::StaticClass(), TEXT("const AActor handle data type should stay bound to the native AActor class")));
				ASSERT_THAT(IsTrue(
					ActorHandleUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::FunctionArgument).StartsWith(TEXT("const AActor")),
					TEXT("const AActor handle data type should render as a const object type")));

				const TSharedPtr<FAngelscriptType> ArrayType = FAngelscriptType::GetByAngelscriptTypeName(TEXT("TArray"));
				ExpectUsageMatches(
					*TestRunner,
					TEXT("array<int> data type"),
					ArrayValueUsage,
					ArrayType,
					ArrayTypeInfo);
				ExpectQualifierFlags(*TestRunner, TEXT("array<int> data type"), ArrayValueUsage, false, false);
				ASSERT_THAT(AreEqual(
					FString(TEXT("TArray<int>")),
					ArrayValueUsage.GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
					TEXT("array<int> data type should render the bound container declaration")));
				ASSERT_THAT(AreEqual(1, ArrayValueUsage.SubTypes.Num(), TEXT("array<int> data type should expose exactly one template subtype")));

				if (ArrayValueUsage.SubTypes.Num() == 1)
				{
					ASSERT_THAT(IsTrue(ArrayValueUsage.SubTypes[0].IsValid(), TEXT("array<int> subtype should resolve to a valid type")));
					ASSERT_THAT(AreEqual(
						FString(TEXT("int")),
						ArrayValueUsage.SubTypes[0].GetAngelscriptDeclaration(FAngelscriptType::EAngelscriptDeclarationMode::MemberVariable),
						TEXT("array<int> subtype should render as int")));
					ASSERT_THAT(AreEqual(0, ArrayValueUsage.SubTypes[0].SubTypes.Num(), TEXT("array<int> subtype should not recurse further")));
				}
			}
		}

		}
	}
};

#endif
