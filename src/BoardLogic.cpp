#include <sstream>

#include "Arduino.h"
#include "WiFi.h"
#include <Adafruit_NeoPixel.h>
#include <esp_task_wdt.h>

#include "AuthProvider.hpp"
#include "BaseLCDWrapper.hpp"
#include "BaseRfidWrapper.hpp"
#include "BoardLogic.hpp"
#include "FabBackend.hpp"
#include "Logging.hpp"
#include "Machine.hpp"
#include "SavedConfig.hpp"
#include "Tasks.hpp"
#include "conf.hpp"
#include "pins.hpp"
#include "secrets.hpp"

#ifndef GIT_VERSION
#define GIT_VERSION "??????"
#endif

namespace fablabbg
{
  using milliseconds = std::chrono::milliseconds;

  BaseRFIDWrapper &BoardLogic::getRfid() const
  {
    if (rfid.has_value())
      return rfid.value().get();
    else
    {
      ESP_LOGE(TAG, "RFID not initialized");
      while (true)
      {
        Tasks::task_delay(1s);
      };
    }
  }

  /// @brief connects and polls the server for up-to-date machine information
  void BoardLogic::refreshFromServer()
  {
    ESP_LOGD(TAG, "BoardLogic::refreshFromServer() called");

    if (server.connect())
    {
      // Check the configured machine data from the server
      auto result = server.checkMachine();
      if (result->request_ok)
      {
        if (result->is_valid)
        {
          machine.setMaintenanceNeeded(result->maintenance);
          machine.setAllowed(result->allowed);
          machine.setAutologoffDelay(std::chrono::minutes(result->logoff));
          machine.setGracePeriod(std::chrono::minutes(result->grace));
          machine.setMachineName(result->name);
          MachineType mt = static_cast<MachineType>(result->type);
          machine.setMachineType(mt);

          ESP_LOGD(TAG, "Machine data updated:%s", machine.toString().c_str());
        }
        else
        {
          ESP_LOGW(TAG, "The configured machine ID %u is unknown to the server\r\n", machine.getMachineId().id);
        }
      }
    }
  }

  /// @brief Called when a RFID tag has been detected
  void BoardLogic::onNewCard(card::uid_t uid)
  {
    ESP_LOGD(TAG, "New card present");

    if (!ready_for_a_new_card)
    {
      return;
    }
    ready_for_a_new_card = false;

    if (machine.isFree())
    {
      // machine is free
      if (!authorize(uid))
      {
        ESP_LOGI(TAG, "Login failed for %s", card::uid_str(uid).c_str());
      }
      Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
      refreshFromServer();
      return;
    }

    // machine is busy
    if (machine.getActiveUser().card_uid == uid)
    {
      // we can logout. we should require that the card stays in the field for some seconds, to prevent accidental logout. maybe sound a buzzer?
      logout();
    }
    else
    {
      // user is not the same, display who is using it
      changeStatus(Status::AlreadyInUse);
    }
    Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
    return;
  }

  /// @brief Removes the current machine user and changes the status to LoggedOut
  void BoardLogic::logout()
  {
    auto result = server.finishUse(machine.getActiveUser().card_uid,
                                   machine.getUsageDuration());

    ESP_LOGI(TAG, "Logout, result finishUse: %d", result->request_ok);

    machine.logout();
    changeStatus(Status::LoggedOut);
    beep_ok();
    Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
  }

  /// @brief Asks the user to keep the RFID tag on the reader as confirmation
  /// @return True if the user accepted, False if user bailed out
  bool BoardLogic::longTap(const card::uid_t card, const std::string &short_prompt) const
  {
    constexpr auto STEPS_COUNT = 6;
    constexpr milliseconds delay_per_step = std::chrono::duration_cast<std::chrono::milliseconds>(conf::machine::LONG_TAP_DURATION) / STEPS_COUNT;
    const BoardInfo bi = {server.isOnline(), machine.getPowerState(), machine.isShutdownImminent()};

    for (auto step = 0; step < STEPS_COUNT; step++)
    {
      std::stringstream ss;
      ss << short_prompt << " " << step << "/" << STEPS_COUNT;
      getLcd().setRow(1, ss.str());
      getLcd().update(bi);

      auto start = std::chrono::system_clock::now();
      if (!getRfid().cardStillThere(card, delay_per_step))
      {
        getLcd().setRow(1, "* ANNULLATO *");
        getLcd().update(bi);
        return false;
      }

      // cardStillThere may have returned immediately, so we need to wait a bit
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start);
      if (delay_per_step - elapsed > 10ms)
      {
        Tasks::task_delay(delay_per_step - elapsed);
      }
    }

