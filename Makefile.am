bin_PROGRAMS = voidcaster

voidcaster_SOURCES = interact.c msa.c treemunger.c voidcaster.c

# Force linking with C++ linker due to LLVM/libclang being C++ libraries.
voidcaster_LINK = $(CXX) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
