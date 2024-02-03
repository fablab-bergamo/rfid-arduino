#ifndef PINS_H_
#define PINS_H_

#include <cstdint>
#include <array>
#include <Adafruit_NeoPixel.h>

namespace fablabbg
{
  static constexpr uint8_t NO_PIN = -1;

  struct pins_config
  {
    struct mfrc522_config
    { /* SPI RFID chip pins definition */
      uint8_t sda_pin;
      uint8_t mosi_pin;
      uint8_t miso_pin;
      uint8_t sck_pin;
      uint8_t reset_pin;
    };
    struct lcd_config /* LCD parallel interface pins definition */
    {
      uint8_t rs_pin; /* Reset */
      uint8_t en_pin; /* Enable */
      uint8_t d0_pin;
      uint8_t d1_pin;
      uint8_t d2_pin;
      uint8_t d3_pin;
      uint8_t bl_pin;  /* Backlight pin */
      bool active_low; /* Backlight active low*/
    };
    struct relay_config
    {
      uint8_t ch1_pin; /* Control pin for Machine 1 */
      bool active_low;
    };
    struct buzzer_config
    {
      uint8_t pin;
    };
    struct led_config
    {
      uint8_t pin;
      bool is_neopixel;
      uint32_t neopixel_config;
    };
    struct buttons_config
    {
      uint8_t factory_defaults_pin;
    };
    // Struct members
    mfrc522_config mfrc522;
    lcd_config lcd;
    relay_config relay;
    buzzer_config buzzer;
    led_config led;
    buttons_config buttons;
  };

#ifdef PINS_ESP32
  constexpr pins_config pins{
      {.sda_pin = 27U,
       .mosi_pin = 33U,
       .miso_pin = 32U,
       .sck_pin = 26U,
       .reset_pin = 20U}, // RFID
      {.rs_pin = 15U,
       .en_pin = 2U,
       .d0_pin = 0U,
       .d1_pin = 4U,
       .d2_pin = 16U,
       .d3_pin = 17U,
       .bl_pin = 18U,
       .active_low = false}, // LCD
      {.ch1_pin = 14U,
       .active_low = true}, // relay
      {.pin = 12U},         // buzzer
      {.pin = 19U,
       .is_neopixel = true,
       .neopixel_config = NEO_RGB + NEO_KHZ800}, // Neopixel
      {.factory_defaults_pin = NO_PIN}           // Factory defaults button
  };
#endif
#if (WOKWI_SIMULATION)
  constexpr pins_config pins{
      {.sda_pin = 27U,
       .mosi_pin = 26U,
       .miso_pin = 33U,
       .sck_pin = 32U,
       .reset_pin = 16U}, // RFID
      {.rs_pin = 15U,
       .en_pin = 18U,
       .d0_pin = 2U,
       .d1_pin = 4U,
       .d2_pin = 5U,
       .d3_pin = 19U,
       .bl_pin = NO_PIN,
       .active_low = false}, // LCD
      {.ch1_pin = 14U,
       .active_low = false}, // relay
      {.pin = 12U},          // buzzer
      {.pin = 20U,
       .is_neopixel = true,
       .neopixel_config = NEO_GRB + NEO_KHZ800}, // Neopixel
      {.factory_defaults_pin = 21U}              // Factory defaults button
  };
#endif
#ifdef PINS_ESP32S3
  constexpr pins_config pins{
      {.sda_pin = 17U,
       .mosi_pin = 8U,
       .miso_pin = 3U,
       .sck_pin = 18U,
       .reset_pin = 12U}, // RFID
      {.rs_pin = 5U,
       .en_pin = 4U,
       .d0_pin = 6U,
       .d1_pin = 7U,
       .d2_pin = 15U,
       .d3_pin = 2U,
       .bl_pin = 13U,
       .active_low = false}, // LCD
      {.ch1_pin = 10U,
       .active_low = true}, // relay
      {.pin = 9U},          // buzzer
      {.pin = 48U,
       .is_neopixel = true,
       .neopixel_config = NEO_GRB + NEO_KHZ800}, // Neopixel
      {.factory_defaults_pin = NO_PIN}           // Factory defaults button
  };
#endif
#ifdef PINS_ESP32_WROVERKIT
  constexpr pins_config pins{
      {.sda_pin = 15U,
       .mosi_pin = 0U,
       .miso_pin = 16U,
       .sck_pin = 5U,
       .reset_pin = 4U}, // RFID
      {.rs_pin = 12U,
       .en_pin = 14U,
       .d0_pin = 26U,
       .d1_pin = 21U,
       .d2_pin = 22U,
       .d3_pin = 23U,
       .bl_pin = NO_PIN,
       .active_low = false}, // LCD
      {.ch1_pin = 2U,
       .active_low = true}, // relay
      {.pin = 13U},         // buzzer
      {.pin = 27U,
       .is_neopixel = true,
       .neopixel_config = NEO_RGB + NEO_KHZ800}, // Neopixel
      {.factory_defaults_pin = NO_PIN}           // Factory defaults button
  };
#endif

  // Check at compile time that there are no duplicate pin definitions
  constexpr bool no_duplicates()
  {
    std::array<uint8_t, 16> pin_nums{
        pins.mfrc522.sda_pin,
        pins.mfrc522.mosi_pin,
        pins.mfrc522.miso_pin,
        pins.mfrc522.sck_pin,
        pins.mfrc522.reset_pin,
        pins.lcd.rs_pin,
        pins.lcd.en_pin,
        pins.lcd.d0_pin,
        pins.lcd.d1_pin,
        pins.lcd.d2_pin,
        pins.lcd.d3_pin,
        pins.lcd.bl_pin,
        pins.relay.ch1_pin,
        pins.buzzer.pin,
        pins.led.pin,
        pins.buttons.factory_defaults_pin};

    // No constexpr std::sort available
    for (auto i = 0; i < pin_nums.size(); ++i)
    {
      if (pin_nums[i] == NO_PIN)
        continue;

      for (auto j = i + 1; j < pin_nums.size(); ++j)
      {
        if (pin_nums[i] == pin_nums[j])
          return false;
      }
    }
    return true;
  }

  static_assert(no_duplicates(), "Duplicate pin definition, check pins.hpp");

} // namespace fablabbg
#endif // PINS_H_