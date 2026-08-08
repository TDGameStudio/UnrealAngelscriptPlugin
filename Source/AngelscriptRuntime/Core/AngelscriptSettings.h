#pragma once

#include "CoreMinimal.h"

#include "AngelscriptSettings.generated.h"

UENUM()
enum class EAngelscriptPropertyEditSpecifier : uint8
{
	EditAnywhere,
	EditInstanceOnly,
	EditDefaultsOnly,
	NotEditable,
};

UENUM()
enum class EAngelscriptPropertyBlueprintSpecifier : uint8
{
	BlueprintReadWrite,
	BlueprintReadOnly,
	BlueprintHidden,
};

UENUM()
enum class EAngelscriptMathNamespace : uint8
{
	// Use the Math:: namespace for math functions in angelscript
	Math UMETA(DisplayName = "Math::"),
	// Use the FMath:: namespace for math functions in angelscript
	FMath UMETA(DisplayName = "FMath::"),
};

UENUM()
enum class EAngelscriptStaticClassMode : uint8
{
	Allowed,
	Deprecated,
	Disallowed,
};

UCLASS(Config=Engine, DefaultConfig)
class ANGELSCRIPTRUNTIME_API UAngelscriptSettings : public UObject
{
	GENERATED_BODY()
public:
	// Additional preprocessor flags which will be defined when preprocessing angelscript files.
	// Add them e.g. to BaseEngine.ini:
	//   [/Script/AngelscriptRuntime.AngelscriptSettings]
	//   +PreprocessorFlags="FOO"
	//   +PreprocessorFlags="BAR"
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	TArray<FString> PreprocessorFlags;

	/* Whether to use the new automatic import system (explicit import statements no longer used) */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Backwards Compatibility", Meta = (ConfigRestartRequired = true))
	bool bAutomaticImports = true;

	/* Emit warnings when import statements are used while automatic imports are turned on. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Backwards Compatibility", Meta = (EditCondition = "bAutomaticImports", EditConditionHides, ConfigRestartRequired = true))
	bool bWarnOnManualImportStatements = true;

	/* Namespace to use for math library functions in angelscript */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Backwards Compatibility", Meta = (ConfigRestartRequired = true))
	EAngelscriptMathNamespace MathNamespace = EAngelscriptMathNamespace::Math;

	/* Whether UFUNCTION()s should be BlueprintCallable by default without explicit BlueprintCallable specifier. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	bool bDefaultFunctionBlueprintCallable = true;

	/* Default Edit access specifier for script UPROPERTY()s without explicit Edit specifier on classes. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	EAngelscriptPropertyEditSpecifier DefaultPropertyEditSpecifier = EAngelscriptPropertyEditSpecifier::EditAnywhere;

	/* Default Edit access specifier for script UPROPERTY()s without explicit Edit specifier on structs. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	EAngelscriptPropertyEditSpecifier DefaultPropertyEditSpecifierForStructs = EAngelscriptPropertyEditSpecifier::EditAnywhere;

	/* Default Blueprint access specifier for script UPROPERTY()s without explicit Blueprint specifier. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	EAngelscriptPropertyBlueprintSpecifier DefaultPropertyBlueprintSpecifier = EAngelscriptPropertyBlueprintSpecifier::BlueprintReadWrite;

	/* Some properties are implicitly treated as UPROPERTY:s to be seen correctly by the GC, if true this ensures such properties are marked as transient to avoid unintentional serialization */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	bool bMarkNonUpropertyPropertiesAsTransient = false;

