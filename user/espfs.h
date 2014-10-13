#ifndef ESPFS_H
#define ESPFS_H



typedef struct EspFsFile EspFsFile;

EspFsFile *espFsOpen(char *fileName);
int espFsRead(EspFsFile *fh, char *buff, int len);
void espFsClose(EspFsFile *fh);


#endif