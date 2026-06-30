#pragma once

#include "CQTest.h"

#ifndef WITH_ANGELSCRIPT_UNITTESTS
#define WITH_ANGELSCRIPT_UNITTESTS 0
#endif

#if !WITH_ANGELSCRIPT_UNITTESTS

#undef _TEST_CLASS_IMPL_EXT
#undef _TEST_CLASS_IMPL
#undef TEST_METHOD_WITH_TAGS
#undef TEST_METHOD
#undef TEST_CLASS_WITH_ASSERTS
#undef TEST_CLASS_WITH_ASSERTS_AND_TAGS
#undef TEST_CLASS
#undef TEST_CLASS_WITH_TAGS
#undef TEST_CLASS_WITH_BASE
#undef TEST_CLASS_WITH_BASE_AND_TAGS
#undef TEST_CLASS_WITH_BASE_AND_ASSERTS
#undef TEST_CLASS_WITH_FLAGS
#undef TEST_CLASS_WITH_FLAGS_AND_TAGS
#undef TEST_CLASS_WITH_BASE_AND_FLAGS
#undef TEST_CLASS_WITH_BASE_AND_FLAGS_AND_TAGS
#undef TEST
#undef TEST_WITH_TAGS

#define _TEST_CLASS_IMPL_EXT(_ClassName, _TestDir, _BaseClass, _AsserterType, _TestFlags, _TestTags) \
	struct _ClassName : public _BaseClass<_ClassName, _AsserterType>

#define _TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, _AsserterType, _TestFlags) \
	_TEST_CLASS_IMPL_EXT(_ClassName, _TestDir, _BaseClass, _AsserterType, _TestFlags, DefaultTags)

#define TEST_METHOD_WITH_TAGS(_MethodName, _TestTags) void _MethodName()
#define TEST_METHOD(_MethodName) TEST_METHOD_WITH_TAGS(_MethodName, DefaultTags)

#define TEST_CLASS_WITH_ASSERTS(_ClassName, _TestDir, _AsserterType) \
	_TEST_CLASS_IMPL(_ClassName, _TestDir, TTest, _AsserterType, DefaultFlags)
#define TEST_CLASS_WITH_ASSERTS_AND_TAGS(_ClassName, _TestDir, _AsserterType, _TestTags) \
	_TEST_CLASS_IMPL_EXT(_ClassName, _TestDir, TTest, _AsserterType, DefaultFlags, _TestTags)
#define TEST_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_ASSERTS(_ClassName, _TestDir, FNoDiscardAsserter)
#define TEST_CLASS_WITH_TAGS(_ClassName, _TestDir, _TestTags) \
	TEST_CLASS_WITH_ASSERTS_AND_TAGS(_ClassName, _TestDir, FNoDiscardAsserter, _TestTags)
#define TEST_CLASS_WITH_BASE(_ClassName, _TestDir, _BaseClass) \
	_TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, FNoDiscardAsserter, DefaultFlags)
#define TEST_CLASS_WITH_BASE_AND_TAGS(_ClassName, _TestDir, _BaseClass, _TestTags) \
	_TEST_CLASS_IMPL_EXT(_ClassName, _TestDir, _BaseClass, FNoDiscardAsserter, DefaultFlags, _TestTags)
#define TEST_CLASS_WITH_BASE_AND_ASSERTS(_ClassName, _TestDir, _BaseClass, _AsserterType) \
	_TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, _AsserterType, DefaultFlags)
#define TEST_CLASS_WITH_FLAGS(_ClassName, _TestDir, _Flags) \
	_TEST_CLASS_IMPL(_ClassName, _TestDir, TTest, FNoDiscardAsserter, _Flags)
#define TEST_CLASS_WITH_FLAGS_AND_TAGS(_ClassName, _TestDir, _Flags, _TestTags) \
	_TEST_CLASS_IMPL_EXT(_ClassName, _TestDir, TTest, FNoDiscardAsserter, _Flags, _TestTags)
#define TEST_CLASS_WITH_BASE_AND_FLAGS(_ClassName, _TestDir, _BaseClass, _Flags) \
	_TEST_CLASS_IMPL(_ClassName, _TestDir, _BaseClass, FNoDiscardAsserter, _Flags)
#define TEST_CLASS_WITH_BASE_AND_FLAGS_AND_TAGS(_ClassName, _TestDir, _BaseClass, _Flags, _TestTags) \
	_TEST_CLASS_IMPL_EXT(_ClassName, _TestDir, _BaseClass, FNoDiscardAsserter, _Flags, _TestTags)

#define TEST(_TestName, _TestDir) \
	TEST_CLASS(_TestName, _TestDir) \
	{ \
		TEST_METHOD(_TestName##_Method); \
	}; \
	void _TestName::_TestName##_Method()

#define TEST_WITH_TAGS(_TestName, _TestDir, _TestTags) \
	TEST_CLASS_WITH_TAGS(_TestName, _TestDir, _TestTags) \
	{ \
		TEST_METHOD(_TestName##_Method); \
	}; \
	void _TestName::_TestName##_Method()

#endif
