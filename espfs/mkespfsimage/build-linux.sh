#!/bin/sh

make -f Makefile.linux clean
make -f Makefile.linux USE_HEATSHRINK="yes" GZIP_COMPRESSION="no"
