#
# mman-win32 (mingw32) Makefile
#
include config.mak

CFLAGS=-Wall -O3 -fomit-frame-pointer

ifeq ($(BUILD_STATIC),yes)
	TARGETS+=libmman.a
	INSTALL+=static-install
endif

ifeq ($(BUILD_SHARED),yes)
	TARGETS+=libmman.dll
	INSTALL+=shared-install
	CFLAGS+=-DMMAN_LIBRARY_DLL -DMMAN_LIBRARY
endif

ifeq ($(BUILD_MSVC),yes)
	SHFLAGS+=-Wl,--output-def,libmman.def
	INSTALL+=lib-install
endif

all: $(TARGETS)

mman.o: mman.c mman.h
	$(CC) -o mman.o -c mman.c $(CFLAGS)

libmman.a: mman.o
	$(AR) cru libmman.a mman.o
	$(RANLIB) libmman.a

libmman.dll: mman.o
	$(CC) -shared -o libmman.dll mman.o -Wl,--out-implib,libmman.dll.a

header-install:
	mkdir -p $(DESTDIR)$(incdir)
	cp mman.h $(DESTDIR)$(incdir)

static-install: header-install
	mkdir -p $(DESTDIR)$(libdir)
	cp libmman.a $(DESTDIR)$(libdir)

shared-install: header-install
	mkdir -p $(DESTDIR)$(libdir)
	cp libmman.dll.a $(DESTDIR)$(libdir)
	mkdir -p $(DESTDIR)$(bindir)
	cp libmman.dll $(DESTDIR)$(bindir)

lib-install:
	mkdir -p $(DESTDIR)$(libdir)
	cp libmman.lib $(DESTDIR)$(libdir)

install: $(INSTALL)

test.exe: test.c mman.c mman.h
	$(CC) -o test.exe test.c -L. -lmman

test: $(TARGETS) test.exe
	test.exe

clean::
	rm -f mman.o libmman.a libmman.dll.a libmman.dll libmman.def libmman.lib test.exe *.dat

distclean: clean
	rm -f config.mak

.PHONY: clean distclean install test
