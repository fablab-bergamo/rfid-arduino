#include <cstdint>
#include <string>
#include <array>
#include <chrono>

#include <LiquidCrystal.h>
#include <WiFi.h>

#include "Board.h"
#include "BoardState.h"
#include "pins.h"

static BoardState bstate;

/// @brief connects and polls the server for up-to-date machine information
void refreshFromServer()
{
  if (Board::server.connect())
  {
    // Check the configured machine data from the server
    auto result = Board::server.checkMachine(secrets::machine::machine_id);
    if (result.request_ok)
    {
      if (result.is_valid)
      {
        Serial.printf("The configured machine ID %d is valid, maintenance=%d, allowed=%d\n", secrets::machine::machine_id, result.needs_maintenance, result.allowed);
        Board::machine.maintenanceNeeded = result.needs_maintenance;
        Board::machine.allowed = result.allowed;
      }
      else
      {
        Serial.printf("The configured machine ID %d is unknown to the server\n", secrets::machine::machine_id);
      }
    }
  }
}

/// @brief Opens WiFi and server connection and updates board state accordingly
/// @return True if server connection was successfull
bool tryConnect()
{
  Serial.println("Trying Wifi and server connection...");
  // connection to wifi
  bstate.changeStatus(BoardState::Status::CONNECTING);

  // Try to connect
  Board::server.connect();

  // Get machine data from the server if it is online
  refreshFromServer();

  // Refresh after connection
  bstate.changeStatus(Board::server.isOnline() ? BoardState::Status::CONNECTED : BoardState::Status::OFFLINE);
  delay(500);
  return Board::server.isOnline();
}

void setup()
{
  Serial.begin(115200); // Initialize serial communications with the PC for debugging.
  Serial.println("Starting setup!");
  delay(100);
  if (!bstate.init())
  {
    bstate.changeStatus(BoardState::Status::ERROR);
    bstate.beep_failed();
  }
  delay(100);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  bstate.beep_ok();
}

void loop()
{
  delay(100);

  // Regular connection management
  if (bstate.last_server_poll == 0)
  {
    bstate.last_server_poll = millis();
  }
  if (millis() - bstate.last_server_poll > conf::server::REFRESH_PERIOD_SECONDS * 1000)
  {
    Serial.printf("Free heap:%d bytes\n", ESP.getFreeHeap());
    Serial.printf("Current machine status:%s\n", Board::machine.toString().c_str());
    if (Board::server.isOnline())
    {
      // Get machine data from the server
      refreshFromServer();
    }
    else
    {
      // Reconnect
      tryConnect();
    }
    bstate.last_server_poll = 0;
  }

  // check if there is a card
  if (Board::rfid.isNewCardPresent())
  {
    Serial.println("New card present");
    // if there is a "new" card (could be the same that stayed in the field)
    if (!Board::rfid.readCardSerial() || !bstate.ready_for_a_new_card)
    {
      return;
    }
    bstate.no_card_cpt = 0;
    bstate.ready_for_a_new_card = false;

    // Acquire the UID of the card
    auto uid = Board::rfid.getUid();

    if (Board::machine.isFree())
    {
      // machine is free
      if (bstate.authorize(uid))
      {
        Serial.println("Login successfull");
      }
      else
      {
        Serial.println("Login failed");
      }
      delay(1000);
      refreshFromServer();
    }
    else
    {
      // machine is busy
      if (Board::machine.getActiveUser().card_uid == uid)
      {
        // we can logout. we should require that the card stays in the field for some seconds, to prevent accidental logout. maybe sound a buzzer?
        bstate.logout();
        delay(1000);
      }
      else
      {
        // user is not the same, display who is using it
        bstate.changeStatus(BoardState::Status::ALREADY_IN_USE);
        delay(1000);
      }
    }
  }
  else
  {
    bstate.no_card_cpt++;
    if (bstate.no_card_cpt > 10) // we wait for get SOME "no card" before flipping this flag
      bstate.ready_for_a_new_card = true;

    if (Board::machine.isFree())
    {
      bstate.changeStatus(BoardState::Status::FREE);

      if (Board::machine.isShutdownPending())
      {
        // TODO : beep
      }
      if (Board::machine.canPowerOff())
      {
        Board::machine.power(false);
      }
    }
    else
    {
      bstate.changeStatus(BoardState::Status::IN_USE);

      // auto logout after delay
      if (conf::machine::TIMEOUT_USAGE_MINUTES > 0 &&
          Board::machine.getUsageTime() > conf::machine::TIMEOUT_USAGE_MINUTES * 60 * 1000)
      {
        Serial.println("Auto-logging out user");
        bstate.logout();
        delay(1000);
      }
    }
  }
}