#include "mock/MockMrfc522.hpp"

namespace fablabbg
{
  auto MockMrfc522::getDriverUid() const -> MockMrfc522::UidDriver
  {
    UidDriver retVal{};
    memset(&retVal, 0, sizeof(retVal));
    if (getSimulatedUid().has_value())
    {
      memcpy(retVal.uidByte, &uid.value(), sizeof(uid.value()));
      retVal.size = sizeof(uid.value());
      retVal.sak = 1;
    }
    return retVal;
  }

  auto MockMrfc522::PICC_IsNewCardPresent() -> bool { return getSimulatedUid().has_value(); }

  auto MockMrfc522::PICC_ReadCardSerial() -> bool { return getSimulatedUid().has_value(); }

  void MockMrfc522::reset() { uid = std::nullopt; }

  auto MockMrfc522::PCD_Init() -> bool { return true; }

  auto MockMrfc522::PICC_WakeupA(byte *bufferATQA, byte &bufferSize) -> bool
  {
    if (getSimulatedUid().has_value())
    {
      return true;
    }
    return false;
  }

  auto MockMrfc522::PCD_PerformSelfTest() -> bool { return true; }

  auto MockMrfc522::PCD_SetAntennaGain(MFRC522Constants::PCD_RxGain gain) -> void {}

  auto MockMrfc522::PCD_DumpVersionToSerial() -> void {}

  auto MockMrfc522::setUid(const std::optional<card::uid_t> &uid, const std::optional<std::chrono::milliseconds> &max_delay) -> void
  {
    this->uid = uid;
    if (max_delay.has_value())
    {
      stop_uid_simulate_time = std::chrono::system_clock::now() + max_delay.value();
    }
    else
    {
      stop_uid_simulate_time = std::nullopt;
    }
  }

  auto MockMrfc522::resetUid() -> void
  {
    uid = std::nullopt;
    stop_uid_simulate_time = std::nullopt;
  }

  auto MockMrfc522::getSimulatedUid() const -> std::optional<card::uid_t>
  {
    if (stop_uid_simulate_time.has_value() && std::chrono::system_clock::now() > stop_uid_simulate_time.value())
    {
      return std::nullopt;
    }
    return uid;
  }
} // namespace fablabbg