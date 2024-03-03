#ifndef MACHINE_HPP_
#define MACHINE_HPP_

#include "FabUser.hpp"
#include "MachineConfig.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace fablabbg
{
  class FabBackend;

  class Machine
  {
  public:
    enum class PowerState : uint8_t
    {
      Unknown,
      PoweredOn,
      WaitingPowerOff,
      PoweredOff,
    };

    constexpr Machine(){};

    ~Machine() = default;
    Machine(const Machine &) = delete;             // copy constructor
    Machine &operator=(const Machine &x) = delete; // copy assignment
    Machine(Machine &&) = delete;                  // move constructor
    Machine &operator=(Machine &&) = delete;       // move assignment

    auto getActiveUser() const -> FabUser;

    /// @brief Configure the machine, it must be called before most methods.
    void configure(const MachineConfig &new_config, FabBackend &serv);

    [[nodiscard]] auto getMachineId() const -> MachineID;
    [[nodiscard]] auto getMachineName() const -> const std::string;

    /// @brief Duration of the current usage, or 0s
    [[nodiscard]] auto getUsageDuration() const -> std::chrono::seconds;

    [[nodiscard]] auto getAutologoffDelay() const -> std::chrono::seconds;

    [[nodiscard]] auto getGracePeriod() const -> std::chrono::seconds;

    /// @brief Try to login the user and start the usage timer
    auto login(const FabUser &user) -> bool;

    /// @brief Logoff the user and stop the usage timer
    auto logout() -> void;

    /// @brief Sets the delay after which the user will be logged off automatically
    auto setAutologoffDelay(std::chrono::seconds new_delay) -> void;

    /// @brief Sets the idle period after which the machine will power off automatically
    auto setGracePeriod(std::chrono::seconds new_delay) -> void;

    /// @brief Powers the machine on or off using relay/MQTT/both
    /// @param on_or_off new power state
    auto power(bool on_or_off) -> void;

    /// @brief Sets the machine name as per backend configuration
    /// @param new_name Will be shown on LCD, keep it short.
    auto setMachineName(const std::string &new_name) -> void;

    /// @brief Sets the machine type as per backend configuration
    auto setMachineType(MachineType new_type) -> void;

    /// @brief Returns the current power state of the machine
    [[nodiscard]] auto getPowerState() const -> PowerState;

    /// @brief Indicates if the machine will power down in less than BEEP_REMAINING_MINUTES
    [[nodiscard]] auto isShutdownImminent() const -> bool;

    /// @brief Indicates is the machine is not used by anybody
    [[nodiscard]] auto isFree() const -> bool;

    /// @brief Indicates if POWEROFF_DELAY_MINUTES delay has expired,and the machine is still idle
    [[nodiscard]] auto canPowerOff() const -> bool;

    [[nodiscard]] auto toString() const -> const std::string;

    /// @brief Indicates ff the user shall be logged off automatically
    [[nodiscard]] auto isAutologoffExpired() const -> bool;

    /// @brief Indicates if the machine has been configured
    [[nodiscard]] auto isConfigured() const -> bool;

    /// @brief Returns the current configuration of the machine, used for testing.
    [[nodiscard]] auto getConfig() const -> std::optional<MachineConfig>;

    auto isAllowed() const -> bool;
    auto setAllowed(bool new_allowed) -> void;
    auto isMaintenanceNeeded() const -> bool;
    auto setMaintenanceNeeded(bool new_maintenance_needed) -> void;

  private:
    std::optional<MachineConfig> config{std::nullopt};
    std::optional<std::reference_wrapper<FabBackend>> server{std::nullopt};

    bool active{false};
    FabUser current_user{};

    std::optional<std::chrono::time_point<std::chrono::system_clock>> usage_start_timestamp{std::nullopt}; // When did the machine start?
    std::optional<std::chrono::time_point<std::chrono::system_clock>> logoff_timestamp{std::nullopt};      // When did the last user log off?
    PowerState power_state{PowerState::PoweredOff};

    /// @brief If true, machine needs maintenance
    bool maintenanceNeeded{false};
    /// @brief If true, machine is allowed to be used by anybody
    bool allowed{true};

    auto power_mqtt(bool on_or_off) -> void;
    auto power_relay(bool on_or_off) -> void;
  };
} // namespace fablabbg
#endif // MACHINE_HPP_