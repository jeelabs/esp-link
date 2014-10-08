#ifndef ESPFS_H
#define ESPFS_H

//Pos of esp fs in flash
#define ESPFS_POS 0x20000

typedef struct EspFsFile EspFsFile;

EspFsFile *espFsOpen(char *fileName);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);


#endif