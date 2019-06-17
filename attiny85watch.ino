#include <avr/sleep.h>
#include <TinyWireM.h>
#include "ssd1306.h"
#include "WDT_Time.h"

#define TIMEOUT 50000 // 5 seconds
static uint32_t display_timeout;


// Variable used for millis debounce
volatile long TimeOfLastDebounce1 = 0, TimeOfLastDebounce2 = 0;  // holds the last time the switch was pressed
volatile long DelayofDebounce = 2000;  // amount of time that needs to be expired between presses
volatile int setState = 0;
volatile int upState = 0;


#define UNUSEDPIN 1
#define SETBUTTON 3
#define UPBUTTON  4

SSD1306 oled;
bool sleeping = false;

void setup() {
  pinMode(UNUSEDPIN, INPUT_PULLUP);
  pinMode(SETBUTTON, INPUT_PULLUP);
  pinMode(UPBUTTON, INPUT_PULLUP);

  wdt_setup();

  TinyWireM.begin();
  oled.begin();
  oled.fill(0x00); // clear in black
  set_display_timeout();
  setTime(0, 0, 0, 1, 1, 2019);
}

void loop() {
  if (sleeping) {
    system_sleep();
  } else {
    if (millis() > display_timeout) {
      enter_sleep();
    } else {
      draw_oled();
    }
  }
}

void enter_sleep() {
  oled.fill(0x00); // clear screen to avoid show old time when wake up
  oled.off();
  delay(2); // wait oled stable

  sleeping = true;
}

void wake_up() {
  sleeping = false;

  delay(2); // wait oled stable
  oled.on();

  set_display_timeout();
}

void set_display_timeout() {
  display_timeout = millis() + TIMEOUT;
}

/*
 * UI related
 */

// 0: time; 1: debug
static uint8_t display_mode = 0;
static uint8_t last_display_mode = 0;
// 0: none; 1: year; 2: month; 3: day; 4: hour; 5: minute
static uint8_t selected_field = 0;

void draw_oled() {
  if (display_mode != last_display_mode) {
    oled.fill(0x00);
    last_display_mode = display_mode;
  }
  if (display_mode == 0) {

    // top right corner: battery status
    uint32_t vcc = readVcc();
    // show battery bar from 1.8 V to 3.0 V in 8 pixels, (3000 - 1800) / 8 = 150
    uint8_t bat_level = (vcc >= 3000) ? 8 : ((vcc <= 1800) ? 1 : ((vcc - 1800 + 150) / 150));
    oled.draw_pattern(70, 0, 1, 0b00111111);
    oled.draw_pattern(71, 0, 1, 0b00100001);
    oled.draw_pattern(72, 0, 8, 0b00100001);
    oled.draw_pattern(72, 0, bat_level, 0b00101101);
    oled.draw_pattern(80, 0, 1, 0b00111111);
    oled.draw_pattern(81, 0, 1, 0b00001100);
    
    // 1st rows: print date
    /* print_digits(col, page, size, factor, digits) */
    oled.print_digits(0, 0, 1, 1000, year(), (selected_field == 1) ? true : false);
    /* draw_pattern(col, page, width, pattern) */
    oled.draw_pattern(29, 0, 2, 0b00010000); // dash
    oled.print_digits(32, 0, 1, 10, month(), (selected_field == 2) ? true : false);
    oled.draw_pattern(47, 0, 2, 0b00010000); // dash
    oled.print_digits(50, 0, 1, 10, day(), (selected_field == 3) ? true : false);
    // 2nd-4th rows: print time
    oled.print_digits(0, 1, 3, 10, hour(), (selected_field == 4) ? true : false);
    oled.draw_pattern(31, 2, 2, (second() & 1) ? 0b11000011 : 0); // blink colon
    oled.print_digits(34, 1, 3, 10, minute(), (selected_field == 5) ? true : false);
  } else if (display_mode == 1) {
    oled.print_digits(0, 0, 1, 10000, wdt_get_interrupt_count(), false);
    oled.print_digits(0, 1, 1, 10000, readVcc(), false);
  }
}

// PIN CHANGE interrupt event function
ISR(PCINT0_vect)
{
  sleep_disable();

  if (sleeping) {
    wake_up();
    TimeOfLastDebounce1 = millis();
    TimeOfLastDebounce2 = millis();
  } else {
    if (digitalRead(SETBUTTON) == LOW && setState == 0) { // SET button pressed

     if ((millis() - TimeOfLastDebounce1) > DelayofDebounce) {
        setState=1;
        TimeOfLastDebounce1 = millis();
        
        selected_field++;
        if (selected_field > 5) selected_field = 0;
        if (selected_field > 0) display_mode = 0;
     }
    } else if (setState == 1 && digitalRead(SETBUTTON) == HIGH){
        setState=0;
    }
    
    if (digitalRead(UPBUTTON) == LOW && upState == 0) { // UP button pressed
       
       if ((millis() - TimeOfLastDebounce2) > DelayofDebounce) {
        upState=1;
        TimeOfLastDebounce2 = millis();
      
        if (selected_field == 0) {
          display_mode++; // toggle mode;
          if (display_mode > 1) display_mode = 0;
        } else {
          int set_year = year();
          int set_month = month();
          int set_day = day();
          int set_hour = hour();
          int set_minute = minute();
    
          if (selected_field == 1) {
            set_year++; // add year
            if (set_year > 2029) set_year = 2019; // loop back
          } else if (selected_field == 2) {
            set_month++; // add month
            if (set_month > 12) set_month = 1; // loop back
          } else if (selected_field == 3) {
            set_day++; // add day
            if (set_day > getMonthDays(CalendarYrToTm(set_year), set_month)) set_day = 1; // loop back
          } else if (selected_field == 4) {
            set_hour++; // add day
            if (set_hour > 23) set_hour = 0; // loop back
          } else if (selected_field == 5) {
            set_minute++; // add day
            if (set_minute > 59) set_minute = 0; // loop back
          }
          setTime(set_hour, set_minute, second(), set_day, set_month, set_year);
        }
      } 
    } else if (upState == 1 && digitalRead(UPBUTTON) == HIGH){
          upState=0;
    }
  }

  set_display_timeout(); // extend display timeout while user input
  sleep_enable();
}
