#include "Registration/AngelscriptCompileOnlyStub.h"

#include "angelscript.h"

namespace AngelscriptStandalone
{
	void CompileOnlyTrap(asIScriptGeneric* Generic)
	{
		(void)Generic;
		if (asIScriptContext* Context = asGetActiveContext())
		{
			Context->SetException(CompileOnlyTrapMessage);
		}
	}
}
