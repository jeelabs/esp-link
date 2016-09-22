#include "WebServer.h"

#include <avr/io.h>

#define SAMPLE_COUNT 100
#define PERIOD_COUNT (135 * SAMPLE_COUNT)

uint16_t smin = 0xFFFF;
uint16_t smax = 0;
uint32_t savg = 0;

uint16_t count;
uint32_t voltage = 0;
uint16_t measured_voltage = 0;

#define MAX_HISTORY 3

uint8_t history_cnt = 0;
uint32_t h_ts[MAX_HISTORY];
uint16_t h_min[MAX_HISTORY];
uint16_t h_max[MAX_HISTORY];
uint16_t h_avg[MAX_HISTORY];

uint16_t calibrate = 0x128; // calibrate this manually

void voltageInit()
{
  analogReference(DEFAULT);

  count = 0;
}

void voltageLoop()
{
  uint16_t adc = analogRead(A0);    

  if( adc < smin )
    smin = adc;
  if( adc > smax )
    smax = adc;
  savg += adc;
  
  voltage += adc;
  count++;

  if( (count % SAMPLE_COUNT) == 0 )
  {
    voltage /= SAMPLE_COUNT;
    measured_voltage = voltage * calibrate / 256;
    voltage = 0;
  }
  if( count == PERIOD_COUNT )
  {
    for(int8_t i=MAX_HISTORY-2; i >=0; i-- )
    {
      h_ts[i+1] = h_ts[i];
      h_min[i+1] = h_min[i];
      h_max[i+1] = h_max[i];
      h_avg[i+1] = h_avg[i];
    }

    h_ts[0] = millis();
    h_min[0] = (uint32_t)smin * calibrate / 256;
    h_max[0] = (uint32_t)smax * calibrate / 256;
    h_avg[0] = (savg / PERIOD_COUNT) * calibrate / 256;

    smin = 0xFFFF;
    smax = 0;
    savg = 0;

    if( history_cnt < MAX_HISTORY )
      history_cnt++;
    count = 0;
  }
}

void voltageHtmlCallback(WebServerCommand command, char * data, int dataLen)
{
  switch(command)
  {
    case BUTTON_PRESS:
      // no buttons
      break;   
    case SET_FIELD:
      /* TODO */
      break;
    case LOAD:
    case REFRESH:
      {
        char buf[20];
        uint8_t int_part = measured_voltage / 256;
        uint8_t float_part = ((measured_voltage & 255) * 100) / 256;
        sprintf(buf, "%d.%02d V", int_part, float_part);
        webServer.setArgString("voltage", buf);

        char tab[256];
        tab[0] = 0;
        strcat_P(tab, PSTR("[[\"Time\",\"Min\",\"AVG\",\"Max\"]"));

        for(uint8_t i=0; i < history_cnt; i++ )
        {
          uint8_t min_i = h_min[i] / 256;
          uint8_t min_f = ((h_min[i] & 255) * 100) / 256;
          uint8_t max_i = h_max[i] / 256;
          uint8_t max_f = ((h_max[i] & 255) * 100) / 256;
          uint8_t avg_i = h_avg[i] / 256;
          uint8_t avg_f = ((h_avg[i] & 255) * 100) / 256;

          sprintf(buf, ",[\"%d s\",", h_ts[i] / 1000);
          strcat(tab, buf);
          sprintf(buf, "\"%d.%02d V\",", min_i, min_f);
          strcat(tab, buf);
          sprintf(buf, "\"%d.%02d V\",", avg_i, avg_f);
          strcat(tab, buf);
          sprintf(buf, "\"%d.%02d V\"]", max_i, max_f);
          strcat(tab, buf);
        }

        strcat_P(tab, PSTR("]"));
        webServer.setArgJson("table", tab);
      }
      break;
  }
}

