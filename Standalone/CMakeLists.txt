cmake_minimum_required(VERSION 3.25)

project(AngelscriptStandalone LANGUAGES CXX)

include(CTest)

execute_process(
	COMMAND git -C "${CMAKE_CURRENT_LIST_DIR}/.." rev-parse HEAD
	OUTPUT_VARIABLE ANGELSCRIPT_STANDALONE_GIT_COMMIT
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET
)
if(NOT ANGELSCRIPT_STANDALONE_GIT_COMMIT)
	set(ANGELSCRIPT_STANDALONE_GIT_COMMIT "unknown")
endif()

set(ANGELSCRIPT_RUNTIME_ROOT "${CMAKE_CURRENT_LIST_DIR}/../Source/AngelscriptRuntime")
set(ANGELSCRIPT_FORK_ROOT "${ANGELSCRIPT_RUNTIME_ROOT}/ThirdParty/angelscript/source")
set(ANGELSCRIPT_STANDALONE_COMPAT "${CMAKE_CURRENT_LIST_DIR}/Compat")

set(ANGELSCRIPT_MAINTAINED_FORK_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/Compat/Source/AngelscriptStandaloneEngineCompat.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Compat/Source/AngelscriptStandaloneMemoryCompat.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Compat/Source/AngelscriptStandaloneThreadCompat.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_atomic.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_builder.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_bytecode.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_callfunc.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_compiler.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_configgroup.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_context.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_datatype.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_gc.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_generic.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_globalproperty.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_memory.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_module.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_objecttype.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_outputbuffer.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_parser.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_restore.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_scriptcode.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_scriptengine.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_scriptfunction.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_scriptnode.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_scriptobject.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_string.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_string_util.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_tokenizer.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_typeinfo.cpp"
	"${ANGELSCRIPT_FORK_ROOT}/as_variablescope.cpp"
)

add_library(AngelscriptMaintainedFork STATIC ${ANGELSCRIPT_MAINTAINED_FORK_SOURCES})
target_compile_features(AngelscriptMaintainedFork PUBLIC cxx_std_20)
target_compile_definitions(
	AngelscriptMaintainedFork
	PRIVATE
		AS_MAX_PORTABILITY
		AS_NO_THREADS
		AS_USE_EXCEPTIONS=1
		AS_REFERENCE_DEBUGGING=0
		WITH_AS_DEBUGSERVER=0
		ANGELSCRIPT_EXPORT
		ANGELSCRIPTRUNTIME_API=
		DO_BLUEPRINT_GUARD=0
		UE_BUILD_SHIPPING=0
		WITH_EDITOR=0
)
target_include_directories(
	AngelscriptMaintainedFork
	BEFORE PRIVATE
		"${ANGELSCRIPT_STANDALONE_COMPAT}"
)
target_include_directories(
	AngelscriptMaintainedFork
	PUBLIC
		"${ANGELSCRIPT_RUNTIME_ROOT}/Core"
	"${ANGELSCRIPT_FORK_ROOT}"
)

# Unreal normally injects CoreMinimal.h through the module PCH. Reproduce that
# build boundary here instead of adding standalone-only includes to the fork.
if(MSVC)
	target_compile_options(AngelscriptMaintainedFork PRIVATE /FI"CoreMinimal.h")
else()
	target_compile_options(AngelscriptMaintainedFork PRIVATE -include CoreMinimal.h)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/Tests/CMake/AssertTargetInterfaces.cmake")

set(ANGELSCRIPT_ADDON_ROOT "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/AngelScriptAddons")
set(ANGELSCRIPT_STANDALONE_ADDON_SOURCES
	"${ANGELSCRIPT_ADDON_ROOT}/scriptarray/scriptarray.cpp"
	"${ANGELSCRIPT_ADDON_ROOT}/scriptdictionary/scriptdictionary.cpp"
	"${ANGELSCRIPT_ADDON_ROOT}/scriptmath/scriptmath.cpp"
	"${ANGELSCRIPT_ADDON_ROOT}/scriptstdstring/scriptstdstring.cpp"
	"${ANGELSCRIPT_ADDON_ROOT}/scriptstdstring/scriptstdstring_utils.cpp"
)

