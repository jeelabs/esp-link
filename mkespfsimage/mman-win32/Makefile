#
# mman-win32 (mingw32) Makefile
#
include config.mak

ifeq ($(BUILD_STATIC),yes)
	TARGETS+=libmman.a
	INSTALL+=static-install
endif
ifeq ($(BUILD_MSVC),yes)
	SHFLAGS+=-Wl,--output-def,libmman.def
	INSTALL+=lib-install
endif

all: $(TARGETS)

mman.o: mman.c mman.h
	$(CC) -o mman.o -c mman.c -Wall -O3 -fomit-frame-pointer

libmman.a: mman.o
	$(AR) cru libmman.a mman.o
	$(RANLIB) libmman.a

static-install:
	mkdir -p $(DESTDIR)$(libdir)
	cp libmman.a $(DESTDIR)$(libdir)
	mkdir -p $(DESTDIR)$(incdir)
	cp mman.h $(DESTDIR)$(incdir)

lib-install:
	mkdir -p $(DESTDIR)$(libdir)
	cp libmman.lib $(DESTDIR)$(libdir)

install: $(INSTALL)

test.exe: test.c mman.c mman.h
	$(CC) -o test.exe test.c -L. -lmman

test: $(TARGETS) test.exe
	test.exe

clean::
	rm -f mman.o libmman.a libmman.def libmman.lib test.exe *.dat

distclean: clean
	rm -f config.mak

.PHONY: clean distclean install test
