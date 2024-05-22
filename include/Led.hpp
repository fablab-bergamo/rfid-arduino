#ifndef LED_HPP_
#define LED_HPP_

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <memory>

#include "pins.hpp"

namespace fabomatic
{
  class Led
  {
  public:
    enum class Status
    {
      Off,
      On,
      Blinking,
    };

    Led() = default;

    void set(Status status);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void update();

  private:
    Adafruit_NeoPixel pixel{1, pins.led.pin, pins.led.neopixel_config};
    std::array<uint8_t, 3> color{128, 128, 128};
    Status status{Status::On};
    bool isOn{false};
    bool initialized{false};
    void outputColor(uint8_t r, uint8_t g, uint8_t b);
    void init();
  };

} // namespace fabomatic

#endif // LED_HPP_