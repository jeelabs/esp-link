#ifndef ESPFS_H
#define ESPFS_H

#include "espfsformat.h"

typedef enum {
	ESPFS_INIT_RESULT_OK,
	ESPFS_INIT_RESULT_NO_IMAGE,
	ESPFS_INIT_RESULT_BAD_ALIGN,
} EspFsInitResult;

typedef enum {
	ESPFS_MEMORY,
	ESPFS_FLASH,
} EspFsSource;

typedef struct EspFsFile EspFsFile;
typedef struct EspFsContext EspFsContext;

typedef struct {
	EspFsHeader   header;
	EspFsContext *ctx;
	char          name[256];
	char         *node;
	char *        p;
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