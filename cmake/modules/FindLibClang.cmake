# - Try to find libclang
# Once done this will define
#  LIBCLANG_FOUND - System has libclang
#  LIBCLANG_INCLUDE_DIRS - The libclang include directories
#  LIBCLANG_LIBRARIES - The libraries needed to use libclang
#  LIBCLANG_DEFINITIONS - Compiler switches required for using libclang

# take a hint
set(_llvm_lib_dir)
find_program(LLVM_CONFIG NAMES llvm-config)
if(LLVM_CONFIG)
	execute_process(
		COMMAND ${LLVM_CONFIG} --libdir
		OUTPUT_VARIABLE _llvm_lib_dir
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${LLVM_CONFIG} --includedir
		OUTPUT_VARIABLE _llvm_include_dir
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif(LLVM_CONFIG)

# find the headers
find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h HINTS ${_llvm_include_dir})

# find the library
find_library(LIBCLANG_LIBRARY NAMES clang libclang liblibclang HINTS ${_llvm_lib_dir})

# set to fulfill find_package expectations
set(LIBCLANG_INCLUDE_DIRS ${LIBCLANG_INCLUDE_DIR})
set(LIBCLANG_LIBRARIES ${LIBCLANG_LIBRARY})

# handle all the other expectations
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibClang DEFAULT_MSG
	LIBCLANG_LIBRARY LIBCLANG_INCLUDE_DIR)

# keep these out of sight
mark_as_advanced(LIBCLANG_LIBRARY LIBCLANG_INCLUDE_DIR)
