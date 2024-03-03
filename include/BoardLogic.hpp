#ifndef BOARDLOGIC_HPP_
#define BOARDLOGIC_HPP_

#include "AuthProvider.hpp"
#include "BaseLCDWrapper.hpp"
#include "BaseRfidWrapper.hpp"
#include "FabBackend.hpp"
#include "FabUser.hpp"
#include "Led.hpp"
#include "Machine.hpp"
#include "card.hpp"
#include "pins.hpp"
#include "secrets.hpp"
#include <Adafruit_NeoPixel.h>

namespace fablabbg
{
  class BoardLogic
  {
  public:
    enum class Status : uint8_t
    {
      Clear,
      MachineFree,
      LoggedIn,
      LoginDenied,
      Busy,
      LoggedOut,
      Connecting,
      Connected,
      AlreadyInUse,
      MachineInUse,
      Offline,
      NotAllowed,
      Verifying,
      MaintenanceNeeded,
      MaintenanceQuery,
      MaintenanceDone,
      Error,
      ErrorHardware,
      PortalFailed,
      PortalSuccess,
      PortalStarting,
      Booting,
      ShuttingDown,
      OTAStarting,
      FactoryDefaults,
      OTAError,
    };

    BoardLogic() = default;

    auto refreshFromServer() -> void;
    auto onNewCard(card::uid_t uid) -> void;
    auto logout() -> void;
    auto changeStatus(Status newStatus) -> void;
    auto updateLCD() const -> void;
    auto beep_ok() const -> void;
    auto beep_failed() const -> void;
    auto blinkLed(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) -> void;
    auto checkRfid() -> void;
    auto checkPowerOff() -> void;
    auto setAutologoffDelay(std::chrono::seconds delay) -> void;
    auto setWhitelist(WhiteList whitelist) -> void;
    auto setRebootRequest(bool request) -> void;

    auto board_init() -> bool;
    auto configure(BaseRFIDWrapper &rfid, BaseLCDWrapper &lcd) -> bool;
    auto reconfigure() -> bool;

    [[nodiscard]] auto getStatus() const -> Status;
    [[nodiscard]] auto getRebootRequest() const -> bool;
    [[nodiscard]] auto getServer() -> FabBackend &;
    [[nodiscard]] auto getMachineForTesting() -> Machine &;
    [[nodiscard]] auto getMachine() const -> const Machine &;
    [[nodiscard]] auto authorize(const card::uid_t uid) -> bool;

    // copy reference
    BoardLogic &operator=(const BoardLogic &board) = delete;
    // copy constructor
    BoardLogic(const BoardLogic &) = delete;
    // move constructor
    BoardLogic(BoardLogic &&) = default;
    // move assignment
    BoardLogic &operator=(BoardLogic &&) = default;

  private:
    Status status{Status::Clear};
    Led led;
    FabBackend server;
    std::optional<std::reference_wrapper<BaseRFIDWrapper>> rfid{std::nullopt}; // Configured at runtime
    std::optional<std::reference_wrapper<BaseLCDWrapper>> lcd{std::nullopt};   // Configured at runtime
    bool ready_for_a_new_card{true};
    bool led_status{false};

    Machine machine;
    AuthProvider auth{secrets::cards::whitelist};

    BaseRFIDWrapper &getRfid() const;
    BaseLCDWrapper &getLcd() const;

    bool rebootRequest{false};

    auto longTap(const card::uid_t card, const std::string &short_prompt) const -> bool;
  };
} // namespace fablabbg
#endif // BOARDLOGIC_HPP_