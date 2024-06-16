# FAB-O-MATIC : RFID card access control for FabLab equipments

Build status : [![Build firmware images](https://github.com/fablab-bergamo/Fab-O-matic/actions/workflows/build.yml/badge.svg)](https://github.com/fablab-bergamo/Fab-O-matic/actions/workflows/build.yml)
Test suite : [![Wokwi](https://github.com/fablab-bergamo/Fab-O-matic/actions/workflows/tests.yml/badge.svg)](https://github.com/fablab-bergamo/Fab-O-matic/actions/workflows/tests.yml)

## What is this project?

* A low-cost RFID-card reader with LCD display to monitor and allow machine usage in a Fab Lab environment (e.g. 3D printers, lasers, CNC).
* This repository includes Firmware + Hardware projects

| Front panel | PCB |
| --- | --- |
| ![3D_Front pannel_2024-05-05](https://github.com/fablab-bergamo/rfid-arduino/assets/6236243/b7afca37-4321-4b77-a073-5d35cf670fbc) | ![3D_PCB REV1 0_2024-05-05](https://github.com/fablab-bergamo/rfid-arduino/assets/6236243/dac93cda-b271-4663-a376-2b97a6c22956) |

* Another repository [fablab-bergamo/rfid-backend](https://github.com/fablab-bergamo/rfid-backend) contains the backend server. This server can run on a Raspberry Pi Zero, managing user RFID authentication, track machine usage / maintenance needs.
* Communication between boards and backend uses MQTT.
* Machine power/enable control is achieved through an external relay and/or MQTT switch (Shelly model was tested).
* Hardware project is included in the <code>hardware</code> sub-folder, including Gerber files and instructions for manufacturing.

## Hardware components used

* Compatible with ESP32, ESP32-S2 or ESP32-S3 chips with Arduino framework.
* RFID reader (using mfrc522 compatible chip)
* LCD driver (using Hitachi HD44780 compatible chip)
* 3.3V Relay (or [Shelly](https://www.shellyitalia.com/shelly-plus-1-mini-gen3/) MQTT device)
* 3.3V Buzzer
* NeoPixel for status indication to the user
* MFID RFID cards for user authentication

## PCB and schematics

* See <code>hardware\README.md</code> folder for Gerber files and schematics.
* Revision 0.x uses modules RFID, LCD, Power DC-DC buck converter and ESP32-S3 and a mix of SMD and through-hole components.
* Revision 1.x uses more SMD componente + RFID and LCD modules and was designed for JLCPCB assembly.
* Cost per board is below 30 eur (PCB around 1 eur, components 11 eur + modules (LCD,RFID,Buck,Relay) approx 8 eur).

## Firmware environment

* Language: C++20 with ArduinoFramework for ESP32
* IDE: VSCode + Platform.io extension
* To build on the first time, rename <code>conf/secrets.hpp.example</code> to <code>conf/secrets.hpp</code>.

## Run a demo in the browser

* Download latest Wokwi firmare from Github Actions / Releases
* Extract the BIN file from the release ZIP
* Open WOKWI Circuit [link](https://wokwi.com/projects/363448917434192897)
* In Wokwi v code editor, press F1 > Upload firmware ... and pick the <code>esp32-wokwi.bin</code> file

![image](https://github.com/fablab-bergamo/rfid-arduino/assets/6236243/5c41092e-f8bf-451a-95ec-8dc6d7e07824)

* When the preprocessor constants <code>MQTT_SIMULATION</code> or <code>RFID_SIMULATION</code> are set to true:
  * RFID chip is replaced with a mockup simulating random RFID tags from whitelist from time to time (<code>MockMrfc522</code> class).
  * A simple MQTT broker (<code>MockMQTTBroker</code> class) is run in a separate thread on esp32s2

## Firmware test suite

* A set a test scripts based on Platformio+Unity is included in the project.
* There are three ways to run the tests:

1. Use real hardware (esp32s3, esp32 wroverkit) connected over USB with Platform.io command

```shell
pio test --environment esp32-s3 --verbose
```

This is the method I am using whenever online tests fail (to narrow the run use <code>-f test_which_failed</code>)

2. Use Wokwi-CLI with test images built by Platform.io. It requires a wokwi access token (free as per Jan 2024). The Github action "tests.yml" uses this mechanism.

3. Run the BIN image generated by <code>pio test --without-uploading --without-testing</code> in VS Code Wokwi plugin.

## Firmware configuration steps (/conf folder)

* See <code>conf/conf.hpp</code> to configure LCD dimensions, timeouts, debug logs and some behaviours (e.g. time before to power off the machine). Default configuration should be fine. Some default configuration settings may be overriden by the backend (like grace period or auto-logout delay).
* See <code>conf/secrets.hpp</code> to configure network SSID/Password credentials, MQTT credentials and whitelisted RFID tags. You must copy secrets.hpp.example as secrets.hpp for the first compilation.

* A configuration portal based on WiFiManager allows to configure WiFi credentials, MQTT Broker address and Shelly topic. This makes editing <code>conf/secrets.hpp</code> required only for MQTT Broker credentials settings. To open the configuration portal, the CONFIG button must be pressed for a few seconds when the system is running.

> To add white-listed RFID cards, edit the tuples list <code>whitelist</code>. These RFID tags will be authorized when backend cannot be contacted.

```c++
  static constexpr WhiteList whitelist /* List of RFID tags whitelisted, regardless of connection */
      {
          std::make_tuple(0xAABBCCD1, FabUser::UserLevel::FABLAB_ADMIN, "ABCDEFG"),
          ...
          std::make_tuple(0xAABBCCDA, FabUser::UserLevel::FABLAB_USER, "USER1")
      };
```

* During boot, the board is dumping all the default settings, reported below

```
Machine defaults:
        mqtt_server: fabpi.local
        mqtt_switch_topic: 
        machine_id: 1
        machine_name: MACHINE1
        machine_type: 1
        hostname: BOARD
Compilation settings
        MQTT_SIMULATION: 1
        RFID_SIMULATION: 1
        CORE_DEBUG_LEVEL: 4
        LANGUAGE: en-US
RFID tags:
        UID_BYTE_LEN: 4
        CACHE_LEN: 10
LCD config
        LCD ROWS: 2, COLS: 16
        SHORT_MESSAGE_DELAY: 1000ms
General settings:
        DEFAULT_AUTO_LOGOFF_DELAY: 12h
        BEEP_PERIOD: 120s
        DEFAULT_GRACE_PERIOD: 5min
        DELAY_BETWEEN_BEEPS: 30s
        MAINTENANCE_BLOCK: 1
        LONG_TAP_DURATION: 10s
Debug settings:
        ENABLE_LOGS: 1
        ENABLE_TASK_LOGS: 0
        SERIAL_SPEED_BDS: 115200
        FORCE_PORTAL: 0
        LOAD_EEPROM_DEFAULTS: 0
Buzzer settings:
        LEDC_PWM_CHANNEL: 2
        STANDARD_BEEP_DURATION: 250ms
        NB_BEEPS: 3
        BEEP_HZ: 660
Tasks settings:
        RFID_CHECK_PERIOD: 150ms
        RFID_SELFTEST_PERIOD: 60s
        MQTT_REFRESH_PERIOD: 30s
        WATCHDOG_TIMEOUT: 60s
        WATCHDOG_PERIOD: 1s
        PORTAL_CONFIG_TIMEOUT: 300s
        MQTT_ALIVE_PERIOD: 120s
MQTT settings:
        topic: machine
        response_topic: /reply
        MAX_TRIES: 2
        TIMEOUT_REPLY_SERVER: 2000ms
        PORT_NUMBER: 1883
Hardware settings:
        LED:
                Pin:20 (G:255, B:255)
                Type is neopixel:1, is rgb:0
                Neopixel config flags:82
        Mfrc522 chip:
                SPI settings: MISO: 33, MOSI: 26, SCK: 32, SDA: 27
                RESET pin:16
        LCD module:
                Parallel interface D0:2, D1:4, D2:5, D3:19
                Reset pin:15, Enable pin:18
                Backlight pin:255 (active low:0)
        Relay:
                Control pin:14 (active low:0)
        Buzzer:
                Pin:12
        Buttons:
                Factory defaults pin:21
```

## Firmware debugging with Wokwi ESP32 emulator integrated with VSCode

This is a facultative but very helpful setup to shorten the development workflow.

* Install ESP-IDF extension, Wokwi extension with community evaluation license

> Make sure ESP-IDF platform is esp32s2 (used by wokwi Platformio environment)

* Build PlatformIO wokwi project, and start simulation with command <code>Wokwi: Start Simulator</code>, you shall see the program running:

![image](https://github.com/fablab-bergamo/rfid-arduino/assets/6236243/dfdf33e3-74ac-4246-9c92-4631e0009034)

> See files wokwi.toml and diagram.json

* To configure GDB connection to the Wokwi simulator, edit .vscode\launch.json and add the following fragment inside <code>"configurations"</code> array

```json
  {
    "name": "Wokwi GDB",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/.pio/build/wokwi/firmware.elf",
    "cwd": "${workspaceFolder}",
    "MIMode": "gdb",
    "miDebuggerPath": "${command:espIdf.getXtensaGdb}",
    "miDebuggerServerAddress": "localhost:3333"
  }
```

* To test debugging, first start Wokwi with <code>Wokwi: Start Simulator and wait for debugger</code>
* You can then run the application, setup breakpoints, inspect variables from the Wokwi debugger:

![image](https://github.com/fablab-bergamo/rfid-arduino/assets/6236243/55f926b5-eec8-49d9-b217-628e07f7e3b8)

## Firmware OTA procedure

* Command-line instructions with ESPOTA tool:

```
wget https://raw.githubusercontent.com/espressif/arduino-esp32/master/tools/espota.py
./espota.py -i <board_ip> -d -r -f firmware.bin
```

* Edit platform.io configuration file for the build with the following under the right environmnet

```ini
upload_protocol = espota
upload_port = IP_ADDRESS_HERE or mDNS_NAME.local
```

* Set serial port to AUTO in VSCODE
* Build & Deploy from VSCode. Upload takes 1-2 minutes. Board will reboot automatically when the machine is idle.
* Some key settings are persisted (provided SavedConfig.hpp version field "magic_number" remains the same):

```json
{
  "disablePortal": false,
  "bootCount": 1,
  "ssid": "Wokwi-GUEST",
  "password": "",
  "mqtt_server": "fabpi2.local",
  "mqtt_user": "user",
  "mqtt_password": "password",
  "mqtt_switch_topic": "",
  "machine_id": "1",
  "magic_number": 80,
  "cached_cards": []
}
```

## Firmware languages

* To change language, use the right compilation constant (FABOMATIC_LANG_IT_IT or FABOMATIC_LANG_EN_US). See platformio.ini hardware-rev0-it_IT and hardware-rev0-en_US for example.
* To add a language create new file with the language ISO name <code>include/language</code>, define a new constant FABOMATIC_LANG_xx_xx and update <code>lang.hpp</code> accordingly.

## PlatformIO environments list

* Each environment defines the GPIO pins numbers through constants *PINS_xx* and language through *FABOMATIC_LANG_xx*

| Name | Description | Pins definition | Part of releases? |
|--|--|--|--|
| esp32-s3 | Release build for size statistics | PINS_ESP32S3 | No |
| hardware-base | Base config, debug build on ESP32-S3, for hardware projects | PINS_HARDWARE_REV0 | No |
| hardware-rev0-it_IT | Italian language for HW rev 0.x and 1.x | *as per hardware-base* | Yes |
| hardware-rev0-en_US | English language for HW rev 0.x and 1.x | *as per hardware-base* | Yes |
| esp32-devboard | Used by prototype with ESP32 module on breadboard | PINS_ESP32 | No |
| wokwi | English version for demo and unit tests | PINS_WOKWI | Yes |
| wrover-kit-it_IT | Version for testing with the official ESP-WROVER-KIT V4.1 with ESP32S3 | PINS_ESP32_WROVERKIT | No |

* See <code>conf/pins.hpp</code> to set the GPIO pins for LCD parallel interface, relay, buzzer and RFID reader SPI interface for each model.

## Version history

| Version | Date | Notable changes |
|--|--|--|
|none  | 2021 | Initial version |
|0.1.x | August 2023 | Implemented MQTT communication with backend |
|0.1.x | December 2023 | Added test cases, mqtt broker simulation |
|0.2.x | January 2024 | Added over-the-air updates, WiFi portal for initial config, first deploy |
|0.3.x | February 2024 | Added factory defaults button, power grace period config from backend, PCB draft |
|0.4.x | March 2024 | 1st PCB manufactured (rev0.2), FW +IP address announced over MQTT |
|0.5.x | April 2024 | Fully tested on PCB rev0.2 & rev 0.3 |
|0.6.x | April 2024 | Added RFID cache for network interruptions, config portal now opens only by button push |
|0.7.x | April 2024 | Maintenance operation is displayed on LCD, JSON is now used for data persistence in Flash |
|0.8.x | May 2024 | Added localization with English & Italian language builds, sizes reports for firmware |
|0.9.x | June 2024 | Added buffering of important events when network is down. Espressif Arduino Core 3.0 testing. |

