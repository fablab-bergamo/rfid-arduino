#include "Machine.hpp"
#include "Arduino.h"
#include "FabBackend.hpp"
#include "Logging.hpp"
#include "Tasks.hpp"
#include "pins.hpp"
#include "secrets.hpp"
#include <chrono>
#include <cstdint>
#include <sstream>
#include <type_traits>

namespace fablabbg
{
  using namespace std::chrono_literals;
  using milliseconds = std::chrono::milliseconds;

#define CHECK_CONFIGURED(ret_type)                   \
  if (!config.has_value() || !server.has_value())    \
  {                                                  \
    ESP_LOGE(TAG, "Machine : call configure first"); \
    return ret_type();                               \
  }

  auto Machine::configure(const MachineConfig &new_config, FabBackend &serv) -> void
  {
    // https://stackoverflow.com/questions/67596731/why-is-stdoptionaltoperator-deleted-when-t-contains-a-const-data-memb
    config.emplace(new_config);

    server = std::reference_wrapper<FabBackend>(serv);

    if (config.value().hasRelay())
    {
      digitalWrite(config.value().relay_config.pin,
                   config.value().relay_config.active_low ? HIGH : LOW);
      pinMode(config.value().relay_config.pin, OUTPUT);
    }

    ESP_LOGD(TAG, "Machine configured : %s", toString().c_str());
  }

  /// @brief Returns the machine identifier
  /// @return Machine identifier
  auto Machine::getMachineId() const -> MachineID
  {
    CHECK_CONFIGURED(MachineID);
    return config.value().machine_id;
  }

  /// @brief Indicates whether the machine is used by somebody
  /// @return boolean
  auto Machine::isFree() const -> bool
  {
    return !active;
  }

  /// @brief Log the given user onto the machine, if free and not blocked
  /// @param user user to login
  /// @return true of the user has been successfully logged in
  auto Machine::login(const FabUser &user) -> bool
  {
    CHECK_CONFIGURED(bool);
    if (isFree() && allowed)
    {
      active = true;
      current_user = user;
      power(true);
      usage_start_timestamp = std::chrono::system_clock::now();
      return true;
    }
    return false;
  }

  /// @brief Returns the current power state of the machine
  /// @return
  auto Machine::getPowerState() const -> Machine::PowerState
  {
    return power_state;
  }

  /// @brief Removes the user from the machine, and powers off the machine (respecting POWEROFF_DELAY_MINUTES setting)
  auto Machine::logout() -> void
  {
    CHECK_CONFIGURED(void);
    if (active)
    {
      active = false;
      power_state = PowerState::WaitingPowerOff;
      usage_start_timestamp = std::nullopt;

      // Sets the countdown to power off
      logoff_timestamp = std::chrono::system_clock::now();

      if (config.value().grace_period == 0s)
      {
        power(false);
      }

      ESP_LOGI(TAG, "Machine will be shutdown in %lld s",
               config.value().grace_period.count());
    }
  }

  /// @brief indicates if the machine can be powered off
  /// @return true if the delay has expired
  auto Machine::canPowerOff() const -> bool
  {
    if (!logoff_timestamp.has_value())
      return false;

    CHECK_CONFIGURED(bool);

    return (power_state == PowerState::WaitingPowerOff &&
            std::chrono::system_clock::now() - logoff_timestamp.value() > config.value().grace_period);
  }

  /// @brief indicates if the machine is about to shudown and board should beep
  /// @return true if shutdown is imminent
  auto Machine::isShutdownImminent() const -> bool
  {
    if (!logoff_timestamp.has_value() || conf::machine::BEEP_PERIOD == 0ms)
      return false;

    CHECK_CONFIGURED(bool);

    return (power_state == PowerState::WaitingPowerOff &&
            std::chrono::system_clock::now() - logoff_timestamp.value() > config.value().grace_period);
  }

  /// @brief sets the machine power to on (true) or off (false)
  /// @param value setpoint
  auto Machine::power_relay(bool value) -> void
  {
    CHECK_CONFIGURED(void);
    ESP_LOGI(TAG, "Machine::power_relay : power set to %d", value);

    auto pin = config.value().relay_config.pin;

    if (config.value().relay_config.active_low)
    {
      digitalWrite(pin, value ? LOW : HIGH);
    }
    else
    {
      digitalWrite(pin, value ? HIGH : LOW);
    }

    if (value)
    {
      logoff_timestamp = std::nullopt;
      power_state = PowerState::PoweredOn;
    }
    else
    {
      power_state = PowerState::PoweredOff;
    }
  }

  /// @brief sets the machine power to on (true) or off (false)
  /// @param value setpoint
  auto Machine::power_mqtt(bool value) -> void
  {
    CHECK_CONFIGURED(void);

    ESP_LOGI(TAG, "Machine::power_mqtt : power set to %d", value);

    auto &mqtt_server = server.value().get();
    auto &act_config = config.value();

    String topic{act_config.mqtt_config.topic.data()};
    String payload = value ? act_config.mqtt_config.on_message.data() : act_config.mqtt_config.off_message.data();

    auto retries = 0;
    while (!mqtt_server.publish(topic, payload))
    {
      ESP_LOGE(TAG, "Error while publishing %s to %s", payload.c_str(), topic.c_str());

      mqtt_server.connect();
      Tasks::task_delay(conf::mqtt::TIMEOUT_REPLY_SERVER);
      retries++;
      if (retries > conf::mqtt::MAX_TRIES)
      {
        ESP_LOGW(TAG, "Unable to publish to MQTT (%d/%d)", retries, conf::mqtt::MAX_TRIES);
        return;
      }
    }

    if (value)
    {
      logoff_timestamp = std::nullopt;
      power_state = PowerState::PoweredOn;
    }
    else
    {
      power_state = PowerState::PoweredOff;
    }
  }

