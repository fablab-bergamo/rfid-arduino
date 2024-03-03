#ifndef AUTHPROVIDER_HPP_
#define AUTHPROVIDER_HPP_

#include "FabUser.hpp"
#include "secrets.hpp"
#include <list>
#include <string>
#include <string_view>
#include <tuple>

namespace fablabbg
{
  using WhiteListEntry = std::tuple<card::uid_t, FabUser::UserLevel, std::string_view>;
  using WhiteList = std::array<WhiteListEntry, secrets::cards::LEN>;

  class AuthProvider
  {
  private:
    WhiteList whitelist;
    mutable std::list<FabUser> cache;
    void add_in_cache(card::uid_t uid, const std::string &name, FabUser::UserLevel level) const;
    [[nodiscard]] auto is_in_cache(card::uid_t uid) const -> std::optional<FabUser>;
    [[nodiscard]] auto WhiteListLookup(card::uid_t uid) const -> std::optional<WhiteListEntry>;

  public:
    AuthProvider() = delete;
    AuthProvider(WhiteList whitelist);
    [[nodiscard]] auto tryLogin(card::uid_t uid, FabBackend &server) const -> std::optional<FabUser>;
    void setWhitelist(WhiteList list);
  };
} // namespace fablabbg
#endif // AUTHPROVIDER_HPP_
