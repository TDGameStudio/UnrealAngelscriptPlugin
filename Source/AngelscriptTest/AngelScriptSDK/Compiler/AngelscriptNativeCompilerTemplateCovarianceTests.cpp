#include "Support/AngelscriptNativeBuilderTestSupport.h"
#include "Support/AngelscriptNativeLanguageCaseTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_compiler.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FCompilerTemplateCovarianceTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TemplateCovariance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FInspectableCompiler final : public asCCompiler
	{
	public:
		explicit FInspectableCompiler(asCBuilder* Builder)
			: asCCompiler(Builder)
		{
		}

		bool AreAllCovariant(const asCDataType& To, const asCDataType& From) const
		{
			return AreAllTemplateSubtypesCovariant(To, From);
		}

		bool HasFailedCovariance(const asCDataType& To, const asCDataType& From) const
		{
			return AreTemplateTypesWithFailedCovariance(To, From);
		}
	};

	struct FCovarianceCase
	{
		const TCHAR* Id;
		const TCHAR* Description;
		const asCDataType* To = nullptr;
		const asCDataType* From = nullptr;
		bool bExpectedAllCovariant = false;
		bool bExpectedFailedCovariance = false;
	};

	static void InitializeTemplateInstance(
		asCObjectType& Instance,
		asCObjectType* TemplateBase,
		const asCDataType& SubType,
		bool bCovariant)
	{
		Instance.flags = asOBJ_REF | asOBJ_TEMPLATE;
		if (bCovariant)
		{
			Instance.flags |= asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
		}
		Instance.templateBaseType = TemplateBase;
		Instance.templateSubTypes.PushLast(SubType);
	}

