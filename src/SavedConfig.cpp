#include <optional>

#include <ArduinoJson.h>
#include <EEPROM.h>

#include "Logging.hpp"
#include "SavedConfig.hpp"
#include "conf.hpp"
#include "secrets.hpp"

namespace fablabbg
{
  auto SavedConfig::DefaultConfig() -> SavedConfig
  {
    SavedConfig config;

    // Wifi
    strncpy(config.ssid, secrets::credentials::ssid.data(), FIELD_LENGTH);
    strncpy(config.password, secrets::credentials::password.data(), FIELD_LENGTH);

    // MQTT
    strncpy(config.mqtt_server, conf::default_config::mqtt_server.data(), FIELD_LENGTH);
    strncpy(config.mqtt_user, secrets::credentials::mqtt_user.data(), FIELD_LENGTH);
    strncpy(config.mqtt_password, secrets::credentials::mqtt_password.data(), FIELD_LENGTH);

    // MQTT Switch
    strncpy(config.machine_topic, conf::default_config::machine_topic.data(), FIELD_LENGTH);

    // Machine
    strncpy(config.machine_id, std::to_string(conf::default_config::machine_id.id).c_str(), INT_LENGTH);

    config.magic_number = MAGIC_NUMBER;

    return config;
  }

  auto SavedConfig::LoadFromEEPROM() -> std::optional<SavedConfig>
  {
    SavedConfig config;

    if (!EEPROM.begin(sizeof(SavedConfig)))
    {
      ESP_LOGE(TAG, "SavedConfig::LoadFromEEPROM() : EEPROM.begin failed");
      return std::nullopt;
    }

    EEPROM.get(0, config);

    if (config.magic_number != MAGIC_NUMBER)
    {
      return std::nullopt;
    }
    return config;
  }

  auto SavedConfig::SaveToEEPROM() const -> bool
  {
    if (!EEPROM.begin(sizeof(SavedConfig)))
    {
      ESP_LOGE(TAG, "SavedConfig::SaveToEEPROM() : EEPROM.begin failed");
      return false;
    }

    magic_number = MAGIC_NUMBER;

    EEPROM.put(0, *this);
    auto result = EEPROM.commit();

    if (result)
    {
      ESP_LOGD(TAG, "EEPROM commit success");
      return result;
    }

    ESP_LOGE(TAG, "EEPROM commit failure");
    return false;
  }

  auto SavedConfig::toString() const -> const std::string
  {
    DynamicJsonDocument doc(1024);

    doc["ssid"] = ssid;
    doc["password"] = password;
    doc["mqtt_server"] = mqtt_server;
    doc["mqtt_user"] = mqtt_user;
    doc["mqtt_password"] = mqtt_password;
    doc["machine_topic"] = machine_topic;
    doc["machine_id"] = machine_id;
    doc["magic_number"] = magic_number;

    std::string result;
    serializeJson(doc, result);
    return result;
  }
} // namespace fablabbg
