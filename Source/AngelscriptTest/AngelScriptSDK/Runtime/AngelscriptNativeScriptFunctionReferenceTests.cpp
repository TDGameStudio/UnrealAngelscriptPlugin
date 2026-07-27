#include "../Support/AngelscriptNativeCaseTestSupport.h"
#include "../Support/AngelscriptNativeCoreTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_bytecode.h"
#include "source/as_datatype.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_tokendef.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FScriptFunctionReferenceTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.ScriptFunctionReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static int32 ObserveInternalReferenceCount(asCTypeInfo& Type)
	{
		const int32 CountAfterAdd = Type.AddRefInternal();
		const int32 CountAfterRestore = Type.ReleaseInternal();
		return CountAfterAdd == CountAfterRestore + 1
			? CountAfterRestore
			: INDEX_NONE;
	}

	enum class EReferenceCategory : uint8
	{
		ReturnType,
		ParameterType,
		TemplateSubtype,
		LocalObjectType,
	};

	struct FReferenceCase
	{
		const TCHAR* Id;
		EReferenceCategory Category;
	};

	inline static constexpr FReferenceCase ReferenceCases[] =
	{
		{ TEXT("return"), EReferenceCategory::ReturnType },
		{ TEXT("parameter"), EReferenceCategory::ParameterType },
		{ TEXT("template"), EReferenceCategory::TemplateSubtype },
		{ TEXT("local"), EReferenceCategory::LocalObjectType },
	};

	static void ClearConsumedReferenceState(asCScriptFunction& Function)
	{
		Function.scriptData->byteCode.SetLength(0);
		Function.scriptData->objVariableTypes.SetLength(0);
		Function.returnType = asCDataType::CreatePrimitive(ttVoid, false);
		Function.parameterTypes.SetLength(0);
		Function.templateSubTypes.SetLength(0);
	}

public:
	TEST_METHOD(SignatureAndLocalReferencesReleaseExactlyOnce)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("RT-SCRIPTFUNCTION-REFERENCE-RELEASE",
			ENativeEvidence::Bytecode
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup
				| ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		asCScriptEngine* const InternalEngine = static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(
			InternalEngine,
			TEXT("Script-function reference release should create a case-owned raw SDK engine")));
		if (InternalEngine == nullptr)
		{
			return;
		}

		for (const FReferenceCase& ReferenceCase : ReferenceCases)
		{
			const FNativeCaseContext Case(MakeNativeCaseId(
				"RT-SCRIPTFUNCTION-REFERENCE-RELEASE",
				{ ReferenceCase.Id }));
			asCObjectType ReferencedType(InternalEngine);
			asCScriptFunction Function(InternalEngine, nullptr, asFUNC_SCRIPT);

			ASSERT_THAT(IsNotNull(
				Function.scriptData,
				*Case.Describe(TEXT("script function should allocate bytecode-owned state"))));
			if (Function.scriptData == nullptr)
			{
				continue;
			}

			switch (ReferenceCase.Category)
			{
			case EReferenceCategory::ReturnType:
				Function.returnType = asCDataType::CreateType(&ReferencedType, false);
				break;
			case EReferenceCategory::ParameterType:
				Function.parameterTypes.PushLast(asCDataType::CreateType(&ReferencedType, false));
				break;
			case EReferenceCategory::TemplateSubtype:
				Function.templateSubTypes.PushLast(asCDataType::CreateType(&ReferencedType, false));
				break;
			case EReferenceCategory::LocalObjectType:
				Function.scriptData->objVariableTypes.PushLast(&ReferencedType);
				break;
			}

			Function.scriptData->byteCode.PushLast(static_cast<asDWORD>(asBC_RET));
			Function.AddReferences();
			ASSERT_THAT(AreEqual(
				2,
				ObserveInternalReferenceCount(ReferencedType),
				*Case.Describe(TEXT("AddReferences should retain the selected reference category exactly once"))));

			Function.ReleaseReferences();

			ASSERT_THAT(AreEqual(
				1,
				ObserveInternalReferenceCount(ReferencedType),
				*Case.Describe(TEXT("ReleaseReferences should restore the independent reference baseline"))));

			ClearConsumedReferenceState(Function);
			ASSERT_THAT(AreEqual(
				static_cast<asUINT>(0),
				Function.scriptData->byteCode.GetLength(),
				*Case.Describe(TEXT("fixture cleanup should prevent destructor double release"))));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