    getLcd().setRow(1, "* CONFERMATO *");
    getLcd().update(bi);
    return true;
  }

  /// @brief Checks if the card UID is valid, and tries to check the user in to the machine.
  /// @param uid card uid
  /// @return true if the user is now logged on to the machine
  bool BoardLogic::authorize(card::uid_t uid)
  {
    changeStatus(Status::Verifying);
    FabUser user;

    user.authenticated = false;
    user.holder_name = "?";
    user.card_uid = uid;
    user.user_level = FabUser::UserLevel::Unknown;

    auto response = auth.tryLogin(uid, server);
    if (!response.has_value())
    {
      ESP_LOGI(TAG, "Failed login for %llu", uid);
      changeStatus(Status::LoginDenied);
      beep_failed();
      return false;
    }

    user = response.value();

    if (!machine.isAllowed())
    {
      ESP_LOGI(TAG, "Login refused due to machine not allowed");
      changeStatus(Status::NotAllowed);
      beep_failed();
      return false;
    }

    if (machine.isMaintenanceNeeded())
    {
      if (conf::machine::MAINTENANCE_BLOCK &&
          user.user_level < FabUser::UserLevel::FabStaff)
      {
        changeStatus(Status::MaintenanceNeeded);
        beep_failed();
        Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
        return false;
      }
      if (user.user_level >= FabUser::UserLevel::FabStaff)
      {
        beep_ok();
        changeStatus(Status::MaintenanceQuery);

        if (longTap(user.card_uid, "Registra"))
        {
          auto maint_resp = server.registerMaintenance(user.card_uid);
          if (!maint_resp->request_ok)
          {
            beep_failed();
            changeStatus(Status::Error);
            Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
            // Allow bypass for admins
            if (user.user_level == FabUser::UserLevel::FabAdmin)
            {
              machine.setMaintenanceNeeded(false);
            }
          }
          else
          {
            changeStatus(Status::MaintenanceDone);
            machine.setMaintenanceNeeded(false);
            beep_ok();
            Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
          }
          // Proceed to log-on the staff member to the machine in all cases
        }
      }
    }

    if (machine.login(user))
    {
      auto result = server.startUse(machine.getActiveUser().card_uid);
      ESP_LOGI(TAG, "Login, result startUse: %d", result->request_ok);
      changeStatus(Status::LoggedIn);
      beep_ok();
    }
    else
    {
      changeStatus(Status::NotAllowed);
      beep_failed();
      Tasks::task_delay(conf::lcd::SHORT_MESSAGE_DELAY);
    }

    return true;
  }

  /// @brief Initializes LCD and RFID classes
  bool BoardLogic::board_init()
  {
    ESP_LOGD(TAG, "Board initialization...");

    auto success = getLcd().begin();
    success &= getRfid().init_rfid();

    // Setup buzzer pin for ESP32
    success &= (ledcSetup(conf::buzzer::LEDC_PWM_CHANNEL, conf::buzzer::BEEP_HZ, 10U) != 0);
    ledcAttachPin(pins.buzzer.pin, conf::buzzer::LEDC_PWM_CHANNEL);

    ESP_LOGI(TAG, "Board initialization complete, success = %d", success);

    return success;
  }

  /// @brief Sets the board in the state given.
  /// @param new_state new state
  void BoardLogic::changeStatus(Status new_state)
  {
    if (status != new_state)
    {
      ESP_LOGI(TAG, "** Changing board state to %d", static_cast<int>(new_state));
    }

    status = new_state;
    updateLCD();
  }

  /// @brief Updates the LCD screen as per the current status
  void BoardLogic::updateLCD() const
  {
    char buffer[conf::lcd::COLS + 1] = {0}; // Null terminated strings
    std::string user_name, machine_name, uid_str;

    getLcd().showConnection(true);
    getLcd().showPower(true);

    user_name = machine.getActiveUser().holder_name;
    machine_name = machine.isConfigured() ? machine.getMachineName() : "-";

    if (rfid.has_value())
    {
      auto uid = rfid.value().get().getUid();
      uid_str = card::uid_str(uid);
    }
    else
    {
      uid_str = "????????";
    }

    switch (status)
    {
    case Status::Clear:
      getLcd().clear();
      break;
    case Status::MachineFree:
      getLcd().setRow(0, machine_name);
      if (machine.isMaintenanceNeeded())
      {
        getLcd().setRow(1, ">Manutenzione<");
      }
      else if (!machine.isAllowed())
      {
        getLcd().setRow(1, "> BLOCCATA <");
      }
      else
      {
        getLcd().setRow(1, "Avvicina carta");
      }
      break;
    case Status::AlreadyInUse:
      getLcd().setRow(0, "In uso da");
      getLcd().setRow(1, user_name);
      break;
    case Status::LoggedIn:
      getLcd().setRow(0, "Inizio uso");
      getLcd().setRow(1, user_name);
      break;
    case Status::LoginDenied:
      getLcd().setRow(0, "Carta ignota");
      getLcd().setRow(1, uid_str);
      break;
    case Status::LoggedOut:
      getLcd().setRow(0, "Arrivederci");
      getLcd().setRow(1, user_name);
      break;
    case Status::Connecting:
      getLcd().setRow(0, "Connessione");
      getLcd().setRow(1, "al server MQTT");
      break;
    case Status::Connected:
      getLcd().setRow(0, "Connesso");
      getLcd().setRow(1, "");
      break;
    case Status::MachineInUse:
      if (snprintf(buffer, sizeof(buffer), "Ciao %s", user_name.c_str()) > 0)
        getLcd().setRow(0, buffer);
      getLcd().setRow(1, getLcd().convertSecondsToHHMMSS(machine.getUsageDuration()));
      break;
    case Status::Busy:
      getLcd().setRow(0, "Elaborazione...");
      getLcd().setRow(1, "");
      break;
    case Status::Offline:
      getLcd().setRow(0, "OFFLINE MODE");
      getLcd().setRow(1, "");
      break;
    case Status::NotAllowed:
      getLcd().setRow(0, "Blocco");
      getLcd().setRow(1, "amministrativo");
      break;
    case Status::Verifying:
      getLcd().setRow(0, "VERIFICA IN");
      getLcd().setRow(1, "CORSO");
      break;
    case Status::MaintenanceNeeded:
      getLcd().setRow(0, "Blocco per");
      getLcd().setRow(1, "manutenzione");
      break;
    case Status::MaintenanceQuery:
      getLcd().setRow(0, "Manutenzione?");
      getLcd().setRow(1, "Registra");
      break;
    case Status::MaintenanceDone:
      getLcd().setRow(0, "Manutenzione");
      getLcd().setRow(1, "registrata");
      break;
    case Status::Error:
      getLcd().setRow(0, "Errore");
      getLcd().setRow(1, "V" GIT_VERSION);
      break;
    case Status::ErrorHardware:
      getLcd().setRow(0, "Errore HW");
      getLcd().setRow(1, "V" GIT_VERSION);
      break;
    case Status::PortalFailed:
      getLcd().setRow(0, "Errore portale");
      getLcd().setRow(1, WiFi.softAPIP().toString().c_str());
      break;
    case Status::PortalSuccess:
      getLcd().setRow(0, "AP config OK");
      getLcd().setRow(1, "V" GIT_VERSION);
      break;
    case Status::PortalStarting:
      getLcd().setRow(0, "Apri portale");
      getLcd().setRow(1, WiFi.softAPIP().toString().c_str());
      break;
    case Status::Booting:
      getLcd().setRow(0, "Avvio...");
      getLcd().setRow(1, "V" GIT_VERSION);
      break;
    case Status::ShuttingDown:
      getLcd().setRow(0, machine_name);
      getLcd().setRow(1, "In spegnimento!");
      break;
    case Status::OTAStarting:
      getLcd().setRow(0, "Aggiornamento");
      getLcd().setRow(1, "OTA...");
      break;
    case Status::FactoryDefaults:
      getLcd().setRow(0, "EEPROM reset!");
      getLcd().setRow(1, "Riavvio richiesto");
      break;
    case Status::OTAError:
      getLcd().setRow(0, "Errore OTA");
      getLcd().setRow(1, "");
      break;
    default:
      getLcd().setRow(0, "Unhandled status");
      if (snprintf(buffer, sizeof(buffer), "Value %d", static_cast<int>(status)) > 0)
        getLcd().setRow(1, buffer);
      break;
    }
    BoardInfo bi = {server.isOnline(), machine.getPowerState(), machine.isShutdownImminent()};
    getLcd().update(bi, false);
  }

  /// @brief Gets the current board status
  /// @return board status
  auto BoardLogic::getStatus() const -> BoardLogic::Status
  {
    return status;
  }

  void BoardLogic::beep_ok() const
  {
    if (conf::buzzer::STANDARD_BEEP_DURATION > 0ms)
    {
      ledcWriteTone(conf::buzzer::LEDC_PWM_CHANNEL, conf::buzzer::BEEP_HZ);
      Tasks::task_delay(conf::buzzer::STANDARD_BEEP_DURATION);
      ledcWrite(conf::buzzer::LEDC_PWM_CHANNEL, 0UL);
    }
  }

  void BoardLogic::beep_failed() const
  {
    if (conf::buzzer::STANDARD_BEEP_DURATION > 0ms)
    {
      for (auto i = 0; i < conf::buzzer::NB_BEEPS; i++)
      {
        ledcWriteTone(conf::buzzer::LEDC_PWM_CHANNEL, conf::buzzer::BEEP_HZ);
        Tasks::task_delay(conf::buzzer::STANDARD_BEEP_DURATION);
        ledcWrite(conf::buzzer::LEDC_PWM_CHANNEL, 0UL);
        Tasks::task_delay(conf::buzzer::STANDARD_BEEP_DURATION);
      }
    }
  }

  /// @brief Configures the board with the given references
  bool BoardLogic::configure(BaseRFIDWrapper &rfid, BaseLCDWrapper &lcd)
  {
    this->rfid = rfid;
    this->lcd = lcd;
    return reconfigure();
  }

  /// @brief Configures the board with the given references
  bool BoardLogic::reconfigure()
  {
    auto success = true;

    // Load configuration
    auto config = SavedConfig::LoadFromEEPROM();
    if (!config)
    {
      ESP_LOGW(TAG, "No configuration found in EEPROM, using defaults.");
      auto new_config = SavedConfig::DefaultConfig();
      success &= new_config.SaveToEEPROM();
      config = new_config;
    }

    ESP_LOGD(TAG, "Configuration found in EEPROM: %s", config->toString().c_str());

    server.configure(config.value());

    MachineID mid{(uint16_t)atoi(config.value().machine_id)};
    MachineConfig machine_conf(mid,
                               conf::default_config::machine_type,
                               std::string{conf::default_config::machine_name},
                               pins.relay,
                               std::string{config.value().machine_topic},
                               conf::machine::DEFAULT_AUTO_LOGOFF_DELAY,
                               conf::machine::DEFAULT_GRACE_PERIOD);

    machine.configure(machine_conf, server);

    return success;
  }

  /// @brief Blinks the LED
  void BoardLogic::blinkLed(uint8_t r, uint8_t g, uint8_t b)
  {
    led.set(Led::Status::Blinking);

    if (server.isOnline())
    {
      if (!machine.isAllowed() || machine.isMaintenanceNeeded())
      {
        led.setColor(64, 0, 0); // Red
      }
      else
      {
        if (machine.isFree())
        {
          if (machine.isShutdownImminent())
          {
            led.setColor(64, 0, 64); // Purple
          }
          else
          {
            led.setColor(0, 64, 0); // Green
          }
        }
        else
        {
          led.setColor(0, 0, 64); // Blue
        }
      }
    }
    else
    {
      led.setColor(128, 255, 0); // Orange
    }

    // User override
    if (r != 0 || g != 0 || b != 0)
    {
      led.setColor(r, g, b);
    }

    // Color override
    if (status == Status::ErrorHardware || status == Status::Error)
    {
      led.setColor(255, 0, 0); // Red
    }

    led.update();
  }

  /// @brief Checks if a new card is present
  void BoardLogic::checkRfid()
  {
    auto &rfid = getRfid();

    // check if there is a card
    if (rfid.isNewCardPresent())
    {
      auto result = rfid.readCardSerial();
      if (result)
      {
        onNewCard(result.value());
      }
      return;
    }

    // No new card present
    ready_for_a_new_card = true;
    if (machine.isFree())
    {
      changeStatus(Status::MachineFree);
    }
    else
    {
      changeStatus(Status::MachineInUse);
    }
  }

  /// @brief Checks if the machine must be powered off
  void BoardLogic::checkPowerOff()
  {
    if (machine.canPowerOff())
    {
      machine.power(false);
    }
  }

  /// @brief returns a modificable machine for testing only
  /// @return machine
  Machine &BoardLogic::getMachineForTesting()
  {
    return machine;
  }

  /// @brief Gets the current machine
  /// @return a machine object
  const Machine &BoardLogic::getMachine() const
  {
    return machine;
  }

  BaseLCDWrapper &BoardLogic::getLcd() const
  {
    if (lcd.has_value())
    {
      return lcd.value().get();
    }
    else
    {
      ESP_LOGE(TAG, "LCD not initialized");
      while (true)
      {
        Tasks::task_delay(1s);
      };
    }
  }

  /// @brief Sets the autologoff delay
  /// @param delay new delay
  void BoardLogic::setAutologoffDelay(std::chrono::seconds delay)
  {
    machine.setAutologoffDelay(delay);
  }

  void BoardLogic::setWhitelist(WhiteList whitelist)
  {
    auth.setWhitelist(whitelist);
  }

  FabBackend &BoardLogic::getServer()
  {
    return server;
  }

  void BoardLogic::setRebootRequest(bool request)
  {
    rebootRequest = request;
  }

  bool BoardLogic::getRebootRequest() const
  {
    return rebootRequest;
  }
} // namespace fablabbg