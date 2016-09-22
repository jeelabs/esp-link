#include <EEPROM.h>
#include "WebServer.h"

#define MAGIC 0xABEF

#define MAX_STR_LEN  32

#define POS_MAGIC 0
#define POS_FIRST_NAME (POS_MAGIC + 2)
#define POS_LAST_NAME (POS_FIRST_NAME + MAX_STR_LEN)
#define POS_AGE (POS_LAST_NAME + MAX_STR_LEN)
#define POS_GENDER (POS_AGE+1)
#define POS_NOTIFICATIONS (POS_GENDER+1)

void userInit()
{
  uint16_t magic;
  EEPROM.get(POS_MAGIC, magic);

  if( magic != MAGIC )
  {
    magic = MAGIC;
    EEPROM.put(POS_MAGIC, magic);
    EEPROM.update(POS_FIRST_NAME, 0);
    EEPROM.update(POS_LAST_NAME, 0);
    EEPROM.update(POS_AGE, 0);
    EEPROM.update(POS_GENDER, 'f');
    EEPROM.update(POS_NOTIFICATIONS, 0);
  }
}

void userWriteStr(char * str, int ndx)
{
  for(uint8_t i=0; i < MAX_STR_LEN-1; i++)
  {
    EEPROM.update(ndx + i, str[i]);
    if( str[i] == 0 )
      break;
  }
  EEPROM.update(ndx + MAX_STR_LEN - 1, 0);
}

void userReadStr(char * str, int ndx)
{
  for(uint8_t i=0; i < MAX_STR_LEN; i++)
  {
    str[i] = EEPROM[ndx + i];
  }
}

void userHtmlCallback(WebServerCommand command, char * data, int dataLen)
{
  switch(command)
  {
    case SET_FIELD:
      if( strcmp_P(data, PSTR("first_name")) == 0 )
        userWriteStr(webServer.getArgString(), POS_FIRST_NAME);
      if( strcmp_P(data, PSTR("last_name")) == 0 )
        userWriteStr(webServer.getArgString(), POS_LAST_NAME);
      if( strcmp_P(data, PSTR("age")) == 0 )
        EEPROM.update(POS_AGE, (uint8_t)webServer.getArgInt());
      if( strcmp_P(data, PSTR("gender")) == 0 )
        EEPROM.update(POS_GENDER, (strcmp_P(webServer.getArgString(), PSTR("male")) == 0 ? 'm' : 'f'));
      if( strcmp_P(data, PSTR("notifications")) == 0 )
        EEPROM.update(POS_NOTIFICATIONS, (uint8_t)webServer.getArgBoolean());
      break;
    case LOAD:
      {
        char buf[MAX_STR_LEN];
        userReadStr( buf, POS_FIRST_NAME );
        webServer.setArgString("first_name", buf);
        userReadStr( buf, POS_LAST_NAME );
        webServer.setArgString("last_name", buf);
        webServer.setArgInt("age", (uint8_t)EEPROM[POS_AGE]);
        webServer.setArgStringP("gender", (EEPROM[POS_GENDER] == 'm') ? PSTR("male") : PSTR("female"));
        webServer.setArgBoolean("notifications", EEPROM[POS_NOTIFICATIONS] != 0);
      }
      break;
    case REFRESH:
      // do nothing
      break;
  }
}

