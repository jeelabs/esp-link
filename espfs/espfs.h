#ifndef ESPFS_H
#define ESPFS_H

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

extern EspFsContext * espLinkCtx;
extern EspFsContext * userCtx;

EspFsInitResult espFsInit(EspFsContext *ctx, void *flashAddress, EspFsSource source);
EspFsFile *espFsOpen(EspFsContext *ctx, char *fileName);
int espFsFlags(EspFsFile *fh);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);


#endif