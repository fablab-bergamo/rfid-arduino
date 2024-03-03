#ifndef FABUSER_HPP_
#define FABUSER_HPP_

#include "Arduino.h"
#include "card.hpp"
#include "conf.hpp"
#include <array>
#include <cstdint>
#include <sstream>
#include <string>

namespace fablabbg
{
  struct FabUser
  {
    enum class UserLevel : uint8_t
    {
      Unknown,
      NormalUser,
      FabStaff,
      FabAdmin,
    };

    card::uid_t card_uid = card::INVALID;
    std::string holder_name{""};
    bool authenticated = false;
    UserLevel user_level = UserLevel::Unknown;

    FabUser() = default;

    FabUser(const card::uid_t uid, const std::string &name, bool auth, UserLevel level) : card_uid(uid),
                                                                                          holder_name(name),
                                                                                          authenticated(auth),
                                                                                          user_level(level) {}

    FabUser(std::array<uint8_t, conf::rfid_tags::UID_BYTE_LEN> &uid,
            const std::string &name, bool auth,
            UserLevel level) : card_uid(card::from_array(uid)),
                               holder_name(name),
                               authenticated(auth),
                               user_level(level) {}

    auto operator==(const FabUser &t) const -> bool
    {
      return card_uid == t.card_uid;
    }

    auto toString() const -> const std::string
    {
      std::stringstream sstream{};

      sstream << "User (Auth:" << authenticated;
      sstream << ", UID: " << std::hex << card_uid;
      sstream << ", Name:" << holder_name;

      auto i_level = static_cast<typename std::underlying_type<UserLevel>::type>(user_level);
      sstream << ", level:" << i_level << ")";

      return sstream.str();
    }
  };
} // namespace fablabbg
#endif // FABUSER_HPP_