	/**
	 * Whether to deprecate or disallow the usage of AAnyClass::StaticClass().
	 * The newer alternative does not require the StaticClass call, the class name can be used directly.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	EAngelscriptStaticClassMode StaticClassDeprecation = EAngelscriptStaticClassMode::Allowed;

	/* Whether to allow actor and component classes to be instantiated using their raw constructor, instead of forcing SpawnActor or UComponent::Create() calls. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Backwards Compatibility", Meta = (ConfigRestartRequired = true))
	bool bAllowRawConstructorsForComponentsAndActors = false;

	/**
	 * Whether any code that is placed within check() asserts, Print() statements and other development-only functions should only be allowed to call const methods.
	 * This is intended to help catch issues with side-effects being placed inside expressions that get compiled out in shipping builds.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	bool bForceConstWithinDevelopmentOnlyFunctions = true;

	/**
	 * Only in editor:
	 * If a script function takes longer than this time to execute, kill it with an exception.
	 * This allows the editor to recover from accidental infinite loops, but does not work in cooked games!
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript")
	float EditorMaximumScriptExecutionTime = 1.f;

	/* In order to avoid confusion with blueprints, make the 'float' type in script resolve to 'float64'. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Angelscript", Meta = (ConfigRestartRequired = true))
	bool bScriptFloatIsFloat64 = true;

	/* Deprecate usage of the 'double' type in script, in favor of 'float64' or just 'float' when bScriptFloatIsFloat64. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors", Meta = (ConfigRestartRequired = true))
	bool bDeprecateDoubleType = false;

	/* Emit a warning when using a float constant (eg `0.f`) to initialize a double value. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors", Meta = (ConfigRestartRequired = true))
	bool bWarnOnFloatConstantsForDoubleValues = false;

	/* Emit a warning for precision loss when integer division is used. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors", Meta = (ConfigRestartRequired = true))
	bool bWarnIntegerDivision = true;

	/**
	 * When opening a file in VS Code by clicking a source link, open a VS Code workspace with the Script folder.
	 * Turn this off when using a dedicated .code-workspace file through VSCodeWorkspacePath.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Editor")
	bool bOpenFolderOnVSCodeSourceLinks = true;

	/**
	 * Relative path from the project directory to a VS Code .code-workspace file.
	 * When set, this takes precedence over bOpenFolderOnVSCodeSourceLinks.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Editor")
	FString VSCodeWorkspacePath = FString("");

	/**
	 * Throw an exception when calling a function that requires a World Context to be set, but the current object is not in a world.
	 * Note: this error is only checked in editor for performance reasons.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors")
	bool bErrorWhenUsingInvalidWorldContext = true;

	/**
	 * Show a warning when the result of a const method is not used.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors")
	bool bWarnOnUnusedReturnValueForConstMethods = true;

	/**
	 * Show a warning when an implicit conversion between signed/unsigned integers can cause incorrect results.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors")
	bool bWarnOnImplicitSignedUnsignedConversion = true;

	/**
	 * Show an error when a function or property that is editor-only is used outside of an EDITOR block or editor-only script module.
	 *
	 * Note: Can cause false positives if a function or property is declared separately in editor-only and not-editor-only blocks.
	 * In that case, put the preprocessor directives inside the function body, instead of around the whole function.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors")
	bool bErrorOnIncorrectEditorOnlyCode = true;

	/**
	 * Show a warning when a comparison operator overload is implemented targeting a different type than the containing type.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors")
	bool bWarnOnDivergentComparisonOperatorOverloads = true;

	/**
	 * Show a warning when an increment or decrement (++ / --) expression is used within a complex expression.
	 * Side effects are not usually expected for longer expressions and can be hard to read.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Warnings and Errors")
	bool bWarnOnIncrementDecrementInComplexExpression = true;

	/**
	 * Script package names (/Script/ModuleName) that should be considered editor-only for the purposes of checking for incorrect usage.
	 */
	UPROPERTY(Config)
	TArray<FName> AdditionalEditorOnlyScriptPackageNames;

	/**
	 * Functions that should not be automatically evaluated by the debugger even if they are const accessors.
	 */
	UPROPERTY(Config)
	TSet<FString> DebuggerBlacklistAutomaticFunctionEvaluation;

	/**
	 * Functions that should not be automatically evaluated by the debugger even if they are const accessors, iff the object they are being called
	 * on is not currently a valid world context.
	 */
	UPROPERTY(Config)
	TSet<FString> DebuggerBlacklistAutomaticFunctionEvaluationWithoutWorldContext = {
		"AActor.GetWorldTimerManager",
		"AActor.GetGameInstance",
		"AActor.GetPhysicsVolume",
		"AActor.GetActorTimeDilation",
	};

	static UAngelscriptSettings& Get()
	{
		return *GetMutableDefault<UAngelscriptSettings>();
	}
};

