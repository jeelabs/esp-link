Building esp-link
=================

Before you build esp-link, consider that you can download ready-made firmware images!
Just head over to the [release section](https://github.com/jeelabs/esp-link/releases)
and download the tgz archive.

If you decide to build your own, there are a number of options:
- On linux x86 download the ready-built toolchain and patched SDK like the automated build does
  and compile the firmware
- On linux use a docker image with the toolchain and the SDK to compile the firmware
- On linux download and build the toolchain, download and patch the SDK, then compile the firmware
- On windows use a docker image with the toolchain and the SDK to compile the firmware
- On windows install mingw, python, java, and a slew of other tools and then build the
  firmware

Once you have built the firmware you will want to flash it to your esp8266 module.
Assuming you already have esp-link running you can either go back to the initial flashing
via the serial port or you can use the over-the-air (i.e. Wifi) update method, which is faster
and more reliable (unless you have a non-booting version of esp-link).
The OTA flashing is described at the end of this page,
the serial flashing is described in [FLASHING.md](FLASHING.md).

### Automated builds

For every commit on github an automated build is made. This means that every branch, including
master, and every pull request always has an up-to-date build. These builds are made by Travis
using the instructions in `.travis.yml`, which basically consist of cloning the esp-link repo,
downloading the compiler toolchain, downloading the Espressif SDK, and running `make`.
If you're looking for how to build esp-link the travis instructions will always give you
accurate pointers to what to download.

### Docker (linux or windows)

The [esp-link docker image](https://hub.docker.com/r/jeelabs/esp-link/) contains all the
tools to build esp-link as well as the appropriate Espressif SDK. *It does not contain the
esp-link source code!*. You use the docker image just to build the firmware, you don't have
to do your editing in there. The steps are:
- clone the esp-link github repo
- checkout the branch or tag you want (for example the tag `v2.2.3` for that release)
- cd into the esp-link top directory
- run `make` in docker while mounting your esp-link directory into the container:
  - linux: `docker run -v $PWD:/esp-link jeelabs/esp-link:latest`
  - windows: `docker run -v c:\somepath\esp-link:/esp-link jeelabs/esp-link:latest`,
    where `somepath` is the path to where you cloned esp-link, you probably end up with
    something like `-v c:\Users\tve\source\esp-link:/esp-link`
- if you are not building esp-link `master` then read the release notes to see which version of
  the Espressif SDK you need and use that as tag for the container image, such as
  `jeelabs/esp-link:SDK2.0.0.p1`; you can see the list of available SDKs on
  [dockerhub](https://hub.docker.com/r/jeelabs/esp-link/tags/)o

Sample steps to build esp-link v2.2.3 on a Win7 Pro x64 (these use the docker terminal, there
are multiple way to skin the proverbial cat...):
1) Install Docker Toolbox ( http://www.docker.com/products/docker-toolbox )
2) Install Git Desktop ( https://desktop.github.com/ )
3) Clone esp-link from Github master to local repository ( https://github.com/jeelabs/esp-link )
4) Open Docker Quickstart Terminal
5) cd to local esp-link git repository ( C:\Users\xxxxx\Documents\GitHub\esp-link )
6) Run "docker run -v $PWD:/esp-link jeelabs/esp-link" command in Docker Quickstart Terminal window

Note: there has been one report of messed-up timestamps on windows, the symptom is that `make`
complains about file modification times being in the future. This may be due to the different
way Windows and Linux handle time zones and daylight savings time. PLease report if you
encounter this or know a solution.

### Linux

The firmware has been built using the https://github.com/pfalcon/esp-open-sdk[esp-open-sdk]
on a Linux system. Create an esp8266 directory, install the esp-open-sdk into a sub-directory
using the *non-standalone* install (i.e., there should not be an sdk directory in the esp-open-sdk
dir when done installing, *if you use the standalone install you will get compilation errors*
with std types, such as `uint32_t`).

Download the Espressif "NONOS" SDK (use the version mentioned in the release notes) from their
http://bbs.espressif.com/viewforum.php?f=5[download forum] and also expand it into a
sub-directory. Often there are patches to apply, in that case you need to download the patches
from the same source and apply them.

You can simplify your life (and avoid the hour-long build time for esp-open-sdk) if you are
on an x86 box by downloading the packaged and built esp-open-sdk and the fully patches SDKfrom the
links used in the `.travis.yaml`.

Clone the esp-link repository into a third sub-directory and check out the tag you would like,
such as `git checkout v2.2.3`.
This way the relative paths in the Makefile will work.
If you choose a different directory structure look at the top of the Makefile for the
appropriate environment variables to define.
Do not use the source tarballs from the release page on github,
these will give you trouble compiling because the Makefile uses git to determine the esp-link
version being built.

In order to OTA-update the esp8266 you should `export ESP_HOSTNAME=...` with the hostname or
IP address of your module.

Now, build the code: `make` in the top-level of esp-link. If you want to se the commands being
issued, use `VERBOSE=1 make`.

A few notes from others (I can't fully verify these):

- You may need to install `zlib1g-dev` and `python-serial`
- Make sure you have the correct version of the SDK
- Make sure the paths at the beginning of the makefile are correct
- Make sure `esp-open-sdk/xtensa-lx106-elf/bin` is in the PATH set in the Makefile

### Windows

Please consider installing docker and using the docker image to save yourself grief getting all
the tools installed and working.
If you do want to compile "natively" on Windows it certainly is possible.

It is possible to build esp-link on Windows, but it requires a gaggle of software to be installed:

- Install the unofficial sdk, mingw, SourceTree (gui git client), python 2.7, git cli, Java
- Use SourceTree to checkout under C:\espressif or wherever you installed the unofficial sdk,
  (see this thread for the unofficial sdk http://www.esp8266.com/viewtopic.php?t=820)
- Create a symbolic link under c:/espressif for the git bin directory under program files and
  the java bin directory under program files.
- ...

## Updating the firmware over-the-air

This firmware supports over-the-air (OTA) flashing, so you do not have to deal with serial
flashing again after the initial one! The recommended way to flash is to use `make wiflash`
if you are also building the firmware.
If you are downloading firmware binaries use `./wiflash`.
`make wiflash` assumes that you set `ESP_HOSTNAME` to the hostname or IP address of your esp-link.
You can easily do that using something like `ESP_HOSTNAME=192.168.1.5 make wiflash` or
`ESP_HOSTNAME=es-link.local make wiflash`.

The flashing, restart, and re-associating with your wireless network takes about 15 seconds
and is fully automatic. The first 1MB of flash are divided into two 512KB partitions allowing for new
code to be uploaded into one partition while running from the other. This is the official
OTA upgrade method supported by the SDK, except that the firmware is POSTed to the module
using curl as opposed to having the module download it from a cloud server. On a module with
512KB flash there is only space for one partition and thus no way to do an OTA update.

If you are downloading the prebuilt firmware image you will have both
`user1.bin` and `user2.bin` and run `wiflash.sh <esp-hostname> user1.bin user2.bin`.
This will query esp-link for which file it needs, upload the file, and then reconnect to
ensure all is well.

Note that when you flash the firmware the wifi settings are all preserved so esp-link should
reconnect to your network within a few seconds and the whole flashing process should take 15-30
from beginning to end. If you need to clear the wifi settings you need to reflash the `blank.bin`
using the serial port.

The flash configuration and the OTA upgrade process is described in more detail
in [FLASH.md](FLASH.md). If OTA flashing doesn't work for you the serial flashing is described
in [FLASHING.md](FLASHING.md).
