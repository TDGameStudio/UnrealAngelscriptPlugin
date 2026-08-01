foreach(REQUIRED_VARIABLE
	AS_STANDALONE_BINARY_DIR
	AS_STANDALONE_CONFIGURATION
	AS_STANDALONE_TEST_ROOT
	AS_STANDALONE_MANIFEST_SCRIPT)
	if(NOT DEFINED ${REQUIRED_VARIABLE})
		message(FATAL_ERROR "${REQUIRED_VARIABLE} is required")
	endif()
endforeach()

file(REMOVE_RECURSE "${AS_STANDALONE_TEST_ROOT}")
set(PACKAGE_A "${AS_STANDALONE_TEST_ROOT}/package-a")
set(PACKAGE_B "${AS_STANDALONE_TEST_ROOT}/package-b")

foreach(PACKAGE_ROOT IN ITEMS "${PACKAGE_A}" "${PACKAGE_B}")
	execute_process(
		COMMAND
			"${CMAKE_COMMAND}" --install "${AS_STANDALONE_BINARY_DIR}"
			--config "${AS_STANDALONE_CONFIGURATION}"
			--prefix "${PACKAGE_ROOT}"
		RESULT_VARIABLE INSTALL_RESULT
		OUTPUT_VARIABLE INSTALL_STDOUT
		ERROR_VARIABLE INSTALL_STDERR)
	if(NOT INSTALL_RESULT EQUAL 0)
		message(FATAL_ERROR
			"standalone install failed (${INSTALL_RESULT})\n${INSTALL_STDOUT}\n${INSTALL_STDERR}")
	endif()
	execute_process(
		COMMAND
			"${CMAKE_COMMAND}"
			"-DPACKAGE_ROOT=${PACKAGE_ROOT}"
			-P "${AS_STANDALONE_MANIFEST_SCRIPT}"
		RESULT_VARIABLE MANIFEST_RESULT)
	if(NOT MANIFEST_RESULT EQUAL 0)
		message(FATAL_ERROR "package manifest generation failed")
	endif()
endforeach()

file(READ "${PACKAGE_A}/package-manifest.json" MANIFEST_A)
file(READ "${PACKAGE_B}/package-manifest.json" MANIFEST_B)
if(NOT MANIFEST_A STREQUAL MANIFEST_B)
	message(FATAL_ERROR "two package assemblies produced different manifests")
endif()

set(EXPECTED_FILES
	"as-standalone.exe"
	"README.md"
	"SUPPORT_MATRIX.md"
	"LICENSES/UnrealAngelscript.txt"
	"LICENSES/RapidJSON.txt"
	"LICENSES/ThirdParty-Provenance.md"
	"Schemas/manifest.schema.json"
	"Schemas/symbol.schema.json"
	"Schemas/asset.schema.json"
	"Schemas/result.schema.json"
	"Schemas/differential-result.schema.json"
	"Schemas/corpus-index.schema.json"
	"contracts/default-engine/manifest.json"
	"contracts/default-engine/symbols.jsonl"
	"contracts/default-engine/assets.jsonl"
	"Examples/Native/hello.as"
	"Examples/UEValidation/basic.as")
foreach(EXPECTED IN LISTS EXPECTED_FILES)
	if(NOT EXISTS "${PACKAGE_A}/${EXPECTED}")
		message(FATAL_ERROR "package is missing ${EXPECTED}")
	endif()
endforeach()

set(ALLOWED_FILES ${EXPECTED_FILES} "package-manifest.json")
list(SORT ALLOWED_FILES)
file(
	GLOB_RECURSE ACTUAL_FILES
	LIST_DIRECTORIES false
	RELATIVE "${PACKAGE_A}"
	"${PACKAGE_A}/*")
list(SORT ACTUAL_FILES)
if(NOT ACTUAL_FILES STREQUAL ALLOWED_FILES)
	message(FATAL_ERROR
		"V1 is a CLI-only package; installed file allowlist mismatch\n"
		"expected: ${ALLOWED_FILES}\n"
		"actual: ${ACTUAL_FILES}")
endif()
if(EXISTS "${PACKAGE_A}/contracts/project"
	OR EXISTS "${PACKAGE_A}/contracts/project-engine")
	message(FATAL_ERROR "release package must not contain a project bundle")
endif()

file(READ
	"${PACKAGE_A}/contracts/default-engine/manifest.json"
	DEFAULT_ENGINE_MANIFEST)
