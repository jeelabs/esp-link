#ifndef ESPFS_H
#define ESPFS_H

#include <stdbool.h>

typedef enum {
	ESPFS_INIT_RESULT_OK,
	ESPFS_INIT_RESULT_NO_IMAGE,
	ESPFS_INIT_RESULT_BAD_ALIGN,
} EspFsInitResult;

typedef struct EspFsFile EspFsFile;

bool espFsIsImage(const void* const flashAddress);
EspFsInitResult espFsInit(void *flashAddress);
EspFsFile *espFsOpen(char *fileName);
int espFsFlags(EspFsFile *fh);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);


#endif