public:
	TEST_METHOD(TemplateSubtypeRelationsReportSuccessAndFailurePrecisely)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("COMPILER-INTERNAL-TEMPLATE-COVARIANCE",
			ENativeEvidence::Metadata
				| ENativeEvidence::Runtime
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
			TEXT("Template covariance should create a case-owned raw SDK engine")));
		if (InternalEngine == nullptr)
		{
			return;
		}

		FScopedNativeModuleName ModuleScope(Engine, "CompilerTemplateCovariance");
		asCModule* const Module = CreateBuilderModule(InternalEngine, ModuleScope.Get());
		ASSERT_THAT(IsNotNull(
			Module,
			TEXT("Template covariance should create a case-owned backing module")));
		if (Module == nullptr || Module->builder == nullptr)
		{
			return;
		}

		asCObjectType TemplateBase(InternalEngine);
		asCObjectType OtherTemplateBase(InternalEngine);
		asCObjectType ReferenceBase(InternalEngine);
		asCObjectType ReferenceDerived(InternalEngine);
		asCObjectType ReferenceUnrelated(InternalEngine);
		ReferenceBase.flags = asOBJ_REF;
		ReferenceDerived.flags = asOBJ_REF;
		ReferenceDerived.derivedFrom = &ReferenceBase;
		ReferenceUnrelated.flags = asOBJ_REF;

		const asCDataType IntType = asCDataType::CreatePrimitive(ttInt, false);
		const asCDataType BoolType = asCDataType::CreatePrimitive(ttBool, false);
		const asCDataType BaseReferenceType = asCDataType::CreateType(&ReferenceBase, false);
		const asCDataType DerivedReferenceType = asCDataType::CreateType(&ReferenceDerived, false);
		const asCDataType UnrelatedReferenceType = asCDataType::CreateType(&ReferenceUnrelated, false);

		asCObjectType UnflaggedTo(InternalEngine);
		asCObjectType UnflaggedFrom(InternalEngine);
		InitializeTemplateInstance(UnflaggedTo, &TemplateBase, IntType, false);
		InitializeTemplateInstance(UnflaggedFrom, &TemplateBase, IntType, false);

		asCObjectType ExactTo(InternalEngine);
		asCObjectType ExactFrom(InternalEngine);
		InitializeTemplateInstance(ExactTo, &TemplateBase, IntType, true);
		InitializeTemplateInstance(ExactFrom, &TemplateBase, IntType, true);

		asCObjectType DifferentBaseFrom(InternalEngine);
		InitializeTemplateInstance(DifferentBaseFrom, &OtherTemplateBase, IntType, true);

		asCObjectType DerivedTo(InternalEngine);
		asCObjectType DerivedFrom(InternalEngine);
		InitializeTemplateInstance(DerivedTo, &TemplateBase, BaseReferenceType, true);
		InitializeTemplateInstance(DerivedFrom, &TemplateBase, DerivedReferenceType, true);

		asCObjectType UnrelatedFrom(InternalEngine);
		InitializeTemplateInstance(UnrelatedFrom, &TemplateBase, UnrelatedReferenceType, true);

		asCObjectType PrimitiveMismatchFrom(InternalEngine);
		InitializeTemplateInstance(PrimitiveMismatchFrom, &TemplateBase, BoolType, true);

		asCObjectType NestedTemplateBase(InternalEngine);
		asCObjectType NestedTo(InternalEngine);
		asCObjectType NestedFrom(InternalEngine);
		InitializeTemplateInstance(NestedTo, &NestedTemplateBase, BaseReferenceType, true);
		InitializeTemplateInstance(NestedFrom, &NestedTemplateBase, DerivedReferenceType, true);
		NestedTo.flags &= ~static_cast<asQWORD>(asOBJ_REF);
		NestedTo.flags |= asOBJ_VALUE;
		NestedFrom.flags &= ~static_cast<asQWORD>(asOBJ_REF);
		NestedFrom.flags |= asOBJ_VALUE;
		const asCDataType NestedToType = asCDataType::CreateType(&NestedTo, false);
		const asCDataType NestedFromType = asCDataType::CreateType(&NestedFrom, false);

		asCObjectType OuterNestedTo(InternalEngine);
		asCObjectType OuterNestedFrom(InternalEngine);
		InitializeTemplateInstance(OuterNestedTo, &TemplateBase, NestedToType, true);
		InitializeTemplateInstance(OuterNestedFrom, &TemplateBase, NestedFromType, true);

		const asCDataType UnflaggedToType = asCDataType::CreateType(&UnflaggedTo, false);
		const asCDataType UnflaggedFromType = asCDataType::CreateType(&UnflaggedFrom, false);
		const asCDataType ExactToType = asCDataType::CreateType(&ExactTo, false);
		const asCDataType ExactFromType = asCDataType::CreateType(&ExactFrom, false);
		const asCDataType DifferentBaseFromType = asCDataType::CreateType(&DifferentBaseFrom, false);
		const asCDataType DerivedToType = asCDataType::CreateType(&DerivedTo, false);
		const asCDataType DerivedFromType = asCDataType::CreateType(&DerivedFrom, false);
		const asCDataType UnrelatedFromType = asCDataType::CreateType(&UnrelatedFrom, false);
		const asCDataType PrimitiveMismatchFromType = asCDataType::CreateType(&PrimitiveMismatchFrom, false);
		const asCDataType OuterNestedToType = asCDataType::CreateType(&OuterNestedTo, false);
		const asCDataType OuterNestedFromType = asCDataType::CreateType(&OuterNestedFrom, false);

		const FCovarianceCase Cases[] =
		{
			{
				TEXT("unflagged"),
				TEXT("both template instances omit the covariance flag"),
				&UnflaggedToType,
				&UnflaggedFromType,
				false,
				false,
			},
			{
				TEXT("same_instance"),
				TEXT("the target and source are the same template instance"),
				&ExactToType,
				&ExactToType,
				false,
				false,
			},
			{
				TEXT("different_template_base"),
				TEXT("the instances belong to different template bases"),
				&ExactToType,
				&DifferentBaseFromType,
				false,
				false,
			},
			{
				TEXT("exact_subtype"),
				TEXT("the two instances have the same primitive subtype"),
				&ExactToType,
				&ExactFromType,
				true,
				false,
			},
			{
				TEXT("derived_reference"),
				TEXT("the source reference subtype derives from the target subtype"),
				&DerivedToType,
				&DerivedFromType,
				true,
				false,
			},
			{
				TEXT("unrelated_reference"),
				TEXT("the source reference subtype is unrelated to the target subtype"),
				&DerivedToType,
				&UnrelatedFromType,
				false,
				true,
			},
			{
				TEXT("primitive_mismatch"),
				TEXT("the source primitive subtype differs from the target subtype"),
				&ExactToType,
				&PrimitiveMismatchFromType,
				false,
				true,
			},
			{
				TEXT("nested_covariant"),
				TEXT("the nested source reference subtype derives from the nested target subtype"),
				&OuterNestedToType,
				&OuterNestedFromType,
				true,
				false,
			},
		};

		FInspectableCompiler Compiler(Module->builder);
		for (const FCovarianceCase& Case : Cases)
		{
			const bool bAllCovariant = Compiler.AreAllCovariant(*Case.To, *Case.From);
			const bool bFailedCovariance = Compiler.HasFailedCovariance(*Case.To, *Case.From);

			const struct
			{
				const TCHAR* Oracle;
				bool bActual;
				bool bExpected;
			} Observations[] =
			{
				{ TEXT("all_covariant"), bAllCovariant, Case.bExpectedAllCovariant },
				{ TEXT("failed_covariance"), bFailedCovariance, Case.bExpectedFailedCovariance },
			};

			for (const auto& Observation : Observations)
			{
				FString ReviewSource;
				AppendGeneratedAsLine(
					ReviewSource,
					FString::Printf(TEXT("// Relation: %s"), Case.Description));
				AppendGeneratedAsLine(
					ReviewSource,
					FString::Printf(
						TEXT("// Oracle: %s = %s"),
						Observation.Oracle,
						Observation.bExpected ? TEXT("true") : TEXT("false")));
				PrintGeneratedAsSource(
					*TestRunner,
					MakeNativeCaseId(
						"COMPILER-INTERNAL-TEMPLATE-COVARIANCE",
						{ Observation.Oracle, Case.Id }),
					TEXT("CompilerTemplateCovarianceNativeReview"),
					ReviewSource);

				ASSERT_THAT(AreEqual(
					Observation.bExpected,
					Observation.bActual,
					*FString::Printf(
						TEXT("%s should report %s=%s"),
						Case.Description,
						Observation.Oracle,
						Observation.bExpected ? TEXT("true") : TEXT("false"))));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
