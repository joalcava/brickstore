# based on Qt's QtSeparateDebugInfo.cmake

include(CMakeFindBinUtils)

if(CMAKE_VERSION VERSION_LESS 3.17.0)
set(CMAKE_CURRENT_FUNCTION_LIST_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

# Enable separate debug information for the given target
function(enable_separate_debug_info target) # installDestination)
    set(flags QT_EXECUTABLE)
    set(options)
    set(multiopts ADDITIONAL_INSTALL_ARGS)
    cmake_parse_arguments(arg "${flags}" "${options}" "${multiopts}" ${ARGN})

#    if (NOT QT_FEATURE_separate_debug_info)
#        return()
#    endif()
    if (NOT UNIX AND NOT MINGW)
        return()
    endif()
    get_target_property(target_type ${target} TYPE)
    if (NOT target_type STREQUAL "MODULE_LIBRARY" AND
        NOT target_type STREQUAL "SHARED_LIBRARY" AND
        NOT target_type STREQUAL "EXECUTABLE")
        return()
    endif()
    get_property(target_source_dir TARGET ${target} PROPERTY SOURCE_DIR)
    get_property(skip_separate_debug_info DIRECTORY "${target_source_dir}" PROPERTY _qt_skip_separate_debug_info)
    if (skip_separate_debug_info)
        return()
    endif()

    unset(commands)
    if(APPLE)
        find_program(DSYMUTIL_PROGRAM dsymutil)
        set(copy_bin ${DSYMUTIL_PROGRAM})
        set(strip_bin ${CMAKE_STRIP})
        set(debug_info_suffix dSYM)
        set(copy_bin_out_arg --flat -o)
        set(strip_args -S)
    else()
        set(copy_bin ${CMAKE_OBJCOPY})
        set(strip_bin ${CMAKE_OBJCOPY})
        if(QNX)
            set(debug_info_suffix sym)
            set(debug_info_keep --keep-file-symbols)
            set(strip_args "--strip-debug -R.ident")
        else()
            set(debug_info_suffix debug)
            set(debug_info_keep --only-keep-debug)
            set(strip_args --strip-debug)
        endif()
    endif()
    if(APPLE)
        get_target_property(is_framework ${target} FRAMEWORK)
        if(is_framework)
            qt_internal_get_framework_info(fw ${target})
            set(debug_info_bundle_dir "$<TARGET_BUNDLE_DIR:${target}>.${debug_info_suffix}")
            set(BUNDLE_ID ${fw_name})
        else()
            set(debug_info_bundle_dir "$<TARGET_BUNDLE_DIR:${target}>.${debug_info_suffix}")
            set(BUNDLE_ID ${target})
        endif()
        set(debug_info_contents_dir "${debug_info_bundle_dir}/Contents")
        set(debug_info_target_dir "${debug_info_contents_dir}/Resources/DWARF")
        configure_file(
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SeparateDebugInfo.Info.plist.in"
            "Info.dSYM.plist"
            )
        list(APPEND commands
            COMMAND ${CMAKE_COMMAND} -E make_directory ${debug_info_target_dir}
            COMMAND ${CMAKE_COMMAND} -E copy "Info.dSYM.plist" "${debug_info_contents_dir}/Info.plist"
            )
        set(debug_info_target "${debug_info_target_dir}/$<TARGET_FILE_BASE_NAME:${target}>")

#        if(arg_QT_EXECUTABLE AND QT_FEATURE_debug_and_release)
#            qt_get_cmake_configurations(cmake_configs)
#            foreach(cmake_config ${cmake_configs})
#                # Make installation optional for targets that are not built by default in this config
#                if(NOT (cmake_config STREQUAL QT_MULTI_CONFIG_FIRST_CONFIG))
#                    set(install_optional_arg OPTIONAL)
#                  else()
#                    unset(install_optional_arg)
#                endif()
#                install(DIRECTORY ${debug_info_bundle_dir}
#                           ${arg_ADDITIONAL_INSTALL_ARGS}
#                           ${install_optional_arg}
#                           CONFIGURATIONS ${cmake_config}
#                           DESTINATION ${installDestination})
#            endforeach()
#        else()
#            install(DIRECTORY ${debug_info_bundle_dir}
#                       ${arg_ADDITIONAL_INSTALL_ARGS}
#                       DESTINATION ${installDestination})
#        endif()
    else()
        set(debug_info_target "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.${debug_info_suffix}")
#        install(FILES ${debug_info_target} DESTINATION ${installDestination})
    endif()
    list(APPEND commands
        COMMAND ${copy_bin} ${debug_info_keep} $<TARGET_FILE:${target}>
                ${copy_bin_out_arg} ${debug_info_target}
        COMMAND ${strip_bin} ${strip_args} $<TARGET_FILE:${target}>
        )
    if(NOT APPLE)
        list(APPEND commands
            COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=${debug_info_target} $<TARGET_FILE:${target}>
            )
    endif()
    if(NOT CMAKE_HOST_WIN32)
        list(APPEND commands
            COMMAND chmod -x ${debug_info_target}
            )
    endif()
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        ${commands}
        )
endfunction()
