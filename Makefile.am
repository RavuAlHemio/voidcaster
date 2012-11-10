bin_PROGRAMS = voidcaster

voidcaster_SOURCES = interact.c msa.c treemunger.c voidcaster.c

# Force linking with C++ linker due to LLVM/libclang being C++ libraries.
voidcaster_LINK = $(CXX) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

# Make sure version.h is generated correctly.
version.h:
	@echo Updating $@
	$(AM_V_at)echo "#define GIT_REVINFO \"$(shell git log -1 HEAD --pretty=format:"revision %h from %ai")\"" > version.h

voidcaster.c: version.h
