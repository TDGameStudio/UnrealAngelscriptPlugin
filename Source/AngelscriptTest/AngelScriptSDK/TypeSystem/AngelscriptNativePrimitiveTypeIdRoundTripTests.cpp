#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_datatype.h"
#include "source/as_scriptengine.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FPrimitiveTypeIdRoundTripTests,
	"Angelscript.TestModule.AngelScriptSDK.TypeSystem.PrimitiveTypeIdRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPrimitiveTypeIdCase
	{
		const TCHAR* Id;
		int32 TypeId;
		eTokenType Token;
		const ANSICHAR* Declaration;
		int32 SizeInBytes;
	};

	inline static constexpr FPrimitiveTypeIdCase TypeCases[] =
	{
		{ TEXT("void"), asTYPEID_VOID, ttVoid, "void", 0 },
		{ TEXT("bool"), asTYPEID_BOOL, ttBool, "bool", 1 },
		{ TEXT("int8"), asTYPEID_INT8, ttInt8, "int8", 1 },
		{ TEXT("int16"), asTYPEID_INT16, ttInt16, "int16", 2 },
		{ TEXT("int"), asTYPEID_INT32, ttInt, "int", 4 },
		{ TEXT("int64"), asTYPEID_INT64, ttInt64, "int64", 8 },
		{ TEXT("uint8"), asTYPEID_UINT8, ttUInt8, "uint8", 1 },
		{ TEXT("uint16"), asTYPEID_UINT16, ttUInt16, "uint16", 2 },
		{ TEXT("uint"), asTYPEID_UINT32, ttUInt, "uint", 4 },
		{ TEXT("uint64"), asTYPEID_UINT64, ttUInt64, "uint64", 8 },
		{ TEXT("float32"), asTYPEID_FLOAT32, ttFloat32, "float32", 4 },
		{ TEXT("float64"), asTYPEID_FLOAT64, ttFloat64, "float", 8 },
	};

public:
	TEST_METHOD(PrimitiveIdsPreserveTokenDeclarationAndSize)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("TYPE-ENGINE-PRIMITIVE-TYPEID-ROUNDTRIP",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asIScriptEngine* const PublicEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			PublicEngine,
			TEXT("Primitive type-id roundtrip should create a case-owned raw SDK engine")));
		if (PublicEngine == nullptr)
		{
			return;
		}

		asCScriptEngine* const InternalEngine = static_cast<asCScriptEngine*>(PublicEngine);
		ASSERT_THAT(IsNotNull(
			InternalEngine,
			TEXT("Primitive type-id roundtrip should expose the current-fork script-engine implementation")));
		if (InternalEngine == nullptr)
		{
			return;
		}

		FNativeTestEngine ControlEngine;
		ControlEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			ControlEngine.Destroy();
		};
		asIScriptEngine* const ControlPublicEngine = ControlEngine.Get();
		ASSERT_THAT(IsNotNull(
			ControlPublicEngine,
			TEXT("Primitive type-id isolation should create an independent control engine")));
		if (ControlPublicEngine == nullptr)
		{
			return;
		}
		asCScriptEngine* const ControlInternalEngine =
			static_cast<asCScriptEngine*>(ControlPublicEngine);

		for (const FPrimitiveTypeIdCase& TypeCase : TypeCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"TYPE-ENGINE-PRIMITIVE-TYPEID-ROUNDTRIP",
				{ TypeCase.Id }));
			const asCDataType InternalType = InternalEngine->GetDataTypeFromTypeId(TypeCase.TypeId);
			const asCDataType ControlInternalType =
				ControlInternalEngine->GetDataTypeFromTypeId(TypeCase.TypeId);
			const asCDataType DirectPrimitive = asCDataType::CreatePrimitive(TypeCase.Token, false);

			ASSERT_THAT(IsTrue(
				InternalType.IsValid(),
				*Case.Describe(TEXT("primitive type ID should produce a valid internal data type"))));
			ASSERT_THAT(IsTrue(
				InternalType == DirectPrimitive,
				*Case.Describe(TEXT("script-engine reconstruction should equal direct primitive construction"))));
			ASSERT_THAT(IsTrue(
				ControlInternalType == DirectPrimitive,
				*Case.Describe(TEXT("an independent engine should reconstruct the same primitive value descriptor"))));
			ASSERT_THAT(IsTrue(
				InternalType.IsPrimitive() && !InternalType.IsObject() && !InternalType.IsReference(),
				*Case.Describe(TEXT("primitive type ID should retain primitive-only kind flags"))));
			ASSERT_THAT(AreEqual(
				static_cast<int32>(TypeCase.Token),
				static_cast<int32>(InternalType.GetTokenType()),
				*Case.Describe(TEXT("primitive type ID should map to the exact independent token"))));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(TypeCase.Declaration)),
				FString(UTF8_TO_TCHAR(PublicEngine->GetTypeDeclaration(TypeCase.TypeId))),
				*Case.Describe(TEXT("public type declaration should corroborate the internal primitive mapping"))));
			ASSERT_THAT(AreEqual(
				TypeCase.SizeInBytes,
				PublicEngine->GetSizeOfPrimitiveType(TypeCase.TypeId),
				*Case.Describe(TEXT("public primitive size should match the independent byte-width table"))));
			ASSERT_THAT(AreEqual(
				FString(UTF8_TO_TCHAR(PublicEngine->GetTypeDeclaration(TypeCase.TypeId))),
				FString(UTF8_TO_TCHAR(ControlPublicEngine->GetTypeDeclaration(TypeCase.TypeId))),
				*Case.Describe(TEXT("primitive declarations should not depend on another engine's state"))));
			ASSERT_THAT(AreEqual(
				PublicEngine->GetSizeOfPrimitiveType(TypeCase.TypeId),
				ControlPublicEngine->GetSizeOfPrimitiveType(TypeCase.TypeId),
				*Case.Describe(TEXT("primitive sizes should remain isolated from engine-owned registrations"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
