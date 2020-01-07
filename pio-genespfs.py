# pio-genespfs is a platformio extra script that uses mkespfs to generate a filesystem image
# that it then converts into a C file so all the data can be included into the flash image.

Import("env")
#Import("projenv")

from mkespfs import mkespfs
from io import BytesIO
from pathlib import Path

dir = "html"
espfile = "src/espfs_img.c"

buf = BytesIO()
espfsimg = mkespfs(dir, buf)

fd = Path(espfile).open(mode='w')
fd.write("unsigned char espfs_image[] ");
fd.write("__attribute__((aligned(4))) ");
fd.write("__attribute__((section(\".irom.text\"))) = {");
for i, b in enumerate(buf.getbuffer()):
    if i%16 == 0: fd.write("\n")
    fd.write(" 0x{:02x},".format(b))
fd.write("\n};\n");
fd.close()
