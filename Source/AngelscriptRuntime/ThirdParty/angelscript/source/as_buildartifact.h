/*
   AngelCode Scripting Library
   Copyright (c) 2003-2017 Andreas Jonsson

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the restrictions in the AngelScript source license.
*/

#ifndef AS_BUILDARTIFACT_H
#define AS_BUILDARTIFACT_H

#include "as_config.h"
#include "as_string.h"

BEGIN_AS_NAMESPACE

class asCGlobalProperty;
class asCObjectProperty;
class asCTypeInfo;
class asCScriptFunction;

//[UE++]: Unreal-free, pre-compiler Cache V2 invocation coordinates. Numeric
// values intentionally mirror the common FunctionBody invocation kind without
// introducing an Unreal dependency into the maintained AngelScript fork.
enum asEBuildArtifactInvocationKind
{
	asBUILD_ARTIFACT_INVOCATION_INVALID = 0,
	asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION = 1,
	asBUILD_ARTIFACT_INVOCATION_METHOD = 2,
	asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR = 3,
	asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR = 4,
	asBUILD_ARTIFACT_INVOCATION_FACTORY = 5,
	asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR = 6,
	asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR = 7,
	asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS = 8,
	asBUILD_ARTIFACT_INVOCATION_PUBLIC_SINGLE_FUNCTION = 9,
	asBUILD_ARTIFACT_INVOCATION_LAMBDA = 10
};

enum asEBuildArtifactIneligibleReason
{
	asBUILD_ARTIFACT_INELIGIBLE_NONE = 0,
	asBUILD_ARTIFACT_INELIGIBLE_INVALID_INVOCATION_KIND = 1,
	asBUILD_ARTIFACT_INELIGIBLE_MISSING_MODULE = 2,
	asBUILD_ARTIFACT_INELIGIBLE_MISSING_OWNER = 3,
	asBUILD_ARTIFACT_INELIGIBLE_MISSING_DECLARATION = 4,
	asBUILD_ARTIFACT_INELIGIBLE_MISSING_CANONICAL_SOURCE = 5,
	asBUILD_ARTIFACT_INELIGIBLE_MISSING_SOURCE_SECTION = 6,
	asBUILD_ARTIFACT_INELIGIBLE_PUBLIC_SINGLE_UNSTABLE_COORDINATE = 7,
	asBUILD_ARTIFACT_INELIGIBLE_LAMBDA_UNSTABLE_COORDINATE = 8
};

struct asSBuildArtifactInvocation
{
	asSBuildArtifactInvocation()
		: descriptorVersion(1),
		  kind(asBUILD_ARTIFACT_INVOCATION_INVALID),
		  ineligibleReason(asBUILD_ARTIFACT_INELIGIBLE_INVALID_INVOCATION_KIND),
		  sourceTokenPos(0), sourceTokenLength(0), traits(0),
		  isGenerated(false), hasNode(false)
	{
	}

	asUINT descriptorVersion;
	asEBuildArtifactInvocationKind kind;
	asEBuildArtifactIneligibleReason ineligibleReason;
	asCString moduleName;
	asCString nameSpace;
	asCString ownerName;
	asCString functionName;
	asCString declaration;
	asCString canonicalSource;
	asCString sourceSection;
	asUINT sourceTokenPos;
	asUINT sourceTokenLength;
	asDWORD traits;
	bool isGenerated;
	bool hasNode;

	bool IsCacheable() const
	{
		return ineligibleReason == asBUILD_ARTIFACT_INELIGIBLE_NONE;
	}
};

typedef void (*asBUILDARTIFACTINVOCATIONCALLBACK_t)(
	const asSBuildArtifactInvocation *invocation,
	void *userData);

// The synchronous pre-compiler lookup result. Only RESTORED permits the
// builder to skip the current asCCompiler invocation. A callback that returns
// RESTORED must have committed a complete, validated artifact to function;
// the builder independently verifies that executable state is present.
enum asEBuildArtifactRestoreResult
{
	asBUILD_ARTIFACT_RESTORE_RESTORED = 1,
	asBUILD_ARTIFACT_RESTORE_MISS = 2,
	asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT = 3,
	asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE = 4
};

typedef asEBuildArtifactRestoreResult (*asBUILDARTIFACTRESTORECALLBACK_t)(
	const asSBuildArtifactInvocation *invocation,
	asCScriptFunction *function,
	void *userData);

// These are compiler-input dependencies, not module rebuild edges. They remain
// engine-local pointer coordinates until the host maps them to stable Cache V2
// keys after the authoritative declaration/type state has been produced.
enum asEBuildArtifactDependencyKind
{
	asBUILD_ARTIFACT_DEPENDENCY_INVALID = 0,
	asBUILD_ARTIFACT_DEPENDENCY_DECLARATION = 1,
	asBUILD_ARTIFACT_DEPENDENCY_SIGNATURE = 2,
	asBUILD_ARTIFACT_DEPENDENCY_VALUE_LAYOUT = 3,
	asBUILD_ARTIFACT_DEPENDENCY_PROPERTY_LAYOUT = 4,
	asBUILD_ARTIFACT_DEPENDENCY_GLOBAL_STORAGE = 5,
	asBUILD_ARTIFACT_DEPENDENCY_HARD_VALUE = 6,
	asBUILD_ARTIFACT_DEPENDENCY_FUNCTION_CONTENT = 7,
	asBUILD_ARTIFACT_DEPENDENCY_ENVIRONMENT_ABI = 8
};

enum asEBuildArtifactDependencyReferenceKind
{
	asBUILD_ARTIFACT_REFERENCE_INVALID = 0,
	asBUILD_ARTIFACT_REFERENCE_TYPE = 1,
	asBUILD_ARTIFACT_REFERENCE_FUNCTION = 2,
	asBUILD_ARTIFACT_REFERENCE_GLOBAL = 3,
	asBUILD_ARTIFACT_REFERENCE_PROPERTY = 4
};

struct asSBuildArtifactDependency
{
	asSBuildArtifactDependency()
		: descriptorVersion(1),
		  kind(asBUILD_ARTIFACT_DEPENDENCY_INVALID),
		  referenceKind(asBUILD_ARTIFACT_REFERENCE_INVALID),
		  type(0), function(0), globalProperty(0), propertyOwnerType(0),
		  objectProperty(0)
	{
	}

	asUINT descriptorVersion;
	asEBuildArtifactDependencyKind kind;
	asEBuildArtifactDependencyReferenceKind referenceKind;
	asCTypeInfo *type;
	asCScriptFunction *function;
	asCGlobalProperty *globalProperty;
	asCTypeInfo *propertyOwnerType;
	asCObjectProperty *objectProperty;
};

struct asSBuildArtifactCompileResult
{
	asSBuildArtifactCompileResult()
		: resultVersion(1), compileResult(-1), function(0), dependencies(0),
		  dependencyCount(0),
		  restoreResult(asBUILD_ARTIFACT_RESTORE_MISS),
		  compilerInvoked(true), succeeded(false)
	{
	}

	asUINT resultVersion;
	int compileResult;
	asCScriptFunction *function;
	const asSBuildArtifactDependency *dependencies;
	asUINT dependencyCount;
	asEBuildArtifactRestoreResult restoreResult;
	bool compilerInvoked;
	bool succeeded;
};

typedef void (*asBUILDARTIFACTCOMPILERESULTCALLBACK_t)(
	const asSBuildArtifactInvocation *invocation,
	const asSBuildArtifactCompileResult *result,
	void *userData);
//[UE--]

END_AS_NAMESPACE

#endif
