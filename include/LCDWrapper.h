#ifndef _LCD_WRAPPER_H_
#define _LCD_WRAPPER_H_

#include <array>
#include "LiquidCrystal.h"
#include "BoardState.h"

template <uint8_t _COLS, uint8_t _ROWS>
class LCDWrapper
{
public:
  struct Config
  {
    uint8_t rs;
    uint8_t enable;
    uint8_t d0;
    uint8_t d1;
    uint8_t d2;
    uint8_t d3;
    uint8_t backlight_pin;
    bool backlight_active_low;
    Config(uint8_t rs, uint8_t en, uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3, uint8_t backlight_pin = -1, bool backlight_active_low = false) :
      d0(d0), d1(d1), d2(d2), d3(d3), backlight_pin(backlight_pin), backlight_active_low(backlight_active_low), enable(en), rs(rs) {};
  };
  LCDWrapper(Config config);

  void begin();
  void clear();
  void showConnection(bool show);
  void setRow(uint8_t row, std::string text);
  std::string convertSecondsToHHMMSS(unsigned long millis);
  void update_chars(bool connected);

private:
  const uint8_t CHAR_ANTENNA = 0;
  const uint8_t CHAR_CONNECTION = 1;
  const uint8_t CHAR_NO_CONNECTION = 2;
  const Config config;

  LiquidCrystal lcd;
  bool show_connection_status;

  void backlightOn();
  void backlightOff();

  std::array<std::array<char, _COLS>, _ROWS> buffer;
  std::array<std::array<char, _COLS>, _ROWS> current;

  bool needsUpdate();
  void pretty_print(std::array<std::array<char, _COLS>, _ROWS> buffer);

  // Ideally static constexpr, but the LiquidCrystal library expects non-const uint8_t for char definitions
  uint8_t antenna_char[8] = {0x15, 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
  uint8_t connection_char[8] = {0x00, 0x00, 0x01, 0x01, 0x05, 0x05, 0x15, 0x15};
  uint8_t noconnection_char[8] = {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00};
};

#include "LCDWrapper.tpp"
#endif // _LCD_WRAPPER_H_
