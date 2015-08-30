#include "Time.h"

static tmElements_t tm;          // a cache of time elements
static time_t       cacheTime;   // the time the cache was updated
static time_t       syncInterval = 300;  // time sync will be attempted after this many seconds

static os_timer_t micros_overflow_timer;
static uint32_t micros_at_last_overflow_tick = 0;
static uint32_t micros_overflow_count = 0;

static time_t sysTime = 0;
static time_t prevMillis = 0;
static time_t nextSyncTime = 0;
static timeStatus_t Status = timeNotSet;

getExternalTime getTimePtr;

int16_t ICACHE_FLASH_ATTR 
totalMinutes(int16_t hours, int8_t minutes)
{
  return (hours * 60) + minutes;
}

void ICACHE_FLASH_ATTR 
micros_overflow_tick(void* arg) {
  uint32_t m = system_get_time();
  if (m < micros_at_last_overflow_tick)
    ++micros_overflow_count;
  micros_at_last_overflow_tick = m;
}

unsigned long ICACHE_FLASH_ATTR 
millis() {
  uint32_t m = system_get_time();
  uint32_t c = micros_overflow_count + ((m < micros_at_last_overflow_tick) ? 1 : 0);
  return c * 4294967 + m / 1000;
}

unsigned long ICACHE_FLASH_ATTR 
micros() {
  return system_get_time();
}

void ICACHE_FLASH_ATTR 
time_init() {
  os_timer_setfn(&micros_overflow_timer, (os_timer_func_t*)&micros_overflow_tick, 0);
  os_timer_arm(&micros_overflow_timer, 60000, 1);
}

void ICACHE_FLASH_ATTR 
breakTime(time_t time, tmElements_t *tm){

  uint8_t year;
  uint8_t month, monthLength;
  unsigned long days;

  tm->Second = time % 60;
  time /= 60; // now it is minutes
  tm->Minute = time % 60;
  time /= 60; // now it is hours
  tm->Hour = time % 24;
  time /= 24; // now it is days
  tm->Wday = ((time + 4) % 7) + 1;  // Sunday is day 1 

  year = 0;
  days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tm->Year = year; // year is offset from 1970 

  days -= LEAP_YEAR(year) ? 366 : 365;
  time -= days; // now it is days in this year, starting at 0

  days = 0;
  month = 0;
  monthLength = 0;
  for (month = 0; month<12; month++) {
    if (month == 1) { // february
      if (LEAP_YEAR(year)) {
        monthLength = 29;
      }
      else {
        monthLength = 28;
      }
    }
    else {
      monthLength = monthDays[month];
    }

    if (time >= monthLength) {
      time -= monthLength;
    }
    else {
      break;
    }
  }
  tm->Month = month + 1;  // jan is month 1  
  tm->Day = time + 1;     // day of month
}

/*
* Convert the "timeInput" time_t count into "struct tm" time components.
* This is a more compact version of the C library localtime function.
* Note that year is offset from 1970 !!!
*/
void ICACHE_FLASH_ATTR 
timet_to_tm(time_t timeInput, struct tmElements *tmel){

  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;

  time = (uint32_t)timeInput;
  tmel->Second = time % 60;
  time /= 60;		// Now it is minutes.
  tmel->Minute = time % 60;
  time /= 60;		// Now it is hours.
  tmel->Hour = time % 24;
  time /= 24;		// Now it is days.
  tmel->Wday = ((time + 4) % 7) + 1;	// Sunday is day 1.

  year = 0;
  days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tmel->Year = year;	// The year is offset from 1970.

  days -= LEAP_YEAR(year) ? 366 : 365;
  time -= days;	// Now it is days in this year, starting at 0.

  days = 0;
  month = 0;
  monthLength = 0;
  for (month = 0; month<12; month++) {
    if (month == 1) {	// February.
      if (LEAP_YEAR(year)) {
        monthLength = 29;
      }
      else {
        monthLength = 28;
      }
    }
    else {
      monthLength = monthDays[month];
    }

    if (time >= monthLength) {
      time -= monthLength;
    }
    else {
      break;
    }
  }
  tmel->Month = month + 1;	// Jan is month 1.
  tmel->Day = time + 1;		// Day of month.
}

/*
* Reconstitute "struct tm" elements into a time_t count value.
* Note that the year argument is offset from 1970.
*/
time_t ICACHE_FLASH_ATTR 
tm_to_timet(struct tmElements *tmel){

  int i;
  uint32_t seconds;

  // Seconds from 1970 till 1st Jan 00:00:00 of the given year.
  seconds = tmel->Year*(SECS_PER_DAY * 365);
  for (i = 0; i < tmel->Year; i++) {
    if (LEAP_YEAR(i)) {
      seconds += SECS_PER_DAY;	// Add extra days for leap years.
    }
  }

  // Add the number of elapsed days for the given year. Months start from 1.
  for (i = 1; i < tmel->Month; i++) {
    if ((i == 2) && LEAP_YEAR(tmel->Year)) {
      seconds += SECS_PER_DAY * 29;
    }
    else {
      seconds += SECS_PER_DAY * monthDays[i - 1];	// "monthDay" array starts from 0.
    }
  }
  seconds += (tmel->Day - 1) * SECS_PER_DAY;		// Days...
  seconds += tmel->Hour * SECS_PER_HOUR;		// Hours...
  seconds += tmel->Minute * SECS_PER_MIN;		// Minutes...
  seconds += tmel->Second;				// ...and finally, Seconds.
  return (time_t)seconds;
}

void ICACHE_FLASH_ATTR 
refreshCache(time_t t){
  if (t != cacheTime)
  {
    breakTime(t, &tm);
    cacheTime = t;
  }
}

int ICACHE_FLASH_ATTR 
hour() { // the hour now 
  time_t t = now();
  refreshCache(t);
  return tm.Hour;
}

int ICACHE_FLASH_ATTR 
minute() { // the minute now
  time_t t = now();
  refreshCache(t);
  return tm.Minute;
}

int ICACHE_FLASH_ATTR 
second() {  // the second now
  time_t t = now();
  refreshCache(t);
  return tm.Second;
}

void ICACHE_FLASH_ATTR 
setTime(time_t t){
#ifdef TIME_DRIFT_INFO
  if (sysUnsyncedTime == 0)
    sysUnsyncedTime = t;   // store the time of the first call to set a valid Time   
#endif

  sysTime = t;
  nextSyncTime = t + syncInterval;
  Status = timeSet;
  prevMillis = millis();  // restart counting from now (thanks to Korman for this fix)
}

time_t ICACHE_FLASH_ATTR 
now(){
  while (millis() - prevMillis >= 1000){
    sysTime++;
    prevMillis += 1000;
#ifdef TIME_DRIFT_INFO
    sysUnsyncedTime++; // this can be compared to the synced time to measure long term drift     
#endif	
  }
    if (nextSyncTime <= sysTime){
      if (getTimePtr != 0){
        time_t t = getTimePtr();
        if (t != 0)
          setTime(t);
        else
          Status = (Status == timeNotSet) ? timeNotSet : timeNeedsSync;
      }
    }
  return sysTime;
}

timeStatus_t ICACHE_FLASH_ATTR 
timeStatus(){ // indicates if time has been set and recently synchronized
  return Status;
}

void ICACHE_FLASH_ATTR 
setSyncProvider(getExternalTime getTimeFunction){
  getTimePtr = getTimeFunction;
  nextSyncTime = sysTime;
  now(); // this will sync the clock
}

void ICACHE_FLASH_ATTR 
setSyncInterval(time_t interval){ // set the number of seconds between re-sync
  syncInterval = interval;
}