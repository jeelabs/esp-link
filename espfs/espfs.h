#ifndef ESPFS_H
#define ESPFS_H

//Define this if you want to be able to use Heatshrink-compressed espfs images.
#define ESPFS_HEATSHRINK

typedef enum {
	ESPFS_INIT_RESULT_OK,
	ESPFS_INIT_RESULT_NO_IMAGE,
	ESPFS_INIT_RESULT_BAD_ALIGN,
} EspFsInitResult;

typedef struct EspFsFile EspFsFile;

EspFsInitResult espFsInit(void *flashAddress);
EspFsFile *espFsOpen(char *fileName);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);


#endif