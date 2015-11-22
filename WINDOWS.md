* Install [SourceTree](https://www.sourcetreeapp.com) and check CLI git or other git distribution to do git from CLI
* Install  and latest Java
* Install Python 2.7 to C:\Python27
* Install link shell extension from [here](http://schinagl.priv.at/nt/hardlinkshellext/linkshellextension.html)
* Download and install the [Windows Unofficial Development Kit for Espressif ESP8266](http://programs74.ru/get.php?file=EspressifESP8266DevKit) to c:\espressif
* Create a symbolic link for java/bin and git/bin directories under C:\espressif\git-bin and C:\espressif\java-bin. You have to do it like that because make doesn't like the full path with program files(x86).  You can see all the expected paths in the [espmake.cmd](https://github.com/jeelabs/esp-link/blob/master/espmake.cmd)
* [Download](http://sourceforge.net/projects/mingw/files/Installer/) and install MinGW. Run mingw-get-setup.exe, the installation process to select without GUI, ie uncheck "... also install support for the graphical user interface".
* [Download](http://programs74.ru/get.php?file=EspressifESP8266DevKitAddon) the scripts to automate the installation of additional modules for MinGW.
* Run install-mingw-package.bat. This will establish the basic modules for MinGW.  Installation should proceed without error.
* Checkout esp-link under C:\espressif\esp-link 
* espmake.cmd "make all wiflash" 
* new flash over serial then is espmake.cmd "make all flash"
* if you want to program with serial but not loose your config each time espmake.cmd "make all baseflash"