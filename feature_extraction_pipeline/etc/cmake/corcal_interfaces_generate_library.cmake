# This is a modified version of armarx_interfaces_generate_library
# since the ArmarX provided macro is not flexible enough
macro(corcal_interfaces_generate_library NAME)
    # interface library version
    set(LIB_NAME       ${NAME})
    armarx_set_target("Interfaces: ${LIB_NAME}")

    # used in Installation.cmake
    set(ARMARX_PROJECT_INTERFACE_LIBRARY "${LIB_NAME}" CACHE INTERNAL "" FORCE)

    set(DEPENDEND_PROJECT_NAMES "${ARGV1}")
    set(ADDITIONAL_DEPENDEND_PROJECT_NAMES "${ARGV2}")
    set(PROJECT_INTERFACE_DEPENDENCIES "${DEPENDEND_PROJECT_NAMES}" CACHE INTERNAL "" FORCE)

    armarx_interfaces_generate_library_collect_dependencies(${DEPENDEND_PROJECT_NAMES})
    list(REMOVE_DUPLICATES DEPENDEND_PROJECT_NAMES)

    list(LENGTH DEPENDEND_PROJECT_NAMES PRINT_DEPENDENCIES)
    if (PRINT_DEPENDENCIES)
        message(STATUS "        Dependencies:")
        printlist("            " "${DEPENDEND_PROJECT_NAMES}")
    endif()

    # project vars
    set(SLICE_COMMON_FLAGS
        "-I${PROJECT_SOURCECODE_DIR}"
        "-I${Ice_Slice_DIR}"
        #--stream
        --underscore)

    set(SLICE2CPP_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(SLICE2CPP_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    # the uppercase string is needed for the --dll-export slice flag
    string(TOUPPER ${LIB_NAME} LIB_NAME_UPPERCASE)
    string(REPLACE "-" "_" LIB_NAME_UPPERCASE "${LIB_NAME_UPPERCASE}")
    set(SLICE2CPP_FLAGS --dll-export "${LIB_NAME_UPPERCASE}_IMPORT_EXPORT")

    set(SLICE2JAVA_DEPEND_INTERFACES "")
    set(SLICE2JAVA_DEPEND_JARS "")

    set(SLICE_INCLUDE_FLAGS "")

    # project interface dependency paths
    # go through all dependent projects
    foreach(DEPENDEND_PROJECT_NAME ${DEPENDEND_PROJECT_NAMES})
        # append all directories found in <DEPENDEND_PROJECT_NAME>__INTERFACE_DIRS
        # as include directories for the current compilation process
        foreach(DEPENDEND_INTERFACE_DIR ${${DEPENDEND_PROJECT_NAME}_INTERFACE_DIRS})
            list(APPEND SLICE_INCLUDE_FLAGS "-I${DEPENDEND_INTERFACE_DIR}")
            set(SLICE2JAVA_DEPEND_INTERFACES "${SLICE2JAVA_DEPEND_INTERFACES}:${DEPENDEND_INTERFACE_DIR}")
        endforeach(DEPENDEND_INTERFACE_DIR)

        include_directories(${${DEPENDEND_PROJECT_NAME}_INCLUDE_DIRS})

        # append all dependent interface libraries to the DEPENDEND_INTERFACE_LIBRARIES
        # CMake variable which is used further down in target_link_libraries()
        list(APPEND DEPENDEND_INTERFACE_LIBRARIES "${${DEPENDEND_PROJECT_NAME}_INTERFACE_LIBRARY}")

        # build a list of all dependent java archives
        # needed by the slice2java compiler
        foreach(DEPENDEND_JAR ${${DEPENDEND_PROJECT_NAME}_JARS})
            set(SLICE2JAVA_DEPEND_JARS "${${DEPENDEND_PROJECT_NAME}_JAR_DIR}/${DEPENDEND_JAR}:${SLICE2JAVA_DEPEND_JARS}")
        endforeach(DEPENDEND_JAR)
    endforeach(DEPENDEND_PROJECT_NAME)

    list(APPEND SLICE_COMMON_FLAGS ${SLICE_INCLUDE_FLAGS})

    # setup C++ files in the following loop
    set(LIB_FILES "")
    set(LIB_HEADER_FILES "")

    # run slice on files
    foreach(CURRENT_SLICE_FILE ${SLICE_FILES})
        set(STRIPPED_SLICE_FILE "")
        stripIceSuffix(${CURRENT_SLICE_FILE} STRIPPED_SLICE_FILE)

        # File is usually from the scheme "some/dir/${CURRENT_SLICE_FILE}"
        # this REGEX matches against ${STRIPPED_SLICE_FILE} and
        # CMAKE_MATCH_1 contains "some/dir" afterwards
        string(REGEX MATCH "^(.+)/[^/]+$" VOID ${STRIPPED_SLICE_FILE})
        set(DIR ${CMAKE_MATCH_1})

        # put generated files in a directory with the same hierarchy and names as the .ice file
        set(SLICE_CURRENT_CPP_FLAGS ${SLICE_COMMON_FLAGS} --output-dir ${SLICE2CPP_OUTPUT_DIR}/${DIR})

        # Always add a prefix to the #include directives of generated files
        # Since only #include <> directives are generated the local directory is not
        # regarded when searching for matching header files which would require
        # adding additional include_directories.
        # $DIR is empty if REGEX couldn't match anything in ${STRIPPED_SLICE_FILE}
        # e.g. the file was not found in a subdirectory of CMAKE_CURRENT_SOURCE_DIR.
        string(LENGTH "${PROJECT_SOURCE_DIR}/source/" PROJECT_SOURCE_DIR_LEN)
        string(SUBSTRING "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR_LEN}" "-1" SLICE_FILE_INCLUDE_DIR)
        #set(SLICE_FILE_INCLUDE_DIR "")
        if ("${DIR}" STREQUAL "")
            # omit double // in generated files if $DIR is empty
            list(APPEND SLICE_CURRENT_CPP_FLAGS --include-dir "${SLICE_FILE_INCLUDE_DIR}")
        else()
            list(APPEND SLICE_CURRENT_CPP_FLAGS --include-dir "${SLICE_FILE_INCLUDE_DIR}/${DIR}")
        endif()

        # Get the dependencies of the current slice file
        # required for the slice2cpp target
        set(SLICE_CPP_DEPENDS "")
        set(SLICE_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        sliceDependencies("${SLICE_CURRENT_CPP_FLAGS}" "${SLICE_INPUT_DIR}/${CURRENT_SLICE_FILE}" SLICE_CPP_DEPENDS)
#        message(FATAL_ERROR  "slices: ${SLICE_CPP_DEPENDS}")
        # make sure the directory for generated files exists
        file(MAKE_DIRECTORY ${SLICE2CPP_OUTPUT_DIR}/${DIR})

        # generate the header and implementation files from the slice file
        add_custom_command(OUTPUT  ${SLICE2CPP_OUTPUT_DIR}/${STRIPPED_SLICE_FILE}.h
                                   ${SLICE2CPP_OUTPUT_DIR}/${STRIPPED_SLICE_FILE}.cpp
                           COMMAND ${Ice_slice2cpp}
                           ARGS    ${SLICE_CURRENT_CPP_FLAGS} ${SLICE2CPP_FLAGS} "${SLICE_INPUT_DIR}/${CURRENT_SLICE_FILE}"
                           DEPENDS ${SLICE_CPP_DEPENDS}
                           MAIN_DEPENDENCY "${SLICE_INPUT_DIR}/${CURRENT_SLICE_FILE}"
                           COMMENT "-- Generating cpp file from ${ARMARX_PROJECT_NAME}/interfaces/${CURRENT_SLICE_FILE}")

        list(APPEND LIB_FILES        ${SLICE2CPP_OUTPUT_DIR}/${STRIPPED_SLICE_FILE}.cpp)
        list(APPEND LIB_HEADER_FILES ${SLICE2CPP_OUTPUT_DIR}/${STRIPPED_SLICE_FILE}.h)
    endforeach()

    if(SLICE_FILES_ADDITIONAL_HEADERS)
        list(APPEND LIB_HEADER_FILES ${SLICE_FILES_ADDITIONAL_HEADERS})
    endif()

    if(SLICE_FILES_ADDITIONAL_SOURCES)
        list(APPEND LIB_FILES        ${SLICE_FILES_ADDITIONAL_SOURCES})
    endif()

    # custom target that is reached once all files have been generated
    add_custom_target("slice2cpp_${NAME}_FINISHED" ALL
        DEPENDS ${LIB_FILES}
    )

    # BUILD_INTERFACE_LIBRARY evaluates to FALSE if length is 0
    list(LENGTH LIB_FILES BUILD_INTERFACE_LIBRARY)

    list(APPEND DEPENDEND_INTERFACE_LIBRARIES ${ADDITIONAL_DEPENDEND_PROJECT_NAMES})

    # only build the library if all files have been generated
    armarx_build_if(BUILD_INTERFACE_LIBRARY "NOT building interface library <${LIB_NAME}> because no Slice files could be found in: ${CMAKE_CURRENT_SOURCE_DIR}")
    if(${BUILD_INTERFACE_LIBRARY})
        list(APPEND DEPENDEND_INTERFACE_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
        message(STATUS "###### ${LIB_NAME} -- ${DEPENDEND_INTERFACE_LIBRARIES}")
        armarx_add_library("${LIB_NAME}"  "${LIB_FILES}" "${LIB_HEADER_FILES}" "${DEPENDEND_INTERFACE_LIBRARIES}")
        if(COMPILER_SUPPORTS_suggest_override)
            target_compile_options(${LIB_NAME} PRIVATE -Wno-suggest-override)
        endif()
        add_dependencies("slice2cpp_${NAME}_FINISHED" ${LIB_NAME})
    endif()

    set(SLICE2HTML_INPUT_FILES "")
    foreach(CURRENT_SLICE_FILE ${SLICE_FILES})
        list(APPEND SLICE2HTML_INPUT_FILES "${SLICE_INPUT_DIR}/${CURRENT_SLICE_FILE}")
    endforeach()
    #generateSliceDocumentation("${SLICE2HTML_INPUT_FILES}" "${SLICE_INCLUDE_FLAGS}")

    #generateJavaInterface()
endmacro()
