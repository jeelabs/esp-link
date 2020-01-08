# pio-genespfs is a platformio extra script that uses mkespfs to generate a filesystem image
# that it then converts into a C file so all the data can be included into the flash image.

Import("env")
#Import("projenv")

from mkespfs import mkespfs
from io import BytesIO
from pathlib import Path
import shutil

dir = "html"
hdr = "head-"
temp = "temp-espfs"
espfile = "src/espfs_img.c"

# create empty temporary dir to create fs image
shutil.rmtree(temp, ignore_errors=True)
Path(temp).mkdir()

# place all the files we want into a temp directory and prefix all .html with hdr
f_html = list(Path(dir).rglob('*.html'))
f_css = list(Path(dir).rglob('*.css'))
f_js = list(Path(dir).rglob('*.js'))
f_img = list(Path(dir).rglob('*.ico')) + list(Path(dir).rglob('*.png'))
# simply copy most files
for fn in f_css+f_js+f_img:
    dst = Path(temp).joinpath(fn.relative_to(dir))
    dst.parent.mkdir(exist_ok=True)
    shutil.copyfile(fn, dst)
# prepend shared header to html files
for fn in f_html:
    with open(Path(temp).joinpath(fn.relative_to(dir)), 'wb') as dst:
        with open(Path(dir).joinpath(hdr), 'rb') as src:
            shutil.copyfileobj(src, dst)
        with open(fn, 'rb') as src:
            shutil.copyfileobj(src, dst)

# generate espfs image
buf = BytesIO()
espfsimg = mkespfs(temp, buf)

# remove temp tree
#shutil.rmtree(temp, ignore_errors=True)

# write as C file
fd = Path(espfile).open(mode='w')
fd.write("unsigned char espfs_image[] ");
fd.write("__attribute__((aligned(4))) ");
fd.write("__attribute__((section(\".irom.text\"))) = {");
for i, b in enumerate(buf.getbuffer()):
    if i%16 == 0: fd.write("\n")
    fd.write(" 0x{:02x},".format(b))
fd.write("\n};\n");
fd.close()
