#include "FabBackend.hpp"
#include "secrets.hpp"

#include "Logging.hpp"
#include "SavedConfig.hpp"
#include "Tasks.hpp"
#include <ArduinoJson.h>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

namespace fablabbg
{
  void FabBackend::configure(const SavedConfig &config)
  {
    wifi_ssid = config.ssid;
    wifi_password = config.password;
    broker_hostname = config.mqtt_server;
    mqtt_user = config.mqtt_user;
    mqtt_password = config.mqtt_password;

#if (PINS_WOKWI)
    channel = 6;
    broker_hostname = "localhost";
#else
    channel = -1;
#endif
    online = false;

    std::stringstream ss;
    ss << conf::mqtt::topic << "/" << config.machine_id;
    topic = ss.str();

    char client_name[16]{0};
    if (auto result = std::snprintf(client_name, sizeof(client_name), "BOARD%ld", random(0, 1000));
        result < 0)
    {
      ESP_LOGE(TAG, "Failure to generate client name");
    }
    mqtt_client_name = client_name;

    if (client.connected()) // Topic or IP may also have changed
    {
      client.disconnect();
      wifi_client.stop();
    }
    ESP_LOGD(TAG, "FabServer configured");
  }

  /// @brief Posts to MQTT server and waits for answer
  /// @param query query to be posted
  /// @return true if the server answered
  bool FabBackend::publishWithReply(const ServerMQTT::Query &query)
  {
    auto try_cpt = 0;
    auto published = false;

    while (try_cpt < conf::mqtt::MAX_TRIES)
    {
      try_cpt++;

      if (!published)
      {
        if (publish(query))
        {
          published = true;
        }
        else
        {
          ESP_LOGE(TAG, "MQTT Client: failure to send query %s", query.payload().data());
          Tasks::task_delay(conf::mqtt::TIMEOUT_REPLY_SERVER);
        }
      }

      if (waitForAnswer(conf::mqtt::TIMEOUT_REPLY_SERVER))
      {
        ESP_LOGD(TAG, "MQTT Client: received answer: %s", last_reply.data());
        return true;
      }

      ESP_LOGW(TAG, "MQTT Client: no answer received, retrying %d/%d", try_cpt, conf::mqtt::MAX_TRIES);
      Tasks::task_delay(conf::mqtt::TIMEOUT_REPLY_SERVER);
    }

    ESP_LOGE(TAG, "MQTT Client: failure to send query %s", query.payload().data());
    return false;
  }

  /// @brief publish on MQTT the requested info
  /// @param mqtt_topic topic to publish to
  /// @param mqtt_payload payload to publish
  /// @return true if successfull
  bool FabBackend::publish(String mqtt_topic, String mqtt_payload)
  {
    if (mqtt_payload.length() + mqtt_topic.length() > FabBackend::MAX_MSG_SIZE - 8)
    {
      ESP_LOGE(TAG, "MQTT Client: Message is too long: %s", mqtt_payload.c_str());
      return false;
    }

    answer_pending = true;
    last_query.assign(mqtt_payload.c_str());

    ESP_LOGI(TAG, "MQTT Client: sending message %s on topic %s", mqtt_payload.c_str(), mqtt_topic.c_str());

    return client.publish(mqtt_topic.c_str(), mqtt_payload.c_str());
  }

  /// @brief posts to MQTT server
  /// @param query message to post
  /// @return true if the message was published
  bool FabBackend::publish(const ServerMQTT::Query &query)
  {
    String s_payload(query.payload().data());
    String s_topic(topic.c_str());

    if (s_payload.length() + topic.length() > FabBackend::MAX_MSG_SIZE - 8)
    {
      ESP_LOGE(TAG, "MQTT Client: Message is too long: %s", s_payload.c_str());
      return false;
    }

    answer_pending = true;
    last_query.assign(s_payload.c_str());

    ESP_LOGD(TAG, "MQTT Client: sending message %s on topic %s", s_payload.c_str(), s_topic.c_str());

    return client.publish(s_topic, s_payload);
  }

  bool FabBackend::loop()
  {
    if (!client.loop())
    {
      if (online)
      {
        ESP_LOGI(TAG, "MQTT Client: connection lost");
      }
      online = false;
      return false;
    }
    return true;
  }

