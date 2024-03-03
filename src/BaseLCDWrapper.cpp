
#include <chrono>
#include <string>

#include "BaseLCDWrapper.hpp"

namespace fablabbg
{
  auto BaseLCDWrapper::convertSecondsToHHMMSS(std::chrono::seconds duration) const -> const std::string
  {
    //! since something something does not support to_string we have to resort to ye olde cstring stuff
    char buf[9] = {0};

    snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu",
             duration.count() / 3600UL,
             (duration.count() % 3600UL) / 60UL,
             duration.count() % 60UL);

    return {buf};
  }
} // namespace fablabbg