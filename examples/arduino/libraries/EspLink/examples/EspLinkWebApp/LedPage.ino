#include "WebServer.h"

#define LED_PIN  13

int8_t  blinking = 0;
int8_t  frequency = 10;
uint8_t pattern = 2;
uint16_t elapse = 100;
uint16_t elapse_delta = 200;
uint32_t next_ts = 0;

#define MAX_LOGS 5
uint32_t log_ts[MAX_LOGS];
uint8_t  log_msg[MAX_LOGS];
uint8_t  log_ptr = 0;

void ledInit()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, false);
}

void ledLoop()
{ 
  if( blinking )
  {
    if( next_ts <= millis() )
    {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      next_ts += elapse;
      elapse = elapse_delta - elapse;
    }
  }
}

void ledAddLog(uint8_t msg)
{
  if( log_ptr >= MAX_LOGS )
    log_ptr = MAX_LOGS - 1;

  for(int8_t i=log_ptr-1; i >= 0; i--)
  {
    log_ts[i+1] = log_ts[i];
    log_msg[i+1] = log_msg[i];
  }
  log_msg[0] = msg;
  log_ts[0] = millis();
  log_ptr++;
}

void ledHistoryToLog(char * buf)
{
  buf[0] = 0;
  strcat(buf, "[");
  for(uint8_t i=0; i < log_ptr; i++)
  {
    if( i != 0 )
      strcat(buf, ",");
    
    char bf[20];
    sprintf(bf, "\"%lds: ", log_ts[i] / 1000);
    strcat(buf, bf);

    uint8_t msg = log_msg[i];
    if( msg == 0xE1 )
    {
      strcat_P(buf, PSTR("set pattern to 25%-75%"));
    }
    else if( msg == 0xE2 )
    {
      strcat_P(buf, PSTR("set pattern to 50%-50%"));
    }
    else if( msg == 0xE3 )
    {
      strcat_P(buf, PSTR("set pattern to 75%-25%"));
    }
    else if( msg == 0xF0 )
    {
      strcat_P(buf, PSTR("set led on"));
    }
    else if( msg == 0xF1 )
    {
      strcat_P(buf, PSTR("set led blinking"));
    }
    else if( msg == 0xF2 )
    {
      strcat_P(buf, PSTR("set led off"));
    }
    else
    {
      strcat_P(buf, PSTR("set frequency to "));
      sprintf(bf, "%d Hz", msg);
      strcat(buf, bf);
    }
    strcat(buf, "\"");
  }
  strcat(buf, "]");
}


void ledHtmlCallback(WebServerCommand command, char * data, int dataLen)
{
  switch(command)
  {
    case BUTTON_PRESS:
      if( strcmp_P(data, PSTR("btn_on") ) == 0 )
      {
        if( blinking || digitalRead(LED_PIN) == false )
          ledAddLog(0xF0);
        blinking = 0;
        digitalWrite(LED_PIN, true);
      } else if( strcmp_P(data, PSTR("btn_off") ) == 0 )
      {
        if( blinking || digitalRead(LED_PIN) == true )
          ledAddLog(0xF2);
        blinking = 0;
        digitalWrite(LED_PIN, false);
      } else if( strcmp_P(data, PSTR("btn_blink") ) == 0 )
      {
        if( !blinking )
          ledAddLog(0xF1);
        blinking = 1;
        next_ts = millis() + elapse;
      }
      break;
    case SET_FIELD:
      if( strcmp_P(data, PSTR("frequency") ) == 0 )
      {
        int8_t oldf = frequency;
        frequency = webServer.getArgInt();
        digitalWrite(LED_PIN, false);
        elapse_delta = 2000 / frequency;
        elapse = pattern * elapse_delta / 4;
        if( oldf != frequency )
          ledAddLog(frequency);
      }
      else if( strcmp_P(data, PSTR("pattern") ) == 0 )
      {
        int8_t oldp = pattern;
        char * arg = webServer.getArgString();

        if( strcmp_P(arg, PSTR("25_75")) == 0 )
          pattern = 1;
        else if( strcmp_P(arg, PSTR("50_50")) == 0 )
          pattern = 2;
        else if( strcmp_P(arg, PSTR("75_25")) == 0 )
          pattern = 3;

        digitalWrite(LED_PIN, false);
        elapse = pattern * elapse_delta / 4;

        if( oldp != pattern )
          ledAddLog(0xE0 + pattern);
      }
      break;
    case LOAD:
      webServer.setArgNum(4);
      webServer.setArgInt("frequency", frequency);

      switch(pattern)
      {
        case 1:
          webServer.setArgStringP("pattern", PSTR("25_75"));
          break;
        case 2:
          webServer.setArgStringP("pattern", PSTR("50_50"));
          break;
        case 3:
          webServer.setArgStringP("pattern", PSTR("75_25"));
          break;
      }
    case REFRESH:
      {
        if( command == REFRESH )
          webServer.setArgNum(2);

        if( blinking )
          webServer.setArgStringP("text", PSTR("LED is blinking"));
        else
          webServer.setArgStringP("text", digitalRead(LED_PIN) ? PSTR("LED is turned on") : PSTR("LED is turned off"));

        char buf[255];
        ledHistoryToLog(buf);
        webServer.setArgJson("led_history", buf);
      }
      break;
    default:
      break;
  }
}


