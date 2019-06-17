/*
 * Revised time function count time by WDT timer instead of millis() function
 * Add WDT and power related function
 * Ref.:
 * time function v1.4: https://github.com/PaulStoffregen/Time
 * WDT and power related: http://www.re-innovation.co.uk/web12/index.php/en/blog-75/306-sleep-modes-on-attiny85
 * readVcc: http://forum.arduino.cc/index.php?topic=222847.0
*/

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

#include <avr/wdt.h>        // Supplied Watch Dog Timer Macros 
#include <avr/sleep.h>      // Supplied AVR Sleep Macros

// Routines to clear and set bits (used in the sleep code)
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#include "WDT_Time.h"

static tmElements_t tm;          // a cache of time elements
static time_t cacheTime;   // the time the cache was updated
static uint32_t syncInterval = 300;  // time sync will be attempted after this many seconds

#define WDT_INTERVAL 6 // ~1 second
// TODO: dynamic calibrate wdt_millis_per_interrupt by current voltage (readVcc) and temperature 
static uint16_t wdt_millis_per_interrupt = 1012;//998; // calibrate value

static uint32_t wdt_interrupt_count = 0;
static uint32_t wdt_millis = 0;

void refreshCache(time_t t) {
  if (t != cacheTime) {
    breakTime(t, tm);
    cacheTime = t;
  }
}

int hour() { // the hour now
  return hour(now());
}

int hour(time_t t) { // the hour for the given time
  refreshCache(t);
  return tm.Hour;
}

int hourFormat12() { // the hour now in 12 hour format
  return hourFormat12(now());
}

int hourFormat12(time_t t) { // the hour for the given time in 12 hour format
  refreshCache(t);
  if ( tm.Hour == 0 )
    return 12; // 12 midnight
  else if ( tm.Hour  > 12)
    return tm.Hour - 12 ;
  else
    return tm.Hour ;
}

uint8_t isAM() { // returns true if time now is AM
  return !isPM(now());
}

uint8_t isAM(time_t t) { // returns true if given time is AM
  return !isPM(t);
}

uint8_t isPM() { // returns true if PM
  return isPM(now());
}

uint8_t isPM(time_t t) { // returns true if PM
  return (hour(t) >= 12);
}

int minute() {
  return minute(now());
}

int minute(time_t t) { // the minute for the given time
  refreshCache(t);
  return tm.Minute;
}

int second() {
  return second(now());
}

int second(time_t t) {  // the second for the given time
  refreshCache(t);
  return tm.Second;
}

int day() {
  return (day(now()));
}

int day(time_t t) { // the day for the given time (0-6)
  refreshCache(t);
  return tm.Day;
}

int weekday() {   // Sunday is day 1
  return  weekday(now());
}

int weekday(time_t t) {
  refreshCache(t);
  return tm.Wday;
}

int month() {
  return month(now());
}

int month(time_t t) {  // the month for the given time
  refreshCache(t);
  return tm.Month;
}

int year() {  // as in Processing, the full four digit year: (2009, 2010 etc)
  return year(now());
}

int year(time_t t) { // the year for the given time
  refreshCache(t);
  return tmYearToCalendar(tm.Year);
}

uint8_t getMonthDays(uint8_t y, uint8_t m) {
  return ((m == 2) && LEAP_YEAR(y)) ? 29 : monthDays[m - 1];
}

/*============================================================================*/
/* functions to convert to and from system time */
/* These are for interfacing with time serivces and are not normally needed in a sketch */

void breakTime(time_t timeInput, tmElements_t &tm) {
  // break the given time_t into time components
  // this is a more compact version of the C library localtime function
  // note that year is offset from 1970 !!!

  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;

  time = (uint32_t)timeInput;
  tm.Second = time % 60;
  time /= 60; // now it is minutes
  tm.Minute = time % 60;
  time /= 60; // now it is hours
  tm.Hour = time % 24;
  time /= 24; // now it is days
  tm.Wday = ((time + 4) % 7) + 1;  // Sunday is day 1

  year = 0;
  days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tm.Year = year; // year is offset from 1970

  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // now it is days in this year, starting at 0

  days = 0;
  month = 1;
  monthLength = 0;
  for (month = 1; month <= 12; month++) {
    monthLength = getMonthDays(year, month);

    if (time >= monthLength) {
      time -= monthLength;
    } else {
      break;
    }
  }
  tm.Month = month;  // jan is month 1
  tm.Day = time + 1;     // day of month
}

