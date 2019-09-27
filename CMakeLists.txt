#
# Copyright (c) 2019 Chris Burns <chris@kitty.city>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 3.1)
project(svlynx LANGUAGES C)
enable_testing()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SVL_OUTPUT_NAME ${CMAKE_SHARED_LIBRARY_PREFIX}svlynxDSO${CMAKE_SHARED_LIBRARY_SUFFIX})
set(SVL_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(SVL_OUTPUT_FILE ${SVL_OUTPUT_DIR}/${SVL_OUTPUT_NAME} CACHE FILEPATH "Output path of module library (DSO)")
set(SVL_DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(SVL_LINK_LIBS "")
set(SVL_DEFINES "")

if(NOT EXISTS ${SVL_DEPS_DIR})
    file(MAKE_DIRECTORY ${SVL_DEPS_DIR})
endif()

if(MSVC)
    set(CompilerFlags
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
    )

    foreach(CompilerFlag ${CompilerFlags})
      string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

    list(APPEND SVL_DEFINES -D_CRT_SECURE_NO_WARNINGS)
endif()

#
# build socklynx
set(SL_SKIP_POSTBUILD true CACHE BOOL "")
mark_as_advanced(FORCE SL_SKIP_POSTBUILD)
if(NOT EXISTS ${SVL_DEPS_DIR}/socklynx)
    execute_process(COMMAND git clone https://github.com/c6burns/socklynx WORKING_DIRECTORY ${SVL_DEPS_DIR})
endif()

add_custom_target(socklynx_update
    COMMAND git checkout master
    COMMAND git pull
    WORKING_DIRECTORY ${SVL_DEPS_DIR}/socklynx
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/socklynx/include)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/socklynx)
add_dependencies(socklynx socklynx_update)

#
# build tn
if(NOT EXISTS ${SVL_DEPS_DIR}/tn)
    execute_process(COMMAND git clone https://github.com/c6burns/tn WORKING_DIRECTORY ${SVL_DEPS_DIR})
endif()

add_custom_target(tn_update
    COMMAND git checkout master
    COMMAND git pull
    WORKING_DIRECTORY ${SVL_DEPS_DIR}/tn
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/tn/include)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/tn)
include_directories(${TN_INCLUDE_DIRS})
add_dependencies(tn tn_update)
list(APPEND SVL_LINK_LIBS tn)

#
# build svlynx
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/svlynx)
add_library(svlynx
    include/svlynx/svlynx.h
    src/svlynx/service.c
    include/svlynx/service.h
    src/svlynx/service_internal.c
    src/svlynx/service_internal.h
    src/svlynx/link.c
    include/svlynx/link.h
)
target_link_libraries(svlynx ${SVL_LINK_LIBS})
target_compile_definitions(svlynx PRIVATE ${SVL_DEFINES})
add_dependencies(svlynx tn)
list(APPEND SVL_LINK_LIBS svlynx)

#
# build svlynxDSO
add_library(svlynxDSO MODULE
    src/svlynx/plugin.c
)
target_link_libraries(svlynxDSO ${SVL_LINK_LIBS})
target_compile_definitions(svlynxDSO PRIVATE ${SVL_DEFINES})
add_dependencies(svlynxDSO svlynx)

#
# build svlynx_sv
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/svlynx_sv)
add_executable(svlynx_sv
    src/svlynx_sv/svlynx_sv.c
    include/svlynx_sv/svlynx_sv.h
)
target_link_libraries(svlynx_sv ${SVL_LINK_LIBS})
target_compile_definitions(svlynx_sv PRIVATE ${SVL_DEFINES})
add_dependencies(svlynx_sv svlynx)

#
# build svlynx_lg
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/svlynx_lg)
add_executable(svlynx_lg
    src/svlynx_lg/svlynx_lg.c
    include/svlynx_lg/svlynx_lg.h
)
target_link_libraries(svlynx_lg ${SVL_LINK_LIBS})
target_compile_definitions(svlynx_lg PRIVATE ${SVL_DEFINES})
add_dependencies(svlynx_lg svlynx)

#
# copy outputs
add_custom_command(
    TARGET svlynxDSO
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:svlynxDSO>
        ${SVL_OUTPUT_FILE}
)

if(APPLE)
    set_target_properties(svlynxDSO PROPERTIES BUNDLE TRUE)
    string(REPLACE ".bundle" ".dylib" SVL_OUTPUT_FILE_BUNDLE ${SVL_OUTPUT_FILE})

    # copy dylib for .net core
    add_custom_command(
        TARGET svlynxDSO
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:svlynxDSO>
            ${SVL_OUTPUT_FILE_BUNDLE}
    )
endif()

#
# build svlynx test executables
set(SVL_TESTS_C
    "tests/test_service.c"
    "tests/test_plugin.c"
)
set(TESTS ${SVL_TESTS_C})

tn_add_test_case(svl_service_setup_cleanup)
tn_add_test_case(svl_service_setup_cleanup_twice)
tn_add_test_case(svl_service_start_stop)
tn_add_test_case(svl_service_start_stop_twice)
tn_add_test_case(svl_service_start_stop_immediately)
tn_add_test_case(svl_service_start_stop_immediately_twice)
tn_add_test_case(svl_service_listen_acquire)
tn_add_test_case(svl_plugin_setup_cleanup)
tn_add_test_case(svl_plugin_setup_cleanup_twice)
tn_add_test_case(svl_plugin_double_setup_cleanup)
tn_add_test_case(svl_plugin_setup_double_cleanup)
tn_add_test_case(svl_plugin_double_setup_double_cleanup)
tn_add_test_case(svl_plugin_double_setup_double_cleanup_twice)
tn_add_test_case(svl_plugin_start_stop)
tn_add_test_case(svl_plugin_start_stop_twice)
tn_add_test_case(svl_plugin_double_start_stop)
tn_add_test_case(svl_plugin_start_double_stop)
tn_add_test_case(svl_plugin_double_start_double_stop)
tn_add_test_case(svl_plugin_double_start_double_stop_twice)
tn_add_test_case(svl_plugin_start_stop_immediately)
tn_add_test_case(svl_plugin_start_stop_immediately_twice)

tn_generate_test_driver(svl-tests svl)
target_link_libraries(svl-tests ${SVL_LINK_LIBS})
target_compile_definitions(svl-tests PRIVATE ${SVL_DEFINES})

# runner for single test for easy debug step through an impl issue
#tn_test_single(test_service svl_service_listen_acquire "${SVL_LINK_LIBS}" "${SVL_DEFINES}")
