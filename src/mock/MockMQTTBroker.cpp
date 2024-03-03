#include "mock/MockMQTTBroker.hpp"
#include "Logging.hpp"
#include "conf.hpp"

static const char *const TAG2 = "MockMQTTBroker";
namespace fablabbg
{
  auto MockMQTTBroker::start() -> void
  {
    while (WiFi.status() != WL_CONNECTED)
    { // Wait for the Wi-Fi to connect
      ESP_LOGD(TAG2, "MQTT BROKER: WiFi status changed to %d", WiFi.status());
      is_running = false;
      return;
    }
    if (!is_running)
    {
      is_running = init(conf::mqtt::PORT_NUMBER, true);

      ESP_LOGI(TAG2, "MQTT BROKER: started with result %d", is_running.load());
    }
  }

  auto MockMQTTBroker::onEvent(sMQTTEvent *event) -> bool
  {
    switch (event->Type())
    {
    case NewClient_sMQTTEventType:
    {
      auto *e = static_cast<sMQTTNewClientEvent *>(event);
      ESP_LOGD(TAG2, "MQTT BROKER: client connected, id:%s", e->Client()->getClientId().c_str());
    }
    break;
    case Public_sMQTTEventType:
    {
      auto *e = static_cast<sMQTTPublicClientEvent *>(event);
      topic = e->Topic();
      payload = e->Payload();

      ESP_LOGI(TAG2, "MQTT BROKER: Received  %s -> %s", topic.c_str(), payload.c_str());
      queries.push({topic, payload, topic + "/reply"});
    }
    break;
    case RemoveClient_sMQTTEventType:
    {
      auto *e = static_cast<sMQTTRemoveClientEvent *>(event);
      ESP_LOGD(TAG2, "MQTT BROKER: removed client id: %s", e->Client()->getClientId().c_str());
    }
    break;
    case LostConnect_sMQTTEventType:
    {
      is_running = false;
      ESP_LOGD(TAG2, "MQTT BROKER: lost connection");
    }
    break;
    default:
      ESP_LOGD(TAG2, "MQTT BROKER: unhandled event %d", event->Type());
      break;
    }
    return true;
  }

  auto MockMQTTBroker::isRunning() const -> bool
  {
    return is_running;
  }

  /// @brief Returns a fake server reply for testing purposes
  /// @return json payload
  auto MockMQTTBroker::defaultReplies(const std::string &query) const -> const std::string
  {
    if (query.find("checkmachine") != std::string::npos)
    {
      return "{\"request_ok\":true,\"is_valid\":true,\"allowed\":true,\"maintenance\":false,\"logoff\":30,\"name\":\"ENDER_1\",\"type\":1}";
    }

    if (query.find("maintenance") != std::string::npos)
    {
      return "{\"request_ok\":true}";
    }

    if (query.find("startuse") != std::string::npos)
    {
      return "{\"request_ok\":true}";
    }

    if (query.find("stopuse") != std::string::npos)
    {
      return "{\"request_ok\":true}";
    }

    if (query.find("checkuser") != std::string::npos)
    {
      auto uid = query.substr(query.find("uid") + 6 + 4, 4);
      auto response = "{\"request_ok\":true,\"is_valid\":true,\"level\":2,\"name\":\"USER" + uid + "\",\"is_valid\":true}";
      return response;
    }
    if (query.find(conf::default_config::machine_topic) != std::string::npos) // Shelly doesn't reply
    {
      return "";
    }

    return std::string{"{\"request_ok\":true}"};
  }

  auto MockMQTTBroker::configureReplies(std::function<const std::string(const std::string &, const std::string &)> callback) -> void
  {
    while (true)
    {
      if (lock())
      {
        this->callback = callback;
        unlock();
        break;
      }
    }
  }

  auto MockMQTTBroker::processQueries() -> size_t
  {
    if (!queries.empty())
    {
      auto [topic, query, reply_topic] = queries.front();
      queries.pop();
      std::string response{""};
      while (true)
      {
        if (lock())
        {
          response = callback(topic, query);
          unlock();
          break;
        }
        delay(1);
      }
      if (!response.empty())
      {
        ESP_LOGI(TAG2, "MQTT BROKER: Sending %s -> %s", reply_topic.c_str(), response.c_str());
        publish(reply_topic, response);
      }
    }
    return queries.size();
  }

  void MockMQTTBroker::mainLoop()
  {
    // Check if the server is online
    if (!isRunning())
    {
      start();
    }
    else
    {
      update();
      processQueries();
    }
  }
} // namespace fablabbg