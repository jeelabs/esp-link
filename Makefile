#
# Makefile for esp-link - https://github.com/jeelabs/esp-link
#
# Makefile heavily adapted to esp-link and wireless flashing by Thorsten von Eicken
# Lots of work, in particular to support windows, by brunnels
# Original from esphttpd and others...
#
# Start by setting the directories for the toolchain a few lines down
# the default target will build the firmware images
# `make flash` will flash the esp serially
# `make wiflash` will flash the esp over wifi
# `VERBOSE=1 make ...` will print debug info
# `ESP_HOSTNAME=my.esp.example.com make wiflash` is an easy way to override a variable

# optional local configuration file
-include local.conf

# The Wifi station configuration can be hard-coded here, which makes esp-link come up in STA+AP
# mode trying to connect to the specified AP *only* if the flash wireless settings are empty!
# This happens on a full serial flash and avoids having to hunt for the AP...
# STA_SSID ?=
# STA_PASS ?= 

# The SOFTAP configuration can be hard-coded here, the minimum parameters to set are AP_SSID && AP_PASS
# The AP SSID has to be at least 8 characters long, same for AP PASSWORD
# The AP AUTH MODE can be set to:
#  0 = AUTH_OPEN, 
#  1 = AUTH_WEP, 
#  2 = AUTH_WPA_PSK, 
#  3 = AUTH_WPA2_PSK, 
#  4 = AUTH_WPA_WPA2_PSK
# SSID hidden default 0, ( 0 | 1 ) 
# Max connections default 4, ( 1 ~ 4 )
# Beacon interval default 100, ( 100 ~ 60000ms )
#
# AP_SSID ?=esp_link_test
# AP_PASS ?=esp_link_test
# AP_AUTH_MODE ?=4
# AP_SSID_HIDDEN ?=0
# AP_MAX_CONN ?=4
# AP_BEACON_INTERVAL ?=100

# If CHANGE_TO_STA is set to "yes" the esp-link module will switch to station mode
# once successfully connected to an access point. Else it will stay in STA+AP mode.
CHANGE_TO_STA ?= yes

# hostname or IP address for wifi flashing
ESP_HOSTNAME  ?= esp-link

# --------------- toolchain configuration ---------------

# Base directory for the compiler. Needs a / at the end.
# Typically you'll install https://github.com/pfalcon/esp-open-sdk
# IMPORTANT: use esp-open-sdk `make STANDALONE=n`: the SDK bundled with esp-open-sdk will *not* work!
XTENSA_TOOLS_ROOT ?= $(abspath ../esp-open-sdk/xtensa-lx106-elf/bin)/

# Firmware version 
# WARNING: if you change this expect to make code adjustments elsewhere, don't expect
# that esp-link will magically work with a different version of the SDK!!!
SDK_VERS ?= esp_iot_sdk_v2.1.0

# Try to find the firmware manually extracted, e.g. after downloading from Espressif's BBS,
# http://bbs.espressif.com/viewforum.php?f=46
# USING THE SDK BUNDLED WITH ESP-OPEN-SDK WILL NOT WORK!!!
SDK_BASE ?= $(wildcard ../$(SDK_VERS))

# If the firmware isn't there, see whether it got downloaded as part of esp-open-sdk
# This used to work at some point, but is not supported, uncomment if you feel lucky ;-)
#ifeq ($(SDK_BASE),)
#SDK_BASE := $(wildcard $(XTENSA_TOOLS_ROOT)/../../$(SDK_VERS))
#endif

# Clean up SDK path
SDK_BASE := $(abspath $(SDK_BASE))
$(info SDK     is $(SDK_BASE))

# Path to bootloader file
BOOTFILE	?= $(SDK_BASE/bin/boot_v1.6.bin)

# Esptool.py path and port, only used for 1-time serial flashing
# Typically you'll use https://github.com/themadinventor/esptool
# Windows users use the com port i.e: ESPPORT ?= com3
ESPTOOL		?= $(abspath ../esp-open-sdk/esptool/esptool.py)
ESPPORT		?= /dev/ttyUSB0
ESPBAUD		?= 230400

# --------------- chipset configuration   ---------------

# Pick your flash size: "512KB", "1MB", or "4MB"
FLASH_SIZE ?= 4MB

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

# --------------- esp-link modules config options ---------------

# Optional Modules: mqtt rest socket web-server syslog
MODULES ?= mqtt rest socket web-server syslog

