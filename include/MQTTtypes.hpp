#ifndef MQTTTYPES_HPP_
#define MQTTTYPES_HPP_

#include "ArduinoJson.h"
#include "FabUser.hpp"
#include "Machine.hpp"
#include "card.hpp"
#include "string"
#include <memory>
#include <string_view>

namespace fablabbg::ServerMQTT
{
  class Query
  {
  public:
    virtual auto payload() const -> const std::string = 0;
    virtual ~Query() = default;
  };

  class UserQuery final : public Query
  {
  public:
    const card::uid_t uid;

    UserQuery() = delete;
    constexpr UserQuery(card::uid_t card_uid) : uid(card_uid){};

    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class MachineQuery final : public Query
  {
  public:
    constexpr MachineQuery() = default;
    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class AliveQuery final : public Query
  {
  public:
    constexpr AliveQuery() = default;
    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class StartUseQuery final : public Query
  {
  public:
    const card::uid_t uid;

    StartUseQuery() = delete;
    constexpr StartUseQuery(card::uid_t card_uid) : uid(card_uid){};

    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class StopUseQuery final : public Query
  {
  public:
    const card::uid_t uid;
    const std::chrono::seconds duration_s;

    StopUseQuery() = delete;

    /// @brief Request to register machine usage stop
    /// @param card_uid machine user card id
    /// @param mid machine id
    /// @param duration duration of usage, in seconds
    constexpr StopUseQuery(card::uid_t card_uid, std::chrono::seconds duration) : uid(card_uid), duration_s(duration){};
    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class InUseQuery final : public Query
  {
  public:
    const card::uid_t uid;
    const std::chrono::seconds duration_s;

    InUseQuery() = delete;

    /// @brief Request to register machine usage stop
    /// @param card_uid machine user card id
    /// @param mid machine id
    /// @param duration duration of usage, in seconds
    constexpr InUseQuery(card::uid_t card_uid, std::chrono::seconds duration) : uid(card_uid), duration_s(duration){};
    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class RegisterMaintenanceQuery final : public Query
  {
  public:
    const card::uid_t uid;

    RegisterMaintenanceQuery() = delete;
    constexpr RegisterMaintenanceQuery(card::uid_t card_uid) : uid(card_uid){};

    [[nodiscard]] auto payload() const -> const std::string override;
  };

  class Response
  {
  public:
    const bool request_ok{false}; /* True if the request was processed by the server */

    Response() = delete;
    constexpr Response(bool result) : request_ok(result){};
  };

  enum class UserResult : uint8_t
  {
    Invalid = 0,
    Authorized = 1,
    Unauthorized = 2,
    MaintenanceUnauthorized = 3,
  };

  class UserResponse final : public Response
  {
  public:
    uint8_t result{static_cast<uint8_t>(UserResult::Invalid)};  /* Result of the user check */
    std::string holder_name{""};                                /* Name of the user from server DB */
    FabUser::UserLevel user_level{FabUser::UserLevel::Unknown}; /* User priviledges */

    UserResponse() = delete;
    UserResponse(bool rok) : Response(rok){};

    UserResponse(bool rok, UserResult res) : Response(rok),
                                             result(static_cast<uint8_t>(res)){};

    [[nodiscard]] static auto fromJson(JsonDocument &doc) -> std::unique_ptr<UserResponse>;

    [[nodiscard]] auto getResult() const -> UserResult;
    [[nodiscard]] auto toString() const -> const std::string;
  };

  class MachineResponse final : public Response
  {
  public:
    bool is_valid = false;   /* True if the machine has a valid ID */
    bool maintenance = true; /* True if the machine needs maintenance */
    bool allowed = false;    /* True if the machine can be used by anybody */
    uint16_t logoff = 0;     /* Timeout in minutes */
    std::string name{""};    /* Name of the machine from server DB */
    uint8_t type = 0;        /* Type of the machine */
    uint16_t grace = 0;      /* Grace period in minutes */
    MachineResponse() = delete;
    MachineResponse(bool rok) : Response(rok){};

    [[nodiscard]] static auto fromJson(JsonDocument &doc) -> std::unique_ptr<MachineResponse>;
  };

  class SimpleResponse final : public Response
  {
  public:
    SimpleResponse() = delete;
    constexpr SimpleResponse(bool rok) : Response(rok){};

    [[nodiscard]] static auto fromJson(JsonDocument &doc) -> std::unique_ptr<SimpleResponse>;
  };

} // namespace fablabbg::ServerMQTT
#endif // MQTTTYPES_HPP_
