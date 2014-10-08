#include "driver/uart.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "../mkespfsimage/espfsformat.h"
#include "espfs.h"

struct EspFsFile {
	EspFsHeader *header;
	char decompressor;
	int32_t posDecomp;
	char *posStart;
	char *posComp;
	void *decompData;
};

/*
Available locations, at least in my flash, with boundaries partially guessed:
0x00000 (0x10000): Code/data (RAM data?)
0x10000 (0x30000): Free (filled with zeroes) (parts used by ESPCloud and maybe SSL)
0x40000 (0x20000): Code/data (ROM data?)
0x60000 (0x1C000): Free
0x7c000 (0x04000): Param store
0x80000 - end of flash

Accessing the flash through the mem emulation at 0x40200000 is a bit hairy: All accesses
*must* be aligned 32-bit accesses. Reading a short, byte or unaligned word will result in
a memory exception, crashing the program.
*/


//Copies len bytes over from dst to src, but does it using *only*
//aligned 32-bit reads.
void memcpyAligned(char *dst, char *src, int len) {
	int x;
	int w, b;
	for (x=0; x<len; x++) {
		b=((int)src&3);
		w=*((int *)(src-b));
		if (b==0) *dst=(w>>0);
		if (b==1) *dst=(w>>8);
		if (b==2) *dst=(w>>16);
		if (b==3) *dst=(w>>24);
		dst++; src++;
	}
}



EspFsFile *espFsOpen(char *fileName) {
	char *p=(char *)(ESPFS_POS+0x40200000);
	char *hpos;
	char namebuf[256];
	EspFsHeader h;
	EspFsFile *r;
	//Skip initial slashes
	while(fileName[0]=='/') fileName++;
	//Go find that file!
	while(1) {
		hpos=p;
		os_memcpy(&h, p, sizeof(EspFsHeader));
		//ToDo: check magic
		if (h.flags&FLAG_LASTFILE) {
//			os_printf("End of archive.\n");
			return NULL;
		}
		p+=sizeof(EspFsHeader);
		os_memcpy(namebuf, p, sizeof(namebuf));
//		os_printf("Found file %s . Namelen=%x fileLen=%x\n", namebuf, h.nameLen,h.fileLenComp);
		if (os_strcmp(namebuf, fileName)==0) {
			p+=h.nameLen;
			r=(EspFsFile *)os_malloc(sizeof(EspFsFile));
			if (r==NULL) return NULL;
			r->header=(EspFsHeader *)hpos;
			r->decompressor=h.compression;
			r->posComp=p;
			r->posStart=p;
			r->posDecomp=0;
			r->decompData=NULL;
			//ToDo: Init any decompressor
			return r;
		}
		//Skip name and file
		p+=h.nameLen+h.fileLenComp;
		if ((int)p&3) p+=4-((int)p&3); //align to next 32bit val
//		os_printf("Next addr = %x\n", (int)p);
	}
}


int espFsRead(EspFsFile *fh, char *buff, int len) {
	if (fh==NULL) return 0;
	if (fh->decompressor==COMPRESS_NONE) {
		int toRead;
		toRead=fh->header->fileLenComp-(fh->posComp-fh->posStart);
		if (len>toRead) len=toRead;
//		os_printf("Reading %d bytes from %x\n", len, fh->posComp);
		memcpyAligned(buff, fh->posComp, len);
		fh->posDecomp+=len;
		fh->posComp+=len;
//		os_printf("Done reading %d bytes, pos=%x\n", len, fh->posComp);
		return len;
	}
}

void espFsClose(EspFsFile *fh) {
	if (fh==NULL) return;
	os_free(fh);
}



