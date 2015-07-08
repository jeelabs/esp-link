#
# Makefile for esp-link - https://github.com/jeelabs/esp-link
#
# Start by setting the directories for the toolchain a few lines down
# the default target will build the firmware images
# `make flash` will flash the esp serially
# `make wiflash` will flash the esp over wifi
# `VERBOSE=1 make ...` will print debug info
# `ESP_HOSTNAME=my.esp.example.com make wiflash` is an easy way to override a variable
#
# Makefile heavily adapted to esp-link and wireless flashing by Thorsten von Eicken
# Original from esphttpd and others...

# --------------- toolchain configuration ---------------

# Base directory for the compiler. Needs a / at the end.
# Typically you'll install https://github.com/pfalcon/esp-open-sdk
XTENSA_TOOLS_ROOT ?= $(abspath ../esp-open-sdk/xtensa-lx106-elf/bin)/

# Base directory of the ESP8266 SDK package, absolute
# Typically you'll download from Espressif's BBS, http://bbs.espressif.com/viewforum.php?f=5
SDK_BASE	?= $(abspath ../esp_iot_sdk_v1.2.0)

# Esptool.py path and port, only used for 1-time serial flashing
# Typically you'll use https://github.com/themadinventor/esptool
ESPTOOL		?= $(abspath ../esp-open-sdk/esptool/esptool.py)
ESPPORT		?= /dev/ttyUSB0
ESPBAUD		?= 460800

# --------------- chipset configuration   ---------------

ESP_SPI_SIZE        ?= 0      # 0->512KB
ESP_FLASH_MODE      ?= 0      # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 0      # 0->40Mhz
ESP_FLASH_MAX       ?= 241664 # max bin file for 512KB flash: 236KB

# hostname or IP address for wifi flashing
ESP_HOSTNAME        ?= esp-link

# The pin assignments below are used when the settings in flash are invalid, they
# can be changed via the web interface
# GPIO pin used to reset attached microcontroller, acative low
MCU_RESET_PIN       ?= 12
# GPIO pin used with reset to reprogram MCU (ISP=in-system-programming, unused with AVRs), active low
MCU_ISP_PIN         ?= 13
# GPIO pin used for "connectivity" LED, active low
LED_CONN_PIN        ?= 0
# GPIO pin used for "serial activity" LED, active low
LED_SERIAL_PIN      ?= 14

# --------------- esp-link version        ---------------

# This queries git to produce a version string like "esp-link v0.9.0 2015-06-01 34bc76"
# If you don't have a proper git checkout or are on windows, then simply swap for the constant
# Steps to release: create release on github, git pull, git describe --tags to verify you're
# on the release tag, make release, upload esp-link.tgz into the release files
#VERSION ?= "esp-link custom version"
DATE    := $(shell date '+%F %T')
BRANCH  := $(shell git describe --tags)
SHA     := $(shell if git diff --quiet HEAD; then git rev-parse --short HEAD | cut -d"/" -f 3; \
	else echo "development"; fi)
VERSION ?=esp-link $(BRANCH) - $(DATE) - $(SHA)

# --------------- esp-link config options ---------------

# If CHANGE_TO_STA is set to "yes" the esp-link module will switch to station mode
# once successfully connected to an access point. Else it will stay in AP+STA mode.

CHANGE_TO_STA ?= yes

# --------------- esphttpd config options ---------------

# If GZIP_COMPRESSION is set to "yes" then the static css, js, and html files will be compressed
# with gzip before added to the espfs image and will be served with gzip Content-Encoding header.
# This could speed up the downloading of these files, but might break compatibility with older
# web browsers not supporting gzip encoding because Accept-Encoding is simply ignored.
# Enable this option if you have large static files to serve (for e.g. JQuery, Twitter bootstrap)
# By default only js, css and html files are compressed using heatshrink.
# If you have text based static files with different extensions what you want to serve compressed
# then you will need to add the extension to the following places:
# - Add the extension to this Makefile at the webpages.espfs target to the find command
# - Add the extension to the gzippedFileTypes array in the user/httpd.c file
#
# Adding JPG or PNG files (and any other compressed formats) is not recommended, because GZIP
# compression does not work effectively on compressed files.

