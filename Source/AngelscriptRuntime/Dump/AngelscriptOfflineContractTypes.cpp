#include "AngelscriptOfflineContractTypes.h"

namespace AngelscriptOfflineContract
{
	const TCHAR* LexToString(const EBundleKind Value)
	{
		switch (Value)
		{
		case EBundleKind::DefaultEngine: return TEXT("default-engine");
		case EBundleKind::Project: return TEXT("project");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const ESymbolKind Value)
	{
		switch (Value)
		{
		case ESymbolKind::Type: return TEXT("type");
		case ESymbolKind::Callable: return TEXT("callable");
		case ESymbolKind::Property: return TEXT("property");
		case ESymbolKind::EnumValue: return TEXT("enum-value");
		case ESymbolKind::Typedef: return TEXT("typedef");
		case ESymbolKind::Funcdef: return TEXT("funcdef");
		case ESymbolKind::Delegate: return TEXT("delegate");
		case ESymbolKind::Global: return TEXT("global");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const ETypeKind Value)
	{
		switch (Value)
		{
		case ETypeKind::Primitive: return TEXT("primitive");
		case ETypeKind::Value: return TEXT("value");
		case ETypeKind::Reference: return TEXT("reference");
		case ETypeKind::Template: return TEXT("template");
		case ETypeKind::Enum: return TEXT("enum");
		case ETypeKind::Typedef: return TEXT("typedef");
		case ETypeKind::Funcdef: return TEXT("funcdef");
		case ETypeKind::Delegate: return TEXT("delegate");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const ECallableKind Value)
	{
		switch (Value)
		{
		case ECallableKind::GlobalFunction: return TEXT("global-function");
		case ECallableKind::Method: return TEXT("method");
		case ECallableKind::Behavior: return TEXT("behavior");
		case ECallableKind::Factory: return TEXT("factory");
		case ECallableKind::Constructor: return TEXT("constructor");
		case ECallableKind::Destructor: return TEXT("destructor");
		case ECallableKind::Event: return TEXT("event");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const EParameterDirection Value)
	{
		switch (Value)
		{
		case EParameterDirection::Value: return TEXT("value");
		case EParameterDirection::In: return TEXT("in");
		case EParameterDirection::Out: return TEXT("out");
		case EParameterDirection::InOut: return TEXT("inout");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const EOriginLayer Value)
	{
		switch (Value)
		{
		case EOriginLayer::HostSurface: return TEXT("host-surface");
		case EOriginLayer::ScriptBaseline: return TEXT("script-baseline");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const EOriginKind Value)
	{
		switch (Value)
		{
		case EOriginKind::Manual: return TEXT("manual");
		case EOriginKind::Generated: return TEXT("generated");
		case EOriginKind::NativeModule: return TEXT("native-module");
		case EOriginKind::Reflective: return TEXT("reflective");
		case EOriginKind::Blueprint: return TEXT("blueprint");
		case EOriginKind::Script: return TEXT("script");
		case EOriginKind::OptionalPlugin: return TEXT("optional-plugin");
		case EOriginKind::Project: return TEXT("project");
		case EOriginKind::Unknown: return TEXT("unknown");
		default: return TEXT("unknown");
		}
	}

	const TCHAR* LexToString(const EAvailability Value)
	{
		switch (Value)
		{
		case EAvailability::Available: return TEXT("available");
		case EAvailability::EditorOnly: return TEXT("editor-only");
		case EAvailability::Deprecated: return TEXT("deprecated");
		case EAvailability::Internal: return TEXT("internal");
		case EAvailability::Unavailable: return TEXT("unavailable");
		case EAvailability::Unknown: return TEXT("unknown");
		default: return TEXT("unknown");
		}
	}
}
