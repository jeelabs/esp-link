#ifndef PAGES_H
#define PAGES_H

void ledHtmlCallback(WebServerCommand command, char * data, int dataLen);
void ledLoop();
void ledInit();

void userHtmlCallback(WebServerCommand command, char * data, int dataLen);
void userInit();

#endif /* PAGES_H */