#Static gzipping is disabled by default.
GZIP_COMPRESSION ?= yes

# If COMPRESS_W_YUI is set to "yes" then the static css and js files will be compressed with
# yui-compressor. This option works only when GZIP_COMPRESSION is set to "yes".
# http://yui.github.io/yuicompressor/
#Disabled by default.
COMPRESS_W_YUI ?= yes
YUI-COMPRESSOR ?= yuicompressor-2.4.8.jar

# If USE_HEATSHRINK is set to "yes" then the espfs files will be compressed with Heatshrink and
# decompressed on the fly while reading the file.
# Because the decompression is done in the esp8266, it does not require any support in the browser.
USE_HEATSHRINK ?= no

# -------------- End of config options -------------

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# name for the target project
TARGET		= httpd

# espressif tool to concatenate sections for OTA upload using bootloader v1.2+
APPGEN_TOOL	?= gen_appbin.py

# which modules (subdirectories) of the project to include in compiling
MODULES		= espfs httpd user serial
EXTRA_INCDIR	= include . # lib/heatshrink/

# libraries used in this project, mainly provided by the SDK
LIBS		= c gcc hal phy pp net80211 wpa main lwip

# compiler flags using during compilation of source files
CFLAGS		= -Os -ggdb -std=c99 -Werror -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions \
		-nostdlib -mlongcalls -mtext-section-literals -ffunction-sections -fdata-sections \
		-D__ets__ -DICACHE_FLASH -D_STDINT_H -Wno-address -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
		-DMCU_RESET_PIN=$(MCU_RESET_PIN) -DMCU_ISP_PIN=$(MCU_ISP_PIN) \
		-DLED_CONN_PIN=$(LED_CONN_PIN) -DLED_SERIAL_PIN=$(LED_SERIAL_PIN) \
		-DVERSION="$(VERSION)"

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--gc-sections

# linker script used for the above linker step
LD_SCRIPT 	:= build/eagle.esphttpd.v6.ld
LD_SCRIPT1	:= build/eagle.esphttpd1.v6.ld
LD_SCRIPT2	:= build/eagle.esphttpd2.v6.ld

# various paths from the SDK used in this project
SDK_LIBDIR		= lib
SDK_LDDIR			= ld
SDK_INCDIR		= include include/json
SDK_TOOLSDIR	= tools

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCP := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDP := $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump


####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LDDIR 	:= $(addprefix $(SDK_BASE)/,$(SDK_LDDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
SDK_TOOLS		:= $(addprefix $(SDK_BASE)/,$(SDK_TOOLSDIR))
APPGEN_TOOL	:= $(addprefix $(SDK_TOOLS)/,$(APPGEN_TOOL))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC)) $(BUILD_BASE)/espfs_img.o
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)
USER1_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user1.out)
USER2_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user2.out)

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DGZIP_COMPRESSION
endif

ifeq ("$(USE_HEATSHRINK)","yes")
CFLAGS		+= -DESPFS_HEATSHRINK
endif

ifeq ("$(CHANGE_TO_STA)","yes")
CFLAGS          += -DCHANGE_TO_STA
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean webpages.espfs wiflash

all: echo_version checkdirs $(FW_BASE) firmware/user1.bin firmware/user2.bin

echo_version:
	@echo VERSION: $(VERSION)

$(TARGET_OUT): $(APP_AR) $(LD_SCRIPT)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
#	$(OBJDP) -x $(TARGET_OUT) | egrep '(espfs_img)'

$(USER1_OUT): $(APP_AR) $(LD_SCRIPT1)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT1) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
	@echo Dump  : $(OBJDP) -x $(USER1_OUT)
	@echo Disass: $(OBJDP) -d -l -x $(USER1_OUT)
#	$(Q) $(OBJDP) -x $(TARGET_OUT) | egrep espfs_img

$(USER2_OUT): $(APP_AR) $(LD_SCRIPT2)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) -T$(LD_SCRIPT2) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
#	$(Q) $(OBJDP) -x $(TARGET_OUT) | egrep espfs_img

$(FW_BASE): $(TARGET_OUT)
	$(vecho) "FW $@"
	$(Q) mkdir -p $@
	$(Q) $(ESPTOOL) elf2image $(TARGET_OUT) --output $@/

