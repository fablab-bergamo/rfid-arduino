#include <memory>

#include "Logging.hpp"
#include "RFIDWrapper.hpp"
#include "card.hpp"
#include "conf.hpp"
#include "pins.hpp"

namespace fablabbg
{
  /// @brief Constructor
  template <typename Driver>
  RFIDWrapper<Driver>::RFIDWrapper() : driver{std::make_unique<Driver>()} {};

  /// @brief indicates if a new card is present in the RFID chip antenna area
  /// @return true if a new card is present
  template <typename Driver>
  bool RFIDWrapper<Driver>::isNewCardPresent() const
  {
    auto result = driver->PICC_IsNewCardPresent();

    if (conf::debug::ENABLE_LOGS && result)
      ESP_LOGD(TAG, "isNewCardPresent=%d", result);

    return result;
  }

  /// @brief tries to read the card serial number
  /// @return true if successfull, result can be read with getUid()
  template <typename Driver>
  auto RFIDWrapper<Driver>::readCardSerial() const -> std::optional<card::uid_t>
  {
    auto result = driver->PICC_ReadCardSerial();
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
  template <typename Driver>
  auto RFIDWrapper<Driver>::cardStillThere(const card::uid_t original, std::chrono::milliseconds max_delay) const -> bool
  {
    auto start = std::chrono::system_clock::now();
    do
    {
      // Detect Tag without looking for collisions
      byte bufferATQA[2];
      byte bufferSize = sizeof(bufferATQA);

      if (driver->PICC_WakeupA(bufferATQA, bufferSize))
      {
        if (readCardSerial() == original)
          return true;
      }
      delay(20);
    } while (std::chrono::system_clock::now() - start < max_delay);

    return false;
  }

  /// @brief Performs a RFID chip self test
  /// @return true if successfull
  template <typename Driver>
  auto RFIDWrapper<Driver>::selfTest() const -> bool
  {
    auto result = driver->PCD_PerformSelfTest();
    if (conf::debug::ENABLE_LOGS)
    {
      ESP_LOGD(TAG, "RFID self test = %d", result);
    }
    return result;
  }

  /// @brief Performs a chip reset
  template <typename Driver>
  auto RFIDWrapper<Driver>::reset() const -> void
  {
    digitalWrite(pins.mfrc522.reset_pin, HIGH);
    delay(15);
    digitalWrite(pins.mfrc522.reset_pin, LOW);
    delay(15);
  }

  /// @brief Transforms the RFID acquired bytes into a uid_id object
  /// @return card ID
  template <typename Driver>
  auto RFIDWrapper<Driver>::getUid() const -> card::uid_t
  {
    std::array<uint8_t, conf::rfid_tags::UID_BYTE_LEN> arr{0};
    auto const &uid = driver->getDriverUid();
    memcpy(arr.data(), uid.uidByte, std::min(conf::rfid_tags::UID_BYTE_LEN, uid.size));

    auto c = card::from_array(arr);

    return c;
  }

  /// @brief Initializes RFID chip including self test
  template <typename Driver>
  auto RFIDWrapper<Driver>::init_rfid() const -> bool
  {

    ESP_LOGI(TAG, "Configuring SPI RFID (SCK=%d, MISO=%d, MOSI=%d, SDA=%d) RESET=%d",
             pins.mfrc522.sck_pin, pins.mfrc522.miso_pin, pins.mfrc522.mosi_pin,
             pins.mfrc522.sda_pin, pins.mfrc522.reset_pin);

    reset();

    if (!driver->PCD_Init())
    {
      ESP_LOGE(TAG, "mfrc522 Init failed");
      return false;
    }

    if (conf::debug::ENABLE_LOGS)
      driver->PCD_DumpVersionToSerial();

    driver->PCD_SetAntennaGain(Driver::RxGainMax);
    delay(5);

    if (!driver->PCD_PerformSelfTest())
    {
      ESP_LOGE(TAG, "Self-test failure for RFID");
      return false;
    }
    return true;
  }

  template <typename Driver>
  auto RFIDWrapper<Driver>::getDriver() -> Driver &
  {
    return *driver.get();
  }
} // namespace fablabbg