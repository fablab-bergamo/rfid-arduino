#ifndef MOCK_MOCKLCDLIBRARY_HPP_
#define MOCK_MOCKLCDLIBRARY_HPP_

#include <stdint.h>

namespace fablabbg
{
  class MockLcdLibrary final
  {
  public:
    constexpr MockLcdLibrary() = default;
    MockLcdLibrary(uint8_t rs_pin, uint8_t en_pin, uint8_t d0_pin, uint8_t d1_pin, uint8_t d2_pin, uint8_t d3_pin){};
    auto begin(uint8_t cols, uint8_t rows) -> void{};
    auto createChar(uint8_t num, const uint8_t *values) -> void{};
    auto clear() -> void{};
    auto print(const char *text) -> void{};
    auto setCursor(uint8_t col, uint8_t row) -> void{};
    auto write(uint8_t value) -> void{};
  };
} // namespace fablabbg

#endif // MOCK_MOCKLCDLIBRARY_HPP_