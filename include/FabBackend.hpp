#ifndef FABBACKEND_HPP_
#define FABBACKEND_HPP_

#include <array>
#include <chrono>
#include <functional>
#include <string>

#include "WiFi.h"
#include <ArduinoJson.h>
#include <MQTTClient.h>

#include "FabUser.hpp"
#include "MQTTtypes.hpp"
#include "SavedConfig.hpp"
#include "conf.hpp"

namespace fablabbg
{
  class FabBackend
  {
  private:
    constexpr static auto MAX_MSG_SIZE = 255;
    MQTTClient client{MAX_MSG_SIZE}; // Default is 128, and can be reached with some messages
    StaticJsonDocument<MAX_MSG_SIZE> doc;

    std::string wifi_ssid{""};
    std::string wifi_password{""};
    std::string broker_hostname{""};
    std::string mqtt_user{""};
    std::string mqtt_password{""};
    std::string mqtt_client_name{""};

    MQTTClientCallbackSimpleFunction callback;
    WiFiClient wifi_client;

    std::string topic{""};
    std::string response_topic{""};
    std::string last_query{""};
    std::string last_reply{""};

    bool online{false};
    bool answer_pending{false};
    int32_t channel{-1};

    auto messageReceived(String &topic, String &payload) -> void;

    [[nodiscard]] auto publish(const ServerMQTT::Query &payload) -> bool;
    [[nodiscard]] auto waitForAnswer(std::chrono::milliseconds timeout) -> bool;
    [[nodiscard]] auto publishWithReply(const ServerMQTT::Query &payload) -> bool;

    template <typename RespT, typename QueryT, typename... QueryArgs>
    [[nodiscard]] auto processQuery(QueryArgs &&...) -> std::unique_ptr<RespT>;

    template <typename QueryT, typename... QueryArgs>
    [[nodiscard]] auto processQuery(QueryArgs &&...args) -> bool;

  public:
    FabBackend() = default;

    [[nodiscard]] auto checkCard(const card::uid_t uid) -> std::unique_ptr<ServerMQTT::UserResponse>;
    [[nodiscard]] auto checkMachine() -> std::unique_ptr<ServerMQTT::MachineResponse>;
    [[nodiscard]] auto startUse(const card::uid_t uid) -> std::unique_ptr<ServerMQTT::SimpleResponse>;
    [[nodiscard]] auto inUse(const card::uid_t uid, std::chrono::seconds duration) -> std::unique_ptr<ServerMQTT::SimpleResponse>;
    [[nodiscard]] auto finishUse(const card::uid_t uid, std::chrono::seconds duration) -> std::unique_ptr<ServerMQTT::SimpleResponse>;
    [[nodiscard]] auto registerMaintenance(const card::uid_t maintainer) -> std::unique_ptr<ServerMQTT::SimpleResponse>;
    [[nodiscard]] auto alive() -> bool;
    [[nodiscard]] auto publish(String topic, String payload) -> bool;
    [[nodiscard]] auto isOnline() const -> bool;

    auto connect() -> bool;
    auto connectWiFi() -> bool;
    auto loop() -> bool;

    auto configure(const SavedConfig &config) -> void; // Must be called before using the server
    auto disconnect() -> void;
    auto setChannel(int32_t channel) -> void;

    // Rule of 5 https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-five
    FabBackend(const FabBackend &) = delete;            // copy constructor
    FabBackend &operator=(const FabBackend &) = delete; // copy assignment
    FabBackend(FabBackend &&) = delete;                 // move constructor
    FabBackend &operator=(FabBackend &&) = delete;      // move assignment
  };
} // namespace fablabbg
#endif // FABBACKEND_HPP_