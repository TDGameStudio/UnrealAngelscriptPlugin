#ifndef ANGELSCRIPT_RUNTIME_UNITTEST_POLICY_OWNER
#error ANGELSCRIPT_RUNTIME_UNITTEST_POLICY_OWNER must be publicly propagated by AngelscriptRuntime.Build.cs.
#endif

#if ANGELSCRIPT_RUNTIME_UNITTEST_POLICY_OWNER != 1
#error ANGELSCRIPT_RUNTIME_UNITTEST_POLICY_OWNER must identify AngelscriptRuntime as the unit-test compile-policy owner.
#endif

#ifndef WITH_ANGELSCRIPT_UNITTESTS
#error WITH_ANGELSCRIPT_UNITTESTS must be defined and publicly propagated by AngelscriptRuntime.Build.cs.
#endif

#ifndef WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS
#error WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS must be defined by AngelscriptRuntime.Build.cs.
#endif

static_assert(
	WITH_ANGELSCRIPT_UNITTESTS == 0 || WITH_ANGELSCRIPT_UNITTESTS == 1,
	"WITH_ANGELSCRIPT_UNITTESTS must be a boolean-like compile definition.");

static_assert(
	WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS == 0 || WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS == 1,
	"WITH_ANGELSCRIPT_NATIVE_MODULE_FUNCTION_ADDRESS must be a boolean-like compile definition.");