if(NOT DEFAULT_ENGINE_MANIFEST MATCHES "\"bundleKind\":\"default-engine\""
	OR NOT DEFAULT_ENGINE_MANIFEST MATCHES "\"unrealVersion\":\"5\\.8\\.0-55116800"
	OR NOT DEFAULT_ENGINE_MANIFEST MATCHES "\"unreal\\.project-name\":\"AngelscriptProject\""
	OR NOT DEFAULT_ENGINE_MANIFEST MATCHES "\"name\":\"symbols\\.jsonl\",\"recordCount\":130068"
	OR NOT DEFAULT_ENGINE_MANIFEST MATCHES "\"AngelscriptGameplayTags\""
	OR NOT DEFAULT_ENGINE_MANIFEST MATCHES "\"AngelscriptGAS\"")
	message(FATAL_ERROR
		"packaged default is not the declared UE 5.8 AngelscriptProject export")
endif()

set(EXECUTABLE "${PACKAGE_A}/as-standalone.exe")
execute_process(
	COMMAND "${EXECUTABLE}" --version
	RESULT_VARIABLE VERSION_RESULT
	OUTPUT_VARIABLE VERSION_OUTPUT
	ERROR_VARIABLE VERSION_ERROR)
if(NOT VERSION_RESULT EQUAL 0
	OR NOT VERSION_OUTPUT MATCHES "^Unreal AngelScript 1\\.0\\.0"
	OR NOT VERSION_OUTPUT MATCHES "AngelScript 2\\.33\\.0 WIP lineage"
	OR NOT VERSION_OUTPUT MATCHES "ue-validation profile 1\\.0 \\(compile-only\\)")
	message(FATAL_ERROR
		"installed --version contract failed\n${VERSION_OUTPUT}\n${VERSION_ERROR}")
endif()

execute_process(
	COMMAND "${EXECUTABLE}" --help
	RESULT_VARIABLE HELP_RESULT
	OUTPUT_VARIABLE HELP_OUTPUT
	ERROR_VARIABLE HELP_ERROR)
if(NOT HELP_RESULT EQUAL 0
	OR NOT HELP_OUTPUT MATCHES "compile --dialect native\\|ue"
	OR HELP_OUTPUT MATCHES "run --dialect ue"
	OR HELP_OUTPUT MATCHES "run-ue")
	message(FATAL_ERROR
		"installed --help contract failed\n${HELP_OUTPUT}\n${HELP_ERROR}")
endif()

execute_process(
	COMMAND
		"${EXECUTABLE}" run
		--script-root "${PACKAGE_A}/Examples/Native"
		--entry hello.as
		--output "${AS_STANDALONE_TEST_ROOT}/native-output"
		--diagnostics text
		--
		Package
	RESULT_VARIABLE NATIVE_RESULT
	OUTPUT_VARIABLE NATIVE_STDOUT
	ERROR_VARIABLE NATIVE_STDERR)
if(NOT NATIVE_RESULT EQUAL 0
	OR NOT NATIVE_STDOUT MATCHES "hello, Package")
	message(FATAL_ERROR
		"installed native example failed (${NATIVE_RESULT})\n${NATIVE_STDOUT}\n${NATIVE_STDERR}")
endif()

execute_process(
	COMMAND
		"${EXECUTABLE}" compile
		--dialect ue
		--script-root "${PACKAGE_A}/Examples/UEValidation"
		--entry basic.as
		--output "${AS_STANDALONE_TEST_ROOT}/ue-output"
		--diagnostics text
		--emit-bytecode
	RESULT_VARIABLE UE_RESULT
	OUTPUT_VARIABLE UE_STDOUT
	ERROR_VARIABLE UE_STDERR)
if(NOT UE_RESULT EQUAL 0)
	message(FATAL_ERROR
		"installed packaged-default UE analysis failed (${UE_RESULT})\n${UE_STDOUT}\n${UE_STDERR}")
endif()
file(READ "${AS_STANDALONE_TEST_ROOT}/ue-output/result.json" UE_RESULT_JSON)
if(NOT UE_RESULT_JSON MATCHES "\"kind\":\"default-engine\""
	OR NOT UE_RESULT_JSON MATCHES "\"ueValidationOnly\": true")
	message(FATAL_ERROR "installed UE analysis did not use the packaged default boundary")
endif()
