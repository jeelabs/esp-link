ESP-LINK OTA Flash Layout
=========================

The flash layout dictated by the bootloader is the following (all this assumes a 512KB flash chip
and is documented in Espressif's `99C-ESP8266__OTA_Upgrade__EN_v1.5.pdf`):
 - @0x00000 4KB bootloader
 - @0x01000 236KB partition1
 - @0x3E000 16KB esp-link parameters
 - @0x40000 4KB unused
 - @0x41000 236KB partition2
 - @0x7E000 16KB system wifi parameters

What this means is that we can flash just about anything into partition1 or partition2 as long
as it doesn't take more than 236KB and has the right format that the boot loader understands.
We can't mess with the first 4KB nor the last 16KB of the flash.

Now how does a code partition break down? that is reflected in the following definition found in
the loader scripts:
```
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40201010, len = 0x2B000
```
This means that 80KB (0x14000) are reserved for "dram0_0", 32KB (0x8000) for "iram1_0" and
172KB (0x2B000) are reserved for irom0_0. The segments are used as follows:
 - dram0_0 is the data RAM and some of that gets initialized at boot time from flash (static variable initialization)
 - iram1_0 is the instruction RAM and all of that gets loaded at boot time from flash
 - irom0_0 is the instruction cache which gets loaded on-demand from flash (all functions
   with the `ICACHE_FLASH_ATTR` attribute go there)

You might notice that 80KB+32KB+172KB is more than 236KB and that's because not the entire dram0_0
segment needs to be loaded from flash, only the portion with statically initialized data.
You might also notice that while iram1_0 is as large as the chip's instruction RAM (at least
according to the info I've seen) the size of the irom0_0 segment is smaller than it could be,
since it's really not bounded by any limitation of the processor (it simply backs the cache).

When putting the OTA flash process together I ran into loader issues, namely, while I was having
relatively little initialized data and also not 32KB of iram1_0 instructions I was overflowing
the allotted 172KB of irom0_0. To fix the problem the build process modifies the loader scripts
(see the `build/eagle.esphttpd1.v6.ld` target in the Makefile) to increase the irom0_0 segment
to 224KB (a somewhat arbitrary value). This doesn't mean that there will be 224KB of irom0_0
in flash, it just means that that's the maximum the linker will put there without giving an error.
In the end what has to fit into the magic 236KB is the sum of the actual initialized data,
the actually used iram1_0 segment, and the irom0_0 segment.
In addition, the dram0_0 and iram1_0 segments can't exceed what's specified
in the loader script 'cause those are the limitations of the processor.

Now that you hopefully understand the above you can understand the line printed by the Makefile
when linking the firmware, which looks something like:
```
** user1.bin uses 218592 bytes of 241664 available
```
Here 241664 is 236KB and 218592 is the size of what's getting flashed, so you can tell that you have
another 22KB to spend (modulo some 4KB flash segment rounding).
(Note that user2.bin has exactly the same size, so the Makefile doesn't print its info.)
The Makefile also prints a few more details:
```
ls -ls eagle*bin
  4 -rwxrwxr-x 1 tve tve   2652 May 24 10:12 eagle.app.v6.data.bin
176 -rwxrwxr-x 1 tve tve 179732 May 24 10:12 eagle.app.v6.irom0text.bin
  8 -rwxrwxr-x 1 tve tve   5732 May 24 10:12 eagle.app.v6.rodata.bin
 32 -rwxrwxr-x 1 tve tve  30402 May 24 10:12 eagle.app.v6.text.bin
```
This says that we have 179732 bytes of irom0_0, we have 5732+2652 bytes of dram0_0 (read-only data
plus initialized read-write data), and we have 30402 bytes of iram1_0.

There's an additional twist to all this for the espfs "file system" that esphttpd uses.
The data for this is loaded at the end of irom0_0 and is called espfs.
The Makefile modifies the loader script to place the espfs at the start of irom0_0 and
ensure that it's 32-bit aligned. The size of the espfs is shown here:
```
4026be14 g       .irom0.text    00000000 _binary_espfs_img_end
40269e98 g       .irom0.text    00000000 _binary_espfs_img_start
00001f7c g       *ABS*  00000000 _binary_espfs_img_size
```
Namely, 0x1f7c = 8060 bytes.


