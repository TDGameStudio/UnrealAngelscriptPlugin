#ifndef WITH_ANGELSCRIPT_UNITTESTS
#error WITH_ANGELSCRIPT_UNITTESTS must be defined by AngelscriptTest.Build.cs.
#endif

#ifndef WITH_ANGELSCRIPT_MODULE_BINDINGS
#error WITH_ANGELSCRIPT_MODULE_BINDINGS must be defined by AngelscriptRuntime.Build.cs.
#endif

static_assert(
	WITH_ANGELSCRIPT_UNITTESTS == 0 || WITH_ANGELSCRIPT_UNITTESTS == 1,
	"WITH_ANGELSCRIPT_UNITTESTS must be a boolean-like compile definition.");

static_assert(
	WITH_ANGELSCRIPT_MODULE_BINDINGS == 0 || WITH_ANGELSCRIPT_MODULE_BINDINGS == 1,
	"WITH_ANGELSCRIPT_MODULE_BINDINGS must be a boolean-like compile definition.");
