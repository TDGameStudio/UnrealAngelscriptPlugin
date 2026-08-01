#include "angelscript.h"
#include "scriptarray/scriptarray.h"
#include "scriptdictionary/scriptdictionary.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"

#include <iostream>
#include <string>

namespace
{
	void MessageCallback(const asSMessageInfo* Message, void*)
	{
		std::cerr
			<< (Message->section != nullptr ? Message->section : "<unknown>")
			<< ':' << Message->row << ':' << Message->col
			<< ": " << (Message->message != nullptr ? Message->message : "<no message>")
			<< '\n';
	}

	bool Require(bool Condition, const char* What)
	{
		if (!Condition)
		{
			std::cerr << "FAILED: " << What << '\n';
		}
		return Condition;
	}
}

int main()
{
	asIScriptEngine* Engine = asCreateScriptEngine();
	if (!Require(Engine != nullptr, "create engine"))
	{
		return 1;
	}

	Engine->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
	if (!Require(
			Engine->SetEngineProperty(asEP_ALLOW_IMPLICIT_HANDLE_TYPES, 1) >= 0,
			"enable fork implicit-handle profile"))
	{
		Engine->ShutDownAndRelease();
		return 1;
	}
	if (!Require(
			Engine->SetEngineProperty(asEP_ALLOW_UNSAFE_REFERENCES, 1) >= 0,
			"enable maintained-fork reference profile"))
	{
		Engine->ShutDownAndRelease();
		return 1;
	}
	RegisterScriptArray(Engine, true);
	RegisterStdString(Engine);
	RegisterStdStringUtils(Engine);
	RegisterScriptDictionary(Engine);
	RegisterScriptMath(Engine);

	const char* Source = R"AS(
		int main()
		{
			array<string> values = array<string>();
			values.insertLast("alpha");
			values.insertLast("beta");
			dictionary valuesByName = dictionary();
			int64 storedAnswer = 42;
			valuesByName.set("answer", storedAnswer);
			int64 answer = 0;
			const bool found = valuesByName.get("answer", answer);
			const array<string> splitValues = "one,two".split(",");
			return values.length() == 2
				&& values[0] == "alpha"
				&& splitValues.length() == 2
				&& join(splitValues, "|") == "one|two"
				&& found
				&& answer == 42
				&& abs(-4.0f) == 4.0f
				? 17 : -1;
		}
	)AS";

	asIScriptModule* Module = Engine->GetModule("addons", asGM_ALWAYS_CREATE);
	bool Passed = Require(Module != nullptr, "create module");
	Passed &= Require(Module->AddScriptSection("addons.as", Source) >= 0, "add script section");
	Passed &= Require(Module->Build() >= 0, "build add-on fixture");

	asIScriptFunction* Main = Module->GetFunctionByDecl("int main()");
	Passed &= Require(Main != nullptr, "resolve main");
	asIScriptContext* Context = Engine->CreateContext();
	Passed &= Require(Context != nullptr, "create context");
	if (Context != nullptr && Main != nullptr)
	{
		Passed &= Require(Context->Prepare(Main) >= 0, "prepare main");
		const int ExecuteResult = Context->Execute();
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			asIScriptFunction* ExceptionFunction = Context->GetExceptionFunction();
			std::cerr
				<< "execution state: " << ExecuteResult
				<< ", exception: "
				<< (Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "<none>")
				<< ", line: " << Context->GetExceptionLineNumber()
				<< ", function: "
				<< (ExceptionFunction != nullptr ? ExceptionFunction->GetDeclaration() : "<none>")
				<< '\n';
		}
		Passed &= Require(ExecuteResult == asEXECUTION_FINISHED, "execute main");
		Passed &= Require(static_cast<int>(Context->GetReturnDWord()) == 17, "add-on fixture result");
		Context->Release();
	}

	Engine->ShutDownAndRelease();
	return Passed ? 0 : 1;
}
