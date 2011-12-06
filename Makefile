CC=clang
CXX=clang++
CPPFLAGS=-I/usr/local/include
LIBS= \
	-llibclang
LDFLAGS=

ifeq ($(VERBOSE),1)
Q=
else
Q=@
endif

-include config.mk

all: voidcaster

voidcaster: voidcaster.o msa.o
	@echo LINK $@
	$(Q)$(CXX) $(LDFLAGS) $(CLDFLAGS) -o $@ $(LIBS) $^ $(LIBS)

%.o: %.c
	@echo CC $@
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CLDFLAGS) -c -o $@ $^

clean:
	rm -f voidcaster
	rm -f voidcaster.o

# force rebuild if config changes
ifneq ($(wildcard config.mk),)
Makefile: config.mk
endif
%.c: Makefile

.PHONY: all clean