# --------------- esphttpd config options ---------------

# If GZIP_COMPRESSION is set to "yes" then the static css, js, and html files will be compressed
# with gzip before added to the espfs image and will be served with gzip Content-Encoding header.
# This could speed up the downloading of these files, but might break compatibility with older
# web browsers not supporting gzip encoding because Accept-Encoding is simply ignored.
# Enable this option if you have large static files to serve (for e.g. JQuery, Twitter bootstrap)
# If you have text based static files with different extensions what you want to serve compressed
# then you will need to add the extension to the following places:
# - Add the extension to this Makefile at the webpages.espfs target to the find command
# - Add the extension to the gzippedFileTypes array in the user/httpd.c file
#
# Adding JPG or PNG files (and any other compressed formats) is not recommended, because GZIP
# compression does not work effectively on compressed files.
GZIP_COMPRESSION ?= yes

# If COMPRESS_W_HTMLCOMPRESSOR is set to "yes" then the static css and js files will be compressed with
# htmlcompressor and yui-compressor. This option works only when GZIP_COMPRESSION is set to "yes".
# https://code.google.com/p/htmlcompressor/#For_Non-Java_Projects
# http://yui.github.io/yuicompressor/
# enabled by default.
COMPRESS_W_HTMLCOMPRESSOR ?= yes
HTML_COMPRESSOR ?= htmlcompressor-1.5.3.jar
YUI_COMPRESSOR ?= yuicompressor-2.4.8.jar

# -------------- End of config options -------------

HTML_PATH = $(abspath ./html)/
WIFI_PATH = $(HTML_PATH)wifi/

ESP_FLASH_MAX       ?= 503808  # max bin file

ifeq ("$(FLASH_SIZE)","512KB")
# Winbond 25Q40 512KB flash, typ for esp-01 thru esp-11
ESP_SPI_SIZE        ?= 0       # 0->512KB (256KB+256KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 0       # 0->40Mhz
ET_FS               ?= 4m      # 4Mbit flash size in esptool flash command
ET_FF               ?= 40m     # 40Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x7E000 # where to flash blank.bin to erase wireless settings

else ifeq ("$(FLASH_SIZE)","1MB")
# ESP-01E
ESP_SPI_SIZE        ?= 2       # 2->1MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80MHz
ET_FS               ?= 8m      # 8Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0xFE000 # where to flash blank.bin to erase wireless settings

else ifeq ("$(FLASH_SIZE)","2MB")
# Manuf 0xA1 Chip 0x4015 found on wroom-02 modules
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 4       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
ET_FS               ?= 16m     # 16Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x1FE000 # where to flash blank.bin to erase wireless settings

else
# Winbond 25Q32 4MB flash, typ for esp-12
# Here we're using two partitions of approx 0.5MB because that's what's easily available in terms
# of linker scripts in the SDK. Ideally we'd use two partitions of approx 1MB, the remaining 2MB
# cannot be used for code (esp8266 limitation).
ESP_SPI_SIZE        ?= 4       # 6->4MB (1MB+1MB) or 4->4MB (512KB+512KB)
ESP_FLASH_MODE      ?= 0       # 0->QIO, 2->DIO
ESP_FLASH_FREQ_DIV  ?= 15      # 15->80Mhz
ET_FS               ?= 32m     # 32Mbit flash size in esptool flash command
ET_FF               ?= 80m     # 80Mhz flash speed in esptool flash command
ET_BLANK            ?= 0x3FE000 # where to flash blank.bin to erase wireless settings
endif

# --------------- esp-link version        ---------------

# Version-fu :-) This code assumes that a new maj.minor is started using a "vN.M.0" tag on master
# and that thereafter the desired patchlevel number is just the number of commits since the tag.
#
# Get the current branch name if not using travis
TRAVIS_BRANCH?=$(shell git symbolic-ref --short HEAD --quiet)
# Use git describe to get the latest version tag, commits since then, sha and dirty flag, this
# results is something like "v1.2.0-13-ab6cedf-dirty"
NO_TAG ?= "no-tag"
VERSION := $(shell (git describe --tags --match 'v*.*.*' --long --dirty || echo $(NO_TAG)) | sed -re 's/(\.0)?-/./')
# If not on master then insert the branch name
ifneq ($(TRAVIS_BRANCH),master)
ifneq ($(findstring V%,$(TRAVIS_BRANCH)),)
VERSION := $(shell echo $(VERSION) | sed -e 's/-/-$(TRAVIS_BRANCH)-/')
endif
endif
VERSION :=$(VERSION)
$(info VERSION is $(VERSION))

# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# name for the target project
TARGET		= httpd

# espressif tool to concatenate sections for OTA upload using bootloader v1.2+
APPGEN_TOOL	?= gen_appbin.py

CFLAGS=

# set defines for optional modules
ifneq (,$(findstring mqtt,$(MODULES)))
	CFLAGS		+= -DMQTT
endif

ifneq (,$(findstring rest,$(MODULES)))
	CFLAGS		+= -DREST
endif

ifneq (,$(findstring syslog,$(MODULES)))
	CFLAGS		+= -DSYSLOG
endif

ifneq (,$(findstring web-server,$(MODULES)))
	CFLAGS		+= -DWEBSERVER
endif

ifneq (,$(findstring socket,$(MODULES)))
	CFLAGS		+= -DSOCKET
endif

# which modules (subdirectories) of the project to include in compiling
LIBRARIES_DIR 	= libraries
MODULES		+= espfs httpd user serial cmd esp-link
MODULES		+= $(foreach sdir,$(LIBRARIES_DIR),$(wildcard $(sdir)/*))
EXTRA_INCDIR 	= include .

# libraries used in this project, mainly provided by the SDK
LIBS = c gcc hal phy pp net80211 wpa main lwip_536 crypto

# compiler flags using during compilation of source files
CFLAGS	+= -Os -ggdb -std=c99 -Werror -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions \
	-nostdlib -mlongcalls -mtext-section-literals -ffunction-sections -fdata-sections \
	-D__ets__ -DICACHE_FLASH -Wno-address -DFIRMWARE_SIZE=$(ESP_FLASH_MAX) \
	-DMCU_RESET_PIN=$(MCU_RESET_PIN) -DMCU_ISP_PIN=$(MCU_ISP_PIN) \
	-DLED_CONN_PIN=$(LED_CONN_PIN) -DLED_SERIAL_PIN=$(LED_SERIAL_PIN) \
	-DVERSION="esp-link $(VERSION)"

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--gc-sections

# linker script used for the above linker step
LD_SCRIPT 	:= build/eagle.esphttpd.v6.ld
LD_SCRIPT1	:= build/eagle.esphttpd1.v6.ld
LD_SCRIPT2	:= build/eagle.esphttpd2.v6.ld

# various paths from the SDK used in this project
SDK_LIBDIR	= lib
SDK_LDDIR	= ld
SDK_INCDIR	= include include/json
SDK_TOOLSDIR	= tools

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCP		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy
OBJDP		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objdump
ELF_SIZE	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-size

####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_LDDIR 	:= $(addprefix $(SDK_BASE)/,$(SDK_LDDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))
SDK_TOOLS	:= $(addprefix $(SDK_BASE)/,$(SDK_TOOLSDIR))
APPGEN_TOOL	:= $(addprefix $(SDK_TOOLS)/,$(APPGEN_TOOL))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC)) $(BUILD_BASE)/espfs_img.o
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
USER1_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user1.out)
USER2_OUT 	:= $(addprefix $(BUILD_BASE)/,$(TARGET).user2.out)

INCDIR		:= $(addprefix -I,$(SRC_DIR))
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

ifneq ($(strip $(STA_SSID)),)
CFLAGS		+= -DSTA_SSID="$(STA_SSID)"
endif

ifneq ($(strip $(STA_PASS)),)
CFLAGS		+= -DSTA_PASS="$(STA_PASS)"
endif

ifneq ($(strip $(AP_SSID)),)
CFLAGS		+= -DAP_SSID="$(AP_SSID)"
endif

ifneq ($(strip $(AP_PASS)),)
CFLAGS		+= -DAP_PASS="$(AP_PASS)"
endif

ifneq ($(strip $(AP_AUTH_MODE)),)
CFLAGS		+= -DAP_AUTH_MODE="$(AP_AUTH_MODE)"
endif

ifneq ($(strip $(AP_SSID_HIDDEN)),)
CFLAGS		+= -DAP_SSID_HIDDEN="$(AP_SSID_HIDDEN)"
endif

ifneq ($(strip $(AP_MAX_CONN)),)
CFLAGS		+= -DAP_MAX_CONN="$(AP_MAX_CONN)"
endif

ifneq ($(strip $(AP_BEACON_INTERVAL)),)
CFLAGS		+= -DAP_BEACON_INTERVAL="$(AP_BEACON_INTERVAL)"
endif

ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DGZIP_COMPRESSION
endif

ifeq ("$(CHANGE_TO_STA)","yes")
CFLAGS		+= -DCHANGE_TO_STA
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q)$(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean webpages.espfs wiflash

all: checkdirs $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

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

$(FW_BASE):
	$(vecho) "FW $@"
	$(Q) mkdir -p $@

$(FW_BASE)/user1.bin: $(USER1_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER1_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER1_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER1_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER1_OUT) eagle.app.v6.irom0text.bin
	$(Q) $(ELF_SIZE) -A $(USER1_OUT) |grep -v " 0$$" |grep .
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER1_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE) 0 >/dev/null
	$(Q) rm -f eagle.app.v6.*.bin
	$(Q) mv eagle.app.flash.bin $@
	@echo "    user1.bin uses $$(stat -c '%s' $@) bytes of" $(ESP_FLASH_MAX) "available"
	$(Q) if [ $$(stat -c '%s' $@) -gt $$(( $(ESP_FLASH_MAX) )) ]; then echo "$@ too big!"; false; fi

$(FW_BASE)/user2.bin: $(USER2_OUT) $(FW_BASE)
	$(Q) $(OBJCP) --only-section .text -O binary $(USER2_OUT) eagle.app.v6.text.bin
	$(Q) $(OBJCP) --only-section .data -O binary $(USER2_OUT) eagle.app.v6.data.bin
	$(Q) $(OBJCP) --only-section .rodata -O binary $(USER2_OUT) eagle.app.v6.rodata.bin
	$(Q) $(OBJCP) --only-section .irom0.text -O binary $(USER2_OUT) eagle.app.v6.irom0text.bin
	$(Q) COMPILE=gcc PATH=$(XTENSA_TOOLS_ROOT):$(PATH) python $(APPGEN_TOOL) $(USER2_OUT) 2 $(ESP_FLASH_MODE) $(ESP_FLASH_FREQ_DIV) $(ESP_SPI_SIZE) 1 >/dev/null
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
	./wiflash $(ESP_HOSTNAME) $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin

baseflash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash 0x01000 $(FW_BASE)/user1.bin

flash: all
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash -fs $(ET_FS) -ff $(ET_FF) \
	  0x00000 "$(SDK_BASE)/bin/boot_v1.5.bin" 0x01000 $(FW_BASE)/user1.bin \
	  $(ET_BLANK) $(SDK_BASE)/bin/blank.bin

tools/$(HTML_COMPRESSOR):
	$(Q) echo "The jar files in the tools dir are missing, they should be in the source repo"
	$(Q) echo "The following commands can be used to fetch them, but the URLs have changed..."
	$(Q) echo mkdir -p tools
	$(Q) echo "cd tools; wget --no-check-certificate https://github.com/yui/yuicompressor/releases/download/v2.4.8/$(YUI_COMPRESSOR) -O $(YUI_COMPRESSOR)"
	$(Q) echo "cd tools; wget --no-check-certificate https://htmlcompressor.googlecode.com/files/$(HTML_COMPRESSOR) -O $(HTML_COMPRESSOR)"

ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
$(BUILD_BASE)/espfs_img.o: tools/$(HTML_COMPRESSOR)
endif

$(BUILD_BASE)/espfs_img.o: html/ html/wifi/ espfs/mkespfsimage/mkespfsimage
	$(Q) rm -rf html_compressed; mkdir html_compressed; mkdir html_compressed/wifi;
	$(Q) cp -r html/*.ico html_compressed;
	$(Q) cp -r html/*.css html_compressed;
	$(Q) cp -r html/*.js html_compressed;
	$(Q) cp -r html/wifi/*.png html_compressed/wifi;
	$(Q) cp -r html/wifi/*.js html_compressed/wifi;
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) echo "Compressing assets with htmlcompressor. This may take a while..."
	$(Q) java -jar tools/$(HTML_COMPRESSOR) \
	  -t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
	  -o $(abspath ./html_compressed)/ \
	  $(HTML_PATH)head- \
	  $(HTML_PATH)*.html
	$(Q) java -jar tools/$(HTML_COMPRESSOR) \
	  -t html --remove-surrounding-spaces max --remove-quotes --remove-intertag-spaces \
	  -o $(abspath ./html_compressed)/wifi/ \
	  $(WIFI_PATH)*.html
	$(Q) echo "Compressing assets with yui-compressor. This may take a while..."
	$(Q) for file in `find html_compressed -type f -name "*.js"`; do \
	    java -jar tools/$(YUI_COMPRESSOR) $$file --line-break 0 -o $$file; \
	  done
	$(Q) for file in `find html_compressed -type f -name "*.css"`; do \
	    java -jar tools/$(YUI_COMPRESSOR) $$file -o $$file; \
	  done
else
	$(Q) cp -r html/head- html_compressed;
	$(Q) cp -r html/*.html html_compressed;
	$(Q) cp -r html/wifi/*.html html_compressed/wifi;	
endif
ifeq (,$(findstring mqtt,$(MODULES)))
	$(Q) rm -rf html_compressed/mqtt.html
	$(Q) rm -rf html_compressed/mqtt.js
endif
	$(Q) for file in `find html_compressed -type f -name "*.htm*"`; do \
	    cat html_compressed/head- $$file >$${file}-; \
	    mv $$file- $$file; \
	  done
	$(Q) rm html_compressed/head-
	$(Q) cd html_compressed; find . \! -name \*- | ../espfs/mkespfsimage/mkespfsimage > ../build/espfs.img; cd ..;
	$(Q) ls -sl build/espfs.img
	$(Q) cd build; $(OBJCP) -I binary -O elf32-xtensa-le -B xtensa --rename-section .data=.espfs \
	  espfs.img espfs_img.o; cd ..

# edit the loader script to add the espfs section to the end of irom with a 4 byte alignment.
# we also adjust the sizes of the segments 'cause we need more irom0
build/eagle.esphttpd1.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
		-e '/^  irom0_0_seg/ s/6B000/7C000/' \
		$(SDK_LDDIR)/eagle.app.v6.new.1024.app1.ld >$@
build/eagle.esphttpd2.v6.ld: $(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld
	$(Q) sed -e '/\.irom\.text/{' -e 'a . = ALIGN (4);' -e 'a *(.espfs)' -e '}'  \
		-e '/^  irom0_0_seg/ s/6B000/7C000/' \
		$(SDK_LDDIR)/eagle.app.v6.new.1024.app2.ld >$@

espfs/mkespfsimage/mkespfsimage: espfs/mkespfsimage/
	$(Q) $(MAKE) -C espfs/mkespfsimage GZIP_COMPRESSION="$(GZIP_COMPRESSION)"

release: all
	$(Q) rm -rf release; mkdir -p release/esp-link-$(VERSION)
	$(Q) egrep -a 'esp-link [a-z0-9.]+ - 201' $(FW_BASE)/user1.bin | cut -b 1-80
	$(Q) egrep -a 'esp-link [a-z0-9.]+ - 201' $(FW_BASE)/user2.bin | cut -b 1-80
	$(Q) cp $(FW_BASE)/user1.bin $(FW_BASE)/user2.bin $(SDK_BASE)/bin/blank.bin \
	       "$(SDK_BASE)/bin/boot_v1.7.bin" "$(SDK_BASE)/bin/esp_init_data_default.bin" \
	       wiflash avrflash megaflash release/esp-link-$(VERSION)
	$(Q) tar zcf esp-link-$(VERSION).tgz -C release esp-link-$(VERSION)
	$(Q) echo "Release file: esp-link-$(VERSION).tgz"
	$(Q) rm -rf release

docker:
	$(Q) docker build -t jeelabs/esp-link .
clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) -type f | xargs rm -f
	$(Q) make -C espfs/mkespfsimage/ clean
	$(Q) rm -rf $(FW_BASE)
	$(Q) rm -f webpages.espfs
ifeq ("$(COMPRESS_W_HTMLCOMPRESSOR)","yes")
	$(Q) rm -rf html_compressed
endif

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))

depend:
	makedepend -p${BUILD_BASE}/ -Y -- $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) -I${XTENSA_TOOLS_ROOT}../xtensa-lx106-elf/include -I${XTENSA_TOOLS_ROOT}../lib/gcc/xtensa-lx106-elf/4.8.2/include -- */*.c

# Rebuild version at least at every Makefile change

${BUILD_BASE}/esp-link/main.o: Makefile

# DO NOT DELETE

