#include <memory>

#include "RFIDWrapper.hpp"
#include "pins.hpp"
#include "MFRC522v2.h"
#include "MFRC522DriverSPI.h"
#include "MFRC522DriverPinSimple.h"
#include "MFRC522Debug.h"
#include "conf.hpp"
#include "card.hpp"

namespace fablabbg
{
  RFIDWrapper::RFIDWrapper()
  {
    // Configure SPI bus
    SPI.begin(pins.mfrc522.sck_pin, pins.mfrc522.miso_pin, pins.mfrc522.mosi_pin, pins.mfrc522.sda_pin);

    pinMode(pins.mfrc522.reset_pin, OUTPUT);

    // Smart pointers members destructors will run & free the memory, when class will be distructed.
    rfid_simple_driver = std::make_unique<MFRC522DriverPinSimple>(pins.mfrc522.sda_pin);
    spi_rfid_driver = std::make_unique<MFRC522DriverSPI>(*rfid_simple_driver); // Create SPI driver.
    mfrc522 = std::make_unique<MFRC522>(*spi_rfid_driver);
  }

  /// @brief indicates if a new card is present in the RFID chip antenna area
  /// @return true if a new card is present
  bool RFIDWrapper::isNewCardPresent() const
  {
    auto result = mfrc522->PICC_IsNewCardPresent();

    if (conf::debug::ENABLE_LOGS && result)
      Serial.printf("isNewCardPresent=%d\r\n", result);

    return result;
  }

  /// @brief tries to read the card serial number
  /// @return true if successfull, result can be read with getUid()
  std::optional<card::uid_t> RFIDWrapper::readCardSerial() const
  {
    auto result = mfrc522->PICC_ReadCardSerial();

    if (conf::debug::ENABLE_LOGS)
    {
      Serial.printf("readCardSerial=%d (SAK=%d, Size=%d)\r\n", result,
                    mfrc522->uid.sak, mfrc522->uid.size);
    }
    if (result)
    {
      return getUid();
    }
    else
    {
      return std::nullopt;
    }
  }

  /// @brief indicates if the card is still present in the RFID chip antenna area
  /// @param original the card ID to check
  bool RFIDWrapper::cardStillThere(const card::uid_t original) const
  {
    static constexpr auto NB_TRIES = 3;
    for (auto i = 0; i < NB_TRIES; i++)
    {
      // Detect Tag without looking for collisions
      byte bufferATQA[2];
      byte bufferSize = sizeof(bufferATQA);

      MFRC522::StatusCode result = mfrc522->PICC_WakeupA(bufferATQA, &bufferSize);

      if (result == MFRC522::StatusCode::STATUS_OK)
      {
        if (readCardSerial() && getUid() == original)
          return true;
      }
      delay(5);
    }
    return false;
  }

  /// @brief Performs a RFID chip self test
  /// @return true if successfull
  bool RFIDWrapper::selfTest() const
  {
    auto result = mfrc522->PCD_PerformSelfTest();
    if (conf::debug::ENABLE_LOGS)
    {
      Serial.printf("RFID self test = %d\r\n", result);
    }
    return result;
  }

  /// @brief Performs a chip reset
  void RFIDWrapper::reset() const
  {
    digitalWrite(pins.mfrc522.reset_pin, 1);
    delay(25);
    digitalWrite(pins.mfrc522.reset_pin, 0);
    delay(25);
  }

  /// @brief Transforms the RFID acquired bytes into a uid_id object
  /// @return card ID
  card::uid_t RFIDWrapper::getUid() const
  {
    uint8_t arr[conf::rfid_tags::UID_BYTE_LEN]{0};

    memcpy(arr, mfrc522->uid.uidByte, std::min(conf::rfid_tags::UID_BYTE_LEN, mfrc522->uid.size));

    auto c = card::from_array(arr);

    if (conf::debug::ENABLE_LOGS)
    {
      auto str_id = card::uid_str(c);
      Serial.printf("getUid=%s\r\n", str_id.c_str());
    }

    return c;
  }

  /// @brief Initializes RFID chip including self test
  bool RFIDWrapper::init_rfid() const
  {

    if (conf::debug::ENABLE_LOGS)
    {
      constexpr auto MAX_LEN = 80;
      char buffer[MAX_LEN] = {0};
      if (snprintf(buffer, sizeof(buffer), "Configuring SPI RFID (SCK=%d, MISO=%d, MOSI=%d, SDA=%d) RESET=%d",
                   pins.mfrc522.sck_pin, pins.mfrc522.miso_pin, pins.mfrc522.mosi_pin,
                   pins.mfrc522.sda_pin, pins.mfrc522.reset_pin) > 0)
        Serial.println(buffer);
    }

    reset();

    if (!mfrc522->PCD_Init())
    {
      Serial.println("mfrc522 Init failed");
      return false;
    }

    if (conf::debug::ENABLE_LOGS)
      MFRC522Debug::PCD_DumpVersionToSerial(*mfrc522, Serial);

    mfrc522->PCD_SetAntennaGain(MFRC522::PCD_RxGain::RxGain_max);
    delay(5);

    if (!mfrc522->PCD_PerformSelfTest())
    {
      Serial.println("Self-test failure for RFID");
      return false;
    }
    return true;
  }
} // namespace fablabbg