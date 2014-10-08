
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>
#include "espfsformat.h"

//Routines to convert host format to the endianness used in the xtensa
short htoxs(short in) {
	char r[2];
	r[0]=in;
	r[1]=in>>8;
	return *((short *)r);
}

int htoxl(int in) {
	unsigned char r[4];
	r[0]=in;
	r[1]=in>>8;
	r[2]=in>>16;
	r[3]=in>>24;
	return *((int *)r);
}


void handleFile(int f, char *name) {
	char *fdat;
	off_t size;
	EspFsHeader h;
	int nameLen;
	size=lseek(f, 0, SEEK_END);
	fdat=mmap(NULL, size, PROT_READ, MAP_SHARED, f, 0);
	if (fdat==MAP_FAILED) {
		perror("mmap");
		return;
	}
	
	//Fill header data
	h.magic=('E'<<0)+('S'<<8)+('f'<<16)+('s'<<24);
	h.flags=0;
	h.compression=COMPRESS_NONE;
	nameLen=strlen(name)+1;
	if (nameLen&3) nameLen+=4-(nameLen&3); //Round to next 32bit boundary
	h.nameLen=htoxs(nameLen);
	h.fileLenComp=htoxl(size);
	h.fileLenDecomp=htoxl(size);
	
	write(1, &h, sizeof(EspFsHeader));
	write(1, name, nameLen); //ToDo: this can eat up a few bytes after the buffer.
	write(1, fdat, size);
	//Pad out to 32bit boundary
	while (size&3) {
		write(1, "\000", 1);
		size++;
	}
	munmap(fdat, size);
}

//Write final dummy header with FLAG_LASTFILE set.
void finishArchive() {
	EspFsHeader h;
	h.magic=('E'<<0)+('S'<<8)+('f'<<16)+('s'<<24);
	h.flags=FLAG_LASTFILE;
	h.compression=COMPRESS_NONE;
	h.nameLen=htoxs(0);
	h.fileLenComp=htoxl(0);
	h.fileLenDecomp=htoxl(0);
	write(1, &h, sizeof(EspFsHeader));
}


int main(int argc, char **argv) {
	int f;
	char fileName[1024];
	char *realName;
	struct stat statBuf;
	int serr;
	while(fgets(fileName, sizeof(fileName), stdin)) {
		//Kill off '\n' at the end
		fileName[strlen(fileName)-1]=0;
		//Only include files
		serr=stat(fileName, &statBuf);
		if ((serr==0) && S_ISREG(statBuf.st_mode)) {
			//Strip off './' or '/' madness.
			realName=fileName;
			if (fileName[0]=='.') realName++;
			if (realName[0]=='/') realName++;
			f=open(fileName, O_RDONLY);
			if (f>0) {
				handleFile(f, realName);
				fprintf(stderr, "%s\n", realName);
				close(f);
			} else {
				perror(fileName);
			}
		} else {
			if (serr!=0) {
				perror(fileName);
			}
		}
	}
	finishArchive();
}

