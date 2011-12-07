llp=/usr/local

CC=clang
CXX=clang++
CPPFLAGS=-I$(llp)/include
CFLAGS=-Wall
LIBS= \
	$(llp)/lib/liblibclang.a \
	$(llp)/lib/libclangSema.a \
	$(llp)/lib/libclangAnalysis.a \
	$(llp)/lib/libclangCodeGen.a \
	$(llp)/lib/libclangFrontend.a \
	$(llp)/lib/libclangDriver.a \
	$(llp)/lib/libclangSerialization.a \
	$(llp)/lib/libclangParse.a \
	$(llp)/lib/libclangLex.a \
	$(llp)/lib/libclangAST.a \
	$(llp)/lib/libclangBasic.a \
	$(llp)/lib/libLLVMMC.a \
	$(llp)/lib/libLLVMSupport.a \
	-lpthread \
	-ldl
LDFLAGS=

ifeq ($(VERBOSE),1)
Q=
else
Q=@
endif

-include config.mk

OBJECTS= \
	voidcaster.o \
	msa.o \
	treemunger.o

all: voidcaster

voidcaster: $(OBJECTS)
	@echo LINK $@
	$(Q)$(CXX) $(LDFLAGS) $(CLDFLAGS) -o $@ $(LIBS) $^ $(LIBS)

%.o: %.c
	@echo CC $@
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CLDFLAGS) -c -o $@ $^

clean:
	rm -f voidcaster
	rm -f $(OBJECTS)

# force rebuild if config changes
ifneq ($(wildcard config.mk),)
Makefile: config.mk
endif
%.c: Makefile

.PHONY: all clean