  /// @brief blocks until the server answers or until the timeout is reached
  /// @return true if the server answered
  bool FabBackend::waitForAnswer(std::chrono::milliseconds max_duration)
  {
    const auto start_time = std::chrono::system_clock::now();
    const auto DELAY_MS = 50ms;
    do
    {
      if (answer_pending)
      {
        client.loop();
        if (!client.connected())
        {
          ESP_LOGW(TAG, "MQTT Client: connection lost while waiting for answer");
          connect();
        }
        Tasks::task_delay(DELAY_MS);
      }
      else
      {
        return true;
      }
    } while (std::chrono::system_clock::now() < (start_time + max_duration));

    ESP_LOGE(TAG, "Failure, no answer from MQTT server (timeout:%lld ms)", max_duration.count());
    return false;
  }

  /// @brief true if the server has been reached successfully
  /// @return boolean
  bool FabBackend::isOnline() const
  {
    return online;
  }

  /// @brief Callback for MQTT messages
  /// @param topic topic the message was received on
  /// @param payload payload of the message
  void FabBackend::messageReceived(String &s_topic, String &s_payload)
  {
    ESP_LOGI(TAG, "MQTT Client: Received on %s -> %s", s_topic.c_str(), s_payload.c_str());

    last_reply.assign(s_payload.c_str());
    answer_pending = false;
  }