add_library(AngelscriptStandaloneAddons STATIC ${ANGELSCRIPT_STANDALONE_ADDON_SOURCES})
target_compile_features(AngelscriptStandaloneAddons PUBLIC cxx_std_20)
target_include_directories(AngelscriptStandaloneAddons PUBLIC "${ANGELSCRIPT_ADDON_ROOT}")
target_link_libraries(AngelscriptStandaloneAddons PUBLIC AngelscriptMaintainedFork)

set(ANGELSCRIPT_STANDALONE_HOST_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/Source/Adapters/AngelscriptAdapterRegistry.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Adapters/AngelscriptTemplateTraits.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/CLI/AngelscriptStandaloneArguments.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneArtifact.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneDeclarationAnalysis.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneDifferentialResult.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/Frontend/AngelscriptStandaloneDeclarations.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/Frontend/AngelscriptStandaloneFrontendSession.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/Frontend/AngelscriptStandaloneLexing.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/Frontend/AngelscriptStandaloneRewrite.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/Frontend/AngelscriptStandaloneSource.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneFrontend.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneNativeCompiler.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneSemanticObserver.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneSourceGraph.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptStandaloneUECompiler.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Compiler/AngelscriptValidationArtifact.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Contract/AngelscriptOfflineBundleLoader.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Contract/AngelscriptBundleTypeOracle.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Contract/AngelscriptOfflineIndices.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Contract/AngelscriptOfflineManifest.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Contract/AngelscriptOfflineRecords.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Host/AngelscriptStandaloneDiagnosticSink.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Host/AngelscriptStandaloneFileSystem.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Runtime/AngelscriptStandaloneAllocator.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Runtime/AngelscriptStandaloneExecutionGuard.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Runtime/AngelscriptStandaloneRunner.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Registration/AngelscriptCapabilityClassification.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Registration/AngelscriptCompileOnlyStub.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Registration/AngelscriptRegistrationLoader.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Registration/AngelscriptRegistrationPlan.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Registration/AngelscriptScriptBaselinePlan.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Resources/AngelscriptAssetIndex.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Resources/AngelscriptAssetPath.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Resources/AngelscriptResourceContext.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Resources/AngelscriptResourceDiagnostics.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Resources/AngelscriptResourceValidator.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/StdLib/AngelscriptStandaloneStdLib.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Support/AngelscriptStandaloneHash.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Support/AngelscriptStandaloneJson.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Source/Support/AngelscriptStandaloneStreamingHash.cpp"
)

add_library(AngelscriptStandaloneHost STATIC ${ANGELSCRIPT_STANDALONE_HOST_SOURCES})
target_compile_features(AngelscriptStandaloneHost PUBLIC cxx_std_20)
target_include_directories(
	AngelscriptStandaloneHost
	PUBLIC
		"${CMAKE_CURRENT_LIST_DIR}/Source"
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RapidJSON/include"
)
target_link_libraries(AngelscriptStandaloneHost PUBLIC AngelscriptStandaloneAddons)

add_executable(
	AngelscriptStandaloneCli
	"${CMAKE_CURRENT_LIST_DIR}/Source/CLI/AngelscriptStandaloneMain.cpp"
)
set_target_properties(AngelscriptStandaloneCli PROPERTIES OUTPUT_NAME "as-standalone")
target_link_libraries(AngelscriptStandaloneCli PRIVATE AngelscriptStandaloneHost)

set(
	ANGELSCRIPT_PACKAGED_DEFAULT_ARCHIVE
	"${CMAKE_CURRENT_LIST_DIR}/Contracts/UE5.8/default-engine.zip"
)
set(
	ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE
	"${CMAKE_CURRENT_BINARY_DIR}/packaged-contracts/default-engine"
)
if(NOT EXISTS "${ANGELSCRIPT_PACKAGED_DEFAULT_ARCHIVE}")
	message(FATAL_ERROR
		"packaged UE 5.8 default bundle is missing: "
		"${ANGELSCRIPT_PACKAGED_DEFAULT_ARCHIVE}")
