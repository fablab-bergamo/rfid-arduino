#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include <WiFi.h>

#include "MQTTtypes.hpp"
#include "ArduinoJson.hpp"
#include "FabUser.hpp"
#include "card.hpp"

#ifndef GIT_VERSION
#define GIT_VERSION "unknown"
#endif

namespace fablabbg::ServerMQTT
{
  auto UserQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"checkuser\","
       << "\"uid\":\"" << card::uid_str(uid) << "\""
       << "}";
    return ss.str();
  }

  auto MachineQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"checkmachine\""
       << "}";
    return ss.str();
  }

  auto AliveQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"alive\","
       << "\"version\":\"" << GIT_VERSION << "\","
       << "\"ip\":\"" << WiFi.localIP().toString().c_str() << "\""
       << "}";
    return ss.str();
  }

  auto StartUseQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"startuse\", "
       << "\"uid\":\"" << card::uid_str(uid) << "\""
       << "}";
    return ss.str();
  }

  auto StopUseQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"stopuse\", "
       << "\"uid\":\"" << card::uid_str(uid) << "\","
       << "\"duration\":" << duration_s.count()
       << "}";
    return ss.str();
  }

  auto InUseQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"inuse\", "
       << "\"uid\":\"" << card::uid_str(uid) << "\","
       << "\"duration\":" << duration_s.count()
       << "}";
    return ss.str();
  }

  auto RegisterMaintenanceQuery::payload() const -> const std::string
  {
    std::stringstream ss{};
    ss << "{\"action\":\"maintenance\", "
       << "\"uid\":\"" << card::uid_str(uid) << "\""
       << "}";
    return ss.str();
  }

  auto UserResponse::getResult() const -> UserResult
  {
    return static_cast<UserResult>(result);
  }

  auto UserResponse::fromJson(JsonDocument &doc) -> std::unique_ptr<UserResponse>
  {
    auto response = std::make_unique<UserResponse>(doc["request_ok"].as<bool>());
    response->result = doc["is_valid"];
    response->holder_name = doc["name"].as<std::string>();
    response->user_level = static_cast<FabUser::UserLevel>(doc["level"].as<int>());

    return response;
  }

  auto UserResponse::toString() const -> const std::string
  {
    std::stringstream ss{};
    ss << "UserResponse: "
       << "request_ok: " << request_ok << ", "
       << "result: " << static_cast<int>(result) << ", "
       << "holder_name: " << holder_name << ", "
       << "user_level: " << static_cast<int>(user_level);
    return ss.str();
  }

  auto MachineResponse::fromJson(JsonDocument &doc) -> std::unique_ptr<MachineResponse>
  {
    auto response = std::make_unique<MachineResponse>(doc["request_ok"].as<bool>());
    response->is_valid = doc["is_valid"];
    response->maintenance = doc["maintenance"];
    response->allowed = doc["allowed"];
    response->logoff = doc["logoff"];
    response->name = doc["name"].as<std::string>();
    response->type = doc["type"];
    if (!doc["grace"].isNull())
    {
      response->grace = doc["grace"];
    }
    else
    {
      response->grace = std::chrono::duration_cast<std::chrono::minutes>(conf::machine::DEFAULT_GRACE_PERIOD).count();
    }

    return response;
  }

  auto SimpleResponse::fromJson(JsonDocument &doc) -> std::unique_ptr<SimpleResponse>
  {
    auto response = std::make_unique<SimpleResponse>(doc["request_ok"].as<bool>());
    return response;
  }
} // namespace fablabbg::ServerMQTT