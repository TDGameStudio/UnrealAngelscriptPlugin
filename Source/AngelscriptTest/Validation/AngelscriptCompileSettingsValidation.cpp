#ifndef WITH_ANGELSCRIPT_UNITTESTS
#error WITH_ANGELSCRIPT_UNITTESTS must be defined by AngelscriptTest.Build.cs.
#endif

static_assert(
	WITH_ANGELSCRIPT_UNITTESTS == 0 || WITH_ANGELSCRIPT_UNITTESTS == 1,
	"WITH_ANGELSCRIPT_UNITTESTS must be a boolean-like compile definition.");
