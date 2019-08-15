cmake_minimum_required(VERSION 3.1)
project(svlynx LANGUAGES C)
enable_testing()

set(SVL_DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(SVL_LINK_LIBS "")

if(NOT EXISTS ${SVL_DEPS_DIR})
	file(MAKE_DIRECTORY ${SVL_DEPS_DIR})
endif()

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
add_dependencies(svlynx tn)
list(APPEND SVL_LINK_LIBS svlynx)

#
# build svlynx_sv
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/svlynx_sv)
add_executable(svlynx_sv
	src/svlynx_sv/svlynx_sv.c
	include/svlynx_sv/svlynx_sv.h
)
target_link_libraries(svlynx_sv ${SVL_LINK_LIBS})
add_dependencies(svlynx_sv svlynx)

#
# build svlynx_lg
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/svlynx_lg)
add_executable(svlynx_lg
	src/svlynx_lg/svlynx_lg.c
	include/svlynx_lg/svlynx_lg.h
)
target_link_libraries(svlynx_lg ${SVL_LINK_LIBS})
add_dependencies(svlynx_lg svlynx)
