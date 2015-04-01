#ifndef ESPFS_H
#define ESPFS_H

//Define this if you want to be able to use Heatshrink-compressed espfs images.
#define ESPFS_HEATSHRINK

//Pos of esp fs in flash
#define ESPFS_POS  0x12000
#define ESPFS_SIZE 0x2E000


typedef struct EspFsFile EspFsFile;

EspFsFile *espFsOpen(char *fileName);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);


#endif