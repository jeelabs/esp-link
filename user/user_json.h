#ifndef __USER_JSON_H__
#define __USER_JSON_H__

#include <esp8266.h>
#include "json/jsonparse.h"
#include "json/jsontree.h"

#define JSON_SIZE   1024
void json_parse(struct jsontree_context* json, char* ptrJSONMessage);
void json_ws_send(struct jsontree_value* tree, const char* path, char* pbuf);
int json_putchar(int c);
struct jsontree_value* find_json_path(struct jsontree_context* json, const char* path);

#endif
