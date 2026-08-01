#include "StdLib/AngelscriptStandaloneStdLib.h"

#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"

#include <iostream>

namespace AngelscriptStandalone
{
	namespace
	{
		struct FStandardLibraryHostState
		{
			FStandardLibraryOptions Options;
		};

		char StandardLibraryUserDataKey;

		asPWORD GetStandardLibraryUserDataKey()
		{
			return reinterpret_cast<asPWORD>(&StandardLibraryUserDataKey);
		}

		FStandardLibraryHostState* GetActiveHostState()
		{
			asIScriptContext* Context = asGetActiveContext();
			asIScriptEngine* Engine = Context != nullptr ? Context->GetEngine() : nullptr;
			return Engine != nullptr
				? static_cast<FStandardLibraryHostState*>(
					Engine->GetUserData(GetStandardLibraryUserDataKey()))
				: nullptr;
		}

		void CleanupStandardLibraryHostState(asIScriptEngine* Engine)
		{
			if (Engine == nullptr)
			{
				return;
			}
			auto* State = static_cast<FStandardLibraryHostState*>(
				Engine->SetUserData(nullptr, GetStandardLibraryUserDataKey()));
			delete State;
		}

		void PrintGeneric(asIScriptGeneric* Generic)
		{
			const auto* Message = *reinterpret_cast<scriptstring_t**>(Generic->GetAddressOfArg(0));
			if (Message == nullptr)
			{
				return;
			}
			FStandardLibraryHostState* State = GetActiveHostState();
			if (State != nullptr && State->Options.Print)
			{
				State->Options.Print(std::string(Message->data(), Message->size()));
			}
			else
			{
				std::cout << *Message << '\n';
			}
		}

		void AssertGeneric(asIScriptGeneric* Generic)
		{
			const bool Condition = Generic->GetArgByte(0) != 0;
			if (Condition)
			{
				return;
			}
			const auto* Message = *reinterpret_cast<scriptstring_t**>(Generic->GetAddressOfArg(1));
			asIScriptContext* Context = asGetActiveContext();
			if (Context != nullptr)
			{
				Context->SetException(
					Message != nullptr && !Message->empty()
						? Message->c_str()
						: "standalone assertion failed");
			}
		}
	}

	bool RegisterNativeStandardLibrary(
		asIScriptEngine* Engine,
		const FStandardLibraryOptions& Options,
		std::string& OutError)
	{
		if (Engine == nullptr)
		{
			OutError = "cannot register standard library on a null engine";
			return false;
		}

		auto* HostState = static_cast<FStandardLibraryHostState*>(
			Engine->GetUserData(GetStandardLibraryUserDataKey()));
		if (HostState == nullptr)
		{
			HostState = new FStandardLibraryHostState();
			Engine->SetUserData(HostState, GetStandardLibraryUserDataKey());
			Engine->SetEngineUserDataCleanupCallback(
				&CleanupStandardLibraryHostState,
				GetStandardLibraryUserDataKey());
		}
		HostState->Options = Options;

		RegisterScriptArray(Engine, true);
		RegisterStdString(Engine);
		RegisterStdStringUtils(Engine);
		RegisterScriptDictionary(Engine);
		RegisterScriptMath(Engine);

		if (Engine->RegisterGlobalFunction(
				"void print(const string &in message)",
				asFUNCTION(PrintGeneric),
				asCALL_GENERIC) < 0)
		{
			OutError = "failed to register print";
			return false;
		}
		if (Engine->RegisterGlobalFunction(
				"void assert(bool condition, const string &in message = \"\")",
				asFUNCTION(AssertGeneric),
				asCALL_GENERIC) < 0)
		{
			OutError = "failed to register assert";
			return false;
		}
		return true;
	}
}
