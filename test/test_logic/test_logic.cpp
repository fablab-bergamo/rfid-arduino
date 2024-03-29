#include <atomic>
#include <chrono>
#include <functional>
#include <pthread.h>
#include <string>
#include <vector>

#include "BoardLogic.hpp"
#include "FabBackend.hpp"
#include "LCDWrapper.hpp"
#include "RFIDWrapper.hpp"
#include "SavedConfig.hpp"
#include "conf.hpp"
#include "mock/MockLcdLibrary.hpp"
#include "mock/MockMQTTBroker.hpp"
#include "mock/MockMrfc522.hpp"
#include "test_common.h"
#include <Arduino.h>
#include <unity.h>

using namespace std::chrono_literals;

[[maybe_unused]] static const char *TAG3 = "test_logic";

fablabbg::RFIDWrapper<fablabbg::MockMrfc522> rfid;
fablabbg::LCDWrapper<fablabbg::MockLcdLibrary> lcd{fablabbg::pins.lcd};
fablabbg::BoardLogic logic;

using BoardLogic = fablabbg::BoardLogic;

constexpr fablabbg::card::uid_t get_test_uid(size_t idx)
{
  auto [card_uid, level, name] = fablabbg::tests::test_whitelist[idx];
  return card_uid;
}

namespace fablabbg::tests
{
  void test_machine_defaults()
  {
    auto &machine = logic.getMachine();

    TEST_ASSERT_TRUE_MESSAGE(machine.isConfigured(), "Machine not configured");

    auto config_req = machine.getConfig();
    TEST_ASSERT_TRUE_MESSAGE(config_req.has_value(), "Machine config not available");

    auto config = config_req.value();

    auto result = machine.getMachineId().id == fablabbg::conf::default_config::machine_id.id;
    TEST_ASSERT_TRUE_MESSAGE(result, "Machine ID not per default configuration");

    result = machine.getMachineName() == fablabbg::conf::default_config::machine_name;
    TEST_ASSERT_TRUE_MESSAGE(result, "Machine Name not per default configuration");

    result = machine.getAutologoffDelay() == fablabbg::conf::machine::DEFAULT_AUTO_LOGOFF_DELAY;
    TEST_ASSERT_TRUE_MESSAGE(result, "Machine autologoff delay not per default configuration");

    result = machine.toString().length() > 0;
    TEST_ASSERT_TRUE_MESSAGE(result, "Machine toString() failed");

    TEST_ASSERT_TRUE_MESSAGE(config.hasRelay() || fablabbg::pins.relay.ch1_pin == fablabbg::NO_PIN, "Machine relay not configured");
    TEST_ASSERT_TRUE_MESSAGE(config.hasMqttSwitch() || fablabbg::conf::default_config::machine_topic.empty(), "Machine MQTT switch not configured");

    TEST_ASSERT_TRUE_MESSAGE(config.mqtt_config.topic == fablabbg::conf::default_config::machine_topic, "Machine MQTT topic not configured");
    TEST_ASSERT_TRUE_MESSAGE(config.relay_config.pin == fablabbg::pins.relay.ch1_pin, "Machine relay pin not configured");
    TEST_ASSERT_TRUE_MESSAGE(config.machine_id.id == fablabbg::conf::default_config::machine_id.id, "Machine ID not configured");
  }

  void test_simple_methods()
  {
    logic.beep_failed();
    logic.beep_ok();
    logic.blinkLed();

    std::vector statuses{BoardLogic::Status::Error, BoardLogic::Status::ErrorHardware, BoardLogic::Status::Connected,
                         BoardLogic::Status::Connecting, BoardLogic::Status::Clear, BoardLogic::Status::MachineFree,
                         BoardLogic::Status::LoggedIn, BoardLogic::Status::LoginDenied, BoardLogic::Status::Busy,
                         BoardLogic::Status::LoggedOut, BoardLogic::Status::AlreadyInUse, BoardLogic::Status::MachineInUse,
                         BoardLogic::Status::Offline, BoardLogic::Status::NotAllowed, BoardLogic::Status::Verifying,
                         BoardLogic::Status::MaintenanceNeeded, BoardLogic::Status::MaintenanceQuery,
                         BoardLogic::Status::MaintenanceDone, BoardLogic::Status::PortalStarting, BoardLogic::Status::PortalFailed,
                         BoardLogic::Status::PortalSuccess, BoardLogic::Status::Booting, BoardLogic::Status::ShuttingDown,
                         BoardLogic::Status::OTAStarting, BoardLogic::Status::FactoryDefaults};

    for (const auto status : statuses)
    {
      logic.changeStatus(status);
      TEST_ASSERT_EQUAL_UINT8_MESSAGE(status, logic.getStatus(), "Status mismatch");
    }

    for (auto i = 0; i < 3; ++i)
    {
      logic.checkPowerOff();
      logic.blinkLed();
      logic.updateLCD();
      logic.reconfigure();
    }
  }

