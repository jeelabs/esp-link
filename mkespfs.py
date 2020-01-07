#! /usr/bin/python3

from sys import argv, exit, stderr, stdout
from pathlib import Path
import re, gzip, struct

MAGIC = 0x73665345
FL_GZIP = 2 # gzipped file flag
FL_LAST = 1 # last entry file flag

if len(argv) != 2 or not Path(argv[1]).is_dir():
    print("Usage: {} directory".format(argv[0]), file=stderr)
    exit(1)
dir = argv[1]

f_html = list(Path(dir).rglob('*.html'))
f_css = list(Path(dir).rglob('*.css'))
f_js = list(Path(dir).rglob('*.js'))
f_img = list(Path(dir).rglob('*.ico')) + list(Path(dir).rglob('*.png'))
f_all = f_html + f_css + f_js + f_img

for fn in f_all:
    if not fn.is_file(): continue
    out_path = fn.relative_to(dir).as_posix().encode('ascii')
    info = fn.stat()
    data_un = fn.read_bytes()
    data_comp = gzip.compress(data_un)

    print("Processing {} -> {}[{}], {}->{} bytes".format(fn, out_path, len(out_path), info.st_size,
        len(data_comp)), file=stderr)

    header = struct.pack('<IBBHII', MAGIC, FL_GZIP, 0, len(out_path), len(data_comp), info.st_size)
    stdout.buffer.write(header)
    stdout.buffer.write(out_path)
    if len(out_path)%4 != 0:
        stdout.buffer.write(bytes(4)[:4-len(out_path)%4])
    stdout.buffer.write(data_comp)
    if len(data_comp)%4 != 0:
        stdout.buffer.write(bytes(4)[:4-len(data_comp)%4])

trailer = struct.pack('<IBBHII', MAGIC, FL_LAST, 0, 0, 0, 0)

print("DONE", file=stderr)