endif()
file(REMOVE_RECURSE "${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}")
file(MAKE_DIRECTORY "${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}")
file(
	ARCHIVE_EXTRACT
	INPUT "${ANGELSCRIPT_PACKAGED_DEFAULT_ARCHIVE}"
	DESTINATION "${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}"
)
set(
	ANGELSCRIPT_PACKAGED_DEFAULT_FILES
	"manifest.json;a8dfe32e63967a6ef49cd6c3e8089e24be0393dab3f8d003d9ec2d25eb434ffa"
	"symbols.jsonl;911698ba49bf65c6ac7abacd5938691ce0872e81bbfd16c17a5d0a7ff3d71f54"
	"assets.jsonl;28a4028fc2418ca96f5a82734265ca9dfdf59f5903056882039b77be98d45d9f"
)
while(ANGELSCRIPT_PACKAGED_DEFAULT_FILES)
	list(POP_FRONT ANGELSCRIPT_PACKAGED_DEFAULT_FILES DEFAULT_FILE DEFAULT_SHA256)
	set(DEFAULT_PATH "${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}/${DEFAULT_FILE}")
	if(NOT EXISTS "${DEFAULT_PATH}")
		message(FATAL_ERROR
			"packaged UE 5.8 default bundle is missing ${DEFAULT_FILE}")
	endif()
	file(SHA256 "${DEFAULT_PATH}" ACTUAL_SHA256)
	if(NOT ACTUAL_SHA256 STREQUAL DEFAULT_SHA256)
		message(FATAL_ERROR
			"packaged UE 5.8 default ${DEFAULT_FILE} hash mismatch: "
			"expected ${DEFAULT_SHA256}, got ${ACTUAL_SHA256}")
	endif()
endwhile()
add_custom_command(
	TARGET AngelscriptStandaloneCli
	POST_BUILD
	COMMAND
		"${CMAKE_COMMAND}" -E make_directory
		"$<TARGET_FILE_DIR:AngelscriptStandaloneCli>/contracts/default-engine"
	COMMAND
		"${CMAKE_COMMAND}" -E copy_if_different
		"${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}/manifest.json"
		"$<TARGET_FILE_DIR:AngelscriptStandaloneCli>/contracts/default-engine/manifest.json"
	COMMAND
		"${CMAKE_COMMAND}" -E copy_if_different
		"${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}/symbols.jsonl"
		"$<TARGET_FILE_DIR:AngelscriptStandaloneCli>/contracts/default-engine/symbols.jsonl"
	COMMAND
		"${CMAKE_COMMAND}" -E copy_if_different
		"${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}/assets.jsonl"
		"$<TARGET_FILE_DIR:AngelscriptStandaloneCli>/contracts/default-engine/assets.jsonl"
	VERBATIM
)

if(MSVC)
	target_compile_options(AngelscriptMaintainedFork PRIVATE /W4 /permissive- /EHsc)
	target_compile_options(AngelscriptStandaloneAddons PRIVATE /W4 /permissive- /EHsc)
	target_compile_options(AngelscriptStandaloneHost PRIVATE /W4 /permissive- /EHsc)
	target_compile_options(AngelscriptStandaloneCli PRIVATE /W4 /permissive- /EHsc)
endif()

add_executable(
	AngelscriptStandaloneSmokeTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneSmokeTests.cpp"
)
target_link_libraries(AngelscriptStandaloneSmokeTests PRIVATE AngelscriptMaintainedFork)

add_test(NAME AngelscriptStandalone.Smoke COMMAND AngelscriptStandaloneSmokeTests)
set_tests_properties(AngelscriptStandalone.Smoke PROPERTIES LABELS "standalone;native-core;smoke")

add_executable(
	AngelscriptSemanticObserverTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptSemanticObserverTests.cpp"
)
target_link_libraries(
	AngelscriptSemanticObserverTests
	PRIVATE
		AngelscriptStandaloneHost
)
add_test(
	NAME AngelscriptStandalone.SemanticObserver
	COMMAND AngelscriptSemanticObserverTests
)
set_tests_properties(
	AngelscriptStandalone.SemanticObserver
	PROPERTIES
		LABELS "standalone;native-core;semantic-observer"
)

add_executable(
	AngelscriptStandaloneFrontendTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneFrontendTests.cpp"
)
target_compile_features(AngelscriptStandaloneFrontendTests PRIVATE cxx_std_20)
target_link_libraries(AngelscriptStandaloneFrontendTests PRIVATE AngelscriptStandaloneHost)

add_test(NAME AngelscriptStandalone.Frontend COMMAND AngelscriptStandaloneFrontendTests)
set_tests_properties(
	AngelscriptStandalone.Frontend
	PROPERTIES
		LABELS "standalone;frontend;preprocessor"
)

