/*
This is a simple read-only implementation of a file system. It uses a block of data coming from the
mkespfsimg tool, and can use that block to do abstracted operations on the files that are in there.
It's written for use with httpd, but doesn't need to be used as such.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
 * this notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return.
 * ----------------------------------------------------------------------------
 */


//These routines can also be tested by comping them in with the espfstest tool. This
//simplifies debugging, but needs some slightly different headers. The #ifdef takes
//care of that.

#ifdef __ets__
//esp build
#include <esp8266.h>
#else
//Test build
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define os_malloc malloc
#define os_free free
#define os_memcpy memcpy
#define os_memset memset
#define os_strncmp strncmp
#define os_strcmp strcmp
#define os_strcpy strcpy
#define os_printf printf
#define ICACHE_FLASH_ATTR
#endif

#include "espfsformat.h"
#include "espfs.h"

EspFsContext espLinkCtxDef;
EspFsContext userPageCtxDef;

EspFsContext * espLinkCtx = &espLinkCtxDef;
EspFsContext * userPageCtx = &userPageCtxDef;

struct EspFsContext
{
	char*       data;
	EspFsSource source;
	uint8_t     valid;
};

struct EspFsFile {
	EspFsContext *ctx;
	EspFsHeader *header;
	char decompressor;
	int32_t posDecomp;
	char *posStart;
	char *posComp;
	void *decompData;
};

/*
Available locations, at least in my flash, with boundaries partially guessed. This
is using 0.9.1/0.9.2 SDK on a not-too-new module.
0x00000 (0x10000): Code/data (RAM data?)
0x10000 (0x02000): Gets erased by something?
0x12000 (0x2E000): Free (filled with zeroes) (parts used by ESPCloud and maybe SSL)
0x40000 (0x20000): Code/data (ROM data?)
0x60000 (0x1C000): Free
0x7c000 (0x04000): Param store
0x80000 - end of flash

Accessing the flash through the mem emulation at 0x40200000 is a bit hairy: All accesses
*must* be aligned 32-bit accesses. Reading a short, byte or unaligned word will result in
a memory exception, crashing the program.
*/

//Copies len bytes over from dst to src, but does it using *only*
//aligned 32-bit reads. Yes, it's no too optimized but it's short and sweet and it works.

