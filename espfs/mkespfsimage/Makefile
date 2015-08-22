GZIP_COMPRESSION?=no

CFLAGS=-I.. -std=gnu99

ifeq ("$(GZIP_COMPRESSION)","yes")
LDFLAGS+=-lz
CFLAGS+=-DESPFS_GZIP
endif

ifeq ($(OS),Windows_NT)
CFLAGS+=-Imman-win32
LDFLAGS+=-Lmman-win32 -lmman
TARGET =mkespfsimage.exe
else
TARGET =mkespfsimage
endif

OBJS=main.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)