time_t makeTime(tmElements_t &tm) {
  // assemble time elements into time_t
  // note year argument is offset from 1970 (see macros in time.h to convert to other formats)
  // previous version used full four digit year (or digits since 2000),i.e. 2009 was 2009 or 9

  int i;
  uint32_t seconds;

  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds = tm.Year * (SECS_PER_DAY * 365);
  for (i = 0; i < tm.Year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }

  // add days for this year, months start from 1
  for (i = 1; i < tm.Month; i++) {
    if ( (i == 2) && LEAP_YEAR(tm.Year)) {
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * monthDays[i - 1]; //monthDay array starts from 0
    }
  }
  seconds += (tm.Day - 1) * SECS_PER_DAY;
  seconds += tm.Hour * SECS_PER_HOUR;
  seconds += tm.Minute * SECS_PER_MIN;
  seconds += tm.Second;
  return (time_t)seconds;
}
/*=====================================================*/
/* Low level system time functions  */

static uint32_t sysTime = 0;
static uint32_t prevMillis = 0;
static uint32_t nextSyncTime = 0;
static timeStatus_t Status = timeNotSet;

getExternalTime getTimePtr;  // pointer to external sync function
//setExternalTime setTimePtr; // not used in this version

#ifdef TIME_DRIFT_INFO   // define this to get drift data
time_t sysUnsyncedTime = 0; // the time sysTime unadjusted by sync
#endif


time_t now() {
  while (wdt_millis - prevMillis >= 1000) {
    sysTime++;
    prevMillis += 1000;
#ifdef TIME_DRIFT_INFO
    sysUnsyncedTime++; // this can be compared to the synced time to measure long term drift
#endif
  }
  if (nextSyncTime <= sysTime) {
    if (getTimePtr != 0) {
      time_t t = getTimePtr();
      if (t != 0) {
        setTime(t);
      } else {
        nextSyncTime = sysTime + syncInterval;
        Status = (Status == timeNotSet) ?  timeNotSet : timeNeedsSync;
      }
    }
  }
  return (time_t)sysTime;
}

void setTime(time_t t) {
#ifdef TIME_DRIFT_INFO
  if (sysUnsyncedTime == 0)
    sysUnsyncedTime = t;   // store the time of the first call to set a valid Time
#endif

  sysTime = (uint32_t)t;
  nextSyncTime = (uint32_t)t + syncInterval;
  Status = timeSet;
  prevMillis = wdt_millis;  // restart counting from now (thanks to Korman for this fix)
}

void setTime(int hr, int min, int sec, int dy, int mnth, int yr) {
  // year can be given as full four digit year or two digts (2010 or 10 for 2010);
  //it is converted to years since 1970
  if ( yr > 99)
    yr = yr - 1970;
  else
    yr += 30;
  tm.Year = yr;
  tm.Month = mnth;
  tm.Day = dy;
  tm.Hour = hr;
  tm.Minute = min;
  tm.Second = sec;
  setTime(makeTime(tm));
}

void adjustTime(long adjustment) {
  sysTime += adjustment;
}

// indicates if time has been set and recently synchronized
timeStatus_t timeStatus() {
  now(); // required to actually update the status
  return Status;
}

void setSyncProvider( getExternalTime getTimeFunction) {
  getTimePtr = getTimeFunction;
  nextSyncTime = sysTime;
  now(); // this will sync the clock
}

void setSyncInterval(time_t interval) { // set the number of seconds between re-sync
  syncInterval = (uint32_t)interval;
  nextSyncTime = sysTime + syncInterval;
}

// 0=16ms, 1=32ms,2=64ms,3=128ms,4=250ms,5=500ms
// 6=1 sec,7=2 sec, 8=4 sec, 9= 8sec
void setup_watchdog(int ii) {
  byte bb;
  int ww;
  if (ii > 9 ) ii = 9;
  bb = ii & 7;
  if (ii > 7) bb |= (1 << 5);
  bb |= (1 << WDCE);
  ww = bb;

  MCUSR &= ~(1 << WDRF);
  // start timed sequence
  WDTCR |= (1 << WDCE) | (1 << WDE);
  // set new watchdog timeout value
  WDTCR = bb;
  WDTCR |= _BV(WDIE);
  sbi(GIMSK, PCIE); // Turn on Pin Change interrupts (Tell Attiny85 we want to use pin change interrupts (can be any pin))
  sbi(PCMSK, PCINT3);
  sbi(PCMSK, PCINT4);
  sei();    // Enable the Interrupts
}

void wdt_setup() {
  setup_watchdog(WDT_INTERVAL);
}

// WDT interrupt event function
ISR(WDT_vect) {
  sleep_disable();
  wdt_interrupt_count++;
  wdt_millis += wdt_millis_per_interrupt;
  sleep_enable();
}

uint32_t wdt_get_interrupt_count() {
  return wdt_interrupt_count;
}

// set system into the sleep state
// system wakes up when wtchdog is timed out
void system_sleep() {
  cbi(ADCSRA, ADEN);                   // switch Analog to Digitalconverter OFF
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
  sleep_mode();                        // System actually sleeps here
  sbi(ADCSRA, ADEN);                   // switch Analog to Digitalconverter ON
}

uint32_t readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  uint32_t result = (high << 8) | low;

  //  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  result = 1089952UL / result; // calibrated value
  return result; // Vcc in millivolts
}