add_executable(
	AngelscriptStandaloneAddonsTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneAddonsTests.cpp"
)
target_link_libraries(
	AngelscriptStandaloneAddonsTests
	PRIVATE
		AngelscriptStandaloneAddons
)

add_test(NAME AngelscriptStandalone.Addons COMMAND AngelscriptStandaloneAddonsTests)
set_tests_properties(AngelscriptStandalone.Addons PROPERTIES LABELS "standalone;native-core;addons;stdlib")

add_executable(
	AngelscriptStandaloneCliTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneCliTests.cpp"
)
target_link_libraries(
	AngelscriptStandaloneCliTests
	PRIVATE
		AngelscriptStandaloneHost
)

add_test(NAME AngelscriptStandalone.Cli COMMAND AngelscriptStandaloneCliTests)
set_tests_properties(AngelscriptStandalone.Cli PROPERTIES LABELS "standalone;native-core;cli;artifacts")

add_test(
	NAME AngelscriptStandalone.CliEndToEnd
	COMMAND
		"${CMAKE_COMMAND}"
		"-DAS_STANDALONE_EXE=$<TARGET_FILE:AngelscriptStandaloneCli>"
		"-DAS_STANDALONE_FIXTURE_ROOT=${CMAKE_CURRENT_LIST_DIR}/Tests/Fixtures/NativeCli"
		"-DAS_STANDALONE_TEST_ROOT=${CMAKE_BINARY_DIR}/cli-end-to-end"
		-P
		"${CMAKE_CURRENT_LIST_DIR}/Tests/CMake/RunCliEndToEnd.cmake"
)
set_tests_properties(
	AngelscriptStandalone.CliEndToEnd
	PROPERTIES
		LABELS "standalone;native-core;cli;artifacts;e2e"
		TIMEOUT 30
)

add_test(
	NAME AngelscriptStandalone.UECliEndToEnd
	COMMAND
		"${CMAKE_COMMAND}"
		"-DAS_STANDALONE_EXE=$<TARGET_FILE:AngelscriptStandaloneCli>"
		"-DAS_STANDALONE_FIXTURE_ROOT=${CMAKE_CURRENT_LIST_DIR}/Tests/Fixtures/OfflineContract/producer-v1"
		"-DAS_STANDALONE_TEST_ROOT=${CMAKE_BINARY_DIR}/ue-cli-end-to-end"
		-P
		"${CMAKE_CURRENT_LIST_DIR}/Tests/CMake/RunUECliEndToEnd.cmake"
)
set_tests_properties(
	AngelscriptStandalone.UECliEndToEnd
	PROPERTIES
		LABELS "standalone;ue-analysis;cli;artifacts;e2e"
		TIMEOUT 30
)

add_executable(
	AngelscriptStandaloneRuntimeTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneRuntimeTests.cpp"
)
target_link_libraries(
	AngelscriptStandaloneRuntimeTests
	PRIVATE
		AngelscriptStandaloneHost
)

add_test(NAME AngelscriptStandalone.Runtime COMMAND AngelscriptStandaloneRuntimeTests)
set_tests_properties(AngelscriptStandalone.Runtime PROPERTIES LABELS "standalone;native-core;runtime;limits")
set_tests_properties(AngelscriptStandalone.Runtime PROPERTIES TIMEOUT 30)