firmware/user1.bin: $(USER1_OUT)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER1_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER1_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER1_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER1_OUT) eagle.app.v6.irom0text.bin
	ls -ls eagle*bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER1_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE)
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	@echo "** user1.bin uses $$(stat -c '%s' $@) bytes of" $(ESP_FLASH_MAX) "available"
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

firmware/user2.bin: $(USER2_OUT)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER2_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER2_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER2_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER2_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER2_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE)
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi


$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@

wiflash: all
	./wiflash $(ESP_HOSTNAME) firmware/user1.bin firmware/user2.bin

flash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash \
	  0x00000 "$(SDK_BASE)/bin/boot_v1.4(b1).bin" 0x01000 $(FW_BASE)/user1.bin \
	  0x7E000 $(SDK_BASE)/bin/blank.bin

yui/$(YUI-COMPRESSOR):
	$(Q) mkdir -p yui
	cd yui; wget https://github.com/yui/yuicompressor/releases/download/v2.4.8/$(YUI-COMPRESSOR)

ifeq ("$(COMPRESS_W_YUI)","yes")
$(BUILD_BASE)/espfs_img.o: yui/$(YUI-COMPRESSOR)
endif

$(BUILD_BASE)/espfs_img.o: html/ html/wifi/ espfs/mkespfsimage/mkespfsimage
	$(Q) rm -rf html_compressed;
	$(Q) cp -r html html_compressed;
	$(Q) for file in `find html_compressed -type f -name "*.htm*"`; do \
			cat html_compressed/head- $$file >$${file}-; \
			mv $$file- $$file; \
		done
ifeq ("$(COMPRESS_W_YUI)","yes")
	$(Q) echo "Compression assets with yui-compressor. This may take a while..."
	$(Q) for file in `find html_compressed -type f -name "*.js"`; do \
			java -jar yui/$(YUI-COMPRESSOR) $$file --nomunge --line-break 40 -o $$file; \
		done
	$(Q) for file in `find html_compressed -type f -name "*.css"`; do \
			java -jar yui/$(YUI-COMPRESSOR) $$file -o $$file; \
		done
endif
	$(Q) cd html_compressed; find . \! -name \*- | ../espfs/mkespfsimage/mkespfsimage > ../build/espfs.img; cd ..;
	$(Q) ls -sl build/espfs.img
	$(Q) cd build; $(OBJCP) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.espfs \
			espfs.img espfs_img.o; cd ..

# edit the loader script to add the espfs section to the end of irom with a 4 byte alignment.
# we also adjust the sizes of the segments 'cause we need more irom0
# in the end the only thing that matters wrt size is that the whole shebang fits into the
# 236KB available (in a 512KB flash)
build/eagle.esphttpd.v6.ld: $(SDK_LDDIR)/eagle.app.v6.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
	    $(SDK_LDDIR)/eagle.app.v6.ld >$@
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
	    $(SDK_LDDIR)/eagle.app.v6.new.512.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
			-e '/^  irom0_0_seg/ s/2B000/38000/' \
	    $(SDK_LDDIR)/eagle.app.v6.new.512.app2.ld >$@

espfs/mkespfsimage/mkespfsimage: espfs/mkespfsimage/
	$(Q) $(MAKE) -C espfs/mkespfsimage USE_HEATSHRINK="$(USE_HEATSHRINK)" GZIP_COMPRESSION="$(GZIP_COMPRESSION)"

release: all
	$(Q) rm -rf release; mkdir -p release/esp-link
	$(Q) cp firmware/user1.bin firmware/user2.bin $(SDK_BASE)/bin/blank.bin \
		   "$(SDK_BASE)/bin/boot_v1.4(b1).bin" wiflash release/esp-link
	$(Q) tar zcf esp-link.tgz -C release esp-link
	$(Q) rm -rf release

clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) -type f | xargs rm -f
	$(Q) make -C espfs/mkespfsimage/ clean
	$(Q) rm -rf $(FW_BASE)
	$(Q) rm -f webpages.espfs
ifeq ("$(COMPRESS_W_YUI)","yes")
	$(Q) rm -rf html_compressed
endif

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
