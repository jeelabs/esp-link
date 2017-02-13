#ifndef ESPFS_H
#define ESPFS_H

#include "espfsformat.h"

typedef enum {
	ESPFS_INIT_RESULT_OK,
	ESPFS_INIT_RESULT_NO_IMAGE,
	ESPFS_INIT_RESULT_BAD_ALIGN,
} EspFsInitResult;

// Only 1 MByte of the flash can be directly accessed with ESP8266
// If flash size is >1 Mbyte, SDK API is required to retrieve flash content
typedef enum {
	ESPFS_MEMORY, // read data directly from memory (fast, max 1 MByte)
	ESPFS_FLASH,  // read data from flash using SDK API (no limit for the size)
} EspFsSource;

typedef struct EspFsFile EspFsFile;
typedef struct EspFsContext EspFsContext;

typedef struct {
	EspFsHeader   header;      // the header of the current file
	EspFsContext *ctx;         // pointer to espfs context
	char          name[256];   // the name of the current file
	char         *position;    // position of the iterator (pointer on the file system)
} EspFsIterator;

extern EspFsContext * espLinkCtx;
extern EspFsContext * userPageCtx;

EspFsInitResult espFsInit(EspFsContext *ctx, void *flashAddress, EspFsSource source);
EspFsFile *espFsOpen(EspFsContext *ctx, char *fileName);
int espFsIsValid(EspFsContext *ctx);
int espFsFlags(EspFsFile *fh);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);

void espFsIteratorInit(EspFsContext *ctx, EspFsIterator *iterator);
int espFsIteratorNext(EspFsIterator *iterator);

#endif