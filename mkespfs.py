#! /usr/bin/python3
# Given a directory name as argument, this script generates an espfs filesystem image
# with all the html, js, css, png, and ico files found in the directory tree.
# Each file is compressed with gzip so it can be served up in compressed form from the esp8266.

# Format description from Jeroen:
# The idea 'borrows' from cpio: it's basically a concatenation of {header, filename, file} data.
# Header, filename and file data is 32-bit aligned. The last file is indicated by data-less header
# with the FLAG_LASTFILE flag set.
#
# #define FLAG_LASTFILE (1<<0)
# #define FLAG_GZIP (1<<1)
# #define COMPRESS_NONE 0
# #define COMPRESS_HEATSHRINK 1
# #define ESPFS_MAGIC 0x73665345
#
# typedef struct {
# 	int32_t magic;
# 	int8_t flags;
# 	int8_t compression;
# 	int16_t nameLen; // name must be null-terminated and NameLen includes padding!
# 	int32_t fileLenComp;
# 	int32_t fileLenDecomp;
# } __attribute__((packed)) EspFsHeader;



from sys import argv, exit, stderr, stdout
from pathlib import Path
import re, gzip, struct

def mkespfs(dir, outbuf):
    MAGIC = 0x73665345
    FL_GZIP = 2 # gzipped file flag
    FL_LAST = 1 # last entry file flag

    sz = 0
    for fn in Path(dir).rglob('*.*'):
        if not fn.is_file(): continue
        out_path = fn.relative_to(dir).as_posix().encode('ascii') + b'\000'
        while len(out_path) & 3 != 0: out_path += b'\000'
        info = fn.stat()
        data_un = fn.read_bytes()
        data_comp = gzip.compress(data_un)
        flag = FL_GZIP
        if len(data_un) <= len(data_comp):
            data_comp = data_un
            flag = 0

        print("Processing {} -> {}[{}], {}->{} bytes".format(fn, out_path, len(out_path), info.st_size,
            len(data_comp)), file=stderr)

        header = struct.pack('<IBBHII', MAGIC, flag, 0, len(out_path), len(data_comp), info.st_size)
        outbuf.write(header)
        sz += len(header)
        outbuf.write(out_path)
        outbuf.write(data_comp)
        sz += len(data_comp)
        if len(data_comp)%4 != 0:
            outbuf.write(bytes(4)[:4-len(data_comp)%4])
        sz = (sz+3) // 4 * 4

    trailer = struct.pack('<IBBHII', MAGIC, FL_LAST, 0, 0, 0, 0)
    outbuf.write(trailer)
    sz += len(trailer)

    print("espfs image: {} bytes".format(sz), file=stderr)

if __name__ == "__main__":
    if len(argv) != 2 or not Path(argv[1]).is_dir():
        print("Usage: {} directory".format(argv[0]), file=stderr)
        exit(1)
    mkespfs(argv[1], stdout.buffer)

