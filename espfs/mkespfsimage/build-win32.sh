#!/bin/sh

cd mman-win32
./configure && make
cd ..
make -f Makefile.windows clean
make -f Makefile.windows USE_HEATSHRINK="yes" GZIP_COMPRESSION="no"
