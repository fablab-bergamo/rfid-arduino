#include "MachineConfig.hpp"
#include <sstream>

namespace fablabbg
{
  auto MachineConfig::toString() const -> const std::string
  {
    std::stringstream sstream;

    sstream << "MachineConfig (ID:" << machine_id.id;
    sstream << ", Name:" << machine_name;
    sstream << ", Type:" << static_cast<int>(machine_type);
    sstream << ", RelayConfig (pin:" << static_cast<int>(relay_config.pin);
    sstream << ", active_low:" << relay_config.active_low;
    sstream << "), MQTTConfig (topic:" << mqtt_config.topic;
    sstream << ", on_message:" << mqtt_config.on_message;
    sstream << ", off_message:" << mqtt_config.off_message;
    sstream << "), AutologoffDelay (min):" << std::chrono::duration_cast<std::chrono::minutes>(autologoff).count();
    sstream << ", HasRelay:" << hasRelay();
    sstream << ", HasMqttSwitch:" << hasMqttSwitch();
    sstream << ", GracePeriod (s):" << grace_period.count();
    sstream << "))";

    return sstream.str();
  }

  auto MachineConfig::hasRelay() const -> bool
  {
    return relay_config.pin != NO_PIN;
  }

  auto MachineConfig::hasMqttSwitch() const -> bool
  {
    return !mqtt_config.topic.empty();
  }
} // namespace fablabbg