#ifndef BASELCDWRAPPER_HPP_
#define BASELCDWRAPPER_HPP_

#include "BoardInfo.hpp"
#include "Machine.hpp"
#include <chrono>
#include <string>

namespace fablabbg
{
  class BaseLCDWrapper
  {
  public:
    virtual ~BaseLCDWrapper() = default;

    virtual auto begin() -> bool = 0;
    virtual auto clear() -> void = 0;
    virtual auto showConnection(bool show) -> void = 0;
    virtual auto showPower(bool show) -> void = 0;
    virtual auto setRow(uint8_t row, const std::string &text) -> void = 0;
    virtual auto update(const BoardInfo &boardinfo, bool forced = false) -> void = 0;

    [[nodiscard]] auto convertSecondsToHHMMSS(std::chrono::seconds duration) const -> const std::string;
  };
} // namespace fablabbg

#endif // BASELCDWRAPPER_HPP_