if(MSVC)
	add_executable(
		AngelscriptStandaloneAllocatorLifetimeTests
		"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneAllocatorLifetimeTests.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/Source/Runtime/AngelscriptStandaloneAllocator.cpp"
	)
	target_include_directories(
		AngelscriptStandaloneAllocatorLifetimeTests
		PRIVATE
			"${CMAKE_CURRENT_LIST_DIR}/Source"
	)
	target_link_libraries(
		AngelscriptStandaloneAllocatorLifetimeTests
		PRIVATE
			AngelscriptMaintainedFork
	)
	target_compile_options(
		AngelscriptStandaloneAllocatorLifetimeTests
		PRIVATE
			/fsanitize=address
	)
	target_compile_definitions(
		AngelscriptStandaloneAllocatorLifetimeTests
		PRIVATE
			_DISABLE_STRING_ANNOTATION
			_DISABLE_VECTOR_ANNOTATION
	)
	target_link_options(
		AngelscriptStandaloneAllocatorLifetimeTests
		PRIVATE
			/INCREMENTAL:NO
	)
	get_filename_component(ANGELSCRIPT_MSVC_TOOL_DIRECTORY "${CMAKE_LINKER}" DIRECTORY)
	find_file(
		ANGELSCRIPT_ASAN_RUNTIME
		NAMES clang_rt.asan_dynamic-x86_64.dll
		PATHS "${ANGELSCRIPT_MSVC_TOOL_DIRECTORY}"
		NO_DEFAULT_PATH
		REQUIRED
	)
	add_custom_command(
		TARGET AngelscriptStandaloneAllocatorLifetimeTests
		POST_BUILD
		COMMAND
			"${CMAKE_COMMAND}" -E copy_if_different
			"${ANGELSCRIPT_ASAN_RUNTIME}"
			"$<TARGET_FILE_DIR:AngelscriptStandaloneAllocatorLifetimeTests>"
		VERBATIM
	)
	add_test(
		NAME AngelscriptStandalone.AllocatorLifetime
		COMMAND AngelscriptStandaloneAllocatorLifetimeTests
	)
	set_tests_properties(
		AngelscriptStandalone.AllocatorLifetime
		PROPERTIES
			LABELS "standalone;native-core;runtime;memory-safety"
			TIMEOUT 30
	)
endif()

add_executable(
	AngelscriptOfflineContractTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptOfflineContractTests.cpp"
)
target_link_libraries(
	AngelscriptOfflineContractTests
	PRIVATE
		AngelscriptStandaloneHost
)
target_compile_definitions(
	AngelscriptOfflineContractTests
	PRIVATE
		ANGELSCRIPT_OFFLINE_FIXTURE_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Fixtures/OfflineContract/producer-v1"
)

add_test(NAME AngelscriptStandalone.OfflineContract COMMAND AngelscriptOfflineContractTests)
set_tests_properties(
	AngelscriptStandalone.OfflineContract
	PROPERTIES
		LABELS "standalone;offline-contract;bundle"
)

add_executable(
	AngelscriptStandaloneUEAnalysisTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneUEAnalysisTests.cpp"
)
target_link_libraries(
	AngelscriptStandaloneUEAnalysisTests
	PRIVATE
		AngelscriptStandaloneHost
)
target_compile_definitions(
	AngelscriptStandaloneUEAnalysisTests
	PRIVATE
		ANGELSCRIPT_OFFLINE_FIXTURE_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Fixtures/OfflineContract/producer-v1"
)

add_test(
	NAME AngelscriptStandalone.UEAnalysis
	COMMAND AngelscriptStandaloneUEAnalysisTests
)
set_tests_properties(
	AngelscriptStandalone.UEAnalysis
	PROPERTIES
		LABELS "standalone;ue-analysis;source-closure;registration"
)

add_executable(
	AngelscriptStandaloneAdaptersTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneAdaptersTests.cpp"
)
target_link_libraries(
	AngelscriptStandaloneAdaptersTests
	PRIVATE
		AngelscriptStandaloneHost
)
add_test(
	NAME AngelscriptStandalone.Adapters
	COMMAND AngelscriptStandaloneAdaptersTests
)
set_tests_properties(
	AngelscriptStandalone.Adapters
	PROPERTIES
		LABELS "standalone;ue-analysis;template-adapters"
)

add_executable(
	AngelscriptStandaloneResourcesTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneResourcesTests.cpp"
)
target_link_libraries(
	AngelscriptStandaloneResourcesTests
	PRIVATE
		AngelscriptStandaloneHost
)
add_test(
	NAME AngelscriptStandalone.Resources
	COMMAND AngelscriptStandaloneResourcesTests
)
set_tests_properties(
	AngelscriptStandalone.Resources
	PROPERTIES
		LABELS "standalone;ue-analysis;offline-resources"
)

add_executable(
	AngelscriptStandaloneArchitectureTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneArchitectureTests.cpp"
)
target_compile_features(AngelscriptStandaloneArchitectureTests PRIVATE cxx_std_20)
target_compile_definitions(
	AngelscriptStandaloneArchitectureTests
	PRIVATE
		ANGELSCRIPT_STANDALONE_SOURCE_ROOT="${CMAKE_CURRENT_LIST_DIR}/.."
		ANGELSCRIPT_STANDALONE_BUILD_ROOT="${CMAKE_BINARY_DIR}"
)

