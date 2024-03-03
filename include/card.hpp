#ifndef CARD_HPP_
#define CARD_HPP_

#include "Arduino.h"
#include "conf.hpp"
#include <iomanip>
#include <sstream>

#include "Logging.hpp"

namespace fablabbg::card
{
  using uid_t = u_int64_t;
  static constexpr uid_t INVALID = 0ULL;
  
  /// @brief Returns a string representation of the UID
  /// @param uid number to convert
  /// @return an hex string representation of the UID (e.g. "123456ADCD")
  [[nodiscard]] inline std::string uid_str(const card::uid_t uid)
  {
    uint64_t number = static_cast<uint64_t>(uid);
    uint32_t long1 = static_cast<uint32_t>(number & 0xFFFF0000) >> 16;
    uint32_t long2 = static_cast<uint32_t>(number & 0x0000FFFF);

    std::stringstream ss{};
    ss << std::setfill('0') << std::setw(4) << std::hex << long1;
    ss << std::setfill('0') << std::setw(4) << std::hex << long2;
    return ss.str();
  }

  /// @brief Converts a UID from an array of bytes to a number
  /// @param uid array of bytes
  /// @return the UID as a number
  [[nodiscard]] constexpr inline uid_t from_array(const std::array<uint8_t, conf::rfid_tags::UID_BYTE_LEN> &uid)
  {
    card::uid_t result = 0;
    for (auto i = (conf::rfid_tags::UID_BYTE_LEN - 1); i >= 0; i--)
    {
      result <<= 8;
      result |= uid[i];
    }
    return result;
  }

  inline void print(uint64_t uid)
  {
    ESP_LOGI(TAG, "Card UID = %s", card::uid_str(uid).c_str());
  }
} // namespace fablabbg::card
#endif // CARD_HPP_