  /// @brief Connects to the WiFi network
  /// @return true if the connection succeeded
  bool FabBackend::connectWiFi()
  {
    static constexpr auto NB_TRIES = 30;
    static constexpr auto DELAY_MS = 100ms;

    // Connect WiFi if needed
    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.mode(WIFI_STA);
      ESP_LOGD(TAG, "FabServer::connectWiFi() : WiFi connection state=%d, connecting to SSID:%s (channel:%d)", WiFi.status(), wifi_ssid.c_str(), channel);
      WiFi.begin(wifi_ssid.data(), wifi_password.data(), channel);
      Tasks::task_delay(DELAY_MS);
      for (auto i = 0; i < NB_TRIES; i++)
      {
        if (WiFi.status() == WL_CONNECTED)
        {
          ESP_LOGD(TAG, "FabServer::connectWiFi() : WiFi connection successfull");
          break;
        }
        Tasks::task_delay(DELAY_MS);
      }
    }
    return WiFi.status() == WL_CONNECTED;
  }

  /// @brief Establish WiFi connection and connects to FabServer
  /// @return true if both operations succeeded
  bool FabBackend::connect()
  {
    const auto status = WiFi.status();

    ESP_LOGD(TAG, "FabServer::connect() called, Wifi status=%d", status);

    // Check if WiFi nextwork is available, and if not, try to connect
    if (status != WL_CONNECTED)
    {
      online = false;

      if (client.connected())
      {
        ESP_LOGD(TAG, "Closing MQTT client due to WiFi down");
        client.disconnect();
      }

      connectWiFi();
    }

    // Check if WiFi is available but MQTT client is not
    if (WiFi.status() == WL_CONNECTED &&
        !client.connected())
    {
      IPAddress ip;
      if (WiFi.hostByName(broker_hostname.c_str(), ip))
      {
        ESP_LOGD(TAG, "Resolved MQTT server [%s] as [%s]", broker_hostname.c_str(), ip.toString().c_str());
      }
      else
      {
        ESP_LOGE(TAG, "Failed to resolve MQTT server [%s]", broker_hostname.c_str());
        return false;
      }

      ESP_LOGD(TAG, "Connecting to MQTT server [%s:%d]...", ip.toString().c_str(), conf::mqtt::PORT_NUMBER);

      client.begin(ip, conf::mqtt::PORT_NUMBER, wifi_client);

      callback = [&](String &a, String &b)
      { return messageReceived(a, b); };

      client.onMessage(callback);

      if (!client.connect(mqtt_client_name.c_str(),
                          mqtt_user.c_str(),
                          mqtt_password.c_str(), false))
      {
        ESP_LOGW(TAG, "Failure to connect as client: %s with username %s, last error %d", mqtt_client_name.c_str(), mqtt_user.c_str(), client.lastError());
      }

      // Setup subscriptions
      if (client.connected())
      {
        std::stringstream tmp_topic;
        tmp_topic << topic << conf::mqtt::response_topic;
        response_topic.assign(tmp_topic.str());

        if (!client.subscribe(response_topic.c_str()))
        {
          ESP_LOGE(TAG, "MQTT Client: failure to subscribe to reply topic %s", response_topic.c_str());
        }
        else
        {
          ESP_LOGD(TAG, "MQTT Client: subscribed to reply topic %s", response_topic.c_str());
          online = true;
        }
        // Announce the board to the server
        if (auto query = ServerMQTT::AliveQuery{}; publish(query))
        {
          ESP_LOGI(TAG, "MQTT Client: board announced to server");
        }
        else
        {
          ESP_LOGW(TAG, "MQTT Client: failure to announce board to server");
        }
      }
      else
      {
        ESP_LOGW(TAG, "Failure to connect to MQTT server %s", broker_hostname.c_str());
      }
    }

    if (!client.connected())
    {
      online = false;
    }

    return online;
  }

  /// @brief Disconnects from the server
  void FabBackend::disconnect()
  {
    client.disconnect();
  }

  /// @brief Process a MQTT query and returns the response
  /// @tparam T type of the response returned to the caller (it will be wrapped in a unique_ptr)
  /// @tparam Q type of the query sent to the server
  /// @tparam ...Args arguments to be passed to the constructor of Q
  /// @return backend response (if request_ok)
  template <typename RespT, typename QueryT, typename... QueryArgs>
  std::unique_ptr<RespT> FabBackend::processQuery(QueryArgs &&...args)
  {
    static_assert(std::is_base_of<ServerMQTT::Query, QueryT>::value, "QueryT must inherit from Query");
    static_assert(std::is_base_of<ServerMQTT::Response, RespT>::value, "RespT must inherit from Response");

    if (isOnline())
    {
      if (QueryT query{args...}; publishWithReply(query))
      {
        // Deserialize the JSON document
        auto payload = last_reply.c_str();
        if (DeserializationError error = deserializeJson(doc, payload))
        {
          ESP_LOGE(TAG, "Failed to parse json: %s (%s)", payload, error.c_str());
          return std::make_unique<RespT>(false);
        }
        return RespT::fromJson(doc);
      }
    }
    return std::make_unique<RespT>(false);
  }

  template <typename QueryT, typename... QueryArgs>
  bool FabBackend::processQuery(QueryArgs &&...args)
  {
    static_assert(std::is_base_of<ServerMQTT::Query, QueryT>::value, "QueryT must inherit from Query");

    if (isOnline())
    {
      if (QueryT query{args...}; publish(query))
      {
        return true;
      }
    }
    return false;
  }

  /// @brief Checks if the card ID is known to the server
  /// @param uid card uid
  /// @return backend response (if request_ok)
  std::unique_ptr<ServerMQTT::UserResponse> FabBackend::checkCard(card::uid_t uid)
  {
    return processQuery<ServerMQTT::UserResponse, ServerMQTT::UserQuery>(uid);
  }

  /// @brief Checks the machine status on the server
  /// @return backend response (if request_ok)
  std::unique_ptr<ServerMQTT::MachineResponse> FabBackend::checkMachine()
  {
    return processQuery<ServerMQTT::MachineResponse, ServerMQTT::MachineQuery>();
  }

  /// @brief register the starting of a machine usage
  /// @param uid card uid
  /// @return backend response (if request_ok)
  std::unique_ptr<ServerMQTT::SimpleResponse> FabBackend::startUse(card::uid_t uid)
  {
    return processQuery<ServerMQTT::SimpleResponse, ServerMQTT::StartUseQuery>(uid);
  }

  /// @brief Register end of machine usage
  /// @param uid card ID of the machine user
  /// @param duration_s duration of usage in seconds
  /// @return backend response (if request_ok)
  std::unique_ptr<ServerMQTT::SimpleResponse> FabBackend::finishUse(card::uid_t uid, std::chrono::seconds duration_s)
  {
    return processQuery<ServerMQTT::SimpleResponse, ServerMQTT::StopUseQuery>(uid, duration_s);
  }

  /// @brief Inform the backend that the machine is in use. This is used to prevent
  ///        loosing information if the machine is rebooted while in use.
  /// @param uid card ID of the machine user
  /// @param duration_s duration of usage in seconds
  /// @return backend response (if request_ok)
  std::unique_ptr<ServerMQTT::SimpleResponse> FabBackend::inUse(card::uid_t uid, std::chrono::seconds duration_s)
  {
    return processQuery<ServerMQTT::SimpleResponse, ServerMQTT::InUseQuery>(uid, duration_s);
  }

  /// @brief Registers a maintenance action
  /// @param maintainer who performed the maintenance
  /// @return server response (if request_ok)
  std::unique_ptr<ServerMQTT::SimpleResponse> FabBackend::registerMaintenance(card::uid_t maintainer)
  {
    return processQuery<ServerMQTT::SimpleResponse, ServerMQTT::RegisterMaintenanceQuery>(maintainer);
  }

  /// @brief Sends a ping to the server
  /// @return true if packet was sent
  bool FabBackend::alive()
  {
    return processQuery<ServerMQTT::AliveQuery>();
  }

  /// @brief set channel to use for WiFi connection
  /// @param channel
  void FabBackend::setChannel(int32_t channel)
  {
    this->channel = channel;
  }
} // namespace fablabbg