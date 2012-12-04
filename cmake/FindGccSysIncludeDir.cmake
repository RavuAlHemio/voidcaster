# - Finds the GCC system include directory.
# Once done this will define
#  GCC - Path to GCC
#  GCC_SYS_INCLUDE_DIR - GCC system include path
find_program(GCC NAMES gcc)
if(GCC)
	execute_process(
		COMMAND ${GCC} -print-file-name=include
		OUTPUT_VARIABLE GCC_SYS_INCLUDE_DIR
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
endif(GCC)