//ToDo: perhaps os_memcpy also does unaligned accesses?
#ifdef __ets__
void ICACHE_FLASH_ATTR memcpyAligned(char *dst, const char *src, int len) {
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
#else
#define memcpyAligned memcpy
#endif

void ICACHE_FLASH_ATTR memcpyFromFlash(char *dst, const char *src, int len)
{
	if( spi_flash_read( (int)src, (void *)dst, len ) != SPI_FLASH_RESULT_OK )
		os_memset( dst, 0, len ); // if read was not successful, reply with zeroes
}

// memcpy on MEMORY/FLASH file systems
void espfs_memcpy( EspFsContext * ctx, void * dest, const void * src, int count )
{
	if( ctx->source == ESPFS_MEMORY )
		os_memcpy( dest, src, count );
	else
		memcpyFromFlash(dest, src, count);
}

// aligned memcpy on MEMORY/FLASH file systems
void espfs_memcpyAligned( EspFsContext * ctx, void * dest, const void * src, int count )
{
	if( ctx->source == ESPFS_MEMORY )
		memcpyAligned(dest, src, count);
	else
		memcpyFromFlash(dest, src, count);
}

// initializes an EspFs context
EspFsInitResult ICACHE_FLASH_ATTR espFsInit(EspFsContext *ctx, void *flashAddress, EspFsSource source) {
	ctx->valid = 0;
	ctx->source = source;
	// base address must be aligned to 4 bytes
	if (((int)flashAddress & 3) != 0) {
		return ESPFS_INIT_RESULT_BAD_ALIGN;
	}

	// check if there is valid header at address
	EspFsHeader testHeader;
	espfs_memcpy(ctx, &testHeader, flashAddress, sizeof(EspFsHeader));
	if (testHeader.magic != ESPFS_MAGIC) {
		return ESPFS_INIT_RESULT_NO_IMAGE;
	}

	ctx->data = (char *)flashAddress;
	ctx->valid = 1;
	return ESPFS_INIT_RESULT_OK;
}

// Returns flags of opened file.
int ICACHE_FLASH_ATTR espFsFlags(EspFsFile *fh) {
	if (fh == NULL) {
#ifdef ESPFS_DBG
		os_printf("File handle not ready\n");
#endif
		return -1;
	}

	int8_t flags;
	espfs_memcpyAligned(fh->ctx, (char*)&flags, (char*)&fh->header->flags, 1);
	return (int)flags;
}

// creates and initializes an iterator over the espfs file system
void ICACHE_FLASH_ATTR espFsIteratorInit(EspFsContext *ctx, EspFsIterator *iterator)
{
	if( ctx->data == NULL )
	{
		iterator->ctx = NULL;
		return;
	}
	iterator->ctx = ctx;
	iterator->position = NULL;
}

// moves iterator to the next file on espfs
// returns 1 if iterator move was successful, otherwise 0 (last file)
// iterator->header and iterator->name will contain file information
int ICACHE_FLASH_ATTR espFsIteratorNext(EspFsIterator *iterator)
{
	if( iterator->ctx == NULL )
		return 0;
	
	char * position = iterator->position;
	if( position == NULL )
		position = iterator->ctx->data; // first node
	else
	{
		// jump the iterator to the next file
		
		position+=sizeof(EspFsHeader) + iterator->header.nameLen+iterator->header.fileLenComp;
		if ((int)position&3) position+=4-((int)position&3); //align to next 32bit val
	}
	
	iterator->position = position;
	EspFsHeader * hdr = &iterator->header;
	espfs_memcpy(iterator->ctx, hdr, position, sizeof(EspFsHeader));
	
	if (hdr->magic!=ESPFS_MAGIC) {
#ifdef ESPFS_DBG
		os_printf("Magic mismatch. EspFS image broken.\n");
#endif
		return 0;
	}
	if (hdr->flags&FLAG_LASTFILE) {
		//os_printf("End of image.\n");
		iterator->ctx = NULL; // invalidate the iterator
		return 0;
	}
	
	position += sizeof(EspFsHeader);
	
	//Grab the name of the file.
	espfs_memcpy(iterator->ctx, iterator->name, position, sizeof(iterator->name));
	
	return 1;
}

//Open a file and return a pointer to the file desc struct.
EspFsFile ICACHE_FLASH_ATTR *espFsOpen(EspFsContext *ctx, char *fileName) {
	EspFsIterator it;
	espFsIteratorInit(ctx, &it);
	if (it.ctx == NULL) {
#ifdef ESPFS_DBG
		os_printf("Call espFsInit first!\n");
#endif
		return NULL;
	}
	//Strip initial slashes
	while(fileName[0]=='/') fileName++;
	
	//Search the file
	while( espFsIteratorNext(&it) ) 
	{
		if (os_strcmp(it.name, fileName)==0) {
			//Yay, this is the file we need!
			EspFsFile * r=(EspFsFile *)os_malloc(sizeof(EspFsFile)); //Alloc file desc mem
			//os_printf("Alloc %p[%d]\n", r, sizeof(EspFsFile));
			if (r==NULL) return NULL;
			r->ctx = ctx;
			r->header=(EspFsHeader *)it.position;
			r->decompressor=it.header.compression;
			r->posComp=it.position + it.header.nameLen  + sizeof(EspFsHeader);
			r->posStart=it.position + it.header.nameLen  + sizeof(EspFsHeader);
			r->posDecomp=0;
			if (it.header.compression==COMPRESS_NONE) {
				r->decompData=NULL;
			} else {
#ifdef ESPFS_DBG
				os_printf("Invalid compression: %d\n", h.compression);
#endif
				return NULL;
			}
			return r;
		}
	}
	return NULL;
}

//Read len bytes from the given file into buff. Returns the actual amount of bytes read.
int ICACHE_FLASH_ATTR espFsRead(EspFsFile *fh, char *buff, int len) {
	int flen, fdlen;
	if (fh==NULL) return 0;
	//Cache file length.
	espfs_memcpyAligned(fh->ctx, (char*)&flen, (char*)&fh->header->fileLenComp, 4);
	espfs_memcpyAligned(fh->ctx, (char*)&fdlen, (char*)&fh->header->fileLenDecomp, 4);
	//Do stuff depending on the way the file is compressed.
	if (fh->decompressor==COMPRESS_NONE) {
		int toRead;
		toRead=flen-(fh->posComp-fh->posStart);
		if (len>toRead) len=toRead;
//		os_printf("Reading %d bytes from %x\n", len, (unsigned int)fh->posComp);
		espfs_memcpyAligned(fh->ctx, buff, fh->posComp, len);
		fh->posDecomp+=len;
		fh->posComp+=len;
//		os_printf("Done reading %d bytes, pos=%x\n", len, fh->posComp);
		return len;
	}
	return 0;
}

//Close the file.
void ICACHE_FLASH_ATTR espFsClose(EspFsFile *fh) {
	if (fh==NULL) return;
	//os_printf("Freed %p\n", fh);
	os_free(fh);
}

// checks if the file system is valid (detect if the content is an espfs image or random data)
int ICACHE_FLASH_ATTR espFsIsValid(EspFsContext *ctx) {
	return ctx->valid;
}

