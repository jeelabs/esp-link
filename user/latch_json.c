#include "latch_json.h"
#include "user_funcs.h"
#include <json/jsontree.h>
#include <json/jsonparse.h>
#include <cmd.h>

static LatchState latch;

static void ICACHE_FLASH_ATTR
updateLatch() {
  os_printf("ESP: Latch Callback\n");
  cmdCallback* latchCb = CMD_GetCbByName("Latch");
  if (latchCb->callback != -1) {
    uint16_t crc = CMD_ResponseStart(CMD_CB_EVENTS, (uint32_t)&latchCb->callback, 0, 1);
    crc = CMD_ResponseBody(crc, (uint8_t*)&latch, sizeof(LatchState));
    CMD_ResponseEnd(crc);
  }
}

static int ICACHE_FLASH_ATTR
latchGet(struct jsontree_context *js_ctx) {
  return 0;
}

static int ICACHE_FLASH_ATTR
latchSet(struct jsontree_context *js_ctx, struct jsonparse_state *parser) {
  int type;
  int ix = -1;
  while ((type = jsonparse_next(parser)) != 0) {
    if (type == JSON_TYPE_ARRAY) {
      ix = -1;
    }
    else if (type == JSON_TYPE_OBJECT) {
      ix++;
    }
    else if (type == JSON_TYPE_PAIR_NAME) {
      if (jsonparse_strcmp_value(parser, "states") == 0) {
        char latchStates[9];
        jsonparse_next(parser); jsonparse_next(parser);
        jsonparse_copy_value(parser, latchStates, sizeof(latchStates));
        os_printf("latch states %s\n", latchStates);
        uint8_t states = binToByte(latchStates);
        latch.stateBits = states;
      }
      else if (jsonparse_strcmp_value(parser, "fallbackstates") == 0) {
        char fallbackStates[9];
        jsonparse_next(parser); jsonparse_next(parser);
        jsonparse_copy_value(parser, fallbackStates, sizeof(fallbackStates));
        os_printf("latch states %s\n", fallbackStates);
        uint8_t fbstates = binToByte(fallbackStates);
        latch.fallbackStateBits = fbstates;
      }
    }
  }
  return 0;
}

static struct jsontree_callback latchCallback = JSONTREE_CALLBACK(latchGet, latchSet);
static char* latchQueueName;

JSONTREE_OBJECT(latchJsonObj,
  JSONTREE_PAIR("states", &latchCallback),
  JSONTREE_PAIR("fallbackstates", &latchCallback));

