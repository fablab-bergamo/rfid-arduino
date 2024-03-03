#ifndef MOCK_MOCKMRFC522_HPP_
#define MOCK_MOCKMRFC522_HPP_

#include "Arduino.h"
#include <memory>
#include <optional>

#include "FabUser.hpp"
#include "MFRC522DriverPinSimple.h"
#include "MFRC522DriverSPI.h"
#include "MFRC522v2.h"

namespace fablabbg
{
  class MockMrfc522
  {
  private:
    std::optional<card::uid_t> uid{std::nullopt};
    std::optional<std::chrono::time_point<std::chrono::system_clock>> stop_uid_simulate_time{std::nullopt};
    std::optional<card::uid_t> getSimulatedUid() const;

  public:
    struct UidDriver
    {
      byte size; // Number of bytes in the UID. 4, 7 or 10.
      byte uidByte[10];
      byte sak; // The SAK (Select acknowledge) byte returned from the PICC after successful selection.
    };

    constexpr MockMrfc522(){};

    auto PICC_IsNewCardPresent() -> bool;
    auto PICC_ReadCardSerial() -> bool;
    void reset();
    auto PCD_Init() -> bool;
    auto PICC_WakeupA(byte *bufferATQA, byte &bufferSize) -> bool;
    auto PCD_PerformSelfTest() -> bool;
    auto getDriverUid() const -> MockMrfc522::UidDriver;
    void PCD_SetAntennaGain(MFRC522Constants::PCD_RxGain gain);
    void PCD_DumpVersionToSerial();

    void setUid(const std::optional<card::uid_t> &uid, const std::optional<std::chrono::milliseconds> &max_delay);
    void resetUid();

    static constexpr auto RxGainMax = MFRC522::PCD_RxGain::RxGain_max;
  };
} // namespace fablabbg

#endif // MOCK_MOCKMRFC522_HPP_