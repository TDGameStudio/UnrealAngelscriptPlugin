if(NOT DEFINED PACKAGE_ROOT)
	message(FATAL_ERROR "PACKAGE_ROOT is required")
endif()

cmake_path(ABSOLUTE_PATH PACKAGE_ROOT NORMALIZE OUTPUT_VARIABLE PACKAGE_ROOT)
if(NOT IS_DIRECTORY "${PACKAGE_ROOT}")
	message(FATAL_ERROR "package root does not exist: ${PACKAGE_ROOT}")
endif()

file(
	GLOB_RECURSE PACKAGE_FILES
	LIST_DIRECTORIES false
	RELATIVE "${PACKAGE_ROOT}"
	"${PACKAGE_ROOT}/*"
)
list(FILTER PACKAGE_FILES EXCLUDE REGEX "^package-manifest\\.json$")
list(SORT PACKAGE_FILES)

set(MANIFEST "{\"files\":[")
set(FIRST_FILE TRUE)
foreach(RELATIVE_PATH IN LISTS PACKAGE_FILES)
	set(ABSOLUTE_PATH "${PACKAGE_ROOT}/${RELATIVE_PATH}")
	file(SHA256 "${ABSOLUTE_PATH}" FILE_HASH)
	file(SIZE "${ABSOLUTE_PATH}" FILE_SIZE)
	string(REPLACE "\\" "/" NORMALIZED_PATH "${RELATIVE_PATH}")
	if(FIRST_FILE)
		set(FIRST_FILE FALSE)
	else()
		string(APPEND MANIFEST ",")
	endif()
	string(APPEND MANIFEST
		"{\"path\":\"${NORMALIZED_PATH}\",\"sha256\":\"${FILE_HASH}\",\"size\":${FILE_SIZE}}")
endforeach()
list(LENGTH PACKAGE_FILES FILE_COUNT)
string(APPEND MANIFEST
	"],\"format\":\"angelscript-standalone-package/1.0\",\"fileCount\":${FILE_COUNT}}\n")
file(WRITE "${PACKAGE_ROOT}/package-manifest.json" "${MANIFEST}")
