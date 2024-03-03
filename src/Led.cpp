#include "Led.hpp"
#include "Logging.hpp"

namespace fablabbg
{
  auto Led::init() -> void
  {
    if (initialized || pins.led.pin == NO_PIN)
    {
      return;
    }

    ESP_LOGD(TAG, "Initializing LED (pin %d, is_neopixel %d, flags %u)",
             pins.led.pin, pins.led.is_neopixel, pins.led.neopixel_config);

    if (pins.led.is_neopixel)
    {
      pixel.begin();
    }
    else if (pins.led.is_rgb)
    {
      pinMode(pins.led.pin, OUTPUT);
      pinMode(pins.led.green_pin, OUTPUT);
      pinMode(pins.led.blue_pin, OUTPUT);
    }
    else
    {
      digitalWrite(pins.led.pin, LOW);
      pinMode(pins.led.pin, OUTPUT);
    }
    initialized = true;
  }

  auto Led::setColor(uint8_t r, uint8_t g, uint8_t b) -> void
  {
    color = {r, g, b};
  }

  auto Led::set(Status new_status) -> void
  {
    this->status = new_status;
  }

  auto Led::outputColor(uint8_t r, uint8_t g, uint8_t b) -> void
  {
    if constexpr (pins.led.is_neopixel)
    {
      pixel.setPixelColor(0, r, g, b);
      pixel.show();
    }
    if constexpr (pins.led.is_rgb)
    {
      analogWrite(pins.led.pin, r);
      analogWrite(pins.led.green_pin, g);
      analogWrite(pins.led.blue_pin, b);
    }
    if constexpr (!pins.led.is_rgb || !pins.led.is_neopixel)
    {
      auto light = r > 0 || g > 0 || b > 0;
      digitalWrite(pins.led.pin, light ? HIGH : LOW);
    }
  }

  auto Led::update() -> void
  {
    init();

    switch (status)
    {
    case Status::Off:
      isOn = false;
      break;
    case Status::On:
      isOn = true;
      break;
    case Status::Blinking:
      isOn = !isOn;
      break;
    }

    if (isOn)
    {
      outputColor(color[0], color[1], color[2]);
    }
    else
    {
      outputColor(0, 0, 0);
    }
  }

} // namespace fablabbg