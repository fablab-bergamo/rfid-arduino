#include "AuthProvider.hpp"

#include <cstdint>
#include <string>

#include "FabBackend.hpp"
#include "Logging.hpp"

namespace fablabbg
{
  namespace Board
  {
    // Only main.cpp instanciates the variables through Board.h file
    extern FabBackend server;
  } // namespace Board

  AuthProvider::AuthProvider(WhiteList list) : whitelist{list}, cache{} {}

  /// @brief Checks if the cache contains the card ID, and uses that if available
  /// @param uid card id
  /// @return FabUser if found
  auto AuthProvider::is_in_cache(card::uid_t uid) const -> std::optional<FabUser>
  {
    auto elem = std::find_if(cache.begin(), cache.end(),
                             [uid](const auto &input)
                             { return input.card_uid == uid; });

    if (elem == end(cache))
    {
      return std::nullopt;
    }

    return {*elem};
  }

  /// @brief Cache the user request
  /// @param uid card id of the user
  /// @param name name of the user to be cached
  /// @param level priviledge level of the user
  void AuthProvider::add_in_cache(card::uid_t uid, const std::string &name, FabUser::UserLevel level) const
  {
    // Check if already in cache
    if (is_in_cache(uid).has_value())
      return;

    // Add into the list
    FabUser new_elem{uid, name, true, level};
    cache.push_front(new_elem);

    // Keep cache size under CACHE_LEN
    if (cache.size() > conf::rfid_tags::CACHE_LEN)
    {
      cache.pop_back();
    }
  }

  /// @brief Verifies the card ID against the server (if available) or the whitelist
  /// @param uid card ID
  /// @param out a FabUser with an authenticated flag==true if server or whitelist confirmed the ID
  /// @return false if the user was not found on server or whitelist
  auto AuthProvider::tryLogin(card::uid_t uid, FabBackend &server) const -> std::optional<FabUser>
  {
    FabUser user;
    using UserResult = ServerMQTT::UserResult;
    auto uid_str = card::uid_str(uid);

    ESP_LOGD(TAG, "tryLogin called for %s", uid_str.c_str());

    if (!server.isOnline())
      server.connect();

    if (server.isOnline())
    {
      auto response = server.checkCard(uid);
      if (response->request_ok && response->getResult() == UserResult::Authorized)
      {
        user.authenticated = true;
        user.card_uid = uid;
        user.holder_name = response->holder_name;
        user.user_level = response->user_level;
        // Cache the positive result
        add_in_cache(uid, user.holder_name, response->user_level);

        ESP_LOGD(TAG, " -> online check OK (%s)", user.toString().c_str());

        return user;
      }

      ESP_LOGD(TAG, " -> online check NOT OK");

      user.authenticated = false;

      if (response->request_ok)
        return std::nullopt;

      // If request failed, we need to check the whitelist
    }

    if (auto result = WhiteListLookup(uid); result.has_value())
    {
      auto [card, level, name] = result.value();
      user.card_uid = card;
      user.authenticated = true;
      user.user_level = level;
      user.holder_name = name;
      ESP_LOGD(TAG, " -> whilelist check OK (%s)", user.toString().c_str());

      return user;
    }

    ESP_LOGD(TAG, " -> whilelist check NOK");
    return std::nullopt;
  }

  /// @brief Checks if the card ID is whitelisted
  /// @param uid card ID
  /// @return a whitelistentry object if the card is found in whitelist
  auto AuthProvider::WhiteListLookup(card::uid_t candidate_uid) const -> std::optional<WhiteListEntry>
  {
    auto elem = std::find_if(whitelist.begin(), whitelist.end(),
                             [candidate_uid](const auto &input)
                             {
                               auto [w_uid, w_level, w_name] = input;
                               return w_uid == candidate_uid;
                             });

    if (elem == end(whitelist))
    {
      ESP_LOGD(TAG, "%s not found in whitelist", card::uid_str(candidate_uid).c_str());
      return std::nullopt;
    }

    return {*elem};
  }

  /// @brief Sets the whitelist
  /// @param list the whitelist to set
  auto AuthProvider::setWhitelist(WhiteList list) -> void
  {
    whitelist = list;
    cache.clear();
  }

} // namespace fablabbg