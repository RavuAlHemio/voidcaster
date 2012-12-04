execute_process(
	COMMAND ${GIT_EXECUTABLE} log -1 HEAD --pretty=format:"revision %h from %ai"
	OUTPUT_VARIABLE GIT_REVINFO
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(${SRC} ${DST} @ONLY)
