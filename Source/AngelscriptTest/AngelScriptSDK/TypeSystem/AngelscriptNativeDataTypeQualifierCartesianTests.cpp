#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"
#include "../Support/AngelscriptNativeExecutionTestSupport.h"
#include "../Support/AngelscriptNativeFixtureTestSupport.h"

#include "CQTest.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

using AngelscriptNativeTestSupport::AppendGeneratedAsLine;

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDataTypeQualifierCartesianTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.DataTypeQualifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FTypeCase
	{
		const ANSICHAR* Name;
		eTokenType Token;
		int32 SizeInBytes;
		int32 Alignment;
		bool bInteger;
		bool bUnsigned;
		bool bFloat32;
		bool bFloat64;
		bool bBoolean;
	};

	struct FQualifierCase
	{
		const ANSICHAR* Name;
		bool bConst;
		bool bReference;
	};

	inline static constexpr FTypeCase TypeCases[] =
	{
		{ "int8", ttInt8, 1, 1, true, false, false, false, false },
		{ "int16", ttInt16, 2, 2, true, false, false, false, false },
		{ "int", ttInt, 4, 4, true, false, false, false, false },
		{ "int64", ttInt64, 8, 8, true, false, false, false, false },
		{ "uint8", ttUInt8, 1, 1, false, true, false, false, false },
		{ "uint16", ttUInt16, 2, 2, false, true, false, false, false },
		{ "uint", ttUInt, 4, 4, false, true, false, false, false },
		{ "uint64", ttUInt64, 8, 8, false, true, false, false, false },
		{ "float32", ttFloat32, 4, 4, false, false, true, false, false },
		{ "float64", ttFloat64, 8, 8, false, false, false, true, false },
		{ "bool", ttBool, 1, 1, false, false, false, false, true },
	};

	inline static constexpr FQualifierCase QualifierCases[] =
	{
		{ "mutable", false, false },
		{ "const", true, false },
		{ "reference", false, true },
		{ "const_reference", true, true },
	};

	static FString TypeName(const FTypeCase& TypeCase)
	{
		return ANSI_TO_TCHAR(TypeCase.Name);
	}

	static FString ExpectedFormat(const FTypeCase& TypeCase, const FQualifierCase& QualifierCase)
	{
		FString Result;
		if (QualifierCase.bConst)
		{
			Result += TEXT("const ");
		}

		if (TypeCase.Token == ttFloat64)
		{
			Result += asCDataType::floatIsFloat64 ? TEXT("float") : TEXT("float64");
		}
		else if (TypeCase.Token == ttFloat32)
		{
			Result += asCDataType::floatIsFloat64 ? TEXT("float32") : TEXT("float");
		}
		else
		{
			Result += TypeName(TypeCase);
		}

		if (QualifierCase.bReference)
		{
			Result += TEXT("&");
		}
		return Result;
	}

	static asCDataType MakeDataType(const FTypeCase& TypeCase, const FQualifierCase& QualifierCase)
	{
		asCDataType Result = asCDataType::CreatePrimitive(TypeCase.Token, QualifierCase.bConst);
		if (QualifierCase.bReference)
		{
			Result.MakeReference(true);
		}
		return Result;
	}

	static FString BuildWitnessSource(const FTypeCase& TypeCase, const FQualifierCase& QualifierCase)
	{
		FString Source;
		AppendGeneratedAsLine(Source, *FString::Printf(
			TEXT("// type=%s qualifier=%s format=%s"),
			*TypeName(TypeCase),
			ANSI_TO_TCHAR(QualifierCase.Name),
			*ExpectedFormat(TypeCase, QualifierCase)));
		AppendGeneratedAsLine(Source, TEXT("void Entry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Marker = 17;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString MakeModuleName(const AngelscriptNativeTestSupport::FNativeCaseContext& Case)
	{
		return TEXT("DataTypeQualifier_") + Case.GetId().Replace(TEXT("-"), TEXT("_"));
	}

public:
	TEST_METHOD(PrimitiveTypesByQualifier)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace AngelscriptSDKTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		AS_NATIVE_PRODUCT("TYPE-DATATYPE-QUALIFIER-CARTESIAN",
			ENativeEvidence::Compile
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Cleanup
			| ENativeEvidence::Isolation);
		FNoDiscardAsserter Assertions(*TestRunner);

		asIScriptEngine* const ScriptEngine = Engine.Get();
		if (!Assertions.IsNotNull(ScriptEngine,
			TEXT("Data-type qualifier product should create a raw SDK engine")))
		{
			return;
		}

		for (const FTypeCase& TypeCase : TypeCases)
		{
			for (const FQualifierCase& QualifierCase : QualifierCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId(
					"TYPE-DATATYPE-QUALIFIER-CARTESIAN",
					{
						ANSI_TO_TCHAR(TypeCase.Name),
						ANSI_TO_TCHAR(QualifierCase.Name),
					}));
				const FString ModuleName = MakeModuleName(Case);
				const FString Source = BuildWitnessSource(TypeCase, QualifierCase);
				PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);

				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
				const FTCHARToUTF8 SourceUtf8(*Source);
				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					SourceUtf8.Get(),
					Module);
				if (!Assertions.AreEqual(
					asSUCCESS,
					BuildResult,
					*FString::Printf(
						TEXT("%s; result=%d messages={%s}"),
						*Case.Describe(TEXT("data-type witness source should compile")),
						BuildResult,
						*Engine.GetMessagesText())))
				{
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}

				asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "void Entry()");
				if (!Assertions.IsNotNull(Entry,
					*Case.Describe(TEXT("data-type witness source should publish Entry"))))
				{
					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					continue;
				}

				{
					asIScriptContext* const Context = ScriptEngine->CreateContext();
					ON_SCOPE_EXIT
					{
						if (Context != nullptr)
						{
							Context->Release();
						}
					};
					if (Assertions.IsNotNull(Context,
						*Case.Describe(TEXT("data-type witness should create a context"))))
					{
						(void)Assertions.AreEqual(
							asEXECUTION_FINISHED,
							PrepareAndExecute(Context, Entry),
							*Case.Describe(TEXT("data-type witness Entry should execute")));
						(void)Assertions.AreEqual(
							asSUCCESS,
							Context->Unprepare(),
							*Case.Describe(TEXT("data-type witness context should unprepare")));
					}
				}

				asCDataType Actual = MakeDataType(TypeCase, QualifierCase);
				(void)Assertions.IsTrue(
					Actual.IsValid(),
					*Case.Describe(TEXT("primitive data type should be valid")));
				(void)Assertions.AreEqual(
					TypeCase.Token,
					Actual.GetTokenType(),
					*Case.Describe(TEXT("primitive data type should retain its token type")));
				(void)Assertions.AreEqual(
					QualifierCase.bConst,
					Actual.IsReadOnly(),
					*Case.Describe(TEXT("data type should retain const/read-only state")));
				(void)Assertions.AreEqual(
					QualifierCase.bReference,
					Actual.IsReference(),
					*Case.Describe(TEXT("data type should retain reference state")));
				(void)Assertions.IsTrue(
					Actual.IsPrimitive() && !Actual.IsObject() && !Actual.IsObjectHandle(),
					*Case.Describe(TEXT("primitive data type should report primitive-only kind flags")));
				(void)Assertions.AreEqual(TypeCase.bInteger, Actual.IsIntegerType(),
					*Case.Describe(TEXT("integer predicate should match the independent type table")));
				(void)Assertions.AreEqual(TypeCase.bUnsigned, Actual.IsUnsignedType(),
					*Case.Describe(TEXT("unsigned predicate should match the independent type table")));
				(void)Assertions.AreEqual(TypeCase.bFloat32, Actual.IsFloat32Type(),
					*Case.Describe(TEXT("float32 predicate should match the independent type table")));
				(void)Assertions.AreEqual(TypeCase.bFloat64, Actual.IsFloat64Type(),
					*Case.Describe(TEXT("float64 predicate should match the independent type table")));
				(void)Assertions.AreEqual(TypeCase.bBoolean, Actual.IsBooleanType(),
					*Case.Describe(TEXT("boolean predicate should match the independent type table")));
				(void)Assertions.AreEqual(
					TypeCase.SizeInBytes,
					Actual.GetSizeInMemoryBytes(),
					*Case.Describe(TEXT("data type should report the expected memory size")));
				(void)Assertions.AreEqual(
					TypeCase.SizeInBytes <= 4 ? 1 : 2,
					Actual.GetSizeInMemoryDWords(),
					*Case.Describe(TEXT("data type should report the expected dword count")));
				(void)Assertions.AreEqual(
					QualifierCase.bReference ? 8 : TypeCase.Alignment,
					Actual.GetAlignment(),
					*Case.Describe(TEXT("data type should report the expected alignment")));
				(void)Assertions.AreEqual(
					ExpectedFormat(TypeCase, QualifierCase),
					FString(UTF8_TO_TCHAR(Actual.Format(nullptr).AddressOf())),
					*Case.Describe(TEXT("data type should preserve the fork canonical format")));
				(void)Assertions.IsTrue(
					Actual.CanBeInstantiated() && Actual.CanBeCopied() && !Actual.IsNullHandle(),
					*Case.Describe(TEXT("primitive data type should remain instantiable, copyable, and non-null-handle")));
				(void)Assertions.IsFalse(
					Actual.SupportHandles(),
					*Case.Describe(TEXT("primitive data type should not claim object-handle support")));

				const FQualifierCase MutableValueQualifier = { "mutable", false, false };
				const FQualifierCase ConstValueQualifier = { "const", true, false };
				const FQualifierCase MutableReferenceQualifier = { "reference", false, true };
				const FQualifierCase ConstReferenceQualifier = { "const_reference", true, true };
				const asCDataType MutableValue = MakeDataType(TypeCase, MutableValueQualifier);
				const asCDataType ConstValue = MakeDataType(TypeCase, ConstValueQualifier);
				const asCDataType MutableReference = MakeDataType(TypeCase, MutableReferenceQualifier);
				const asCDataType ConstReference = MakeDataType(TypeCase, ConstReferenceQualifier);
				(void)Assertions.IsTrue(
					MutableValue.IsEqualExceptConst(ConstValue),
					*Case.Describe(TEXT("const comparison should ignore only constness")));
				(void)Assertions.IsTrue(
					MutableValue.IsEqualExceptRef(MutableReference),
					*Case.Describe(TEXT("reference comparison should ignore only reference state")));
				(void)Assertions.IsTrue(
					MutableValue.IsEqualExceptRefAndConst(ConstReference),
					*Case.Describe(TEXT("combined comparison should ignore constness and reference state")));
				(void)Assertions.IsFalse(
					MutableValue == ConstValue,
					*Case.Describe(TEXT("exact equality should retain constness")));
				(void)Assertions.IsFalse(
					MutableValue == MutableReference,
					*Case.Describe(TEXT("exact equality should retain reference state")));

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				(void)Assertions.IsNull(
					ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
					*Case.Describe(TEXT("data-type witness module should be discarded")));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
