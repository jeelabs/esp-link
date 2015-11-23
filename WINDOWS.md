* Install [SourceTree](https://www.sourcetreeapp.com) and check CLI git or other git distribution to obtain git from CLI
* Install the latest Java JRE
* Install Python 2.7 to C:\Python27
* Install link shell extension from [here](http://schinagl.priv.at/nt/hardlinkshellext/linkshellextension.html)
* Download and install the [Windows Unofficial Development Kit for Espressif ESP8266](http://programs74.ru/get.php?file=EspressifESP8266DevKit) to c:\espressif
* Create a symbolic link for java/bin and git/bin directories under C:\espressif\git-bin and C:\espressif\java-bin. You must do this because "make" doesn't work properly with paths like "program files(x86)".  You can see all the expected paths in the [espmake.cmd](https://github.com/jeelabs/esp-link/blob/master/espmake.cmd)
* [Download](http://sourceforge.net/projects/mingw/files/Installer/) and install MinGW. Run mingw-get-setup.exe.  During the installation process select without GUI.  (uncheck "... also install support for the graphical user interface")
* [Download](http://programs74.ru/get.php?file=EspressifESP8266DevKitAddon) the scripts to automate the installation of additional modules for MinGW.
* Run install-mingw-package.bat. This will install the basic modules required for MinGW to build esp8266.
* Checkout esp-link from git to C:\espressif\esp-link 
* When you're done open a command prompt and run: espmake.cmd "make all wiflash" 
* For a new flash over serial use: espmake.cmd "make all flash"
* If you want to program with serial but not loose your config each time use: espmake.cmd "make all baseflash"
* You can open the esp-link.sln file in Visual Studio 2013.  "Build Solution" will issue "make all wiflash".  "Clean Solution" will issue "make clean".  "Rebuild Solution" will issue "make clean all".  This can be changed under solution properties -> Configuration Properties -> NMake