add_test(NAME AngelscriptStandalone.Architecture COMMAND AngelscriptStandaloneArchitectureTests)
set_tests_properties(AngelscriptStandalone.Architecture PROPERTIES LABELS "standalone;architecture")

add_executable(
	AngelscriptStandaloneCompatTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneCompatTests.cpp"
)
target_compile_features(AngelscriptStandaloneCompatTests PRIVATE cxx_std_20)
target_include_directories(
	AngelscriptStandaloneCompatTests
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/Compat"
)
target_link_libraries(
	AngelscriptStandaloneCompatTests
	PRIVATE
		AngelscriptMaintainedFork
)
add_test(NAME AngelscriptStandalone.Compat COMMAND AngelscriptStandaloneCompatTests)
set_tests_properties(AngelscriptStandalone.Compat PROPERTIES LABELS "standalone;compat;architecture")

install(TARGETS AngelscriptStandaloneCli RUNTIME DESTINATION .)
install(
	FILES
		"${CMAKE_CURRENT_LIST_DIR}/README.md"
		"${CMAKE_CURRENT_LIST_DIR}/SUPPORT_MATRIX.md"
	DESTINATION .
)
install(
	DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Examples/"
	DESTINATION Examples
)
install(
	DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/Schemas/"
	DESTINATION Schemas
)
install(
	DIRECTORY "${ANGELSCRIPT_PACKAGED_DEFAULT_BUNDLE}/"
	DESTINATION contracts/default-engine
)
install(
	FILES "${CMAKE_CURRENT_LIST_DIR}/../LICENSE.md"
	DESTINATION LICENSES
	RENAME UnrealAngelscript.txt
)
install(
	FILES "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RapidJSON/LICENSE.txt"
	DESTINATION LICENSES
	RENAME RapidJSON.txt
)
install(
	FILES "${CMAKE_CURRENT_LIST_DIR}/ThirdParty/README.md"
	DESTINATION LICENSES
	RENAME ThirdParty-Provenance.md
)

set(
	ANGELSCRIPT_STANDALONE_PACKAGE_ROOT
	"${CMAKE_BINARY_DIR}/package/$<CONFIG>/as-standalone-win64"
)
add_custom_target(
	AngelscriptStandalonePackage
	COMMAND
		"${CMAKE_COMMAND}"
		"-DAS_STANDALONE_BINARY_DIR=${CMAKE_BINARY_DIR}"
		"-DAS_STANDALONE_CONFIGURATION=$<CONFIG>"
		"-DPACKAGE_ROOT=${ANGELSCRIPT_STANDALONE_PACKAGE_ROOT}"
		"-DPACKAGE_MANIFEST_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/Tools/GeneratePackageManifest.cmake"
		"-DPACKAGE_ARCHIVE=${CMAKE_BINARY_DIR}/package/$<CONFIG>/as-standalone-win64.zip"
		-P "${CMAKE_CURRENT_LIST_DIR}/Tools/AssemblePackage.cmake"
	DEPENDS AngelscriptStandaloneCli
	VERBATIM
)

add_test(
	NAME AngelscriptStandalone.Package
	COMMAND
		"${CMAKE_COMMAND}"
		"-DAS_STANDALONE_BINARY_DIR=${CMAKE_BINARY_DIR}"
		"-DAS_STANDALONE_CONFIGURATION=$<CONFIG>"
		"-DAS_STANDALONE_TEST_ROOT=${CMAKE_BINARY_DIR}/package-inspection"
		"-DAS_STANDALONE_MANIFEST_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/Tools/GeneratePackageManifest.cmake"
		-P
		"${CMAKE_CURRENT_LIST_DIR}/Tests/CMake/RunPackageInspection.cmake"
)
set_tests_properties(
	AngelscriptStandalone.Package
	PROPERTIES
		LABELS "standalone;package;release;architecture;e2e"
		TIMEOUT 90
)

