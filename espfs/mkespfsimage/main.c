#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "espfs.h"
#ifdef __MINGW32__
#include "mman-win32/mman.h"
#else
#include <sys/mman.h>
#endif
#ifdef __WIN32__
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "espfsformat.h"

//Gzip
#ifdef ESPFS_GZIP
// If compiler complains about missing header, try running "sudo apt-get install zlib1g-dev" 
// to install missing package.
#include <zlib.h>
#endif



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

#ifdef ESPFS_GZIP
size_t compressGzip(char *in, int insize, char *out, int outsize, int level) {
	z_stream stream;
	int zresult;

	stream.zalloc = Z_NULL;
	stream.zfree  = Z_NULL;
	stream.opaque = Z_NULL;
	stream.next_in = in;
	stream.avail_in = insize;
	stream.next_out = out;
	stream.avail_out = outsize;
	// 31 -> 15 window bits + 16 for gzip
	zresult = deflateInit2 (&stream, level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	if (zresult != Z_OK) {
		fprintf(stderr, "DeflateInit2 failed with code %d\n", zresult);
		exit(1);
	}

	zresult = deflate(&stream, Z_FINISH);
	if (zresult != Z_STREAM_END) {
		fprintf(stderr, "Deflate failed with code %d\n", zresult);
		exit(1);
	}

	zresult = deflateEnd(&stream);
	if (zresult != Z_OK) {
		fprintf(stderr, "DeflateEnd failed with code %d\n", zresult);
		exit(1);
	}

	return stream.total_out;
}

char **gzipExtensions = NULL;

int shouldCompressGzip(char *name) {
	char *ext = name + strlen(name);
	while (*ext != '.') {
		ext--;
		if (ext < name) {
			// no dot in file name -> no extension -> nothing to match against
			return 0;
		}
	}
	ext++;

	int i = 0;
	while (gzipExtensions[i] != NULL) {
		if (strcmp(ext,gzipExtensions[i]) == 0) {
			return 1;
		}
		i++;
	}

	return 0;
}

int parseGzipExtensions(char *input) {
	char *token;
	char *extList = input;
	int count = 2; // one for first element, second for terminator

	// count elements
	while (*extList != 0) {
		if (*extList == ',') count++;
		extList++;
	}

	// split string
	extList = input;
	gzipExtensions = malloc(count * sizeof(char*));
	count = 0;
	token = strtok(extList, ",");
	while (token) {
		gzipExtensions[count++] = token;
		token = strtok(NULL, ",");
	}
	// terminate list
	gzipExtensions[count] = NULL;

	return 1;
}
#endif

int handleFile(int f, char *name, int compression, int level, char **compName, off_t *csizePtr) {
	char *fdat, *cdat;
	off_t size, csize;
	EspFsHeader h;
	int nameLen;
	int8_t flags = 0;
	size=lseek(f, 0, SEEK_END);
	fdat=mmap(NULL, size, PROT_READ, MAP_SHARED, f, 0);
	if (fdat==MAP_FAILED) {
		perror("mmap");
		return 0;
	}

#ifdef ESPFS_GZIP
	if (shouldCompressGzip(name)) {
		csize = size*3;
		if (csize<100) // gzip has some headers that do not fit when trying to compress small files
			csize = 100; // enlarge buffer if this is the case
		cdat=malloc(csize);
		csize=compressGzip(fdat, size, cdat, csize, level);
		compression = COMPRESS_NONE;
		flags = FLAG_GZIP;
	} else
#endif
	if (compression==COMPRESS_NONE) {
		csize=size;
		cdat=fdat;
	} else {
		fprintf(stderr, "Unknown compression - %d\n", compression);
		exit(1);
	}

	if (csize>size) {
		//Compressing enbiggened this file. Revert to uncompressed store.
		compression=COMPRESS_NONE;
		csize=size;
		cdat=fdat;
		flags=0;
	}

	//Fill header data
	h.magic=('E'<<0)+('S'<<8)+('f'<<16)+('s'<<24);
	h.flags=flags;
	h.compression=compression;
	h.nameLen=nameLen=strlen(name)+1;
	if (h.nameLen&3) h.nameLen+=4-(h.nameLen&3); //Round to next 32bit boundary
	h.nameLen=htoxs(h.nameLen);
	h.fileLenComp=htoxl(csize);
	h.fileLenDecomp=htoxl(size);

	write(1, &h, sizeof(EspFsHeader));
	write(1, name, nameLen);
	while (nameLen&3) {
		write(1, "\000", 1);
		nameLen++;
	}
	write(1, cdat, csize);
	//Pad out to 32bit boundary
	while (csize&3) {
		write(1, "\000", 1);
		csize++;
	}
	munmap(fdat, size);

	if (compName != NULL) {
		if (h.compression==COMPRESS_NONE) {
			if (h.flags & FLAG_GZIP) {
				*compName = "gzip";
			} else {
				*compName = "none";
			}
		} else {
			*compName = "unknown";
		}
	}
  *csizePtr = csize;
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
	int compType;  //default compression type - heatshrink
	int compLvl=-1;

	compType = COMPRESS_NONE;

	for (x=1; x<argc; x++) {
		if (strcmp(argv[x], "-c")==0 && argc>=x-2) {
			compType=atoi(argv[x+1]);
			x++;
		} else if (strcmp(argv[x], "-l")==0 && argc>=x-2) {
			compLvl=atoi(argv[x+1]);
			if (compLvl<1 || compLvl>9) err=1;
			x++;
#ifdef ESPFS_GZIP
		} else if (strcmp(argv[x], "-g")==0 && argc>=x-2) {
			if (!parseGzipExtensions(argv[x+1])) err=1;
			x++;
#endif
		} else {
			err=1;
		}
	}

#ifdef ESPFS_GZIP
	if (gzipExtensions == NULL) {
		parseGzipExtensions(strdup("html,css,js,ico"));
	}
#endif

	if (err) {
		fprintf(stderr, "%s - Program to create espfs images\n", argv[0]);
		fprintf(stderr, "Usage: \nfind | %s [-c compressor] [-l compression_level] ", argv[0]);
#ifdef ESPFS_GZIP
		fprintf(stderr, "[-g gzipped_extensions] ");
#endif
		fprintf(stderr, "> out.espfs\n");
		fprintf(stderr, "Compressors:\n");
		fprintf(stderr, "0 - None(default)\n");
		fprintf(stderr, "\nCompression level: 1 is worst but low RAM usage, higher is better compression \nbut uses more ram on decompression. -1 = compressors default.\n");
#ifdef ESPFS_GZIP
		fprintf(stderr, "\nGzipped extensions: list of comma separated, case sensitive file extensions \nthat will be gzipped. Defaults to 'html,css,js'\n");
#endif
		exit(0);
	}

#ifdef __WIN32__
	setmode(fileno(stdout), _O_BINARY);
#endif

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
				char *compName = "unknown";
        off_t csize;
				rate=handleFile(f, realName, compType, compLvl, &compName, &csize);
				fprintf(stderr, "%-16s (%3d%%, %s, %4u bytes)\n", realName, rate, compName, (uint32_t)csize);
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

