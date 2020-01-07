#! /usr/bin/python3
# Given a directory name as argument, this script generates an espfs filesystem image
# with all the html, js, css, png, and ico files found in the directory tree.
# Each file is compressed with gzip so it can be served up in compressed form from the esp8266.

from sys import argv, exit, stderr, stdout
from pathlib import Path
import re, gzip, struct

def mkespfs(dir, outbuf):
    MAGIC = 0x73665345
    FL_GZIP = 2 # gzipped file flag
    FL_LAST = 1 # last entry file flag

    f_html = list(Path(dir).rglob('*.html'))
    f_css = list(Path(dir).rglob('*.css'))
    f_js = list(Path(dir).rglob('*.js'))
    f_img = list(Path(dir).rglob('*.ico')) + list(Path(dir).rglob('*.png'))
    f_all = f_html + f_css + f_js + f_img
    sz = 0

    for fn in f_all:
        if not fn.is_file(): continue
        out_path = fn.relative_to(dir).as_posix().encode('ascii')
        info = fn.stat()
        data_un = fn.read_bytes()
        data_comp = gzip.compress(data_un)

        #print("Processing {} -> {}[{}], {}->{} bytes".format(fn, out_path, len(out_path), info.st_size,
        #    len(data_comp)), file=stderr)

        header = struct.pack('<IBBHII', MAGIC, FL_GZIP, 0, len(out_path), len(data_comp), info.st_size)
        outbuf.write(header)
        sz += len(header)
        outbuf.write(out_path)
        sz = (sz+len(out_path)+3) // 4 * 4
        if len(out_path)%4 != 0:
            outbuf.write(bytes(4)[:4-len(out_path)%4])
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