  auto Machine::power(bool on_or_off) -> void
  {
    CHECK_CONFIGURED(void);

    ESP_LOGI(TAG, "Machine::power : power set to %d", on_or_off);

    if (config.value().hasRelay())
    {
      power_relay(on_or_off);
    }
    if (config.value().hasMqttSwitch())
    {
      power_mqtt(on_or_off);
    }
  }

  auto Machine::getActiveUser() const -> FabUser
  {
    return current_user;
  }

  /// @brief Gets the duration the machine has been used
  /// @return milliseconds since the machine has been started
  auto Machine::getUsageDuration() const -> std::chrono::seconds
  {
    if (usage_start_timestamp.has_value())
    {
      return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - usage_start_timestamp.value());
    }
    return 0s;
  }

  auto Machine::getMachineName() const -> const std::string
  {
    CHECK_CONFIGURED(std::string);
    return std::string{config.value().machine_name.data()};
  }

  auto Machine::toString() const -> const std::string
  {
    std::stringstream sstream{};

    if (!config.has_value() || !server.has_value())
    {
      sstream << "Machine (not configured)";
      return sstream.str();
    }

    sstream << "Machine (ID:" << getMachineId().id;
    sstream << ", Name:" << getMachineName();
    sstream << ", IsFree: " << isFree();
    sstream << ", IsAllowed:" << allowed;
    sstream << ", PowerState:" << static_cast<int>(getPowerState());
    sstream << ", " << current_user.toString();
    sstream << ", UsageDuration (s):" << getUsageDuration().count();
    sstream << ", ShutdownImminent:" << isShutdownImminent();
    sstream << ", MaintenanceNeeded:" << maintenanceNeeded;
    sstream << ", " << config.value().toString();
    sstream << ", Active:" << active;
    sstream << ", Last logoff:" << (logoff_timestamp.has_value() ? logoff_timestamp.value().time_since_epoch().count() : 0);
    sstream << ", GracePeriod (s):" << getGracePeriod().count();
    sstream << ")";

    return sstream.str();
  }

  auto Machine::getAutologoffDelay() const -> std::chrono::seconds
  {
    CHECK_CONFIGURED(std::chrono::seconds);
    return config.value().autologoff;
  }

  auto Machine::setAutologoffDelay(std::chrono::seconds new_delay) -> void
  {
    CHECK_CONFIGURED(void);

    if (config.value().autologoff != new_delay)
    {
      ESP_LOGD(TAG, "Changing autologoff delay to %lld min",
               std::chrono::duration_cast<std::chrono::minutes>(config.value().autologoff).count());
    }

    config.value().autologoff = new_delay;
  }

  auto Machine::isAutologoffExpired() const -> bool
  {
    return getAutologoffDelay() > 0min &&
           getUsageDuration() > getAutologoffDelay();
  }

  auto Machine::setMachineName(const std::string &new_name) -> void
  {
    CHECK_CONFIGURED(void);

    if (config.value().machine_name != new_name)
    {
      ESP_LOGD(TAG, "Changing machine name to %s", new_name.data());
    }

    config.value().machine_name = new_name;
  }

  auto Machine::setMachineType(MachineType new_type) -> void
  {
    CHECK_CONFIGURED(void);

    if (config.value().machine_type != new_type)
      ESP_LOGD(TAG, "Changing machine type to %d", static_cast<uint8_t>(new_type));

    config.value().machine_type = new_type;
  }

  auto Machine::isConfigured() const -> bool
  {
    return config.has_value() && server.has_value();
  }

  auto Machine::getConfig() const -> std::optional<MachineConfig>
  {
    return config;
  }

  auto Machine::isAllowed() const -> bool
  {
    return allowed;
  }

  auto Machine::setAllowed(bool new_allowed) -> void
  {
    allowed = new_allowed;
  }

  auto Machine::isMaintenanceNeeded() const -> bool
  {
    return maintenanceNeeded;
  }

  auto Machine::setMaintenanceNeeded(bool new_maintenance_needed) -> void
  {
    maintenanceNeeded = new_maintenance_needed;
  }

  auto Machine::getGracePeriod() const -> std::chrono::seconds
  {
    CHECK_CONFIGURED(std::chrono::seconds);
    return config.value().grace_period;
  }

  auto Machine::setGracePeriod(std::chrono::seconds new_delay) -> void
  {
    CHECK_CONFIGURED(void);

    if (config.value().grace_period != new_delay)
    {
      ESP_LOGD(TAG, "Changing grace period to %lld seconds",
               new_delay.count());
    }

    config.value().grace_period = new_delay;
  }
} // namespace fablabbg