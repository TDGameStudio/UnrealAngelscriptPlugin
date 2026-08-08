#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	struct FReferenceBindFamilySource
	{
		FString Registration;
		FString Header;
		FString Implementation;
	};

	static bool LoadReferenceBindFamily(const TCHAR* TypeName, FReferenceBindFamilySource& OutSource)
	{
		const FString BindsDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript/Source/AngelscriptRuntime/Binds"));
		return FFileHelper::LoadFileToString(
				OutSource.Registration,
				*FPaths::Combine(BindsDirectory, FString::Printf(TEXT("Bind_%s.cpp"), TypeName)))
			&& FFileHelper::LoadFileToString(
				OutSource.Header,
				*FPaths::Combine(BindsDirectory, FString::Printf(TEXT("Bind_%s_Functions.h"), TypeName)))
			&& FFileHelper::LoadFileToString(
				OutSource.Implementation,
				*FPaths::Combine(BindsDirectory, FString::Printf(TEXT("Bind_%s_Functions.cpp"), TypeName)));
	}

	static bool LoadBindRegistration(const TCHAR* FileName, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(
			OutSource,
			*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime/Binds"),
				FileName));
	}

	static bool LoadRuntimeSource(const TCHAR* RelativePath, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(
			OutSource,
			*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime"),
				RelativePath));
	}

	static bool LoadPluginSource(const TCHAR* RelativePath, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(
			OutSource,
			*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source"),
				RelativePath));
	}

	static bool ContainsInlineDirectCallableLambda(const FString& RegistrationSource)
	{
		const FRegexPattern InlineLambdaAfterArgumentSeparator(TEXT(",\\s*\\[\\s*\\]\\s*\\("));
		FRegexMatcher Matcher(InlineLambdaAfterArgumentSeparator, RegistrationSource);
		return Matcher.FindNext();
	}

	static int32 CountSourceOccurrences(const FString& Source, const FString& Needle)
	{
		if (Needle.IsEmpty())
			return 0;

		int32 Count = 0;
		int32 SearchFrom = 0;
		while ((SearchFrom = Source.Find(*Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom)) != INDEX_NONE)
		{
			++Count;
			SearchFrom += Needle.Len();
		}
		return Count;
	}

	static const TArray<FString>& GetBindTraitCalls()
	{
		static const TArray<FString> TraitCalls = {
			TEXT(".EditorOnly("),
			TEXT(".Deprecated("),
			TEXT(".PropertyAccessor("),
			TEXT(".GeneratedAccessor("),
			TEXT(".NoDiscard("),
			TEXT(".WorldContext("),
			TEXT(".Callable("),
			TEXT(".ForceConstArgumentExpressions("),
			TEXT(".DeterminesOutputType("),
			TEXT(".PassScriptFunctionAsFirstParam("),
			TEXT(".PassScriptObjectTypeAsFirstParam("),
			TEXT(".Documentation("),
			TEXT(".NativeConstructor("),
			TEXT(".NativeDestructor("),
			TEXT(".NativeAssignment("),
			TEXT(".NativeUObjectCast("),
			TEXT(".NativeMethod("),
			TEXT(".NativeFunction("),
			TEXT(".NativeFunctionHeader("),
			TEXT(".NativeUFunction("),
			TEXT(".NativeTArrayIndex("),
			TEXT(".NativeTArrayIteratorCreate("),
			TEXT(".NativeTArrayIteratorProceed("),
			TEXT(".NativeTemplateInstantiatedCall("),
			TEXT(".NativeDelegateExecute("),
			TEXT(".NativeMulticastExecute("),
			TEXT(".NativeEventFunctionExecute("),
			TEXT(".NativePushArgument("),
			TEXT(".NativePushArgumentRef("),
			TEXT(".CompileOutEntirely("),
			TEXT(".CompileOutAsMethodChain("),
			TEXT(".CompileOutInTest("),
			TEXT(".CompileOutIfNoLog("),
			TEXT(".CompileOutAsEnsure("),
			TEXT(".CompileOutAsCheck("),
			TEXT(".ReplaceWithFirstArgInTest("),
			TEXT(".PureConstant("),
		};
		return TraitCalls;
	}

	static bool ContainsBindRegistrationCall(const FString& Line)
	{
		static const TArray<FString> RegistrationCalls = {
			TEXT(".Method("),
			TEXT(".GenericMethod("),
			TEXT(".Constructor("),
			TEXT(".ImplicitConstructor("),
			TEXT(".Destructor("),
			TEXT(".Factory("),
			TEXT(".Behaviour("),
			TEXT(".TemplateCallback("),
			TEXT(".Property("),
			TEXT("BindGlobalFunctionForTarget("),
			TEXT("BindGlobalGenericFunctionForTarget("),
			TEXT("BindGlobalFunctionDirectForTarget("),
			TEXT("BindMethodDirectForTarget("),
			TEXT("BindGlobalVariableForTarget("),
		};
		for (const FString& RegistrationCall : RegistrationCalls)
		{
			if (Line.Contains(RegistrationCall))
			{
				return true;
			}
		}
		return false;
	}

	static int32 CountBindTraitCalls(const FString& Line)
	{
		int32 Count = 0;
		for (const FString& TraitCall : GetBindTraitCalls())
		{
			Count += CountSourceOccurrences(Line, TraitCall);
		}
		return Count;
	}

	static bool StartsWithBindTraitCall(const FString& Line)
	{
		const FString TrimmedLine = Line.TrimStart();
		for (const FString& TraitCall : GetBindTraitCalls())
		{
			if (TrimmedLine.StartsWith(TraitCall))
			{
				return true;
			}
		}
		return false;
	}

	static void FindBindTraitChainLayoutViolations(
		const FString& FileName,
		const FString& Source,
		TArray<FString>& OutViolations)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			const int32 TraitCount = CountBindTraitCalls(Line);
			if (TraitCount == 0)
			{
				continue;
			}

			if (TraitCount > 1)
			{
				OutViolations.Add(FString::Printf(
					TEXT("%s:%d: multiple traits must each occupy an independent line: %s"),
					*FileName,
					LineIndex + 1,
					*Line.TrimStartAndEnd()));
				continue;
			}

			const bool bStartsWithTrait = StartsWithBindTraitCall(Line);
			if (!bStartsWithTrait)
			{
				if (!ContainsBindRegistrationCall(Line))
				{
					continue;
				}

				const bool bHasFollowingTrait =
					LineIndex + 1 < Lines.Num() && StartsWithBindTraitCall(Lines[LineIndex + 1]);
				if (bHasFollowingTrait)
				{
					OutViolations.Add(FString::Printf(
						TEXT("%s:%d: every trait in a multi-trait chain must use an independent continuation line: %s"),
						*FileName,
						LineIndex + 1,
						*Line.TrimStartAndEnd()));
				}
				else if (Line.Len() > 120)
				{
					OutViolations.Add(FString::Printf(
						TEXT("%s:%d: a single trait must move to a continuation line when the raw line exceeds 120 columns: %s"),
						*FileName,
						LineIndex + 1,
						*Line.TrimStartAndEnd()));
				}
				continue;
			}

			const bool bPartOfMultiTraitChain =
				(LineIndex > 0 && StartsWithBindTraitCall(Lines[LineIndex - 1]))
				|| (LineIndex + 1 < Lines.Num() && StartsWithBindTraitCall(Lines[LineIndex + 1]));
			if (bPartOfMultiTraitChain || LineIndex == 0)
			{
				continue;
			}

			const FString RegistrationLine = Lines[LineIndex - 1].TrimEnd();
			if (!RegistrationLine.TrimStartAndEnd().EndsWith(TEXT(")"))
				|| !ContainsBindRegistrationCall(RegistrationLine))
			{
				continue;
			}

			const FString CombinedLine = RegistrationLine + Line.TrimStartAndEnd();
			if (CombinedLine.Len() <= 120)
			{
				OutViolations.Add(FString::Printf(
					TEXT("%s:%d: a single trait must remain inline when the complete raw line fits within 120 columns: %s"),
					*FileName,
					LineIndex + 1,
					*CombinedLine));
			}
		}
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptBindSourceLayoutTests,
	"Angelscript.TestModule.Engine.BindingArchitecture.SourceLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReferenceFamiliesUseExplicitFileStaticPhasesWithoutLegacyBindObjects)
	{
		FReferenceBindFamilySource Color;
		FReferenceBindFamilySource Vector;
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FColor"), Color), TEXT("FColor reference-family sources should be readable")));
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FVector"), Vector), TEXT("FVector reference-family sources should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("FColor should declare direct infrastructure contribution and manual providers"),
			Color.Registration.Contains(TEXT("const FAngelscriptBind Bind_FColor"))
			&& Color.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
			&& Color.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
			&& Color.Registration.Contains(TEXT("ToStringContribution")));
		bPassed &= TestRunner->TestTrue(TEXT("FVector should split declaration, infrastructure contribution, and manual providers"),
			Vector.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
			&& Vector.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
			&& Vector.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
			&& Vector.Registration.Contains(TEXT("ToStringContribution")));
		bPassed &= TestRunner->TestFalse(TEXT("FColor should not retain a legacy FBind registration"),
			Color.Registration.Contains(TEXT("FAngelscriptBinds::FBind")) || Color.Registration.Contains(TEXT("FAngelscriptBinds::EOrder")));
		bPassed &= TestRunner->TestFalse(TEXT("FVector should not retain a legacy FBind registration"),
			Vector.Registration.Contains(TEXT("FAngelscriptBinds::FBind")) || Vector.Registration.Contains(TEXT("FAngelscriptBinds::EOrder")));
		TestRunner->TestTrue(TEXT("Reference bind families should expose phase ownership at their declaration sites"), bPassed);
	}

	TEST_METHOD(ProjectOwnedCallablesHaveOneSemanticOwnerAndCppDefinitions)
	{
		FReferenceBindFamilySource Color;
		FReferenceBindFamilySource Vector;
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FColor"), Color), TEXT("FColor reference-family sources should be readable")));
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FVector"), Vector), TEXT("FVector reference-family sources should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("FColor should have one primary project-owned callable owner"),
			Color.Header.Contains(TEXT("struct FAngelscriptFColorBinds"))
			&& !Color.Registration.Contains(TEXT("struct FAngelscriptFColorBinds")));
		bPassed &= TestRunner->TestTrue(TEXT("FVector should have one primary project-owned callable owner"),
			Vector.Header.Contains(TEXT("struct FAngelscriptFVectorBinds"))
			&& !Vector.Registration.Contains(TEXT("struct FAngelscriptFVectorBinds")));
		bPassed &= TestRunner->TestTrue(TEXT("FColor callable names should describe behavior"),
			Color.Header.Contains(TEXT("ConstructRGBA"))
			&& Color.Header.Contains(TEXT("ConstructPacked"))
			&& Color.Header.Contains(TEXT("AppendToString")));
		bPassed &= TestRunner->TestTrue(TEXT("FVector callable names should describe behavior"),
			Vector.Header.Contains(TEXT("ConstructXYZ"))
			&& Vector.Header.Contains(TEXT("ConstructZero"))
			&& Vector.Header.Contains(TEXT("ConstructScalar"))
			&& Vector.Header.Contains(TEXT("ConstructCopy"))
			&& Vector.Header.Contains(TEXT("ConstructFromVector3f"))
			&& Vector.Header.Contains(TEXT("AppendToString")));
		bPassed &= TestRunner->TestFalse(TEXT("Non-template FColor callable definitions should not live in the header"),
			Color.Header.Contains(TEXT("FAngelscriptFColorBinds::")));
		bPassed &= TestRunner->TestFalse(TEXT("Non-template FVector callable definitions should not live in the header"),
			Vector.Header.Contains(TEXT("FAngelscriptFVectorBinds::")));
		bPassed &= TestRunner->TestTrue(TEXT("FColor callable definitions should live in the companion cpp"),
			Color.Implementation.Contains(TEXT("FAngelscriptFColorBinds::ConstructRGBA"))
			&& Color.Implementation.Contains(TEXT("FAngelscriptFColorBinds::AppendToString")));
		bPassed &= TestRunner->TestTrue(TEXT("FVector callable definitions should live in the companion cpp"),
			Vector.Implementation.Contains(TEXT("FAngelscriptFVectorBinds::ConstructXYZ"))
			&& Vector.Implementation.Contains(TEXT("FAngelscriptFVectorBinds::AppendToString")));
		TestRunner->TestTrue(TEXT("Reference-family callable ownership should be stable and reviewable"), bPassed);
	}

	TEST_METHOD(ProductionRegistrationSitesUseNamedCallablesButKeepEnginePointerExemptions)
	{
		FReferenceBindFamilySource Color;
		FReferenceBindFamilySource Vector;
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FColor"), Color), TEXT("FColor reference-family sources should be readable")));
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FVector"), Vector), TEXT("FVector reference-family sources should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("FColor hand-written AS registration calls should not embed non-capturing callable lambdas"),
			ContainsInlineDirectCallableLambda(Color.Registration));
		bPassed &= TestRunner->TestFalse(TEXT("FVector hand-written AS registration calls should not embed non-capturing callable lambdas"),
			ContainsInlineDirectCallableLambda(Vector.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("FColor project-owned callables should route through the named owner"),
			Color.Registration.Contains(TEXT("&FAngelscriptFColorBinds::ConstructRGBA"))
			&& Color.Registration.Contains(TEXT("&FAngelscriptFColorBinds::AppendToString")));
		bPassed &= TestRunner->TestTrue(TEXT("FVector project-owned callables should route through the named owner"),
			Vector.Registration.Contains(TEXT("&FAngelscriptFVectorBinds::ConstructXYZ"))
			&& Vector.Registration.Contains(TEXT("&FAngelscriptFVectorBinds::AppendToString")));
		bPassed &= TestRunner->TestTrue(TEXT("Existing engine member/free pointers should remain direct pointer-only exemptions"),
			Color.Registration.Contains(TEXT("&FColor::FromHex"))
			&& Color.Registration.Contains(TEXT("METHOD_TRIVIAL(FColor, ToHex)"))
			&& Vector.Registration.Contains(TEXT("METHOD_TRIVIAL(FVector, Size)"))
			&& Vector.Registration.Contains(TEXT("FUNC_TRIVIAL(FVector::Distance)")));
		bPassed &= TestRunner->TestFalse(TEXT("Pointer-only engine callables should not be copied into FColor's project owner"),
			Color.Header.Contains(TEXT("FromHex")) || Color.Header.Contains(TEXT("ToHex")));
		bPassed &= TestRunner->TestFalse(TEXT("Pointer-only engine callables should not be copied into FVector's project owner"),
			Vector.Header.Contains(TEXT("Distance")) || Vector.Header.Contains(TEXT("Size")));
		TestRunner->TestTrue(TEXT("Named-callable policy should stay narrow enough for existing pointer registrations"), bPassed);
	}

	TEST_METHOD(ParallelWaveProvidersUseDirectPhasesAndNamedCallableOwners)
	{
		struct FProviderExpectation
		{
			const TCHAR* FileStem;
			const TCHAR* OwnerName;
			bool bNeedsTypeDeclarations;
			bool bNeedsTypeInfrastructure;
			bool bNeedsToStringContribution;
		};

		static const FProviderExpectation Providers[] = {
			{TEXT("FCollisionQueryParams"), TEXT("FAngelscriptFCollisionQueryParamsBinds"), true, true, false},
			{TEXT("InputEvents"), TEXT("FAngelscriptInputEventsBinds"), false, true, true},
			{TEXT("Json"), TEXT("FAngelscriptJsonBinds"), true, false, false},
		};

		bool bPassed = true;
		for (const FProviderExpectation& Provider : Providers)
		{
			FReferenceBindFamilySource Family;
			if (!TestRunner->TestTrue(
				*FString::Printf(TEXT("%s provider family should be readable"), Provider.FileStem),
				LoadReferenceBindFamily(Provider.FileStem, Family)))
			{
				bPassed = false;
				continue;
			}

			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a legacy provider"), Provider.FileStem),
				Family.Registration.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Family.Registration.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
					|| Family.Registration.Contains(TEXT("FAngelscriptBinds::EOrder")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should register callable surface in ManualBindings"), Provider.FileStem),
				Family.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings")));
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s declaration phase should match its provider responsibilities"), Provider.FileStem),
				Family.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations")),
				Provider.bNeedsTypeDeclarations);
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s infrastructure phase should match its provider responsibilities"), Provider.FileStem),
				Family.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure")),
				Provider.bNeedsTypeInfrastructure);
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s ToString phase should match its provider responsibilities"), Provider.FileStem),
				Family.Registration.Contains(TEXT("ToStringContribution")),
				Provider.bNeedsToStringContribution);
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should own project callables in %s"), Provider.FileStem, Provider.OwnerName),
				Family.Header.Contains(Provider.OwnerName)
					&& Family.Implementation.Contains(FString::Printf(TEXT("%s::"), Provider.OwnerName)));
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s registration should not embed direct callable lambdas"), Provider.FileStem),
				ContainsInlineDirectCallableLambda(Family.Registration));
		}

		TestRunner->TestTrue(TEXT("Parallel migration wave should preserve direct phases and stable callable ownership"), bPassed);
	}

	TEST_METHOD(CoreProviderWaveUsesDirectExplicitCallbacks)
	{
		FReferenceBindFamilySource Math;
		FReferenceBindFamilySource SoftObjectPtr;
		FString Primitives;
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FMath"), Math), TEXT("FMath provider family should be readable")));
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("TSoftObjectPtr"), SoftObjectPtr), TEXT("TSoftObjectPtr provider family should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_Primitives.cpp"), Primitives), TEXT("Primitive provider source should be readable")));

		const auto HasLegacyProvider = [](const FString& Source)
		{
			return Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Source.Contains(TEXT("FAngelscriptBinds::EOrder"));
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("FMath should not retain a legacy provider"), HasLegacyProvider(Math.Registration));
		bPassed &= TestRunner->TestFalse(TEXT("TSoftObjectPtr should not retain a legacy provider"), HasLegacyProvider(SoftObjectPtr.Registration));
		bPassed &= TestRunner->TestFalse(TEXT("Primitives should not retain a legacy provider"), HasLegacyProvider(Primitives));
		bPassed &= TestRunner->TestTrue(TEXT("FMath should use a direct manual callback and one named callable owner"),
			Math.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& Math.Header.Contains(TEXT("FAngelscriptFMathBinds"))
				&& Math.Implementation.Contains(TEXT("FAngelscriptFMathBinds::"))
				&& !ContainsInlineDirectCallableLambda(Math.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("TSoftObjectPtr should split declaration, infrastructure, and manual callbacks"),
			SoftObjectPtr.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
				&& SoftObjectPtr.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
				&& SoftObjectPtr.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& SoftObjectPtr.Header.Contains(TEXT("FAngelscriptTSoftObjectPtrBinds"))
				&& SoftObjectPtr.Implementation.Contains(TEXT("FAngelscriptTSoftObjectPtrBinds::"))
				&& !ContainsInlineDirectCallableLambda(SoftObjectPtr.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("Primitives should explicitly target adapters, constants, and ToString contributions"),
			Primitives.Contains(TEXT("Binds.GetTargetTypeDatabase"))
				&& Primitives.Contains(TEXT("Binds.RegisterTypeForTarget"))
				&& Primitives.Contains(TEXT("Binds.BindGlobalVariableForTarget"))
				&& Primitives.Contains(TEXT(".PureConstant("))
				&& Primitives.Contains(TEXT("FToStringHelper::Register(Binds"))
				&& !Primitives.Contains(TEXT("PreviousBind")));
		TestRunner->TestTrue(TEXT("Core provider wave should preserve explicit phases and stable callable ownership"), bPassed);
	}

	TEST_METHOD(SecondCoreProviderWaveUsesDirectExplicitCallbacks)
	{
		FReferenceBindFamilySource String;
		FReferenceBindFamilySource WorldCollision;
		FString Delegates;
		FString DelegateHeader;
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("FString"), String), TEXT("FString provider family should be readable")));
		ASSERT_THAT(IsTrue(LoadReferenceBindFamily(TEXT("WorldCollision"), WorldCollision), TEXT("WorldCollision provider family should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_Delegates.cpp"), Delegates), TEXT("Delegate provider source should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_Delegates.h"), DelegateHeader), TEXT("Delegate callable header should be readable")));

		const auto HasLegacyProvider = [](const FString& Source)
		{
			return Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Source.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| Source.Contains(TEXT("PreviousBind"));
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("FString should not retain a legacy provider or ambient previous-bind mutation"), HasLegacyProvider(String.Registration));
		bPassed &= TestRunner->TestFalse(TEXT("WorldCollision should not retain a legacy provider or ambient previous-bind mutation"), HasLegacyProvider(WorldCollision.Registration));
		bPassed &= TestRunner->TestFalse(TEXT("Delegates should not retain a legacy provider or ambient previous-bind mutation"), HasLegacyProvider(Delegates));
		bPassed &= TestRunner->TestTrue(TEXT("FString should split declaration, infrastructure, and manual callbacks with a named callable owner"),
			String.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
				&& String.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
				&& String.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& String.Registration.Contains(TEXT("Binds.GetTargetToStringList"))
				&& String.Header.Contains(TEXT("FAngelscriptFStringBinds"))
				&& String.Implementation.Contains(TEXT("FAngelscriptFStringBinds::"))
				&& !ContainsInlineDirectCallableLambda(String.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("WorldCollision should split type and callable phases with explicit target registration and a named callable owner"),
			WorldCollision.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
				&& WorldCollision.Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
				&& WorldCollision.Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& WorldCollision.Registration.Contains(TEXT("Binds.BindGlobalFunctionForTarget"))
				&& WorldCollision.Header.Contains(TEXT("FAngelscriptWorldCollisionBinds"))
				&& WorldCollision.Implementation.Contains(TEXT("FAngelscriptWorldCollisionBinds::"))
				&& !ContainsInlineDirectCallableLambda(WorldCollision.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("Delegates should split declaration and callable phases while retaining its established header-visible ABI owners"),
			Delegates.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
				&& Delegates.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& Delegates.Contains(TEXT("Binds.GetTargetBindDatabase"))
				&& Delegates.Contains(TEXT("Binds.GetTargetTypeDatabase"))
				&& DelegateHeader.Contains(TEXT("FAngelscriptSparseDelegateOperations"))
				&& Delegates.Contains(TEXT("FAngelscriptSparseDelegateOperations::"))
				&& !ContainsInlineDirectCallableLambda(Delegates));
		TestRunner->TestTrue(TEXT("Second core provider wave should preserve explicit phases, target ownership, and named callables"), bPassed);
	}

	TEST_METHOD(ContainerTemplateProvidersInstallDirectMethodSurfaceBeforeInfrastructure)
	{
		struct FContainerProviderExpectation
		{
			const TCHAR* TypeName;
			const TCHAR* OwnerName;
			int32 SpecializedNativeTraitCount;
			int32 TemplateNativeTraitCount;
			int32 ScriptObjectTypeTraitCount;
			int32 DocumentationCount;
		};

		static const FContainerProviderExpectation Providers[] = {
			{TEXT("TArray"), TEXT("FArrayOperations::"), 6, 24, 35, 4},
			{TEXT("TSet"), TEXT("FAngelscriptSetBinds::"), 2, 10, 18, 0},
			{TEXT("TMap"), TEXT("FAngelscriptMapBinds::"), 2, 13, 25, 5},
		};

		bool bPassed = true;
		for (const FContainerProviderExpectation& Provider : Providers)
		{
			FString Source;
			if (!TestRunner->TestTrue(
				*FString::Printf(TEXT("%s provider source should be readable"), Provider.TypeName),
				LoadBindRegistration(*FString::Printf(TEXT("Bind_%s.cpp"), Provider.TypeName), Source)))
			{
				bPassed = false;
				continue;
			}

			const FString DeclarationName = FString::Printf(TEXT("%s.Declaration"), Provider.TypeName);
			const FString MethodSurfaceName = FString::Printf(TEXT("%s.MethodSurface"), Provider.TypeName);
			const FString InfrastructureName = FString::Printf(TEXT("%s.TypeInfrastructure"), Provider.TypeName);

			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a legacy provider or ambient previous-bind mutation"), Provider.TypeName),
				Source.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
					|| Source.Contains(TEXT("FAngelscriptBinds::EOrder"))
					|| Source.Contains(TEXT("PreviousBind")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should declare one template declaration provider and two ordered infrastructure providers"), Provider.TypeName),
				Source.Contains(DeclarationName)
					&& Source.Contains(MethodSurfaceName)
					&& Source.Contains(InfrastructureName)
					&& CountSourceOccurrences(Source, TEXT("EAngelscriptBindPhase::TypeDeclarations")) == 1
					&& CountSourceOccurrences(Source, TEXT("EAngelscriptBindPhase::TypeInfrastructure")) == 2
					&& !Source.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
					&& MethodSurfaceName.Compare(InfrastructureName, ESearchCase::CaseSensitive) < 0);
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should explicitly target template declarations, method lookup, adapters, and finders"), Provider.TypeName),
				Source.Contains(TEXT("Binds.ValueClassForTarget"))
					&& Source.Contains(TEXT("Binds.ExistingClassForTarget"))
					&& Source.Contains(TEXT("Binds.RegisterTypeForTarget"))
					&& Source.Contains(TEXT("Binds.GetTargetTypeDatabase"))
					&& Source.Contains(TEXT("Binds.RegisterTypeFinderForTarget")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should retain its established callable owner"), Provider.TypeName),
				Source.Contains(Provider.OwnerName));
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s registration should not embed direct AS callable lambdas"), Provider.TypeName),
				ContainsInlineDirectCallableLambda(Source));
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s should attach every specialized container native form to its exact result"), Provider.TypeName),
				CountSourceOccurrences(Source, TEXT(".NativeTArray")),
				Provider.SpecializedNativeTraitCount);
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain non-template native metadata macros"), Provider.TypeName),
				Source.Contains(TEXT("SCRIPT_NATIVE_")));
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s should attach every template native form to its exact result"), Provider.TypeName),
				CountSourceOccurrences(Source, TEXT(".NativeTemplateInstantiatedCall(")),
				Provider.TemplateNativeTraitCount);
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain ambient template-native metadata macros"), Provider.TypeName),
				Source.Contains(TEXT("SCRIPT_NATIVE_TEMPLATED_CALL")));
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s should attach hidden template metadata to the exact fluent result"), Provider.TypeName),
				CountSourceOccurrences(Source, TEXT(".PassScriptObjectTypeAsFirstParam()")),
				Provider.ScriptObjectTypeTraitCount);
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s should preserve documentation attachments on exact fluent results"), Provider.TypeName),
				CountSourceOccurrences(Source, TEXT(".Documentation(")),
				Provider.DocumentationCount);
		}

		TestRunner->TestTrue(TEXT("Container templates should install their complete direct method surface before specialization infrastructure"), bPassed);
	}

	TEST_METHOD(SpecializedNativeMetadataAndBlueprintEventHelpersUseExactResults)
	{
		FString BindsHeader;
		FString BindsImplementation;
		FString DocsHeader;
		FString StaticJITHeader;
		FString StaticJITImplementation;
		FString BlueprintEvents;
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBinds.h"), BindsHeader), TEXT("Direct bind fluent declarations should be readable")));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBinds.cpp"), BindsImplementation), TEXT("Direct bind fluent definitions should be readable")));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptDocs.h"), DocsHeader), TEXT("Direct bind documentation declarations should be readable")));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("StaticJIT/StaticJITBinds.h"), StaticJITHeader), TEXT("StaticJIT bind declarations should be readable")));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("StaticJIT/StaticJITBinds.cpp"), StaticJITImplementation), TEXT("StaticJIT bind definitions should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_BlueprintEvent.cpp"), BlueprintEvents), TEXT("Blueprint event provider source should be readable")));

		struct FSpecializedNativeExpectation
		{
			const TCHAR* FluentName;
			const TCHAR* StaticJITName;
		};

		static const FSpecializedNativeExpectation Expectations[] = {
			{TEXT("NativeConstructor"), TEXT("BindNativeConstructor")},
			{TEXT("NativeDestructor"), TEXT("BindNativeDestructor")},
			{TEXT("NativeAssignment"), TEXT("BindNativeAssignment")},
			{TEXT("NativeUObjectCast"), TEXT("BindNativeUObjectCast")},
			{TEXT("NativeMethod"), TEXT("BindNativeMethod")},
			{TEXT("NativeFunction"), TEXT("BindNativeFunction")},
			{TEXT("NativeFunctionHeader"), TEXT("BindNativeFunctionHeader")},
			{TEXT("NativeUFunction"), TEXT("BindUFunction")},
			{TEXT("NativeTArrayIndex"), TEXT("BindTArrayIndex")},
			{TEXT("NativeTArrayIteratorCreate"), TEXT("BindTArrayIteratorCreate")},
			{TEXT("NativeTArrayIteratorProceed"), TEXT("BindTArrayIteratorProceed")},
			{TEXT("NativeTemplateInstantiatedCall"), TEXT("BindTemplateInstantiatedCall")},
			{TEXT("NativeDelegateExecute"), TEXT("BindDelegateExecute")},
			{TEXT("NativeMulticastExecute"), TEXT("BindMulticastExecute")},
			{TEXT("NativeEventFunctionExecute"), TEXT("BindEventFunctionExecute")},
			{TEXT("NativePushArgument"), TEXT("BindPushArg")},
			{TEXT("NativePushArgumentRef"), TEXT("BindPushArgRef")},
		};

		bool bPassed = true;
		for (const FSpecializedNativeExpectation& Expectation : Expectations)
		{
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should be declared and defined on the exact fluent result"), Expectation.FluentName),
				BindsHeader.Contains(FString::Printf(TEXT("FAngelscriptBoundFunction& %s("), Expectation.FluentName))
					&& BindsImplementation.Contains(FString::Printf(TEXT("FAngelscriptBoundFunction::%s("), Expectation.FluentName)));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should expose an explicit engine/function StaticJIT overload"), Expectation.StaticJITName),
				StaticJITHeader.Contains(FString::Printf(TEXT("%s(FAngelscriptEngine& Engine, asIScriptFunction* ScriptFunction"), Expectation.StaticJITName))
					&& CountSourceOccurrences(
						StaticJITImplementation,
						FString::Printf(TEXT("FScriptFunctionNativeForm::%s("), Expectation.StaticJITName))
						== 1);
		}

		bPassed &= TestRunner->TestFalse(
			TEXT("Native metadata should not expose ambient FAngelscriptBinds overloads or macro facades"),
			StaticJITHeader.Contains(TEXT("(FAngelscriptBinds&"))
				|| StaticJITHeader.Contains(TEXT("#define SCRIPT_NATIVE_"))
				|| StaticJITHeader.Contains(TEXT("#define SCRIPT_TRIVIAL_NATIVE_")));
		bPassed &= TestRunner->TestFalse(
			TEXT("Native metadata should never attach through ambient previous-bind state"),
			StaticJITImplementation.Contains(TEXT("AddNativeFormToPrevious")));
		bPassed &= TestRunner->TestFalse(
			TEXT("Core binding state should not retain previous function/property ids or previous-bind mutation APIs"),
			BindsHeader.Contains(TEXT("PreviouslyBoundFunction"))
				|| BindsHeader.Contains(TEXT("PreviouslyBoundGlobalProperty"))
				|| BindsHeader.Contains(TEXT("PreviousBind"))
				|| BindsImplementation.Contains(TEXT("PreviouslyBoundFunction"))
				|| BindsImplementation.Contains(TEXT("PreviouslyBoundGlobalProperty"))
				|| BindsImplementation.Contains(TEXT("PreviousBind")));
		bPassed &= TestRunner->TestFalse(
			TEXT("Documentation should not expose macros that resolve an ambient previous binding"),
			DocsHeader.Contains(TEXT("SCRIPT_BIND_DOCUMENTATION"))
				|| DocsHeader.Contains(TEXT("SCRIPT_GLOBAL_DOCUMENTATION")));

		bPassed &= TestRunner->TestTrue(TEXT("Blueprint event helpers should be a direct manual provider with explicit engine and type-database ownership"),
			BlueprintEvents.Contains(TEXT("BlueprintEvents.HelperGlobals"))
				&& BlueprintEvents.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& BlueprintEvents.Contains(TEXT("Binds.GetTargetEngine()"))
				&& BlueprintEvents.Contains(TEXT("Binds.GetTargetTypeDatabase()"))
				&& BlueprintEvents.Contains(TEXT("Binds.BindGlobalFunctionForTarget")));
		bPassed &= TestRunner->TestTrue(TEXT("Blueprint event helpers should attach every specialized native form to the exact result"),
			CountSourceOccurrences(BlueprintEvents, TEXT(".NativePushArgument()")) == 2
				&& CountSourceOccurrences(BlueprintEvents, TEXT(".NativePushArgumentRef()")) == 2
				&& CountSourceOccurrences(BlueprintEvents, TEXT(".NativeEventFunctionExecute()")) == 1
				&& CountSourceOccurrences(BlueprintEvents, TEXT(".NativeDelegateExecute()")) == 1
				&& CountSourceOccurrences(BlueprintEvents, TEXT(".NativeMulticastExecute()")) == 1);
		bPassed &= TestRunner->TestTrue(TEXT("Blueprint event helper callables should have one file-private named owner"),
			BlueprintEvents.Contains(TEXT("struct FAngelscriptBlueprintEventHelperBinds"))
				&& BlueprintEvents.Contains(TEXT("&FAngelscriptBlueprintEventHelperBinds::PushArgumentSpecialized"))
				&& BlueprintEvents.Contains(TEXT("&FAngelscriptBlueprintEventHelperBinds::ExecuteMulticastDelegate"))
				&& !ContainsInlineDirectCallableLambda(BlueprintEvents));
		bPassed &= TestRunner->TestFalse(TEXT("Blueprint event helpers should not retain a legacy provider"),
			BlueprintEvents.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| BlueprintEvents.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| BlueprintEvents.Contains(TEXT("FAngelscriptBinds::EOrder")));
		TestRunner->TestTrue(TEXT("Specialized native metadata should stay exact through the Blueprint event helper migration"), bPassed);
	}

	TEST_METHOD(RemainingNativeMetadataProvidersUseExactFluentResults)
	{
		struct FProviderExpectation
		{
			const TCHAR* FileName;
			const TCHAR* FluentCall;
			int32 ExpectedCount;
		};

		static const FProviderExpectation Providers[] = {
			{TEXT("Bind_FCollisionQueryParams.cpp"), TEXT(".NativeConstructor("), 16},
			{TEXT("Bind_FLinearColor.cpp"), TEXT(".NativeAssignment("), 1},
			{TEXT("Bind_TArray.cpp"), TEXT(".NativeTArrayIndex()"), 2},
			{TEXT("Bind_TArray.cpp"), TEXT(".NativeTArrayIteratorCreate()"), 2},
			{TEXT("Bind_TArray.cpp"), TEXT(".NativeTArrayIteratorProceed()"), 2},
			{TEXT("Bind_TMap.cpp"), TEXT(".NativeTArrayIteratorCreate()"), 2},
			{TEXT("Bind_TSet.cpp"), TEXT(".NativeTArrayIteratorCreate()"), 2},
		};

		TMap<FString, FString> Sources;
		bool bPassed = true;
		for (const FProviderExpectation& Provider : Providers)
		{
			FString& Source = Sources.FindOrAdd(Provider.FileName);
			if (Source.IsEmpty())
			{
				bPassed &= TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should be readable"), Provider.FileName),
					LoadBindRegistration(Provider.FileName, Source));
			}
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("%s should preserve every exact %s attachment"), Provider.FileName, Provider.FluentCall),
				CountSourceOccurrences(Source, Provider.FluentCall),
				Provider.ExpectedCount);
		}

		for (const TPair<FString, FString>& Provider : Sources)
		{
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a native metadata macro call"), *Provider.Key),
				Provider.Value.Contains(TEXT("SCRIPT_NATIVE_"))
					|| Provider.Value.Contains(TEXT("SCRIPT_TRIVIAL_NATIVE_")));
		}

		TestRunner->TestTrue(TEXT("Remaining native metadata providers should attach through exact fluent results"), bPassed);
	}

	TEST_METHOD(BlueprintTypeFoundationUsesExplicitPhasesFacadeAndStableEngineSnapshot)
	{
		FString BlueprintType;
		FString BindsHeader;
		ASSERT_THAT(IsTrue(
			LoadBindRegistration(TEXT("Bind_BlueprintType.cpp"), BlueprintType),
			TEXT("BlueprintType provider source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptBinds.h"), BindsHeader),
			TEXT("Explicit bind state declarations should be readable")));

		const int32 MethodSurfacePosition = BlueprintType.Find(
			TEXT("Bind_TObjectPtr_MethodSurface"),
			ESearchCase::CaseSensitive);
		const int32 InfrastructurePosition = BlueprintType.Find(
			TEXT("Bind_BlueprintType_Infrastructure"),
			ESearchCase::CaseSensitive);

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("BlueprintType should expose its foundation plus one direct reflection provider"),
			CountSourceOccurrences(BlueprintType, TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_")) == 10
				&& CountSourceOccurrences(BlueprintType, TEXT("EAngelscriptBindPhase::TypeDeclarations")) == 4
				&& CountSourceOccurrences(BlueprintType, TEXT("EAngelscriptBindPhase::TypeInfrastructure")) == 4
				&& CountSourceOccurrences(BlueprintType, TEXT("EAngelscriptBindPhase::ManualBindings")) == 1
				&& CountSourceOccurrences(BlueprintType, TEXT("EAngelscriptBindPhase::ReflectionBindings")) == 1
				&& MethodSurfacePosition != INDEX_NONE
				&& InfrastructurePosition != INDEX_NONE
				&& MethodSurfacePosition < InfrastructurePosition);
		bPassed &= TestRunner->TestFalse(TEXT("BlueprintType reflection should not retain legacy integer-order providers"),
			BlueprintType.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| BlueprintType.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| BlueprintType.Contains(TEXT("FAngelscriptBinds::RegisterBinds")));
		bPassed &= TestRunner->TestTrue(TEXT("BlueprintType reflection should execute through one explicit-target callback"),
			BlueprintType.Contains(TEXT("BindBlueprintTypeReflectionBindings(FAngelscriptBinds& Binds)"))
				&& BlueprintType.Contains(TEXT("Bind_BlueprintType_ReflectionBindings"))
				&& BlueprintType.Contains(TEXT("Binds.GetTargetScriptEngine()"))
				&& BlueprintType.Contains(TEXT("Binds.GetTargetBindDatabase()"))
				&& BlueprintType.Contains(TEXT("Binds.ExistingClassForTarget")));
		bPassed &= TestRunner->TestTrue(TEXT("UClass declarations should use the fail-closed explicit reference facade"),
			BlueprintType.Contains(TEXT("Binds.ReferenceClassForTarget"))
				&& !BlueprintType.Contains(TEXT("RegisterObjectType("))
				&& !BlueprintType.Contains(TEXT("plainUserData")));
		bPassed &= TestRunner->TestTrue(TEXT("BlueprintType policy should read configuration only from its selected engine"),
			BlueprintType.Contains(TEXT("IsEditorOnlyClassForTarget"))
				&& BlueprintType.Contains(TEXT("ShouldDisallowInstantiationForTarget"))
				&& BlueprintType.Contains(TEXT("ShouldBindEngineTypeForTarget"))
				&& BlueprintType.Contains(TEXT("Binds.GetTargetEngine()")));
		bPassed &= TestRunner->TestTrue(TEXT("Non-database class discovery should capture one engine-owned snapshot for every phase"),
			BlueprintType.Contains(TEXT("GetOrCaptureBlueprintTypeClasses"))
				&& BindsHeader.Contains(TEXT("BlueprintTypeClassSnapshot"))
				&& BindsHeader.Contains(TEXT("bBlueprintTypeClassSnapshotCaptured")));
		bPassed &= TestRunner->TestTrue(TEXT("BlueprintType callables and traits should use the named owner and exact results"),
			BlueprintType.Contains(TEXT("struct FAngelscriptBlueprintTypeBinds"))
				&& BlueprintType.Contains(TEXT("&FAngelscriptBlueprintTypeBinds::ValidateObjectPtrTemplate"))
				&& BlueprintType.Contains(TEXT(".PassScriptObjectTypeAsFirstParam()"))
				&& !BlueprintType.Contains(TEXT("PreviousBind")));
		TestRunner->TestTrue(TEXT("BlueprintType foundation should remain explicit, stable, and fail closed"), bPassed);
	}

	TEST_METHOD(ReflectionCallableEventAndFallbackUseExplicitTargetsAndExactResults)
	{
		FString BlueprintType;
		FString BlueprintCallable;
		FString BlueprintEvent;
		FString ReflectiveFallback;
		FString FunctionSignature;
		ASSERT_THAT(IsTrue(
			LoadBindRegistration(TEXT("Bind_BlueprintType.cpp"), BlueprintType)
				&& LoadBindRegistration(TEXT("Bind_BlueprintCallable.cpp"), BlueprintCallable)
				&& LoadBindRegistration(TEXT("Bind_BlueprintEvent.cpp"), BlueprintEvent)
				&& LoadBindRegistration(TEXT("BlueprintCallableReflectiveFallback.cpp"), ReflectiveFallback)
				&& LoadBindRegistration(TEXT("Helper_FunctionSignature.h"), FunctionSignature),
			TEXT("Reflection binding sources should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("BlueprintType should own one explicit reflection callback and pass its context to every commit path"),
			BlueprintType.Contains(TEXT("BindBlueprintTypeReflectionBindings(FAngelscriptBinds& Binds)"))
				&& BlueprintType.Contains(TEXT("Bind_BlueprintType_ReflectionBindings"))
				&& BlueprintType.Contains(TEXT("EAngelscriptBindPhase::ReflectionBindings"))
				&& BlueprintType.Contains(TEXT("BindBlueprintCallable(Binds,"))
				&& BlueprintType.Contains(TEXT("BindBlueprintEvent(Binds,"))
				&& BlueprintType.Contains(TEXT("BindBlueprintCallable_FromPrep(Binds,"))
				&& BlueprintType.Contains(TEXT("BindBlueprintEvent_FromPrep(Binds,"))
				&& BlueprintType.Contains(TEXT("Binds.GetTargetScriptEngine()"))
				&& BlueprintType.Contains(TEXT("Binds.GetTargetBindDatabase()"))
				&& BlueprintType.Contains(TEXT("Binds.ExistingClassForTarget")));
		bPassed &= TestRunner->TestFalse(TEXT("BlueprintType reflection should not call ambient reflection bind entry points"),
			BlueprintType.Contains(TEXT("BindBlueprintCallable(ClassType"))
				|| BlueprintType.Contains(TEXT("BindBlueprintEvent(ClassType")));

		bPassed &= TestRunner->TestTrue(TEXT("BlueprintCallable should resolve and register only through its explicit target"),
			BlueprintCallable.Contains(TEXT("FAngelscriptBinds& Binds"))
				&& BlueprintCallable.Contains(TEXT("Binds.GetTargetBindState().ClassFunctionBindings"))
				&& BlueprintCallable.Contains(TEXT("BindBlueprintCallableReflectionFallback("))
				&& BlueprintCallable.Contains(TEXT("Binds.GetTargetEngine(), Signature.ClassName"))
				&& BlueprintCallable.Contains(TEXT("Binds.BindGlobalFunctionDirectForTarget"))
				&& BlueprintCallable.Contains(TEXT("Binds.BindMethodDirectForTarget"))
				&& CountSourceOccurrences(BlueprintCallable, TEXT("FAngelscriptBoundFunction PrimaryBinding")) >= 2
				&& CountSourceOccurrences(BlueprintCallable, TEXT("Signature.ModifyScriptFunction(PrimaryBinding)")) >= 4
				&& CountSourceOccurrences(BlueprintCallable, TEXT("PrimaryBinding.NativeUFunction(")) >= 4);
		bPassed &= TestRunner->TestFalse(TEXT("BlueprintCallable should not retain ambient registration or integer-result metadata"),
			BlueprintCallable.Contains(TEXT("FAngelscriptEngine::Get()"))
				|| BlueprintCallable.Contains(TEXT("FAngelscriptBinds::BindGlobalFunction("))
				|| BlueprintCallable.Contains(TEXT("FAngelscriptBinds::BindGlobalFunctionDirect("))
				|| BlueprintCallable.Contains(TEXT("FAngelscriptBinds::BindMethodDirect("))
				|| BlueprintCallable.Contains(TEXT("SCRIPT_NATIVE_UFUNCTION"))
				|| BlueprintCallable.Contains(TEXT("ModifyScriptFunction(FunctionId)"))
				|| BlueprintCallable.Contains(TEXT("ModifyScriptFunction(GlobalFunctionId)"))
				|| BlueprintCallable.Contains(TEXT("ModifyScriptFunction(NamespacedFunctionId)")));

		bPassed &= TestRunner->TestTrue(TEXT("BlueprintEvent should attach callable and native metadata to its exact primary result"),
			BlueprintEvent.Contains(TEXT("CommitBlueprintEventBinding("))
				&& BlueprintEvent.Contains(TEXT("NewOwnedBlueprintEventSignature(Binds)"))
				&& BlueprintEvent.Contains(TEXT("BindGlobalFunctionDirectForTarget"))
				&& BlueprintEvent.Contains(TEXT("BindMethodDirectForTarget"))
				&& BlueprintEvent.Contains(TEXT("Signature.ModifyScriptFunction(PrimaryBinding)"))
				&& BlueprintEvent.Contains(TEXT("PrimaryBinding.Callable(false)"))
				&& BlueprintEvent.Contains(TEXT("PrimaryBinding.NativeUFunction(Function, NativeFunctionName, false)"))
				&& BlueprintEvent.Contains(TEXT("InvokeReflectionFallbackFromGenericCall")));
		bPassed &= TestRunner->TestFalse(TEXT("BlueprintEvent should not retain an ambient reflection registration wrapper"),
			BlueprintEvent.Contains(TEXT("Legacy ambient-engine entry point"))
				|| BlueprintEvent.Contains(TEXT("FAngelscriptEngine::Get()"))
				|| BlueprintEvent.Contains(TEXT("SCRIPT_NATIVE_UFUNCTION"))
				|| BlueprintEvent.Contains(TEXT("SetPreviousBindIsCallable")));

		bPassed &= TestRunner->TestTrue(TEXT("Reflective fallback should preserve generic and RPC routing while targeting an exact result"),
			ReflectiveFallback.Contains(TEXT("BindBlueprintCallableReflectionFallback("))
				&& ReflectiveFallback.Contains(TEXT("FAngelscriptBinds& Binds"))
				&& ReflectiveFallback.Contains(TEXT("Binds.BindGlobalFunctionDirectForTarget"))
				&& ReflectiveFallback.Contains(TEXT("Binds.BindMethodDirectForTarget"))
				&& ReflectiveFallback.Contains(TEXT("Signature.ModifyScriptFunction(OutPrimaryBinding)"))
				&& ReflectiveFallback.Contains(TEXT("Binding.bReflectiveFallbackBound = true"))
				&& ReflectiveFallback.Contains(TEXT("Binding.bUsesGenericCall = true"))
				&& ReflectiveFallback.Contains(TEXT("Binding.Origin = EAngelscriptFunctionBindingOrigin::Reflective"))
				&& ReflectiveFallback.Contains(TEXT("FUNC_Net"))
				&& ReflectiveFallback.Contains(TEXT("GetFunctionCallspace"))
				&& ReflectiveFallback.Contains(TEXT("CallRemoteFunction"))
				&& ReflectiveFallback.Contains(TEXT("Function->Invoke"))
				&& ReflectiveFallback.Contains(TEXT("ProcessEvent")));
		bPassed &= TestRunner->TestFalse(TEXT("Reflective fallback should not register through an ambient engine or integer result"),
			ReflectiveFallback.Contains(TEXT("FAngelscriptEngine::Get()"))
				|| ReflectiveFallback.Contains(TEXT("FAngelscriptBinds::BindGlobalFunctionDirect("))
				|| ReflectiveFallback.Contains(TEXT("FAngelscriptBinds::BindMethodDirect("))
				|| ReflectiveFallback.Contains(TEXT("ModifyScriptFunction(FunctionId)"))
				|| ReflectiveFallback.Contains(TEXT("ModifyScriptFunction(GlobalFunctionId)"))
				|| ReflectiveFallback.Contains(TEXT("ModifyScriptFunction(NamespacedFunctionId)")));

		bPassed &= TestRunner->TestTrue(TEXT("FunctionSignature should mutate the exact bound function and selected engine"),
			FunctionSignature.Contains(TEXT("ModifyScriptFunction(FAngelscriptBoundFunction& BoundFunction)"))
				&& FunctionSignature.Contains(TEXT("if (!BoundFunction.IsValid())"))
				&& FunctionSignature.Contains(TEXT("BoundFunction.GetTargetEngine()"))
				&& FunctionSignature.Contains(TEXT("BoundFunction.GetFunction()"))
				&& FunctionSignature.Contains(TEXT("IsEditorOnlyClassForTarget(TargetEngine, Class)"))
				&& !FunctionSignature.Contains(TEXT("ModifyScriptFunction(int FunctionId)"))
				&& !FunctionSignature.Contains(TEXT("FAngelscriptEngine::Get()")));
		TestRunner->TestTrue(TEXT("Reflection binding should remain explicit, exact, generic, and RPC-safe"), bPassed);
	}

	TEST_METHOD(UObjectAndUStructProvidersUseDirectPhasesNamedOwnersAndExactTraits)
	{
		FReferenceBindFamilySource UObjectFamily;
		FReferenceBindFamilySource UStructFamily;
		FString BindsHeader;
		ASSERT_THAT(IsTrue(
			LoadReferenceBindFamily(TEXT("UObject"), UObjectFamily),
			TEXT("UObject provider family should be readable")));
		ASSERT_THAT(IsTrue(
			LoadReferenceBindFamily(TEXT("UStruct"), UStructFamily),
			TEXT("UStruct provider family should be readable")));
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptBinds.h"), BindsHeader),
			TEXT("Explicit bind state declarations should be readable")));

		const auto HasLegacyProvider = [](const FString& Source)
		{
			return Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Source.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| Source.Contains(TEXT("PreviousBind"));
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("UObject should expose four manual providers and one infrastructure contribution"),
			CountSourceOccurrences(UObjectFamily.Registration, TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_")) == 5
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT("EAngelscriptBindPhase::ManualBindings")) == 4
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT("EAngelscriptBindPhase::TypeInfrastructure")) == 1);
		bPassed &= TestRunner->TestFalse(TEXT("UObject should not retain legacy providers or direct callable lambdas"),
			HasLegacyProvider(UObjectFamily.Registration)
				|| ContainsInlineDirectCallableLambda(UObjectFamily.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("UObject project-owned callables should live in one companion owner"),
			UObjectFamily.Header.Contains(TEXT("struct FAngelscriptUObjectBinds"))
				&& !UObjectFamily.Registration.Contains(TEXT("struct FAngelscriptUObjectBinds"))
				&& UObjectFamily.Implementation.Contains(TEXT("FAngelscriptUObjectBinds::CreateObject"))
				&& UObjectFamily.Implementation.Contains(TEXT("FAngelscriptUObjectBinds::CastToType")));
		bPassed &= TestRunner->TestTrue(TEXT("UObject output inference, documentation, and cast metadata should attach to exact results"),
			CountSourceOccurrences(UObjectFamily.Registration, TEXT(".DeterminesOutputType(")) == 2
				&& UObjectFamily.Registration.Contains(TEXT(".DeterminesOutputType(0)"))
				&& UObjectFamily.Registration.Contains(TEXT(".DeterminesOutputType(1)"))
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT(".Documentation(")) == 5
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT(".NativeUObjectCast(")) == 1);
		bPassed &= TestRunner->TestTrue(TEXT("UObject native pointer metadata should preserve its physical registration multiset"),
			CountSourceOccurrences(UObjectFamily.Registration, TEXT("METHOD_TRIVIAL(")) == 11
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT("METHODPR_TRIVIAL(")) == 3
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT("FUNC_TRIVIAL(")) == 1
				&& CountSourceOccurrences(UObjectFamily.Registration, TEXT("FUNCPR_TRIVIAL(")) == 1);

		bPassed &= TestRunner->TestTrue(TEXT("UStruct should expose direct declarations, infrastructure, and reflection providers in both build branches"),
			CountSourceOccurrences(UStructFamily.Registration, TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_UStruct_")) == 6
				&& CountSourceOccurrences(UStructFamily.Registration, TEXT("EAngelscriptBindPhase::TypeDeclarations")) == 2
				&& CountSourceOccurrences(UStructFamily.Registration, TEXT("EAngelscriptBindPhase::TypeInfrastructure")) == 2
				&& CountSourceOccurrences(UStructFamily.Registration, TEXT("EAngelscriptBindPhase::ReflectionBindings")) == 2);
		bPassed &= TestRunner->TestFalse(TEXT("UStruct should not retain legacy providers or direct callable lambdas"),
			HasLegacyProvider(UStructFamily.Registration)
				|| ContainsInlineDirectCallableLambda(UStructFamily.Registration));
		bPassed &= TestRunner->TestTrue(TEXT("UStruct special-member callables should live in one companion owner"),
			UStructFamily.Header.Contains(TEXT("struct FAngelscriptUStructBinds"))
				&& !UStructFamily.Registration.Contains(TEXT("struct FAngelscriptUStructBinds"))
				&& UStructFamily.Implementation.Contains(TEXT("FAngelscriptUStructBinds::GenericCopyConstruct"))
				&& UStructFamily.Implementation.Contains(TEXT("FAngelscriptUStructBinds::GenericAssign")));
		bPassed &= TestRunner->TestTrue(TEXT("UStruct should mutate only its selected engine and reuse one frozen non-database snapshot"),
			UStructFamily.Registration.Contains(TEXT("ValueClassForTarget"))
				&& UStructFamily.Registration.Contains(TEXT("ExistingClassForTarget"))
				&& UStructFamily.Registration.Contains(TEXT("RegisterTypeForTarget"))
				&& UStructFamily.Registration.Contains(TEXT("RegisterTypeFinderForTarget"))
				&& BindsHeader.Contains(TEXT("UStructTypeSnapshot"))
				&& BindsHeader.Contains(TEXT("bUStructTypeSnapshotCaptured")));
		bPassed &= TestRunner->TestTrue(TEXT("UStruct special members should preserve exact native-form parity"),
			CountSourceOccurrences(UStructFamily.Registration, TEXT(".NativeConstructor(")) == 9
				&& CountSourceOccurrences(UStructFamily.Registration, TEXT(".NativeDestructor(")) == 3
				&& CountSourceOccurrences(UStructFamily.Registration, TEXT(".NativeAssignment(")) == 3);
		TestRunner->TestTrue(TEXT("UObject and UStruct migration should preserve provider, callable, and trait ownership"), bPassed);
	}

	TEST_METHOD(ReflectionDependentTestProvidersUseExplicitPostReflectionCallbacks)
	{
		FString PerformanceProvider;
		FString CoverageProvider;
		FString OptionalProvider;
		ASSERT_THAT(IsTrue(
			LoadPluginSource(
				TEXT("AngelscriptTest/Performance/AngelscriptPerformanceTestTypes.cpp"),
				PerformanceProvider),
			TEXT("Performance post-reflection provider source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadPluginSource(
				TEXT("AngelscriptTest/Coverage/AngelscriptCoverageGCTestHelpers.cpp"),
				CoverageProvider),
			TEXT("Coverage post-reflection provider source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadPluginSource(
				TEXT("AngelscriptTest/Bindings/AngelscriptOptionalBindingsTests.cpp"),
				OptionalProvider),
			TEXT("Optional post-reflection provider source should be readable")));

		const auto IsExplicitPostReflectionProvider = [](const FString& Source)
		{
			return Source.Contains(TEXT("AS_FORCE_LINK const FAngelscriptBind"))
				&& Source.Contains(TEXT("EAngelscriptBindPhase::PostReflectionBindings"))
				&& Source.Contains(TEXT("BindGlobalFunctionForTarget"))
				&& !Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				&& !Source.Contains(TEXT("FAngelscriptBinds::EOrder"))
				&& !Source.Contains(TEXT("PreviousBind"));
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("Performance bindings should execute explicitly after reflection"),
			IsExplicitPostReflectionProvider(PerformanceProvider)
				&& PerformanceProvider.Contains(TEXT("ExistingClassForTarget"))
				&& PerformanceProvider.Contains(TEXT("FNamespace Namespace("))
				&& CountSourceOccurrences(PerformanceProvider, TEXT(".EditorOnly()")) == 32);
		bPassed &= TestRunner->TestTrue(TEXT("Coverage helpers should execute explicitly after reflection"),
			IsExplicitPostReflectionProvider(CoverageProvider)
				&& CoverageProvider.Contains(TEXT("Binds.GetTargetEngine()")));
		bPassed &= TestRunner->TestTrue(TEXT("Optional helpers should execute explicitly after reflection"),
			IsExplicitPostReflectionProvider(OptionalProvider));
		TestRunner->TestTrue(TEXT("Legacy Late+101 test providers should remain explicit post-reflection consumers"), bPassed);
	}

	TEST_METHOD(LowCouplingProvidersUseExplicitDirectCallbacks)
	{
		FString Deprecations;
		FString CoreGlobals;
		FString PlatformMisc;
		FString ConfigEnums;
		FString CollisionProfile;
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_Deprecations.cpp"), Deprecations), TEXT("Deprecation provider source should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_CoreGlobals.cpp"), CoreGlobals), TEXT("Core globals provider source should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_FGenericPlatformMisc.cpp"), PlatformMisc), TEXT("Platform misc provider source should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_ConfigEnums.cpp"), ConfigEnums), TEXT("Config enum provider source should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_CollisionProfile.cpp"), CollisionProfile), TEXT("Collision profile provider source should be readable")));

		const auto HasLegacyProvider = [](const FString& Source)
		{
			return Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Source.Contains(TEXT("FAngelscriptBinds::EOrder"));
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("Deprecation metadata should not retain a legacy provider"), HasLegacyProvider(Deprecations));
		bPassed &= TestRunner->TestFalse(TEXT("Core globals should not retain a legacy provider"), HasLegacyProvider(CoreGlobals));
		bPassed &= TestRunner->TestFalse(TEXT("Platform misc should not retain a legacy provider"), HasLegacyProvider(PlatformMisc));
		bPassed &= TestRunner->TestFalse(TEXT("Config enums should not retain a legacy provider"), HasLegacyProvider(ConfigEnums));
		bPassed &= TestRunner->TestFalse(TEXT("Collision profiles should not retain a legacy provider"), HasLegacyProvider(CollisionProfile));
		bPassed &= TestRunner->TestTrue(TEXT("Deprecation metadata should preserve early execution through TypeDeclarations"),
			Deprecations.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations")));
		bPassed &= TestRunner->TestTrue(TEXT("Core globals should use a direct ManualBindings provider and explicit target registration"),
			CoreGlobals.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
			&& CoreGlobals.Contains(TEXT("Binds.BindGlobalFunctionForTarget")));
		bPassed &= TestRunner->TestTrue(TEXT("Platform misc should use an explicit target namespace and global function registration"),
			PlatformMisc.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
			&& PlatformMisc.Contains(TEXT("FNamespace Namespace(Binds.GetTargetEngine()"))
			&& PlatformMisc.Contains(TEXT("Binds.BindGlobalFunctionForTarget")));
		bPassed &= TestRunner->TestTrue(TEXT("Config enums should register declarations through the explicit target context"),
			ConfigEnums.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
			&& ConfigEnums.Contains(TEXT("Binds.EnumForTarget")));
		bPassed &= TestRunner->TestTrue(TEXT("Collision profiles should use explicit namespace, property, and documentation targets"),
			CollisionProfile.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
			&& CollisionProfile.Contains(TEXT("FNamespace Namespace(Binds.GetTargetEngine()"))
			&& CollisionProfile.Contains(TEXT("Binds.BindGlobalVariableForTarget"))
			&& CollisionProfile.Contains(TEXT("AddDocumentationForGlobalVariable(Binds.GetTargetEngine()")));
		TestRunner->TestTrue(TEXT("Low-coupling providers should demonstrate direct callback forms without ambient engine access"), bPassed);
	}

	TEST_METHOD(EngineSubsystemDoesNotOwnBindingCollectionState)
	{
		FString SubsystemHeader;
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptSubsystem.h"), SubsystemHeader),
			TEXT("Angelscript subsystem header should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("Subsystem should not own direct binding records or collections"),
			SubsystemHeader.Contains(TEXT("FAngelscriptBindRecord"))
				|| SubsystemHeader.Contains(TEXT("FAngelscriptBindCollection"))
				|| SubsystemHeader.Contains(TEXT("TArray<FAngelscriptBind")));
		bPassed &= TestRunner->TestFalse(TEXT("Subsystem should not own a callback array or pointer view"),
			SubsystemHeader.Contains(TEXT("BindCallbacks"))
				|| SubsystemHeader.Contains(TEXT("BindArray"))
				|| SubsystemHeader.Contains(TEXT("BindView")));
		bPassed &= TestRunner->TestFalse(TEXT("Subsystem should not cache expanded binding operations"),
			SubsystemHeader.Contains(TEXT("ExpandedBind"))
				|| SubsystemHeader.Contains(TEXT("BindOperations"))
				|| SubsystemHeader.Contains(TEXT("BindingManifest")));
		bPassed &= TestRunner->TestFalse(TEXT("Subsystem should not own a binding registration handle"),
			SubsystemHeader.Contains(TEXT("BindRegistrationHandle"))
				|| SubsystemHeader.Contains(TEXT("BindingRegistrationHandle")));
		bPassed &= TestRunner->TestTrue(TEXT("Subsystem should retain only primary-engine lifecycle ownership"),
			SubsystemHeader.Contains(TEXT("FAngelscriptEngine OwnedEngine"))
				&& SubsystemHeader.Contains(TEXT("FAngelscriptEngine* PrimaryEngine")));
		TestRunner->TestTrue(TEXT("The process-global bind collection should not leak into subsystem state"), bPassed);
	}

	TEST_METHOD(MemberPointerOnlyProvidersUseExplicitTargetClasses)
	{
		static const TCHAR* ProviderFiles[] = {
			TEXT("Bind_FBodyInstance.cpp"),
			TEXT("Bind_FInputActionKeyMapping.cpp"),
			TEXT("Bind_UFXSystemComponent.cpp"),
			TEXT("Bind_UInputSettings.cpp"),
			TEXT("Bind_ULocalPlayer.cpp"),
			TEXT("Bind_UPackage.cpp"),
			TEXT("Bind_USkeletalMeshComponent.cpp"),
			TEXT("Bind_USkinnedMeshComponent.cpp"),
		};

		bool bPassed = true;
		for (const TCHAR* ProviderFile : ProviderFiles)
		{
			FString Source;
			if (!TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should be readable"), ProviderFile),
				LoadBindRegistration(ProviderFile, Source)))
			{
				bPassed = false;
				continue;
			}

			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a legacy provider"), ProviderFile),
				Source.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
					|| Source.Contains(TEXT("FAngelscriptBinds::EOrder")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should register in ManualBindings"), ProviderFile),
				Source.Contains(TEXT("EAngelscriptBindPhase::ManualBindings")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should resolve its class through the explicit context"), ProviderFile),
				Source.Contains(TEXT("Binds.ExistingClassForTarget")));
		}

		TestRunner->TestTrue(TEXT("Member-pointer-only providers should be explicit direct callbacks"), bPassed);
	}

	TEST_METHOD(EnumProviderUsesExplicitPhaseSplitAndTargetState)
	{
		FString Source;
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_UEnum.cpp"), Source), TEXT("UEnum provider source should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("UEnum should not retain a legacy provider"),
			Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Source.Contains(TEXT("FAngelscriptBinds::EOrder")));
		bPassed &= TestRunner->TestTrue(TEXT("UEnum should split declarations from its manual member-pointer surface"),
			Source.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
				&& Source.Contains(TEXT("EAngelscriptBindPhase::ManualBindings")));
		bPassed &= TestRunner->TestTrue(TEXT("UEnum should route enum declarations and member bindings through the explicit context"),
			Source.Contains(TEXT("Binds.EnumForTarget"))
				&& Source.Contains(TEXT("Binds.ExistingClassForTarget")));
		bPassed &= TestRunner->TestTrue(TEXT("UEnum should route its mutable support state through the selected engine"),
			Source.Contains(TEXT("Binds.GetTargetBindDatabase"))
				&& Source.Contains(TEXT("Binds.RegisterTypeForTarget"))
				&& Source.Contains(TEXT("Binds.RegisterTypeFinderForTarget"))
				&& Source.Contains(TEXT("Binds.GetTargetEngine().GetScriptEnumTypeLookup"))
				&& Source.Contains(TEXT("AddUnrealDocumentationForType(Binds.GetTargetEngine()")));
		TestRunner->TestTrue(TEXT("UEnum should use direct callbacks without creating an empty callable companion"), bPassed);
	}

	TEST_METHOD(OptionalTemplateProviderUsesExplicitPhasesAndHeaderOwner)
	{
		FString Registration;
		FString Header;
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_TOptional.cpp"), Registration), TEXT("TOptional provider source should be readable")));
		ASSERT_THAT(IsTrue(LoadBindRegistration(TEXT("Bind_TOptional.h"), Header), TEXT("TOptional support header should be readable")));

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("TOptional should not retain a legacy provider"),
			Registration.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Registration.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Registration.Contains(TEXT("FAngelscriptBinds::EOrder")));
		bPassed &= TestRunner->TestTrue(TEXT("TOptional should split declaration, early method surface, and infrastructure registration"),
			Registration.Contains(TEXT("EAngelscriptBindPhase::TypeDeclarations"))
				&& Registration.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
				&& Registration.Contains(TEXT("Bind_TOptional_MethodSurface"))
				&& Registration.Contains(TEXT("TOptional.MethodSurface"))
				&& !Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings")));
		bPassed &= TestRunner->TestTrue(TEXT("TOptional should use explicit target registration and type lookup"),
			Registration.Contains(TEXT("Binds.ValueClassForTarget"))
				&& Registration.Contains(TEXT("Binds.ExistingClassForTarget"))
				&& Registration.Contains(TEXT("Binds.RegisterTypeForTarget"))
				&& Registration.Contains(TEXT("Binds.RegisterTypeFinderForTarget"))
				&& Registration.Contains(TEXT("FromProperty(*TargetTypeDatabase")));
		bPassed &= TestRunner->TestTrue(TEXT("TOptional should replace the inline template callback and ambient traits with named/fluent forms"),
			Registration.Contains(TEXT("&FAngelscriptOptionalBinds::ValidateTemplate"))
				&& Registration.Contains(TEXT(".PassScriptObjectTypeAsFirstParam()"))
				&& CountSourceOccurrences(Registration, TEXT(".NativeTemplateInstantiatedCall(")) == 13
				&& Registration.Contains(TEXT(".Documentation(TEXT("))
				&& !Registration.Contains(TEXT("PreviousBindPassScriptObjectTypeAsFirstParam"))
				&& !Registration.Contains(TEXT("SCRIPT_NATIVE_TEMPLATED_CALL"))
				&& !Registration.Contains(TEXT("SCRIPT_BIND_DOCUMENTATION")));
		bPassed &= TestRunner->TestTrue(TEXT("TOptional should retain its existing header-visible template owner"),
			Header.Contains(TEXT("struct ANGELSCRIPTRUNTIME_API FAngelscriptOptionalBinds"))
				&& Header.Contains(TEXT("Construct_Template"))
				&& Header.Contains(TEXT("GetValue_Template")));
		TestRunner->TestTrue(TEXT("TOptional should keep template visibility while using direct explicit callbacks"), bPassed);
	}

	TEST_METHOD(SmallCallableProvidersUseNamedOwnersAndDirectCallbacks)
	{
		struct FProviderExpectation
		{
			const TCHAR* TypeName;
			const TCHAR* OwnerName;
			const TCHAR* PhaseName = TEXT("ManualBindings");
		};

		static const FProviderExpectation Providers[] = {
			{TEXT("FPlatformApplicationMisc"), TEXT("FAngelscriptFPlatformApplicationMiscBinds")},
			{TEXT("FCpuProfilerTraceScoped"), TEXT("FAngelscriptFCpuProfilerTraceScopedBinds")},
			{TEXT("FApp"), TEXT("FAngelscriptFAppBinds")},
			{TEXT("UPoseableMeshComponent"), TEXT("FAngelscriptUPoseableMeshComponentBinds")},
			{TEXT("FAngelscriptGameThreadScopeWorldContext"), TEXT("FAngelscriptFAngelscriptGameThreadScopeWorldContextBinds")},
			{TEXT("LandscapeProxy"), TEXT("FAngelscriptLandscapeProxyBinds")},
			{TEXT("FCommandLine"), TEXT("FAngelscriptFCommandLineBinds")},
			{TEXT("FPlatformMisc"), TEXT("FAngelscriptFPlatformMiscBinds")},
			{TEXT("UCollisionProfile"), TEXT("FAngelscriptUCollisionProfileBinds")},
			{TEXT("FMessageDialog"), TEXT("FAngelscriptFMessageDialogBinds")},
			{TEXT("FLatentActionInfo"), TEXT("FAngelscriptFLatentActionInfoBinds")},
			{TEXT("UProjectileMovementComponent"), TEXT("FAngelscriptUProjectileMovementComponentBinds")},
			{TEXT("AVolume"), TEXT("FAngelscriptAVolumeBinds")},
			{TEXT("FParse"), TEXT("FAngelscriptFParseBinds")},
			{TEXT("FGeometry"), TEXT("FAngelscriptFGeometryBinds")},
			{TEXT("FOverlapResult"), TEXT("FAngelscriptFOverlapResultBinds")},
			{TEXT("UGameInstance"), TEXT("FAngelscriptUGameInstanceBinds")},
			{TEXT("FPlatformProcess"), TEXT("FAngelscriptFPlatformProcessBinds")},
			{TEXT("FPlane4f"), TEXT("FAngelscriptFPlane4fBinds")},
			{TEXT("FAnchors"), TEXT("FAngelscriptFAnchorsBinds")},
			{TEXT("UEnhancedInputComponent"), TEXT("FAngelscriptUEnhancedInputComponentBinds")},
			{TEXT("Hash"), TEXT("FAngelscriptHashBinds")},
			{TEXT("SystemTimers"), TEXT("FAngelscriptSystemTimersBinds")},
			{TEXT("FFileHelper"), TEXT("FAngelscriptFFileHelperBinds")},
			{TEXT("UPrimitiveComponent"), TEXT("FAngelscriptUPrimitiveComponentBinds")},
			{TEXT("FNumberFormattingOptions"), TEXT("FAngelscriptFNumberFormattingOptionsBinds")},
			{TEXT("FStringTableRegistry"), TEXT("FAngelscriptFStringTableRegistryBinds")},
			{TEXT("FInputActionValue"), TEXT("FAngelscriptFInputActionValueBinds")},
			{TEXT("FPlane"), TEXT("FAngelscriptFPlaneBinds")},
			{TEXT("FMargin"), TEXT("FAngelscriptFMarginBinds")},
			{TEXT("FPaths"), TEXT("FAngelscriptFPathsBinds")},
			{TEXT("FGuid"), TEXT("FAngelscriptFGuidBinds")},
			{TEXT("FInputBindingHandle"), TEXT("FAngelscriptFInputBindingHandleBinds")},
			{TEXT("FRandomStream"), TEXT("FAngelscriptFRandomStreamBinds")},
			{TEXT("SoftObjectPath"), TEXT("FAngelscriptSoftObjectPathBinds")},
			{TEXT("FCollisionShape"), TEXT("FAngelscriptFCollisionShapeBinds")},
			{TEXT("FIntPoint"), TEXT("FAngelscriptFIntPointBinds")},
			{TEXT("FDateTime"), TEXT("FAngelscriptFDateTimeBinds")},
			{TEXT("FTimespan"), TEXT("FAngelscriptFTimespanBinds")},
			{TEXT("FIntVector"), TEXT("FAngelscriptFIntVectorBinds")},
			{TEXT("FIntVector2"), TEXT("FAngelscriptFIntVector2Binds")},
			{TEXT("FIntVector4"), TEXT("FAngelscriptFIntVector4Binds")},
			{TEXT("FSphere"), TEXT("FAngelscriptFSphereBinds")},
			{TEXT("FSphere3f"), TEXT("FAngelscriptFSphere3fBinds")},
			{TEXT("FBox"), TEXT("FAngelscriptFBoxBinds")},
			{TEXT("FBox3f"), TEXT("FAngelscriptFBox3fBinds")},
			{TEXT("FBoxSphereBounds"), TEXT("FAngelscriptFBoxSphereBoundsBinds")},
			{TEXT("FBoxSphereBounds3f"), TEXT("FAngelscriptFBoxSphereBounds3fBinds")},
			{TEXT("Stats"), TEXT("FAngelscriptStatsBinds")},
			{TEXT("FHitResult"), TEXT("FAngelscriptFHitResultBinds")},
			{TEXT("FVector4"), TEXT("FAngelscriptFVector4Binds")},
			{TEXT("FVector4f"), TEXT("FAngelscriptFVector4fBinds")},
			{TEXT("FVector2D"), TEXT("FAngelscriptFVector2DBinds")},
			{TEXT("FVector2f"), TEXT("FAngelscriptFVector2fBinds")},
			{TEXT("UAssetManager"), TEXT("FAngelscriptUAssetManagerBinds")},
			{TEXT("JsonObjectConverter"), TEXT("FAngelscriptJsonObjectConverterBinds")},
			{TEXT("FInstancedStruct"), TEXT("FAngelscriptFInstancedStructBinds")},
			{TEXT("UWorld"), TEXT("FAngelscriptUWorldBinds")},
			{TEXT("FMemoryReader"), TEXT("FAngelscriptFMemoryReaderBinds")},
			{TEXT("UInputMappingContext"), TEXT("FAngelscriptUInputMappingContextBinds")},
			{TEXT("Console"), TEXT("FAngelscriptConsoleBinds")},
			{TEXT("Logging"), TEXT("FAngelscriptLoggingBinds")},
			{TEXT("FQuat"), TEXT("FAngelscriptFQuatBinds")},
			{TEXT("FQuat4f"), TEXT("FAngelscriptFQuat4fBinds")},
			{TEXT("FRotator"), TEXT("FAngelscriptFRotatorBinds")},
			{TEXT("FRotator3f"), TEXT("FAngelscriptFRotator3fBinds")},
			{TEXT("FunctionLibraryMixins"), TEXT("FAngelscriptFunctionLibraryMixinsBinds"), TEXT("PostReflectionBindings")},
			{TEXT("Subsystems"), TEXT("FAngelscriptSubsystemsBinds"), TEXT("PostReflectionBindings")},
			{TEXT("APlayerController"), TEXT("FAngelscriptAPlayerControllerBinds")},
			{TEXT("USceneComponent"), TEXT("FAngelscriptUSceneComponentBinds")},
			{TEXT("FLinearColor"), TEXT("FAngelscriptFLinearColorBinds")},
			{TEXT("UDataTable"), TEXT("FAngelscriptUDataTableBinds")},
			{TEXT("FName"), TEXT("FAngelscriptFNameBinds")},
			{TEXT("FTransform"), TEXT("FAngelscriptFTransformBinds")},
			{TEXT("FTransform3f"), TEXT("FAngelscriptFTransform3fBinds")},
			{TEXT("FFormatArgumentValue"), TEXT("FAngelscriptFFormatArgumentValueBinds")},
			{TEXT("FText"), TEXT("FAngelscriptFTextBinds")},
			{TEXT("AActor"), TEXT("FAngelscriptActorBinds"), TEXT("PostReflectionBindings")},
			{TEXT("UActorComponent"), TEXT("FAngelscriptUActorComponentBinds"), TEXT("PostReflectionBindings")},
			{TEXT("Debugging"), TEXT("FAngelscriptDebuggingBinds")},
			{TEXT("FAngelscriptDelegateWithPayload"), TEXT("FAngelscriptDelegateWithPayloadBinds")},
			{TEXT("AssetRegistry"), TEXT("FAngelscriptAssetRegistryBinds")},
			{TEXT("UUserWidget"), TEXT("FAngelscriptUUserWidgetBinds")},
			{TEXT("FVector3f"), TEXT("FAngelscriptFVector3fBinds")},
		};

		bool bPassed = true;
		for (const FProviderExpectation& Provider : Providers)
		{
			FReferenceBindFamilySource Source;
			if (!TestRunner->TestTrue(
				*FString::Printf(TEXT("%s callable-family sources should be readable"), Provider.TypeName),
				LoadReferenceBindFamily(Provider.TypeName, Source)))
			{
				bPassed = false;
				continue;
			}

			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a legacy provider"), Provider.TypeName),
				Source.Registration.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Source.Registration.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
					|| Source.Registration.Contains(TEXT("FAngelscriptBinds::EOrder")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should register in %s"), Provider.TypeName, Provider.PhaseName),
				Source.Registration.Contains(FString::Printf(TEXT("EAngelscriptBindPhase::%s"), Provider.PhaseName)));
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not embed callable lambdas in registration calls"), Provider.TypeName),
				ContainsInlineDirectCallableLambda(Source.Registration));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should declare one named callable owner"), Provider.TypeName),
				Source.Header.Contains(TEXT("struct"))
					&& Source.Header.Contains(Provider.OwnerName));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should route registrations through its named owner"), Provider.TypeName),
				Source.Registration.Contains(FString::Printf(TEXT("&%s::"), Provider.OwnerName)));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s named callable definitions should live in the companion cpp"), Provider.TypeName),
				Source.Implementation.Contains(FString::Printf(TEXT("%s::"), Provider.OwnerName)));
		}

		TestRunner->TestTrue(TEXT("Small callable providers should have direct phase and named-function ownership"), bPassed);
	}

	TEST_METHOD(GeneratedOverrideProvidersUseExplicitTargetFunctionTables)
	{
		static const TCHAR* ProviderFiles[] = {
			TEXT("Bind_AssetManagerScriptMixins.cpp"),
			TEXT("Bind_InputComponentScriptMixins.cpp"),
		};

		bool bPassed = true;
		for (const TCHAR* ProviderFile : ProviderFiles)
		{
			FString Source;
			if (!TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should be readable"), ProviderFile),
				LoadBindRegistration(ProviderFile, Source)))
			{
				bPassed = false;
				continue;
			}

			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a legacy provider"), ProviderFile),
				Source.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
					|| Source.Contains(TEXT("FAngelscriptBinds::EOrder")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should run before generated bindings in ManualBindings"), ProviderFile),
				Source.Contains(TEXT("EAngelscriptBindPhase::ManualBindings")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should mutate only its explicit engine function table"), ProviderFile),
				Source.Contains(TEXT("Binds.RegisterFunctionBindingForTarget"))
					&& !Source.Contains(TEXT("FAngelscriptBinds::RegisterFunctionBinding(")));
		}

		TestRunner->TestTrue(TEXT("Generated override providers should use explicit engine-owned function tables"), bPassed);
	}

	TEST_METHOD(NativeModuleTransportUsesDirectGeneratedPhase)
	{
		FString Source;
		FString EngineSource;
		ASSERT_THAT(IsTrue(
			LoadBindRegistration(TEXT("Bind_NativeModuleFunctionBinding.cpp"), Source),
			TEXT("Native-module function transport source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptEngine.cpp"), EngineSource),
			TEXT("AngelScript engine lifecycle source should be readable")));

		ASSERT_THAT(IsFalse(
			Source.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| Source.Contains(TEXT("FAngelscriptBinds::EOrder")),
			TEXT("Native-module function transport should not retain a legacy provider")));
		ASSERT_THAT(IsTrue(
			Source.Contains(TEXT("EAngelscriptBindPhase::GeneratedBindings"))
				&& Source.Contains(TEXT("&BindNativeModuleFunctionBindings"))
				&& Source.Contains(TEXT("BindNativeModuleFunctionBindings(FAngelscriptBinds& Binds)"))
				&& Source.Contains(TEXT("RegisterExistingNativeModuleFunctionBindings(Binds)")),
			TEXT("Native-module function transport should subscribe through a direct GeneratedBindings callback")));
		ASSERT_THAT(IsTrue(
			Source.Contains(TEXT("Binds.GetTargetBindState()"))
				&& Source.Contains(TEXT("Binds.RegisterFunctionBindingForTarget")),
			TEXT("Native-module function transport should inspect and mutate only explicit engine-owned function tables")));
		ASSERT_THAT(IsFalse(
			Source.Contains(TEXT("FAngelscriptBinds::GetClassFunctionBindings()"))
				|| Source.Contains(TEXT("FAngelscriptBinds::RegisterFunctionBinding(")),
			TEXT("Native-module function transport should not route injection, duplicate checks, or removal through ambient bind state")));
		ASSERT_THAT(IsTrue(
			Source.Contains(TEXT("GAngelscriptNativeModuleFunctionBindingUnregisterEngine"))
				&& EngineSource.Contains(TEXT("GAngelscriptNativeModuleFunctionBindingUnregisterEngine(*this)")),
			TEXT("AngelScript engine Shutdown should explicitly unregister native-module target records before bind-state teardown")));
		ASSERT_THAT(IsTrue(
			Source.Contains(TEXT("TAtomic<bool> bInjectionDone"))
				&& Source.Contains(TEXT("while (!bInjectionDone.Load())"))
				&& Source.Contains(TEXT("InjectNativeModuleFunctionBindingStateIntoTarget(State, Target)")),
			TEXT("GeneratedBindings should finish current-target native-module injection before ReflectionBindings can start")));
		ASSERT_THAT(IsTrue(
			EngineSource.Contains(TEXT("TAtomic<bool> bInitializationDone"))
				&& EngineSource.Contains(TEXT("TAtomic<bool> bInitializationSucceeded"))
				&& EngineSource.Contains(TEXT("bInitializationSucceeded.Store("))
				&& EngineSource.Contains(TEXT("bInitializationDone.Store(true)"))
				&& EngineSource.Contains(TEXT("while (!bInitializationDone.Load())"))
				&& EngineSource.Contains(TEXT("return bInitializationSucceeded.Load();"))
				&& EngineSource.Contains(TEXT("if (!bInitializationSucceeded)"))
				&& !EngineSource.Contains(TEXT("volatile bool bInitializationDone")),
			TEXT("Threaded engine initialization should publish completion and result through atomic synchronization")));
	}

	TEST_METHOD(ActorAndComponentProvidersSeparateManualSurfaceFromReflectedSynthesis)
	{
		FString ActorSource;
		FString ComponentSource;
		FString TimerSource;
		ASSERT_THAT(IsTrue(
			LoadBindRegistration(TEXT("Bind_AActor.cpp"), ActorSource),
			TEXT("AActor registration source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindRegistration(TEXT("Bind_UActorComponent.cpp"), ComponentSource),
			TEXT("UActorComponent registration source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindRegistration(TEXT("Bind_SystemTimers.cpp"), TimerSource),
			TEXT("SystemTimers registration source should be readable")));

		auto ExtractSection = [](const FString& Source, const TCHAR* StartToken, const TCHAR* EndToken)
		{
			const int32 StartIndex = Source.Find(StartToken, ESearchCase::CaseSensitive);
			const int32 EndIndex = Source.Find(EndToken, ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIndex + 1);
			if (StartIndex == INDEX_NONE || EndIndex == INDEX_NONE || EndIndex <= StartIndex)
			{
				return FString();
			}
			return Source.Mid(StartIndex, EndIndex - StartIndex);
		};

		const FString ActorManual = ExtractSection(
			ActorSource,
			TEXT("void BindAActor("),
			TEXT("void BindActorPostReflectionAccessors("));
		const FString ActorPostReflection = ExtractSection(
			ActorSource,
			TEXT("void BindActorPostReflectionAccessors("),
			TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_AActor"));
		ASSERT_THAT(IsTrue(
			ActorManual.Contains(TEXT("void GetAllActorsOfClass(?& OutActors)"))
				&& ActorManual.Contains(TEXT("AActor SpawnActor(const TSubclassOf<AActor>& Class"))
				&& ActorManual.Contains(TEXT("AActor SpawnPersistentActor(const TSubclassOf<AActor>& Class")),
			TEXT("Ordinary actor queries and spawn helpers should register in ManualBindings")));
		ASSERT_THAT(IsTrue(
			ActorPostReflection.Contains(TEXT("TObjectRange<UClass>"))
				&& ActorPostReflection.Contains(TEXT("FAngelscriptActorBinds::SpawnActorFromMeta"))
				&& !ActorPostReflection.Contains(TEXT("void GetAllActorsOfClass(?& OutActors)"))
				&& !ActorPostReflection.Contains(TEXT("AActor SpawnActor(const TSubclassOf<AActor>& Class")),
			TEXT("AActor PostReflectionBindings should contain only reflected typed-accessor synthesis")));

		const FString ComponentManual = ExtractSection(
			ComponentSource,
			TEXT("void BindUActorComponent("),
			TEXT("void BindComponentPostReflectionAccessors("));
		const FString ComponentPostReflection = ExtractSection(
			ComponentSource,
			TEXT("void BindComponentPostReflectionAccessors("),
			TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_UActorComponent"));
		ASSERT_THAT(IsTrue(
			ComponentManual.Contains(TEXT("UActorComponent CreateComponent(const TSubclassOf<UActorComponent>& ComponentClass"))
				&& ComponentManual.Contains(TEXT("void __Actor_GetComponentByClass"))
				&& ComponentManual.Contains(TEXT("void __Actor_CreateComponentByClass")),
			TEXT("Ordinary component methods and generic helpers should register in ManualBindings")));
		ASSERT_THAT(IsTrue(
			ComponentPostReflection.Contains(TEXT("TObjectRange<UClass>"))
				&& ComponentPostReflection.Contains(TEXT("FAngelscriptActorBinds::CreateComponentFromMeta"))
				&& !ComponentPostReflection.Contains(TEXT("UActorComponent CreateComponent(const TSubclassOf<UActorComponent>& ComponentClass"))
				&& !ComponentPostReflection.Contains(TEXT("void __Actor_GetComponentByClass")),
			TEXT("UActorComponent PostReflectionBindings should contain only reflected typed-accessor synthesis")));

		ASSERT_THAT(AreEqual(
			2,
			CountSourceOccurrences(ActorSource, TEXT(".DeterminesOutputType(0)")),
			TEXT("AActor should preserve both exact output-type traits")));
		ASSERT_THAT(AreEqual(
			1,
			CountSourceOccurrences(ActorSource, TEXT(".PassScriptFunctionAsFirstParam()")),
			TEXT("AActor should preserve its reflected spawn script-function trait")));
		ASSERT_THAT(AreEqual(
			3,
			CountSourceOccurrences(ComponentSource, TEXT(".DeterminesOutputType(0)")),
			TEXT("UActorComponent should preserve all three exact output-type traits")));
		ASSERT_THAT(AreEqual(
			4,
			CountSourceOccurrences(ComponentSource, TEXT(".PassScriptFunctionAsFirstParam()")),
			TEXT("UActorComponent should preserve all four reflected script-function traits")));
		ASSERT_THAT(AreEqual(
			4,
			CountSourceOccurrences(TimerSource, TEXT(".WorldContext()")),
			TEXT("SystemTimers should preserve all four exact WorldContext traits")));
	}

	TEST_METHOD(ReflectionSynthesisProvidersUseExplicitTypeDatabaseLookups)
	{
		struct FProviderSource
		{
			const TCHAR* FileName;
			const TCHAR* CallbackName;
		};
		const FProviderSource Providers[] = {
			{TEXT("Bind_AActor.cpp"), TEXT("BindActorPostReflectionAccessors")},
			{TEXT("Bind_UActorComponent.cpp"), TEXT("BindComponentPostReflectionAccessors")},
			{TEXT("Bind_Subsystems.cpp"), TEXT("BindSubsystems")}};

		for (const FProviderSource& Provider : Providers)
		{
			FString Source;
			ASSERT_THAT(IsTrue(
				LoadBindRegistration(Provider.FileName, Source),
				FString::Printf(TEXT("%s registration source should be readable"), Provider.FileName)));
			ASSERT_THAT(IsFalse(
				Source.Contains(TEXT("FAngelscriptType::GetByClass(Class)")),
				FString::Printf(
					TEXT("%s callback %s should not resolve a registration target through the ambient engine"),
					Provider.FileName,
					Provider.CallbackName)));
			ASSERT_THAT(IsTrue(
				Source.Contains(TEXT("FAngelscriptType::GetByClass("))
					&& Source.Contains(TEXT("Binds.GetTargetTypeDatabase(),")),
				FString::Printf(
					TEXT("%s callback %s should resolve classes through its explicit target database"),
					Provider.FileName,
					Provider.CallbackName)));
		}
	}

	TEST_METHOD(EngineBindDatabaseLifecycleUsesOwnedInstanceWithoutAmbientResolution)
	{
		FString EngineSource;
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptEngine.cpp"), EngineSource),
			TEXT("AngelscriptEngine implementation should be readable")));
		ASSERT_THAT(IsFalse(
			EngineSource.Contains(TEXT("FAngelscriptBindDatabase::Get().Load("))
				|| EngineSource.Contains(TEXT("FAngelscriptBindDatabase::Get().Save("))
				|| EngineSource.Contains(TEXT("FAngelscriptBindDatabase::Get().Clear(")),
			TEXT("Engine BindDB load/save/clear lifecycle should not resolve through an ambient current engine")));
		ASSERT_THAT(IsTrue(
			EngineSource.Contains(TEXT("BindDatabase->Load("))
				&& EngineSource.Contains(TEXT("BindDatabase->Save("))
				&& EngineSource.Contains(TEXT("BindDatabase->Clear(")),
			TEXT("Engine BindDB load/save/clear lifecycle should mutate the engine-owned database directly")));
	}

	TEST_METHOD(EngineScopedBindingStoresDoNotFallbackToProcessStatics)
	{
		FString BindsSource;
		FString BindDatabaseSource;
		FString TypeSource;
		FString NativeFormHeader;
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptBinds.cpp"), BindsSource)
				&& LoadRuntimeSource(TEXT("Core/AngelscriptBindDatabase.cpp"), BindDatabaseSource)
				&& LoadRuntimeSource(TEXT("Core/AngelscriptType.cpp"), TypeSource)
				&& LoadRuntimeSource(TEXT("StaticJIT/StaticJITBinds.h"), NativeFormHeader),
			TEXT("Engine-scoped binding store implementations should be readable")));

		ASSERT_THAT(IsFalse(
			BindsSource.Contains(TEXT("static FAngelscriptBindState LegacyBindState"))
				|| BindDatabaseSource.Contains(TEXT("static FAngelscriptBindDatabase LegacyBindDatabase"))
				|| TypeSource.Contains(TEXT("static FAngelscriptTypeDatabase LegacyDatabase")),
			TEXT("Binding, BindDB, and type mutations should never fall back to unpartitioned process-static stores")));
		ASSERT_THAT(IsFalse(
			NativeFormHeader.Contains(TEXT("ReleaseAllNativeForms")),
			TEXT("StaticJIT native-form cleanup should be owned by engine-state destruction, not ambient engine selection")));
	}

	TEST_METHOD(SubsystemGatesPrimaryEngineInitializationAndPublicationOnBindPreparation)
	{
		FString SubsystemSource;
		ASSERT_THAT(IsTrue(
			LoadRuntimeSource(TEXT("Core/AngelscriptSubsystem.cpp"), SubsystemSource),
			TEXT("Angelscript subsystem implementation should be readable")));

		const int32 PreparationGate = SubsystemSource.Find(
			TEXT("if (!FAngelscriptBind::PrepareForEngineInitialization(BindPreparationDiagnostic))"));
		const int32 ExistingEngineInitialization = SubsystemSource.Find(
			TEXT("if (!CurrentEngine->Initialize())"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			PreparationGate);
		const int32 OwnedEngineInitialization = SubsystemSource.Find(
			TEXT("if (!OwnedEngine.Initialize())"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			PreparationGate);
		const int32 ExistingEnginePublication = SubsystemSource.Find(
			TEXT("bInitializedPrimaryEngine = true"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			ExistingEngineInitialization);
		const int32 OwnedEnginePublication = SubsystemSource.Find(
			TEXT("bInitializedPrimaryEngine = true"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			OwnedEngineInitialization);

		ASSERT_THAT(IsTrue(
			PreparationGate != INDEX_NONE
				&& ExistingEngineInitialization > PreparationGate
				&& OwnedEngineInitialization > PreparationGate
				&& ExistingEnginePublication > ExistingEngineInitialization
				&& OwnedEnginePublication > OwnedEngineInitialization,
			TEXT("Bind preparation must gate both engine initialization paths and primary-engine publication")));
	}

	TEST_METHOD(OutOfDirectoryProvidersUseDirectExplicitArchitecture)
	{
		struct FCallableProviderExpectation
		{
			const TCHAR* RegistrationPath;
			const TCHAR* HeaderPath;
			const TCHAR* ImplementationPath;
			const TCHAR* CallbackName;
			const TCHAR* OwnerName;
			const TCHAR* ExplicitRegistrationNeedle;
			int32 ExpectedGlobalRegistrationCount;
			int32 ExpectedMethodRegistrationCount;
			int32 ExpectedOwnerReferenceCount;
		};

		const FCallableProviderExpectation CallableProviders[] = {
			{
				TEXT("AngelscriptRuntime/Testing/AngelscriptTest.cpp"),
				TEXT("AngelscriptRuntime/Testing/AngelscriptTest_Functions.h"),
				TEXT("AngelscriptRuntime/Testing/AngelscriptTest_Functions.cpp"),
				TEXT("BindAngelscriptTest"),
				TEXT("FAngelscriptTestBinds"),
				TEXT("Binds.BindGlobalFunctionForTarget"),
				14,
				9,
				1},
			{
				TEXT("AngelscriptRuntime/Testing/AngelscriptTestSuite.cpp"),
				TEXT("AngelscriptRuntime/Testing/AngelscriptTestSuite_Functions.h"),
				TEXT("AngelscriptRuntime/Testing/AngelscriptTestSuite_Functions.cpp"),
				TEXT("BindAngelscriptScriptTestSuite"),
				TEXT("FAngelscriptScriptTestSuiteBinds"),
				TEXT("Binds.ExistingClassForTarget"),
				0,
				22,
				22},
			{
				TEXT("AngelscriptEditor/EditorMenuExtensions/ScriptEditorPrompts.cpp"),
				TEXT("AngelscriptEditor/EditorMenuExtensions/ScriptEditorPrompts_Functions.h"),
				TEXT("AngelscriptEditor/EditorMenuExtensions/ScriptEditorPrompts_Functions.cpp"),
				TEXT("BindScriptEditorPrompts"),
				TEXT("FAngelscriptScriptEditorPromptsBinds"),
				TEXT("Binds.BindGlobalFunctionForTarget"),
				6,
				0,
				6},
		};

		bool bPassed = true;
		for (const FCallableProviderExpectation& Provider : CallableProviders)
		{
			FString Registration;
			FString Header;
			FString Implementation;
			if (!TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should be readable"), Provider.RegistrationPath),
				LoadPluginSource(Provider.RegistrationPath, Registration))
				|| !TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should be readable"), Provider.HeaderPath),
					LoadPluginSource(Provider.HeaderPath, Header))
				|| !TestRunner->TestTrue(
					*FString::Printf(TEXT("%s should be readable"), Provider.ImplementationPath),
					LoadPluginSource(Provider.ImplementationPath, Implementation)))
			{
				bPassed = false;
				continue;
			}

			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not retain a legacy provider"), Provider.RegistrationPath),
				Registration.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Registration.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
					|| Registration.Contains(TEXT("FAngelscriptBinds::EOrder")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should use ManualBindings"), Provider.RegistrationPath),
				Registration.Contains(TEXT("EAngelscriptBindPhase::ManualBindings")));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should use a named explicit callback"), Provider.RegistrationPath),
				Registration.Contains(FString::Printf(TEXT("&%s"), Provider.CallbackName))
					&& Registration.Contains(Provider.ExplicitRegistrationNeedle));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should retain its exact registration and owner-reference counts"), Provider.RegistrationPath),
				CountSourceOccurrences(Registration, TEXT("Binds.BindGlobalFunctionForTarget("))
						== Provider.ExpectedGlobalRegistrationCount
					&& CountSourceOccurrences(Registration, TEXT(".Method("))
						== Provider.ExpectedMethodRegistrationCount
					&& CountSourceOccurrences(
						Registration,
						FString::Printf(TEXT("&%s::"), Provider.OwnerName))
						== Provider.ExpectedOwnerReferenceCount
					&& CountSourceOccurrences(
						Registration,
						TEXT("AS_FORCE_LINK const FAngelscriptBind")) == 1);
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(TEXT("%s should not pass an inline lambda to a hand-written AS callable registration"), Provider.RegistrationPath),
				ContainsInlineDirectCallableLambda(Registration));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should declare one named callable owner"), Provider.HeaderPath),
				Header.Contains(Provider.OwnerName));
			bPassed &= TestRunner->TestTrue(
				*FString::Printf(TEXT("%s should define its named callable owner"), Provider.ImplementationPath),
				Implementation.Contains(FString::Printf(TEXT("%s::"), Provider.OwnerName)));
		}

		FString SkipSource;
		ASSERT_THAT(IsTrue(
			LoadPluginSource(TEXT("AngelscriptRuntime/Core/AngelscriptSkipBinds.cpp"), SkipSource),
			TEXT("AngelscriptSkipBinds source should be readable")));
		bPassed &= TestRunner->TestFalse(
			TEXT("AngelscriptSkipBinds should not retain a legacy provider"),
			SkipSource.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| SkipSource.Contains(TEXT("FAngelscriptBinds::RegisterBinds"))
				|| SkipSource.Contains(TEXT("FAngelscriptBinds::EOrder")));
		bPassed &= TestRunner->TestTrue(
			TEXT("AngelscriptSkipBinds should contribute metadata in ManualBindings through the explicit target state"),
			SkipSource.Contains(TEXT("EAngelscriptBindPhase::ManualBindings"))
				&& SkipSource.Contains(TEXT("Binds.GetTargetBindState()"))
				&& SkipSource.Contains(TEXT("&BindDefaultSkipConfiguration"))
				&& CountSourceOccurrences(SkipSource, TEXT("SkipBindNames.Add(")) == 5
				&& CountSourceOccurrences(SkipSource, TEXT("SkipBindClasses.Add(")) == 4
				&& CountSourceOccurrences(
					SkipSource,
					TEXT("AS_FORCE_LINK const FAngelscriptBind")) == 1);
		bPassed &= TestRunner->TestFalse(
			TEXT("Metadata-only SkipBinds should not grow an empty callable companion"),
			IFileManager::Get().FileExists(*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime/Core/AngelscriptSkipBinds_Functions.h"))));

		TestRunner->TestTrue(TEXT("Out-of-Binds providers should use direct phases, explicit targets, and named callable ownership"), bPassed);
	}

	TEST_METHOD(RuntimeBindTraitChainsFollowTheExactOneHundredTwentyColumnLayout)
	{
		const FString BindsDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript/Source/AngelscriptRuntime/Binds"));
		TArray<FString> BindFiles;
		IFileManager::Get().FindFiles(BindFiles, *FPaths::Combine(BindsDirectory, TEXT("Bind_*.cpp")), true, false);
		BindFiles.Sort();

		TArray<FString> Violations;
		for (const FString& BindFile : BindFiles)
		{
			FString Source;
			ASSERT_THAT(IsTrue(
				FFileHelper::LoadFileToString(Source, *FPaths::Combine(BindsDirectory, BindFile)),
				FString::Printf(TEXT("Binding source should be readable: %s"), *BindFile)));
			FindBindTraitChainLayoutViolations(BindFile, Source, Violations);
		}

		ASSERT_THAT(IsTrue(
			Violations.IsEmpty(),
			FString::Printf(
				TEXT("Runtime bind trait chains must keep short single traits inline, split long single traits, and place every multi-trait element on its own line:\n%s"),
				*FString::Join(Violations, TEXT("\n")))));
	}
};

#endif
