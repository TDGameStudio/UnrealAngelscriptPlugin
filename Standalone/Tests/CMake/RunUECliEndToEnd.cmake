if(NOT DEFINED AS_STANDALONE_EXE)
	message(FATAL_ERROR "AS_STANDALONE_EXE is required")
endif()
if(NOT DEFINED AS_STANDALONE_FIXTURE_ROOT)
	message(FATAL_ERROR "AS_STANDALONE_FIXTURE_ROOT is required")
endif()
if(NOT DEFINED AS_STANDALONE_TEST_ROOT)
	message(FATAL_ERROR "AS_STANDALONE_TEST_ROOT is required")
endif()

file(REMOVE_RECURSE "${AS_STANDALONE_TEST_ROOT}")
file(MAKE_DIRECTORY "${AS_STANDALONE_TEST_ROOT}/source")
file(COPY
	"${AS_STANDALONE_FIXTURE_ROOT}/project/"
	DESTINATION "${AS_STANDALONE_TEST_ROOT}/bundle")
file(WRITE
	"${AS_STANDALONE_TEST_ROOT}/source/Main.as"
	"int Validate() { return 42; }\n")

execute_process(
	COMMAND
		"${AS_STANDALONE_EXE}"
		compile
		--dialect ue
		--script-root "${AS_STANDALONE_TEST_ROOT}/source"
		--entry Main.as
		--bundle "${AS_STANDALONE_TEST_ROOT}/bundle"
		--output "${AS_STANDALONE_TEST_ROOT}/output"
		--diagnostics json
		--emit-bytecode
	RESULT_VARIABLE COMPILE_RESULT
	OUTPUT_VARIABLE COMPILE_STDOUT
	ERROR_VARIABLE COMPILE_STDERR)
if(NOT COMPILE_RESULT EQUAL 0)
	message(FATAL_ERROR
		"UE-validation CLI failed (${COMPILE_RESULT})\n${COMPILE_STDOUT}\n${COMPILE_STDERR}")
endif()

file(READ "${AS_STANDALONE_TEST_ROOT}/output/result.json" RESULT_JSON)
if(NOT RESULT_JSON MATCHES "\"profile\": \"ue-validation\"")
	message(FATAL_ERROR "result.json did not identify the UE-validation profile")
endif()
if(NOT RESULT_JSON MATCHES "\"ueValidationOnly\": true")
	message(FATAL_ERROR "result.json did not preserve the no-execution boundary")
endif()
if(NOT EXISTS "${AS_STANDALONE_TEST_ROOT}/output/diagnostics.jsonl")
	message(FATAL_ERROR "UE-validation diagnostics.jsonl is missing")
endif()
file(GLOB BYTECODE_FILES
	"${AS_STANDALONE_TEST_ROOT}/output/modules/*.asbc")
file(GLOB CLASS_FILES
	"${AS_STANDALONE_TEST_ROOT}/output/modules/*.classes.jsonl")
list(LENGTH BYTECODE_FILES BYTECODE_COUNT)
list(LENGTH CLASS_FILES CLASS_COUNT)
if(NOT BYTECODE_COUNT EQUAL 1 OR NOT CLASS_COUNT EQUAL 1)
	message(FATAL_ERROR
		"UE-validation output must contain one bytecode and one class record file")
endif()

execute_process(
	COMMAND
		"${AS_STANDALONE_EXE}"
		compile
		--dialect ue
		--script-root "${AS_STANDALONE_TEST_ROOT}/source"
		--entry Main.as
		--bundle "${AS_STANDALONE_TEST_ROOT}/missing"
		--output "${AS_STANDALONE_TEST_ROOT}/invalid-output"
	RESULT_VARIABLE INVALID_RESULT
	OUTPUT_QUIET
	ERROR_QUIET)
if(NOT INVALID_RESULT EQUAL 2)
	message(FATAL_ERROR
		"invalid explicit bundle must exit 2 without fallback; got ${INVALID_RESULT}")
endif()
if(EXISTS "${AS_STANDALONE_TEST_ROOT}/invalid-output/result.json")
	message(FATAL_ERROR
		"invalid explicit bundle published a manifest-valid output")
endif()
