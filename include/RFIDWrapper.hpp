#ifndef RFIDWRAPPER_HPP_
#define RFIDWRAPPER_HPP_

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "BaseRfidWrapper.hpp"
#include "card.hpp"
#include "conf.hpp"

namespace fablabbg
{
  template <typename Driver>
  class RFIDWrapper final : public BaseRFIDWrapper
  {
  private:
    std::unique_ptr<Driver> driver;

  public:
    RFIDWrapper();

    /// @brief Initializes the RFID driver
    [[nodiscard]] auto init_rfid() const -> bool override;

    /// @brief Returns true if a card is present in the field
    [[nodiscard]] auto isNewCardPresent() const -> bool override;

    /// @brief Returns true if the card is in the field, waiting up to max_delay to confirm presence
    /// @details This function may return immediately if the card responds quickly
    /// @param original card to look for
    /// @param max_delay maximum delay to contact the card
    /// @return true if the card is found
    [[nodiscard]] auto cardStillThere(const card::uid_t original, std::chrono::milliseconds max_delay) const -> bool override;

    /// @brief Reads the card serial number
    /// @return std::nullopt if the card is not present, the card serial otherwise
    [[nodiscard]] auto readCardSerial() const -> std::optional<card::uid_t> override;

    [[nodiscard]] auto selfTest() const -> bool override;

    auto reset() const -> void override;

    [[nodiscard]] auto getUid() const -> card::uid_t override;

    /// @brief Returns the driver object for testing/simulation
    Driver &getDriver();

    RFIDWrapper(const RFIDWrapper &) = delete;             // copy constructor
    RFIDWrapper &operator=(const RFIDWrapper &x) = delete; // copy assignment
    RFIDWrapper(RFIDWrapper &&) = delete;                  // move constructor
    RFIDWrapper &operator=(RFIDWrapper &&) = delete;       // move assignment
  };
} // namespace fablabbg

#include "RFIDWrapper.tpp"

#endif // RFIDWRAPPER_HPP_