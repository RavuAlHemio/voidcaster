CC=clang
CXX=clang++
CPPFLAGS=-I/usr/local/include
LIBS= \
	-llibclang
LDFLAGS=

all: voidcaster

voidcaster: voidcaster.o msa.o
	$(CXX) $(LDFLAGS) $(CLDFLAGS) -o $@ $(LIBS) $^ $(LIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CLDFLAGS) -c -o $@ $^

clean:
	rm -f voidcaster
	rm -f voidcaster.o

.PHONY: all clean
