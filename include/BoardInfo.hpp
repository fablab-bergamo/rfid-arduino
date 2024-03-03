#ifndef BOARDINFO_HPP
#define BOARDINFO_HPP

#include "Machine.hpp"

namespace fablabbg
{
  struct BoardInfo
  {
    bool server_connected;
    Machine::PowerState power_state;
    bool power_warning;

    [[nodiscard]] auto operator==(const BoardInfo &other) const -> bool
    {
      return server_connected == other.server_connected &&
             power_state == other.power_state &&
             power_warning == other.power_warning;
    };
  };
} // namespace fablabbg
#endif // BOARDINFO_HPP