  void test_whitelist_no_network()
  {
    machine_init(logic, rfid);

    // Check whitelist is recognized
    for (auto &wle : test_whitelist)
    {
      const auto [card_uid, level, name] = wle;
      TEST_ASSERT_TRUE_MESSAGE(logic.authorize(card_uid), "Card not authorized");
      TEST_ASSERT_TRUE_MESSAGE(logic.getStatus() == BoardLogic::Status::LoggedIn, "Status not LoggedIn");
      TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getActiveUser().card_uid == card_uid, "User UID not equal");
      TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getActiveUser().holder_name == name, "User name not equal");
      TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getActiveUser().user_level == level, "User level not equal");
      logic.logout();
      TEST_ASSERT_TRUE_MESSAGE(logic.getStatus() == BoardLogic::Status::LoggedOut, "Status not LoggedOut");
    }

    // Check that machine is free
    logic.checkRfid();
    TEST_ASSERT_TRUE_MESSAGE(logic.getStatus() == BoardLogic::Status::MachineFree, "Status not MachineFree");
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isFree(), "Machine not free");

    // Logon simulating RFID tag
    const auto card_uid = get_test_uid(1);

    simulate_rfid_card(rfid, logic, card_uid, std::nullopt);
    TEST_ASSERT_TRUE_MESSAGE(rfid.isNewCardPresent(), "New card not present");
    TEST_ASSERT_TRUE_MESSAGE(rfid.readCardSerial().has_value(), "Card serial not read");
    TEST_ASSERT_TRUE_MESSAGE(rfid.readCardSerial().value() == card_uid, "Card serial not equal");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::LoggedIn, logic.getStatus(), "Status not LoggedIn");

    // Card away, machine shall be busy
    auto new_state = simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineInUse, new_state, "Status not MachineInUse");
    TEST_ASSERT_FALSE_MESSAGE(logic.getMachine().isFree(), "Machine is free");

    // Same card back, shall logout user
    new_state = simulate_rfid_card(rfid, logic, card_uid);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::LoggedOut, new_state, "Status not LoggedOut");
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isFree(), "Machine is not free");
  }

  void test_one_user_at_a_time()
  {
    machine_init(logic, rfid);

    const auto card_uid = get_test_uid(0);
    auto new_state = simulate_rfid_card(rfid, logic, card_uid);

    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::LoggedIn, new_state, "Status not LoggedIn");
    TEST_ASSERT_TRUE_MESSAGE(!logic.getMachine().isFree(), "Machine is free");

    // Card away, machine shall be busy
    new_state = simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_TRUE_MESSAGE(!logic.getMachine().isFree(), "Machine is free");

    // New card, shall be denied
    const auto card_uid2 = get_test_uid(1);
    simulate_rfid_card(rfid, logic, card_uid2, 100ms);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getActiveUser().card_uid == card_uid, "User UID has changed");

    // New card away, first user shall still be here
    simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getActiveUser().card_uid == card_uid, "User UID has changed");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineInUse, logic.getStatus(), "Status not MachineInUse");

    // Original card, shall log out
    simulate_rfid_card(rfid, logic, card_uid);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isFree(), "Machine is not free");
    // Original card away
    simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isFree(), "Machine is not free");

    // Now new card must succeed
    simulate_rfid_card(rfid, logic, card_uid2);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::LoggedIn, logic.getStatus(), "Status not LoggedIn");

    // Card away
    simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineInUse, logic.getStatus(), "Status not MachineInUse");

    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getActiveUser().card_uid == card_uid2, "User UID is not expected");
    TEST_ASSERT_TRUE_MESSAGE(!logic.getMachine().isFree(), "Machine is free");
  }

  void test_user_autologoff()
  {
    machine_init(logic, rfid);

    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getAutologoffDelay() == conf::machine::DEFAULT_AUTO_LOGOFF_DELAY, "Autologoff delay not default");

    logic.setAutologoffDelay(2s);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().getAutologoffDelay() == 2s, "Autologoff delay not 2s");

    simulate_rfid_card(rfid, logic, get_test_uid(0));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::LoggedIn, logic.getStatus(), "Status not LoggedIn");

    // Card away
    simulate_rfid_card(rfid, logic, std::nullopt);
    delay(1000);
    TEST_ASSERT_FALSE_MESSAGE(logic.getMachine().isFree(), "Machine is free");
    TEST_ASSERT_FALSE_MESSAGE(logic.getMachine().isAutologoffExpired(), "Autologoff not expired");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineInUse, logic.getStatus(), "Status not MachineInUse");

    // Now shall expire afer 2s
    simulate_rfid_card(rfid, logic, std::nullopt);
    delay(1000);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isAutologoffExpired(), "Autologoff expired");

    logic.logout();
    simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isFree(), "Machine is free");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineFree, logic.getStatus(), "Status not MachineFree");
  }

  void test_machine_maintenance()
  {
    machine_init(logic, rfid);

    auto &machine = logic.getMachineForTesting();
    const auto card_fabuser = get_test_uid(2);
    const auto card_admin = get_test_uid(0);

    machine.setAllowed(true);
    machine.setMaintenanceNeeded(true);
    simulate_rfid_card(rfid, logic, card_fabuser);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MaintenanceNeeded, logic.getStatus(), "Status not MaintenanceNeeded");

    simulate_rfid_card(rfid, logic, std::nullopt);
    simulate_rfid_card(rfid, logic, card_admin, conf::machine::LONG_TAP_DURATION + 3s); // Log in + Conferma manutenzione perché non ritorna prima della conclusione
    simulate_rfid_card(rfid, logic, std::nullopt);                                      // Card away
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineInUse, logic.getStatus(), "Status not MachineInUse by admin");
    TEST_ASSERT_FALSE_MESSAGE(logic.getMachine().isMaintenanceNeeded(), "Maintenance not cleared by admin");

    // Logoff admin
    simulate_rfid_card(rfid, logic, card_admin);
    TEST_ASSERT_TRUE_MESSAGE(logic.getMachine().isFree(), "Machine is not free");
    simulate_rfid_card(rfid, logic, std::nullopt);

    // Now try to logon with fabuser (should succeed because maintenance is cleared)
    simulate_rfid_card(rfid, logic, card_fabuser);
    simulate_rfid_card(rfid, logic, std::nullopt);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::MachineInUse, logic.getStatus(), "Status not MachineInUse by normal user");
  }

  void test_machine_allowed()
  {
    machine_init(logic, rfid);

    auto &machine = logic.getMachineForTesting();
    auto card_fabuser = get_test_uid(3);
    auto card_admin = get_test_uid(0);

    machine.setAllowed(false);
    machine.setMaintenanceNeeded(false);

    // check if blocked for normal users
    simulate_rfid_card(rfid, logic, card_fabuser);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::NotAllowed, logic.getStatus(), "Status not NotAllowed");
    simulate_rfid_card(rfid, logic, std::nullopt);

    // still blocked for admins
    simulate_rfid_card(rfid, logic, card_admin);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(BoardLogic::Status::NotAllowed, logic.getStatus(), "Status not NotAllowed for admins");
    simulate_rfid_card(rfid, logic, std::nullopt);
  }
} // namespace fablabbg::tests

