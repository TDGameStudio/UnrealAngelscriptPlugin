#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneSource.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace AngelscriptStandalone::Frontend
{
	enum class EDeclarationKind
	{
		Class,
		Struct,
		Enum,
		Delegate,
		Property,
		Function,
		Event,
		Global,
	};

	enum class EAccess
	{
		Unspecified,
		Public,
		Protected,
		Private,
	};

	struct FSourceReference
	{
		std::string LogicalPath;
		FByteOffset Begin = 0;
		FByteOffset End = 0;
	};

	struct FTypeReference
	{
		std::string Spelling;
		std::string StableTypeId;
		bool bConst = false;
		bool bReference = false;
		bool bHandle = false;
	};

	struct FParameterDeclaration
	{
		std::string Name;
		FTypeReference Type;
		std::string DefaultValue;
	};

	struct FMetadataEntry
	{
		std::string Name;
		std::string Value;
	};

	struct FEnumValueDeclaration
	{
		std::string Name;
		std::string ValueExpression;
	};

	struct FDeclaration
	{
		EDeclarationKind Kind = EDeclarationKind::Global;
		EAccess Access = EAccess::Unspecified;
		std::string StableId;
		std::string ModuleId;
		std::string Namespace;
		std::string Name;
		std::string QualifiedName;
		std::string Owner;
		std::string Declaration;
		FTypeReference Type;
		std::vector<FTypeReference> BaseTypes;
		std::vector<FParameterDeclaration> Parameters;
		std::vector<FMetadataEntry> Metadata;
		std::vector<FEnumValueDeclaration> EnumValues;
		std::string DefaultValue;
		bool bStatic = false;
		bool bConst = false;
		bool bOverride = false;
		bool bFinal = false;
		FSourceReference Source;
	};

	struct FDeclarationDiagnostic
	{
		std::string Code;
		std::string Message;
		FSourceSpan Span;
	};

	struct FDeclarationScanResult
	{
		bool bSuccess = false;
		std::vector<FDeclaration> Declarations;
		std::vector<FDeclarationDiagnostic> Diagnostics;
		std::vector<FSourceSpan> AnnotationSpans;
	};

	class ITypeOracle
	{
	public:
		virtual ~ITypeOracle() = default;
		virtual bool ResolveType(
			std::string_view TypeSpelling,
			std::string& OutStableTypeId) const = 0;
	};

	struct FTypeResolutionResult
	{
		bool bSuccess = false;
		std::vector<FDeclarationDiagnostic> Diagnostics;
	};

	FDeclarationScanResult ScanDeclarations(
		const FSourceInput& Source,
		std::string_view PreprocessedSource);

	FTypeResolutionResult ResolveDeclarationTypes(
		std::vector<FDeclaration>& Declarations,
		const ITypeOracle& Oracle);
}
