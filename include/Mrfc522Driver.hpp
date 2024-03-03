#ifndef MRFC522DRIVER_HPP_
#define MRFC522DRIVER_HPP_

#include "Arduino.h"
#include <memory>

#include "FabUser.hpp"
#include "MFRC522Debug.h"
#include "MFRC522DriverPinSimple.h"
#include "MFRC522DriverSPI.h"
#include "MFRC522v2.h"
#include "pins.hpp"

namespace fablabbg
{
  class Mrfc522Driver
  {
  private:
    std::unique_ptr<MFRC522DriverPinSimple> rfid_simple_driver;
    std::unique_ptr<MFRC522DriverSPI> spi_rfid_driver;
    std::unique_ptr<MFRC522> mfrc522;

  public:
    struct UidDriver
    {
      byte size; // Number of bytes in the UID. 4, 7 or 10.
      byte uidByte[10];
      byte sak; // The SAK (Select acknowledge) byte returned from the PICC after successful selection.
    };

    Mrfc522Driver();
    auto PICC_IsNewCardPresent() -> bool;
    auto PICC_ReadCardSerial() -> bool;
    auto reset() -> void;
    auto PCD_Init() -> bool;
    auto PICC_WakeupA(byte *bufferATQA, byte bufferSize) -> bool;
    auto PCD_PerformSelfTest() -> bool;
    auto getDriverUid() const -> UidDriver;
    auto PCD_SetAntennaGain(MFRC522Constants::PCD_RxGain gain) -> void;
    auto PCD_DumpVersionToSerial() -> void;

    static constexpr auto RxGainMax = MFRC522::PCD_RxGain::RxGain_max;
  };
} // namespace fablabbg

#endif // MRFC522DRIVER_HPP_