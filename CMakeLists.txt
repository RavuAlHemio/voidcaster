cmake_minimum_required(VERSION 2.8)

# C++ support is necessary for linking with libclang
project(voidcaster C CXX)

# FindLibClang is not part of the CMake distribution
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# find the GCC system include directory
include(${CMAKE_SOURCE_DIR}/cmake/FindGccSysIncludeDir.cmake)

# for generating version.h
find_package(Git)

# to find version.h
include_directories(${CMAKE_BINARY_DIR})

# The Voidcaster itself
add_executable(voidcaster
	interact.c
	msa.c
	treemunger.c
	voidcaster.c
)

# compile and link it against libclang
find_package(LibClang)
include_directories(${LIBCLANG_INCLUDE_DIRS})
target_link_libraries(voidcaster ${LIBCLANG_LIBRARIES})

# define the GCC system include directory
if(GCC_SYS_INCLUDE_DIR)
set_property(TARGET voidcaster APPEND PROPERTY COMPILE_DEFINITIONS GCC_SYSINCLUDE="${GCC_SYS_INCLUDE_DIR}")
endif(GCC_SYS_INCLUDE_DIR)

# generate version.h
add_custom_target(
	version
	${CMAKE_COMMAND} -D SRC="${CMAKE_SOURCE_DIR}/version.h.in"
	                 -D DST="${CMAKE_BINARY_DIR}/version.h"
	                 -D GIT_EXECUTABLE="${GIT_EXECUTABLE}"
	                 -P ${CMAKE_SOURCE_DIR}/cmake/version.cmake
)
add_dependencies(voidcaster version)