add_executable(
	AngelscriptStandaloneCorpusTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneCorpusTests.cpp"
)
target_include_directories(
	AngelscriptStandaloneCorpusTests
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/ThirdParty/RapidJSON/include"
)
target_compile_definitions(
	AngelscriptStandaloneCorpusTests
	PRIVATE
		ANGELSCRIPT_STANDALONE_CORPUS_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Corpus"
		ANGELSCRIPT_STANDALONE_REPOSITORY_ROOT="${CMAKE_CURRENT_LIST_DIR}/../../.."
		ANGELSCRIPT_OFFLINE_FIXTURE_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Fixtures/OfflineContract/producer-v1"
)
target_link_libraries(
	AngelscriptStandaloneCorpusTests
	PRIVATE
		AngelscriptStandaloneHost
)
add_test(
	NAME AngelscriptStandalone.Corpus
	COMMAND AngelscriptStandaloneCorpusTests
)
set_tests_properties(
	AngelscriptStandalone.Corpus
	PROPERTIES
		LABELS "standalone;corpus;differential;release;native-core;ue-analysis"
		TIMEOUT 30
)

add_executable(
	AngelscriptStandaloneSoakTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneSoakTests.cpp"
)
target_compile_definitions(
	AngelscriptStandaloneSoakTests
	PRIVATE
		ANGELSCRIPT_STANDALONE_CORPUS_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Corpus"
)
target_link_libraries(
	AngelscriptStandaloneSoakTests
	PRIVATE
		AngelscriptStandaloneHost
)
add_test(
	NAME AngelscriptStandalone.Soak
	COMMAND AngelscriptStandaloneSoakTests
)
set_tests_properties(
	AngelscriptStandalone.Soak
	PROPERTIES
		LABELS "standalone;soak;determinism;limits;release"
		TIMEOUT 120
)

add_executable(
	AngelscriptStandaloneBenchmarkTests
	"${CMAKE_CURRENT_LIST_DIR}/Tests/AngelscriptStandaloneBenchmarkTests.cpp"
)
target_compile_definitions(
	AngelscriptStandaloneBenchmarkTests
	PRIVATE
		ANGELSCRIPT_STANDALONE_CORPUS_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Corpus"
		ANGELSCRIPT_OFFLINE_FIXTURE_ROOT="${CMAKE_CURRENT_LIST_DIR}/Tests/Fixtures/OfflineContract/producer-v1"
		ANGELSCRIPT_STANDALONE_GIT_COMMIT="${ANGELSCRIPT_STANDALONE_GIT_COMMIT}"
)
target_link_libraries(
	AngelscriptStandaloneBenchmarkTests
	PRIVATE
		AngelscriptStandaloneHost
)
add_test(
	NAME AngelscriptStandalone.Benchmarks
	COMMAND
		AngelscriptStandaloneBenchmarkTests
		--output "${CMAKE_BINARY_DIR}/benchmarks/$<CONFIG>"
)
set_tests_properties(
	AngelscriptStandalone.Benchmarks
	PROPERTIES
		LABELS "standalone;benchmarks;performance;release"
		TIMEOUT 120
)

# The maintained UE-spelled SDK header includes CoreMinimal.h. Standalone
# consumers that compile against that header select Compat explicitly and
# privately; AngelscriptMaintainedFork must not propagate the include tree.
set(ANGELSCRIPT_STANDALONE_COMPAT_CONSUMERS
	AngelscriptStandaloneAddons
	AngelscriptStandaloneHost
	AngelscriptStandaloneCli
	AngelscriptStandaloneSmokeTests
	AngelscriptSemanticObserverTests
	AngelscriptStandaloneAddonsTests
	AngelscriptStandaloneCliTests
	AngelscriptStandaloneRuntimeTests
	AngelscriptStandaloneAllocatorLifetimeTests
	AngelscriptOfflineContractTests
	AngelscriptStandaloneUEAnalysisTests
	AngelscriptStandaloneAdaptersTests
	AngelscriptStandaloneResourcesTests
	AngelscriptStandaloneCompatTests
	AngelscriptStandaloneCorpusTests
	AngelscriptStandaloneSoakTests
	AngelscriptStandaloneBenchmarkTests
)
foreach(compat_consumer IN LISTS ANGELSCRIPT_STANDALONE_COMPAT_CONSUMERS)
	target_include_directories(
		${compat_consumer}
		BEFORE PRIVATE
			"${ANGELSCRIPT_STANDALONE_COMPAT}"
	)
endforeach()