void tearDown(void){};

void setUp(void)
{
  TEST_ASSERT_TRUE_MESSAGE(fablabbg::SavedConfig::DefaultConfig().SaveToEEPROM(), "Default config save failed");
  TEST_ASSERT_TRUE_MESSAGE(logic.configure(rfid, lcd), "BoardLogic configure failed");
  TEST_ASSERT_TRUE_MESSAGE(logic.board_init(), "BoardLogic init failed");
  logic.setWhitelist(fablabbg::tests::test_whitelist);
  // Disable MQTT for tests
  if (auto server_config = fablabbg::SavedConfig::LoadFromEEPROM(); server_config.has_value())
  {
    auto conf = server_config.value();
    strncpy(conf.mqtt_server, "INVALID_SERVER\0", sizeof(conf.mqtt_server));
    logic.getServer().configure(conf);
  }
};

void setup()
{
  delay(1000);
  auto config = fablabbg::SavedConfig::LoadFromEEPROM();
  UNITY_BEGIN();
  RUN_TEST(fablabbg::tests::test_machine_defaults);
  RUN_TEST(fablabbg::tests::test_simple_methods);
  RUN_TEST(fablabbg::tests::test_machine_allowed);
  RUN_TEST(fablabbg::tests::test_machine_maintenance);
  RUN_TEST(fablabbg::tests::test_whitelist_no_network);
  RUN_TEST(fablabbg::tests::test_one_user_at_a_time);
  RUN_TEST(fablabbg::tests::test_user_autologoff);
  UNITY_END(); // stop unit testing
  if (config.has_value())
  {
    config.value().SaveToEEPROM();
  }
};

void loop()
{
}
