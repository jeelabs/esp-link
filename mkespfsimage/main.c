#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <string.h>
#include "espfsformat.h"

//Heatshrink
#include "heatshrink_common.h"
#include "heatshrink_config.h"
#include "heatshrink_encoder.h"


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

size_t compressHeatshrink(char *in, int insize, char *out, int outsize, int level) {
	char *inp=in;
	char *outp=out;
	int len;
	int ws[]={5, 6, 8, 11, 13};
	int ls[]={3, 3, 4, 4, 4};
	HSE_poll_res pres;
	HSE_sink_res sres;
	size_t r;
	if (level==-1) level=8;
	level=(level-1)/2; //level is now 0, 1, 2, 3, 4
	heatshrink_encoder *enc=heatshrink_encoder_alloc(ws[level], ls[level]);
	if (enc==NULL) {
		perror("allocating mem for heatshrink");
		exit(1);
	}
	//Save encoder parms as first byte
	*outp=(ws[level]<<4)|ls[level];
	outp++; outsize--;

	r=1;
	do {
		if (insize>0) {
			sres=heatshrink_encoder_sink(enc, inp, insize, &len);
			if (sres!=HSER_SINK_OK) break;
			inp+=len; insize-=len;
			if (insize==0) heatshrink_encoder_finish(enc);
		}
		do {
			pres=heatshrink_encoder_poll(enc, outp, outsize, &len);
			if (pres!=HSER_POLL_MORE && pres!=HSER_POLL_EMPTY) break;
			outp+=len; outsize-=len;
			r+=len;
		} while (pres==HSER_POLL_MORE);
	} while (insize!=0);

	if (insize!=0) {
		fprintf(stderr, "Heatshrink: Bug? insize is still %d. sres=%d pres=%d\n", insize, sres, pres);
		exit(1);
	}

	heatshrink_encoder_free(enc);
	return r;
}

int handleFile(int f, char *name, int compression, int level) {
	char *fdat, *cdat;
	off_t size, csize;
	EspFsHeader h;
	int nameLen;
	size=lseek(f, 0, SEEK_END);
	fdat=mmap(NULL, size, PROT_READ, MAP_SHARED, f, 0);
	if (fdat==MAP_FAILED) {
		perror("mmap");
		return 0;
	}
	
	if (compression==COMPRESS_NONE) {
		csize=size;
		cdat=fdat;
	} else if (compression==COMPRESS_HEATSHRINK) {
		cdat=malloc(size*2);
		csize=compressHeatshrink(fdat, size, cdat, size*2, level);
	} else {
		fprintf(stderr, "Unknown compression - %d\n", compression);
		exit(1);
	}

	if (csize>size) {
		//Compressing enbiggened this file. Revert to uncompressed store.
		compression=COMPRESS_NONE;
		csize=size;
		cdat=fdat;
	}

	//Fill header data
	h.magic=('E'<<0)+('S'<<8)+('f'<<16)+('s'<<24);
	h.flags=0;
	h.compression=compression;
	nameLen=strlen(name)+1;
	if (nameLen&3) nameLen+=4-(nameLen&3); //Round to next 32bit boundary
	h.nameLen=htoxs(nameLen);
	h.fileLenComp=htoxl(csize);
	h.fileLenDecomp=htoxl(size);
	
	write(1, &h, sizeof(EspFsHeader));
	write(1, name, nameLen); //ToDo: this can eat up a few bytes after the buffer.
	write(1, cdat, csize);
	//Pad out to 32bit boundary
	while (csize&3) {
		write(1, "\000", 1);
		csize++;
	}
	munmap(fdat, size);
	return (csize*100)/size;
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
	int f, x;
	char fileName[1024];
	char *realName;
	struct stat statBuf;
	int serr;
	int rate;
	int err=0;
	int compType=1; //default compression type - heatshrink
	int compLvl=-1;

	for (x=1; x<argc; x++) {
		if (strcmp(argv[x], "-c")==0 && argc>=x-2) {
			compType=atoi(argv[x=1]);
			x++;
		} else if (strcmp(argv[x], "-l")==0 && argc>=x-2) {
			compLvl=atoi(argv[x=1]);
			if (compLvl<1 || compLvl>9) err=1;
			x++;
		} else {
			err=1;
		}
	}

	if (err) {
		fprintf(stderr, "%s - Program to create espfs images\n", argv[0]);
		fprintf(stderr, "Usage: \nfind | %s [-c compressor] [-l compression_level] > out.espfs\n", argv[0]);
		fprintf(stderr, "Compressors:\n0 - None\n1 - Heatshrink(defautl\n");
		fprintf(stderr, "Compression level: 1 is worst but low RAM usage, higher is better compression \nbut uses more ram on decompression. -1 = compressors default.\n");
		exit(0);
	}

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
				rate=handleFile(f, realName, compType, compLvl);
				fprintf(stderr, "%s (%d%%)\n", realName, rate);
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
	return 0;
}

