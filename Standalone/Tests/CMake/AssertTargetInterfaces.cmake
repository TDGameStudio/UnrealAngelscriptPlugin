function(angelscript_assert_maintained_fork_interface target_name compat_include)
	get_target_property(interface_definitions ${target_name} INTERFACE_COMPILE_DEFINITIONS)
	if(NOT interface_definitions)
		set(interface_definitions "")
	endif()

	set(private_definitions
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
	foreach(private_definition IN LISTS private_definitions)
		if(private_definition IN_LIST interface_definitions)
			message(FATAL_ERROR
				"${target_name} exposes fork-private compile definition "
				"'${private_definition}' through INTERFACE_COMPILE_DEFINITIONS")
		endif()
	endforeach()

	get_target_property(interface_includes ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
	if(NOT interface_includes)
		set(interface_includes "")
	endif()
	file(TO_CMAKE_PATH "${compat_include}" normalized_compat_include)
	foreach(interface_include IN LISTS interface_includes)
		file(TO_CMAKE_PATH "${interface_include}" normalized_interface_include)
		if(normalized_interface_include STREQUAL normalized_compat_include)
			message(FATAL_ERROR
				"${target_name} exposes Standalone/Compat through "
				"INTERFACE_INCLUDE_DIRECTORIES")
		endif()
	endforeach()
endfunction()

angelscript_assert_maintained_fork_interface(
	AngelscriptMaintainedFork
	"${ANGELSCRIPT_STANDALONE_COMPAT}"
)
