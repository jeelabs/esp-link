GZIP_COMPRESSION ?= no

ifeq ($(OS),Windows_NT)

TARGET = mkespfsimage.exe

CC = gcc
LD = $(CC)
CFLAGS=-c -I.. -Imman-win32 -std=gnu99
LDFLAGS=-Lmman-win32 -lmman 

ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS += -DESPFS_GZIP
LDFLAGS += -lz
endif

OBJECTS = main.o

all: libmman $(TARGET)

libmman:
	$(Q) make -C mman-win32

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(OBJECTS) $(TARGET) ./mman-win32/libmman.a ./mman-win32/mman.o

.PHONY: all clean

else

CC=gcc
CFLAGS=-I.. -std=gnu99
ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS+= -DESPFS_GZIP
endif

OBJS=main.o
TARGET=mkespfsimage

$(TARGET): $(OBJS)
ifeq ("$(GZIP_COMPRESSION)","yes")
	$(CC) -o $@ $^ -lz
else
	$(CC) -o $@ $^
endif

clean:
	rm -f $(TARGET) $(OBJS)

endif
