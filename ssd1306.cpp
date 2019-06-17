/*
 * Ref.:
 * DigisparkOLED: https://github.com/digistump/DigistumpArduino/tree/master/digistump-avr/libraries/DigisparkOLED
 * SSD1306 data sheet: https://www.adafruit.com/datasheets/SSD1306.pdf
 */
#include "ssd1306.h"
#include "watchdigit.h"
#include <avr/pgmspace.h>
#include <TinyWireM.h>

/*
 * Software Configuration, data sheet page 64
 */

static const uint8_t ssd1306_configuration[] PROGMEM = {

#ifdef SCREEN128X64
  0xA8, 0x3F,   // Set MUX Ratio, 0F-3F
#else // SCREEN128X32 / SCREED64X32
  0xA8, 0x1F,   // Set MUX Ratio, 0F-3F
#endif

  0xD3, 0x00,   // Set Display Offset
  0x40,         // Set Display Start line
  0xA1,         // Set Segment re-map, mirror, A0/A1
  0xC8,         // Set COM Output Scan Direction, flip, C0/C8

#ifdef SCREEN128X64
  0xDA, 0x02,   // Set COM Pins hardware configuration, Sequential
#else // SCREEN128X32 / SCREED64X32
  0xDA, 0x12,   // Set Com Pins hardware configuration, Alternative
#endif

  0x81, 0x01,   // Set Contrast Control, 01-FF
  0xA4,         // Disable Entire Display On, 0xA4=Output follows RAM content; 0xA5,Output ignores RAM content
  0xA6,         // Set Display Mode. A6=Normal; A7=Inverse
  0xD5, 0x80,   // Set Osc Frequency
  0x8D, 0x14,   // Enable charge pump regulator
  0xAF          // Display ON in normal mode
};

SSD1306::SSD1306(void) {}

void SSD1306::begin(void)
{
  for (uint8_t i = 0; i < sizeof (ssd1306_configuration); i++) {
    ssd1306_send_command(pgm_read_byte_near(&ssd1306_configuration[i]));
  }
}

void SSD1306::ssd1306_send_command_start(void) {
  TinyWireM.beginTransmission(SSD1306_I2C_ADDR);
  TinyWireM.send(0x00); //command
}

void SSD1306::ssd1306_send_command_stop(void) {
  TinyWireM.endTransmission();
}

void SSD1306::ssd1306_send_command(uint8_t command)
{
  ssd1306_send_command_start();
  TinyWireM.send(command);
  ssd1306_send_command_stop();
}

void SSD1306::ssd1306_send_data_start(void)
{
  TinyWireM.beginTransmission(SSD1306_I2C_ADDR);
  TinyWireM.send(0x40); //data
}

void SSD1306::ssd1306_send_data_stop(void)
{
  TinyWireM.endTransmission();
}

void SSD1306::ssd1306_send_data_byte(uint8_t data)
{
  if (TinyWireM.write(data) == 0) {
    // push data if detect buffer used up
    ssd1306_send_data_stop();
    ssd1306_send_data_start();
    TinyWireM.write(data);
  }
}

void SSD1306::set_area(uint8_t col, uint8_t page, uint8_t col_range_minus_1, uint8_t page_range_minus_1)
{
  ssd1306_send_command_start();
  TinyWireM.send(0x20);
  TinyWireM.send(0x01);
  TinyWireM.send(0x21);
  TinyWireM.send(XOFFSET + col);
  TinyWireM.send(XOFFSET + col + col_range_minus_1);
  TinyWireM.send(0x22);
  TinyWireM.send(page);
  TinyWireM.send(page + page_range_minus_1);
  ssd1306_send_command_stop();
}

void SSD1306::fill(uint8_t data)
{
  set_area(0, 0, WIDTH - 1, PAGES - 1);
  uint16_t data_size = (WIDTH) * (PAGES);

  ssd1306_send_data_start();
  for (uint16_t i = 0; i < data_size; i++)
  {
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();
}

void SSD1306::v_line(uint8_t col, uint8_t data)
{
  set_area(col, 0, 0, PAGES);
  ssd1306_send_data_start();
  for (uint8_t i = 0; i <= PAGES; i++)
  {
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();
}

void SSD1306::draw_pattern(uint8_t col, uint8_t page, uint8_t width, uint8_t pattern) {
  set_area(col, page, width, 0);
  ssd1306_send_data_start();
  for (uint8_t i = 0; i < width; i++) {
    ssd1306_send_data_byte(pattern);
  }
  ssd1306_send_data_stop();
}

void SSD1306::draw_digit(uint8_t col, uint8_t page, uint8_t digit, bool invert_color)
{
  set_area(col, page, FONTWIDTH - 1, 1 - 1);
  uint16_t offset = digit * FONTWIDTH;
  uint8_t data;

  ssd1306_send_data_start();
  for (uint8_t i = 0; i < FONTWIDTH; i++) // 8x7 font size data: (8 / 8) * 7 = 7
  {
    data = pgm_read_byte_near(&watch_digit[offset++]);
    if (invert_color) data = ~ data; // invert
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();
}

void SSD1306::draw_3x_digit(uint8_t col, uint8_t page, uint8_t digit, bool invert_color)
{
  set_area(col, page, FONT3XWIDTH - 1, 3 - 1);
  uint16_t offset = digit * 3 * FONT3XWIDTH; // 24x15 font size data: (24 / 8) * 15 = 45
  uint8_t data;

  ssd1306_send_data_start();
  for (uint8_t j = 0; j < 3 * FONT3XWIDTH; j++)
  {
    data = pgm_read_word_near(&watch_3x_digit[offset++]);
    if (invert_color) data = ~ data; // invert
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();
}

void SSD1306::print_digits(uint8_t col, uint8_t page, uint8_t font_size, uint32_t factor, uint32_t digits, bool invert_color) {
  uint16_t cur_digit = digits / factor;
  if (font_size == 1) {
    draw_digit(col, page, cur_digit, invert_color);
  } else {
    draw_3x_digit(col, page, cur_digit, invert_color);
  }

  if (factor > 1) {
    if (font_size == 1) {
      print_digits(col + FONTWIDTH, page, font_size, factor / 10, digits - (cur_digit * factor), invert_color);
    } else {
      print_digits(col + FONT3XWIDTH, page, font_size, factor / 10, digits - (cur_digit * factor), invert_color);
    }
  }
}

void SSD1306::off(void)
{
  ssd1306_send_command(0xAE);
}

void SSD1306::on(void)
{
  ssd1306_send_command(0xAF);
}

